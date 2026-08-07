-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function OnDrawUI(framebufferWidth, framebufferHeight)
    local currentScene = Scene.GetCurrent()
    if currentScene == "prototype" and not showPrototypeControls then
        return
    end

    if UI.BeginPanel("RacingUnitedLuaMain", 720, 700) then
        UI.ModuleLabel()
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
