-- World-owned native weather audio lifecycle. Weather authority remains in
-- Physics.SurfaceWorld; this script only supplies the module's sound bank.
weatherAudioHandle = 0
weatherAudioEnabled = Save.GetBool("world.audio.weather.enabled", true)
weatherAudioMessage = "Weather audio is waiting for the native audio service"

function WeatherAudioStart()
    WeatherAudioStop()
    if not Audio.IsAvailable() then
        weatherAudioMessage = Audio.GetLastError()
        return
    end
    weatherAudioHandle = Audio.CreateWeatherSound(
        RacingUnitedWeatherAudioDefinition)
    if weatherAudioHandle == 0 then
        weatherAudioMessage = "AUDIO ERROR: " .. Audio.GetLastError()
        return
    end
    Audio.SetWeatherSoundEnabled(weatherAudioHandle, weatherAudioEnabled)
    weatherAudioMessage = "Native rain and wind ambience is active"
end

function WeatherAudioStop()
    if weatherAudioHandle ~= 0 then
        Audio.DestroyWeatherSound(weatherAudioHandle)
        weatherAudioHandle = 0
    end
end

function SetWeatherAudioEnabled(enabled)
    weatherAudioEnabled = enabled == true
    Save.SetBool("world.audio.weather.enabled", weatherAudioEnabled)
    if weatherAudioHandle ~= 0 then
        Audio.SetWeatherSoundEnabled(weatherAudioHandle, weatherAudioEnabled)
    end
end

function GetWeatherAudioState()
    if weatherAudioHandle == 0 then
        return nil
    end
    return Audio.GetWeatherSoundState(weatherAudioHandle)
end
