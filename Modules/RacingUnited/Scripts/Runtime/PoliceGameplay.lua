-- STUDIO17 police / clandestine free-roam gameplay runtime.
-- Consumes HGAME police authoring plus the STUDIO13-16 traffic/navigation stack.

RacingPolice = RacingPolice or {}

local function clamp(v,a,b) if v<a then return a elseif v>b then return b else return v end end
local function distance2(a,b)
    if a==nil or b==nil then return math.huge end
    local dx=(a.x or 0)-(b.x or 0); local dz=(a.z or 0)-(b.z or 0)
    return math.sqrt(dx*dx+dz*dz)
end
local function hourActive(startHour,endHour,hour)
    startHour=tonumber(startHour) or 0.0; endHour=tonumber(endHour) or 24.0; hour=tonumber(hour) or 12.0
    if startHour==endHour then return true end
    if startHour<endHour then return hour>=startHour and hour<endHour end
    return hour>=startHour or hour<endHour
end
local function playerPosition()
    if nativeVehicleBody and nativeVehicleBody~=0 and Physics and Physics.BodyExists(nativeVehicleBody) then
        local x,y,z=Physics.GetBodyPosition(nativeVehicleBody); return {x=x or 0,y=y or 0,z=z or 0}
    end
    if playerEntity and playerEntity~=0 and Entity and Entity.Exists(playerEntity) then
        local x,y,z=Entity.GetWorldPosition(playerEntity); return {x=x or 0,y=y or 0,z=z or 0}
    end
    return nil
end
local function playerSpeedKmh()
    if nativeVehicleBody and nativeVehicleBody~=0 and Physics and Physics.BodyExists(nativeVehicleBody) then
        local x,y,z=Physics.GetBodyLinearVelocity(nativeVehicleBody)
        return math.sqrt((x or 0)^2+(y or 0)^2+(z or 0)^2)*3.6
    end
    return 0.0
end
local function currentHour()
    if Environment and Environment.GetTimeOfDay then return Environment.GetTimeOfDay() or 12.0 end
    return 12.0
end

function RacingPolice.CreateState()
    return {
        state="Idle", heat=0.0, heatLevel=0, lastInfraction="none", lastInfractionAgeS=math.huge,
        lostSightS=0.0, searchRemainingS=0.0, cooldownRemainingS=0.0, bustHoldS=0.0,
        dispatchCooldownS=0.0, roadblockCooldownS=0.0, activePoliceUnits={}, activeRoadblockId=0,
        roadblockIncidentId=0, lastPlayerNodeId=0, playerSpeedKmh=0.0, playerSpeedLimitKmh=0.0,
        patrolWeight=0.0, patrolZoneCount=0, escapeZone=false, witness=false,
        infractions=0, pursuits=0, escapes=0, busts=0, roadblocks=0, dispatched=0,
        activeMeetId=0, activeMeetRisk=0.0, debugMessage="Police gameplay idle"
    }
end

function RacingPolice.GetConfiguration() return RacingGameplay.GetPoliceGameplayConfiguration() end
function RacingPolice.GetState() return RacingPolice.state end

local heatByKind={ speeding=0.30, collision=0.65, dangerous_driving=0.50, illegal_race=1.00, evasion=0.75, police_collision=1.50, roadblock_breach=0.85 }
function RacingPolice.ReportInfraction(kind,severity,position,forceWitness)
    local cfg=RacingPolice.GetConfiguration(); local state=RacingPolice.state
    if state==nil or cfg.enabled~=true then return false,"police gameplay disabled" end
    local base=heatByKind[kind] or 0.35; severity=math.max(0.0,tonumber(severity) or 1.0)
    local pos=position or playerPosition(); local hour=currentHour(); local coverage={weight=0.0,zones={}}
    if pos then coverage=RacingGameplay.GetPolicePatrolCoverageAt(pos.x,pos.y,pos.z,hour) end
    local witnessed=forceWitness==true or (coverage.weight or 0.0)>0.05
    if not witnessed and cfg.civilianWitnesses==true then
        local witnessChance=clamp(0.20*severity,0.0,0.85); witnessed=math.random()<witnessChance
    end
    if not witnessed then return false,"infraction not witnessed" end
    state.heat=clamp((state.heat or 0.0)+base*severity,0.0,math.max(1,cfg.maxHeatLevel or 5))
    state.heatLevel=math.max(1,math.ceil(state.heat-0.001)); state.lastInfraction=kind or "unknown"; state.lastInfractionAgeS=0.0
    state.witness=true; state.infractions=(state.infractions or 0)+1
    if state.state=="Idle" or state.state=="Cooldown" or state.state=="Search" then state.state="Pursuit"; state.pursuits=(state.pursuits or 0)+1 end
    state.debugMessage="Witnessed "..tostring(kind).." / heat "..string.format("%.2f",state.heat)
    return true,state.debugMessage
