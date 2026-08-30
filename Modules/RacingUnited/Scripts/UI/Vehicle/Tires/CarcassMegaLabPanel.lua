-- TIRE45 development-only carcass parameter laboratory.
-- Nothing in this panel is serialized.  It intentionally exposes the raw
-- reduced-order structural solver so we can discover the correct physical
-- parameterization from evidence instead of hiding another guessed fix.

tireCarcassMegaLab = tireCarcassMegaLab or {
    wheel = 1,
    group = 1,
    scenario = 1,
    steps = 48,
    infos = nil,
    snapshotA = nil,
    snapshotB = nil,
    result = nil,
    searchRunning = false,
    searchBudget = 1000000,
    searchBatch = 1,
    searchSpread = 0.20,
    searchSeed = 451729,
    searchProgress = 0,
    searchBestScore = math.huge,
    searchBestTrial = nil,
    searchBestResult = nil,
    searchMsPerCandidate = nil,
    recallTrial = 0,
    searchBase = nil,
    searchGroups = nil
}

local TireCarcassScenarios = {
    "static_flat",
    "hard_acceleration",
    "hard_braking",
    "hard_cornering",
    "combined_braking_cornering",
    "low_pressure",
    "zero_pressure",
    "partial_road_edge",
    "banked_road",
    "flat_spot",
    "airborne_relaxation",
    "high_pressure"
}

local function CarcassLabAvailable()
    return nativeVehicle ~= nil and nativeVehicle ~= 0
        and Vehicle.Exists(nativeVehicle)
        and Vehicle.GetTireCarcassLabParameterCount ~= nil
end

local function EnsureCarcassParameterInfos()
    if tireCarcassMegaLab.infos ~= nil then return end
    tireCarcassMegaLab.infos = {}
    local count = Vehicle.GetTireCarcassLabParameterCount()
    for index = 1, count do
        local info = Vehicle.GetTireCarcassLabParameterInfo(index)
        if info ~= nil then
            info.index = index
            table.insert(tireCarcassMegaLab.infos, info)
        end
    end
    tireCarcassMegaLab.searchGroups = {}
    for index = 1, 16 do tireCarcassMegaLab.searchGroups[index] = index <= 6 end
end

local function CarcassGroupName(groupIndex)
    if tireCarcassMegaLab.infos == nil then return "unknown" end
    for _, info in ipairs(tireCarcassMegaLab.infos) do
        if info.group_index == groupIndex then return info.group end
    end
    return "unknown"
end

local function CarcassSearchMask()
    local mask = 0
    for index = 1, 16 do
        if tireCarcassMegaLab.searchGroups[index] then
            mask = mask | (1 << (index - 1))
        end
    end
    return mask
end

local function CaptureCarcassSnapshot()
    local snapshot = {}
    for _, info in ipairs(tireCarcassMegaLab.infos) do
        snapshot[info.index] = Vehicle.GetTireCarcassLabParameter(
            nativeVehicle, tireCarcassMegaLab.wheel, info.index)
    end
    return snapshot
end

local function RestoreCarcassSnapshot(snapshot)
    if snapshot == nil then return end
    tireCarcassMegaLab.searchRunning = false
    for index, value in pairs(snapshot) do
        Vehicle.SetTireCarcassLabParameter(
            nativeVehicle, tireCarcassMegaLab.wheel, index, value)
    end
end

local function PlotRange(values)
    if values == nil or #values == 0 then return -1.0, 1.0 end
    local lo, hi = math.huge, -math.huge
    for _, value in ipairs(values) do
        lo = math.min(lo, value)
        hi = math.max(hi, value)
    end
    if hi <= lo then
        local p = math.max(0.1, math.abs(lo) * 0.05)
        return lo - p, hi + p
    end
    local p = (hi - lo) * 0.08
    return lo - p, hi + p
end

