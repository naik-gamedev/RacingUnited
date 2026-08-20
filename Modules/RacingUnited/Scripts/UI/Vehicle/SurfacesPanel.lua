-- Per-wheel contacted-collider surface diagnostics.
local function ApplySurfaceWeather(weather, enabled, rain, humidity, wind, cloud, windDirection)
    if Physics.SetSurfaceWeather(
        enabled, rain, humidity, wind, cloud,
        weather.drainage_capacity_mm_per_hour or 4.0,
        weather.evaporation_reference_mm_per_hour or 0.35,
        windDirection or weather.wind_direction_deg or 45.0) then
        vehicleMessage = string.format(
            "Weather: rain %.1f mm/h | wind %.1f m/s | humidity %.0f%%",
            rain, wind, humidity * 100.0)
        return true
    end
    vehicleMessage = "Native surface weather rejected these values."
    return false
end

function DrawVehicleSurfacesPanel()
    SetPrototypeScenePreset("surface")

    UI.TextDisabled("PER-WHEEL SURFACE DETECTION - STEP 29F")
    UI.TextWrapped("Each suspension ray reads the material and wetness from the exact collider beneath that tire. Different tires may contact different surfaces simultaneously.")
    UI.Spacing()

    if UI.Button("RESET ON SPLIT GRIP - LEFT ASPHALT / RIGHT ICE") then
        ResetNativeVehicleSplitGrip()
    end
    if UI.Button("RESET BEFORE SURFACE RUNWAY") then
        ResetNativeVehicleSurfaceRunway()
    end
    if UI.Button("RESET ON DRY ASPHALT") then
        ResetNativeVehicle()
        SetPrototypeScenePreset("surface")
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("DETERMINISTIC TIRE WEATHER / ROAD FILM")
    UI.TextWrapped("Rain feeds a spatial water layer baked automatically from the collision mesh. Water follows elevation and camber, pools in depressions, drains by material, and is cleared along the lines vehicles actually drive.")
    local weather = Physics.GetSurfaceWeather()
    if weather ~= nil then
        local presetWidth = math.max(100.0, (UI.GetAvailableWidth() - 16.0) * 0.25)
        if UI.Button("DRY", presetWidth, 30.0, false) then
            ApplySurfaceWeather(weather, true, 0.0, 0.40, 3.0, 0.10, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("LIGHT RAIN", presetWidth, 30.0, false) then
            ApplySurfaceWeather(weather, true, 2.0, 0.75, 4.0, 0.75, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("HEAVY RAIN", presetWidth, 30.0, false) then
            ApplySurfaceWeather(weather, true, 25.0, 0.92, 8.0, 1.0, weather.wind_direction_deg or 45.0)
        end
        UI.SameLine()
        if UI.Button("STORM", presetWidth, 30.0, false) then
            ApplySurfaceWeather(weather, true, 80.0, 0.98, 20.0, 1.0, weather.wind_direction_deg or 45.0)
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
            ApplySurfaceWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        local humidityPercent = humidity * 100.0
        humidityPercent, changed = UI.SliderFloat(
            "Relative humidity", humidityPercent, 0.0, 100.0, "%.0f%%")
        if changed then
            humidity = humidityPercent / 100.0
            ApplySurfaceWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        wind, changed = UI.SliderFloat(
            "Wind", wind, 0.0, 40.0, "%.1f m/s")
        if changed then
            ApplySurfaceWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        windDirection, changed = UI.SliderFloat(
            "Wind direction", windDirection, 0.0, 360.0, "%.0f deg")
        if changed then
            ApplySurfaceWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        local cloudPercent = cloud * 100.0
        cloudPercent, changed = UI.SliderFloat(
            "Cloud cover", cloudPercent, 0.0, 100.0, "%.0f%%")
        if changed then
            cloud = cloudPercent / 100.0
            ApplySurfaceWeather(weather, true, rain, humidity, wind, cloud, windDirection)
        end
        if UI.Button("RESET ACCUMULATED WATER FILM") then
            Physics.ResetSurfaceWeather()
            vehicleMessage = "Weather reference film and cumulative totals reset."
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
            "%d cells from %d collision triangles | %.0f Hz | %s",
            hydrology.cells or 0,
            hydrology.source_triangles or 0,
            hydrology.update_rate_hz or 0.0,
            hydrology.loaded_from_cache and "cached topology" or "fresh bake"))
        UI.Text(string.format(
            "Water %.3f m3 | wet cells %d | deepest %.2f mm",
            hydrology.water_volume_m3 or 0.0,
            hydrology.wet_cells or 0,
            hydrology.maximum_water_depth_mm or 0.0))
        UI.TextDisabled(string.format(
            "Runoff %.3f m3 | soil %.3f | drains %.3f | evaporation %.3f",
            hydrology.runoff_volume_m3 or 0.0,
            hydrology.infiltration_volume_m3 or 0.0,
            hydrology.drainage_volume_m3 or 0.0,
            hydrology.evaporation_volume_m3 or 0.0))
        UI.TextDisabled(string.format(
            "Tires cleared %.3f L | spray/carried %.3f L | step %.3f ms",
            hydrology.tire_cleared_volume_l or 0.0,
            hydrology.tire_spray_volume_l or 0.0,
            hydrology.last_step_ms or 0.0))

        local showHydrology, debugChanged = UI.Checkbox(
            "Show water depth + flow overlay",
            hydrology.debug_visualization == true)
        if debugChanged then
            Physics.SetSurfaceHydrologyDebug(showHydrology)
        end
        if UI.Button("RESET SPATIAL WATER / DRY LINE") then
            Physics.ResetSurfaceHydrology()
            vehicleMessage = "Spatial water, puddles and tire-cleared lines reset."
        end
        UI.TextWrapped(hydrology.bake_message or "")
        UI.TextDisabled("The topology is cached. Water depth, puddles and dry lines are never baked and remain session-dynamic.")
    else
        UI.TextDisabled("Hydrology becomes available automatically after a static triangle collision scene loads.")
    end

    UI.Spacing()
    if vehicleWheelTelemetry[1] ~= nil then
        local labels = { "FL", "FR", "RL", "RR" }
        UI.Text("Contacted surface by wheel:")
        for index = 1, math.min(4, #vehicleWheelTelemetry) do
            local wheel = vehicleWheelTelemetry[index]
            UI.Text(string.format("%s: %-12s | wet %.0f%% | collider %s",
                labels[index], VehicleDetectedSurfaceLabel(wheel),
                wheel.surfaceWetness * 100.0, tostring(wheel.contactCollider)))
        end
        UI.Spacing()
        UI.Text(string.format("FL effective friction: %.3f | grip used: %.1f%%",
            vehicleWheelTelemetry[1].effectiveFriction,
            vehicleWheelTelemetry[1].gripUtilization * 100.0))
        if vehicleWheelTelemetry[2] ~= nil then
            UI.Text(string.format("FR effective friction: %.3f | grip used: %.1f%%",
                vehicleWheelTelemetry[2].effectiveFriction,
                vehicleWheelTelemetry[2].gripUtilization * 100.0))
        end
    else
        UI.TextDisabled("Wheel telemetry is not available yet.")
    end

    UI.Spacing()
    UI.TextDisabled("Runway order: wet asphalt -> gravel -> dirt -> grass -> snow -> ice")
    UI.TextDisabled(vehicleMessage)
end
