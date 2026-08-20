-- CAMLAB01 vehicle-mounted camera authoring and persistent preset state.
-- Native C++ owns only the live vehicle-relative camera transform/navigation;
-- Lua owns the named Racing United/iRacing-style presets and module save data.

local cameraVehicleKey = tostring(PrototypeCarDefinition.id or "vehicle")

local function CameraDefaultPose(x, y, z, pitch, yaw, roll)
    return {
        x = x, y = y, z = z,
        pitch = pitch or 0.0,
        yaw = yaw or 0.0,
        roll = roll or 0.0
    }
end

local function WheelCameraDefault(wheelIndex, sideSign)
    local mount = vehicleWheelMounts[wheelIndex] or { 0.0, 0.85, 0.0 }
    -- Slightly behind, slightly above and just outside the wheel. The modest
    -- inward yaw leaves the wheel in frame while still showing road ahead.
    return CameraDefaultPose(
        (mount[1] or 0.0) + sideSign * 0.20,
        (mount[2] or 0.85) + 0.16,
        (mount[3] or 0.0) - 0.58,
        -4.0,
        sideSign < 0.0 and 11.0 or -11.0,
        0.0)
end

vehicleCameraViews = {
    { id = "cockpit",  name = "Cockpit",  default = CameraDefaultPose(-0.32, 1.42,  0.30, -2.0,   0.0, 0.0) },
    { id = "nose",     name = "Nose",     default = CameraDefaultPose( 0.00, 0.64,  2.03,  0.0,   0.0, 0.0) },
    { id = "gearbox",  name = "Gearbox",  default = CameraDefaultPose( 0.00, 0.73, -1.72, -2.0, 180.0, 0.0) },
    { id = "roll_bar", name = "Roll Bar", default = CameraDefaultPose( 0.00, 1.72, -0.02, -3.0,   0.0, 0.0) },
    { id = "f_susp",   name = "F Susp",   default = CameraDefaultPose(-0.54, 0.89,  0.88, -5.0,  10.0, 0.0) },
    { id = "r_susp",   name = "R Susp",   default = CameraDefaultPose(-0.54, 0.89, -1.72, -4.0,  10.0, 0.0) },
    { id = "fl_wheel", name = "FL Wheel", default = WheelCameraDefault(1, -1.0) },
    { id = "fr_wheel", name = "FR Wheel", default = WheelCameraDefault(2,  1.0) },
    { id = "rl_wheel", name = "RL Wheel", default = WheelCameraDefault(3, -1.0) },
    { id = "rr_wheel", name = "RR Wheel", default = WheelCameraDefault(4,  1.0) }
}

local function CopyCameraPose(source)
    return {
        x = source.x, y = source.y, z = source.z,
        pitch = source.pitch, yaw = source.yaw, roll = source.roll
    }
end

local function CameraSavePrefix(cameraId)
    return "vehicle.camera." .. cameraVehicleKey .. "." .. cameraId .. "."
end

local function LoadCameraPose(view)
    local prefix = CameraSavePrefix(view.id)
    local fallback = view.default
    return {
        x = Save.GetNumber(prefix .. "x", fallback.x),
        y = Save.GetNumber(prefix .. "y", fallback.y),
        z = Save.GetNumber(prefix .. "z", fallback.z),
        pitch = Save.GetNumber(prefix .. "pitch", fallback.pitch),
        yaw = Save.GetNumber(prefix .. "yaw", fallback.yaw),
        roll = Save.GetNumber(prefix .. "roll", fallback.roll)
    }
end

vehicleCameraLab = {
    selectedId = Save.GetString(
        "vehicle.camera." .. cameraVehicleKey .. ".selected", "cockpit"),
    active = false,
    flySpeed = Save.GetNumber(
        "vehicle.camera." .. cameraVehicleKey .. ".fly_speed", 1.5),
    message = "Select a vehicle camera to preview and edit it.",
    poses = {}
}

for _, view in ipairs(vehicleCameraViews) do
    vehicleCameraLab.poses[view.id] = LoadCameraPose(view)
end

local function FindVehicleCameraView(cameraId)
    for _, view in ipairs(vehicleCameraViews) do
        if view.id == cameraId then
            return view
        end
    end
    return nil
end

function GetSelectedVehicleCameraView()
    local view = FindVehicleCameraView(vehicleCameraLab.selectedId)
    if view == nil then
        view = vehicleCameraViews[1]
        vehicleCameraLab.selectedId = view.id
    end
    return view, vehicleCameraLab.poses[view.id]
end

function ApplySelectedVehicleCameraPose()
    if not Camera.IsAvailable() then
        vehicleCameraLab.message = "Native vehicle camera service is unavailable."
        return false
    end
    local _, pose = GetSelectedVehicleCameraView()
    Camera.SetVehiclePose(
        pose.x, pose.y, pose.z,
        pose.pitch, pose.yaw, pose.roll)
    Camera.SetFlySpeed(vehicleCameraLab.flySpeed)
    Camera.SetVehicleViewActive(true)
    vehicleCameraLab.active = true
    return true
