-- Scene geographic metadata + the single Heritage astronomical clock.
local environmentPanelMessage = ""
local environmentDraftYear = nil
local environmentDraftMonth = nil
local environmentDraftDay = nil
local environmentDraftDirty = false
local environmentLastDateKey = ""

function DrawSceneEnvironmentPanel()
    UI.TextDisabled("SCENE LOCATION / CALENDAR")
    UI.TextWrapped("Latitude, longitude and elevation belong to the Scene_*.glb. Heritage combines them with this calendar/time to rotate the real celestial sphere and calculate the Sun and Moon.")
    UI.Spacing()

    local latitude, longitude, elevation, timezone, utcOffset = Environment.GetLocation()
    UI.Text(string.format(
        "Location %.6f deg N/S | %.6f deg E/W | %.1f m",
        latitude or 0.0,
        longitude or 0.0,
        elevation or 0.0))
    UI.TextDisabled(string.format(
        "Timezone %s | current UTC offset %+d min",
        tostring(timezone or "AUTO"),
        utcOffset or 0))

    local year, month, day = Environment.GetDate()
    local actualDateKey = string.format("%04d-%02d-%02d", year or 2026, month or 1, day or 1)
    if environmentDraftYear == nil or (not environmentDraftDirty and actualDateKey ~= environmentLastDateKey) then
        environmentDraftYear = year or 2026
        environmentDraftMonth = month or 1
        environmentDraftDay = day or 1
        environmentLastDateKey = actualDateKey
    end

    local fieldChanged = false
    environmentDraftYear, fieldChanged = UI.InputInt("Year", environmentDraftYear, 1)
    environmentDraftDirty = environmentDraftDirty or fieldChanged
    environmentDraftMonth, fieldChanged = UI.InputInt("Month", environmentDraftMonth, 1)
    environmentDraftDirty = environmentDraftDirty or fieldChanged
    environmentDraftDay, fieldChanged = UI.InputInt("Day", environmentDraftDay, 1)
    environmentDraftDirty = environmentDraftDirty or fieldChanged
    if UI.Button("APPLY DATE") then
        if Environment.SetDate(environmentDraftYear, environmentDraftMonth, environmentDraftDay) then
            environmentPanelMessage = "Astronomical calendar updated."
            environmentDraftDirty = false
            environmentLastDateKey = string.format(
                "%04d-%02d-%02d",
                environmentDraftYear,
                environmentDraftMonth,
                environmentDraftDay)
        else
            environmentPanelMessage = "Invalid Gregorian date."
        end
    end

    local timeHours = Environment.GetTimeOfDay() or 12.0
    local changed = false
    timeHours, changed = UI.SliderFloat(
        "Local time", timeHours, 0.0, 23.999, "%05.2f h")
    if changed then
        Environment.SetTimeOfDay(timeHours)
    end

    local timeScale = Environment.GetTimeScale() or 240.0
    timeScale, changed = UI.SliderFloat(
        "Time scale", timeScale, 0.0, 3600.0, "%.0fx")
    if changed then
        Environment.SetTimeScale(timeScale)
    end

    local cycleEnabled = Environment.IsCycleEnabled() == true
    cycleEnabled, changed = UI.Checkbox("Advance calendar/time", cycleEnabled)
    if changed then
        Environment.SetCycleEnabled(cycleEnabled)
    end

    UI.Spacing()
    UI.TextDisabled("GLB Custom Properties")
    UI.TextWrapped("Canonical keys: heritage.latitude_deg, heritage.longitude_deg, heritage.elevation_m, heritage.timezone. Put them on scene extras or a top-level Empty named Heritage_SceneMetadata. Optional heritage.utc_offset_minutes overrides timezone/longitude offset derivation.")
    UI.TextDisabled("The sky map is universal; tracks never need their own Europe/Japan star texture.")
    UI.TextDisabled(environmentPanelMessage)
end
