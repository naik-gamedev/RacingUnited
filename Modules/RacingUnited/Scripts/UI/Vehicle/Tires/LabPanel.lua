-- TIRE15C2 development-only tire/rubber acceleration controls.
-- These multipliers are runtime lab state only; they are not saved into the
-- vehicle definition, tire part or GLB metadata.

tireSteadyStateLab = tireSteadyStateLab or {
    wheel = 1,
    sweep = 1,
    secondarySlice = 1,
    maximumPoints = 180,
    runA = nil,
    runB = nil
}

local TireSteadyStateSweepNames = {
    "pure_longitudinal",
    "pure_lateral",
    "combined_slip_map",
    "load_sensitivity",
    "pressure_sensitivity",
    "camber_sensitivity",
    "turn_slip_sensitivity"
}

tireStatefulScenarioLab = tireStatefulScenarioLab or {
    scenario = 1,
    maximumPoints = 240,
    result = nil
}

tireFleetPerformanceLab = tireFleetPerformanceLab or {
    vehicles = 150,
    durationSeconds = 0.25,
    result = nil
}

local TireStatefulScenarioNames = {
    "relaxation_step",
    "heating_cooling",
    "sustained_cornering_wear",
    "braking_flat_spot",
    "brake_rim_soak",
    "slow_puncture_pressure_loss",
    "blowout_pressure_loss"
}

local function TireCalibrationRun()
    if nativeVehicle == nil or nativeVehicle == 0
        or not Vehicle.Exists(nativeVehicle) then
        vehicleMessage = "Spawn a native vehicle before running tire calibration."
        return nil
    end
    local result = Vehicle.RunTireCalibrationSweep(
        nativeVehicle,
        tireSteadyStateLab.wheel,
        TireSteadyStateSweepNames[tireSteadyStateLab.sweep],
        tireSteadyStateLab.secondarySlice,
        tireSteadyStateLab.maximumPoints)
    if result == nil then
        vehicleMessage = "Tire calibration failed: " .. Vehicle.GetLastError()
        return nil
    end
    tireSteadyStateLab.secondarySlice = result.secondary_slice or 1
    vehicleMessage = string.format(
        "Tire calibration: %s | wheel %d | %d plotted / %d native samples",
        result.name or "unknown",
        tireSteadyStateLab.wheel,
        result.sample_count or 0,
        (result.primary_count or 0) * (result.secondary_count or 1))
    return result
end

local function TireCalibrationRange(first, second)
    local minimum = math.huge
    local maximum = -math.huge
    local function Consume(values)
        if values == nil then return end
        for index = 1, #values do
            local value = values[index]
            if type(value) == "number" then
                minimum = math.min(minimum, value)
                maximum = math.max(maximum, value)
            end
        end
    end
    Consume(first)
    Consume(second)
    if minimum == math.huge then return -1.0, 1.0 end
    if maximum <= minimum then
        local padding = math.max(1.0, math.abs(minimum) * 0.05)
        return minimum - padding, maximum + padding
    end
    local padding = (maximum - minimum) * 0.05
    return minimum - padding, maximum + padding
end

