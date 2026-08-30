local trafficDebugMessage = ""
local trafficDebugSelectedAgent = 0

function DrawPrototypeTrafficPanel()
    UI.TextDisabled("LIVE TRAFFIC / NAVIGATION")
    UI.TextWrapped("STUDIO16 inspects live traffic decisions: routes, merges, roundabouts, stop/yield behavior, overtaking, queue discharge, parking, recovery, collisions, incidents and streaming tiers.")
    UI.Spacing()

    local config = RacingTrafficAgents.GetConfiguration()
    local manager = RacingTrafficAgents.manager
    local telemetry = RacingTrafficAgents.GetTelemetry(manager)
    UI.Text(string.format(
        "Agents %d | Full %d | Simplified %d | Dormant %d | Waiting %d",
        telemetry.active or 0, telemetry.full or 0, telemetry.simplified or 0,
        telemetry.dormant or 0, telemetry.waiting or 0))
    UI.Text(string.format(
        "Portals %d | Active incidents %d | Collisions %d | Local density x%.2f | Vehicle backend: %s",
        telemetry.portals or 0, telemetry.incidents or 0, telemetry.collisions or 0,
        telemetry.localDensityMultiplier or 1.0,
        RacingTrafficAgents.vehicleFactoryMode or "not started"))
    UI.Text(string.format(
        "Live traffic: %s | Heritage full vehicle dynamics: %s | Full-agent cap: %d",
        config.enabled and "ON" or "OFF",
        config.useHeritageVehicleDynamics and "ON" or "OFF",
        config.maxFullPhysicsAgents or 0))
    UI.Spacing()

    if UI.Button("SPAWN ONE AGENT", 180.0, 28.0, false) then
        local _, message = RacingTrafficAgents.DebugSpawnOne()
        trafficDebugMessage = message or "Spawn request failed"
    end
    UI.SameLine()
    if UI.Button("CLEAR RUNTIME INCIDENTS", 220.0, 28.0, false) then
        RacingTrafficAgents.DebugClearRuntimeIncidents()
        trafficDebugMessage = "Cleared runtime incidents; authored incidents remain."
    end
    if trafficDebugMessage ~= "" then UI.TextWrapped(trafficDebugMessage) end
    UI.Separator()

    local debug = RacingGameplay.GetTrafficDebugConfiguration()
    local details = RacingTrafficAgents.GetDebugSnapshot(manager, debug.maxDetailedAgents or 48)
    if #details == 0 then
        UI.TextDisabled("No live traffic agents. Enable live traffic in Heritage Studio or use SPAWN ONE AGENT for a manual debug spawn.")
        return
    end

    UI.TextDisabled("AGENT TELEMETRY")
    for _, agent in ipairs(details) do
        local selected = trafficDebugSelectedAgent == agent.id
        local label = string.format("#%d  %s  %.1f km/h  [%s]%s",
            agent.id, agent.profile or "Traffic", agent.speedKmh or 0.0,
            agent.streamingTier or "?", agent.waitReason and ("  - " .. agent.waitReason) or "")
        if UI.Button(label, UI.GetAvailableWidth(), 26.0, selected) then
            trafficDebugSelectedAgent = agent.id
            RacingTrafficAgents.SetDebugSelectedAgent(agent.id)
        end
    end

    if trafficDebugSelectedAgent ~= 0 then
        local agent = RacingTrafficAgents.GetAgentTelemetry(manager, trafficDebugSelectedAgent)
        if agent ~= nil then
            UI.Separator()
            UI.Text(string.format("SELECTED AGENT #%d / %s / %s", agent.id, agent.profile or "?", agent.class or "?"))
            UI.Text(string.format("Speed %.1f -> %.1f km/h | accel %.2f m/s2", agent.speedKmh or 0.0, agent.desiredSpeedKmh or 0.0, agent.accelerationMps2 or 0.0))
            UI.Text(string.format("Route node %s -> goal %s | %d / %d", tostring(agent.currentNodeId), tostring(agent.goalNodeId), agent.routeIndex or 0, agent.routeLength or 0))
            UI.Text(string.format("Weather speed x%.2f | following gap x%.2f | density regions %d", agent.environmentSpeedFactor or 1.0, agent.environmentFollowingGapFactor or 1.0, agent.densityRegionCount or 0))
            UI.Text(string.format("Spawn portal %s | destination portal %s | incident %s (%.2f)", tostring(agent.sourcePortalId or "-"), tostring(agent.destinationPortalId or "-"), tostring(agent.incidentInfluenceId or "-"), agent.incidentInfluence or 0.0))
            UI.Text(string.format("Decision: %s | lane score %.3f | reason %s", tostring(agent.decision or "follow route"), agent.laneChangeScore or 0.0, tostring(agent.laneChangeReason or "-")))
            UI.Text(string.format("Stop dwell %.2fs | queue reaction %.2fs | merge wait %s", agent.stopDwellSeconds or 0.0, agent.queueReleaseSeconds or 0.0, agent.mergeWait and "YES" or "NO"))
            UI.Text(string.format("Recovery %s | parking maneuver %s | emergency target %s", tostring(agent.recoveryStage or "none"), tostring(agent.parkingManeuver or "-"), tostring(agent.incidentResponseId or "-")))
            if UI.Button("CREATE BREAKDOWN HERE", 210.0, 28.0, false) then
                local _, message = RacingTrafficAgents.DebugCreateBreakdown(agent.id)
                trafficDebugMessage = message or "Could not create runtime breakdown"
            end
        end
    end
end
