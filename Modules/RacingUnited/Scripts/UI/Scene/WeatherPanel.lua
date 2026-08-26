-- Scene-owned weather and hydrology controls. Weather is world state, not vehicle state.
local sceneWeatherMessage = ""

local function ApplySceneWeather(weather, enabled, rain, humidity, wind, cloud, windDirection)
    if Physics.SetSurfaceWeather(
        enabled, rain, humidity, wind, cloud,
        weather.drainage_capacity_mm_per_hour or 4.0,
        weather.evaporation_reference_mm_per_hour or 0.35,
        windDirection or weather.wind_direction_deg or 45.0) then
        sceneWeatherMessage = string.format(
            "Weather: rain %.1f mm/h | wind %.1f m/s | humidity %.0f%%",
            rain, wind, humidity * 100.0)
        return true
    end
    sceneWeatherMessage = "Native surface weather rejected these values."
    return false
end

function DrawSceneWeatherPanel()
    UI.TextDisabled("SCENE WEATHER / PRECIPITATION")
    UI.TextWrapped("Rain, humidity, wind, cloud cover and hydrology belong to the loaded scene. Vehicles only consume the resulting contacted-surface state.")
    UI.Spacing()

    local weather = Physics.GetSurfaceWeather()
    if weather ~= nil then
        local presetWidth = math.max(100.0, (UI.GetAvailableWidth() - 16.0) * 0.25)
        if UI.Button("DRY", presetWidth, 30.0, false) then
            ApplySceneWeather(weather, true, 0.0, 0.40, 3.0, 0.10, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("LIGHT RAIN", presetWidth, 30.0, false) then
            ApplySceneWeather(weather, true, 2.0, 0.75, 4.0, 0.75, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("HEAVY RAIN", presetWidth, 30.0, false) then
            ApplySceneWeather(weather, true, 25.0, 0.92, 8.0, 1.0, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("STORM", presetWidth, 30.0, false) then
            ApplySceneWeather(weather, true, 80.0, 0.98, 20.0, 1.0, weather.wind_direction_deg or 45.0)
        end

        local rain = weather.rain_mm_per_hour or 0.0
        local humidity = weather.relative_humidity or 0.55
        local wind = weather.wind_mps or 2.0
        local windDirection = weather.wind_direction_deg or 45.0
        local cloud = weather.cloud_cover or 0.20
        local changed = false
        rain, changed = UI.SliderFloat(
            "Rainfall", rain, 0.0, 150.0, "%.1f mm/h")
        if changed then
            ApplySceneWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        local humidityPercent = humidity * 100.0
        humidityPercent, changed = UI.SliderFloat(
            "Relative humidity", humidityPercent, 0.0, 100.0, "%.0f%%")
        if changed then
            humidity = humidityPercent / 100.0
            ApplySceneWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        wind, changed = UI.SliderFloat(
            "Wind", wind, 0.0, 40.0, "%.1f m/s")
        if changed then
            ApplySceneWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        windDirection, changed = UI.SliderFloat(
            "Wind direction", windDirection, 0.0, 360.0, "%.0f deg")
        if changed then
            ApplySceneWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        local cloudPercent = cloud * 100.0
        cloudPercent, changed = UI.SliderFloat(
            "Cloud cover", cloudPercent, 0.0, 100.0, "%.0f%%")
        if changed then
            cloud = cloudPercent / 100.0
            ApplySceneWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        if UI.Button("RESET ACCUMULATED WATER FILM") then
            Physics.ResetSurfaceWeather()
            sceneWeatherMessage = "Weather reference film and cumulative totals reset."
        end
        UI.Text(string.format(
            "Weather reference film %.3f mm | wet %.1f%% | road %.1f C",
            weather.water_film_mm or 0.0,
            (weather.effective_wetness or 0.0) * 100.0,
            weather.road_temperature_c or 20.0))
        UI.TextDisabled(string.format(
            "Drain %.2f mm/h | evaporate %.2f mm/h | accumulated rain %.2f mm",
            weather.current_drainage_mm_per_hour or 0.0,
            weather.current_evaporation_mm_per_hour or 0.0,
            weather.cumulative_rain_mm or 0.0))
        if rain > 0.0 then
            UI.TextDisabled(string.format(
                "Rain population %.0f drops/m3 | mean %.2f mm | flux-weighted fall %.2f m/s",
                weather.rain_drop_number_concentration_m3 or 0.0,
                weather.rain_drop_mean_diameter_mm or 0.0,
                weather.rain_drop_flux_terminal_mps or 0.0))
        end
    else
        UI.TextDisabled("Native surface weather is unavailable.")
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("SCENE HYDROLOGY / DYNAMIC DRY LINE")
    local hydrology = Physics.GetSurfaceHydrology()
    if hydrology ~= nil and hydrology.available then
        UI.Text(string.format(
            "%d static support cells from %d collision triangles | %s",
            hydrology.cells or 0,
            hydrology.source_triangles or 0,
            hydrology.loaded_from_cache and "cached .hhyd v15" or "fresh .hhyd v15 bake"))
        UI.TextDisabled(string.format(
            "Prebaked world tiles %d | far payload %.1f MiB",
            hydrology.prebaked_world_tiles or 0,
            (hydrology.prebaked_far_payload_bytes or 0) / (1024.0 * 1024.0)))
        UI.TextDisabled("CPU Hydro runtime is retired. Live standing/runoff water and tire dry-line are GPU authority; use F8 for GPU water telemetry.")

        local showHydrology, debugChanged = UI.Checkbox(
            "Show water depth + flow overlay",
            hydrology.debug_visualization == true)
        if debugChanged then
            Physics.SetSurfaceHydrologyDebug(showHydrology)
        end
        if UI.Button("RESET SPATIAL WATER / DRY LINE") then
            Physics.ResetSurfaceHydrology()
            sceneWeatherMessage = "Spatial water, puddles and tire-cleared lines reset."
        end
        UI.TextWrapped(hydrology.bake_message or "")
        UI.TextDisabled("Static basin topology is cached; rainfall exposure, puddle reconstruction and tire-cleared state live only in the GPU runtime.")
    else
        UI.TextDisabled("Hydrology becomes available automatically after a static triangle collision scene loads.")
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("PREBAKED WATER RUNTIME")
    UI.TextWrapped("Production water has one fixed path: the scene .hhyd cache owns basin capacity and downhill flow; runtime rainfall is a single scene exposure value; only the <=100 m resident topology atlas and local tire dry-line are dynamic. There is no selectable Water Laboratory, no live neighbour-flow CFD, and no periodic all-tile Hydro update.")

    UI.Spacing()
    UI.TextDisabled(sceneWeatherMessage)
end
