RacingTraffic = RacingTraffic or {}

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function activeHour(windowStart, windowEnd, hour)
    local startHour = windowStart or 0.0
    local endHour = windowEnd or 24.0
    local h = hour or 12.0
    if math.abs(startHour - endHour) < 0.0001 then return true end
    if startHour < endHour then return h >= startHour and h < endHour end
    return h >= startHour or h < endHour
end

function RacingTraffic.GetRules()
    return RacingGameplay.data.trafficRules or {}
end

function RacingTraffic.GetStreamingConfiguration()
    return RacingGameplay.data.trafficStreaming or {}
end

function RacingTraffic.GetStreamingTier(distanceM)
    local cfg = RacingTraffic.GetStreamingConfiguration()
    if distanceM <= (cfg.fullSimulationRadiusM or 450.0) then return "full" end
    if distanceM <= (cfg.simplifiedSimulationRadiusM or 900.0) then return "simplified" end
    if distanceM <= (cfg.dormantPersistenceRadiusM or 2500.0) then return "dormant" end
    return "unloaded"
end

function RacingTraffic.GetIntersectionController(intersectionId)
    for _, controller in ipairs(RacingGameplay.data.intersectionControllers or {}) do
        if controller.intersectionId == intersectionId then return controller end
    end
    return nil
end