local function DrawSyntheticCarcassResult(result)
    if result == nil then return end
    UI.Text(string.format(
        "%s | score %.3f | %d exact structural steps",
        result.scenario or "unknown",
        result.pathology_score or 0.0,
        result.integration_steps or 0))
    UI.Text(string.format(
        "Road penetration %.3f mm | rim penetration %.3f mm | inward hook %.3f mm",
        result.road_penetration_mm or 0.0,
        result.rim_penetration_mm or 0.0,
        result.lower_hook_mm or 0.0))
    UI.Text(string.format(
        "F/R asymmetry %.3f mm | tread-width height range %.3f mm | max displacement %.2f mm",
        result.static_asymmetry_mm or 0.0,
        result.footprint_height_range_mm or 0.0,
        result.maximum_displacement_mm or 0.0))
    UI.Text(string.format(
        "Bottom front/center/rear %.2f / %.2f / %.2f mm | residual velocity %.5f m/s",
        result.front_bottom_height_mm or 0.0,
        result.center_bottom_height_mm or 0.0,
        result.rear_bottom_height_mm or 0.0,
        result.rms_velocity_mps or 0.0))
    UI.Text(string.format(
        "Bottom-center displacement F/D/L %.2f / %.2f / %.2f mm | radial/tangent %.2f / %.2f mm",
        result.center_forward_displacement_mm or 0.0,
        result.center_down_displacement_mm or 0.0,
        result.center_lateral_displacement_mm or 0.0,
        result.center_radial_displacement_mm or 0.0,
        result.center_tangential_displacement_mm or 0.0))
    if result.radial_profile_mm ~= nil then
        local lo, hi = PlotRange(result.radial_profile_mm)
        UI.PlotLinesRange(
            "24-station radial displacement (mm)", 80.0, lo, hi,
            table.unpack(result.radial_profile_mm))
    end
    if result.bottom_cross_section_mm ~= nil then
        local lo, hi = PlotRange(result.bottom_cross_section_mm)
        UI.PlotLinesRange(
            "13-band bottom cross-section height (mm)", 80.0, lo, hi,
            table.unpack(result.bottom_cross_section_mm))
    end
end

