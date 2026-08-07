-- Module-owned save, audio and input diagnostics.
function DrawPrototypeModulePanel()
    SetPrototypeScenePreset("vehicle")
    local changed = false

    UI.TextDisabled("MODULE STATE / SAVE")
    UI.Separator()
    UI.Spacing()

    demoSpeed, changed = UI.SliderFloat("Prototype speed", demoSpeed, 0.0, 5.0, "%.2f")
    if changed then Save.SetNumber("prototype.speed", demoSpeed) end

    showTechnicalText, changed = UI.Checkbox("Show technical explanation", showTechnicalText)
    if changed then Save.SetBool("prototype.show_technical_text", showTechnicalText) end

    testValue, changed = UI.InputInt("Test integer", testValue, 1)
    if changed then Save.SetInt("prototype.test_integer", testValue) end

    UI.Text("Career credits: " .. tostring(credits))
    if UI.Button("ADD 1,000 CREDITS") then
        credits = credits + 1000
        Save.SetInt("career.credits", credits)
        saveMessage = "Added credits; automatic save queued"
    end
    if UI.Button("SPEND 500 CREDITS") then
        credits = math.max(0, credits - 500)
        Save.SetInt("career.credits", credits)
        saveMessage = "Spent credits; automatic save queued"
    end

    if UI.Button("SAVE NOW") then FlushSave("Module state written to disk") end
    if UI.Button("RESET DEMO SAVE") then
        Save.Clear()
        demoSpeed = 1.0
        showTechnicalText = true
        testValue = 5
        credits = 25000
        ambienceVolume = 0.35
        if ambienceHandle ~= 0 then Audio.SetVolume(ambienceHandle, ambienceVolume) end
        Save.SetNumber("prototype.speed", demoSpeed)
        Save.SetBool("prototype.show_technical_text", showTechnicalText)
        Save.SetInt("prototype.test_integer", testValue)
        Save.SetInt("career.credits", credits)
        Save.SetNumber("audio.demo_ambience_volume", ambienceVolume)
        FlushSave("Demo save reset to defaults")
    end

    if showTechnicalText then
        UI.TextDisabled("Save file: " .. Save.GetPath())
        UI.TextDisabled("Dirty in memory: " .. tostring(Save.IsDirty()))
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("AUDIO")
    UI.Separator()
    UI.Spacing()

    ambienceVolume, changed = UI.SliderFloat(
        "Demo ambience volume", ambienceVolume, 0.0, 1.0, "%.2f")
    if changed then
        Save.SetNumber("audio.demo_ambience_volume", ambienceVolume)
        if ambienceHandle ~= 0 then Audio.SetVolume(ambienceHandle, ambienceVolume) end
    end
    if UI.Button("PLAY UI CONFIRMATION SOUND") then PlayUiConfirmation() end
    if UI.Button("START COUNTRYSIDE AMBIENCE") then StartAmbience() end
    if UI.Button("STOP COUNTRYSIDE AMBIENCE") then StopAmbience() end
    UI.TextDisabled(audioMessage)
    UI.TextDisabled("Available: " .. tostring(Audio.IsAvailable()))

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("INPUT")
    UI.Separator()
    UI.Spacing()
    UI.Text("Input service available: " .. tostring(Input.IsAvailable()))
    UI.TextWrapped("Throttle: " .. Input.GetBinding("Throttle"))
    UI.TextWrapped("Brake: " .. Input.GetBinding("Brake"))
    UI.TextWrapped("Steer left: " .. Input.GetBinding("Steer Left"))
    UI.TextWrapped("Steer right: " .. Input.GetBinding("Steer Right"))
    UI.TextWrapped("Handbrake: " .. Input.GetBinding("Handbrake"))
    UI.ProgressBar(inputPosition, 420, 18, "Steering test position")
    UI.ProgressBar((inputDrive + 1.0) * 0.5, 420, 18, "Drive input")
    UI.TextDisabled(inputMessage)
    UI.TextDisabled(Input.GamepadConnected(0)
        and ("Gamepad: " .. Input.GetGamepadName(0))
        or "Gamepad: none detected")
end
