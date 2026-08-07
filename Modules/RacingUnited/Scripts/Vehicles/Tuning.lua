-- Applies mutable debug tuning to the native vehicle service.
function ApplyVehicleTireModel()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    local success = Vehicle.SetTireModel(
        nativeVehicle,
        vehicleTire.nominalLoad,
        vehicleTire.peakFriction,
        vehicleTire.longitudinalStiffness,
        vehicleTire.corneringStiffness,
        vehicleTire.loadSensitivity,
        vehicleTire.longitudinalRelaxation,
        vehicleTire.lateralRelaxation,
        vehicleTire.wheelInertia,
        vehicleTire.pneumaticTrail,
        vehicleTire.stiffnessLoadExponent,
        vehicleTire.longitudinalShapeFactor,
        vehicleTire.lateralShapeFactor,
        vehicleTire.longitudinalCurvatureFactor,
        vehicleTire.lateralCurvatureFactor,
        vehicleTire.combinedSlipExponent,
        vehicleTire.pneumaticTrailFalloff)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    -- SetTireModel intentionally retains its legacy meaning and updates every
    -- wheel. The debug sliders therefore become an explicit all-wheel override.
    vehicleWheelTireProfileNames = {}
    for index = 1, Vehicle.GetWheelCount(nativeVehicle) do
        vehicleWheelTireProfileNames[index] = "manual_all_wheels"
    end
    vehicleMessage = "Applied manual tire tuning to every wheel"
    return true
end

function ApplyNativeWheelTireProfile(wheelIndex, profile)
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    if profile == nil then
        vehicleMessage = "VEHICLE ERROR: missing tire profile for wheel " .. tostring(wheelIndex)
        return false
    end

    local success = Vehicle.SetWheelTireModel(
        nativeVehicle,
        wheelIndex,
        profile.nominalLoad,
        profile.peakFriction,
        profile.longitudinalStiffness,
        profile.corneringStiffness,
        profile.loadSensitivity,
        profile.longitudinalRelaxation,
        profile.lateralRelaxation,
        profile.wheelInertia,
        profile.pneumaticTrail,
        profile.stiffnessLoadExponent,
        profile.longitudinalShapeFactor,
        profile.lateralShapeFactor,
        profile.longitudinalCurvatureFactor,
        profile.lateralCurvatureFactor,
        profile.combinedSlipExponent,
        profile.pneumaticTrailFalloff)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    vehicleWheelTireProfileNames[wheelIndex] = profile.id or "unnamed_profile"
    return true
end

function ApplyDefinitionTireProfiles()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end

    for index, wheel in ipairs(PrototypeCarDefinition.wheels) do
        local profile = TirePresets[wheel.tireProfile]
        if not ApplyNativeWheelTireProfile(index, profile) then
            return false
        end
    end
    vehicleMessage = "Restored the vehicle definition's per-wheel tire profiles"
    return true
end

function ApplyTireProfileToAllWheels(profileName)
    local profile = TirePresets[profileName]
    if profile == nil then
        vehicleMessage = "VEHICLE ERROR: unknown tire profile " .. tostring(profileName)
        return false
    end
    for index = 1, Vehicle.GetWheelCount(nativeVehicle) do
        if not ApplyNativeWheelTireProfile(index, profile) then
            return false
        end
    end
    vehicleMessage = "Applied " .. profile.displayName .. " to every wheel"
    return true
end

function ApplyVehicleDriverAids()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    local success = Vehicle.SetDriverAids(
        nativeVehicle,
        vehicleDriverAids.absEnabled,
        vehicleDriverAids.tractionControlEnabled,
        vehicleDriverAids.absTargetSlip,
        vehicleDriverAids.tractionTargetSlip,
        vehicleDriverAids.minimumSpeed,
        vehicleDriverAids.modulationRate,
        vehicleDriverAids.maximumHandbrakeTorque)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
    return success
end

function ApplyVehicleBrakeBias()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    local frontBrakePerWheel = vehicleDriverAids.frontBrakeBias * 0.5
    local rearBrakePerWheel = (1.0 - vehicleDriverAids.frontBrakeBias) * 0.5
    local success =
        Vehicle.SetWheelBrakeFactors(
            nativeVehicle, 1, frontBrakePerWheel, 0.0)
        and Vehicle.SetWheelBrakeFactors(
            nativeVehicle, 2, frontBrakePerWheel, 0.0)
        and Vehicle.SetWheelBrakeFactors(
            nativeVehicle, 3, rearBrakePerWheel, 0.5)
        and Vehicle.SetWheelBrakeFactors(
            nativeVehicle, 4, rearBrakePerWheel, 0.5)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
    return success
end