local function DrawCarcassParameterGroup()
    local group = tireCarcassMegaLab.group
    UI.Text(string.format(
        "PARAMETER GROUP %d/16 — %s", group, CarcassGroupName(group)))
    local shown = 0
    for _, info in ipairs(tireCarcassMegaLab.infos) do
        if info.group_index == group then
            local value = Vehicle.GetTireCarcassLabParameter(
                nativeVehicle, tireCarcassMegaLab.wheel, info.index)
            if value ~= nil then
                local changed = false
                if info.integer then
                    value, changed = UI.SliderFloat(
                        info.label, value, info.minimum, info.maximum, "%.0f")
                    value = math.floor(value + 0.5)
                elseif info.key == "contact_slop_mm" then
                    value, changed = UI.SliderFloat(
                        info.label, value, info.minimum, info.maximum, "%.3f mm")
                else
                    value, changed = UI.SliderFloat(
                        info.label, value, info.minimum, info.maximum, "%.4f")
                end
                if changed then
                    tireCarcassMegaLab.searchRunning = false
                    Vehicle.SetTireCarcassLabParameter(
                        nativeVehicle, tireCarcassMegaLab.wheel, info.index, value)
                end
                shown = shown + 1
            end
        end
    end
    UI.TextDisabled(string.format(
        "%d sliders in this group; %d raw structural parameters total.",
        shown, #tireCarcassMegaLab.infos))
end

local function RunCurrentSyntheticScenario()
    tireCarcassMegaLab.result = Vehicle.RunTireCarcassSyntheticScenario(
        nativeVehicle,
        tireCarcassMegaLab.wheel,
        TireCarcassScenarios[tireCarcassMegaLab.scenario],
        tireCarcassMegaLab.steps)
    if tireCarcassMegaLab.result == nil then
        vehicleMessage = "Carcass scenario failed: " .. Vehicle.GetLastError()
    else
        vehicleMessage = "Carcass exact synthetic scenario completed."
    end
end

local function AdvanceCarcassSearch()
    if not tireCarcassMegaLab.searchRunning then return end
    if tireCarcassMegaLab.searchProgress >= tireCarcassMegaLab.searchBudget then
        tireCarcassMegaLab.searchRunning = false
        return
    end
    local count = math.min(
        tireCarcassMegaLab.searchBatch,
        tireCarcassMegaLab.searchBudget - tireCarcassMegaLab.searchProgress)
    local result = Vehicle.RunTireCarcassSearchBatch(
        nativeVehicle,
        tireCarcassMegaLab.wheel,
        TireCarcassScenarios[tireCarcassMegaLab.scenario],
        tireCarcassMegaLab.searchSeed,
        tireCarcassMegaLab.searchProgress,
        count,
        tireCarcassMegaLab.searchSpread,
        CarcassSearchMask(),
        tireCarcassMegaLab.steps)
    if result == nil then
        tireCarcassMegaLab.searchRunning = false
        vehicleMessage = "Carcass search stopped: " .. Vehicle.GetLastError()
        return
    end
    tireCarcassMegaLab.searchProgress = tireCarcassMegaLab.searchProgress + count
    if result.elapsed_seconds ~= nil and result.evaluated_count ~= nil
        and result.evaluated_count > 0 then
        local measured = result.elapsed_seconds * 1000.0 / result.evaluated_count
        if tireCarcassMegaLab.searchMsPerCandidate == nil then
            tireCarcassMegaLab.searchMsPerCandidate = measured
        else
            tireCarcassMegaLab.searchMsPerCandidate =
                tireCarcassMegaLab.searchMsPerCandidate * 0.8 + measured * 0.2
        end
    end
    if result.best_score ~= nil
        and result.best_score < tireCarcassMegaLab.searchBestScore then
        tireCarcassMegaLab.searchBestScore = result.best_score
        tireCarcassMegaLab.searchBestTrial = result.best_trial
        tireCarcassMegaLab.searchBestResult = result.result
    end
end

function DrawTireCarcassMegaLab()
    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("PHYSICS CARCASS MEGALAB | TIRE45")
    UI.TextWrapped(
        "Raw development access to the TIRE44 structural lattice. 217 parameters are exposed: global physics plus every 24 circumferential station and 13 width band. Nothing here is saved into the tire or Magic Formula. Reset/disable returns to the production solver.")

    if not CarcassLabAvailable() then
        UI.TextDisabled("Spawn the native vehicle to use the carcass laboratory.")
        return
    end
    EnsureCarcassParameterInfos()

    local wheelCount = math.max(1, Vehicle.GetWheelCount(nativeVehicle))
    local changed = false
    tireCarcassMegaLab.wheel, changed = UI.InputInt(
        "Carcass lab wheel (1-based)", tireCarcassMegaLab.wheel, 1)
    tireCarcassMegaLab.wheel = math.max(1, math.min(wheelCount, tireCarcassMegaLab.wheel))

    local enabled = Vehicle.GetTireCarcassLabEnabled(
        nativeVehicle, tireCarcassMegaLab.wheel)
    if enabled == nil then enabled = false end
    enabled, changed = UI.Checkbox("Enable raw carcass overrides", enabled)
    if changed then
        Vehicle.SetTireCarcassLabEnabled(
            nativeVehicle, tireCarcassMegaLab.wheel, enabled)
    end

    UI.TextDisabled("LIVE SIMULATION CONTROL")
    local physicsScale = Physics.GetTimeScale() or 1.0
    UI.Text(string.format("Physics time scale: %.3f x", physicsScale))
    local quarter = math.max(80.0, (UI.GetAvailableWidth() - 18.0) * 0.25)
    if UI.Button("FREEZE 0x", quarter, 28.0, false) then Physics.SetTimeScale(0.0) end
    UI.SameLine()
    if UI.Button("SLOW 0.10x", quarter, 28.0, false) then Physics.SetTimeScale(0.10) end
    UI.SameLine()
    if UI.Button("SLOW 0.25x", quarter, 28.0, false) then Physics.SetTimeScale(0.25) end
    UI.SameLine()
    if UI.Button("REALTIME 1x", quarter, 28.0, false) then Physics.SetTimeScale(1.0) end
    if UI.Button("RESET CARCASS DYNAMIC STATE", UI.GetAvailableWidth(), 28.0, false) then
        Vehicle.ResetTireCarcassLabState(nativeVehicle, tireCarcassMegaLab.wheel)
        vehicleMessage = "Carcass displacement/velocity state reset; current 217 parameters preserved."
    end

    local width = math.max(110.0, (UI.GetAvailableWidth() - 8.0) * 0.5)
    if UI.Button("RESET 217 PARAMETERS", width, 30.0, false) then
        Vehicle.ResetTireCarcassLab(nativeVehicle, tireCarcassMegaLab.wheel)
        tireCarcassMegaLab.searchRunning = false
        vehicleMessage = "TIRE45 carcass controls reset to exact TIRE44 defaults."
    end
    UI.SameLine()
    if UI.Button("DISABLE / PRODUCTION", width, 30.0, false) then
        Vehicle.SetTireCarcassLabEnabled(nativeVehicle, tireCarcassMegaLab.wheel, false)
        tireCarcassMegaLab.searchRunning = false
        vehicleMessage = "Carcass lab disabled; production TIRE44 values active."
    end
    if UI.Button("COPY THIS WHEEL TO ALL", width, 30.0, false) then
        Vehicle.CopyTireCarcassLabToAllWheels(nativeVehicle, tireCarcassMegaLab.wheel)
    end
    UI.SameLine()
    if UI.Button("RUN EXACT SCENARIO", width, 30.0, false) then
        RunCurrentSyntheticScenario()
    end

    if UI.Button("CAPTURE A", width, 28.0, false) then
        tireCarcassMegaLab.snapshotA = CaptureCarcassSnapshot()
    end
    UI.SameLine()
    if UI.Button("RESTORE A", width, 28.0, false) then
        RestoreCarcassSnapshot(tireCarcassMegaLab.snapshotA)
    end
    if UI.Button("CAPTURE B", width, 28.0, false) then
        tireCarcassMegaLab.snapshotB = CaptureCarcassSnapshot()
    end
    UI.SameLine()
    if UI.Button("RESTORE B", width, 28.0, false) then
        RestoreCarcassSnapshot(tireCarcassMegaLab.snapshotB)
    end

    UI.Spacing()
    tireCarcassMegaLab.group, changed = UI.InputInt(
        "Visible parameter group (1-16)", tireCarcassMegaLab.group, 1)
    tireCarcassMegaLab.group = math.max(1, math.min(16, tireCarcassMegaLab.group))
    DrawCarcassParameterGroup()

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("EXACT ISOLATED STRUCTURAL SCENARIO")
    tireCarcassMegaLab.scenario, changed = UI.InputInt(
        "Scenario (1-12)", tireCarcassMegaLab.scenario, 1)
    tireCarcassMegaLab.scenario = math.max(1, math.min(#TireCarcassScenarios, tireCarcassMegaLab.scenario))
    UI.Text("Selected: " .. TireCarcassScenarios[tireCarcassMegaLab.scenario])
    tireCarcassMegaLab.steps, changed = UI.InputInt(
        "Exact structural integration steps", tireCarcassMegaLab.steps, 4)
    tireCarcassMegaLab.steps = math.max(4, math.min(240, tireCarcassMegaLab.steps))
    UI.TextDisabled(
        "This calls the same 24x13 implicit solver as the live tire; it is not a reduced proxy. The score only sorts obvious pathologies and never feeds physics.")
    DrawSyntheticCarcassResult(tireCarcassMegaLab.result)

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("BRUTE-FORCE EXACT PARAMETER SEARCH")
    UI.TextWrapped(
        "Set a budget up to 1,000,000 exact candidates. Search advances in bounded batches while this LAB tab is visible, so one million is allowed without one giant frozen call. One million exact solves can take hours; measured ms/candidate and ETA are shown once search starts. Use 1 trial/frame for interactive use or raise it if you accept a slower frame while searching.")
    tireCarcassMegaLab.searchBudget, changed = UI.InputInt(
        "Candidate budget", tireCarcassMegaLab.searchBudget, 1000)
    tireCarcassMegaLab.searchBudget = math.max(1, math.min(1000000, tireCarcassMegaLab.searchBudget))
    tireCarcassMegaLab.searchBatch, changed = UI.InputInt(
        "Exact candidates per UI frame", tireCarcassMegaLab.searchBatch, 1)
    tireCarcassMegaLab.searchBatch = math.max(1, math.min(256, tireCarcassMegaLab.searchBatch))
    tireCarcassMegaLab.searchSpread, changed = UI.SliderFloat(
        "Random search spread", tireCarcassMegaLab.searchSpread, 0.0, 1.0, "%.3f")
    tireCarcassMegaLab.searchSeed, changed = UI.InputInt(
        "Deterministic search seed", tireCarcassMegaLab.searchSeed, 1)
    tireCarcassMegaLab.searchSeed = math.max(0, tireCarcassMegaLab.searchSeed)

    UI.TextDisabled("Groups included in random search:")
    for index = 1, 16 do
        local selected = tireCarcassMegaLab.searchGroups[index]
        selected, changed = UI.Checkbox(
            string.format("%02d %s", index, CarcassGroupName(index)), selected)
        tireCarcassMegaLab.searchGroups[index] = selected
    end

    if not tireCarcassMegaLab.searchRunning then
        if UI.Button("START / RESTART SEARCH", width, 32.0, false) then
            tireCarcassMegaLab.searchRunning = true
            tireCarcassMegaLab.searchProgress = 0
            tireCarcassMegaLab.searchBestScore = math.huge
            tireCarcassMegaLab.searchBestTrial = nil
            tireCarcassMegaLab.searchBestResult = nil
            tireCarcassMegaLab.searchMsPerCandidate = nil
            tireCarcassMegaLab.searchBase = CaptureCarcassSnapshot()
        end
    else
        if UI.Button("STOP SEARCH", width, 32.0, false) then
            tireCarcassMegaLab.searchRunning = false
        end
    end
    UI.SameLine()
    if UI.Button("APPLY BEST TRIAL", width, 32.0, false)
        and tireCarcassMegaLab.searchBestTrial ~= nil then
        RestoreCarcassSnapshot(tireCarcassMegaLab.searchBase)
        Vehicle.ApplyTireCarcassSearchTrial(
            nativeVehicle,
            tireCarcassMegaLab.wheel,
            tireCarcassMegaLab.searchSeed,
            tireCarcassMegaLab.searchBestTrial,
            tireCarcassMegaLab.searchSpread,
            CarcassSearchMask())
        tireCarcassMegaLab.searchRunning = false
        vehicleMessage = string.format(
            "Applied carcass search trial %d (score %.4f).",
            tireCarcassMegaLab.searchBestTrial,
            tireCarcassMegaLab.searchBestScore)
    end

    UI.Text(string.format(
        "Search: %s | %d / %d candidates (%.2f%%)",
        tireCarcassMegaLab.searchRunning and "RUNNING" or "stopped",
        tireCarcassMegaLab.searchProgress,
        tireCarcassMegaLab.searchBudget,
        100.0 * tireCarcassMegaLab.searchProgress
            / math.max(1, tireCarcassMegaLab.searchBudget)))
    if tireCarcassMegaLab.searchMsPerCandidate ~= nil then
        local remaining = math.max(0, tireCarcassMegaLab.searchBudget
            - tireCarcassMegaLab.searchProgress)
        local etaHours = remaining * tireCarcassMegaLab.searchMsPerCandidate
            / 3600000.0
        UI.Text(string.format(
            "Measured %.3f ms/candidate | remaining ETA %.2f h (current CPU / current step count)",
            tireCarcassMegaLab.searchMsPerCandidate, etaHours))
    end
    if tireCarcassMegaLab.searchBestTrial ~= nil then
        UI.Text(string.format(
            "Best trial %d | pathology score %.5f",
            tireCarcassMegaLab.searchBestTrial,
            tireCarcassMegaLab.searchBestScore))
        DrawSyntheticCarcassResult(tireCarcassMegaLab.searchBestResult)
    end

    UI.Spacing()
    UI.TextDisabled("DETERMINISTIC TRIAL RECALL")
    UI.TextWrapped(
        "Every search candidate is reproducible from seed + trial index + spread + group mask. Type a candidate number here to reapply it directly without rerunning the preceding candidates.")
    tireCarcassMegaLab.recallTrial, changed = UI.InputInt(
        "Trial index to apply", tireCarcassMegaLab.recallTrial, 1)
    tireCarcassMegaLab.recallTrial = math.max(0, tireCarcassMegaLab.recallTrial)
    if UI.Button("APPLY TRIAL INDEX", UI.GetAvailableWidth(), 30.0, false) then
        if tireCarcassMegaLab.searchBase == nil then
            tireCarcassMegaLab.searchBase = CaptureCarcassSnapshot()
        end
        RestoreCarcassSnapshot(tireCarcassMegaLab.searchBase)
        if Vehicle.ApplyTireCarcassSearchTrial(
            nativeVehicle,
            tireCarcassMegaLab.wheel,
            tireCarcassMegaLab.searchSeed,
            tireCarcassMegaLab.recallTrial,
            tireCarcassMegaLab.searchSpread,
            CarcassSearchMask()) then
            vehicleMessage = string.format(
                "Applied deterministic carcass trial %d.",
                tireCarcassMegaLab.recallTrial)
        else
            vehicleMessage = "Could not apply carcass trial: " .. Vehicle.GetLastError()
        end
    end

    AdvanceCarcassSearch()
end
