-- Step 29J.1 vehicle visual coordinator. Body and articulated-wheel tooling live
-- in separate files so this tab remains small as the asset pipeline grows.
function DrawVehicleVisualPanel()
    SetPrototypeScenePreset("visual")

    if UI.BeginTabBar("VehicleVisualTabs") then
        if UI.BeginTabItem("BODY") then
            DrawVehicleVisualBodyPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("WHEELS") then
            DrawVehicleVisualWheelsPanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
