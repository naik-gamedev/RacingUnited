-- STUDIO16 advanced physical traffic-agent solver.
--
-- The agent layer intentionally consumes the HROAD semantic graph and the
-- STUDIO13 RacingTraffic operations facade instead of duplicating routing,
-- restrictions, traffic-light, streaming or reservation policy.
--
-- A pluggable vehicle factory keeps behavior independent from representation:
-- the default implementation creates kinematic debug/collision proxies, while
-- a later full traffic-vehicle factory can bind the exact same agents to
-- complete Heritage Vehicle instances without rewriting AI decisions.

RacingTrafficAgents = RacingTrafficAgents or {}

local function clamp(value, minimum, maximum)
    if value < minimum then return minimum end
    if value > maximum then return maximum end
    return value
end

local function lerp(a, b, t)
    return a + (b - a) * t
end

local function distance3(a, b)
    if a == nil or b == nil then return math.huge end
    local dx = (b.x or 0.0) - (a.x or 0.0)
    local dy = (b.y or 0.0) - (a.y or 0.0)
    local dz = (b.z or 0.0) - (a.z or 0.0)
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end

local function horizontalDistance(a, b)
    if a == nil or b == nil then return math.huge end
    local dx = (b.x or 0.0) - (a.x or 0.0)
    local dz = (b.z or 0.0) - (a.z or 0.0)
    return math.sqrt(dx * dx + dz * dz)
end

local function nodeById(id)
    return RacingGameplay.GetTrafficNode(id)
end

local function linkById(id)
    for _, link in ipairs(RacingGameplay.data.trafficLinks or {}) do
        if link.id == id then return link end
    end
    return nil
end

local function splitConnectorIds(value)
    local ids = {}
    local source = tostring(value or "")
    for token in string.gmatch(source, "%d+") do
        ids[tonumber(token)] = true
    end
    return ids
end

local function connectorForLink(link)
    if link == nil or link.type ~= "Junction Turn" then return nil end
    local fromNode = nodeById(link.fromNodeId)
    local toNode = nodeById(link.toNodeId)
    if fromNode == nil or toNode == nil then return nil end
    for _, connector in ipairs(RacingGameplay.data.turnConnectors or {}) do
        if connector.enabled
            and connector.fromRoadId == fromNode.roadId
            and connector.toRoadId == toNode.roadId
            and ((connector.fromLane or 0) == 0 or connector.fromLane == fromNode.laneIndex)
            and ((connector.toLane or 0) == 0 or connector.toLane == toNode.laneIndex) then
            return connector
        end
    end
    return nil
end

local function connectorHasGreen(state, connector)
    if state == nil or connector == nil then return true end
    local signal = state.signals and state.signals[connector.intersectionId] or nil
    if signal == nil then return true end
    if signal.stage ~= "green" then return false end
    local phase = signal.phase or {}
    local allowed = splitConnectorIds(phase.connectorIds)
    if next(allowed) == nil then return true end
    return allowed[connector.id] == true
end

local function defaultProfile()
    return {
        id = 0,
        name = "Default Traffic Driver",
        class = "Sedan",
        enabled = true,
        vehiclePreset = "PrototypeCar",
        spawnWeight = 1.0,
        lengthM = 4.4,
        widthM = 1.82,
        maxSpeedFactor = 1.0,
        accelerationFactor = 1.0,
        brakingFactor = 1.0,
        desiredTimeGapS = 1.6,
        minimumGapM = 2.5,
        reactionTimeS = 0.45,
        laneChangeAggression = 0.5,
        courtesy = 0.6,
        speedCompliance = 0.92,
        illegalOvertakeChance = 0.0,
        parkingSkill = 0.8
    }
end

function RacingTrafficAgents.GetConfiguration()
    return RacingGameplay.GetTrafficAgentSimulationConfiguration()
end

function RacingTrafficAgents.GetBehaviorConfiguration()
    return RacingGameplay.GetTrafficBehaviorConfiguration and RacingGameplay.GetTrafficBehaviorConfiguration() or {}
end

function RacingTrafficAgents.GetProfiles(vehicleClass)
    return RacingGameplay.GetEnabledTrafficAgentProfiles(vehicleClass)
end

function RacingTrafficAgents.ChooseProfile(vehicleClass)
    local profiles = RacingTrafficAgents.GetProfiles(vehicleClass)
    if #profiles == 0 then profiles = RacingTrafficAgents.GetProfiles(nil) end
    if #profiles == 0 then return defaultProfile() end
    local total = 0.0
    for _, profile in ipairs(profiles) do
        total = total + math.max(0.0, profile.spawnWeight or 0.0)
    end
    if total <= 0.0 then return profiles[1] end
    local pick = math.random() * total
    local running = 0.0
    for _, profile in ipairs(profiles) do
        running = running + math.max(0.0, profile.spawnWeight or 0.0)
        if pick <= running then return profile end
    end
    return profiles[#profiles]
end

local function createDefaultProxy(agent)
    local config = RacingTrafficAgents.GetConfiguration()
    if config.createDebugProxyVehicles == false then return nil end
    if Entity == nil or Physics == nil then return nil end
    local entity = Entity.Create("Traffic Agent " .. tostring(agent.id) .. " - " .. tostring(agent.profile.name or "Driver"))
    if entity == 0 then return nil end
    Entity.AddTag(entity, "TrafficAgent")
    local length = math.max(0.8, agent.profile.lengthM or 4.4)
    local width = math.max(0.5, agent.profile.widthM or 1.82)
    Entity.SetLocalScale(entity, width, 1.45, length)
    if agent.profile.class == "Emergency" then
        Entity.SetDebugPrimitive(entity, "box", 0.85, 0.18, 0.12)
    elseif agent.profile.class == "Truck" or agent.profile.class == "Van" then
        Entity.SetDebugPrimitive(entity, "box", 0.82, 0.62, 0.18)
    elseif agent.profile.class == "Sport" then
        Entity.SetDebugPrimitive(entity, "box", 0.20, 0.48, 0.88)
    else
        Entity.SetDebugPrimitive(entity, "box", 0.28, 0.62, 0.82)
    end
    local body = Physics.CreateBody(entity, "kinematic", math.max(150.0, length * width * 120.0))
    local collider = 0
    if body ~= 0 then
        collider = Physics.CreateBoxCollider(body, width * 0.5, 0.725, length * 0.5, 0.0, 0.0, 0.0, 0.78, 0.05, false)
        Physics.SetBodyAllowSleep(body, false)
    end
    return { entity = entity, body = body, collider = collider }
end

local function destroyDefaultProxy(handle)
    if handle == nil then return end
    if handle.body and handle.body ~= 0 and Physics.BodyExists(handle.body) then
        Physics.DestroyBody(handle.body)
    end
    if handle.entity and handle.entity ~= 0 and Entity.Exists(handle.entity) then
        Entity.Destroy(handle.entity)
    end
end

local function updateDefaultProxy(handle, agent)
    if handle == nil then return end
    local x, y, z = agent.x or 0.0, agent.y or 0.0, agent.z or 0.0
    if handle.body and handle.body ~= 0 and Physics.BodyExists(handle.body) then
        Physics.SetBodyPosition(handle.body, x, y + 0.75, z)
        Physics.SetBodyRotation(handle.body, 0.0, agent.headingDeg or 0.0, 0.0)
        local radians = math.rad(agent.headingDeg or 0.0)
        local speed = agent.speedMps or 0.0
        Physics.SetBodyLinearVelocity(handle.body, math.sin(radians) * speed, 0.0, math.cos(radians) * speed)
        Physics.SetBodyAngularVelocity(handle.body, 0.0, 0.0, 0.0)
    elseif handle.entity and handle.entity ~= 0 and Entity.Exists(handle.entity) then
        Entity.SetLocalPosition(handle.entity, x, y + 0.75, z)
        Entity.SetLocalRotation(handle.entity, 0.0, agent.headingDeg or 0.0, 0.0)
    end
end

local defaultVehicleFactory = {
    Spawn = createDefaultProxy,
    Destroy = destroyDefaultProxy,
    Update = updateDefaultProxy
}

function RacingTrafficAgents.SetVehicleFactory(factory)
    RacingTrafficAgents.vehicleFactory = factory or defaultVehicleFactory
end

local function currentPlayerPosition()
    if nativeVehicleBody ~= nil and nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody) then
        local x, y, z = Physics.GetBodyPosition(nativeVehicleBody)
        return { x = x, y = y, z = z }
    end
    if playerEntity ~= nil and playerEntity ~= 0 and Entity.Exists(playerEntity) then
        local x, y, z = Entity.GetWorldPosition(playerEntity)
        return { x = x, y = y, z = z }
    end
    return nil
end