end

function SelectVehicleCameraView(cameraId)
    local view = FindVehicleCameraView(cameraId)
    if view == nil then
        return false
    end
    if Camera.IsAvailable() then
        Camera.SetFlyEnabled(false)
    end
    vehicleCameraLab.selectedId = cameraId
    Save.SetString(
        "vehicle.camera." .. cameraVehicleKey .. ".selected", cameraId)
    ApplySelectedVehicleCameraPose()
    vehicleCameraLab.message = "Previewing " .. view.name .. " camera."
    return true
end

function UseDefaultChaseCamera()
    if Camera.IsAvailable() then
        Camera.SetFlyEnabled(false)
        Camera.SetVehicleViewActive(false)
    end
    vehicleCameraLab.active = false
    vehicleCameraLab.message = "Returned to the normal spring-damped chase camera."
end

function SyncVehicleCameraAuthoringPose()
    if not Camera.IsAvailable() or not Camera.IsVehicleViewActive() then
        vehicleCameraLab.active = false
        return
    end

    vehicleCameraLab.active = true
    local x, y, z, pitch, yaw, roll = Camera.GetVehiclePose()
    local _, pose = GetSelectedVehicleCameraView()
    if x ~= nil then
        pose.x = x
        pose.y = y
        pose.z = z
        pose.pitch = pitch
        pose.yaw = yaw
        pose.roll = roll
    end
end

function SetVehicleCameraFlyEnabled(enabled)
    if not Camera.IsAvailable() then
        return false
    end
    if enabled and not Camera.IsVehicleViewActive() then
        ApplySelectedVehicleCameraPose()
    end
    Camera.SetFlySpeed(vehicleCameraLab.flySpeed)
    Camera.SetFlyEnabled(enabled)
    vehicleCameraLab.message = enabled
        and "Free-fly authoring active. Shift+` / ¨ exits and keeps the new pose."
        or "Free-fly stopped; the edited pose is kept in memory until saved."
    return Camera.IsFlyEnabled()
end

local function WriteCameraPose(view, pose)
    local prefix = CameraSavePrefix(view.id)
    Save.SetNumber(prefix .. "x", pose.x)
    Save.SetNumber(prefix .. "y", pose.y)
    Save.SetNumber(prefix .. "z", pose.z)
    Save.SetNumber(prefix .. "pitch", pose.pitch)
    Save.SetNumber(prefix .. "yaw", pose.yaw)
    Save.SetNumber(prefix .. "roll", pose.roll)
end

function SaveCurrentVehicleCamera()
    SyncVehicleCameraAuthoringPose()
    local view, pose = GetSelectedVehicleCameraView()
    WriteCameraPose(view, pose)
    Save.SetNumber(
        "vehicle.camera." .. cameraVehicleKey .. ".fly_speed",
        vehicleCameraLab.flySpeed)
    if Save.Flush() then
        vehicleCameraLab.message = "Saved " .. view.name .. " camera permanently."
        return true
    end
    vehicleCameraLab.message = "CAMERA SAVE ERROR: " .. Save.GetLastError()
    return false
end

function SaveAllVehicleCameras()
    SyncVehicleCameraAuthoringPose()
    for _, view in ipairs(vehicleCameraViews) do
        WriteCameraPose(view, vehicleCameraLab.poses[view.id])
    end
    Save.SetNumber(
        "vehicle.camera." .. cameraVehicleKey .. ".fly_speed",
        vehicleCameraLab.flySpeed)
    Save.SetString(
        "vehicle.camera." .. cameraVehicleKey .. ".selected",
        vehicleCameraLab.selectedId)
    if Save.Flush() then
        vehicleCameraLab.message = "Saved all vehicle camera presets permanently."
        return true
    end
    vehicleCameraLab.message = "CAMERA SAVE ERROR: " .. Save.GetLastError()
    return false
end

function ReloadCurrentVehicleCamera()
    local view = GetSelectedVehicleCameraView()
    vehicleCameraLab.poses[view.id] = LoadCameraPose(view)
    if vehicleCameraLab.active then
        ApplySelectedVehicleCameraPose()
    end
    vehicleCameraLab.message = "Reloaded saved " .. view.name .. " camera."
end

function ResetCurrentVehicleCameraDefault()
    local view = GetSelectedVehicleCameraView()
    vehicleCameraLab.poses[view.id] = CopyCameraPose(view.default)
    if vehicleCameraLab.active then
        ApplySelectedVehicleCameraPose()
    end
    vehicleCameraLab.message = "Reset " .. view.name .. " to its project default; save to make it permanent."
end

function VehicleCameraUpdate()
    if not Camera.IsAvailable() then
        return
    end
    SyncVehicleCameraAuthoringPose()
end

if Camera.IsAvailable() then
    Camera.SetFlySpeed(vehicleCameraLab.flySpeed)
end
