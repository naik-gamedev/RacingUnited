-- Physics tab coordinator. Heavy diagnostics are separated into small sub-tabs.
function DrawPrototypePhysicsPanel()
    UI.TextDisabled("NATIVE PHYSICS LAB")
    UI.Separator()
    UI.Spacing()

    if UI.BeginTabBar("PrototypePhysicsTabs") then
        if UI.BeginTabItem("WORLD") then
            DrawPhysicsWorldPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("SUSPENSION") then
            DrawPhysicsSuspensionPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("QUERIES / CCD") then
            DrawPhysicsQueriesPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("BODY") then
            DrawPhysicsBodyPanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
