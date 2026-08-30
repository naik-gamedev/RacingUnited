-- Racing United runtime facade for Heritage Studio-authored gameplay/venue data.
-- Generated/StudioGameplay.lua is loaded immediately before this file by Main.lua.

RacingGameplay = RacingGameplay or {}
RacingGameplay.data = StudioGameplay or {
    version = 18,
    race = {}, raceMarkers = {}, broadcastCameraPaths = {}, broadcastCameraNodes = {}, coneCourse = {}, courseCones = {}, coneCourseGates = {}, raceRoutes = {}, raceRouteNodes = {}, raceLayouts = {},
    raceSessions = {}, raceControl = {}, raceSupportPoints = {},
    trafficNodes = {}, trafficLinks = {}, roadSplines = {}, roadSplineNodes = {}, roadIntersections = {},
    turnConnectors = {}, trafficSignalPhases = {}, parkingStrips = {}, trafficPopulation = {}, navigationBuild = {},
    trafficRules = {}, trafficStreaming = {}, intersectionControllers = {}, roadRestrictions = {},
    trafficAgentSimulation = {}, trafficAgentProfiles = {},
    trafficSpawnPortals = {}, trafficDensityRegions = {}, trafficIncidents = {},
    trafficEnvironment = {}, trafficBehavior = {}, trafficDebug = {},
    events = {}, eventExecution = {}, worldPoints = {}, motorsport = {}, motorsportAi = {}, motorsportReplay = {}, motorsportClasses = {}, motorsportEntrants = {}, motorsportChampionships = {}, motorsportRounds = {}
}

local function FindById(collection, id)
    if collection == nil then return nil end
    for _, value in ipairs(collection) do
        if value.id == id then return value end
    end
    return nil
end

local function FindByName(collection, name)
    if collection == nil or name == nil then return nil end
    for _, value in ipairs(collection) do
        if value.name == name then return value end
    end
    return nil
end

local function FindByIdOrName(collection, idOrName)
    if type(idOrName) == "number" then return FindById(collection, idOrName) end
    return FindByName(collection, idOrName)
end

