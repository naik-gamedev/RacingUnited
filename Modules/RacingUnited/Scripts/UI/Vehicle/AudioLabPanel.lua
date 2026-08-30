-- AUDIO01 - Heritage Engine Sound Capture Laboratory
-- Engine Simulator CE is the virtual dyno/source generator. Heritage captures
-- its Windows output losslessly, preserves raw WAVs, and auditions non-destructive
-- acoustic shaping before the RPM x throttle bank is committed to a vehicle.

local engineLabProfile = nil
local engineLabProfileName = "Peugeot206RC_EW10J4S"
local engineLabCaptureSeconds = 6.0
local engineLabBankSeconds = 4.0
local engineLabBankIndex = 1
local engineLabPendingBankAdvance = false
local engineLabWasCapturing = false

local engineLabTargets = {}
table.insert(engineLabTargets, { rpm = 850, throttle = 0, label = "Warm idle" })
for rpm = 1000, 7000, 500 do
    for _, throttle in ipairs({25, 50, 75, 100}) do
        table.insert(engineLabTargets, {
            rpm = rpm,
            throttle = throttle,
            label = string.format("%d RPM / %d%% throttle", rpm, throttle)
        })
    end
end

local function EnsureEngineLabProfile()
    if engineLabProfile == nil then
        engineLabProfile = Audio.EngineLabGetProfile()
    end
    if engineLabProfile == nil then
        engineLabProfile = {}
    end
end

local function PushEngineLabProfile()
    if engineLabProfile ~= nil then
        Audio.EngineLabSetProfile(engineLabProfile)
    end
end

local function LabSlider(label, field, minimum, maximum, format)
    EnsureEngineLabProfile()
    local value = engineLabProfile[field] or minimum
    local changed = false
    value, changed = UI.SliderFloat(label, value, minimum, maximum, format or "%.3f")
    if changed then
        engineLabProfile[field] = value
        PushEngineLabProfile()
    end
    return changed
end

local function DrawEngineLabStatus(state)
    UI.Text("Backend: " .. tostring(Audio.GetBackend()))
    UI.Text("Lab root: " .. tostring(state.root or ""))
    if state.capturing then
        UI.ProgressBar(state.progress or 0.0, -1.0, 18.0,
            string.format("CAPTURING %.0f%%", (state.progress or 0.0) * 100.0))
        UI.TextDisabled("Heritage master output is muted during WASAPI loopback capture so only Engine Simulator is recorded.")
    elseif (state.lastRawPath or "") ~= "" then
        UI.Text(string.format(
            "Last raw: %.2f s | %d Hz | peak %.3f | RMS %.3f",
            state.capturedDurationSeconds or 0.0,
            state.sampleRate or 0,
            state.peak or 0.0,
            state.rms or 0.0))
        UI.TextWrapped(tostring(state.lastRawPath))
    else
        UI.TextDisabled("No Engine Simulator source has been captured yet.")
    end
    if (state.lastError or "") ~= "" then
        UI.TextWrapped("LAB ERROR: " .. tostring(state.lastError))
    end
end

