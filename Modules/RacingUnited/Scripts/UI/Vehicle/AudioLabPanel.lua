function DrawVehicleAudioLabPanel()
    UI.TextDisabled("NATIVE VEHICLE AUDIO - FIRST PHYSICALLY INFORMED SLICE")
    UI.TextWrapped(
        "Seven independent sources consume live RPM, torque, clutch, wheel slip, wetness, speed and suspension motion. " ..
        "This procedural baseline proves the architecture; measured recordings and more advanced synthesis can replace individual layers later.")
    UI.Spacing()

    local enabled, changed = UI.Checkbox("Enable vehicle sound", vehicleAudioEnabled)
    if changed then
        SetVehicleAudioEnabled(enabled)
    end
    UI.Text("Backend: " .. tostring(Audio.GetBackend()))
    local runtimeStats = Audio.GetRuntimeStats()
    if runtimeStats ~= nil then
        UI.Text(string.format(
            "Native voices: %d | decoded cache: %.2f MiB",
            runtimeStats.activeVoices or 0,
            runtimeStats.cachedAudioMiB or 0.0))
    end
    UI.Text("Definition: " .. tostring(PrototypeCarDefinition.audio.id))

    local state = nil
    if vehicleAudioHandle ~= 0 then
        state = Audio.GetVehicleSoundState(vehicleAudioHandle)
    end
    if state ~= nil and state.valid then
        UI.Text(string.format(
            "RPM %.0f | load %.0f%% | gear %d | speed %.1f km/h",
            state.engineRpm or 0.0,
            (state.engineLoad or 0.0) * 100.0,
            state.gear or 0,
            (state.speedMetersPerSecond or 0.0) * 3.6))
        UI.Text(string.format(
            "%s listener | LOD %s | distance %.1f m | %d model layers + %d sample voices + %d events",
            state.interior and "INTERIOR" or "EXTERIOR",
            tostring(state.detail),
            state.distanceMeters or 0.0,
            state.activeLayerCount or 0,
            state.activeSampleVoices or 0,
            state.activeTransientVoices or 0))
        UI.Text(string.format(
            "Tire slip %.3f | suspension activity %.3f m/s",
            state.averageTireSlip or 0.0,
            state.suspensionActivity or 0.0))
        if state.acousticPathTraced then
            UI.Text(string.format(
                "Acoustic paths: %d rays | %d reflections | direct %.0f%% | delay %.1f ms%s",
                state.acousticRayCount or 0,
                state.reflectionPathCount or 0,
                (state.directPathGain or 1.0) * 100.0,
                state.reflectionDelayMilliseconds or 0.0,
                state.directPathOccluded and " | OCCLUDED" or ""))
        else
            UI.TextDisabled("Acoustic paths: fleet fallback (distance / pan / Doppler)")
        end
    else
        UI.TextDisabled(vehicleAudioMessage)
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("AUTHORED SOURCE LAYOUT")
    UI.Text("Exhaust / intake / mechanical / transmission / tires / wind / chassis")
    UI.Text("Hybrid RPM bank: CC0 startup + six crossfaded engine loops")
    UI.Text("Event bank: deterministic gear-change and suspension-impact voices")
    UI.TextDisabled("Full detail <= 50 m | reduced <= 150 m | crowd <= 400 m")
end
