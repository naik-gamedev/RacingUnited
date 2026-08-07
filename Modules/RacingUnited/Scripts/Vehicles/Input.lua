-- Player-vehicle input registration and interpretation.
Input.RegisterAction("Throttle", "Key:W", "Car")
Input.RegisterAction("Brake", "Key:S", "Car")
Input.RegisterAction("Steer Left", "Key:A", "Car")
Input.RegisterAction("Steer Right", "Key:D", "Car")
Input.RegisterAction("Shift Up", "Key:E", "Car")
Input.RegisterAction("Shift Down", "Key:Q", "Car")
Input.RegisterAction("Select Reverse", "Key:R", "Car")
Input.RegisterAction("Select Neutral", "Key:N", "Car")
Input.RegisterAction("Handbrake", "Key:LeftShift", "Car")

function ReadVehicleSteeringInput()
    -- Positive steering means a left turn in this module coordinate convention.
    return Input.Value("Steer Left") - Input.Value("Steer Right")
end

function ReportVehicleGearChange(success, actionText)
    if success then
        vehicleMessage = actionText
    else
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
end
