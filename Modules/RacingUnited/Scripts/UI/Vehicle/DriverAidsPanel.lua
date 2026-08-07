-- ABS, traction-control and brake distribution controls.
function DrawVehicleDriverAidsPanel()
    local changed = false
    SetPrototypeScenePreset("vehicle")

    UI.Text(string.format("ABS: %s (%d active) | TCS: %s (%d active)",
        tostring(vehicleDriverAids.absEnabled), vehicleDriverAids.absActiveWheels,
        tostring(vehicleDriverAids.tractionControlEnabled), vehicleDriverAids.tractionActiveWheels))
    UI.Text(string.format("Brake bias: %.0f%% front | handbrake input: %.2f",
        vehicleDriverAids.frontBrakeBias * 100.0, vehicleDriverAids.handbrakeInput))

    vehicleDriverAids.absEnabled, changed = UI.Checkbox(
        "Anti-lock braking system", vehicleDriverAids.absEnabled)
    if changed then
        ApplyVehicleDriverAids()
        vehicleMessage = vehicleDriverAids.absEnabled and "ABS enabled"
            or "ABS disabled: wheels can now lock under braking"
    end

    vehicleDriverAids.tractionControlEnabled, changed = UI.Checkbox(
        "Traction control", vehicleDriverAids.tractionControlEnabled)
    if changed then
        ApplyVehicleDriverAids()
        vehicleMessage = vehicleDriverAids.tractionControlEnabled and "Traction control enabled"
            or "Traction control disabled: driven wheels can spin freely"
    end

    vehicleDriverAids.absTargetSlip, changed = UI.SliderFloat(
        "ABS target braking slip", vehicleDriverAids.absTargetSlip, 0.05, 0.40, "%.3f")
    if changed then ApplyVehicleDriverAids() end
    vehicleDriverAids.tractionTargetSlip, changed = UI.SliderFloat(
        "TCS target drive slip", vehicleDriverAids.tractionTargetSlip, 0.05, 0.50, "%.3f")
    if changed then ApplyVehicleDriverAids() end
    vehicleDriverAids.minimumSpeed, changed = UI.SliderFloat(
        "Driver-aid minimum activation speed", vehicleDriverAids.minimumSpeed, 0.0, 10.0, "%.2f m/s")
    if changed then ApplyVehicleDriverAids() end
    vehicleDriverAids.modulationRate, changed = UI.SliderFloat(
        "ABS / traction modulation rate", vehicleDriverAids.modulationRate, 2.0, 60.0, "%.1f /s")
    if changed then ApplyVehicleDriverAids() end
    vehicleDriverAids.maximumHandbrakeTorque, changed = UI.SliderFloat(
        "Maximum rear handbrake torque", vehicleDriverAids.maximumHandbrakeTorque,
        500.0, 8000.0, "%.0f Nm")
    if changed then ApplyVehicleDriverAids() end
    vehicleDriverAids.frontBrakeBias, changed = UI.SliderFloat(
        "Service-brake front bias", vehicleDriverAids.frontBrakeBias, 0.45, 0.80, "%.2f")
    if changed then
        ApplyVehicleBrakeBias()
        vehicleMessage = string.format("Set service-brake bias to %.0f%% front",
            vehicleDriverAids.frontBrakeBias * 100.0)
    end

    if vehicleWheelTelemetry[1] ~= nil then
        local fl = vehicleWheelTelemetry[1]
        UI.Text(string.format("FL brake: %.0f Nm | ABS pressure: %.0f%% | active=%s",
            fl.serviceBrakeTorque, fl.antiLockModulation * 100.0, tostring(fl.antiLockActive)))
    end
    if vehicleWheelTelemetry[3] ~= nil then
        local rl = vehicleWheelTelemetry[3]
        UI.Text(string.format("RL drive / handbrake: %.0f / %.0f Nm | TCS torque: %.0f%% | active=%s",
            rl.driveTorque, rl.handbrakeTorque,
            rl.tractionControlModulation * 100.0, tostring(rl.tractionControlActive)))
    end
    UI.TextDisabled(vehicleMessage)
end
