-- STUDIO15 full Heritage Vehicle representation for FULL-tier traffic agents.
--
-- This remains opt-in through HROAD v6.  Traffic intelligence stays in
-- TrafficAgents.lua; this file only translates an agent's desired path/speed
-- into a real dynamic chassis, suspension, tires and steering/brake inputs.
RacingTrafficVehicleFactory = RacingTrafficVehicleFactory or {}

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function normalizeAngleDegrees(value)
    local angle = value or 0.0
    while angle > 180.0 do angle = angle - 360.0 end
    while angle < -180.0 do angle = angle + 360.0 end
    return angle
end

local function vehicleMass(profile)
    local className = profile.class or "Sedan"
    if className == "Truck" then return 5200.0 end
    if className == "Van" then return 2200.0 end
    if className == "Motorcycle" then return 260.0 end
    if className == "Sport" then return 1450.0 end
    if className == "Emergency" then return 1900.0 end
    return 1550.0
end

function RacingTrafficVehicleFactory.Spawn(agent)
    if Entity == nil or Physics == nil or Vehicle == nil then return nil end
    local profile = agent.profile or {}
    local length = math.max(2.4, profile.lengthM or 4.4)
    local width = math.max(1.2, profile.widthM or 1.82)
    local height = (profile.class == "Truck" or profile.class == "Van") and 2.3 or 1.45
    local mass = vehicleMass(profile)

    local entity = Entity.Create("Traffic Vehicle " .. tostring(agent.id) .. " - " .. tostring(profile.name or "Driver"))
    if entity == 0 then return nil end
    Entity.AddTag(entity, "TrafficAgent")
    Entity.AddTag(entity, "TrafficVehicle")
    Entity.SetLocalScale(entity, width, height, length)
    Entity.SetDebugPrimitive(entity, "box", 0.18, 0.52, 0.92)

    local body = Physics.CreateBody(entity, "dynamic", mass)
    if body == 0 then Entity.Destroy(entity); return nil end
    Physics.SetBodyGravityFactor(body, 1.0)
    Physics.SetBodyLinearDamping(body, 0.04)
    Physics.SetBodyAngularDamping(body, 0.14)
    Physics.SetBodyAllowSleep(body, true)
    local collider = Physics.CreateBoxCollider(
        body, width * 0.46, height * 0.34, length * 0.43,
        0.0, height * 0.05, 0.0, 0.78, 0.04, false)
    if collider == 0 then Physics.DestroyBody(body); Entity.Destroy(entity); return nil end

    local config = RacingTrafficAgents.GetConfiguration()
    local highRate = math.max(120.0, config.trafficVehicleHighRateHz or 250.0)
    local driveForce = mass * 5.2
    local brakeForce = mass * 8.5
    local vehicle = Vehicle.Create(body, highRate, driveForce, brakeForce, 38.0, 1.02, 9.5, 0.016)
    if vehicle == 0 then Physics.DestroyBody(body); Entity.Destroy(entity); return nil end

    local wheelbase = length * 0.57
    local track = width * 0.78
    local radius = clamp(length * 0.075, 0.28, 0.48)
    local mountY = -height * 0.17
    local frontZ = wheelbase * 0.5
    local rearZ = -wheelbase * 0.5
    local spring = mass * 9.80665 / 0.32
    local bump = mass * 1.25
    local rebound = mass * 1.75
    local function addWheel(x, z, drive, steer, brake, handbrake)
        return Vehicle.AddWheel(
            vehicle, x, mountY, z,
            0.0, -1.0, 0.0,
            radius, 0.22, 0.10, 0.08,
            spring, bump, rebound,
            drive, steer, brake, handbrake)
    end
    local brakePerWheel = 0.25
    local ok = addWheel(-track * 0.5, frontZ, 0.0, 1.0, brakePerWheel, 0.0)
        and addWheel(track * 0.5, frontZ, 0.0, 1.0, brakePerWheel, 0.0)
        and addWheel(-track * 0.5, rearZ, 0.5, 0.0, brakePerWheel, 0.5)
        and addWheel(track * 0.5, rearZ, 0.5, 0.0, brakePerWheel, 0.5)
    if not ok then
        Vehicle.Destroy(vehicle)
        Physics.DestroyBody(body)
        Entity.Destroy(entity)
        return nil
    end
    Vehicle.SetDriverAids(vehicle, true, true, 0.16, 0.12, 2.5, 18.0, 3500.0)
    Vehicle.SetGear(vehicle, 1)
    Physics.SetBodyPosition(body, agent.x or 0.0, (agent.y or 0.0) + height * 0.55, agent.z or 0.0)
    Physics.SetBodyRotation(body, 0.0, agent.headingDeg or 0.0, 0.0)
    return {
        entity = entity,
        body = body,
        collider = collider,
        vehicle = vehicle,
        height = height,
        physical = true
    }
end

function RacingTrafficVehicleFactory.Destroy(handle)
    if handle == nil then return end
    if handle.vehicle and handle.vehicle ~= 0 and Vehicle.Exists(handle.vehicle) then Vehicle.Destroy(handle.vehicle) end
    if handle.body and handle.body ~= 0 and Physics.BodyExists(handle.body) then Physics.DestroyBody(handle.body) end
    if handle.entity and handle.entity ~= 0 and Entity.Exists(handle.entity) then Entity.Destroy(handle.entity) end
end

function RacingTrafficVehicleFactory.Update(handle, agent)
    if handle == nil or handle.vehicle == nil or not Vehicle.Exists(handle.vehicle) then return end
    if handle.body == nil or not Physics.BodyExists(handle.body) then return end
    local px, py, pz = Physics.GetBodyPosition(handle.body)
    local _, yaw = Physics.GetBodyRotation(handle.body)
    local dx = (agent.x or px) - px
    local dz = (agent.z or pz) - pz
    local targetHeading = agent.headingDeg or yaw or 0.0
    if math.abs(dx) + math.abs(dz) > 1.0 then targetHeading = math.deg(math.atan(dx, dz)) end
    local headingError = normalizeAngleDegrees(targetHeading - (yaw or 0.0))
    local steering = clamp(headingError / 28.0, -1.0, 1.0)
    local speed = math.max(0.0, Vehicle.GetSpeed(handle.vehicle) or 0.0)
    local targetSpeed = math.max(0.0, agent.desiredSpeedMps or agent.speedMps or 0.0)
    local speedError = targetSpeed - speed
    local throttle = clamp(speedError / 4.0, 0.0, 1.0)
    local brake = clamp(-speedError / 5.0, 0.0, 1.0)
    if agent.waitReason ~= nil and targetSpeed < 0.5 then throttle = 0.0; brake = math.max(brake, 0.55) end
    Vehicle.SetInputs(handle.vehicle, throttle, brake, steering, 0.0)

    -- Physical traffic follows the logical planner but is allowed to solve its
    -- own suspension/tire/contact state.  Only recover gross divergence, e.g.
    -- after a collision or spawning into invalid geometry.
    local planarError = math.sqrt(dx * dx + dz * dz)
    if planarError > 35.0 then
        Physics.SetBodyPosition(handle.body, agent.x or px, (agent.y or py) + (handle.height or 1.45) * 0.55, agent.z or pz)
        Physics.SetBodyRotation(handle.body, 0.0, agent.headingDeg or yaw or 0.0, 0.0)
        Physics.SetBodyLinearVelocity(handle.body, 0.0, 0.0, 0.0)
        Physics.SetBodyAngularVelocity(handle.body, 0.0, 0.0, 0.0)
    end
end
