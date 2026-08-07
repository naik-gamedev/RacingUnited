-- Reads native vehicle state into Lua-owned debug telemetry.
function RefreshVehicleTelemetry()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return
    end
    vehicleSpeed = Vehicle.GetSpeed(nativeVehicle)
    vehicleGroundedWheels = Vehicle.GetGroundedWheelCount(nativeVehicle)
    vehicleLastHighRateSteps = Vehicle.GetLastHighRateStepCount(nativeVehicle)
    vehicleTotalHighRateSteps = Vehicle.GetTotalHighRateStepCount(nativeVehicle)
    vehicleSteeringInput,
        vehicleSteeringTarget,
        vehicleSteeringCenter,
        vehicleSteeringInner,
        vehicleSteeringOuter,
        vehicleDetectedWheelbase,
        vehicleDetectedSteerTrack,
        vehicleSteeringRateFactor =
        Vehicle.GetSteeringState(nativeVehicle)
    vehicleSteeringInput = vehicleSteeringInput or 0.0
    vehicleSteeringTarget = vehicleSteeringTarget or 0.0
    vehicleSteeringCenter = vehicleSteeringCenter or 0.0
    vehicleSteeringInner = vehicleSteeringInner or 0.0
    vehicleSteeringOuter = vehicleSteeringOuter or 0.0
    vehicleDetectedWheelbase = vehicleDetectedWheelbase or 0.0
    vehicleDetectedSteerTrack = vehicleDetectedSteerTrack or 0.0
    vehicleSteeringRateFactor = vehicleSteeringRateFactor or 1.0
    vehicleCurrentGear,
        vehicleRequestedGear,
        vehicleShifting,
        vehicleShiftTimeRemaining,
        vehicleEngineRpm,
        vehicleEngineTorque,
        vehicleClutchEngagement,
        vehicleClutchSlipRpm,
        vehicleWheelCoupledRpm,
        vehicleSelectedGearRatio,
        vehicleFinalDriveRatio,
        vehicleOutputTorque,
        vehicleDrivenWheelSpeedDifferenceRpm,
        vehicleDifferentialMode =
        Vehicle.GetDrivetrainState(nativeVehicle)
    vehicleCurrentGear = vehicleCurrentGear or 0
    vehicleRequestedGear = vehicleRequestedGear or vehicleCurrentGear
    vehicleShifting = vehicleShifting or false
    vehicleShiftTimeRemaining = vehicleShiftTimeRemaining or 0.0
    vehicleEngineRpm = vehicleEngineRpm or vehicleIdleRpm
    vehicleEngineTorque = vehicleEngineTorque or 0.0
    vehicleClutchEngagement = vehicleClutchEngagement or 0.0
    vehicleClutchSlipRpm = vehicleClutchSlipRpm or 0.0
    vehicleWheelCoupledRpm = vehicleWheelCoupledRpm or 0.0
    vehicleSelectedGearRatio = vehicleSelectedGearRatio or 0.0
    vehicleFinalDriveRatio = vehicleFinalDriveRatio or 0.0
    vehicleOutputTorque = vehicleOutputTorque or 0.0
    vehicleDrivenWheelSpeedDifferenceRpm =
        vehicleDrivenWheelSpeedDifferenceRpm or 0.0
    vehicleDifferentialMode = vehicleDifferentialMode or 0
    vehicleForwardGearCount = Vehicle.GetForwardGearCount(nativeVehicle)
    vehicleDriverAids.absEnabled,
        vehicleDriverAids.tractionControlEnabled,
        vehicleDriverAids.absActiveWheels,
        vehicleDriverAids.tractionActiveWheels,
        vehicleDriverAids.absTargetSlip,
        vehicleDriverAids.tractionTargetSlip,
        vehicleDriverAids.minimumSpeed,
        vehicleDriverAids.handbrakeInput =
        Vehicle.GetDriverAidState(nativeVehicle)
    vehicleDriverAids.absEnabled = vehicleDriverAids.absEnabled ~= false
    vehicleDriverAids.tractionControlEnabled =
        vehicleDriverAids.tractionControlEnabled ~= false
    vehicleDriverAids.absActiveWheels = vehicleDriverAids.absActiveWheels or 0
    vehicleDriverAids.tractionActiveWheels = vehicleDriverAids.tractionActiveWheels or 0
    vehicleDriverAids.absTargetSlip = vehicleDriverAids.absTargetSlip or 0.16
    vehicleDriverAids.tractionTargetSlip = vehicleDriverAids.tractionTargetSlip or 0.12
    vehicleDriverAids.minimumSpeed = vehicleDriverAids.minimumSpeed or 2.5
    vehicleDriverAids.handbrakeInput = vehicleDriverAids.handbrakeInput or 0.0
    vehicleWheelTelemetry = {}
    for index = 1, Vehicle.GetWheelCount(nativeVehicle) do
        local grounded, length, compression, compressionVelocity, normalForce,
            longitudinalForce, lateralForce, steerAngle, angularVelocity,
            rotationDegrees, centerX, centerY, centerZ,
            contactX, contactY, contactZ, longitudinalSpeed, lateralSpeed,
            slipRatio, slipAngleDegrees, relaxedSlipRatio,
            relaxedSlipAngleDegrees, effectiveFriction, gripUtilization,
            pureLongitudinalForce, pureLateralForce, combinedSlipScale,
            pneumaticTrail, aligningTorque, driveTorque, brakeTorque,
            serviceBrakeTorque, handbrakeTorque, antiLockModulation,
            tractionControlModulation, antiLockActive,
            tractionControlActive, contactCollider, surfaceName, surfaceId,
            surfaceWetness, suspensionSpringForce, suspensionDampingForce,
            suspensionBumpStopForce, suspensionDroopStopForce,
            suspensionUnclampedForce, damperDissipationWatts =
            Vehicle.GetWheelState(nativeVehicle, index)
        vehicleWheelTelemetry[index] = {
            grounded = grounded,
            length = length or 0.0,
            compression = compression or 0.0,
            compressionVelocity = compressionVelocity or 0.0,
            normalForce = normalForce or 0.0,
            longitudinalForce = longitudinalForce or 0.0,
            lateralForce = lateralForce or 0.0,
            steerAngle = steerAngle or 0.0,
            angularVelocity = angularVelocity or 0.0,
            rotationDegrees = rotationDegrees or 0.0,
            centerX = centerX or 0.0,
            centerY = centerY or 0.0,
            centerZ = centerZ or 0.0,
            contactX = contactX or 0.0,
            contactY = contactY or 0.0,
            contactZ = contactZ or 0.0,
            longitudinalSpeed = longitudinalSpeed or 0.0,
            lateralSpeed = lateralSpeed or 0.0,
            slipRatio = slipRatio or 0.0,
            slipAngleDegrees = slipAngleDegrees or 0.0,
            relaxedSlipRatio = relaxedSlipRatio or 0.0,
            relaxedSlipAngleDegrees = relaxedSlipAngleDegrees or 0.0,
            effectiveFriction = effectiveFriction or 0.0,
            gripUtilization = gripUtilization or 0.0,
            pureLongitudinalForce = pureLongitudinalForce or 0.0,
            pureLateralForce = pureLateralForce or 0.0,
            combinedSlipScale = combinedSlipScale or 1.0,
            pneumaticTrail = pneumaticTrail or 0.0,
            aligningTorque = aligningTorque or 0.0,
            driveTorque = driveTorque or 0.0,
            brakeTorque = brakeTorque or 0.0,
            serviceBrakeTorque = serviceBrakeTorque or 0.0,
            handbrakeTorque = handbrakeTorque or 0.0,
            antiLockModulation = antiLockModulation or 1.0,
            tractionControlModulation =
                tractionControlModulation or 1.0,
            antiLockActive = antiLockActive or false,
            tractionControlActive = tractionControlActive or false,
            contactCollider = contactCollider or 0,
            surfaceName = surfaceName or "default",
            surfaceId = surfaceId or 0,
            surfaceWetness = surfaceWetness or 0.0,
            suspensionSpringForce = suspensionSpringForce or 0.0,
            suspensionDampingForce = suspensionDampingForce or 0.0,
            suspensionBumpStopForce = suspensionBumpStopForce or 0.0,
            suspensionDroopStopForce = suspensionDroopStopForce or 0.0,
            suspensionUnclampedForce = suspensionUnclampedForce or 0.0,
            damperDissipationWatts = damperDissipationWatts or 0.0
        }
    end
end
