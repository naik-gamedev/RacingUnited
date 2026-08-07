-- Step 29H: focused live tire telemetry for the front-left wheel.
function DrawVehicleTiresLivePanel()
    if vehicleWheelTelemetry[1] == nil then
        UI.TextDisabled("No tire telemetry is available yet.")
        return
    end

    local wheel = vehicleWheelTelemetry[1]
    UI.TextDisabled("FRONT-LEFT LIVE TIRE | profile: " .. (vehicleWheelTireProfileNames[1] or "unknown"))
    UI.Text(string.format(
        "Grounded: %s | normal load: %.0f N | surface: %s",
        tostring(wheel.grounded), wheel.normalForce, wheel.surfaceName))
    UI.Text(string.format(
        "Raw slip ratio / angle: %.3f / %.2f deg",
        wheel.slipRatio, wheel.slipAngleDegrees))
    UI.Text(string.format(
        "Relaxed slip ratio / angle: %.3f / %.2f deg",
        wheel.relaxedSlipRatio, wheel.relaxedSlipAngleDegrees))
    UI.Text(string.format(
        "Pure Fx / Fy: %.0f / %.0f N",
        wheel.pureLongitudinalForce, wheel.pureLateralForce))
    UI.Text(string.format(
        "Combined Fx / Fy: %.0f / %.0f N",
        wheel.longitudinalForce, wheel.lateralForce))
    UI.Text(string.format(
        "Combined scale / grip used: %.3f / %.1f%%",
        wheel.combinedSlipScale, wheel.gripUtilization * 100.0))
    UI.Text(string.format(
        "Effective friction: %.3f | pneumatic trail: %.4f m",
        wheel.effectiveFriction, wheel.pneumaticTrail))
    UI.Text(string.format("Aligning torque: %.1f Nm", wheel.aligningTorque))

    UI.Spacing()
    UI.TextDisabled("Pure forces show what each slip direction requested before combined-slip sharing.")
    UI.TextDisabled("Combined scale falls below 1.0 when acceleration/braking and cornering compete for grip.")
end
