-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function DrawMainScreen()
    local imageLoaded = UI.Image(
        "UI/RacingUnited_Banner.png",
        560,
        0,
        "center",
        1.0)

    if not imageLoaded then
        UI.TextColored("Banner error: " .. UI.GetLastError(), 1.0, 0.35, 0.35, 1.0)
    end

    UI.Spacing()
    UI.Title("RACING UNITED")
    UI.Subtitle("LUA + DATA-DRIVEN ENTITY SCENES + ENGINE SERVICES")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("This menu and its persistent state are controlled by Main.lua. Each module writes only inside its own UserData save directory.")
    UI.TextDisabled("Audio: " .. Audio.GetBackend())
    UI.Spacing()

    if UI.Button("START MODULE PROTOTYPE") then
        LoadScene("prototype")
    end

    if UI.Button("ABOUT SCENE API") then
        LoadScene("about")
    end

    if UI.Button("ENGINE SETTINGS") then
        Engine.OpenSettings()
    end

    if UI.Button("EXIT") then
        Engine.Exit()
    end
end