local function CopySorted(collection, predicate, reverse)
    local result = {}
    for _, value in ipairs(collection or {}) do
        if predicate == nil or predicate(value) then result[#result + 1] = value end
    end
    table.sort(result, function(a, b)
        local ao = a.order or 0
        local bo = b.order or 0
        if reverse then return ao > bo end
        return ao < bo
    end)
    return result
end

function RacingGameplay.GetRaceConfiguration()
    return RacingGameplay.data.race or {}
end

function RacingGameplay.GetRaceControl()
    return RacingGameplay.data.raceControl or {}
end

function RacingGameplay.GetRaceMarker(idOrName)
    return FindByIdOrName(RacingGameplay.data.raceMarkers, idOrName)
end

function RacingGameplay.MarkerAppliesToLayout(marker, layoutId)
    if marker == nil then return false end
    local markerLayoutId = marker.layoutId or 0
    local requestedLayoutId = layoutId or 0
    return markerLayoutId == 0 or (requestedLayoutId ~= 0 and markerLayoutId == requestedLayoutId)
end

function RacingGameplay.GetMarkersByType(markerType, layoutId)
    local result = {}
    for _, marker in ipairs(RacingGameplay.data.raceMarkers or {}) do
        if marker.type == markerType and RacingGameplay.MarkerAppliesToLayout(marker, layoutId) then
            result[#result + 1] = marker
        end
    end
    table.sort(result, function(a, b) return (a.order or 0) < (b.order or 0) end)
    return result
end

function RacingGameplay.GetBroadcastCameraPaths(layoutId)
    local result = {}
    for _, path in ipairs(RacingGameplay.data.broadcastCameraPaths or {}) do
        local pathLayoutId = path.layoutId or 0
        if path.enabled ~= false and (pathLayoutId == 0 or ((layoutId or 0) ~= 0 and pathLayoutId == layoutId)) then
            result[#result + 1] = path
        end
    end
    return result
end

function RacingGameplay.GetBroadcastCameraPathNodes(pathId)
    local result = {}
    for _, node in ipairs(RacingGameplay.data.broadcastCameraNodes or {}) do
        if node.pathId == pathId then result[#result + 1] = node end
    end
    table.sort(result, function(a, b) return (a.order or 0) < (b.order or 0) end)
    return result
end

function RacingGameplay.GetConeCourseConfiguration()
    return RacingGameplay.data.coneCourse or { enabled=true, defaultAssetPath="Assets/Props/TrafficCone.glb", minimumContactImpulseNs=1.0, defaultHitPenaltySeconds=2.0, defaultDisplacementToleranceM=0.12, wrongElementPenaltySeconds=10.0, missedElementDnf=true, resetEventConesOnStart=true, recordConeHitsToReplay=true, eventConesVisibleOnlyWhileActive=true }
end

function RacingGameplay.GetCourseCone(idOrName)
    return FindByIdOrName(RacingGameplay.data.courseCones, idOrName)
end

function RacingGameplay.ConeAppliesToEvent(cone, eventId, includePersistent)
    if cone == nil or cone.enabled == false then return false end
    local authored = cone.eventId or 0
    if authored == 0 then return includePersistent ~= false end
    return (eventId or 0) ~= 0 and authored == eventId
end

function RacingGameplay.GetCourseCones(eventId, includePersistent)
    local result = {}
    for _, cone in ipairs(RacingGameplay.data.courseCones or {}) do
        if RacingGameplay.ConeAppliesToEvent(cone, eventId, includePersistent) then result[#result + 1] = cone end
    end
    return result
end

function RacingGameplay.GetConeCourseGates(eventId)
    return CopySorted(RacingGameplay.data.coneCourseGates, function(gate)
        return gate.enabled ~= false and (eventId == nil or eventId == 0 or (gate.eventId or 0) == eventId)
    end, false)
end

function RacingGameplay.GetCheckpointSequence(layoutId)
    local result = {}
    for _, marker in ipairs(RacingGameplay.data.raceMarkers or {}) do
        if RacingGameplay.MarkerAppliesToLayout(marker, layoutId)
            and (marker.type == "Start / Finish" or marker.type == "Checkpoint" or marker.type == "Sector" or marker.type == "Timing Loop") then
            result[#result + 1] = marker
        end
    end
    table.sort(result, function(a, b) return (a.order or 0) < (b.order or 0) end)
    return result
end

function RacingGameplay.GetRaceRoute(idOrName)
    return FindByIdOrName(RacingGameplay.data.raceRoutes, idOrName)
end

function RacingGameplay.GetRouteNodes(routeIdOrName, reverse)
    local route = RacingGameplay.GetRaceRoute(routeIdOrName)
    if route == nil then return {} end
    return CopySorted(RacingGameplay.data.raceRouteNodes, function(node) return node.routeId == route.id end, reverse == true)
end

function RacingGameplay.GetRaceLayout(idOrName)
    return FindByIdOrName(RacingGameplay.data.raceLayouts, idOrName)
end

function RacingGameplay.GetEnabledLayouts()
    local result = {}
    for _, layout in ipairs(RacingGameplay.data.raceLayouts or {}) do
        if layout.enabled then result[#result + 1] = layout end
    end
    return result
end

function RacingGameplay.ResolveLayout(layoutOrIdOrName)
    local layout = layoutOrIdOrName
    if type(layoutOrIdOrName) ~= "table" then layout = RacingGameplay.GetRaceLayout(layoutOrIdOrName) end
    if layout == nil then return nil end
    local startFinish = RacingGameplay.GetRaceMarker(layout.startFinishMarkerId)
    if not RacingGameplay.MarkerAppliesToLayout(startFinish, layout.id) then startFinish = nil end
    return {
        layout = layout,
        route = RacingGameplay.GetRaceRoute(layout.routeId),
        routeNodes = RacingGameplay.GetRouteNodes(layout.routeId, layout.reverse),
        pitRoute = RacingGameplay.GetRaceRoute(layout.pitRouteId),
        pitRouteNodes = RacingGameplay.GetRouteNodes(layout.pitRouteId, false),
        startFinish = startFinish,
        markers = RacingGameplay.GetCheckpointSequence(layout.id)
    }
end

function RacingGameplay.GetSessionChain()
    return CopySorted(RacingGameplay.data.raceSessions, function(session) return session.enabled end, false)
end

function RacingGameplay.GetRaceSupportPointsByType(pointType)
    local result = {}
    for _, point in ipairs(RacingGameplay.data.raceSupportPoints or {}) do
        if point.enabled and (pointType == nil or point.type == pointType) then result[#result + 1] = point end
    end
    return result
end

function RacingGameplay.GetMotorsportConfiguration() return RacingGameplay.data.motorsport or {} end
function RacingGameplay.GetMotorsportAiConfiguration() return RacingGameplay.data.motorsportAi or {} end
function RacingGameplay.GetMotorsportReplayConfiguration()
    return RacingGameplay.data.motorsportReplay or { enabled=true, sampleHz=12.0, preRollSeconds=8.0, postRollSeconds=5.0, maximumIncidentClips=12, maximumRecordedCompetitors=32, capturePlayer=true, captureControls=true, ghostReviewEnabled=true, maximumGhostVehicles=16, broadcastDirectorEnabled=true, autoIncidentCamera=true, incidentCameraDistanceM=13.0, incidentCameraHeightM=4.5, tracksideCameraLeadM=22.0, helicopterCameraHeightM=28.0, cameraSmoothing=9.0 }
end
function RacingGameplay.GetMotorsportClass(idOrName) return FindByIdOrName(RacingGameplay.data.motorsportClasses, idOrName) end
function RacingGameplay.GetMotorsportEntrant(idOrName)
    if type(idOrName)=="number" then return FindById(RacingGameplay.data.motorsportEntrants,idOrName) end
    for _,v in ipairs(RacingGameplay.data.motorsportEntrants or {}) do if v.driverName==idOrName then return v end end
    return nil
end
function RacingGameplay.GetEntrantsForEvent(eventId, classId)
    local result={}
    for _,entrant in ipairs(RacingGameplay.data.motorsportEntrants or {}) do
        if entrant.enabled and ((entrant.eventId or 0)==0 or entrant.eventId==eventId) and (classId==nil or classId==0 or (entrant.classId or 0)==classId) then result[#result+1]=entrant end
    end
    table.sort(result,function(a,b)
        local ga=(a.gridOverride or 0)>0 and a.gridOverride or 9999; local gb=(b.gridOverride or 0)>0 and b.gridOverride or 9999
        if ga~=gb then return ga<gb end return (a.raceNumber or 9999)<(b.raceNumber or 9999)
    end)
    return result
end
function RacingGameplay.GetChampionship(idOrName) return FindByIdOrName(RacingGameplay.data.motorsportChampionships,idOrName) end
function RacingGameplay.GetChampionshipRounds(championshipId) return CopySorted(RacingGameplay.data.motorsportRounds,function(r) return r.enabled and r.championshipId==championshipId end,false) end

function RacingGameplay.GetTrafficNode(id)
    return FindById(RacingGameplay.data.trafficNodes, id)
end

function RacingGameplay.GetOutgoingRoadLinks(nodeId)
    local result = {}
    for _, link in ipairs(RacingGameplay.data.trafficLinks or {}) do
        if link.fromNodeId == nodeId or (link.bidirectional and link.toNodeId == nodeId) then result[#result + 1] = link end
    end
    return result
end


function RacingGameplay.GetRoad(idOrName)
    return FindByIdOrName(RacingGameplay.data.roadSplines, idOrName)
end

function RacingGameplay.GetRoadNodes(roadIdOrName)
    local road = RacingGameplay.GetRoad(roadIdOrName)
    if road == nil then return {} end
    return CopySorted(RacingGameplay.data.roadSplineNodes, function(node) return node.roadId == road.id end, false)
end

function RacingGameplay.GetRoadsByClass(roadClass)
    local result = {}
    for _, road in ipairs(RacingGameplay.data.roadSplines or {}) do
        if road.enabled and (roadClass == nil or road.class == roadClass) then result[#result + 1] = road end
    end
    return result
end


function RacingGameplay.GetRoadLaneDescriptors(roadIdOrName)
    local road = RacingGameplay.GetRoad(roadIdOrName)
    if road == nil then return {} end
    local result = {}
    local laneWidth = road.laneWidthM or 3.25
    local medianHalf = (road.medianWidthM or 0.0) * 0.5
    local forwardCount = math.max(0, road.lanesForward or 0)
    local backwardCount = road.oneWay and 0 or math.max(0, road.lanesBackward or 0)
    for lane = 1, forwardCount do
        result[#result + 1] = { roadId = road.id, direction = 1, lane = lane, lateralOffsetM = medianHalf + laneWidth * (lane - 0.5) }
    end
    for lane = 1, backwardCount do
        result[#result + 1] = { roadId = road.id, direction = -1, lane = lane, lateralOffsetM = -(medianHalf + laneWidth * (lane - 0.5)) }
    end
    return result
end

function RacingGameplay.GetRoadNetworkSummary()
    local laneCount = 0
    for _, road in ipairs(RacingGameplay.data.roadSplines or {}) do
        if road.enabled then laneCount = laneCount + #(RacingGameplay.GetRoadLaneDescriptors(road.id)) end
    end
    return {
        roads = #(RacingGameplay.data.roadSplines or {}),
        controlNodes = #(RacingGameplay.data.roadSplineNodes or {}),
        generatedLanes = laneCount,
        intersections = #(RacingGameplay.data.roadIntersections or {}),
        turnConnectors = #(RacingGameplay.data.turnConnectors or {}),
        parkingStrips = #(RacingGameplay.data.parkingStrips or {})
    }
end

function RacingGameplay.GetIntersection(idOrName)
    return FindByIdOrName(RacingGameplay.data.roadIntersections, idOrName)
end

function RacingGameplay.GetTurnConnectors(intersectionId, fromRoadId, toRoadId)
    local result = {}
    for _, connector in ipairs(RacingGameplay.data.turnConnectors or {}) do
        if connector.enabled and (intersectionId == nil or connector.intersectionId == intersectionId)
            and (fromRoadId == nil or connector.fromRoadId == fromRoadId)
            and (toRoadId == nil or connector.toRoadId == toRoadId) then
            result[#result + 1] = connector
        end
    end
    return result
end

function RacingGameplay.GetSignalPhases(intersectionId)
    return CopySorted(RacingGameplay.data.trafficSignalPhases, function(phase) return phase.intersectionId == intersectionId end, false)
end

function RacingGameplay.GetParkingStrips(roadId)
    local result = {}
    for _, parking in ipairs(RacingGameplay.data.parkingStrips or {}) do
        if roadId == nil or parking.roadId == roadId then result[#result + 1] = parking end
    end
    return result
end

function RacingGameplay.GetTrafficPopulationConfiguration()
    return RacingGameplay.data.trafficPopulation or {}
end

function RacingGameplay.GetNavigationBuildConfiguration()
    return RacingGameplay.data.navigationBuild or {}
end

function RacingGameplay.GetTrafficAgentSimulationConfiguration()
    return RacingGameplay.data.trafficAgentSimulation or {}
end

function RacingGameplay.GetTrafficAgentProfile(idOrName)
    return FindByIdOrName(RacingGameplay.data.trafficAgentProfiles, idOrName)
end

function RacingGameplay.GetEnabledTrafficAgentProfiles(vehicleClass)
    local result = {}
    for _, profile in ipairs(RacingGameplay.data.trafficAgentProfiles or {}) do
        if profile.enabled and (vehicleClass == nil or profile.class == vehicleClass) then
            result[#result + 1] = profile
        end
    end
    return result
end

function RacingGameplay.GetTrafficAgentProfileWeightTotal(vehicleClass)
    local total = 0.0
    for _, profile in ipairs(RacingGameplay.GetEnabledTrafficAgentProfiles(vehicleClass)) do
        total = total + math.max(0.0, profile.spawnWeight or 0.0)
    end
    return total
end

local function TrafficHourActive(startHour, endHour, hour)
    local startValue = tonumber(startHour) or 0.0
    local endValue = tonumber(endHour) or 24.0
    local now = tonumber(hour) or 12.0
    if startValue == endValue then return true end
    if startValue < endValue then return now >= startValue and now < endValue end
    return now >= startValue or now < endValue
end

function RacingGameplay.GetTrafficSpawnPortal(idOrName)
    return FindByIdOrName(RacingGameplay.data.trafficSpawnPortals, idOrName)
end

function RacingGameplay.GetActiveTrafficSpawnPortals(hour, mode)
    local result = {}
    for _, portal in ipairs(RacingGameplay.data.trafficSpawnPortals or {}) do
        local modeAllowed = mode == nil or portal.mode == mode or portal.mode == "Spawn + Despawn"
        if portal.enabled and modeAllowed and TrafficHourActive(portal.startHour, portal.endHour, hour) then result[#result + 1] = portal end
    end
    return result
end

function RacingGameplay.GetTrafficDensityAt(x, y, z, hour)
    local result = { densityMultiplier = 1.0, speedMultiplier = 1.0, laneChangeAggressionOffset = 0.0, parkingMultiplier = 1.0, regionCount = 0 }
    for _, region in ipairs(RacingGameplay.data.trafficDensityRegions or {}) do
        if region.enabled and TrafficHourActive(region.startHour, region.endHour, hour) then
            local dx = (region.x or 0.0) - x
            local dz = (region.z or 0.0) - z
            local radius = math.max(0.1, region.radiusM or 250.0)
            local distance = math.sqrt(dx * dx + dz * dz)
            if distance <= radius then
                local influence = 1.0 - math.min(1.0, distance / radius)
                result.densityMultiplier = result.densityMultiplier * (1.0 + ((region.densityMultiplier or 1.0) - 1.0) * influence)
                result.speedMultiplier = result.speedMultiplier * (1.0 + ((region.speedMultiplier or 1.0) - 1.0) * influence)
                result.laneChangeAggressionOffset = result.laneChangeAggressionOffset + (region.laneChangeAggressionOffset or 0.0) * influence
                result.parkingMultiplier = result.parkingMultiplier * (1.0 + ((region.parkingMultiplier or 1.0) - 1.0) * influence)
                result.regionCount = result.regionCount + 1
            end
        end
    end
    return result
end

function RacingGameplay.GetTrafficIncident(idOrName)
    return FindByIdOrName(RacingGameplay.data.trafficIncidents, idOrName)
end

function RacingGameplay.GetActiveTrafficIncidents()
    local result = {}
    for _, incident in ipairs(RacingGameplay.data.trafficIncidents or {}) do
        if incident.enabled then result[#result + 1] = incident end
    end
    return result
end

function RacingGameplay.GetTrafficEnvironmentConfiguration()
    return RacingGameplay.data.trafficEnvironment or {}
end

function RacingGameplay.GetTrafficBehaviorConfiguration()
    return RacingGameplay.data.trafficBehavior or {}
end

function RacingGameplay.GetTrafficDebugConfiguration()
    return RacingGameplay.data.trafficDebug or {}
end

function RacingGameplay.GetNearestTrafficPortal(x, y, z, mode, hour)
    local best = nil
    local bestDistanceSq = math.huge
    for _, portal in ipairs(RacingGameplay.GetActiveTrafficSpawnPortals(hour, mode)) do
        local dx = (portal.x or 0.0) - x
        local dy = (portal.y or 0.0) - y
        local dz = (portal.z or 0.0) - z
        local distanceSq = dx * dx + dy * dy + dz * dz
        if distanceSq < bestDistanceSq then bestDistanceSq = distanceSq; best = portal end
    end
    if best == nil then return nil, math.huge end
    return best, math.sqrt(bestDistanceSq)
end

function RacingGameplay.GetNearestRoadNode(x, y, z, roadIdOrName)
    local nodes = RacingGameplay.data.roadSplineNodes or {}
    if roadIdOrName ~= nil then nodes = RacingGameplay.GetRoadNodes(roadIdOrName) end
    local best = nil
    local bestDistanceSq = math.huge
    for _, node in ipairs(nodes) do
        local dx = (node.x or 0.0) - x
        local dy = (node.y or 0.0) - y
        local dz = (node.z or 0.0) - z
        local distanceSq = dx * dx + dy * dy + dz * dz
        if distanceSq < bestDistanceSq then bestDistanceSq = distanceSq; best = node end
    end
    if best == nil then return nil, math.huge end
    return best, math.sqrt(bestDistanceSq)
end

function RacingGameplay.GetEvent(idOrName)
    return FindByIdOrName(RacingGameplay.data.events, idOrName)
end

function RacingGameplay.GetEventExecutionConfiguration()
    return RacingGameplay.data.eventExecution or {}
end

function RacingGameplay.GetEnabledEvents()
    local result = {}
    for _, event in ipairs(RacingGameplay.data.events or {}) do
        if event.enabled then result[#result + 1] = event end
    end
    return result
end

function RacingGameplay.ResolveEventRoute(event)
    if event == nil then return nil, nil end
    local layoutId = event.layoutId or 0
    local startMarker = RacingGameplay.GetRaceMarker(event.startMarkerId)
    local finishMarker = RacingGameplay.GetRaceMarker(event.finishMarkerId)
    if not RacingGameplay.MarkerAppliesToLayout(startMarker, layoutId) then startMarker = nil end
    if not RacingGameplay.MarkerAppliesToLayout(finishMarker, layoutId) then finishMarker = nil end
    local resolvedLayout = RacingGameplay.ResolveLayout(layoutId)
    if (event.startMarkerId or 0) == 0 and startMarker == nil and resolvedLayout ~= nil then startMarker = resolvedLayout.startFinish end
    return startMarker, finishMarker
end

function RacingGameplay.ResolveEventVenue(eventOrIdOrName)
    local event = eventOrIdOrName
    if type(eventOrIdOrName) ~= "table" then event = RacingGameplay.GetEvent(eventOrIdOrName) end
    if event == nil then return nil end
    local startMarker, finishMarker = RacingGameplay.ResolveEventRoute(event)
    return {
        event = event,
        startMarker = startMarker,
        finishMarker = finishMarker,
        venue = RacingGameplay.ResolveLayout(event.layoutId),
        sessions = RacingGameplay.GetSessionChain(),
        raceControl = RacingGameplay.GetRaceControl()
    }
end

function RacingGameplay.GetWorldPoint(idOrName)
    return FindByIdOrName(RacingGameplay.data.worldPoints, idOrName)
end

function RacingGameplay.GetWorldPointsByType(pointType)
    local result = {}
    for _, point in ipairs(RacingGameplay.data.worldPoints or {}) do
        if point.enabled and point.type == pointType then result[#result + 1] = point end
    end
    return result
end

function RacingGameplay.GetNearestWorldPoint(x, y, z, pointType)
    local best = nil
    local bestDistanceSq = math.huge
    for _, point in ipairs(RacingGameplay.data.worldPoints or {}) do
        if point.enabled and (pointType == nil or point.type == pointType) then
            local dx = (point.x or 0.0) - x
            local dy = (point.y or 0.0) - y
            local dz = (point.z or 0.0) - z
            local distanceSq = dx * dx + dy * dy + dz * dz
            if distanceSq < bestDistanceSq then bestDistanceSq = distanceSq; best = point end
        end
    end
    if best == nil then return nil, math.huge end
    return best, math.sqrt(bestDistanceSq)
end

function RacingGameplay.GetNearestRouteNode(x, y, z, routeIdOrName)
    local best = nil
    local bestDistanceSq = math.huge
    for _, node in ipairs(RacingGameplay.GetRouteNodes(routeIdOrName, false)) do
        local dx = (node.x or 0.0) - x
        local dy = (node.y or 0.0) - y
        local dz = (node.z or 0.0) - z
        local distanceSq = dx * dx + dy * dy + dz * dz
        if distanceSq < bestDistanceSq then bestDistanceSq = distanceSq; best = node end
    end
    if best == nil then return nil, math.huge end
    return best, math.sqrt(bestDistanceSq)
end

-- STUDIO17 police / clandestine free-roam gameplay facade.
function RacingGameplay.GetPoliceGameplayConfiguration()
    return RacingGameplay.data.policeGameplay or {
        enabled = false, maxHeatLevel = 5, maxPursuitUnits = 12,
        civilianWitnessRadiusM = 120.0, policeDetectionRadiusM = 180.0,
        speedToleranceKmh = 12.0, heatDecayDelayS = 20.0, heatDecayPerSecond = 0.035,
        lostSightSeconds = 12.0, searchDurationS = 90.0, cooldownDurationS = 30.0,
        bustHoldSeconds = 5.0, backupDelayS = 4.0, roadblockMinimumHeat = 3.0,
        civilianWitnesses = true, speedingGeneratesHeat = true,
        collisionsGenerateHeat = true, illegalRacesGenerateHeat = true,
        evasionEscalatesHeat = true
    }
end

function RacingGameplay.GetPolicePatrolZones()
    return RacingGameplay.data.policePatrolZones or {}
end

function RacingGameplay.GetPoliceRoadblockSites()
    return RacingGameplay.data.policeRoadblockSites or {}
end

function RacingGameplay.GetPoliceEscapeZones()
    return RacingGameplay.data.policeEscapeZones or {}
end

function RacingGameplay.GetClandestineMeets()
    return RacingGameplay.data.clandestineMeets or {}
end

function RacingGameplay.GetPolicePatrolCoverageAt(x, y, z, hour)
    local result = { weight = 0.0, responseMultiplier = 1.0, speedToleranceKmh = nil, maximumUnits = 0, zones = {} }
    for _, zone in ipairs(RacingGameplay.GetPolicePatrolZones()) do
        if zone.enabled and TrafficHourActive(zone.startHour, zone.endHour, hour) then
            local dx = (zone.x or 0.0) - (x or 0.0)
            local dz = (zone.z or 0.0) - (z or 0.0)
            local radius = math.max(1.0, zone.radiusM or 600.0)
            local distance = math.sqrt(dx * dx + dz * dz)
            if distance <= radius then
                local influence = 1.0 - math.min(1.0, distance / radius)
                result.weight = result.weight + math.max(0.0, zone.patrolWeight or 1.0) * influence
                result.responseMultiplier = math.max(result.responseMultiplier, zone.responseMultiplier or 1.0)
                local tolerance = zone.speedToleranceKmh or 10.0
                result.speedToleranceKmh = result.speedToleranceKmh == nil and tolerance or math.min(result.speedToleranceKmh, tolerance)
                result.maximumUnits = math.max(result.maximumUnits, zone.maximumUnits or 0)
                result.zones[#result.zones + 1] = zone
            end
        end
    end
    return result
end

function RacingGameplay.GetPoliceEscapeInfluenceAt(x, y, z)
    local result = { active = false, heatDecayMultiplier = 1.0, searchTimeMultiplier = 1.0, breakLineOfSight = false, safehouse = false, zones = {} }
    for _, zone in ipairs(RacingGameplay.GetPoliceEscapeZones()) do
        if zone.enabled then
            local dx = (zone.x or 0.0) - (x or 0.0)
            local dz = (zone.z or 0.0) - (z or 0.0)
            local radius = math.max(1.0, zone.radiusM or 180.0)
            if dx * dx + dz * dz <= radius * radius then
                result.active = true
                result.heatDecayMultiplier = math.max(result.heatDecayMultiplier, zone.heatDecayMultiplier or 1.0)
                result.searchTimeMultiplier = math.min(result.searchTimeMultiplier, zone.searchTimeMultiplier or 1.0)
                result.breakLineOfSight = result.breakLineOfSight or zone.breakLineOfSight == true
                result.safehouse = result.safehouse or zone.safehouse == true
                result.zones[#result.zones + 1] = zone
            end
        end
    end
    return result
end

function RacingGameplay.GetOpenClandestineMeets(hour)
    local result = {}
    for _, meet in ipairs(RacingGameplay.GetClandestineMeets()) do
        if meet.enabled and TrafficHourActive(meet.openHour, meet.closeHour, hour) then result[#result + 1] = meet end
    end
    return result
end