local function DrawEngineLabCapture(state)
    UI.TextDisabled("ENGINE SIMULATOR CE -> HERITAGE RAW SOURCE")
    UI.TextWrapped(
        "Run Engine-Simulator Community Edition on the same Windows playback device. " ..
        "Heritage records the endpoint with WASAPI loopback into stereo 32-bit-float 48 kHz WAV. " ..
        "The source WAV remains untouched; all shaping below is non-destructive.")
    UI.Spacing()

    engineLabCaptureSeconds = UI.SliderFloat(
        "Calibration sample seconds", engineLabCaptureSeconds, 2.0, 12.0, "%.1f s")

    if not state.capturing then
        if UI.Button("CAPTURE CALIBRATION SAMPLE", UI.GetAvailableWidth(), 34.0, false) then
            Audio.EngineLabStartCalibrationCapture(engineLabCaptureSeconds)
        end
    else
        if UI.Button("STOP CAPTURE", UI.GetAvailableWidth(), 34.0, false) then
            Audio.EngineLabStopCapture()
        end
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("GUIDED RPM x THROTTLE BANK")
    UI.TextWrapped(
        "53 steady-state cells: warm idle, then 1000-7000 RPM every 500 RPM at 25/50/75/100% throttle. " ..
        "Use Engine Simulator Dyno + RPM HOLD, stabilize the requested point, then capture it here.")

    local total = #engineLabTargets
    engineLabBankIndex = math.max(1, math.min(engineLabBankIndex, total))
    local target = engineLabTargets[engineLabBankIndex]
    UI.Text(string.format("Target %d / %d: %s", engineLabBankIndex, total, target.label))
    if target.rpm == 850 then
        UI.TextDisabled("Let the EW10J4S settle at warm idle; do not use RPM hold for this cell.")
    else
        UI.TextDisabled(string.format(
            "Engine Simulator: Dyno ON | RPM HOLD %d | throttle %d%% | wait until stable.",
            target.rpm, target.throttle))
    end

    engineLabBankSeconds = UI.SliderFloat(
        "Bank cell seconds", engineLabBankSeconds, 2.0, 8.0, "%.1f s")

    if UI.Button("PREVIOUS TARGET") then
        engineLabBankIndex = math.max(1, engineLabBankIndex - 1)
    end
    UI.SameLine()
    if UI.Button("NEXT TARGET") then
        engineLabBankIndex = math.min(total, engineLabBankIndex + 1)
    end

    if not state.capturing then
        if UI.Button("CAPTURE CURRENT TARGET", UI.GetAvailableWidth(), 36.0, false) then
            local started = Audio.EngineLabStartBankCapture(
                "Peugeot206RC", "EW10J4S",
                target.rpm, target.throttle,
                engineLabBankSeconds)
            if started then
                engineLabPendingBankAdvance = true
            end
        end
    end

    -- A completed guided capture advances exactly once. The native lab owns
    -- file naming + CSV manifest, while Lua owns only this authoring cursor.
    if engineLabPendingBankAdvance and engineLabWasCapturing and not state.capturing then
        if (state.lastRawPath or "") ~= "" and (state.lastError or "") == "" then
            engineLabBankIndex = math.min(total, engineLabBankIndex + 1)
        end
        engineLabPendingBankAdvance = false
    end
    engineLabWasCapturing = state.capturing

    UI.ProgressBar((engineLabBankIndex - 1) / math.max(total, 1), -1.0, 16.0,
        string.format("Bank progress cursor: %d / %d", engineLabBankIndex, total))
end

local function DrawEngineLabShape(state)
    EnsureEngineLabProfile()
    UI.TextDisabled("MATERIALIZE-STYLE NON-DESTRUCTIVE SOUND SHAPING")
    UI.TextWrapped(
        "RAW is the captured Engine Simulator signal. ENGINE BAY, REAR / EXHAUST and DRIVER CABIN " ..
        "all derive from that same raw source. These controls change acoustic character rather than forcing you to rerecord the RPM bank.")

    UI.Separator()
    UI.TextDisabled("SOURCE CHARACTER")
    LabSlider("Input gain", "inputGainDb", -12.0, 8.0, "%.1f dB")
    LabSlider("High-pass", "highPassHz", 10.0, 250.0, "%.0f Hz")
    LabSlider("Low-pass", "lowPassHz", 3000.0, 22000.0, "%.0f Hz")
    LabSlider("Engine body gain", "bodyGainDb", -8.0, 10.0, "%.1f dB")
    LabSlider("Engine body frequency", "bodyFrequencyHz", 50.0, 350.0, "%.0f Hz")
    LabSlider("Engine body Q", "bodyQ", 0.25, 3.0, "%.2f")
    LabSlider("Electric/raw presence reduction", "presenceCutDb", 0.0, 14.0, "%.1f dB")
    LabSlider("Presence frequency", "presenceFrequencyHz", 800.0, 6500.0, "%.0f Hz")
    LabSlider("Presence Q", "presenceQ", 0.25, 3.0, "%.2f")
    LabSlider("High-frequency shelf", "highShelfDb", -14.0, 8.0, "%.1f dB")
    LabSlider("Pulse-edge softening", "pulseSoftening", 0.0, 1.0, "%.2f")
    LabSlider("Bounded saturation", "saturation", 0.0, 0.5, "%.2f")

    UI.Separator()
    UI.TextDisabled("ENGINE BAY / EXHAUST")
    LabSlider("Mechanical presence", "mechanicalPresence", 0.0, 1.0, "%.2f")
    LabSlider("Intake / induction presence", "intakePresence", 0.0, 1.0, "%.2f")
    LabSlider("Intake resonance frequency", "intakeFrequencyHz", 300.0, 3500.0, "%.0f Hz")
    LabSlider("Stock exhaust muffling", "exhaustMuffling", 0.0, 1.0, "%.2f")
    LabSlider("Exhaust body gain", "exhaustBodyGainDb", -6.0, 10.0, "%.1f dB")
    LabSlider("Exhaust body frequency", "exhaustBodyFrequencyHz", 45.0, 260.0, "%.0f Hz")
    LabSlider("Exhaust body Q", "exhaustBodyQ", 0.25, 3.0, "%.2f")

    UI.Separator()
    UI.TextDisabled("CABIN / LISTENER TRANSMISSION PREVIEW")
    LabSlider("Cabin damping / insulation", "cabinDamping", 0.0, 1.0, "%.2f")
    LabSlider("Low-frequency leakage", "cabinLowFrequencyLeak", 0.0, 1.0, "%.2f")
    LabSlider("Cabin resonance", "cabinResonance", 0.0, 1.0, "%.2f")
    LabSlider("Cabin resonance frequency", "cabinResonanceHz", 45.0, 240.0, "%.0f Hz")
    LabSlider("Reflection / reverb preview", "reverbPreview", 0.0, 0.45, "%.2f")
    LabSlider("Occlusion preview", "occlusionPreview", 0.0, 1.0, "%.2f")
    LabSlider("Processed output gain", "outputGainDb", -12.0, 8.0, "%.1f dB")

    UI.Spacing()
    if UI.Button("RAW", 90.0, 30.0, false) then Audio.EngineLabPlayPreview("raw") end
    UI.SameLine()
    if UI.Button("ENGINE BAY", 120.0, 30.0, false) then Audio.EngineLabPlayPreview("engine_bay") end
    UI.SameLine()
    if UI.Button("REAR / EXHAUST", 140.0, 30.0, false) then Audio.EngineLabPlayPreview("rear_exhaust") end
    UI.SameLine()
    if UI.Button("DRIVER CABIN", 130.0, 30.0, false) then Audio.EngineLabPlayPreview("driver_cabin") end

    if state.previewPlaying then
        if UI.Button("STOP AUDITION") then Audio.EngineLabStopPreview() end
    end

    UI.Spacing()
    engineLabProfileName = UI.InputText("Acoustic profile", engineLabProfileName, 80)
    if UI.Button("SAVE PROFILE") then
        Audio.EngineLabSetProfile(engineLabProfile)
        Audio.EngineLabSaveProfile(engineLabProfileName)
    end
    UI.SameLine()
    if UI.Button("LOAD PROFILE") then
        if Audio.EngineLabLoadProfile(engineLabProfileName) then
            engineLabProfile = Audio.EngineLabGetProfile()
        end
    end

    UI.TextDisabled(
        "Cabin, distance, Doppler, occlusion and environment remain runtime concepts. " ..
        "The profile stores vehicle-intrinsic source/exhaust/cabin coloration; raw captures are never overwritten.")
