-- Live per-wheel bridge for the native nonlinear suspension provider.
local function VehicleSuspensionAvailable()
    return nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle)
end

function ReadVehicleSuspensionModel(wheelIndex)
    if not VehicleSuspensionAvailable() then
        return false
    end

    local provider, springPreloadN, springRateNPerM,
        springProgressionNPerM2, bumpDampingNsPerM,
        bumpHighSpeedDampingNsPerM, bumpDampingKneeVelocityMps,
        reboundDampingNsPerM, reboundHighSpeedDampingNsPerM,
        reboundDampingKneeVelocityMps, bumpStopEngagementM,
        bumpStopRateNPerM, bumpStopProgressionNPerM2,
        droopStopEngagementM, droopStopRateNPerM, motionRatio,
        maximumForceN =
        Vehicle.GetWheelSuspensionModel(nativeVehicle, wheelIndex)
    if provider == nil then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    vehicleSuspension.selectedWheel = wheelIndex
    vehicleSuspension.provider = provider
    vehicleSuspension.springPreloadN = springPreloadN
    vehicleSuspension.springRateNPerM = springRateNPerM
    vehicleSuspension.springProgressionNPerM2 = springProgressionNPerM2
    vehicleSuspension.bumpDampingNsPerM = bumpDampingNsPerM
    vehicleSuspension.bumpHighSpeedDampingNsPerM = bumpHighSpeedDampingNsPerM
    vehicleSuspension.bumpDampingKneeVelocityMps = bumpDampingKneeVelocityMps
    vehicleSuspension.reboundDampingNsPerM = reboundDampingNsPerM
    vehicleSuspension.reboundHighSpeedDampingNsPerM =
        reboundHighSpeedDampingNsPerM
    vehicleSuspension.reboundDampingKneeVelocityMps =
        reboundDampingKneeVelocityMps
    vehicleSuspension.bumpStopEngagementM = bumpStopEngagementM
    vehicleSuspension.bumpStopRateNPerM = bumpStopRateNPerM
    vehicleSuspension.bumpStopProgressionNPerM2 = bumpStopProgressionNPerM2
    vehicleSuspension.droopStopEngagementM = droopStopEngagementM
    vehicleSuspension.droopStopRateNPerM = droopStopRateNPerM
    vehicleSuspension.motionRatio = motionRatio
    vehicleSuspension.maximumForceN = maximumForceN
    return true
end

function ApplyVehicleSuspensionModel(wheelIndex)
    if not VehicleSuspensionAvailable() then
        return false
    end
    local success = Vehicle.SetWheelSuspensionModel(
        nativeVehicle,
        wheelIndex,
        vehicleSuspension.springPreloadN,
        vehicleSuspension.springRateNPerM,
        vehicleSuspension.springProgressionNPerM2,
        vehicleSuspension.bumpDampingNsPerM,
        vehicleSuspension.bumpHighSpeedDampingNsPerM,
        vehicleSuspension.bumpDampingKneeVelocityMps,
        vehicleSuspension.reboundDampingNsPerM,
        vehicleSuspension.reboundHighSpeedDampingNsPerM,
        vehicleSuspension.reboundDampingKneeVelocityMps,
        vehicleSuspension.bumpStopEngagementM,
        vehicleSuspension.bumpStopRateNPerM,
        vehicleSuspension.bumpStopProgressionNPerM2,
        vehicleSuspension.droopStopEngagementM,
        vehicleSuspension.droopStopRateNPerM,
        vehicleSuspension.motionRatio,
        vehicleSuspension.maximumForceN)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
    return success
end

function ApplyVehicleSuspensionModelToAllWheels()
    if not VehicleSuspensionAvailable() then
        return false
    end
    for index = 1, Vehicle.GetWheelCount(nativeVehicle) do
        if not ApplyVehicleSuspensionModel(index) then
            return false
        end
    end
    vehicleMessage = "Copied the selected suspension tune to every wheel"
    return true
end

function RestoreVehicleSuspensionDefinition()
    local suspension = PrototypeCarDefinition.wheelPhysics
    vehicleSuspension.springPreloadN = suspension.springPreloadN
    vehicleSuspension.springRateNPerM = suspension.springRateNPerM
    vehicleSuspension.springProgressionNPerM2 =
        suspension.springProgressionNPerM2
    vehicleSuspension.bumpDampingNsPerM = suspension.bumpDampingNsPerM
    vehicleSuspension.bumpHighSpeedDampingNsPerM =
        suspension.bumpHighSpeedDampingNsPerM
    vehicleSuspension.bumpDampingKneeVelocityMps =
        suspension.bumpDampingKneeVelocityMps
    vehicleSuspension.reboundDampingNsPerM = suspension.reboundDampingNsPerM
    vehicleSuspension.reboundHighSpeedDampingNsPerM =
        suspension.reboundHighSpeedDampingNsPerM
    vehicleSuspension.reboundDampingKneeVelocityMps =
        suspension.reboundDampingKneeVelocityMps
    vehicleSuspension.bumpStopEngagementM = suspension.bumpStopEngagementM
    vehicleSuspension.bumpStopRateNPerM = suspension.bumpStopRateNPerM
    vehicleSuspension.bumpStopProgressionNPerM2 =
        suspension.bumpStopProgressionNPerM2
    vehicleSuspension.droopStopEngagementM = suspension.droopStopEngagementM
    vehicleSuspension.droopStopRateNPerM = suspension.droopStopRateNPerM
    vehicleSuspension.motionRatio = suspension.motionRatio
    vehicleSuspension.maximumForceN = suspension.maximumForceN
    if ApplyVehicleSuspensionModelToAllWheels() then
        vehicleMessage = "Restored the prototype nonlinear suspension tune"
        return true
    end
    return false
end
