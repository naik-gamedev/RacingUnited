-- TIRE15C2 development-only tire/rubber acceleration controls.
-- These multipliers are runtime lab state only; they are not saved into the
-- vehicle definition, tire part or GLB metadata.

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
