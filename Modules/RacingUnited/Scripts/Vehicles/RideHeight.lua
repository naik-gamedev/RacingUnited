-- RIDE01 deterministic kerb-mass ride-height calibration.
-- The authored visual stance is a datum; spring/torsion preload makes physics
-- settle onto that datum. This is not a visual mesh translation.

vehicleRideHeight = vehicleRideHeight or {
    valid = false,
    message = "Static ride height has not been calibrated",
    corners = {},
    assetGeometry = nil
}

local function RideHeightValue(wheel, shared, key, fallback)
    local value = wheel[key]
    if value == nil then
        value = shared[key]
    end
    if value == nil then
        return fallback
    end
    return value
end

local function RideHeightProviderForAxle(axle)
    local architecture = PrototypeCarDefinition.suspensionArchitecture or {}
    local assembly = architecture[axle] or {}
    return assembly.preferredProvider
        or assembly.runtimeProvider
        or "linear_raycast_v1"
end

local function RideHeightCornerLoadN(wheel)
    local chassis = PrototypeCarDefinition.chassis
    local frontFraction = chassis.frontStaticLoadFraction or 0.5
    local leftFraction = chassis.leftStaticLoadFraction or 0.5
    local axleFraction = wheel.axle == "front"
        and frontFraction or (1.0 - frontFraction)
    local isLeft = string.find(wheel.name or "", "left", 1, true) ~= nil
    local sideFraction = isLeft and leftFraction or (1.0 - leftFraction)
    return chassis.massKg * 9.80665 * axleFraction * sideFraction
end

function ConfigurePrototypeStaticRideHeight()
    local definition = PrototypeCarDefinition
    local shared = definition.wheelPhysics
    local setup = definition.rideHeight or {}
    vehicleRideHeight.corners = {}
    vehicleRideHeight.valid = false

    for index, wheel in ipairs(definition.wheels) do
        local targetOffset = wheel.axle == "front"
            and (setup.targetFrontBodyOffsetM or 0.0)
            or (setup.targetRearBodyOffsetM or 0.0)
        local provider = RideHeightProviderForAxle(wheel.axle)
        local result, errorMessage = Vehicle.SolveStaticRideHeight(
            provider,
            RideHeightCornerLoadN(wheel),
            targetOffset,
            wheel.mount[2] - (setup.authoredGroundPlaneLocalY or 0.0),
            RideHeightValue(wheel, shared, "radiusM", 0.30),
            RideHeightValue(wheel, shared, "restLengthM", 0.50),
            RideHeightValue(wheel, shared, "maximumCompressionM", 0.20),
            RideHeightValue(wheel, shared, "maximumDroopM", 0.15),
            RideHeightValue(wheel, shared, "springRateNPerM", 35000.0),
            RideHeightValue(wheel, shared, "springProgressionNPerM2", 0.0),
            RideHeightValue(wheel, shared, "motionRatio", 1.0),
            RideHeightValue(
                wheel, shared, "tireRadialStiffnessNPerM", 220000.0))
        if result == nil then
            vehicleRideHeight.message = "RIDE HEIGHT ERROR "
                .. tostring(wheel.name) .. ": " .. tostring(errorMessage)
            return false
        end

        wheel.springPreloadN = result.spring_preload_n
        wheel.staticRideHeight = result
        result.wheel_name = wheel.name
        result.axle = wheel.axle
        result.provider = provider
        result.supported_load_n = RideHeightCornerLoadN(wheel)
        result.target_body_offset_m = targetOffset
        vehicleRideHeight.corners[index] = result
    end

    vehicleRideHeight.valid = true
    vehicleRideHeight.message =
        "Static ride height calibrated from kerb-mass load + tire compliance"
    return true
end

function RideHeightImportAssetGeometry(metadata)
    if metadata == nil or metadata.ride_height_geometry == nil then
        vehicleRideHeight.assetGeometry = nil
        return false
    end
    vehicleRideHeight.assetGeometry = metadata.ride_height_geometry
    return metadata.ride_height_geometry.valid == true
end

ConfigurePrototypeStaticRideHeight()
