-- Step 29H tire UI coordinator. The dedicated tire tab is split again so
-- advanced curve tuning does not recreate the old giant-scroll debug panel.
function DrawVehicleTiresPanel()
    SetPrototypeScenePreset("vehicle")

    if UI.BeginTabBar("VehicleTireTabs") then
        if UI.BeginTabItem("PROFILES") then
            DrawVehicleTiresProfilesPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("BASIC") then
            DrawVehicleTiresBasicPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("CURVE") then
            DrawVehicleTiresCurvePanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("LIVE") then
            DrawVehicleTiresLivePanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end

    UI.Spacing()
    UI.TextDisabled(vehicleMessage)
end
