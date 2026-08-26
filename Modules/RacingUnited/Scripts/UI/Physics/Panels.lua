-- Physics-owned UI load coordinator.
-- Main.lua should coordinate subsystems, not enumerate every physics panel.

local function IncludePhysicsPanel(relativePath)
    local ok, message = Script.Include(relativePath)
    if not ok then
        error(message or ("Could not include physics UI file: " .. relativePath), 0)
    end
end

IncludePhysicsPanel("UI/Physics/WorldPanel.lua")
IncludePhysicsPanel("UI/Physics/SuspensionPanel.lua")
IncludePhysicsPanel("UI/Physics/QueriesPanel.lua")
IncludePhysicsPanel("UI/Physics/BodyPanel.lua")
