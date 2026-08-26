-- Scene-owned UI load coordinator.
-- Keep weather, hydrology and future scene-level environment panels out of vehicle UI.

local function IncludeScenePanel(relativePath)
    local ok, message = Script.Include(relativePath)
    if not ok then
        error(message or ("Could not include scene UI file: " .. relativePath), 0)
    end
end

IncludeScenePanel("UI/Scene/EnvironmentPanel.lua")
IncludeScenePanel("UI/Scene/WeatherPanel.lua")