function RacingTraffic.GetActiveRestrictions(hour, vehicle, runtimeState)
    local result = {}
    local overrides = runtimeState and runtimeState.restrictionOverrides or nil
    for _, restriction in ipairs(RacingGameplay.data.roadRestrictions or {}) do
        local enabled = restriction.enabled
        if overrides and overrides[restriction.id] ~= nil then enabled = overrides[restriction.id] end
        if enabled and activeHour(restriction.startHour, restriction.endHour, hour) then
            local applies = true
            if restriction.type == "Weight Limit" and (restriction.vehicleMassLimitKg or 0) > 0 then
                applies = vehicle ~= nil and (vehicle.massKg or 0) > restriction.vehicleMassLimitKg
            elseif restriction.type == "Height Limit" and (restriction.vehicleHeightLimitM or 0) > 0 then
                applies = vehicle ~= nil and (vehicle.heightM or 0) > restriction.vehicleHeightLimitM
            end
            if applies then result[#result + 1] = restriction end
        end
    end
    return result
end

local function nodeById(id)
    return RacingGameplay.GetTrafficNode(id)
end

local function restrictionMatchesLink(restriction, link)
    if (restriction.linkId or 0) ~= 0 then return restriction.linkId == link.id end
    if (restriction.roadId or 0) == 0 then return false end
    local fromNode = nodeById(link.fromNodeId)
    local toNode = nodeById(link.toNodeId)
    return (fromNode and fromNode.roadId == restriction.roadId) or (toNode and toNode.roadId == restriction.roadId)
end

local function incidentMatchesLink(incident, link)
    if incident == nil or link == nil then return false end
    if (incident.linkId or 0) ~= 0 then return incident.linkId == link.id end
    local fromNode = nodeById(link.fromNodeId)
    local toNode = nodeById(link.toNodeId)
    if (incident.roadId or 0) ~= 0 then
        return (fromNode and fromNode.roadId == incident.roadId)
            or (toNode and toNode.roadId == incident.roadId)
    end
    if fromNode == nil and toNode == nil then return false end
    local ix, iz = incident.x or 0.0, incident.z or 0.0
    local radius = math.max(0.1, incident.radiusM or 20.0)
    local ax, az = fromNode and (fromNode.x or 0.0) or ix, fromNode and (fromNode.z or 0.0) or iz
    local bx, bz = toNode and (toNode.x or 0.0) or ax, toNode and (toNode.z or 0.0) or az
    local vx, vz = bx - ax, bz - az
    local denom = vx * vx + vz * vz
    local t = denom > 0.0001 and clamp(((ix - ax) * vx + (iz - az) * vz) / denom, 0.0, 1.0) or 0.0
    local px, pz = ax + vx * t, az + vz * t
    local dx, dz = ix - px, iz - pz
    return dx * dx + dz * dz <= radius * radius
end

function RacingTraffic.GetActiveIncidents(runtimeState)
    local result = {}
    local cleared = runtimeState and runtimeState.clearedIncidents or nil
    for _, incident in ipairs(RacingGameplay.GetActiveTrafficIncidents and RacingGameplay.GetActiveTrafficIncidents() or {}) do
        if not (cleared and cleared[incident.id]) then result[#result + 1] = incident end
    end
    if runtimeState then
        for _, incident in pairs(runtimeState.incidents or {}) do
            if incident.enabled ~= false and not (cleared and cleared[incident.id]) then result[#result + 1] = incident end
        end
    end
    return result
end

function RacingTraffic.ReportIncident(state, incident)
    if state == nil or incident == nil then return nil end
    state.incidents = state.incidents or {}
    state.nextIncidentId = state.nextIncidentId or 1000000
    local copy = {}
    for key, value in pairs(incident) do copy[key] = value end
    if copy.id == nil or copy.id == 0 then
        copy.id = state.nextIncidentId
        state.nextIncidentId = state.nextIncidentId + 1
    end
    copy.enabled = copy.enabled ~= false
    copy.ageSeconds = copy.ageSeconds or 0.0
    copy.clearAfterS = copy.clearAfterS or 300.0
    state.incidents[copy.id] = copy
    if state.clearedIncidents then state.clearedIncidents[copy.id] = nil end
    return copy.id
end

function RacingTraffic.ClearIncident(state, incidentId)
    if state == nil then return false end
    state.clearedIncidents = state.clearedIncidents or {}
    state.clearedIncidents[incidentId] = true
    if state.incidents then state.incidents[incidentId] = nil end
    return true
end

function RacingTraffic.ClearRuntimeIncidents(state)
    if state == nil then return end
    state.incidents = {}
end

function RacingTraffic.UpdateIncidents(state, dt)
    if state == nil then return end
    local delta = math.max(0.0, dt or 0.0)
    local expired = {}
    for id, incident in pairs(state.incidents or {}) do
        incident.ageSeconds = (incident.ageSeconds or 0.0) + delta
        if (incident.clearAfterS or 0.0) > 0.0 and incident.ageSeconds >= incident.clearAfterS then expired[#expired + 1] = id end
    end
    state.authoredIncidentAges = state.authoredIncidentAges or {}
    for _, incident in ipairs(RacingGameplay.GetActiveTrafficIncidents and RacingGameplay.GetActiveTrafficIncidents() or {}) do
        if not (state.clearedIncidents and state.clearedIncidents[incident.id]) then
            state.authoredIncidentAges[incident.id] = (state.authoredIncidentAges[incident.id] or 0.0) + delta
            if (incident.clearAfterS or 0.0) > 0.0 and state.authoredIncidentAges[incident.id] >= incident.clearAfterS then expired[#expired + 1] = incident.id end
        end
    end
    for _, id in ipairs(expired) do RacingTraffic.ClearIncident(state, id) end
end

function RacingTraffic.EvaluateLink(link, hour, vehicle, runtimeState)
    if link == nil or link.enabled == false then return nil end
    local emergency = vehicle and vehicle.emergency == true
    local cost = math.max(0.001, link.routeCostMultiplier or 1.0)
    local speedLimit = link.speedLimitKmh or 50.0
    local incidentId, incidentInfluence = nil, 0.0
    for _, restriction in ipairs(RacingTraffic.GetActiveRestrictions(hour, vehicle, runtimeState)) do
        if restrictionMatchesLink(restriction, link) then
            if restriction.blockTraffic and not (emergency and restriction.emergencyExempt) then return nil end
            cost = cost * math.max(0.001, restriction.routeCostMultiplier or 1.0)
            if (restriction.speedLimitKmh or 0) > 0 then speedLimit = math.min(speedLimit, restriction.speedLimitKmh) end
        end
    end
    for _, incident in ipairs(RacingTraffic.GetActiveIncidents(runtimeState)) do
        if incidentMatchesLink(incident, link) then
            local severity = clamp(incident.severity or 0.5, 0.0, 1.0)
            local blocked = clamp(incident.blockedLaneFraction or 0.0, 0.0, 1.0)
            if blocked >= 0.98 and not emergency then return nil end
            cost = cost * math.max(1.0, incident.routeCostMultiplier or (1.0 + severity * 2.0))
            if (incident.speedLimitKmh or 0.0) > 0.0 then speedLimit = math.min(speedLimit, incident.speedLimitKmh) end
            if severity >= incidentInfluence then incidentId, incidentInfluence = incident.id, severity end
        end
    end
    -- STUDIO28: authored cones can temporarily guide, slow, close a lane or
    -- close a road without duplicating the road graph. Event-scoped controls
    -- become active only while that event overlay is armed.
    if RacingConeCourse and RacingConeCourse.ApplyTrafficControls then
        local coneCost,coneSpeed,blockingCone=RacingConeCourse.ApplyTrafficControls(link,cost,speedLimit)
        if blockingCone~=nil and not emergency then return nil end
        if coneCost~=nil then cost=coneCost end
        if coneSpeed~=nil then speedLimit=coneSpeed end
    end
    return {
        link = link,
        costMultiplier = cost,
        speedLimitKmh = speedLimit,
        incidentId = incidentId,
        incidentInfluence = incidentInfluence
    }
end

function RacingTraffic.FindRoute(startNodeId, goalNodeId, options)
    options = options or {}
    if startNodeId == goalNodeId then return { startNodeId }, {}, 0.0 end
    local dist, previousNode, previousLink, open = {}, {}, {}, {}
    dist[startNodeId] = 0.0
    open[startNodeId] = true
    while true do
        local current, currentCost = nil, math.huge
        for nodeId in pairs(open) do
            local d = dist[nodeId] or math.huge
            if d < currentCost then current, currentCost = nodeId, d end
        end
        if current == nil then break end
        open[current] = nil
        if current == goalNodeId then break end
        for _, link in ipairs(RacingGameplay.GetOutgoingRoadLinks(current)) do
            local from = link.fromNodeId
            local to = link.toNodeId
            if link.bidirectional and link.toNodeId == current then from, to = link.toNodeId, link.fromNodeId end
            if from == current then
                local evaluated = RacingTraffic.EvaluateLink(link, options.hour, options.vehicle, options.runtimeState)
                if evaluated then
                    local a, b = nodeById(current), nodeById(to)
                    local dx, dy, dz = 0.0, 0.0, 0.0
                    if a and b then dx=(b.x or 0)-(a.x or 0); dy=(b.y or 0)-(a.y or 0); dz=(b.z or 0)-(a.z or 0) end
                    local length = math.sqrt(dx*dx + dy*dy + dz*dz)
                    local travelCost = math.max(1.0, length) * evaluated.costMultiplier
                    local nd = currentCost + travelCost
                    if nd < (dist[to] or math.huge) then
                        dist[to] = nd; previousNode[to] = current; previousLink[to] = link.id; open[to] = true
                    end
                end
            end
        end
    end
    if dist[goalNodeId] == nil then return nil, nil, math.huge end
    local nodes, links = {}, {}
    local cursor = goalNodeId
    while cursor do
        table.insert(nodes, 1, cursor)
        local linkId = previousLink[cursor]
        if linkId then table.insert(links, 1, linkId) end
        cursor = previousNode[cursor]
    end
    return nodes, links, dist[goalNodeId]
end

function RacingTraffic.CreateRuntimeState()
    return { timeSeconds = 0.0, signals = {}, reservations = {}, restrictionOverrides = {}, incidents = {}, clearedIncidents = {}, authoredIncidentAges = {}, nextIncidentId = 1000000 }
end

function RacingTraffic.SetRestrictionEnabled(state, restrictionId, enabled)
    if state == nil then return end
    state.restrictionOverrides = state.restrictionOverrides or {}
    state.restrictionOverrides[restrictionId] = enabled == true
end

function RacingTraffic.UpdateSignals(state, dt, queueLengths, emergencyRequests)
    if state == nil then return end
    state.timeSeconds = (state.timeSeconds or 0.0) + math.max(0.0, dt or 0.0)
    state.signals = state.signals or {}
    for _, intersection in ipairs(RacingGameplay.data.roadIntersections or {}) do
        if intersection.trafficLights then
            local phases = RacingGameplay.GetSignalPhases(intersection.id)
            if #phases > 0 then
                local controller = RacingTraffic.GetIntersectionController(intersection.id) or {}
                local signal = state.signals[intersection.id] or { phaseIndex = 1, phaseTime = controller.phaseOffsetSeconds or 0.0, stage = "green" }
                signal.phaseIndex = clamp(signal.phaseIndex or 1, 1, #phases)
                local phase = phases[signal.phaseIndex]
                local green = phase.greenSeconds or 25.0
                if controller.mode == "Actuated" or controller.mode == "Adaptive" then
                    local q = queueLengths and (queueLengths[intersection.id] or 0) or 0
                    green = clamp(green + q * 0.75, controller.minimumGreenSeconds or 8.0, controller.maximumGreenSeconds or 50.0)
                end
                if controller.emergencyPreemption and emergencyRequests and emergencyRequests[intersection.id] then green = math.max(green, controller.maximumGreenSeconds or 50.0) end
                signal.phaseTime = (signal.phaseTime or 0.0) + math.max(0.0, dt or 0.0)
                local yellow = phase.yellowSeconds or 3.0
                local allRed = phase.allRedSeconds or 1.0
                local cycle = green + yellow + allRed
                if signal.phaseTime >= cycle then signal.phaseTime = signal.phaseTime - cycle; signal.phaseIndex = signal.phaseIndex % #phases + 1; phase = phases[signal.phaseIndex] end
                if signal.phaseTime < green then signal.stage = "green" elseif signal.phaseTime < green + yellow then signal.stage = "yellow" else signal.stage = "all_red" end
                signal.phase = phase
                state.signals[intersection.id] = signal
            end
        end
    end
end

function RacingTraffic.TryReserveIntersection(state, connectorId, vehicleId, nowSeconds)
    if state == nil then return false, "no state" end
    local connector = nil
    for _, candidate in ipairs(RacingGameplay.data.turnConnectors or {}) do if candidate.id == connectorId then connector = candidate break end end
    if connector == nil or connector.enabled == false then return false, "connector unavailable" end
    local group = connector.conflictGroup or 0
    state.reservations = state.reservations or {}
    local now = nowSeconds or state.timeSeconds or 0.0
    for i = #state.reservations, 1, -1 do if (state.reservations[i].expiresAt or 0) <= now then table.remove(state.reservations, i) end end
    if group ~= 0 then
        for _, reservation in ipairs(state.reservations) do
            if reservation.intersectionId == connector.intersectionId and reservation.conflictGroup == group and reservation.vehicleId ~= vehicleId then return false, "conflict reserved" end
        end
    end
    state.reservations[#state.reservations + 1] = { intersectionId=connector.intersectionId, connectorId=connector.id, conflictGroup=group, vehicleId=vehicleId, expiresAt=now + (connector.reservationSeconds or 2.5) }
    return true
end

function RacingTraffic.ShouldYieldForEmergency(distanceM, vehicleIsEmergency)
    if vehicleIsEmergency then return false end
    local rules = RacingTraffic.GetRules()
    return rules.emergencyCorridor == true and distanceM <= (rules.emergencyYieldRadiusM or 80.0)
end
