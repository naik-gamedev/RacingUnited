-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function OnDrawUI(framebufferWidth, framebufferHeight)
    local currentScene = Scene.GetCurrent()
    if currentScene == "prototype" and not showPrototypeControls then
        return
    end

    UI.SetLayoutEditing(uiLayoutEditing)
    if UI.BeginPanel("RacingUnitedLuaMain", 720, 700) then
        UI.ModuleLabel()
        UI.Spacing()

        local layoutChanged = false
        uiLayoutEditing, layoutChanged = UI.Checkbox(
            "EDIT GUI / HUD LAYOUT", uiLayoutEditing)
        if layoutChanged then
            UI.SetLayoutEditing(uiLayoutEditing)
        end
        if uiLayoutEditing then
            UI.TextColored(
                "LAYOUT UNLOCKED - drag this window; the cyan frame is the safe screen area.",
                0.25, 0.82, 0.96, 1.0)
            if UI.Button("CENTER THIS WINDOW", 190.0, 28.0, false) then
                UI.CenterCurrentPanel()
            end
        end
        UI.Separator()
        UI.Spacing()

        if currentScene == "prototype" then
            DrawPrototypeScreen()
        elseif currentScene == "about" then
            DrawAboutScreen()
        else
            DrawMainScreen()
        end

        if transitionMessage ~= "" or saveMessage ~= "" then
            UI.Spacing()
            UI.Separator()
            if transitionMessage ~= "" then
                UI.TextDisabled(transitionMessage)
            end
            if saveMessage ~= "" then
                UI.TextDisabled(saveMessage)
            end
        end
    end

    UI.EndPanel()
end

function OnShutdown()
    DestroyVehicleDemo()
    DestroyPhysicsDemo()
    Audio.StopAll()
    ambienceHandle = 0
    Save.Flush()
    Engine.Log("Racing United Lua runtime stopped")
end
