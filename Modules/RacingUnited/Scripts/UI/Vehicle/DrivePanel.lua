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
    local rawLeft, rawRight = ReadVehicleSteeringActions()
    UI.Text(string.format(
        "Raw steer actions L / R: %.2f / %.2f   | native input: %.2f (-L / +R)",
        rawLeft, rawRight, vehicleSteeringInput))
    local fl = vehicleWheelTelemetry[1]
    local fr = vehicleWheelTelemetry[2]
    if fl ~= nil and fr ~= nil then
        UI.Text(string.format(
            "Front road-wheel angles FL / FR: %.2f / %.2f deg",
            fl.steerAngle or 0.0, fr.steerAngle or 0.0))
        UI.Text(string.format(
            "Front forward X/Z FL: %.3f/%.3f  FR: %.3f/%.3f",
            fl.wheelForwardX or 0.0, fl.wheelForwardZ or 1.0,
            fr.wheelForwardX or 0.0, fr.wheelForwardZ or 1.0))
    end
    UI.Text(string.format("Detected wheelbase / steer track: %.3f / %.3f m",
        vehicleDetectedWheelbase, vehicleDetectedSteerTrack))
    local actionThrottle = Input.Value("Throttle")
    local actionBrake = Input.Value("Brake")
    local keyboardThrottle, keyboardBrake = ReadVehicleDriveKeyboardInputs()
    local resolvedThrottle, resolvedBrake = ReadVehicleDriveInputs()
    UI.Text(string.format(
        "Drive T/B action %.2f/%.2f | keyboard %d/%d | resolved %.2f/%.2f",
        actionThrottle, actionBrake,
        keyboardThrottle and 1 or 0, keyboardBrake and 1 or 0,
        resolvedThrottle, resolvedBrake))
    UI.TextWrapped("Throttle: " .. Input.GetBinding("Throttle")
        .. " | Brake: " .. Input.GetBinding("Brake"))
    UI.TextWrapped("Steer left: " .. Input.GetBinding("Steer Left")
        .. " | Steer right: " .. Input.GetBinding("Steer Right"))

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
        ApplyVehicleSteeringTuning()
    end

    vehicleAckermannPercent, changed = UI.SliderFloat(
        "Ackermann geometry", vehicleAckermannPercent, -0.50, 1.50, "%.2f")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        ApplyVehicleSteeringTuning()
    end

    vehicleSteeringRate, changed = UI.SliderFloat(
        "Road-wheel steering rate", vehicleSteeringRate, 60.0, 720.0, "%.0f deg/s")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        ApplyVehicleSteeringTuning()
    end

    vehicleSteeringReturnRate, changed = UI.SliderFloat(
        "Steering return rate", vehicleSteeringReturnRate, 60.0, 1080.0, "%.0f deg/s")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        ApplyVehicleSteeringTuning()
    end

    vehicleHighSpeedSteeringRateFactor, changed = UI.SliderFloat(
        "High-speed steering rate factor", vehicleHighSpeedSteeringRateFactor,
        0.05, 1.0, "%.2f")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        ApplyVehicleSteeringTuning()
    end

    vehicleHighSpeedReferenceMps, changed = UI.SliderFloat(
        "High-speed steering reference", vehicleHighSpeedReferenceMps,
        5.0, 100.0, "%.1f m/s")
    if changed and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        ApplyVehicleSteeringTuning()
    end

    if UI.Button("RESET STEERING FROM VEHICLE DEFINITION") then
        ResetVehicleSteeringTuning()
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