end

local function DrawExistingVehicleAudioRuntime()
    UI.TextDisabled("CURRENT HERITAGE VEHICLE AUDIO RUNTIME")
    local enabled, changed = UI.Checkbox("Enable vehicle sound", vehicleAudioEnabled)
    if changed then SetVehicleAudioEnabled(enabled) end
    local runtimeStats = Audio.GetRuntimeStats()
    if runtimeStats ~= nil then
        UI.Text(string.format(
            "Native voices: %d | decoded cache: %.2f MiB",
            runtimeStats.activeVoices or 0,
            runtimeStats.cachedAudioMiB or 0.0))
    end
    local state = nil
    if vehicleAudioHandle ~= 0 then state = Audio.GetVehicleSoundState(vehicleAudioHandle) end
    if state ~= nil and state.valid then
        UI.Text(string.format(
            "RPM %.0f | load %.0f%% | %s | LOD %s | distance %.1f m",
            state.engineRpm or 0.0,
            (state.engineLoad or 0.0) * 100.0,
            state.interior and "INTERIOR" or "EXTERIOR",
            tostring(state.detail),
            state.distanceMeters or 0.0))
    end
end

function DrawVehicleAudioLabPanel()
    local state = Audio.EngineLabGetState()
    if state == nil then
        UI.TextWrapped("Engine Sound Capture Laboratory is unavailable in this build.")
        return
    end

    UI.TextDisabled("HERITAGE ENGINE - ENGINE SOUND CAPTURE LABORATORY [AUDIO01]")
    DrawEngineLabStatus(state)
    UI.Spacing()

    if UI.BeginTabBar("EngineSoundLaboratoryTabs") then
        if UI.BeginTabItem("CAPTURE") then
            DrawEngineLabCapture(state)
            UI.EndTabItem()
        end
        if UI.BeginTabItem("SHAPE / FILTER") then
            DrawEngineLabShape(state)
            UI.EndTabItem()
        end
        if UI.BeginTabItem("RUNTIME") then
            DrawExistingVehicleAudioRuntime()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