local function eligibleLaneNodes()
    local result = {}
    for _, node in ipairs(RacingGameplay.data.trafficNodes or {}) do
        if node.type == "Lane Node" and node.generated ~= false and #(RacingGameplay.GetOutgoingRoadLinks(node.id)) > 0 then
            result[#result + 1] = node
        end
    end
    return result
end

local function destinationNodes()
    local explicit = {}
    for _, node in ipairs(RacingGameplay.data.trafficNodes or {}) do
        if node.type == "Destination" then explicit[#explicit + 1] = node end
    end
    if #explicit > 0 then return explicit end
    return eligibleLaneNodes()
end

local function trim(value)
    return tostring(value or ""):match("^%s*(.-)%s*$")
end

local function portalAllowsProfile(portal, profile)
    local allowed = trim(portal and portal.allowedClasses or "")
    if allowed == "" then return true end
    local wanted = string.lower(trim(profile and profile.class or ""))
    for token in string.gmatch(allowed, "[^,;|]+") do
        if string.lower(trim(token)) == wanted then return true end
    end
    return false
end

local function countPortalAgents(manager, portalId)
    local count = 0
    for _, agent in pairs(manager.agents or {}) do
        if agent.sourcePortalId == portalId then count = count + 1 end
    end
    return count
end

local function nodeOccupiedForSpawn(manager, node, minimumGap)
    for _, agent in pairs(manager.agents or {}) do
        if horizontalDistance(agent, node) < minimumGap then return true end
    end
    return false
end

local function weightedPick(items, weightFunction)
    local total = 0.0
    for _, item in ipairs(items) do total = total + math.max(0.0, weightFunction(item)) end
    if total <= 0.0 then return items[1] end
    local pick = math.random() * total
    local running = 0.0
    for _, item in ipairs(items) do
        running = running + math.max(0.0, weightFunction(item))
        if pick <= running then return item end
    end
    return items[#items]
end

local function chooseStartPortal(manager, playerPosition, profile)
    local config = RacingTrafficAgents.GetConfiguration()
    local valid = {}
    for _, portal in ipairs(RacingGameplay.GetActiveTrafficSpawnPortals(manager.hour, "Spawn Only")) do
        local node = nodeById(portal.nodeId)
        if node ~= nil and #(RacingGameplay.GetOutgoingRoadLinks(node.id)) > 0 and portalAllowsProfile(portal, profile)
            and (profile.class ~= "Emergency" or portal.emergencyAllowed ~= false) then
            local distance = playerPosition and horizontalDistance(playerPosition, portal) or 0.0
            local minimumDistance = math.max(config.spawnMinDistancePlayerM or 100.0, portal.minimumPlayerDistanceM or 0.0)
            local maximumDistance = math.min(config.spawnMaxDistancePlayerM or 650.0, portal.maximumPlayerDistanceM or math.huge)
            local belowPortalCap = countPortalAgents(manager, portal.id) < math.max(1, portal.maxConcurrentAgents or 24)
            if belowPortalCap
                and (playerPosition == nil or (distance >= minimumDistance and distance <= maximumDistance))
                and not nodeOccupiedForSpawn(manager, node, config.minimumSpawnGapM or 25.0) then
                valid[#valid + 1] = { portal = portal, node = node }
            end
        end
    end
    if #valid == 0 then return nil, nil end
    local picked = weightedPick(valid, function(candidate)
        local density = RacingGameplay.GetTrafficDensityAt(candidate.portal.x or candidate.node.x or 0.0, candidate.portal.y or candidate.node.y or 0.0, candidate.portal.z or candidate.node.z or 0.0, manager.hour)
        return math.max(0.0, candidate.portal.spawnWeight or 1.0) * math.max(0.0, density.densityMultiplier or 1.0)
    end)
    return picked.node, picked.portal
end

local function chooseStartNode(manager, playerPosition, profile)
    local portalNode, portal = chooseStartPortal(manager, playerPosition, profile)
    if portalNode ~= nil then return portalNode, portal end

    -- Backward-compatible fallback for HROAD worlds without authored portals.
    local config = RacingTrafficAgents.GetConfiguration()
    local candidates = eligibleLaneNodes()
    local valid = {}
    for _, node in ipairs(candidates) do
        local distance = playerPosition and horizontalDistance(playerPosition, node) or 0.0
        if playerPosition == nil or (distance >= (config.spawnMinDistancePlayerM or 100.0) and distance <= (config.spawnMaxDistancePlayerM or 650.0)) then
            if not nodeOccupiedForSpawn(manager, node, config.minimumSpawnGapM or 25.0) then valid[#valid + 1] = node end
        end
    end
    if #valid == 0 then return nil, nil end
    return valid[math.random(1, #valid)], nil
end

local function chooseDestination(startNodeId, manager, vehicle)
    local routeOptions = {
        hour = manager and manager.hour or 12.0,
        runtimeState = manager and manager.operations or nil,
        vehicle = vehicle
    }
    local portals = RacingGameplay.GetActiveTrafficSpawnPortals(routeOptions.hour, "Despawn Only")
    if #portals > 0 then
        for _ = 1, math.min(20, #portals * 3) do
            local portal = portals[math.random(1, #portals)]
            local candidate = nodeById(portal.nodeId)
            if candidate ~= nil and candidate.id ~= startNodeId then
                local nodes, links = RacingTraffic.FindRoute(startNodeId, candidate.id, routeOptions)
                if nodes ~= nil and #nodes >= 2 then return candidate, nodes, links, portal end
            end
        end
    end

    local candidates = destinationNodes()
    if #candidates == 0 then return nil end
    for _ = 1, math.min(16, #candidates * 2) do
        local candidate = candidates[math.random(1, #candidates)]
        if candidate.id ~= startNodeId then
            local nodes, links = RacingTraffic.FindRoute(startNodeId, candidate.id, routeOptions)
            if nodes ~= nil and #nodes >= 2 then return candidate, nodes, links, nil end
        end
    end
    return nil
end

local function setRoute(agent, nodes, links)
    agent.routeNodes = nodes or {}
    agent.routeLinks = links or {}
    agent.routeIndex = 1
    agent.segmentProgressM = 0.0
    agent.currentNodeId = agent.routeNodes[1] or agent.currentNodeId
    agent.goalNodeId = agent.routeNodes[#agent.routeNodes] or agent.goalNodeId
end

function RacingTrafficAgents.CreateAgent(profile, startNodeId, goalNodeId, options)
    options = options or {}
    profile = profile or RacingTrafficAgents.ChooseProfile(nil)
    local nodes, links = RacingTraffic.FindRoute(startNodeId, goalNodeId, options.routeOptions or {})
    if nodes == nil or #nodes < 2 then return nil, "no route" end
    local start = nodeById(nodes[1])
    if start == nil then return nil, "missing start node" end
    local agent = {
        id = 0,
        profile = profile,
        currentNodeId = nodes[1],
        goalNodeId = nodes[#nodes],
        routeNodes = nodes,
        routeLinks = links or {},
        routeIndex = 1,
        segmentProgressM = 0.0,
        x = start.x or 0.0,
        y = start.y or 0.0,
        z = start.z or 0.0,
        headingDeg = start.headingDeg or 0.0,
        speedMps = options.initialSpeedMps or 0.0,
        accelerationMps2 = 0.0,
        desiredSpeedMps = 0.0,
        laneChangeCooldownS = 0.0,
        reactionTimerS = 0.0,
        stuckTimerS = 0.0,
        waitReason = nil,
        streamingTier = "full",
        proxy = nil,
        telemetry = {},
        emergency = profile.class == "Emergency",
        parked = false,
        parkingTarget = nil,
        sourcePortalId = options.sourcePortalId,
        destinationPortalId = options.destinationPortalId,
        readyToDespawn = false,
        incidentInfluenceId = nil,
        incidentInfluence = 0.0,
        environmentSpeedFactor = 1.0,
        environmentFollowingGapFactor = 1.0,
        environmentBrakingFactor = 1.0,
        densityRegionCount = 0,
        decision = "follow route",
        laneChangeScore = 0.0,
        laneChangeReason = nil,
        stopDwellRemainingS = 0.0,
        stopDwellConnectorId = nil,
        queueReleaseTimerS = 0.0,
        mergeWait = false,
        mergeTargetNodeId = nil,
        recoveryStage = "none",
        recoveryLastAttemptS = -math.huge,
        recoveryReverseRemainingS = 0.0,
        parkingManeuver = nil,
        parkingManeuverRemainingS = 0.0,
        collisionCooldownS = 0.0,
        incidentResponseId = nil,
        incidentResponseTimerS = 0.0
    }
    return agent
end

local function currentSegment(agent)
    local fromId = agent.routeNodes[agent.routeIndex]
    local toId = agent.routeNodes[agent.routeIndex + 1]
    if fromId == nil or toId == nil then return nil end
    local from = nodeById(fromId)
    local to = nodeById(toId)
    local link = linkById(agent.routeLinks[agent.routeIndex])
    if from == nil or to == nil or link == nil then return nil end
    local length = math.max(0.05, distance3(from, to))
    return from, to, link, length
end

local function getEnvironmentModifiers(manager, agent)
    local cfg = RacingGameplay.GetTrafficEnvironmentConfiguration()
    local speedFactor = 1.0
    local followingGapFactor = 1.0
    local brakingFactor = 1.0
    local rain = 0.0
    local cloud = 0.0
    local wetness = 0.0
    if Physics ~= nil and Physics.GetSurfaceWeather ~= nil then
        local weather = Physics.GetSurfaceWeather()
        if weather ~= nil then
            rain = math.max(0.0, weather.rain_mm_per_hour or 0.0)
            cloud = clamp(weather.cloud_cover or 0.0, 0.0, 1.0)
        end
    end
    local surfaceTemperature = 20.0
    if Physics ~= nil and Physics.GetSurfaceEnvironment ~= nil then
        local surfaceWetness, _, surfaceTemp = Physics.GetSurfaceEnvironment()
        if type(surfaceWetness) == "number" then wetness = clamp(surfaceWetness, 0.0, 1.0) end
        if type(surfaceTemp) == "number" then surfaceTemperature = surfaceTemp end
    elseif rain > 0.0 then
        wetness = clamp(rain / 20.0, 0.0, 1.0)
    end

    if wetness > 0.01 or rain > 0.05 then
        local wetInfluence = math.max(wetness, clamp(rain / 8.0, 0.0, 1.0))
        speedFactor = speedFactor * lerp(1.0, cfg.wetSpeedFactor or 0.88, wetInfluence)
        followingGapFactor = followingGapFactor * lerp(1.0, cfg.wetFollowingGapFactor or 1.18, wetInfluence)
        brakingFactor = brakingFactor * lerp(1.0, cfg.wetBrakingFactor or 0.78, wetInfluence)
        if rain >= 10.0 then
            local heavy = clamp((rain - 10.0) / 40.0, 0.0, 1.0)
            speedFactor = speedFactor * lerp(1.0, cfg.heavyRainSpeedFactor or 0.72, heavy)
        end
        if surfaceTemperature <= 0.0 and wetness > 0.05 then
            local iceInfluence = clamp((-surfaceTemperature + wetness * 2.0) / 6.0, 0.0, 1.0)
            speedFactor = speedFactor * lerp(1.0, cfg.iceSpeedFactor or 0.38, iceInfluence)
        end
    end

    local hour = manager and manager.hour or 12.0
    if hour < 6.0 or hour >= 20.0 then speedFactor = speedFactor * (cfg.nightSpeedFactor or 0.92) end
    local poorVisibility = clamp(math.max(cloud * 0.35, rain / 80.0), 0.0, 1.0)
    speedFactor = speedFactor * lerp(1.0, cfg.poorVisibilitySpeedFactor or 0.72, poorVisibility)

    local density = RacingGameplay.GetTrafficDensityAt(agent.x or 0.0, agent.y or 0.0, agent.z or 0.0, hour)
    speedFactor = speedFactor * math.max(0.05, density.speedMultiplier or 1.0)
    return {
        speedFactor = math.max(0.05, speedFactor),
        followingGapFactor = math.max(0.5, followingGapFactor),
        brakingFactor = math.max(0.25, brakingFactor),
        wetness = wetness,
        rainMmPerHour = rain,
        poorVisibility = poorVisibility,
        density = density
    }
end

local function desiredFreeSpeed(agent, evaluatedLink, environment)
    local profile = agent.profile or defaultProfile()
    local limitKmh = evaluatedLink and evaluatedLink.speedLimitKmh or 50.0
    local compliance = clamp(profile.speedCompliance or 0.92, 0.0, 1.0)
    local legalFactor = lerp(1.12, 1.0, compliance)
    local environmentalFactor = environment and environment.speedFactor or 1.0
    return math.max(0.0, limitKmh * legalFactor * math.max(0.1, profile.maxSpeedFactor or 1.0) * environmentalFactor / 3.6)
end

-- Intelligent Driver Model-style longitudinal controller. This is deliberately
-- parameterized by the Studio-authored profile and global road rules instead
-- of hard-coding one personality for every traffic vehicle.
function RacingTrafficAgents.ComputeCarFollowingAcceleration(agent, desiredSpeedMps, leaderGapM, leaderSpeedMps)
    local rules = RacingTraffic.GetRules()
    local profile = agent.profile or defaultProfile()
    local v = math.max(0.0, agent.speedMps or 0.0)
    local v0 = math.max(0.5, desiredSpeedMps or 0.5)
    local a = math.max(0.1, (rules.desiredAccelerationMps2 or 2.0) * (profile.accelerationFactor or 1.0))
    local b = math.max(0.1, (rules.comfortableBrakingMps2 or 2.5) * (profile.brakingFactor or 1.0) * (agent.environmentBrakingFactor or 1.0))
    local timeGap = math.max(0.1, (profile.desiredTimeGapS or rules.desiredTimeGapS or 1.6) * (agent.environmentFollowingGapFactor or 1.0))
    local minimumGap = math.max(0.1, profile.minimumGapM or rules.minimumGapM or 2.5)
    local interaction = 0.0
    if leaderGapM ~= nil and leaderGapM < math.huge then
        local gap = math.max(0.1, leaderGapM)
        local leaderSpeed = math.max(0.0, leaderSpeedMps or 0.0)
        local closingSpeed = v - leaderSpeed
        local desiredGap = minimumGap + math.max(0.0, v * timeGap + (v * closingSpeed) / (2.0 * math.sqrt(a * b)))
        interaction = (desiredGap / gap) * (desiredGap / gap)
    end
    local freeRoad = 1.0 - math.pow(v / v0, 4.0)
    return a * (freeRoad - interaction)
end

local function leaderOnSegment(manager, agent, segmentLength)
    local bestGap, bestSpeed = math.huge, 0.0
    for _, other in pairs(manager.agents) do
        if other.id ~= agent.id
            and other.routeNodes[other.routeIndex] == agent.routeNodes[agent.routeIndex]
            and other.routeNodes[other.routeIndex + 1] == agent.routeNodes[agent.routeIndex + 1]
            and (other.segmentProgressM or 0.0) > (agent.segmentProgressM or 0.0) then
            local gap = (other.segmentProgressM - agent.segmentProgressM) - 0.5 * ((other.profile.lengthM or 4.4) + (agent.profile.lengthM or 4.4))
            if gap < bestGap then bestGap = gap; bestSpeed = other.speedMps or 0.0 end
        end
    end
    if bestGap == math.huge then return nil, nil, nil end
    local bestLeader = nil
    for _, other in pairs(manager.agents) do
        if other.id ~= agent.id and other.routeNodes[other.routeIndex] == agent.routeNodes[agent.routeIndex] and other.routeNodes[other.routeIndex + 1] == agent.routeNodes[agent.routeIndex + 1] and (other.segmentProgressM or 0.0) > (agent.segmentProgressM or 0.0) then
            local gap = (other.segmentProgressM - agent.segmentProgressM) - 0.5 * ((other.profile.lengthM or 4.4) + (agent.profile.lengthM or 4.4))
            if math.abs(gap - bestGap) < 0.1 then bestLeader = other; break end
        end
    end
    return math.max(0.05, bestGap), bestSpeed, bestLeader
end

local chooseNewTrip

local function intersectionForConnector(connector)
    if connector == nil then return nil end
    return RacingGameplay.GetIntersection and RacingGameplay.GetIntersection(connector.intersectionId) or nil
end

local function approachAgentsForNode(manager, targetNodeId, excludedAgentId)
    local result = {}
    for _, other in pairs(manager.agents or {}) do
        if other.id ~= excludedAgentId then
            local _, otherTo, otherLink, otherLength = currentSegment(other)
            if otherTo ~= nil and otherTo.id == targetNodeId and otherLink ~= nil then
                local remaining = math.max(0.0, (otherLength or 0.0) - (other.segmentProgressM or 0.0))
                result[#result + 1] = { agent = other, link = otherLink, remaining = remaining, eta = remaining / math.max(0.5, other.speedMps or 0.0) }
            end
        end
    end
    return result
end

local function roundaboutShouldYield(agent, manager, connector, remainingM)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    if behavior.roundaboutNegotiation == false or connector == nil then return false end
    local intersection = intersectionForConnector(connector)
    if intersection == nil or intersection.priority ~= "Roundabout" then return false end
    local gapSeconds = math.max(0.5, behavior.roundaboutEntryGapS or 2.5)
    local center = { x = intersection.x or 0.0, y = intersection.y or 0.0, z = intersection.z or 0.0 }
    local radius = math.max(6.0, intersection.radiusM or 12.0)
    for _, other in pairs(manager.agents or {}) do
        if other.id ~= agent.id and horizontalDistance(other, center) <= radius * 1.35 then
            local _, _, otherLink = currentSegment(other)
            local otherConnector = connectorForLink(otherLink)
            if otherConnector ~= nil and otherConnector.intersectionId == connector.intersectionId then
                local otherEta = horizontalDistance(other, center) / math.max(1.0, other.speedMps or 0.0)
                local selfEta = math.max(0.0, remainingM or 0.0) / math.max(1.0, agent.speedMps or 0.0)
                if otherEta <= selfEta + gapSeconds then return true end
            end
        end
    end
    return false
end

local function zipperMergeShouldYield(agent, manager, link, remainingM)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    if behavior.zipperMerging == false or link == nil or link.type ~= "Merge" then return false end
    local targetNodeId = link.toNodeId
    if remainingM > math.max(8.0, (agent.speedMps or 0.0) * (behavior.mergeCourtesyGapS or 1.2) + 6.0) then return false end
    manager.mergeState = manager.mergeState or {}
    local state = manager.mergeState[targetNodeId] or { lastLane = nil, lastTime = -math.huge, owner = nil }
    manager.mergeState[targetNodeId] = state
    local currentNode = nodeById(link.fromNodeId)
    local lane = currentNode and currentNode.laneIndex or 0
    local competitors = approachAgentsForNode(manager, targetNodeId, agent.id)
    local nearest = nil
    for _, candidate in ipairs(competitors) do
        if candidate.link.type == "Merge" and candidate.remaining <= remainingM + 10.0 then
            if nearest == nil or candidate.eta < nearest.eta then nearest = candidate end
        end
    end
    if nearest == nil then return false end
    local otherNode = nodeById(nearest.link.fromNodeId)
    local otherLane = otherNode and otherNode.laneIndex or 0
    local alternating = state.lastLane ~= nil and state.lastLane == lane and otherLane ~= lane
    local ownerBlocks = state.owner ~= nil and state.owner ~= agent.id and (manager.timeSeconds - (state.lastTime or -math.huge)) < math.max(0.1, behavior.zipperAlternationWindowS or 3.0)
    return alternating or ownerBlocks or nearest.eta + 0.15 < (remainingM / math.max(0.5, agent.speedMps or 0.0))
end

local function markMergeComplete(agent, manager, link)
    if link == nil or link.type ~= "Merge" then return end
    local currentNode = nodeById(link.fromNodeId)
    manager.mergeState = manager.mergeState or {}
    manager.mergeState[link.toNodeId] = { lastLane = currentNode and currentNode.laneIndex or 0, lastTime = manager.timeSeconds, owner = agent.id }
end

local function nearestTrafficNodeTo(position)
    local best, bestDistance = nil, math.huge
    for _, node in ipairs(RacingGameplay.data.trafficNodes or {}) do
        if node.type == "Lane Node" or node.type == "Destination" or node.type == "Parking" then
            local d = horizontalDistance(position, node)
            if d < bestDistance then best, bestDistance = node, d end
        end
    end
    return best, bestDistance
end

local function maybeDispatchEmergencyAgent(agent, manager, dt)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    if not agent.emergency or behavior.emergencyIncidentDispatch == false then return false end
    if agent.incidentResponseId ~= nil then
        local target = nil
        for _, incident in ipairs(RacingTraffic.GetActiveIncidents(manager.operations)) do if incident.id == agent.incidentResponseId then target = incident; break end end
        if target == nil then agent.incidentResponseId = nil; agent.incidentResponseTimerS = 0.0; return false end
        if horizontalDistance(agent, target) <= math.max(6.0, target.radiusM or 20.0) then
            agent.speedMps = 0.0
            agent.desiredSpeedMps = 0.0
            agent.waitReason = "incident response"
            agent.decision = "responding to incident " .. tostring(target.id)
            agent.incidentResponseTimerS = (agent.incidentResponseTimerS or 0.0) + math.max(0.0, dt or 0.0)
            if agent.incidentResponseTimerS >= 20.0 then
                if (target.id or 0) >= 1000000 then RacingTraffic.ClearIncident(manager.operations, target.id) end
                agent.incidentResponseId = nil; agent.incidentResponseTimerS = 0.0; chooseNewTrip(agent, manager)
            end
            return true
        end
        return false
    end
    local best, bestDistance = nil, math.max(50.0, behavior.emergencyIncidentLookaheadM or 1500.0)
    for _, incident in ipairs(RacingTraffic.GetActiveIncidents(manager.operations)) do
        if incident.emergencyResponse ~= false then
            local d = horizontalDistance(agent, incident)
            if d < bestDistance then best, bestDistance = incident, d end
        end
    end
    if best == nil then return false end
    local targetNode = nearestTrafficNodeTo(best)
    if targetNode == nil or targetNode.id == agent.currentNodeId then return false end
    local nodes, links = RacingTraffic.FindRoute(agent.currentNodeId, targetNode.id, { hour = manager.hour, runtimeState = manager.operations, vehicle = agent })
    if nodes == nil or #nodes < 2 then return false end
    setRoute(agent, nodes, links)
    agent.incidentResponseId = best.id
    agent.incidentResponseTimerS = 0.0
    agent.decision = "dispatch to incident " .. tostring(best.id)
    return true
end

local function signalOrReservationStop(agent, manager, link, remainingM)
    local config = RacingTrafficAgents.GetConfiguration()
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    if zipperMergeShouldYield(agent, manager, link, remainingM) then agent.mergeWait = true; agent.mergeTargetNodeId = link.toNodeId; return true, "zipper merge" end
    local connector = connectorForLink(link)
    if connector == nil then agent.mergeWait = false; return false, nil end
    local intersection = intersectionForConnector(connector)
    if not connectorHasGreen(manager.operations, connector) then
        if remainingM <= math.max(1.0, config.perceptionRangeM or 120.0) then return true, "signal" end
        return false, nil
    end
    if intersection ~= nil and intersection.priority == "Stop" and behavior.enforceStopDwell ~= false
        and remainingM <= math.max(3.0, config.stopLineBufferM or 2.0) + 5.0 then
        if agent.stopDwellConnectorId ~= connector.id then
            agent.stopDwellConnectorId = connector.id
            agent.stopDwellRemainingS = math.max(0.0, behavior.stopDwellS or 1.0)
        end
        if (agent.stopDwellRemainingS or 0.0) > 0.0 then return true, "stop sign" end
    end
    if roundaboutShouldYield(agent, manager, connector, remainingM) then return true, "roundabout gap" end
    agent.mergeWait = false
    if remainingM <= math.max(3.0, config.stopLineBufferM or 2.0) + 4.0 then
        local ok = RacingTraffic.TryReserveIntersection(manager.operations, connector.id, agent.id, manager.timeSeconds)
        if not ok then
            if connector.yield and (behavior.yieldCreepSpeedKmh or 0.0) > 0.0 and remainingM > (config.stopLineBufferM or 2.0) + 1.0 then
                return false, "yield creep"
            end
            return true, connector.yield and "yield" or "intersection reservation"
        end
    end
    return false, nil
end

local function emergencyStopOrSlow(agent, manager)
    if agent.emergency then return false end
    local rules = RacingTraffic.GetRules()
    if rules.emergencyCorridor ~= true then return false end
    local radius = rules.emergencyYieldRadiusM or 80.0
    for _, other in pairs(manager.agents) do
        if other.id ~= agent.id and other.emergency and horizontalDistance(agent, other) <= radius then
            return true
        end
    end
    return false
end

local function targetLaneGaps(manager, agent, targetNode)
    local frontGap, rearGap = math.huge, math.huge
    local frontSpeed, rearSpeed = 0.0, 0.0
    local radians = math.rad(agent.headingDeg or 0.0)
    local forwardX, forwardZ = math.sin(radians), math.cos(radians)
    for _, other in pairs(manager.agents or {}) do
        if other.id ~= agent.id then
            local otherCurrent = nodeById(other.routeNodes and other.routeNodes[other.routeIndex] or other.currentNodeId)
            local otherNext = nodeById(other.routeNodes and other.routeNodes[(other.routeIndex or 1) + 1] or 0)
            local function matchesTargetLane(n)
                return n ~= nil and n.roadId == targetNode.roadId and n.laneIndex == targetNode.laneIndex and n.laneDirection == targetNode.laneDirection
            end
            local onTargetLane = matchesTargetLane(otherCurrent) or matchesTargetLane(otherNext)
            if onTargetLane then
                local dx, dz = (other.x or 0.0) - (agent.x or 0.0), (other.z or 0.0) - (agent.z or 0.0)
                local longitudinal = dx * forwardX + dz * forwardZ
                local clearance = math.abs(longitudinal) - 0.5 * ((other.profile and other.profile.lengthM or 4.4) + (agent.profile and agent.profile.lengthM or 4.4))
                if longitudinal >= 0.0 and clearance < frontGap then frontGap, frontSpeed = math.max(0.0, clearance), other.speedMps or 0.0 end
                if longitudinal < 0.0 and clearance < rearGap then rearGap, rearSpeed = math.max(0.0, clearance), other.speedMps or 0.0 end
            end
        end
    end
    return frontGap, rearGap, frontSpeed, rearSpeed
end

function RacingTrafficAgents.EvaluateLaneChange(agent, manager)
    local config = RacingTrafficAgents.GetConfiguration()
    if config.enableLaneChanges == false or (agent.laneChangeCooldownS or 0.0) > 0.0 then return nil end
    local currentNodeId = agent.routeNodes[agent.routeIndex]
    local profile = agent.profile or defaultProfile()
    local rules = RacingTraffic.GetRules()
    local density = RacingGameplay.GetTrafficDensityAt(agent.x or 0.0, agent.y or 0.0, agent.z or 0.0, manager.hour)
    local environmentCfg = RacingGameplay.GetTrafficEnvironmentConfiguration()
    local wetnessGap = 1.0
    if environmentCfg.weatherAwareLaneChanges ~= false then wetnessGap = agent.environmentFollowingGapFactor or 1.0 end
    local minimumGap = math.max(rules.laneChangeMinimumGapM or 8.0, profile.minimumGapM or 2.5) * wetnessGap
    local candidates = {}
    for _, link in ipairs(RacingGameplay.GetOutgoingRoadLinks(currentNodeId)) do
        if link.enabled ~= false and (link.type == "Lane Change" or (config.enableMerges ~= false and link.type == "Merge")) then
            local target = nodeById(link.toNodeId)
            if target ~= nil then
                local frontGap, rearGap, targetFrontSpeed, targetRearSpeed = targetLaneGaps(manager, agent, target)
                local dynamicRearGap = minimumGap + math.max(0.0, (targetRearSpeed or 0.0) - (agent.speedMps or 0.0)) * math.max(0.2, behavior.mergeCourtesyGapS or 1.2)
                if frontGap >= minimumGap and rearGap >= dynamicRearGap then
                    local aggression = clamp((profile.laneChangeAggression or 0.5) + (density.laneChangeAggressionOffset or 0.0), 0.0, 1.0)
                    local courtesyPenalty = 1.0 + clamp(profile.courtesy or 0.6, 0.0, 1.0) * 0.20
                    local movementBonus = link.type == "Merge" and 0.12 or 0.0
                    local score = aggression / (math.max(1.0, link.routeCostMultiplier or 1.0) * courtesyPenalty) + movementBonus
                    candidates[#candidates + 1] = { link = link, target = target, score = score, frontGap = frontGap, rearGap = rearGap, targetFrontSpeed = targetFrontSpeed, targetRearSpeed = targetRearSpeed }
                end
            end
        end
    end
    table.sort(candidates, function(a, b) return a.score > b.score end)
    local best = candidates[1]
    agent.laneChangeScore = best and best.score or 0.0
    return best
end

local function maybeUseLaneChange(agent, manager, forcedReason)
    -- The route planner already prices semantic lane-change links. This local
    -- behavior path exists for blocked/stuck lanes and deliberately requires a
    -- meaningful aggression threshold so agents do not weave continuously.
    local profile = agent.profile or defaultProfile()
    if forcedReason == nil and (profile.laneChangeAggression or 0.5) < 0.35 then return false end
    if forcedReason == nil and (agent.stuckTimerS or 0.0) < 2.0 then return false end
    local choice = RacingTrafficAgents.EvaluateLaneChange(agent, manager)
    if choice == nil then return false end
    local destination = agent.goalNodeId
    local routeNodes, routeLinks = RacingTraffic.FindRoute(choice.target.id, destination, { runtimeState = manager.operations, vehicle = agent })
    if routeNodes == nil then return false end
    table.insert(routeNodes, 1, agent.currentNodeId)
    table.insert(routeLinks, 1, choice.link.id)
    setRoute(agent, routeNodes, routeLinks)
    agent.laneChangeCooldownS = RacingTraffic.GetRules().laneChangeCooldownS or 3.0
    agent.laneChangeReason = forcedReason or "blocked lane"
    agent.decision = "lane change: " .. agent.laneChangeReason
    return true
end

local function maybeReturnToDrivingLane(agent, manager)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    local rules = RacingTraffic.GetRules()
    if rules.keepToDrivingSide == false or agent.laneChangeReason ~= "overtake slower traffic" or (agent.laneChangeCooldownS or 0.0) > 0.0 then return false end
    local currentNode = nodeById(agent.routeNodes[agent.routeIndex])
    if currentNode == nil then return false end
    local targetLink, targetNode = nil, nil
    for _, link in ipairs(RacingGameplay.GetOutgoingRoadLinks(currentNode.id)) do
        if link.enabled ~= false and link.type == "Lane Change" then
            local candidate = nodeById(link.toNodeId)
            if candidate ~= nil and candidate.roadId == currentNode.roadId and candidate.laneDirection == currentNode.laneDirection then
                local towardDrivingSide = (rules.drivingSide == "Left" and candidate.laneIndex < currentNode.laneIndex)
                    or (rules.drivingSide ~= "Left" and candidate.laneIndex > currentNode.laneIndex)
                if towardDrivingSide then targetLink, targetNode = link, candidate; break end
            end
        end
    end
    if targetNode == nil then agent.laneChangeReason = nil; return false end
    local requiredGap = math.max(2.0, behavior.overtakeReturnGapM or 18.0)
    for _, other in pairs(manager.agents or {}) do
        if other.id ~= agent.id and horizontalDistance(other, targetNode) < requiredGap then return false end
    end
    local routeNodes, routeLinks = RacingTraffic.FindRoute(targetNode.id, agent.goalNodeId, { hour = manager.hour, runtimeState = manager.operations, vehicle = agent })
    if routeNodes == nil then return false end
    table.insert(routeNodes, 1, currentNode.id); table.insert(routeLinks, 1, targetLink.id)
    setRoute(agent, routeNodes, routeLinks)
    agent.laneChangeCooldownS = RacingTraffic.GetRules().laneChangeCooldownS or 3.0
    agent.laneChangeReason = "return to driving lane"
    agent.decision = "returning after overtake"
    return true
end

local function updatePoseOnSegment(agent, from, to, length)
    local t = clamp((agent.segmentProgressM or 0.0) / math.max(0.05, length), 0.0, 1.0)
    agent.x = lerp(from.x or 0.0, to.x or 0.0, t)
    agent.y = lerp(from.y or 0.0, to.y or 0.0, t)
    agent.z = lerp(from.z or 0.0, to.z or 0.0, t)
    local dx = (to.x or 0.0) - (from.x or 0.0)
    local dz = (to.z or 0.0) - (from.z or 0.0)
    if math.abs(dx) + math.abs(dz) > 0.0001 then agent.headingDeg = math.deg(math.atan(dx, dz)) end
end

chooseNewTrip = function(agent, manager)
    local destination, nodes, links, portal = chooseDestination(agent.currentNodeId, manager, agent)
    if destination == nil then return false end
    setRoute(agent, nodes, links)
    agent.goalNodeId = destination.id
    agent.destinationPortalId = portal and portal.id or nil
    return true
end

function RacingTrafficAgents.StepAgent(agent, manager, dt)
    dt = math.max(0.0, dt or 0.0)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    agent.collisionCooldownS = math.max(0.0, (agent.collisionCooldownS or 0.0) - dt)
    if (agent.stopDwellRemainingS or 0.0) > 0.0 and (agent.speedMps or 0.0) < 0.35 then
        agent.stopDwellRemainingS = math.max(0.0, agent.stopDwellRemainingS - dt)
    end
    if (agent.queueReleaseTimerS or 0.0) > 0.0 then
        agent.queueReleaseTimerS = math.max(0.0, agent.queueReleaseTimerS - dt)
        agent.speedMps = 0.0; agent.accelerationMps2 = 0.0; agent.desiredSpeedMps = 0.0
        agent.waitReason = "queue reaction"; agent.decision = "waiting for queue discharge"
        return
    end
    if agent.parkingManeuver == "reverse" then
        local reverseMps = math.max(0.2, behavior.parkingReverseSpeedKmh or 4.0) / 3.6
        local radians = math.rad(agent.headingDeg or 0.0)
        agent.speedMps = -reverseMps
        agent.x = (agent.x or 0.0) - math.sin(radians) * reverseMps * dt
        agent.z = (agent.z or 0.0) - math.cos(radians) * reverseMps * dt
        agent.parkingManeuverRemainingS = math.max(0.0, (agent.parkingManeuverRemainingS or 0.0) - dt)
        agent.waitReason = "reverse parking"; agent.decision = "reverse parking maneuver"
        if agent.parkingManeuverRemainingS <= 0.0 then
            local target = nodeById(agent.parkingTarget)
            if target ~= nil then agent.x, agent.y, agent.z = target.x or agent.x, target.y or agent.y, target.z or agent.z; agent.headingDeg = target.headingDeg or agent.headingDeg end
            agent.speedMps = 0.0; agent.parkingManeuver = nil; agent.parked = true
        end
        return
    elseif agent.parkingManeuver == "exit" then
        agent.parkingManeuverRemainingS = math.max(0.0, (agent.parkingManeuverRemainingS or 0.0) - dt)
        agent.speedMps = 0.0; agent.waitReason = "parking exit"; agent.decision = "checking gap to leave parking"
        if agent.parkingManeuverRemainingS <= 0.0 then agent.parkingManeuver = nil; agent.waitReason = nil end
        return
    end
    if (agent.recoveryReverseRemainingS or 0.0) > 0.0 then
        local reverseMps = 1.5
        local radians = math.rad(agent.headingDeg or 0.0)
        agent.x = (agent.x or 0.0) - math.sin(radians) * reverseMps * dt
        agent.z = (agent.z or 0.0) - math.cos(radians) * reverseMps * dt
        agent.speedMps = -reverseMps
        agent.recoveryReverseRemainingS = math.max(0.0, agent.recoveryReverseRemainingS - dt)
        agent.waitReason = "stuck recovery reverse"; agent.decision = "reversing to recover"
        return
    end
    if agent.parked then
        agent.speedMps = 0.0
        agent.accelerationMps2 = 0.0
        agent.desiredSpeedMps = 0.0
        agent.waitReason = "parked"
        agent.parkingTimerS = math.max(0.0, (agent.parkingTimerS or 0.0) - dt)
        if agent.parkingTimerS <= 0.0 then
            agent.parked = false
            agent.waitReason = nil
            chooseNewTrip(agent, manager)
            if behavior.stagedParkingManeuvers ~= false then
                agent.parkingManeuver = "exit"
                agent.parkingManeuverRemainingS = 0.5 + (1.0 - clamp((agent.profile or defaultProfile()).parkingSkill or 0.8, 0.0, 1.0)) * 1.5
            end
        end
        return
    end
    agent.laneChangeCooldownS = math.max(0.0, (agent.laneChangeCooldownS or 0.0) - dt)
    if agent.emergency and maybeDispatchEmergencyAgent(agent, manager, dt) then return end
    local from, to, link, length = currentSegment(agent)
    if from == nil then
        if not chooseNewTrip(agent, manager) then agent.speedMps = 0.0; agent.waitReason = "no route"; return end
        from, to, link, length = currentSegment(agent)
        if from == nil then return end
    end

    local evaluated = RacingTraffic.EvaluateLink(link, manager.hour, agent, manager.operations)
    if evaluated == nil then
        agent.speedMps = math.max(0.0, agent.speedMps - 4.0 * dt)
        agent.waitReason = "route restricted"
        agent.stuckTimerS = (agent.stuckTimerS or 0.0) + dt
        maybeUseLaneChange(agent, manager)
        return
    end

    local remaining = math.max(0.0, length - (agent.segmentProgressM or 0.0))
    local previousWaitReason = agent.waitReason
    local stop, stopReason = signalOrReservationStop(agent, manager, link, remaining)
    if behavior.queueDischargeReaction ~= false and previousWaitReason == "signal" and stopReason ~= "signal" and (agent.queueReleaseTimerS or 0.0) <= 0.0 then
        local profile = agent.profile or defaultProfile()
        agent.queueReleaseTimerS = math.max(0.0, behavior.queueReactionSpreadS or 0.65) * (0.15 + (profile.reactionTimeS or 0.45) + math.random() * 0.85)
    end
    local emergencyYield = emergencyStopOrSlow(agent, manager)
    local environment = getEnvironmentModifiers(manager, agent)
    agent.environmentSpeedFactor = environment.speedFactor
    agent.environmentFollowingGapFactor = environment.followingGapFactor
    agent.environmentBrakingFactor = environment.brakingFactor
    agent.densityRegionCount = environment.density.regionCount or 0
    agent.incidentInfluenceId = evaluated.incidentId
    agent.incidentInfluence = evaluated.incidentInfluence or 0.0
    local desiredSpeed = desiredFreeSpeed(agent, evaluated, environment)
    local agentConfig = RacingTrafficAgents.GetConfiguration()
    if link.type == "Parking Access" and agentConfig.enableParking ~= false then
        local skill = clamp((agent.profile or defaultProfile()).parkingSkill or 0.8, 0.0, 1.0)
        local approachKmh = math.max(1.0, agentConfig.parkingApproachSpeedKmh or 10.0)
        desiredSpeed = math.min(desiredSpeed, approachKmh * lerp(0.55, 1.0, skill) / 3.6)
    end
    if stop then desiredSpeed = 0.0 end
    if stopReason == "yield creep" then desiredSpeed = math.min(desiredSpeed, math.max(0.0, behavior.yieldCreepSpeedKmh or 4.0) / 3.6) end
    if emergencyYield then desiredSpeed = math.min(desiredSpeed, 8.0 / 3.6) end
    local leaderGap, leaderSpeed, leader = leaderOnSegment(manager, agent, length)
    if behavior.opportunisticOvertaking ~= false and leader ~= nil and leaderGap ~= nil and leaderGap < math.max(20.0, RacingTrafficAgents.GetConfiguration().perceptionRangeM or 120.0) then
        local gainKmh = (desiredSpeed - (leaderSpeed or 0.0)) * 3.6
        local profile = agent.profile or defaultProfile()
        local linkAllows = link.overtakingAllowed ~= false
        if linkAllows and gainKmh >= math.max(0.0, behavior.overtakeMinimumGainKmh or 8.0) and (profile.laneChangeAggression or 0.5) >= 0.4 then
            if maybeUseLaneChange(agent, manager, "overtake slower traffic") then return end
        end
    end
    if leader == nil or leaderGap == nil or leaderGap >= math.max(2.0, behavior.overtakeReturnGapM or 18.0) then
        if maybeReturnToDrivingLane(agent, manager) then return end
    end
    if stop then
        local stopGap = math.max(0.1, remaining - (RacingTrafficAgents.GetConfiguration().stopLineBufferM or 2.0))
        if leaderGap == nil or stopGap < leaderGap then leaderGap = stopGap; leaderSpeed = 0.0 end
    end

    local acceleration = RacingTrafficAgents.ComputeCarFollowingAcceleration(agent, math.max(0.5, desiredSpeed), leaderGap, leaderSpeed)
    local profile = agent.profile or defaultProfile()
    local maxBrake = math.max(1.0, (RacingTraffic.GetRules().comfortableBrakingMps2 or 2.5) * (profile.brakingFactor or 1.0) * (agent.environmentBrakingFactor or 1.0) * 2.2)
    acceleration = clamp(acceleration, -maxBrake, math.max(0.5, (RacingTraffic.GetRules().desiredAccelerationMps2 or 2.0) * (profile.accelerationFactor or 1.0)))
    agent.accelerationMps2 = acceleration
    agent.desiredSpeedMps = desiredSpeed
    agent.speedMps = math.max(0.0, (agent.speedMps or 0.0) + acceleration * dt)
    if desiredSpeed <= 0.01 and agent.speedMps < 0.2 then agent.speedMps = 0.0 end
    agent.segmentProgressM = (agent.segmentProgressM or 0.0) + agent.speedMps * dt
    agent.waitReason = stopReason or (emergencyYield and "emergency corridor" or nil)
    agent.decision = agent.waitReason and ("waiting: " .. agent.waitReason) or "follow route"

    if agent.speedMps < 0.35 and desiredSpeed > 1.0 then agent.stuckTimerS = (agent.stuckTimerS or 0.0) + dt else agent.stuckTimerS = 0.0; agent.recoveryStage = "none" end
    if agent.stuckTimerS > 2.0 then maybeUseLaneChange(agent, manager) end
    if behavior.stuckRecovery ~= false then
        if agent.stuckTimerS >= math.max(1.0, behavior.recoveryRerouteSeconds or 8.0) and manager.timeSeconds - (agent.recoveryLastAttemptS or -math.huge) >= 2.0 then
            agent.recoveryLastAttemptS = manager.timeSeconds
            local nodes, links = RacingTraffic.FindRoute(agent.currentNodeId, agent.goalNodeId, { hour = manager.hour, runtimeState = manager.operations, vehicle = agent })
            if nodes ~= nil and #nodes >= 2 then setRoute(agent, nodes, links); agent.recoveryStage = "rerouted"; agent.decision = "stuck recovery reroute" else agent.recoveryStage = "reroute failed" end
        end
        if agent.stuckTimerS >= math.max(2.0, RacingTrafficAgents.GetConfiguration().stuckTimeoutS or 20.0) and (agent.recoveryReverseRemainingS or 0.0) <= 0.0 then
            agent.recoveryReverseRemainingS = math.max(0.0, behavior.recoveryReverseSeconds or 1.25); agent.recoveryStage = "reverse"
        end
        if agent.stuckTimerS >= math.max((behavior.recoveryRerouteSeconds or 8.0) + 1.0, behavior.recoveryTeleportSeconds or 35.0) then
            local recoveryNode = nodeById(agent.routeNodes[math.min(#agent.routeNodes, agent.routeIndex + 1)])
            if recoveryNode ~= nil then
                agent.x, agent.y, agent.z = recoveryNode.x or agent.x, recoveryNode.y or agent.y, recoveryNode.z or agent.z
                agent.currentNodeId = recoveryNode.id; agent.routeIndex = math.min(#agent.routeNodes, agent.routeIndex + 1); agent.segmentProgressM = 0.0; agent.speedMps = 0.0; agent.stuckTimerS = 0.0; agent.recoveryStage = "relocated"; agent.decision = "last-resort recovery relocation"
            end
        end
    end

    while agent.segmentProgressM >= length do
        local completedLink = link
        local completedTo = to
        markMergeComplete(agent, manager, completedLink)
        local completedConnector = connectorForLink(completedLink)
        if completedConnector ~= nil and agent.stopDwellConnectorId == completedConnector.id then agent.stopDwellConnectorId = nil; agent.stopDwellRemainingS = 0.0 end
        agent.segmentProgressM = agent.segmentProgressM - length
        agent.routeIndex = agent.routeIndex + 1
        agent.currentNodeId = agent.routeNodes[agent.routeIndex] or agent.currentNodeId
        if agentConfig.enableParking ~= false and completedLink ~= nil and completedLink.type == "Parking Access" and completedTo ~= nil and completedTo.type == "Parking" then
            local skill = clamp((agent.profile or defaultProfile()).parkingSkill or 0.8, 0.0, 1.0)
            agent.x, agent.y, agent.z = completedTo.x or agent.x, completedTo.y or agent.y, completedTo.z or agent.z
            agent.headingDeg = completedTo.headingDeg or agent.headingDeg
            agent.speedMps = 0.0
            agent.accelerationMps2 = 0.0
            agent.parkingTarget = completedTo.id
            agent.parkingTimerS = 12.0 + (1.0 - skill) * 8.0 + math.random() * 45.0
            agent.segmentProgressM = 0.0
            if behavior.stagedParkingManeuvers ~= false then
                agent.parkingManeuver = "reverse"
                agent.parkingManeuverRemainingS = 0.8 + (1.0 - skill) * 2.2
                agent.parked = false
                agent.waitReason = "reverse parking"
            else
                agent.parked = true
                agent.waitReason = "parked"
            end
            return
        end
        from, to, link, length = currentSegment(agent)
        if from == nil then
            agent.segmentProgressM = 0.0
            if agent.destinationPortalId ~= nil and agent.currentNodeId == agent.goalNodeId then
                agent.readyToDespawn = true
                agent.speedMps = 0.0
                agent.waitReason = "despawn portal"
                break
            end
            if not chooseNewTrip(agent, manager) then agent.speedMps = 0.0 end
            from, to, link, length = currentSegment(agent)
            break
        end
    end
    if from ~= nil then updatePoseOnSegment(agent, from, to, length) end
end

local function updateStreamingTier(agent, manager, playerPosition)
    local distance = playerPosition and horizontalDistance(agent, playerPosition) or 0.0
    local tier = RacingTraffic.GetStreamingTier(distance)
    agent.streamingTier = tier
    local config = RacingTrafficAgents.GetConfiguration()
    local factory = RacingTrafficAgents.vehicleFactory or defaultVehicleFactory
    local capacity = math.max(0, config.maxFullPhysicsAgents or 64)
    local needsProxy = false
    if tier == "full" then
        if agent.proxy ~= nil then
            if manager.fullPhysicsCount < capacity then
                manager.fullPhysicsCount = manager.fullPhysicsCount + 1
                needsProxy = true
            end
        elseif manager.fullPhysicsCount < capacity then
            needsProxy = true
        end
    end
    if needsProxy and agent.proxy == nil then
        agent.proxy = factory.Spawn and factory.Spawn(agent) or nil
        if agent.proxy ~= nil then manager.fullPhysicsCount = manager.fullPhysicsCount + 1 end
    elseif not needsProxy and agent.proxy ~= nil then
        if factory.Destroy then factory.Destroy(agent.proxy) end
        agent.proxy = nil
    end
    if agent.proxy ~= nil and factory.Update then factory.Update(agent.proxy, agent) end
    return tier
end

function RacingTrafficAgents.CreateManager()
    return {
        agents = {},
        nextAgentId = 1,
        timeSeconds = 0.0,
        hour = 12.0,
        spawnBudget = 0.0,
        despawnBudget = 0.0,
        operations = RacingTraffic.CreateRuntimeState(),
        fullPhysicsCount = 0,
        localDensityMultiplier = 1.0,
        debugRouteMarkers = {},
        mergeState = {},
        collisionScanAccumulator = 0.0,
        telemetry = { active = 0, full = 0, simplified = 0, dormant = 0, waiting = 0, spawned = 0, despawned = 0, portals = 0, incidents = 0, collisions = 0, recoveries = 0 }
    }
end

function RacingTrafficAgents.SpawnAgent(manager, startNode, profile, sourcePortal)
    if manager == nil or startNode == nil then return nil end
    profile = profile or RacingTrafficAgents.ChooseProfile(nil)
    if sourcePortal ~= nil and not portalAllowsProfile(sourcePortal, profile) then return nil end
    local destination, nodes, links, destinationPortal = chooseDestination(startNode.id, manager, profile)
    if destination == nil then return nil end
    local agent = RacingTrafficAgents.CreateAgent(profile, startNode.id, destination.id, {
        routeOptions = { hour = manager.hour, runtimeState = manager.operations, vehicle = profile },
        sourcePortalId = sourcePortal and sourcePortal.id or nil,
        destinationPortalId = destinationPortal and destinationPortal.id or nil
    })
    if agent == nil then return nil end
    setRoute(agent, nodes, links)
    agent.destinationPortalId = destinationPortal and destinationPortal.id or nil
    agent.id = manager.nextAgentId
    manager.nextAgentId = manager.nextAgentId + 1
    manager.agents[agent.id] = agent
    manager.telemetry.spawned = (manager.telemetry.spawned or 0) + 1
    return agent
end

function RacingTrafficAgents.AssignDestination(agent, goalNodeId, options)
    if agent == nil or goalNodeId == nil then return false, "invalid destination" end
    local startNodeId = agent.currentNodeId or (agent.routeNodes and agent.routeNodes[agent.routeIndex or 1])
    if startNodeId == nil then return false, "agent has no current node" end
    options = options or {}
    local manager = options.manager or RacingTrafficAgents.manager
    local nodes, links = RacingTraffic.FindRoute(startNodeId, goalNodeId, {
        hour = options.hour or (manager and manager.hour) or 12.0,
        runtimeState = options.runtimeState or (manager and manager.operations) or nil,
        vehicle = options.vehicle or agent.profile or { emergency = agent.emergency == true }
    })
    if nodes == nil or #nodes == 0 then return false, "no route" end
    setRoute(agent, nodes, links or {})
    agent.goalNodeId = goalNodeId
    agent.destinationPortalId = nil
    return true, "route assigned"
end

function RacingTrafficAgents.SpawnEmergencyUnit(startNodeId, goalNodeId, sourcePortal)
    local manager = RacingTrafficAgents.manager
    if manager == nil then return nil, "traffic manager unavailable" end
    local startNode = nodeById(startNodeId)
    if startNode == nil then return nil, "invalid emergency start node" end
    local profile = RacingTrafficAgents.ChooseProfile("Emergency")
    if profile == nil then return nil, "no emergency profile" end
    local agent = RacingTrafficAgents.SpawnAgent(manager, startNode, profile, sourcePortal)
    if agent == nil then return nil, "emergency spawn failed" end
    agent.emergency = true
    agent.policeUnit = true
    local ok, message = RacingTrafficAgents.AssignDestination(agent, goalNodeId, { manager = manager, vehicle = { emergency = true } })
    if not ok then RacingTrafficAgents.DespawnAgent(manager, agent.id); return nil, message end
    return agent, "emergency unit dispatched"
end

function RacingTrafficAgents.DespawnAgent(manager, agentId)
    if manager == nil then return false end
    local agent = manager.agents[agentId]
    if agent == nil then return false end
    local factory = RacingTrafficAgents.vehicleFactory or defaultVehicleFactory
    if agent.proxy ~= nil and factory.Destroy then factory.Destroy(agent.proxy) end
    manager.agents[agentId] = nil
    manager.telemetry.despawned = (manager.telemetry.despawned or 0) + 1
    return true
end

local function countAgents(manager)
    local count = 0
    for _ in pairs(manager.agents) do count = count + 1 end
    return count
end

local function maintainPopulation(manager, dt, playerPosition)
    local population = RacingGameplay.GetTrafficPopulationConfiguration()
    local density = playerPosition and RacingGameplay.GetTrafficDensityAt(playerPosition.x, playerPosition.y, playerPosition.z, manager.hour) or { densityMultiplier = 1.0 }
    local maximum = math.max(0, math.floor(population.maxActiveVehicles or 0))
    local normalizedDensity = clamp(math.max(0.0, population.globalDensity or 1.0) * math.max(0.0, density.densityMultiplier or 1.0), 0.0, 1.0)
    local target = math.floor(maximum * normalizedDensity + 0.5)
    manager.localDensityMultiplier = density.densityMultiplier or 1.0
    manager.spawnBudget = manager.spawnBudget + dt * math.max(1, RacingTraffic.GetStreamingConfiguration().maxSpawnsPerSecond or 8)
    local active = countAgents(manager)
    while active < target and manager.spawnBudget >= 1.0 do
        local profile = RacingTrafficAgents.ChooseProfile(nil)
        local startNode, portal = chooseStartNode(manager, playerPosition, profile)
        if startNode == nil then break end
        if portal ~= nil and not portalAllowsProfile(portal, profile) then
            profile = RacingTrafficAgents.ChooseProfile(nil)
        end
        if RacingTrafficAgents.SpawnAgent(manager, startNode, profile, portal) == nil then break end
        manager.spawnBudget = manager.spawnBudget - 1.0
        active = active + 1
    end
end

local function buildIntersectionDemand(manager)
    local queues, emergency = {}, {}
    for _, agent in pairs(manager.agents or {}) do
        local _, _, link, length = currentSegment(agent)
        local connector = connectorForLink(link)
        if connector ~= nil then
            local remaining = length and math.max(0.0, length - (agent.segmentProgressM or 0.0)) or math.huge
            if remaining <= math.max(20.0, RacingTrafficAgents.GetConfiguration().perceptionRangeM or 120.0) then
                if (agent.speedMps or 0.0) < 4.0 or agent.waitReason == "signal" or agent.waitReason == "yield" then
                    queues[connector.intersectionId] = (queues[connector.intersectionId] or 0) + 1
                end
                if agent.emergency then emergency[connector.intersectionId] = true end
            end
        end
    end
    return queues, emergency
end

local function detectTrafficCollisions(manager, dt)
    local behavior = RacingTrafficAgents.GetBehaviorConfiguration()
    if behavior.collisionIncidentResponse == false then return end
    manager.collisionScanAccumulator = (manager.collisionScanAccumulator or 0.0) + math.max(0.0, dt or 0.0)
    if manager.collisionScanAccumulator < 0.20 then return end
    manager.collisionScanAccumulator = 0.0
    local threshold = math.max(0.1, behavior.collisionDistanceM or 1.2)
    local ids = RacingTrafficAgents.GetAgentIds(manager)
    for i = 1, #ids do
        local a = manager.agents[ids[i]]
        if a ~= nil and a.streamingTier == "full" and (a.collisionCooldownS or 0.0) <= 0.0 then
            for j = i + 1, #ids do
                local b = manager.agents[ids[j]]
                if b ~= nil and b.streamingTier == "full" and (b.collisionCooldownS or 0.0) <= 0.0 and horizontalDistance(a, b) <= threshold then
                    local relative = math.abs((a.speedMps or 0.0) - (b.speedMps or 0.0))
                    if relative >= 1.5 then
                        a.collisionCooldownS, b.collisionCooldownS = 8.0, 8.0
                        a.speedMps, b.speedMps = 0.0, 0.0
                        a.waitReason, b.waitReason = "collision", "collision"
                        a.decision, b.decision = "collision response", "collision response"
                        local linkId = a.routeLinks and a.routeLinks[a.routeIndex] or 0
                        local link = linkById(linkId)
                        local node = link and nodeById(link.fromNodeId) or nil
                        RacingTraffic.ReportIncident(manager.operations, {
                            name = "Traffic collision " .. tostring(a.id) .. "/" .. tostring(b.id), type = "Collision", enabled = true,
                            roadId = node and node.roadId or 0, linkId = linkId or 0,
                            x = (a.x + b.x) * 0.5, y = (a.y + b.y) * 0.5, z = (a.z + b.z) * 0.5,
                            radiusM = 16.0, severity = clamp(relative / 15.0, 0.25, 1.0), blockedLaneFraction = 0.75,
                            speedLimitKmh = 10.0, routeCostMultiplier = 6.0, clearAfterS = 240.0, emergencyResponse = true, hazardLights = true
                        })
                        manager.telemetry.collisions = (manager.telemetry.collisions or 0) + 1
                        break
                    end
                end
            end
        end
    end
end

local updateDebugRouteVisualization

function RacingTrafficAgents.FixedUpdateManager(manager, dt)
    if manager == nil then return end
    local config = RacingTrafficAgents.GetConfiguration()
    if config.enabled ~= true then return end
    manager.timeSeconds = manager.timeSeconds + math.max(0.0, dt or 0.0)
    manager.operations.timeSeconds = manager.timeSeconds
    if Environment ~= nil and Environment.GetTimeOfDay ~= nil then
        manager.hour = Environment.GetTimeOfDay() or manager.hour
    end
    local queueLengths, emergencyRequests = buildIntersectionDemand(manager)
    RacingTraffic.UpdateSignals(manager.operations, dt, queueLengths, emergencyRequests)
    RacingTraffic.UpdateIncidents(manager.operations, dt)
    local playerPosition = currentPlayerPosition()
    maintainPopulation(manager, dt, playerPosition)
    manager.fullPhysicsCount = 0
    local telemetry = { active = 0, full = 0, simplified = 0, dormant = 0, waiting = 0, spawned = manager.telemetry.spawned or 0, despawned = manager.telemetry.despawned or 0, portals = #RacingGameplay.GetActiveTrafficSpawnPortals(manager.hour), incidents = #RacingTraffic.GetActiveIncidents(manager.operations), localDensityMultiplier = manager.localDensityMultiplier or 1.0 }
    local remove = {}
    for id, agent in pairs(manager.agents) do
        local tier = updateStreamingTier(agent, manager, playerPosition)
        if agent.readyToDespawn then
            remove[#remove + 1] = id
        elseif tier == "unloaded" then
            agent.unloadedSeconds = (agent.unloadedSeconds or 0.0) + dt
            if agent.unloadedSeconds >= (config.despawnGraceS or 5.0) then remove[#remove + 1] = id end
        else
            agent.unloadedSeconds = 0.0
            local cadence = tier == "full" and (config.fullSimulationHz or 30.0) or (config.simplifiedSimulationHz or 10.0)
            agent.updateAccumulator = (agent.updateAccumulator or 0.0) + dt
            local step = 1.0 / math.max(1.0, cadence)
            if tier ~= "dormant" and agent.updateAccumulator >= step then
                local accumulated = agent.updateAccumulator
                agent.updateAccumulator = 0.0
                RacingTrafficAgents.StepAgent(agent, manager, accumulated)
                if agent.proxy ~= nil then
                    local factory = RacingTrafficAgents.vehicleFactory or defaultVehicleFactory
                    if factory.Update then factory.Update(agent.proxy, agent) end
                end
            end
        end
        telemetry.active = telemetry.active + 1
        if tier == "full" then telemetry.full = telemetry.full + 1 elseif tier == "simplified" then telemetry.simplified = telemetry.simplified + 1 elseif tier == "dormant" then telemetry.dormant = telemetry.dormant + 1 end
        if agent.waitReason ~= nil then telemetry.waiting = telemetry.waiting + 1 end
    end
    detectTrafficCollisions(manager, dt)
    telemetry.collisions = manager.telemetry.collisions or telemetry.collisions or 0
    for _, id in ipairs(remove) do RacingTrafficAgents.DespawnAgent(manager, id) end
    manager.telemetry = telemetry
    updateDebugRouteVisualization(manager)
end

function RacingTrafficAgents.GetTelemetry(manager)
    if manager == nil then return {} end
    return manager.telemetry or {}
end

function RacingTrafficAgents.GetAgentTelemetry(manager, agentId)
    if manager == nil then return nil end
    local agent = manager.agents[agentId]
    if agent == nil then return nil end
    return {
        id = agent.id,
        profile = agent.profile and agent.profile.name or "Unknown",
        class = agent.profile and agent.profile.class or "Unknown",
        speedKmh = (agent.speedMps or 0.0) * 3.6,
        desiredSpeedKmh = (agent.desiredSpeedMps or 0.0) * 3.6,
        accelerationMps2 = agent.accelerationMps2 or 0.0,
        currentNodeId = agent.currentNodeId,
        goalNodeId = agent.goalNodeId,
        routeIndex = agent.routeIndex,
        routeLength = #(agent.routeNodes or {}),
        streamingTier = agent.streamingTier,
        waitReason = agent.waitReason,
        stuckSeconds = agent.stuckTimerS or 0.0,
        parked = agent.parked == true,
        parkingTarget = agent.parkingTarget,
        parkingSecondsRemaining = agent.parkingTimerS or 0.0,
        x = agent.x, y = agent.y, z = agent.z,
        headingDeg = agent.headingDeg,
        sourcePortalId = agent.sourcePortalId,
        destinationPortalId = agent.destinationPortalId,
        incidentInfluenceId = agent.incidentInfluenceId,
        incidentInfluence = agent.incidentInfluence or 0.0,
        environmentSpeedFactor = agent.environmentSpeedFactor or 1.0,
        environmentFollowingGapFactor = agent.environmentFollowingGapFactor or 1.0,
        densityRegionCount = agent.densityRegionCount or 0,
        decision = agent.decision or "follow route",
        laneChangeScore = agent.laneChangeScore or 0.0,
        laneChangeReason = agent.laneChangeReason,
        stopDwellSeconds = agent.stopDwellRemainingS or 0.0,
        queueReleaseSeconds = agent.queueReleaseTimerS or 0.0,
        mergeWait = agent.mergeWait == true,
        recoveryStage = agent.recoveryStage or "none",
        parkingManeuver = agent.parkingManeuver,
        incidentResponseId = agent.incidentResponseId
    }
end

local function destroyDebugRouteMarkers(manager)
    if manager == nil then return end
    for _, entity in ipairs(manager.debugRouteMarkers or {}) do
        if entity ~= 0 and Entity ~= nil and Entity.Exists(entity) then Entity.Destroy(entity) end
    end
    manager.debugRouteMarkers = {}
end

function RacingTrafficAgents.SetDebugSelectedAgent(agentId)
    RacingTrafficAgents.debugSelectedAgentId = agentId or 0
end

updateDebugRouteVisualization = function(manager)
    if manager == nil or Entity == nil then return end
    local debug = RacingGameplay.GetTrafficDebugConfiguration()
    if debug.enabled ~= true or debug.showRoutes ~= true then destroyDebugRouteMarkers(manager); return end
    local agent = manager.agents[RacingTrafficAgents.debugSelectedAgentId or 0]
    if agent == nil then
        local ids = {}
        for id in pairs(manager.agents) do ids[#ids + 1] = id end
        table.sort(ids)
        agent = ids[1] and manager.agents[ids[1]] or nil
    end
    if agent == nil then destroyDebugRouteMarkers(manager); return end
    manager.debugRouteMarkers = manager.debugRouteMarkers or {}
    local config = RacingTrafficAgents.GetConfiguration()
    local maximum = math.min(16, math.max(2, config.routeLookaheadLinks or 10))
    local points = {}
    for index = agent.routeIndex, math.min(#(agent.routeNodes or {}), agent.routeIndex + maximum) do
        local node = nodeById(agent.routeNodes[index])
        if node ~= nil then points[#points + 1] = node end
    end
    for i = 1, #points do
        local entity = manager.debugRouteMarkers[i]
        if entity == nil or entity == 0 or not Entity.Exists(entity) then
            entity = Entity.Create("Traffic Route Debug " .. tostring(i))
            Entity.AddTag(entity, "TrafficDebug")
            Entity.SetDebugPrimitive(entity, "sphere", i == #points and 1.0 or 0.12, i == #points and 0.55 or 0.82, i == #points and 0.15 or 1.0)
            Entity.SetLocalScale(entity, 0.55, 0.55, 0.55)
            manager.debugRouteMarkers[i] = entity
        end
        Entity.SetLocalPosition(entity, points[i].x or 0.0, (points[i].y or 0.0) + 0.45, points[i].z or 0.0)
    end
    for i = #manager.debugRouteMarkers, #points + 1, -1 do
        local entity = manager.debugRouteMarkers[i]
        if entity ~= 0 and Entity.Exists(entity) then Entity.Destroy(entity) end
        table.remove(manager.debugRouteMarkers, i)
    end
end

function RacingTrafficAgents.GetAgentIds(manager)
    manager = manager or RacingTrafficAgents.manager
    local ids = {}
    if manager == nil then return ids end
    for id in pairs(manager.agents or {}) do ids[#ids + 1] = id end
    table.sort(ids)
    return ids
end

function RacingTrafficAgents.GetDebugSnapshot(manager, maximum)
    manager = manager or RacingTrafficAgents.manager
    local result = {}
    if manager == nil then return result end
    local limit = math.max(1, maximum or (RacingGameplay.GetTrafficDebugConfiguration().maxDetailedAgents or 48))
    for _, id in ipairs(RacingTrafficAgents.GetAgentIds(manager)) do
        result[#result + 1] = RacingTrafficAgents.GetAgentTelemetry(manager, id)
        if #result >= limit then break end
    end
    return result
end

function RacingTrafficAgents.DebugSpawnOne()
    local config = RacingTrafficAgents.GetConfiguration()
    local runtimeEnabled = false
    if config.enabled ~= true then config.enabled = true; runtimeEnabled = true end
    local manager = RacingTrafficAgents.Start()
    local playerPosition = currentPlayerPosition()
    local profile = RacingTrafficAgents.ChooseProfile(nil)
    local startNode, portal = chooseStartNode(manager, playerPosition, profile)
    if startNode == nil then return nil, "No eligible spawn portal or lane node" end
    local agent = RacingTrafficAgents.SpawnAgent(manager, startNode, profile, portal)
    if agent == nil then return nil, "No reachable destination from the selected spawn" end
    local prefix = runtimeEnabled and "Enabled live traffic for this runtime; " or ""
    return agent.id, prefix .. "spawned traffic agent " .. tostring(agent.id)
end

function RacingTrafficAgents.DebugCreateBreakdown(agentId)
    local manager = RacingTrafficAgents.manager
    local agent = manager and manager.agents[agentId] or nil
    if agent == nil then return nil, "Traffic agent not found" end
    local linkId = agent.routeLinks and agent.routeLinks[agent.routeIndex] or 0
    local link = linkById(linkId)
    local roadId = 0
    if link ~= nil then
        local node = nodeById(link.fromNodeId)
        roadId = node and node.roadId or 0
    end
    local incidentId = RacingTraffic.ReportIncident(manager.operations, {
        name = "Runtime breakdown - agent " .. tostring(agent.id),
        type = "Breakdown",
        enabled = true,
        roadId = roadId,
        linkId = linkId or 0,
        x = agent.x, y = agent.y, z = agent.z,
        radiusM = 18.0, severity = 0.65, blockedLaneFraction = 0.85,
        speedLimitKmh = 15.0, routeCostMultiplier = 5.0,
        clearAfterS = 180.0, emergencyResponse = true, hazardLights = true
    })
    return incidentId, "Created runtime breakdown incident " .. tostring(incidentId)
end

function RacingTrafficAgents.DebugClearRuntimeIncidents()
    local manager = RacingTrafficAgents.manager
    if manager == nil then return false end
    RacingTraffic.ClearRuntimeIncidents(manager.operations)
    return true
end

function RacingTrafficAgents.Start()
    if RacingTrafficAgents.manager ~= nil then return RacingTrafficAgents.manager end
    local config = RacingTrafficAgents.GetConfiguration()
    if config.useHeritageVehicleDynamics == true and RacingTrafficVehicleFactory ~= nil then
        RacingTrafficAgents.SetVehicleFactory(RacingTrafficVehicleFactory)
        RacingTrafficAgents.vehicleFactoryMode = "heritage_vehicle"
    else
        RacingTrafficAgents.SetVehicleFactory(nil)
        RacingTrafficAgents.vehicleFactoryMode = "kinematic_proxy"
    end
    RacingTrafficAgents.manager = RacingTrafficAgents.CreateManager()
    return RacingTrafficAgents.manager
end

function RacingTrafficAgents.Stop()
    local manager = RacingTrafficAgents.manager
    if manager == nil then return end
    local ids = {}
    for id in pairs(manager.agents) do ids[#ids + 1] = id end
    for _, id in ipairs(ids) do RacingTrafficAgents.DespawnAgent(manager, id) end
    destroyDebugRouteMarkers(manager)
    RacingTrafficAgents.manager = nil
end

function RacingTrafficAgents.FixedUpdate(dt)
    local config = RacingTrafficAgents.GetConfiguration()
    if config.enabled ~= true then
        if RacingTrafficAgents.manager ~= nil and next(RacingTrafficAgents.manager.agents) ~= nil then RacingTrafficAgents.Stop() end
        return
    end
    local manager = RacingTrafficAgents.Start()
    RacingTrafficAgents.FixedUpdateManager(manager, dt)
end
