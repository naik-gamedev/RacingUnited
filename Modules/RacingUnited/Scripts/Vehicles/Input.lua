-- Player-vehicle input registration and interpretation.
Input.RegisterAction("Throttle", "Key:W", "Car")
Input.RegisterAction("Brake", "Key:S", "Car")
Input.RegisterAction("Steer Left", "Key:D", "Car")
Input.RegisterAction("Steer Right", "Key:A", "Car")
Input.RegisterAction("Shift Up", "Key:E", "Gears")
Input.RegisterAction("Shift Down", "Key:Q", "Gears")
Input.RegisterAction("Clutch", "", "Gears")
Input.RegisterAction("Select Neutral", "Key:N", "Gears")
Input.RegisterAction("Select Reverse", "Key:R", "Gears")
for gear = 1, 24 do
    Input.RegisterAction("Gear " .. tostring(gear), "", "Gears")
end
Input.RegisterAction("Handbrake", "Key:LeftShift", "Car")
Input.RegisterAction("Handbrake Toggle", "Key:B", "Car")

-- CAM07 camera actions are also declared in Data/InputActions.ini so they are
-- visible in Settings before gameplay Lua runs. Registering here keeps the
-- module API declaration self-contained for alternate startup paths/tests.
Input.RegisterAction("Toggle Free Camera", "Key:Grave", "Camera")
Input.RegisterAction("Camera Forward", "Key:W", "Camera")
Input.RegisterAction("Camera Backward", "Key:S", "Camera")
Input.RegisterAction("Camera Left", "Key:A", "Camera")
Input.RegisterAction("Camera Right", "Key:D", "Camera")
Input.RegisterAction("Camera Up", "Key:E", "Camera")
Input.RegisterAction("Camera Down", "Key:Q", "Camera")
Input.RegisterAction("Camera Fast", "Key:LeftShift", "Camera")
Input.RegisterAction("Camera Slow", "Key:LeftCtrl", "Camera")

-- INPUT01B: the live diagnostic capture exposed the actual failure mode: the
-- Throttle and Brake ACTION values can both be saturated at 1.0 by another
-- configured device while the user's configured arrow-key bindings are idle.
-- INPUT01A independently maxed each action with its keyboard binding, so Up
-- could never release the already-saturated Brake action.  Treat the keyboard
-- throttle/brake pair as one control source instead: whenever either configured
-- keyboard pedal is physically down, that pair explicitly owns BOTH pedal
-- values for that update. This keeps arbitrary user key rebinding authoritative
-- and prevents a stale/saturated gamepad trigger from cancelling keyboard drive.

local handbrakeToggleLatched = false
local handbrakeTogglePressConsumed = false

function UpdateVehicleInputToggles()
    local pressed = Input.Pressed("Handbrake Toggle")
    if not pressed then
        handbrakeTogglePressConsumed = false
        return
    end
    if handbrakeTogglePressConsumed or VehicleCameraOwnsNavigationInput() then
        return
    end

    -- FixedUpdate may run several substeps during one rendered input frame.
    -- Consume the edge exactly once so a single key press cannot toggle ON,
    -- OFF, ON again merely because the physics accumulator ran three steps.
    handbrakeTogglePressConsumed = true
    handbrakeToggleLatched = not handbrakeToggleLatched
    vehicleMessage = handbrakeToggleLatched
        and "Handbrake toggle: LOCKED"
        or "Handbrake toggle: RELEASED"
end

function VehicleHandbrakeToggleLatched()
    return handbrakeToggleLatched
end

function VehicleCameraOwnsNavigationInput()
    return Camera.IsAvailable() and Camera.IsFlyEnabled()
end

local function ConfiguredKeyboardDown(actionName)
    local bindingCount = Input.GetBindingCount(actionName)
    for bindingIndex = 1, bindingCount do
        local binding = Input.GetBindingAt(actionName, bindingIndex)
        local keyName = string.match(binding or "", "^Key:(.+)$")
        if keyName ~= nil and Input.KeyDown(keyName) then
            return true
        end
    end
    return false
end

local function ReadActionWithConfiguredKeyboardFallback(actionName)
    local value = Input.Value(actionName)
    if ConfiguredKeyboardDown(actionName) then
        value = math.max(value, 1.0)
    end
    return value
end

function ReadVehicleDriveKeyboardInputs()
    return ConfiguredKeyboardDown("Throttle"),
        ConfiguredKeyboardDown("Brake")
end

function ReadVehicleDriveInputs()
    if VehicleCameraOwnsNavigationInput() then return 0.0, 0.0 end
    local actionThrottle = Input.Value("Throttle")
    local actionBrake = Input.Value("Brake")
    local keyboardThrottle, keyboardBrake = ReadVehicleDriveKeyboardInputs()

    -- Keyboard pedal ownership is intentionally paired. Holding Up means
    -- throttle=1/brake=0 even if a second device currently reports Brake=1.
    -- Holding Down does the mirror image. Holding both deliberately requests
    -- both pedals, matching the user's physical keyboard state.
    if keyboardThrottle or keyboardBrake then
        return keyboardThrottle and 1.0 or 0.0,
            keyboardBrake and 1.0 or 0.0
    end

    return actionThrottle, actionBrake
end

function ReadVehicleThrottleInput()
    local throttle = ReadVehicleDriveInputs()
    return throttle
end

function ReadVehicleBrakeInput()
    local _, brake = ReadVehicleDriveInputs()
    return brake
end

function ReadVehicleHandbrakeInput()
    -- CAM10: detached/authoring flight suppresses live driving controls, but a
    -- latched parking brake is persistent vehicle state, not a navigation key.
    -- Keep feeding the latch to physics while the camera owns WASD/QE so the
    -- parked car cannot silently roll away as soon as free flight begins.
    if VehicleCameraOwnsNavigationInput() then
        return handbrakeToggleLatched and 1.0 or 0.0
    end
    local held = ReadActionWithConfiguredKeyboardFallback("Handbrake")
    if handbrakeToggleLatched then
        return math.max(held, 1.0)
    end
    return held
end

function ReadVehicleSteeringInput()
    if VehicleCameraOwnsNavigationInput() then return 0.0 end
    -- Temporary input-layer handedness compensation: the current vehicle path
    -- physically responds opposite to the historical action labels. Keep the
    -- steering solver untouched; the action bindings are swapped instead so
    -- the Settings GUI can be rebound to the physical direction the player wants.
    return ReadActionWithConfiguredKeyboardFallback("Steer Right")
        - ReadActionWithConfiguredKeyboardFallback("Steer Left")
end

function ReadVehicleSteeringActions()
    return ReadActionWithConfiguredKeyboardFallback("Steer Left"),
        ReadActionWithConfiguredKeyboardFallback("Steer Right")
end


function ReadVehicleDirectGearSelection()
    if VehicleCameraOwnsNavigationInput() then return nil end
    if Input.Pressed("Select Neutral") then return 0 end
    if Input.Pressed("Select Reverse") then return -1 end
    for gear = 1, 24 do
        if Input.Pressed("Gear " .. tostring(gear)) then
            return gear
        end
    end
    return nil
end

function ReportVehicleGearChange(success, actionText)
    if success then
        vehicleMessage = actionText
    else
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
end
