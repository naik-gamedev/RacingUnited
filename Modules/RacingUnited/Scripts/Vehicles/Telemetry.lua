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
        -- CLEAN01: the native bridge returns one named table containing the
        -- complete wheel/contact/upright snapshot. This replaces the fragile
        -- 169-value positional ABI in first-party Racing United scripts.
        local telemetry = Vehicle.GetWheelTelemetry(nativeVehicle, index) or {}
        if VehicleAlignmentToWorkshopConvention ~= nil then
            telemetry.workshopCamberDegrees,
                telemetry.workshopToeDegrees =
                VehicleAlignmentToWorkshopConvention(
                    index,
                    telemetry.camberAngleDegrees,
                    telemetry.toeAngleDegrees)
        end
        vehicleWheelTelemetry[index] = telemetry
    end
    if RefreshVehicleGeometryMeasurement ~= nil then
        RefreshVehicleGeometryMeasurement()
    end

    -- ROLL02: keep anti-roll state synchronized with the same live chassis and
    -- wheel telemetry frame used for load-transfer diagnostics.
    if RefreshAntiRollBarTelemetry ~= nil then
        RefreshAntiRollBarTelemetry()
    end
    if RefreshChassisFlexTelemetry ~= nil then
        RefreshChassisFlexTelemetry()
    end
end
