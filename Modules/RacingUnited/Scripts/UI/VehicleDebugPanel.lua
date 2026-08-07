-- Vehicle debug UI coordinator. Each subsystem owns a short, focused sub-tab.
function DrawVehicleDebugPanel()
    UI.TextDisabled("NATIVE VEHICLE LAB + WORKSHOP - STEP 29J.6A")
    UI.Separator()
    UI.Spacing()

    if UI.BeginTabBar("VehicleDebugTabs") then
        if UI.BeginTabItem("DRIVE") then
            DrawVehicleDrivePanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("VISUAL") then
            DrawVehicleVisualPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("SURFACES") then
            DrawVehicleSurfacesPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("TIRES") then
            DrawVehicleTiresPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("DRIVETRAIN") then
            DrawVehicleDrivetrainPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("ABS / TCS") then
            DrawVehicleDriverAidsPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("TELEMETRY") then
            DrawVehicleTelemetryPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("WORKSHOP") then
            DrawVehicleWorkshopPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("LAB") then
            DrawVehicleDynamicsLabPanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
