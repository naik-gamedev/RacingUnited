-- Step 29J.2 articulated wheel controls. Wheel dimensions come from Blender;
-- the UI no longer applies convenience radius/width scaling to creator meshes.
function DrawVehicleVisualWheelsPanel()
    local changed = false
    local meshChanged = false
    local geometry = PrototypeCarDefinition.referenceGeometry

    UI.TextDisabled("ARTICULATED WHEELS - AUTHORED 1:1")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Each wheel is placed at the native suspension wheel center and rotates around its own mesh origin. Creator wheel size is now authoritative: radius scale = 1.0 and width scale = 1.0 by default.")
    UI.TextWrapped("Do not resize a correct wheel in Heritage Engine. Fix dimensions and origin placement in Blender.")
    UI.Spacing()

    vehicleWheelVisual.enabled, changed = UI.Checkbox(
        "Enable articulated wheel meshes", vehicleWheelVisual.enabled)
    if changed then
        SetArticulatedWheelVisualsEnabled(vehicleWheelVisual.enabled)
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("2003 PEUGEOT 206 RC REFERENCE GEOMETRY")
    UI.Text(string.format("Wheelbase: %.0f mm", geometry.wheelbaseM * 1000.0))
    UI.Text(string.format("Front track: %.0f mm", geometry.frontTrackM * 1000.0))
    UI.Text(string.format("Rear track: %.0f mm", geometry.rearTrackM * 1000.0))
    UI.Text("Wheel / tire: " .. geometry.rimSize .. " | " .. geometry.tireSize)
    UI.Text(string.format("Reference tire radius: %.1f mm", geometry.wheelRadiusM * 1000.0))

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("WHEEL MESH")
    UI.Text("Scale: authored 1.0000 x / 1.0000 x")
    UI.Text("FL: " .. tostring(vehicleWheelVisual.assetPaths[1]))
    UI.Text("FR: " .. tostring(vehicleWheelVisual.assetPaths[2]))
    UI.Text("RL: " .. tostring(vehicleWheelVisual.assetPaths[3]))
    UI.Text("RR: " .. tostring(vehicleWheelVisual.assetPaths[4]))
    vehicleWheelVisual.doubleSided, changed = UI.Checkbox(
        "Double-sided wheel mesh", vehicleWheelVisual.doubleSided)
    meshChanged = meshChanged or changed
    vehicleWheelVisual.normalize, changed = UI.Checkbox(
        "Normalize wheel OBJ (diagnostic only)", vehicleWheelVisual.normalize)
    meshChanged = meshChanged or changed
    vehicleWheelVisual.color[1], changed = UI.SliderFloat(
        "Wheel color R", vehicleWheelVisual.color[1], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleWheelVisual.color[2], changed = UI.SliderFloat(
        "Wheel color G", vehicleWheelVisual.color[2], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleWheelVisual.color[3], changed = UI.SliderFloat(
        "Wheel color B", vehicleWheelVisual.color[3], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    if meshChanged then ApplyVehicleWheelMeshes() end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("LIVE NATIVE CENTERS")
    local labels = { "FL", "FR", "RL", "RR" }
    for index = 1, math.min(#labels, #vehicleWheelTelemetry) do
        local telemetry = vehicleWheelTelemetry[index]
        if telemetry ~= nil then
            UI.Text(string.format(
                "%s center (%+.3f, %+.3f, %+.3f) m | steer %+6.2f | spin %+7.1f",
                labels[index], telemetry.centerX, telemetry.centerY, telemetry.centerZ,
                telemetry.steerAngle, telemetry.rotationDegrees))
        end
    end

    UI.Spacing()
    if UI.Button("RESET WHEEL VISUALS TO AUTHORED 1:1") then
        ResetVehicleWheelVisualTuning()
    end
    UI.SameLine()
    if UI.Button("RELOAD WHEEL MESH SLOTS") then
        ApplyVehicleWheelMeshes()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextWrapped("Temporary wheel OBJ convention remains: origin on the wheel axle/center, local X along the axle. A later glTF import milestone will replace this bridge with named hierarchy nodes and preserve creator transforms directly.")
    UI.TextDisabled(vehicleWheelVisualMessage)
end
