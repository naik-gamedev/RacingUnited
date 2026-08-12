-- SUS04: reusable left/right suspension coupling. Anti-roll bars are separate
-- from suspension geometry providers: a front MacPherson pair and a rear
-- trailing-arm pair can both use the same native torsional-bar mechanism.

local function AntiRollBarWheelIndexByName(name)
    for index, wheel in ipairs(PrototypeCarDefinition.wheels or {}) do
        if wheel.name == name then return index end
    end
    return nil
end

local function ApplyAntiRollBar(slotIndex, definition)
    if definition == nil then return true end
    local leftIndex = AntiRollBarWheelIndexByName(definition.leftWheel)
    local rightIndex = AntiRollBarWheelIndexByName(definition.rightWheel)
    if leftIndex == nil or rightIndex == nil then
        vehicleMessage = "VEHICLE ERROR: anti-roll-bar wheel names are invalid"
        return false
    end

    local success = Vehicle.SetAntiRollBar(
        nativeVehicle,
        slotIndex,
        leftIndex,
        rightIndex,
        tonumber(definition.torsionalStiffnessNmPerRad) or 0.0,
        tonumber(definition.torsionalDampingNmsPerRad) or 0.0,
        tonumber(definition.leftLeverArmM) or 0.20,
        tonumber(definition.rightLeverArmM) or 0.20,
        tonumber(definition.leftLinkMotionRatio) or 1.0,
        tonumber(definition.rightLinkMotionRatio) or 1.0,
        tonumber(definition.maximumWheelForceN) or 12000.0,
        definition.enabled ~= false)
    if not success then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end
    return true
end

function ApplyDefinitionAntiRollBars()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    if Vehicle.SetAntiRollBar == nil then
        vehicleMessage = "VEHICLE ERROR: native anti-roll-bar API is unavailable"
        return false
    end

    local bars = PrototypeCarDefinition.antiRollBars or {}
    if not ApplyAntiRollBar(1, bars.front) then return false end
    if not ApplyAntiRollBar(2, bars.rear) then return false end

    vehicleAntiRollBars.front = bars.front
    vehicleAntiRollBars.rear = bars.rear
    vehicleAntiRollBars.nativeCount = Vehicle.GetAntiRollBarCount(nativeVehicle)
    return true
end

function RefreshAntiRollBarTelemetry()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle)
        or Vehicle.GetAntiRollBar == nil then
        return false
    end

    for index, name in ipairs({ "front", "rear" }) do
        local leftWheel, rightWheel, stiffness, damping,
            leftLever, rightLever, leftMotion, rightMotion,
            maximumForce, enabled, twist, twistRate, torque,
            leftForce, rightForce = Vehicle.GetAntiRollBar(nativeVehicle, index)
        if leftWheel ~= nil then
            vehicleAntiRollBars[name] = vehicleAntiRollBars[name] or {}
            local state = vehicleAntiRollBars[name]
            state.leftWheelIndex = leftWheel
            state.rightWheelIndex = rightWheel
            state.torsionalStiffnessNmPerRad = stiffness
            state.torsionalDampingNmsPerRad = damping
            state.leftLeverArmM = leftLever
            state.rightLeverArmM = rightLever
            state.leftLinkMotionRatio = leftMotion
            state.rightLinkMotionRatio = rightMotion
            state.maximumWheelForceN = maximumForce
            state.enabled = enabled
            state.twistRadians = twist
            state.twistRateRadiansPerSecond = twistRate
            state.torqueNm = torque
            state.leftWheelForceN = leftForce
            state.rightWheelForceN = rightForce
        end
    end
    return true
end
