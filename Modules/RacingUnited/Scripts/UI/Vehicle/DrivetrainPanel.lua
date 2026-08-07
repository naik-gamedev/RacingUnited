-- Engine, gearbox and differential controls.
function DrawVehicleDrivetrainPanel()
    local changed = false
    SetPrototypeScenePreset("vehicle")

    UI.Text(string.format("Gear: %s | requested: %s | forward gears: %d",
        VehicleGearName(vehicleCurrentGear), VehicleGearName(vehicleRequestedGear), vehicleForwardGearCount))
    UI.Text(string.format("Engine: %.0f RPM | torque: %.1f Nm", vehicleEngineRpm, vehicleEngineTorque))
    UI.ProgressBar(math.max(0.0, math.min(1.0, vehicleEngineRpm / vehicleRedlineRpm)),
        420, 18, "Engine RPM")
    UI.Text(string.format("Clutch: %.2f engaged | slip: %.0f RPM",
        vehicleClutchEngagement, vehicleClutchSlipRpm))
    UI.Text(string.format("Gear ratio / final drive: %.3f / %.3f | axle output: %.1f Nm",
        vehicleSelectedGearRatio, vehicleFinalDriveRatio, vehicleOutputTorque))
    UI.Text(string.format("Differential: %s | driven-wheel delta: %.1f RPM",
        VehicleDifferentialName(vehicleDifferentialMode), vehicleDrivenWheelSpeedDifferenceRpm))

    vehicleMaximumEngineTorque, changed = UI.SliderFloat(
        "Maximum engine torque", vehicleMaximumEngineTorque, 50.0, 1200.0, "%.0f Nm")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetPowertrain(nativeVehicle, vehicleIdleRpm, vehicleRedlineRpm,
            vehicleMaximumEngineTorque, vehicleEngineBrakingTorque,
            vehicleFinalDriveRatio, vehicleDrivetrainEfficiency,
            vehicleShiftDuration, vehicleClutchEngagementRate)
    end

    vehicleFinalDriveRatio, changed = UI.SliderFloat(
        "Final drive ratio", vehicleFinalDriveRatio, 2.0, 6.5, "%.2f")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetPowertrain(nativeVehicle, vehicleIdleRpm, vehicleRedlineRpm,
            vehicleMaximumEngineTorque, vehicleEngineBrakingTorque,
            vehicleFinalDriveRatio, vehicleDrivetrainEfficiency,
            vehicleShiftDuration, vehicleClutchEngagementRate)
    end

    vehicleDifferentialBias, changed = UI.SliderFloat(
        "Limited-slip torque bias", vehicleDifferentialBias, 1.0, 6.0, "%.2f:1")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetDifferential(nativeVehicle, vehicleDifferentialMode, vehicleDifferentialBias)
    end

    if UI.Button("SHIFT DOWN - Q") then
        ReportVehicleGearChange(Vehicle.ShiftDown(nativeVehicle), "Requested the next lower gear")
    end
    if UI.Button("SHIFT UP - E") then
        ReportVehicleGearChange(Vehicle.ShiftUp(nativeVehicle), "Requested the next higher gear")
    end
    if UI.Button("SELECT REVERSE - R") then
        ReportVehicleGearChange(Vehicle.SetGear(nativeVehicle, -1), "Requested reverse gear")
    end
    if UI.Button("SELECT NEUTRAL - N") then
        ReportVehicleGearChange(Vehicle.SetGear(nativeVehicle, 0), "Requested neutral")
    end
    if UI.Button("SELECT FIRST GEAR") then
        ReportVehicleGearChange(Vehicle.SetGear(nativeVehicle, 1), "Requested first gear")
    end
    UI.TextDisabled(vehicleMessage)
end
