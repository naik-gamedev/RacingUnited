-- Prototype lab coordinator. The old single 600-line scroll panel has been
-- replaced by focused top-level tabs and subsystem sub-tabs.
function DrawPrototypeScreen()
    UI.Title("RACING UNITED PROTOTYPE LAB")
    UI.Subtitle("Scene: " .. Scene.GetCurrent() .. " | Tab hides unrelated debug geometry")
    UI.Separator()
    UI.Spacing()

    if UI.BeginTabBar("PrototypeLabTabs") then
        if UI.BeginTabItem("VEHICLE") then
            DrawVehicleDebugPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("PHYSICS") then
            DrawPrototypePhysicsPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("ENTITY") then
            DrawPrototypeEntityPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("MODULE") then
            DrawPrototypeModulePanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("SCENE") then
            DrawPrototypeScenePanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("SAFETY") then
            DrawPrototypeSafetyPanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
