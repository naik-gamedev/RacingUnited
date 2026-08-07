-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function DrawAboutScreen()
    UI.Title("ABOUT / ENGINE SAFETY NET")
    UI.Subtitle("Scene: " .. Scene.GetCurrent())
    UI.Separator()
    UI.Spacing()

    UI.TextWrapped("Heritage Engine now records its build identity, generates an exact Lua binding list from C++, and can run non-destructive lifetime smoke tests. The repository is the technical source of truth; function names should never be guessed from chat memory.")
    UI.Spacing()
    DrawSafetyNetPanel()

    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Scene.Load queues a safe transition. Scene.GetCurrent reports the active scene. Scene.Exists validates scene IDs, and Scene.Reload reloads the current .hscene document without rebuilding Heritage Engine.")
    UI.Spacing()

    local imageWidth, imageHeight, imageOk = UI.GetImageSize("UI/RacingUnited_Banner.png")
    if imageOk then
        UI.Text("Module banner: " .. tostring(imageWidth) .. " x " .. tostring(imageHeight))
        UI.ProgressBar(0.72, 260, 16, "Lua UI primitives")
    else
        UI.TextColored("Image API error: " .. UI.GetLastError(), 1.0, 0.35, 0.35, 1.0)
    end
    UI.Spacing()
    UI.Text("main_menu exists: " .. tostring(Scene.Exists("main_menu")))
    UI.Text("prototype exists: " .. tostring(Scene.Exists("prototype")))
    UI.Text("imaginary_scene exists: " .. tostring(Scene.Exists("imaginary_scene")))
    UI.Spacing()
    UI.TextDisabled("Try editing the clear_color inside Scenes/about.hscene, save it, then press RELOAD THIS SCENE.")
    UI.Spacing()

    if UI.Button("RELOAD THIS SCENE") then
        if not Scene.Reload() then
            transitionMessage = Scene.GetLastError()
        end
    end

    if UI.Button("BACK TO MAIN MENU") then
        LoadScene("main_menu")
    end
end