local function TireCalibrationComparable(first, second)
    if first == nil or second == nil then return false end
    if first.name ~= second.name
        or first.primary_axis ~= second.primary_axis
        or first.sample_count ~= second.sample_count then
        return false
    end
    if first.primary == nil or second.primary == nil
        or #first.primary ~= #second.primary then
        return false
    end
    if #first.primary == 0 then return false end
    local tolerance = 0.000000001
    return math.abs(first.primary[1] - second.primary[1]) <= tolerance
        and math.abs(
            first.primary[#first.primary] - second.primary[#second.primary])
            <= tolerance
end

local function DrawTireCalibrationChannel(label, field, unit)
    local first = tireSteadyStateLab.runA
    if first == nil or first[field] == nil then return end
    local second = TireCalibrationComparable(first, tireSteadyStateLab.runB)
        and tireSteadyStateLab.runB or nil
    local minimum, maximum = TireCalibrationRange(
        first[field], second ~= nil and second[field] or nil)
    UI.PlotLinesRange(
        label .. " A (" .. unit .. ")", 68.0, minimum, maximum,
        table.unpack(first[field]))
    if second ~= nil then
        UI.PlotLinesRange(
            label .. " B (" .. unit .. ")", 68.0, minimum, maximum,
            table.unpack(second[field]))
        local largestDelta = 0.0
        local squaredDelta = 0.0
        for index = 1, #first[field] do
            local delta = second[field][index] - first[field][index]
            largestDelta = math.max(largestDelta, math.abs(delta))
            squaredDelta = squaredDelta + delta * delta
        end
        UI.TextDisabled(string.format(
            "A/B delta: RMS %.4g %s | maximum %.4g %s",
            math.sqrt(squaredDelta / math.max(1, #first[field])), unit,
            largestDelta, unit))
    end
end

local function DrawTireSteadyStateCalibrationLab()
    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("STEADY-STATE FITTED-TIRE CALIBRATION | TIRE18B")
    UI.TextWrapped(
        "Runs the deterministic force model fitted to the selected installed wheel. A/B captures remain in memory so setup and tire-data changes use identical axes and plot scales.")

    local changed = false
    tireSteadyStateLab.wheel, changed = UI.InputInt(
        "Calibration wheel (1-based)", tireSteadyStateLab.wheel, 1)
    local wheelCount = nativeVehicle ~= nil and nativeVehicle ~= 0
        and Vehicle.GetWheelCount(nativeVehicle) or 1
    tireSteadyStateLab.wheel = math.max(
        1, math.min(math.max(1, wheelCount), tireSteadyStateLab.wheel))

    tireSteadyStateLab.sweep, changed = UI.InputInt(
        "Canonical sweep (1-7)", tireSteadyStateLab.sweep, 1)
    tireSteadyStateLab.sweep = math.max(
        1, math.min(#TireSteadyStateSweepNames, tireSteadyStateLab.sweep))
    UI.Text(string.format(
        "Selected: %s", TireSteadyStateSweepNames[tireSteadyStateLab.sweep]))

    tireSteadyStateLab.secondarySlice, changed = UI.InputInt(
        "Combined-map lateral slice (1-based)",
        tireSteadyStateLab.secondarySlice, 1)
    local maximumSlice = TireSteadyStateSweepNames[tireSteadyStateLab.sweep]
        == "combined_slip_map" and 49 or 1
    tireSteadyStateLab.secondarySlice = math.max(
        1, math.min(maximumSlice, tireSteadyStateLab.secondarySlice))

    local buttonWidth = math.max(120.0, (UI.GetAvailableWidth() - 8.0) * 0.5)
    if UI.Button("CAPTURE A", buttonWidth, 32.0, false) then
        tireSteadyStateLab.runA = TireCalibrationRun()
    end
    UI.SameLine()
    if UI.Button("CAPTURE B", buttonWidth, 32.0, false) then
        tireSteadyStateLab.runB = TireCalibrationRun()
    end
    if UI.Button("CLEAR A/B", buttonWidth, 32.0, false) then
        tireSteadyStateLab.runA = nil
        tireSteadyStateLab.runB = nil
        vehicleMessage = "Fitted-tire calibration comparison cleared."
    end
    UI.SameLine()
    if UI.Button("EXPORT FULL CSV", buttonWidth, 32.0, false) then
        if nativeVehicle == nil or nativeVehicle == 0
            or not Vehicle.Exists(nativeVehicle) then
            vehicleMessage = "Spawn a native vehicle before exporting tire calibration."
        else
            local sweepName = TireSteadyStateSweepNames[tireSteadyStateLab.sweep]
            local filename = string.format(
                "wheel_%d_%s.csv", tireSteadyStateLab.wheel, sweepName)
            local exported, message = Vehicle.ExportTireCalibrationSweepCsv(
                nativeVehicle, tireSteadyStateLab.wheel, sweepName, filename)
            vehicleMessage = exported
                and ("Tire calibration exported: " .. message)
                or ("Tire calibration export failed: " .. message)
        end
    end

    local first = tireSteadyStateLab.runA
    if first == nil then
        UI.TextDisabled("Capture A to display fitted-tire evidence.")
        return
    end
    UI.Text(string.format(
        "%s | primary %s (%d) | secondary %s slice %d/%d",
        first.name or "unknown", first.primary_axis or "unknown",
        first.primary_count or 0, first.secondary_axis or "none",
        first.secondary_slice or 1, first.secondary_count or 1))
    UI.TextWrapped(string.format(
        "Source: %s | provenance: %s | confidence %.2f | %s",
        first.parameter_source ~= "" and first.parameter_source or "compatibility seed",
        first.parameter_provenance ~= "" and first.parameter_provenance or "not supplied",
        first.parameter_confidence or 0.0,
        first.legacy_seed and "LEGACY SEED / NOT A MEASURED FIT" or "EXPLICIT FIT"))
    UI.TextDisabled(string.format(
        "Declared validity: load %.0f..%.0f N | pressure %.1f..%.1f PSI | |kappa| <= %.3f | |alpha| <= %.2f deg | |camber| <= %.2f deg",
        first.minimum_load_n or 0.0, first.maximum_load_n or 0.0,
        (first.minimum_pressure_pa or 0.0) / 6894.757293168,
        (first.maximum_pressure_pa or 0.0) / 6894.757293168,
        first.maximum_abs_longitudinal_slip or 0.0,
        math.deg(first.maximum_abs_slip_angle_rad or 0.0),
        math.deg(first.maximum_abs_camber_rad or 0.0)))
    if tireSteadyStateLab.runB ~= nil
        and not TireCalibrationComparable(first, tireSteadyStateLab.runB) then
        UI.TextDisabled(
            "Capture B uses a different sweep/axis. Select matching settings and recapture B for numerical comparison.")
    end

    DrawTireCalibrationChannel("Longitudinal force Fx", "fx", "N")
    DrawTireCalibrationChannel("Lateral force Fy", "fy", "N")
    DrawTireCalibrationChannel("Aligning moment Mz", "mz", "Nm")
    DrawTireCalibrationChannel("Pneumatic trail", "trail_mm", "mm")
    DrawTireCalibrationChannel("Grip utilization", "grip_percent", "%")
end

local function DrawTireScenarioChannel(result, label, field, unit)
    if result == nil or result[field] == nil then return end
    local minimum, maximum = TireCalibrationRange(result[field], nil)
    UI.PlotLinesRange(
        label .. " (" .. unit .. ")", 68.0, minimum, maximum,
        table.unpack(result[field]))
end

local function DrawTireStatefulScenarioLab()
    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("STATEFUL FITTED-TIRE SCENARIOS | TIRE18C")
    UI.TextWrapped(
        "Runs isolated copies of the selected wheel's real transient, thermal, pressure, wear and failure state. The live car is never modified.")
    local changed = false
    tireStatefulScenarioLab.scenario, changed = UI.InputInt(
        "Stateful scenario (1-7)", tireStatefulScenarioLab.scenario, 1)
    tireStatefulScenarioLab.scenario = math.max(
        1, math.min(#TireStatefulScenarioNames,
            tireStatefulScenarioLab.scenario))
    local scenarioName = TireStatefulScenarioNames[
        tireStatefulScenarioLab.scenario]
    UI.Text("Selected: " .. scenarioName)

    local buttonWidth = math.max(120.0, (UI.GetAvailableWidth() - 8.0) * 0.5)
    if UI.Button("RUN STATEFUL SCENARIO", buttonWidth, 32.0, false) then
        if nativeVehicle == nil or nativeVehicle == 0
            or not Vehicle.Exists(nativeVehicle) then
            vehicleMessage = "Spawn a native vehicle before running a tire scenario."
        else
            tireStatefulScenarioLab.result = Vehicle.RunTireScenario(
                nativeVehicle,
                tireSteadyStateLab.wheel,
                scenarioName,
                tireStatefulScenarioLab.maximumPoints)
            if tireStatefulScenarioLab.result ~= nil then
                vehicleMessage = string.format(
                    "Tire scenario: %s | %d plotted / %d native samples",
                    scenarioName,
                    tireStatefulScenarioLab.result.sample_count or 0,
                    tireStatefulScenarioLab.result.native_sample_count or 0)
            else
                vehicleMessage = "Tire scenario failed: " .. Vehicle.GetLastError()
            end
        end
    end
    UI.SameLine()
    if UI.Button("EXPORT SCENARIO CSV", buttonWidth, 32.0, false) then
        if nativeVehicle == nil or nativeVehicle == 0
            or not Vehicle.Exists(nativeVehicle) then
            vehicleMessage = "Spawn a native vehicle before exporting a tire scenario."
        else
            local filename = string.format(
                "wheel_%d_%s.csv", tireSteadyStateLab.wheel, scenarioName)
            local exported, message = Vehicle.ExportTireScenarioCsv(
                nativeVehicle, tireSteadyStateLab.wheel,
                scenarioName, filename)
            vehicleMessage = exported
                and ("Tire scenario exported: " .. message)
                or ("Tire scenario export failed: " .. message)
        end
    end

    local result = tireStatefulScenarioLab.result
    if result == nil then
        UI.TextDisabled("Run a scenario to display its time history.")
        return
    end
    UI.TextDisabled(string.format(
        "%s | native dt %.4f s | recorded every %.3f s",
        result.name or "unknown", result.integration_step_s or 0.0,
        result.sample_interval_s or 0.0))
    DrawTireScenarioChannel(result, "Target longitudinal slip", "target_kappa", "ratio")
    DrawTireScenarioChannel(result, "Effective longitudinal slip", "effective_kappa", "ratio")
    DrawTireScenarioChannel(result, "Target slip angle", "target_alpha_deg", "deg")
    DrawTireScenarioChannel(result, "Effective slip angle", "effective_alpha_deg", "deg")
    DrawTireScenarioChannel(result, "Longitudinal force Fx", "fx", "N")
    DrawTireScenarioChannel(result, "Lateral force Fy", "fy", "N")
    DrawTireScenarioChannel(result, "Aligning moment Mz", "mz", "Nm")
    DrawTireScenarioChannel(result, "Tread temperature", "tread_temp_c", "C")
    DrawTireScenarioChannel(result, "Carcass temperature", "carcass_temp_c", "C")
    DrawTireScenarioChannel(result, "Wheel / rim temperature", "rim_temp_c", "C")
    DrawTireScenarioChannel(result, "Contained-air pressure", "pressure_psi", "PSI")
    DrawTireScenarioChannel(result, "Average tread depth", "average_tread_mm", "mm")
    DrawTireScenarioChannel(result, "Minimum tread depth", "minimum_tread_mm", "mm")
    DrawTireScenarioChannel(result, "Flat-spot depth", "flat_spot_mm", "mm")
    DrawTireScenarioChannel(result, "Contained gas", "gas_mass_percent", "%")
    DrawTireScenarioChannel(result, "Carcass integrity", "structural_percent", "%")
    DrawTireScenarioChannel(result, "Tread attachment", "tread_attachment_percent", "%")
    DrawTireScenarioChannel(result, "Rim contact", "rim_contact_percent", "%")
end

local function ApplyTireDevelopmentControls(wearSpeed, rubberGeneration, marbleMaturity)
    if Physics.SetTireDevelopmentControls(wearSpeed, rubberGeneration, marbleMaturity) then
        vehicleMessage = string.format(
            "Tire lab: wear %.0fx | rubber %.0fx | maturity %.0fx",
            wearSpeed, rubberGeneration, marbleMaturity)
        return true
    end
    vehicleMessage = "Tire lab controls were rejected by native physics."
    return false
end

local function RunTireFleetPerformanceLab(wetWeather)
    if nativeVehicle == nil or nativeVehicle == 0
        or not Vehicle.Exists(nativeVehicle) then
        vehicleMessage = "Spawn a native vehicle before running the fleet tire workload."
        return
    end
    tireFleetPerformanceLab.result = Vehicle.RunTireFleetBenchmark(
        nativeVehicle, 1, tireFleetPerformanceLab.vehicles,
        tireFleetPerformanceLab.durationSeconds, wetWeather)
    local result = tireFleetPerformanceLab.result
    if result == nil then
        vehicleMessage = "Fleet tire benchmark failed: " .. Vehicle.GetLastError()
        return
    end
    vehicleMessage = string.format(
        "%d-car %s tire workload: %.2fx real time (%.1f ms for %.2f simulated s)",
        result.vehicles or 0,
        result.wet_weather and "wet" or "dry",
        result.real_time_factor or 0.0,
        result.wall_clock_ms or 0.0,
        result.simulated_seconds or 0.0)
end

local function DrawTireFleetPerformanceLab()
    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("EXECUTABLE 150-CAR / 600-TIRE WORKLOAD")
    UI.TextWrapped("Runs the fitted front-left tire at 1000 Hz for every simulated tire, with thermal, wear and wet-state work at 100 Hz. Wet mode also executes a synthetic collision-baked hydrology road, 30 Hz runoff and every tire-clearing contact. The player's four tires use the bounded 3x3 distributed patch.")
    local changed = false
    tireFleetPerformanceLab.vehicles, changed = UI.InputInt(
        "Vehicles", tireFleetPerformanceLab.vehicles, 1)
    tireFleetPerformanceLab.vehicles = math.max(
        1, math.min(1000, tireFleetPerformanceLab.vehicles))
    tireFleetPerformanceLab.durationSeconds, changed = UI.SliderFloat(
        "Simulated duration", tireFleetPerformanceLab.durationSeconds,
        0.05, 2.0, "%.2f s")
    local buttonWidth = math.max(120.0, (UI.GetAvailableWidth() - 8.0) * 0.5)
    if UI.Button("RUN DRY FLEET", buttonWidth, 32.0, false) then
        RunTireFleetPerformanceLab(false)
    end
    UI.SameLine()
    if UI.Button("RUN CURRENT WET WEATHER", buttonWidth, 32.0, false) then
        RunTireFleetPerformanceLab(true)
    end
    local result = tireFleetPerformanceLab.result
    if result ~= nil then
        UI.Text(string.format(
            "%d vehicles | %d tires | %.0f whole-tire evaluations",
            result.vehicles or 0, result.tires or 0,
            result.whole_tire_evaluations or 0.0))
        UI.Text(string.format(
            "Wall %.2f ms | simulated %.3f s | %.2fx real time",
            result.wall_clock_ms or 0.0,
            result.simulated_seconds or 0.0,
            result.real_time_factor or 0.0))
        UI.Text(string.format(
            "%.0f tire eval/s | %.3f us per vehicle per 1 ms step",
            result.tire_evaluations_per_second or 0.0,
            result.microseconds_per_vehicle_step or 0.0))
        UI.Text(string.format(
            "Thermal / wear / wet updates: %.0f / %.0f / %.0f | brush cells %.0f",
            result.thermal_updates or 0.0,
            result.wear_updates or 0.0,
            result.wet_updates or 0.0,
            result.distributed_brush_cells or 0.0))
        if (result.wet_weather) then
            UI.Text(string.format(
                "Hydrology: %.0f cells | %.0f flow steps | %.0f tire-water contacts",
                result.hydrology_cells or 0.0,
                result.hydrology_steps or 0.0,
                result.hydrology_tire_contacts or 0.0))
        end
    end
    UI.TextDisabled("Tire-only, single-threaded diagnostic. It does not include chassis collision, AI, graphics, audio or networking; a full 150-car scene profile is still required.")
end

local function TireLabTwoColumnWidth()
    return math.max(120.0, (UI.GetAvailableWidth() - 8.0) * 0.5)
end

local function TriggerFrontLeftFailure(stage, successMessage)
    if Vehicle.TriggerWheelTireFailure(nativeVehicle, 1, stage) then
        vehicleMessage = successMessage
        return true
    end
    vehicleMessage = "Tire failure rejected: " .. Vehicle.GetLastError()
    return false
end

function DrawVehicleTiresLabPanel()
    local controls = Physics.GetTireDevelopmentControls()
    if controls == nil then
        UI.TextDisabled("Native tire development controls are unavailable.")
        return
    end

    UI.TextDisabled("DEVELOPMENT / VISUAL TEST CONTROLS")
    UI.TextDisabled("1x is physically timed. Higher values are deliberately artificial and are never authored into the tire.")
    UI.Spacing()

    local wear = controls.wear_speed or 1.0
    local rubber = controls.rubber_generation or 1.0
    local maturity = controls.marble_maturity or 1.0
    local changed = false

    wear, changed = UI.SliderFloat(
        "Tire wear speed", wear, 0.0, 1000.0, "%.0fx")
    if changed then ApplyTireDevelopmentControls(wear, rubber, maturity) end

    rubber, changed = UI.SliderFloat(
        "Rubber / marble generation", rubber, 0.0, 1000.0, "%.0fx")
    if changed then ApplyTireDevelopmentControls(wear, rubber, maturity) end

    maturity, changed = UI.SliderFloat(
        "Fresh shred -> mature marble", maturity, 0.0, 1000.0, "%.0fx")
    if changed then ApplyTireDevelopmentControls(wear, rubber, maturity) end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("INFLATION / DEFLATION TEST")
    if nativeVehicle ~= nil then
        local minimumPressurePa, maximumPressurePa, coldPressurePa =
            Vehicle.GetTireColdInflationPressureRange(nativeVehicle)
        if minimumPressurePa ~= nil and maximumPressurePa ~= nil and coldPressurePa ~= nil then
            local pascalsPerPsi = 6894.757293168
            local pressurePsi = coldPressurePa / pascalsPerPsi
            local minimumPsi = minimumPressurePa / pascalsPerPsi
            local maximumPsi = maximumPressurePa / pascalsPerPsi
            pressurePsi, changed = UI.SliderFloat(
                "Cold tire pressure", pressurePsi, minimumPsi, maximumPsi, "%.1f PSI")
            if changed then
                if Vehicle.SetTireColdInflationPressure(
                    nativeVehicle, pressurePsi * pascalsPerPsi) then
                    vehicleMessage = string.format(
                        "Cold tire pressure %.1f PSI (%.1f kPa)",
                        pressurePsi, pressurePsi * pascalsPerPsi / 1000.0)
                else
                    vehicleMessage = "Cold tire pressure change rejected by the live test range."
                end
            end
            UI.TextDisabled(string.format(
                "Visual/physics range %.0f-%.0f PSI. 150 PSI is the authored mesh shape endpoint.",
                minimumPsi, maximumPsi))
            UI.TextDisabled(
                "The vehicle still spawns at its authored road pressure; live pressure also changes with gas temperature.")
        else
            UI.TextDisabled("No common inflation-pressure range is available for the fitted tires.")
        end
    else
        UI.TextDisabled("Spawn a vehicle to adjust fitted tire pressure.")
    end

    DrawTireCarcassMegaLab()

    DrawTireSteadyStateCalibrationLab()
    DrawTireStatefulScenarioLab()
    DrawTireFleetPerformanceLab()

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("PERSISTENT FAILURE TEST | FRONT-LEFT")
    UI.TextDisabled("Leak rate follows absolute pressure, gas temperature and an effective physical opening. Load/slip can flex a nail-sealed puncture open.")
    if nativeVehicle ~= nil then
        local failureButtonWidth = TireLabTwoColumnWidth()
        if UI.Button("SLOW PUNCTURE", failureButtonWidth, 32.0, false) then
            TriggerFrontLeftFailure(
                1, "Front-left: embedded-object slow puncture started.")
        end
        UI.SameLine()
        if UI.Button("RAPID LOSS", failureButtonWidth, 32.0, false) then
            TriggerFrontLeftFailure(
                2, "Front-left: rapid pressure loss started.")
        end

        if UI.Button("BLOWOUT", failureButtonWidth, 32.0, false) then
            TriggerFrontLeftFailure(3, "Front-left: blowout triggered.")
        end
        UI.SameLine()
        if UI.Button("COLLAPSE CARCASS", failureButtonWidth, 32.0, false) then
            TriggerFrontLeftFailure(5, "Front-left: carcass collapsed.")
        end
        local failedWheel = vehicleWheelTelemetry ~= nil
            and vehicleWheelTelemetry[1] or nil
        if failedWheel ~= nil then
            UI.Text(string.format(
                "ACTIVE: %s | %.1f PSI | leak %.4f g/s",
                failedWheel.tireFailureStage or "Healthy",
                (failedWheel.tireInflationPressurePa or 0.0) / 6894.757293168,
                failedWheel.tireLeakMassFlowGramsPerSecond or 0.0))
            UI.Text(string.format(
                "Pressurized gas %.2f%% | tread attached %.1f%% | carcass %.1f%%",
                (failedWheel.tirePressurizedGasFraction or 1.0) * 100.0,
                (failedWheel.tireTreadAttachment or 1.0) * 100.0,
                (failedWheel.tireStructuralIntegrity or 1.0) * 100.0))
        end
        UI.TextDisabled(
            "Slow puncture is physically timed: the live PSI and leak readout proves it before deflation becomes obvious.")
    else
        UI.TextDisabled("Spawn a vehicle to trigger a fitted-tire failure.")
    end

    UI.Spacing()
    local developmentButtonWidth = TireLabTwoColumnWidth()
    if UI.Button("NORMAL 1X", developmentButtonWidth, 32.0, false) then
        ApplyTireDevelopmentControls(1.0, 1.0, 1.0)
    end
    UI.SameLine()
    if UI.Button("100X TEST", developmentButtonWidth, 32.0, false) then
        ApplyTireDevelopmentControls(100.0, 100.0, 100.0)
    end
    if UI.Button("1000X MARBLE TIMELAPSE", developmentButtonWidth, 32.0, false) then
        ApplyTireDevelopmentControls(1000.0, 1000.0, 1000.0)
    end

    UI.Spacing()
    local resetButtonWidth = TireLabTwoColumnWidth()
    if UI.Button("RESET TIRE PHYSICAL STATE", resetButtonWidth, 32.0, false) then
        if nativeVehicle ~= nil and Vehicle.ResetTirePhysicalState(nativeVehicle) then
            vehicleMessage = "Tire thermal / failure / wear / contamination state repaired and reset."
        else
            vehicleMessage = "Could not reset tire physical state."
        end
    end
    UI.SameLine()
    if UI.Button("RESET TRACK RUBBER + MARBLES", resetButtonWidth, 32.0, false) then
        if Physics.ResetTrackRubber() then
            vehicleMessage = "Track rubber and marble state reset."
        else
            vehicleMessage = "Could not reset track rubber state."
        end
    end

    local stats = Physics.GetSurfacePresentation()
    if stats ~= nil then
        UI.Spacing()
        UI.Text(string.format(
            "Rubber cells: %.0f | chunks: %.0f | contacts: %.0f",
            stats.active_rubber_cells or 0.0,
            stats.resident_rubber_chunks or 0.0,
            stats.rubber_contact_samples or 0.0))
        UI.Text(string.format(
            "Generated bonded / loose: %.4f / %.4f",
            stats.rubber_deposited_generation or 0.0,
            stats.rubber_loose_generation or 0.0))
        UI.Text(string.format(
            "Persistent logical rubber pieces: %.0f / 500000",
            stats.persistent_rubber_pieces or 0.0))
    end

    UI.Spacing()
    UI.TextDisabled("Fresh rubber starts as shreds/flakes. Repeated tire traffic, agitation, tack and concentration mature it into rounder marbles.")
    UI.TextDisabled("Cars crossing an off-line marble band can sweep and redistribute it; rain and tire pickup can remove it.")
end
