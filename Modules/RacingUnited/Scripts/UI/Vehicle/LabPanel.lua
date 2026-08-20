-- Vehicle LAB coordinator: dynamics experiments and creator-facing cameras live
-- in separate sub-tabs so camera authoring does not crowd the recorder UI.
function DrawVehicleLabPanel()
    if UI.BeginTabBar("VehicleLabSubTabs") then
        if UI.BeginTabItem("DYNAMICS") then
            DrawVehicleDynamicsLabPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("CAMERAS") then
            DrawVehicleCameraLabPanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
