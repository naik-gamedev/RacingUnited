-- Interactive front-end for the native high-rate Vehicle Dynamics Laboratory.
local function DynamicsLabSeries(metric, wheel)
    return {
        Vehicle.GetDynamicsLabSeries(
            nativeVehicle,
            metric,
            wheel or vehicleDynamicsLab.selectedWheel,
            vehicleDynamicsLab.plotPoints)
    }
end

function DrawVehicleDynamicsLabPanel()
    SetPrototypeScenePreset("vehicle")
    UI.TextWrapped("Native fixed-step recorder with repeatable settle, drop, braking and turn-then-brake experiments. Automatic tests temporarily own the controls; manual recording does not.")
    UI.Spacing()

    local changed = false
    vehicleDynamicsLab.durationSeconds, changed = UI.SliderFloat(
        "Manual capture duration", vehicleDynamicsLab.durationSeconds,
        2.0, 60.0, "%.1f seconds")
    vehicleDynamicsLab.captureHertz, changed = UI.SliderFloat(
        "Capture rate", vehicleDynamicsLab.captureHertz,
        120.0, math.min(1000.0, vehicleHighRateHertz), "%.0f Hz")

    if UI.Button("RECORD MANUAL DRIVE") then StartManualDynamicsLabCapture() end
    UI.SameLine()
    if UI.Button("STOP") then StopDynamicsLabCapture() end
    UI.SameLine()
    if UI.Button("CLEAR") then ClearDynamicsLabCapture() end

    if UI.Button("PARKED SETTLE") then StartParkedSettleLab() end
    UI.SameLine()
    if UI.Button("DROP 250 MM") then StartDropTestLab() end
    if UI.Button("STRAIGHT BRAKE") then StartStraightBrakeLab() end
    UI.SameLine()
    if UI.Button("TURN + BRAKE") then StartTurnBrakeLab() end
    UI.SameLine()
    if UI.Button("EXPORT CSV") then ExportDynamicsLabCapture() end

    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        UI.TextDisabled("No native vehicle is available.")
        return
    end

    local recording, complete, samples, capacity, wheelCount,
        duration, captureHertz, peakSpeed, peakRoll, peakPitch, peakYaw,
        peakSuspensionSpeed, peakSlipRatio, peakSlipAngle, peakGrip,
        minimumLoad, maximumLoad, contactLosses, peakTravelStopForce,
        peakDamperDissipation =
        Vehicle.GetDynamicsLabSummary(nativeVehicle)

    samples = samples or 0
    capacity = capacity or 0
    wheelCount = wheelCount or Vehicle.GetWheelCount(nativeVehicle)
    duration = duration or 0.0
    local progress = capacity > 0 and samples / capacity or 0.0
    UI.ProgressBar(progress, -1.0, 0.0, string.format(
        "%d / %d samples | %.3f s", samples, capacity, duration))
    UI.Text(string.format("Scenario: %s | recording=%s | complete=%s | %.0f Hz",
        vehicleDynamicsLab.scenarioLabel, tostring(recording),
        tostring(complete), captureHertz or 0.0))
    UI.Text(string.format("Peaks: %.1f km/h | roll/pitch/yaw %.2f / %.2f / %.2f deg/s",
        peakSpeed or 0.0, peakRoll or 0.0, peakPitch or 0.0, peakYaw or 0.0))
    UI.Text(string.format("Suspension %.3f m/s | slip %.3f / %.2f deg | grip %.1f%%",
        peakSuspensionSpeed or 0.0, peakSlipRatio or 0.0,
        peakSlipAngle or 0.0, peakGrip or 0.0))
    UI.Text(string.format("Grounded load %.0f..%.0f N | contact-loss events %d",
        minimumLoad or 0.0, maximumLoad or 0.0, contactLosses or 0))
    UI.Text(string.format("Travel-stop peak %.0f N | damper dissipation peak %.0f W",
        peakTravelStopForce or 0.0, peakDamperDissipation or 0.0))
    UI.TextDisabled(vehicleDynamicsLab.message)

    if samples < 2 then
        UI.TextDisabled("Plots appear after at least two native samples are captured.")
        return
    end

    vehicleDynamicsLab.selectedWheel, changed = UI.InputInt(
        "Plotted wheel (1-based)", vehicleDynamicsLab.selectedWheel)
    vehicleDynamicsLab.selectedWheel = math.max(
        1, math.min(wheelCount, vehicleDynamicsLab.selectedWheel))

    local speed = DynamicsLabSeries("speed_kph", 1)
    local yaw = DynamicsLabSeries("yaw_rate_degps", 1)
    local roll = DynamicsLabSeries("roll_rate_degps", 1)
    local load = DynamicsLabSeries("wheel_normal_force_n")
    local travel = DynamicsLabSeries("wheel_compression_mm")
    local springForce = DynamicsLabSeries("wheel_suspension_spring_force_n")
    local damperForce = DynamicsLabSeries("wheel_suspension_damping_force_n")
    local bumpStopForce = DynamicsLabSeries(
        "wheel_suspension_bump_stop_force_n")
    local damperPower = DynamicsLabSeries("wheel_damper_dissipation_w")
    local slip = DynamicsLabSeries("wheel_slip_ratio")
    local grip = DynamicsLabSeries("wheel_grip_percent")

    UI.PlotLines("Speed (km/h)", 72.0, table.unpack(speed))
    UI.PlotLines("Yaw rate (deg/s)", 72.0, table.unpack(yaw))
    UI.PlotLines("Roll rate (deg/s)", 72.0, table.unpack(roll))
    UI.PlotLines("Selected-wheel normal load (N)", 72.0, table.unpack(load))
    UI.PlotLines("Selected-wheel compression (mm)", 72.0, table.unpack(travel))
    UI.PlotLines("Selected-wheel spring force (N)", 72.0, table.unpack(springForce))
    UI.PlotLines("Selected-wheel damper force (N)", 72.0, table.unpack(damperForce))
    UI.PlotLines("Selected-wheel bump-stop force (N)", 72.0, table.unpack(bumpStopForce))
    UI.PlotLines("Selected-wheel damper dissipation (W)", 72.0, table.unpack(damperPower))
    UI.PlotLines("Selected-wheel slip ratio", 72.0, table.unpack(slip))
    UI.PlotLines("Selected-wheel grip utilization (%)", 72.0, table.unpack(grip))
end
