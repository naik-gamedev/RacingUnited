-- CLOUDURP15H6B: Racing United owns its startup weather activation.
-- Keep Main.lua as an include-only coordinator and keep Heritage SurfaceWorld
-- dry/disabled by default for engine tests and other games.
local startupWeather = Physics.GetSurfaceWeather()
if startupWeather ~= nil and not startupWeather.enabled then
    Physics.SetSurfaceWeather(
        true,
        startupWeather.rain_mm_per_hour or 0.0,
        startupWeather.relative_humidity or 0.55,
        startupWeather.wind_mps or 2.0,
        startupWeather.cloud_cover or 0.20,
        startupWeather.drainage_capacity_mm_per_hour or 4.0,
        startupWeather.evaporation_reference_mm_per_hour or 0.35,
        startupWeather.wind_direction_deg or 45.0)
end
