-- Player-vehicle input registration and interpretation.
Input.RegisterAction("Throttle", "Key:W", "Car")
Input.RegisterAction("Brake", "Key:S", "Car")
Input.RegisterAction("Steer Left", "Key:D", "Car")
Input.RegisterAction("Steer Right", "Key:A", "Car")
Input.RegisterAction("Shift Up", "Key:E", "Car")
Input.RegisterAction("Shift Down", "Key:Q", "Car")
Input.RegisterAction("Select Reverse", "Key:R", "Car")
Input.RegisterAction("Select Neutral", "Key:N", "Car")
Input.RegisterAction("Handbrake", "Key:LeftShift", "Car")

function ReadVehicleSteeringInput()
    -- Temporary input-layer handedness compensation: the current vehicle path
    -- physically responds opposite to the historical action labels. Keep the
    -- steering solver untouched; the action bindings are swapped instead so
    -- the Settings GUI can be rebound to the physical direction the player wants.
    return Input.Value("Steer Right") - Input.Value("Steer Left")
end

function ReadVehicleSteeringActions()
    return Input.Value("Steer Left"), Input.Value("Steer Right")
end

function ReportVehicleGearChange(success, actionText)
    if success then
        vehicleMessage = actionText
    else
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
end
