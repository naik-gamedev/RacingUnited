-- Compact per-wheel live telemetry.
function DrawVehicleTelemetryPanel()
    SetPrototypeScenePreset("vehicle")
    UI.Text(string.format("Speed %.1f km/h | grounded %d | total 1000 Hz steps %d",
        vehicleSpeed * 3.6, vehicleGroundedWheels, vehicleTotalHighRateSteps))
    UI.TextDisabled("One compact block per wheel; deeper subsystem data lives in its own tab.")
    UI.Spacing()

    local labels = { "FRONT LEFT", "FRONT RIGHT", "REAR LEFT", "REAR RIGHT" }
    for index, wheel in ipairs(vehicleWheelTelemetry) do
        UI.TextDisabled(labels[index] or ("WHEEL " .. tostring(index)))
        UI.Text(string.format("surface=%s wet=%.0f%% grounded=%s",
            VehicleDetectedSurfaceLabel(wheel), wheel.surfaceWetness * 100.0, tostring(wheel.grounded)))
        UI.Text(string.format("load=%.0f N | Fx/Fy=%.0f / %.0f N | steer=%.1f deg",
            wheel.normalForce, wheel.longitudinalForce, wheel.lateralForce, wheel.steerAngle))
        UI.Text(string.format("slip=%.3f / %.2f deg | grip=%.1f%% | Mz=%.1f Nm",
            wheel.relaxedSlipRatio, wheel.relaxedSlipAngleDegrees,
            wheel.gripUtilization * 100.0, wheel.aligningTorque))
        UI.Spacing()
    end
    UI.TextDisabled(vehicleMessage)
end