end

function RacingPolice.BeginClandestineEvent(eventId)
    for _,event in ipairs(RacingGameplay.data.events or {}) do
        if event.id==eventId and event.enabled then
            local cfg=RacingPolice.GetConfiguration()
            if cfg.enabled and cfg.illegalRacesGenerateHeat and (event.policeEnabled or event.type=="Clandestine Circuit" or event.type=="Clandestine Sprint") then
                return RacingPolice.ReportInfraction("illegal_race", math.max(0.25,event.heat or 0.35), nil, true)
            end
            return false,"event does not generate police heat"
        end
    end
    return false,"event not found"
end

local function nearestRoadNodeToPlayer(pos)
    if pos==nil then return nil end
    local best=nil; local bestDistance=math.huge
    for _,node in ipairs(RacingGameplay.data.trafficNodes or {}) do
        if node.type=="Lane Node" or node.type=="Destination" or node.type=="Spawn" then
            local d=distance2(pos,node)
            if d<bestDistance then bestDistance=d; best=node end
        end
    end
    return best,bestDistance
end

local function cleanupPoliceUnits(state)
    local manager=RacingTrafficAgents and RacingTrafficAgents.manager or nil; if manager==nil then return end
    local kept={}
    for _,id in ipairs(state.activePoliceUnits or {}) do
        local agent=manager.agents and manager.agents[id] or nil
        if agent~=nil then kept[#kept+1]=id end
    end
    state.activePoliceUnits=kept
end

local function dispatchPolice(state,pos,coverage)
    local cfg=RacingPolice.GetConfiguration(); if not RacingTrafficAgents or not RacingTrafficAgents.manager then return end
    cleanupPoliceUnits(state)
    local cap=math.min(math.max(0,cfg.maxPursuitUnits or 12), (coverage and coverage.maximumUnits and coverage.maximumUnits>0) and coverage.maximumUnits or (cfg.maxPursuitUnits or 12))
    local wanted=math.min(cap, math.max(1,math.ceil(state.heat or 0.0)*2))
    if #state.activePoliceUnits>=wanted or (state.dispatchCooldownS or 0)>0 then return end
    local goal=nearestRoadNodeToPlayer(pos); if goal==nil then return end
    local portal=nil
    if coverage and coverage.zones then
        for _,zone in ipairs(coverage.zones) do
            if (zone.responsePortalId or 0)~=0 then portal=RacingGameplay.GetTrafficSpawnPortal(zone.responsePortalId); if portal then break end end
        end
    end
    if portal==nil then
        local portals=RacingGameplay.GetActiveTrafficSpawnPortals(currentHour(),"Spawn Only")
        for _,p in ipairs(portals) do if p.emergencyAllowed~=false then portal=p break end end
    end
    if portal==nil then
        local portals=RacingGameplay.GetActiveTrafficSpawnPortals(currentHour(),nil)
        for _,p in ipairs(portals) do if p.emergencyAllowed~=false then portal=p break end end
    end
    if portal==nil then return end
    local startNode=RacingGameplay.GetTrafficNode(portal.nodeId); if startNode==nil then return end
    local agent,message=RacingTrafficAgents.SpawnEmergencyUnit(startNode.id,goal.id,portal)
    if agent then
        agent.policeUnit=true; agent.pursuitTarget="player"; state.activePoliceUnits[#state.activePoliceUnits+1]=agent.id
        state.dispatched=(state.dispatched or 0)+1; state.dispatchCooldownS=math.max(0.5,(cfg.backupDelayS or 4.0)/(coverage.responseMultiplier or 1.0)); state.debugMessage="Police unit dispatched #"..tostring(agent.id)
    else state.debugMessage="Dispatch failed: "..tostring(message) end
end

local function retargetPoliceUnits(state,pos)
    local manager=RacingTrafficAgents and RacingTrafficAgents.manager or nil; if manager==nil then return end
    local goal=nearestRoadNodeToPlayer(pos); if goal==nil then return end
    for _,id in ipairs(state.activePoliceUnits or {}) do
        local agent=manager.agents and manager.agents[id] or nil
        if agent and agent.currentNodeId and (agent.goalNodeId~=goal.id or (agent.routeIndex or 1)>=math.max(1,(agent.routeLength or 1)-2)) then
            RacingTrafficAgents.AssignDestination(agent,goal.id,{manager=manager,vehicle={emergency=true}})
            agent.pursuitTarget="player"
        end
    end
end

local function policeHasSight(state,pos,coverage,escape)
    if pos==nil then return false end
    if escape and escape.breakLineOfSight then return false end
    local cfg=RacingPolice.GetConfiguration(); local manager=RacingTrafficAgents and RacingTrafficAgents.manager or nil
    if coverage and (coverage.weight or 0)>0.08 then return true end
    if manager then
        for _,id in ipairs(state.activePoliceUnits or {}) do
            local agent=manager.agents and manager.agents[id] or nil
            if agent and distance2(agent,pos)<=math.max(25.0,cfg.policeDetectionRadiusM or 180.0) then return true end
        end
    end
    return false
end

local function nearestPoliceDistance(state,pos)
    local manager=RacingTrafficAgents and RacingTrafficAgents.manager or nil; local best=math.huge
    if manager and pos then for _,id in ipairs(state.activePoliceUnits or {}) do local a=manager.agents and manager.agents[id] or nil; if a then best=math.min(best,distance2(a,pos)) end end end
    return best
end

local function selectRoadblock(state,pos)
    local cfg=RacingPolice.GetConfiguration(); if (state.heat or 0)<math.max(cfg.roadblockMinimumHeat or 3.0,1.0) then return nil end
    local candidates={}; local total=0.0
    for _,site in ipairs(RacingGameplay.GetPoliceRoadblockSites()) do
        if site.enabled and (state.heat or 0)>=(site.minimumHeat or 3.0) then
            local d=distance2(pos,site); if d>=80.0 and d<=1800.0 then local w=math.max(0.0,site.selectionWeight or 1.0)*(1.0/(1.0+d*0.001)); total=total+w; candidates[#candidates+1]={site=site,weight=w} end
        end
    end
    if #candidates==0 then return nil end
    local pick=math.random()*math.max(0.001,total); local sum=0.0
    for _,c in ipairs(candidates) do sum=sum+c.weight; if pick<=sum then return c.site end end
    return candidates[#candidates].site
end

local function activateRoadblock(state,site)
    if site==nil then return end
    state.activeRoadblockId=site.id; state.roadblocks=(state.roadblocks or 0)+1; state.roadblockCooldownS=25.0
    local operations=RacingTrafficAgents and RacingTrafficAgents.manager and RacingTrafficAgents.manager.operations or nil
    if operations then
        state.roadblockIncidentId=RacingTraffic.ReportIncident(operations,{name="Police Roadblock: "..tostring(site.name),type="Police Stop",enabled=true,
            linkId=0,roadId=0,x=site.x,y=site.y,z=site.z,radiusM=math.max(8.0,site.widthM or 10.0),severity=0.7,blockedLaneFraction=site.leaveEscapeGap and 0.55 or 0.85,
            speedLimitKmh=8.0,routeCostMultiplier=6.0,clearAfterS=45.0,emergencyResponse=false,hazardLights=true}) or 0
    end
    state.debugMessage="Roadblock activated: "..tostring(site.name)
end

local function clearRoadblock(state)
    if (state.roadblockIncidentId or 0)~=0 and RacingTrafficAgents and RacingTrafficAgents.manager then RacingTraffic.ClearIncident(RacingTrafficAgents.manager.operations,state.roadblockIncidentId) end
    state.activeRoadblockId=0; state.roadblockIncidentId=0
end

local function automaticSpeedEnforcement(state,pos,speed,coverage,dt)
    local cfg=RacingPolice.GetConfiguration(); if not cfg.speedingGeneratesHeat or pos==nil then return end
    local node=nearestRoadNodeToPlayer(pos); state.lastPlayerNodeId=node and node.id or 0; state.playerSpeedLimitKmh=node and (node.speedLimitKmh or 0.0) or 0.0
    if node==nil or (node.speedLimitKmh or 0)<=0 then return end
    local tolerance=(coverage and coverage.speedToleranceKmh) or cfg.speedToleranceKmh or 12.0
    local cameraWitness=false
    for _,point in ipairs(RacingGameplay.data.worldPoints or {}) do
        if point.enabled and (point.type=="Speed Camera" or point.type=="Speed Trap") and distance2(pos,point)<=math.max(2.0,point.radiusM or 8.0) then
            cameraWitness=true; tolerance=math.min(tolerance,cfg.speedToleranceKmh or tolerance); break
        end
    end
    local excess=speed-(node.speedLimitKmh or 0)-tolerance
    state.speedingAccumulatorS=excess>0 and ((state.speedingAccumulatorS or 0)+dt) or 0.0
    if state.speedingAccumulatorS>=1.25 and (state.speedingReportCooldownS or 0)<=0 then
        local severity=clamp(excess/60.0,0.15,1.5); RacingPolice.ReportInfraction("speeding",severity,pos,cameraWitness or (coverage.weight or 0)>0.05)
        state.speedingReportCooldownS=5.0; state.speedingAccumulatorS=0.0
    end
end

function RacingPolice.Update(dt)
    local cfg=RacingPolice.GetConfiguration(); local state=RacingPolice.state
    if state==nil then state=RacingPolice.CreateState(); RacingPolice.state=state end
    dt=math.max(0.0,dt or 0.0); state.lastInfractionAgeS=(state.lastInfractionAgeS or 0)+dt; state.dispatchCooldownS=math.max(0,(state.dispatchCooldownS or 0)-dt); state.roadblockCooldownS=math.max(0,(state.roadblockCooldownS or 0)-dt); state.speedingReportCooldownS=math.max(0,(state.speedingReportCooldownS or 0)-dt)
    if cfg.enabled~=true then state.state="Disabled"; return end
    local pos=playerPosition(); local speed=playerSpeedKmh(); state.playerSpeedKmh=speed; local hour=currentHour()
    local coverage=pos and RacingGameplay.GetPolicePatrolCoverageAt(pos.x,pos.y,pos.z,hour) or {weight=0,zones={}}; state.patrolWeight=coverage.weight or 0; state.patrolZoneCount=#(coverage.zones or {})
    local escape=pos and RacingGameplay.GetPoliceEscapeInfluenceAt(pos.x,pos.y,pos.z) or {active=false,heatDecayMultiplier=1,searchTimeMultiplier=1,breakLineOfSight=false}; state.escapeZone=escape.active==true
    automaticSpeedEnforcement(state,pos,speed,coverage,dt)

    if state.state=="Pursuit" then
        dispatchPolice(state,pos,coverage); retargetPoliceUnits(state,pos)
        local visible=policeHasSight(state,pos,coverage,escape); state.witness=visible
        if visible then state.lostSightS=0 else state.lostSightS=(state.lostSightS or 0)+dt end
        if state.lostSightS>=math.max(0.5,cfg.lostSightSeconds or 12.0) then state.state="Search"; state.searchRemainingS=(cfg.searchDurationS or 90.0)*(escape.searchTimeMultiplier or 1.0); state.debugMessage="Pursuit lost sight: search started" end
        if (state.heat or 0)>=math.max(cfg.roadblockMinimumHeat or 3.0,1.0) and (state.activeRoadblockId or 0)==0 and (state.roadblockCooldownS or 0)<=0 then activateRoadblock(state,selectRoadblock(state,pos)) end
        local policeDistance=nearestPoliceDistance(state,pos)
        if speed<2.0 and policeDistance<7.0 then state.bustHoldS=(state.bustHoldS or 0)+dt else state.bustHoldS=0.0 end
        if state.bustHoldS>=math.max(0.5,cfg.bustHoldSeconds or 5.0) then state.state="Busted"; state.busts=(state.busts or 0)+1; state.debugMessage="Player busted" end
    elseif state.state=="Search" then
        local visible=policeHasSight(state,pos,coverage,escape)
        if visible then state.state="Pursuit"; state.lostSightS=0; state.debugMessage="Police reacquired player" else
            state.searchRemainingS=math.max(0,(state.searchRemainingS or 0)-dt*((escape.searchTimeMultiplier or 1.0)>0 and 1.0/(escape.searchTimeMultiplier or 1.0) or 1.0))
            if state.searchRemainingS<=0 then state.state="Cooldown"; state.cooldownRemainingS=cfg.cooldownDurationS or 30.0; state.escapes=(state.escapes or 0)+1; clearRoadblock(state); state.debugMessage="Escaped pursuit / cooling down" end
        end
    elseif state.state=="Cooldown" then
        state.cooldownRemainingS=math.max(0,(state.cooldownRemainingS or 0)-dt); local decay=(cfg.heatDecayPerSecond or 0.035)*(escape.heatDecayMultiplier or 1.0)
        if state.lastInfractionAgeS>=(cfg.heatDecayDelayS or 20.0) then state.heat=math.max(0,(state.heat or 0)-decay*dt) end
        state.heatLevel=state.heat>0 and math.ceil(state.heat-0.001) or 0
        if state.cooldownRemainingS<=0 and state.heat<=0.001 then state.state="Idle"; state.debugMessage="Police heat cleared" end
    elseif state.state=="Busted" then
        -- Game progression/fines/impound hooks can consume this terminal event later.
        state.heat=0; state.heatLevel=0; clearRoadblock(state)
    else
        state.state="Idle"; if state.lastInfractionAgeS>=(cfg.heatDecayDelayS or 20.0) then state.heat=math.max(0,(state.heat or 0)-(cfg.heatDecayPerSecond or 0.035)*dt*(escape.heatDecayMultiplier or 1.0)) end; state.heatLevel=state.heat>0 and math.ceil(state.heat-0.001) or 0
    end

    local openMeets=RacingGameplay.GetOpenClandestineMeets(hour); state.activeMeetId=0; state.activeMeetRisk=0.0
    if pos then for _,meet in ipairs(openMeets) do if distance2(pos,meet)<=math.max(1.0,meet.radiusM or 35.0) then state.activeMeetId=meet.id; state.activeMeetRisk=meet.policeRisk or 0.0; break end end end
end

function RacingPolice.Start()
    RacingPolice.state=RacingPolice.CreateState()
end
function RacingPolice.Stop()
    if RacingPolice.state then clearRoadblock(RacingPolice.state) end
    RacingPolice.state=nil
end
function RacingPolice.FixedUpdate(dt) RacingPolice.Update(dt) end

function RacingPolice.DebugSetHeat(value)
    local cfg=RacingPolice.GetConfiguration(); local state=RacingPolice.state or RacingPolice.CreateState(); RacingPolice.state=state
    state.heat=clamp(tonumber(value) or 0.0,0.0,math.max(1,cfg.maxHeatLevel or 5)); state.heatLevel=state.heat>0 and math.ceil(state.heat-0.001) or 0
    state.state=state.heat>0 and "Pursuit" or "Idle"; if state.state=="Pursuit" then state.pursuits=(state.pursuits or 0)+1 end
    return state.heat
end
function RacingPolice.DebugEscape()
    local state=RacingPolice.state; if not state then return end
    state.state="Search"; state.searchRemainingS=0.01; state.lostSightS=999.0
end
function RacingPolice.DebugClear()
    if RacingPolice.state then clearRoadblock(RacingPolice.state) end
    RacingPolice.state=RacingPolice.CreateState()
end
function RacingPolice.GetTelemetry()
    local s=RacingPolice.state or RacingPolice.CreateState(); return {
        state=s.state,heat=s.heat,heatLevel=s.heatLevel,units=#(s.activePoliceUnits or {}),roadblockId=s.activeRoadblockId,
        lastInfraction=s.lastInfraction,lastInfractionAgeS=s.lastInfractionAgeS,searchRemainingS=s.searchRemainingS,cooldownRemainingS=s.cooldownRemainingS,
        playerSpeedKmh=s.playerSpeedKmh,playerSpeedLimitKmh=s.playerSpeedLimitKmh,patrolWeight=s.patrolWeight,patrolZoneCount=s.patrolZoneCount,
        escapeZone=s.escapeZone,witness=s.witness,infractions=s.infractions,pursuits=s.pursuits,escapes=s.escapes,busts=s.busts,roadblocks=s.roadblocks,dispatched=s.dispatched,
        activeMeetId=s.activeMeetId,activeMeetRisk=s.activeMeetRisk,debugMessage=s.debugMessage
    }
end
