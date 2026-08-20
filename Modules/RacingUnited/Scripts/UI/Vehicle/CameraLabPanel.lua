-- CAMLAB01 vehicle camera creator UI. Named views follow the concise camera
-- naming convention requested for Racing United, with four extra wheel cameras.

local function CameraPoseControl(label, key, minimum, maximum, format)
    local _, pose = GetSelectedVehicleCameraView()
    local changed = false
    local sliderChanged = false
    pose[key], sliderChanged = UI.SliderFloat(
        label .. "##camera_slider_" .. key,
        pose[key], minimum, maximum, format)
    changed = changed or sliderChanged

    local exactChanged = false
    local positional = key == "x" or key == "y" or key == "z"
    pose[key], exactChanged = UI.InputFloat(
        label .. " exact##camera_exact_" .. key,
        pose[key], minimum, maximum,
        positional and 0.01 or 0.1,
        positional and 0.10 or 1.0,
        format)
    changed = changed or exactChanged
    return changed
end

local function CameraButton(view)
    local label = view.name
    if vehicleCameraLab.selectedId == view.id then
        label = "> " .. label
    end
    if UI.Button(label) then
        SelectVehicleCameraView(view.id)
    end
end

function DrawVehicleCameraLabPanel()
    SetPrototypeScenePreset("vehicle")
    UI.TextWrapped("Vehicle-mounted creator cameras. Positions are vehicle-local: +X right, +Y up, +Z forward. Every preset is independently editable and can be saved permanently for this vehicle.")
    UI.Spacing()

    if not Camera.IsAvailable() then
        UI.TextDisabled("Native vehicle camera authoring service is unavailable in this build.")
        return
    end

    if UI.Button("DEFAULT CHASE CAMERA") then
        UseDefaultChaseCamera()
    end
    UI.SameLine()
    UI.TextDisabled(vehicleCameraLab.active and "custom vehicle camera active" or "normal chase active")

    UI.Separator()
    UI.Text("Driving / in-car cameras")
    CameraButton(vehicleCameraViews[1])
    UI.SameLine(); CameraButton(vehicleCameraViews[2])
    UI.SameLine(); CameraButton(vehicleCameraViews[3])
    CameraButton(vehicleCameraViews[4])
    UI.SameLine(); CameraButton(vehicleCameraViews[5])
    UI.SameLine(); CameraButton(vehicleCameraViews[6])

    UI.Spacing()
    UI.Text("Wheel cameras")
    CameraButton(vehicleCameraViews[7])
    UI.SameLine(); CameraButton(vehicleCameraViews[8])
    CameraButton(vehicleCameraViews[9])
    UI.SameLine(); CameraButton(vehicleCameraViews[10])

    local view, pose = GetSelectedVehicleCameraView()
    UI.Separator()
    UI.Text("Editing: " .. view.name)

    local changed = false
    changed = CameraPoseControl("Position X (m)", "x", -10.0, 10.0, "%.3f") or changed
    changed = CameraPoseControl("Position Y (m)", "y", -5.0, 10.0, "%.3f") or changed
    changed = CameraPoseControl("Position Z (m)", "z", -20.0, 20.0, "%.3f") or changed
    changed = CameraPoseControl("Pitch (deg)", "pitch", -89.0, 89.0, "%.2f") or changed
    changed = CameraPoseControl("Yaw (deg)", "yaw", -180.0, 180.0, "%.2f") or changed
    changed = CameraPoseControl("Roll / rotation angle (deg)", "roll", -180.0, 180.0, "%.2f") or changed

    if changed then
        ApplySelectedVehicleCameraPose()
        vehicleCameraLab.message = "Live-edited " .. view.name .. "; press SAVE CURRENT CAMERA to persist it."
    end

    UI.Spacing()
    local flyChanged = false
    vehicleCameraLab.flySpeed, flyChanged = UI.SliderFloat(
        "Free-fly speed (m/s)", vehicleCameraLab.flySpeed,
        0.10, 20.0, "%.2f")
    if flyChanged then
        Camera.SetFlySpeed(vehicleCameraLab.flySpeed)
    end

    local flyActive = Camera.IsFlyEnabled()
    if UI.Button(flyActive and "STOP FREE FLY + KEEP POSE" or "START FREE FLY") then
        SetVehicleCameraFlyEnabled(not flyActive)
    end
    UI.TextWrapped("Blender-style authoring: Shift + ` / ¨ (Grave key) toggles free fly. Mouse looks around; W/A/S/D move; Q/E move down/up; Shift = 4x speed; Ctrl = 0.25x speed. Driving controls are suppressed while free fly owns these keys.")

    UI.Spacing()
    if UI.Button("SAVE CURRENT CAMERA") then SaveCurrentVehicleCamera() end
    UI.SameLine()
    if UI.Button("SAVE ALL CAMERAS") then SaveAllVehicleCameras() end
    if UI.Button("RELOAD SAVED") then ReloadCurrentVehicleCamera() end
    UI.SameLine()
    if UI.Button("RESET CURRENT DEFAULT") then ResetCurrentVehicleCameraDefault() end

    UI.TextDisabled(vehicleCameraLab.message)
    UI.TextDisabled("Camera settings file: " .. Save.GetPath())
end
