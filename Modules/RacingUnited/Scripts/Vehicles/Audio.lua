-- Module-owned lifecycle for the native vehicle audio graph. A future module
-- may replace this definition or omit the service without affecting physics.
vehicleAudioHandle = 0
vehicleAudioEnabled = Save.GetBool("vehicle.audio.enabled", true)
vehicleAudioMessage = "Vehicle audio is waiting for a native vehicle"

function VehicleAudioOnPrototypeEnter()
    VehicleAudioOnPrototypeExit()
    if nativeVehicle == 0 or not Audio.IsAvailable() then
        vehicleAudioMessage = Audio.GetLastError()
        return
    end
    vehicleAudioHandle = Audio.CreateVehicleSound(
        nativeVehicle,
        PrototypeCarDefinition.audio)
    if vehicleAudioHandle == 0 then
        vehicleAudioMessage = Audio.GetLastError()
        return
    end
    Audio.SetVehicleSoundEnabled(vehicleAudioHandle, vehicleAudioEnabled)
    vehicleAudioMessage = "Native seven-layer vehicle audio graph is active"
end

function VehicleAudioOnPrototypeExit()
    if vehicleAudioHandle ~= 0 then
        Audio.DestroyVehicleSound(vehicleAudioHandle)
        vehicleAudioHandle = 0
    end
end

function SetVehicleAudioEnabled(enabled)
    vehicleAudioEnabled = enabled == true
    Save.SetBool("vehicle.audio.enabled", vehicleAudioEnabled)
    if vehicleAudioHandle ~= 0 then
        Audio.SetVehicleSoundEnabled(vehicleAudioHandle, vehicleAudioEnabled)
    end
end
