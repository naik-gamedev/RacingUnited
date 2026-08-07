-- Core driving and steering controls.
function DrawVehicleDrivePanel()
    local changed = false
    SetPrototypeScenePreset("vehicle")

    UI.Text("Vehicle service available: " .. tostring(Vehicle.IsAvailable()))
    UI.Text(string.format("Speed: %.2f m/s  |  %.1f km/h", vehicleSpeed, vehicleSpeed * 3.6))
    UI.Text(string.format("Grounded wheels: %d / %d", vehicleGroundedWheels,
        nativeVehicle ~= 0 and Vehicle.GetWheelCount(nativeVehicle) or 0))
    UI.Text(string.format("High-rate tire/suspension loop: %.0f Hz | substeps this world tick: %d",
        nativeVehicle ~= 0 and Vehicle.GetHighRateHertz(nativeVehicle) or 0.0,
        vehicleLastHighRateSteps))
    UI.Text(string.format("Steering input / target / centre: %.2f / %.1f / %.1f deg",
        vehicleSteeringInput, vehicleSteeringTarget, vehicleSteeringCenter))
    UI.Text(string.format("Ackermann inner / outer: %.1f / %.1f deg",
        vehicleSteeringInner, vehicleSteeringOuter))
    UI.Text(string.format("Detected wheelbase / steer track: %.2f / %.2f m",
        vehicleDetectedWheelbase, vehicleDetectedSteerTrack))
    UI.TextWrapped("W throttle | S brake | A/D steer | Left Shift handbrake | E/Q shift | R reverse | N neutral")

    vehicleHighRateHertz, changed = UI.SliderFloat(
        "Vehicle tire/suspension update rate", vehicleHighRateHertz, 240.0, 1200.0, "%.0f Hz")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        if Vehicle.SetHighRateHertz(nativeVehicle, vehicleHighRateHertz) then
            vehicleMessage = "Changed the independent vehicle high-rate solver"
        else
            vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        end
    end

    vehicleMaximumSteerAngle, changed = UI.SliderFloat(
        "Maximum steering lock", vehicleMaximumSteerAngle, 20.0, 60.0, "%.1f degrees")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetTuning(nativeVehicle, vehicleMaximumDriveForce,
            vehicleMaximumBrakeForce, vehicleMaximumSteerAngle,
            vehicleTireFriction, vehicleLateralStiffness, vehicleRollingResistance)
    end

    vehicleAckermannPercent, changed = UI.SliderFloat(
        "Ackermann geometry", vehicleAckermannPercent, -0.50, 1.50, "%.2f")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetSteeringGeometry(nativeVehicle, vehicleAckermannPercent,
            vehicleSteeringRate, vehicleSteeringReturnRate,
            vehicleHighSpeedSteeringRateFactor, vehicleHighSpeedReferenceMps)
    end

    vehicleSteeringRate, changed = UI.SliderFloat(
        "Road-wheel steering rate", vehicleSteeringRate, 60.0, 720.0, "%.0f deg/s")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetSteeringGeometry(nativeVehicle, vehicleAckermannPercent,
            vehicleSteeringRate, vehicleSteeringReturnRate,
            vehicleHighSpeedSteeringRateFactor, vehicleHighSpeedReferenceMps)
    end

    vehicleMaximumDriveForce, changed = UI.SliderFloat(
        "Maximum drive force", vehicleMaximumDriveForce, 1000.0, 16000.0, "%.0f N")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetTuning(nativeVehicle, vehicleMaximumDriveForce,
            vehicleMaximumBrakeForce, vehicleMaximumSteerAngle,
            vehicleTireFriction, vehicleLateralStiffness, vehicleRollingResistance)
    end

    if UI.Button("RESET ON DRY ASPHALT") then ResetNativeVehicle() end
    UI.TextDisabled(vehicleMessage)
end
