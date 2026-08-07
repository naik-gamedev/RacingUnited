-- Per-wheel contacted-collider surface diagnostics.
function DrawVehicleSurfacesPanel()
    SetPrototypeScenePreset("surface")

    UI.TextDisabled("PER-WHEEL SURFACE DETECTION - STEP 29F")
    UI.TextWrapped("Each suspension ray reads the material and wetness from the exact collider beneath that tire. Different tires may contact different surfaces simultaneously.")
    UI.Spacing()

    if UI.Button("RESET ON SPLIT GRIP - LEFT ASPHALT / RIGHT ICE") then
        ResetNativeVehicleSplitGrip()
    end
    if UI.Button("RESET BEFORE SURFACE RUNWAY") then
        ResetNativeVehicleSurfaceRunway()
    end
    if UI.Button("RESET ON DRY ASPHALT") then
        ResetNativeVehicle()
        SetPrototypeScenePreset("surface")
    end

    UI.Spacing()
    if vehicleWheelTelemetry[1] ~= nil then
        local labels = { "FL", "FR", "RL", "RR" }
        UI.Text("Contacted surface by wheel:")
        for index = 1, math.min(4, #vehicleWheelTelemetry) do
            local wheel = vehicleWheelTelemetry[index]
            UI.Text(string.format("%s: %-12s | wet %.0f%% | collider %s",
                labels[index], VehicleDetectedSurfaceLabel(wheel),
                wheel.surfaceWetness * 100.0, tostring(wheel.contactCollider)))
        end
        UI.Spacing()
        UI.Text(string.format("FL effective friction: %.3f | grip used: %.1f%%",
            vehicleWheelTelemetry[1].effectiveFriction,
            vehicleWheelTelemetry[1].gripUtilization * 100.0))
        if vehicleWheelTelemetry[2] ~= nil then
            UI.Text(string.format("FR effective friction: %.3f | grip used: %.1f%%",
                vehicleWheelTelemetry[2].effectiveFriction,
                vehicleWheelTelemetry[2].gripUtilization * 100.0))
        end
    else
        UI.TextDisabled("Wheel telemetry is not available yet.")
    end

    UI.Spacing()
    UI.TextDisabled("Runway order: wet asphalt -> gravel -> dirt -> grass -> snow -> ice")
    UI.TextDisabled(vehicleMessage)
end
