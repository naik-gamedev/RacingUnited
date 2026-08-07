-- Repeatable vehicle-dynamics experiments and native recorder control.
-- The recorder itself runs in C++ at the requested vehicle-substep rate; Lua
-- only selects a scenario and supplies its deterministic driver inputs.
vehicleDynamicsLab = {
    scenario = "none",
    scenarioLabel = "No active scenario",
    elapsedSeconds = 0.0,
    durationSeconds = 12.0,
    captureHertz = math.min(1000.0, vehicleHighRateHertz),
    selectedWheel = 1,
    plotPoints = 180,
    message = "Choose a repeatable test or record a manual drive"
}

local function StartDynamicsLabCapture(label, scenario, durationSeconds)
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        vehicleDynamicsLab.message = "LAB ERROR: no native vehicle is active"
        return false
    end

    local captureHertz = math.min(
        vehicleDynamicsLab.captureHertz,
        Vehicle.GetHighRateHertz(nativeVehicle))
    if not Vehicle.StartDynamicsLab(
        nativeVehicle, durationSeconds, captureHertz) then
        vehicleDynamicsLab.message = "LAB ERROR: " .. Vehicle.GetLastError()
        return false
    end

    vehicleDynamicsLab.scenario = scenario
    vehicleDynamicsLab.scenarioLabel = label
    vehicleDynamicsLab.elapsedSeconds = 0.0
    vehicleDynamicsLab.durationSeconds = durationSeconds
    vehicleDynamicsLab.message = string.format(
        "%s recording at %.0f Hz for %.1f seconds",
        label, captureHertz, durationSeconds)
    return true
end

function StartManualDynamicsLabCapture()
    return StartDynamicsLabCapture(
        "Manual drive", "manual", vehicleDynamicsLab.durationSeconds)
end

function StartParkedSettleLab()
    ResetNativeVehicle()
    return StartDynamicsLabCapture("Parked settle", "parked_settle", 6.0)
end

function StartDropTestLab()
    local reset = PrototypeCarDefinition.resetPosition
    ResetNativeVehicleAt(
        reset[1], reset[2] + 0.25, reset[3],
        "Dynamics lab: 250 mm suspension drop")
    return StartDynamicsLabCapture("250 mm drop", "drop", 8.0)
end

function StartStraightBrakeLab()
    ResetNativeVehicle()
    Vehicle.SetGear(nativeVehicle, 1)
    return StartDynamicsLabCapture(
        "Straight acceleration and braking", "straight_brake", 7.0)
end

function StartTurnBrakeLab()
    ResetNativeVehicle()
    Vehicle.SetGear(nativeVehicle, 1)
    return StartDynamicsLabCapture(
        "Turn then brake", "turn_brake", 8.0)
end

function StopDynamicsLabCapture(message)
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.StopDynamicsLab(nativeVehicle)
    end
    vehicleDynamicsLab.scenario = "none"
    vehicleDynamicsLab.scenarioLabel = "No active scenario"
    vehicleDynamicsLab.message = message or "Stopped; captured samples retained"
end

function ClearDynamicsLabCapture()
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.ClearDynamicsLab(nativeVehicle)
    end
    vehicleDynamicsLab.scenario = "none"
    vehicleDynamicsLab.scenarioLabel = "No active scenario"
    vehicleDynamicsLab.elapsedSeconds = 0.0
    vehicleDynamicsLab.message = "Cleared the captured run"
end

function ExportDynamicsLabCapture()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        vehicleDynamicsLab.message = "LAB ERROR: no native vehicle is active"
        return
    end
    local worked, pathOrError = Vehicle.ExportDynamicsLabCsv(
        nativeVehicle, "vehicle_dynamics_latest.csv")
    vehicleDynamicsLab.message = worked
        and ("Exported CSV: " .. pathOrError)
        or ("LAB ERROR: " .. pathOrError)
end

-- Returns true plus a complete input set while an automatic experiment owns
-- the car. Manual recording deliberately leaves ordinary player input alone.
function VehicleDynamicsLabFixedInputs(fixedDeltaTime)
    local scenario = vehicleDynamicsLab.scenario
    if scenario == "none" or scenario == "manual" then
        return false, 0.0, 0.0, 0.0, 0.0
    end

    vehicleDynamicsLab.elapsedSeconds =
        vehicleDynamicsLab.elapsedSeconds + fixedDeltaTime
    local time = vehicleDynamicsLab.elapsedSeconds
    if time >= vehicleDynamicsLab.durationSeconds then
        StopDynamicsLabCapture("Automatic experiment complete; run retained")
        return true, 0.0, 0.0, 0.0, 0.0
    end

    if scenario == "parked_settle" or scenario == "drop" then
        return true, 0.0, 0.0, 0.0, 0.0
    end

    if scenario == "straight_brake" then
        if time < 3.0 then
            return true, 0.78, 0.0, 0.0, 0.0
        end
        if time < 6.25 then
            return true, 0.0, 0.92, 0.0, 0.0
        end
        return true, 0.0, 0.25, 0.0, 0.0
    end

    if scenario == "turn_brake" then
        if time < 2.5 then
            return true, 0.72, 0.0, 0.0, 0.0
        end
        if time < 4.0 then
            return true, 0.48, 0.0, 0.42, 0.0
        end
        if time < 6.5 then
            local steering = 0.42 * math.max(0.0, 1.0 - (time - 4.0) / 2.5)
            return true, 0.0, 0.90, steering, 0.0
        end
        return true, 0.0, 0.30, 0.0, 0.0
    end

    return false, 0.0, 0.0, 0.0, 0.0
end
