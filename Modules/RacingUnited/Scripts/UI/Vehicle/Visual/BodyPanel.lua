-- Step 29J.2 body-mesh slot. Creator-authored geometry is authoritative:
-- no routine body translation/rotation/scale controls are exposed anymore.
function DrawVehicleVisualBodyPanel()
    local changed = false
    local meshChanged = false

    UI.TextDisabled("PLAYER BODY VISUAL")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("PlayerCar.obj remains the temporary creator-owned body slot until the glTF vehicle pipeline. Geometry is expected at authored 1:1 size and authored placement; Heritage Engine no longer asks you to visually resize the car to make it fit.")
    UI.Text("Asset: " .. tostring(vehicleVisual.assetPath))
    UI.Text("Mode: " .. (vehicleVisual.usingFallback and "fallback prototype mesh" or "player-car slot"))
    UI.Text("Runtime transform: identity / 1:1")
    UI.TextDisabled("If geometry is positioned or sized incorrectly, fix the source scene in Blender rather than compensating here.")
    UI.Spacing()

    if UI.Button("USE PLAYER CAR OBJ") then
        UsePlayerVehicleVisual()
    end
    UI.SameLine()
    if UI.Button("USE FALLBACK LOW-POLY") then
        UseFallbackVehicleVisual()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("PRESENTATION")

    vehicleVisual.hideProxyWheels, changed = UI.Checkbox(
        "Hide prototype wheel cylinders", vehicleVisual.hideProxyWheels)
    if changed then
        SetVehicleProxyWheelPreference(vehicleVisual.hideProxyWheels)
    end

    vehicleVisual.doubleSided, changed = UI.Checkbox(
        "Double-sided body mesh", vehicleVisual.doubleSided)
    meshChanged = meshChanged or changed

    vehicleVisual.normalize, changed = UI.Checkbox(
        "Normalize OBJ to unit size (diagnostic only)", vehicleVisual.normalize)
    meshChanged = meshChanged or changed

    vehicleVisual.color[1], changed = UI.SliderFloat(
        "Body color R", vehicleVisual.color[1], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleVisual.color[2], changed = UI.SliderFloat(
        "Body color G", vehicleVisual.color[2], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleVisual.color[3], changed = UI.SliderFloat(
        "Body color B", vehicleVisual.color[3], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed

    if meshChanged then
        ApplyVehicleVisualMesh()
    end

    UI.Spacing()
    UI.TextDisabled("Tip: double-click any numeric slider in the prototype UI to type an exact value and press Enter.")

    if UI.Button("RESET BODY TO AUTHORED IDENTITY") then
        ResetVehicleVisualTuning()
    end
    UI.SameLine()
    if UI.Button("RESET CAR ON DRY ASPHALT") then
        ResetNativeVehicle()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextWrapped("Body slot: Modules\\RacingUnited\\Assets\\Vehicles\\Player\\PlayerCar.obj")
    UI.TextDisabled(vehicleVisualMessage)
end
