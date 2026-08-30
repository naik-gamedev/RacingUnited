-- STUDIO23 motorsport-weekend / competitor authority.
-- STUDIO19 owns grids/results/championships; STUDIO20 adds RacingAI decisions/strategy;
-- STUDIO21 optionally makes nearby native Heritage Vehicle chassis authoritative for physical
-- speed and route progress while retaining logical competitors as the scalable fallback.
RacingMotorsport = RacingMotorsport or {}

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function deterministic01(id,salt)
    local x=((tonumber(id) or 1)*1103515245 + (salt or 0)*12345 + 12345) % 2147483647
    return (x % 100000) / 100000.0
end
local function cfg() return RacingGameplay.GetMotorsportConfiguration and RacingGameplay.GetMotorsportConfiguration() or {} end
local function parsePoints(text)
    local result={}
    for token in string.gmatch(tostring(text or ""),"[^,]+") do result[#result+1]=tonumber(token) or 0.0 end
    return result
end
local state={active=false,event=nil,session=nil,venue=nil,grid={},agents={},qualifying={},lastResults={},championship={},roundPoints={},roundCompleted={},message="No motorsport weekend active.",physicalCount=0,fullPhysicsCount=0}
RacingMotorsport.state=state

local function buildSegments(points,closedLoop)
    if not points or #points<2 then return {},0.0 end
    local segments={}; local total=0.0; local count=#points; local segmentCount=closedLoop and count or count-1
    for i=1,segmentCount do
        local a=points[i]; local b=points[(i % count)+1]
        local dx=(b.x or 0)-(a.x or 0); local dy=(b.y or 0)-(a.y or 0); local dz=(b.z or 0)-(a.z or 0)
        local length=math.sqrt(dx*dx+dy*dy+dz*dz)
        if length>0.01 then segments[#segments+1]={a=a,b=b,start=total,length=length}; total=total+length end
    end
    return segments,total
end

local function projectPointToSegments(segments,gx,gy,gz)
    if not segments or #segments==0 then return nil end
    local best=nil; local bestSq=math.huge
    for _,seg in ipairs(segments) do
        local ax,ay,az=seg.a.x or 0.0,seg.a.y or 0.0,seg.a.z or 0.0
        local bx,by,bz=seg.b.x or 0.0,seg.b.y or 0.0,seg.b.z or 0.0
        local dx,dz=bx-ax,bz-az; local lenSq=dx*dx+dz*dz
        local t=0.0; if lenSq>0.0001 then t=clamp(((gx-ax)*dx+(gz-az)*dz)/lenSq,0.0,1.0) end
        local nx=ax+dx*t; local ny=ay+(by-ay)*t; local nz=az+dz*t
        local ex,ez=gx-nx,gz-nz; local distSq=ex*ex+ez*ez
        if distSq<bestSq then
            local len=math.sqrt(math.max(0.0001,lenSq)); local signed=(dx*ez-dz*ex)/len
            bestSq=distSq; best={distanceM=(seg.start or 0.0)+(seg.length or len)*t,lateralErrorM=signed,nearestX=nx,nearestY=ny,nearestZ=nz,headingDeg=math.deg(math.atan(dx,dz)),distanceSq=distSq}
        end
    end
    return best
end

local function firstMarker(markerType,layoutId)
    local markers=RacingGameplay.GetMarkersByType and RacingGameplay.GetMarkersByType(markerType,layoutId) or {}
    return markers and markers[1] or nil
end

local function crossedRouteDistance(previous,current,target,total,closedLoop)
    if previous==nil or current==nil or target==nil then return false end
    if closedLoop then
        if current>=previous then return target>previous and target<=current end
        return target>previous or target<=current
    end
    return current>=previous and target>previous and target<=current
end

local function routeGeometry(event)
    local venue=RacingGameplay.ResolveEventVenue(event)
    local resolved=venue and venue.venue or nil
    local nodes=resolved and resolved.routeNodes or {}
    local closed=resolved and resolved.route and resolved.route.closedLoop==true or false
    local segments,total=buildSegments(nodes,closed)
    local layoutId=event and event.layoutId or 0
    local aiNodes=RacingGameplay.GetMarkersByType and RacingGameplay.GetMarkersByType("AI Race Line",layoutId) or {}
    local wetNodes=RacingGameplay.GetMarkersByType and RacingGameplay.GetMarkersByType("AI Wet Line",layoutId) or {}
    local aiSegments,aiTotal=buildSegments(aiNodes,closed)
    local wetSegments,wetTotal=buildSegments(wetNodes,closed)
    local pitNodes=resolved and resolved.pitRouteNodes or {}
    local pitSegments,pitTotal=buildSegments(pitNodes,false)

    local pitEntryMarker=firstMarker("Pit Entry",layoutId)
    local pitExitMarker=firstMarker("Pit Exit",layoutId)
    local startFinish=(resolved and resolved.startFinish) or firstMarker("Start / Finish",layoutId)
    local pitBoxes=RacingGameplay.GetMarkersByType and RacingGameplay.GetMarkersByType("Pit Box",layoutId) or {}
    table.sort(pitBoxes,function(a,b) return (a.slot or 9999)<(b.slot or 9999) end)

    local pitEntryMain=nil; local pitExitMain=nil; local pitExitPit=nil; local pitLapLine=nil
    if #pitSegments>0 and #segments>0 then
        local entrySource=pitEntryMarker or pitNodes[1]
        local exitSource=pitExitMarker or pitNodes[#pitNodes]
        local entryProjection=entrySource and projectPointToSegments(segments,entrySource.x or 0.0,entrySource.y or 0.0,entrySource.z or 0.0) or nil
        local exitMainProjection=exitSource and projectPointToSegments(segments,exitSource.x or 0.0,exitSource.y or 0.0,exitSource.z or 0.0) or nil
        local exitPitProjection=exitSource and projectPointToSegments(pitSegments,exitSource.x or 0.0,exitSource.y or 0.0,exitSource.z or 0.0) or nil
        pitEntryMain=entryProjection and entryProjection.distanceM or nil
        pitExitMain=exitMainProjection and exitMainProjection.distanceM or nil
        pitExitPit=exitPitProjection and exitPitProjection.distanceM or pitTotal
        if startFinish then
            local lapProjection=projectPointToSegments(pitSegments,startFinish.x or 0.0,startFinish.y or 0.0,startFinish.z or 0.0)
            local maxDistance=math.max(40.0,(startFinish.gateWidthM or 12.0)*2.5)
            if lapProjection and (lapProjection.distanceSq or math.huge)<=maxDistance*maxDistance then pitLapLine=lapProjection.distanceM end
        end
    end
    return venue,segments,total,aiSegments,aiTotal,wetSegments,wetTotal,pitSegments,pitTotal,pitEntryMain,pitExitMain,pitExitPit,pitLapLine,pitBoxes
end

local function sampleSegments(segments,total,distance,closedLoop)
    if total<=0 or #segments==0 then return 0,0,0,0 end
    local d=distance
    if closedLoop then d=d%total else d=clamp(d,0,total) end
    local seg=segments[#segments]
    for _,candidate in ipairs(segments) do if d<=candidate.start+candidate.length then seg=candidate break end end
    local t=clamp((d-seg.start)/math.max(0.001,seg.length),0,1)
    local x=(seg.a.x or 0)+((seg.b.x or 0)-(seg.a.x or 0))*t
    local y=(seg.a.y or 0)+((seg.b.y or 0)-(seg.a.y or 0))*t
    local z=(seg.a.z or 0)+((seg.b.z or 0)-(seg.a.z or 0))*t
    local heading=math.deg(math.atan((seg.b.x or 0)-(seg.a.x or 0),(seg.b.z or 0)-(seg.a.z or 0)))
    return x,y,z,heading
end

local function positionAtDistance(agent,distance)
    if agent.pitMode and agent.pitSegments and #agent.pitSegments>0 and (agent.pitRouteLength or 0)>0 then
        return sampleSegments(agent.pitSegments,agent.pitRouteLength,distance or agent.pitDistance or 0.0,false)
    end
    if agent.routeLength<=0 or #agent.segments==0 then return 0,0,0,0 end
    local fraction=(distance%agent.routeLength)/math.max(0.001,agent.routeLength)
    local drySegments=(agent.aiSegments and #agent.aiSegments>0) and agent.aiSegments or agent.segments
    local dryTotal=(agent.aiSegments and #agent.aiSegments>0) and agent.aiRouteLength or agent.routeLength
    local x,y,z,heading=sampleSegments(drySegments,dryTotal,fraction*dryTotal,agent.closedLoop)
    local blend=agent.ai and agent.ai.wetLineBlend or 0.0
    if blend>0.001 and agent.wetSegments and #agent.wetSegments>0 then
        local wx,wy,wz=sampleSegments(agent.wetSegments,agent.wetRouteLength,fraction*agent.wetRouteLength,agent.closedLoop)
        x=x+(wx-x)*blend; y=y+(wy-y)*blend; z=z+(wz-z)*blend
    end
    local offset=agent.ai and agent.ai.lateralOffsetM or 0.0
    local h=math.rad(heading); x=x+math.cos(h)*offset; z=z-math.sin(h)*offset
    return x,y,z,heading
end

function RacingMotorsport.SampleAgentPath(agent,distance)
    return positionAtDistance(agent,distance or 0.0)
end

function RacingMotorsport.ProjectAgentToRoute(agent,gx,gy,gz)
    local activeSegments=(agent and agent.pitMode and agent.pitSegments and #agent.pitSegments>0) and agent.pitSegments or (agent and agent.segments or nil)
    if not agent or not activeSegments or #activeSegments==0 then return {distanceM=agent and agent.distance or 0.0,lateralErrorM=0.0,nearestX=gx,nearestY=gy,nearestZ=gz,headingDeg=0.0,onPitPath=agent and agent.pitMode~=nil} end
    local best=projectPointToSegments(activeSegments,gx,gy,gz) or {distanceM=agent.distance or 0.0,lateralErrorM=0.0,nearestX=gx,nearestY=gy,nearestZ=gz,headingDeg=0.0}
    best.onPitPath=agent.pitMode~=nil
    return best
end

local function destroyAgentProxy(agent)
    if agent.fullPhysicsHandle and RacingAIVehicleController and RacingAIVehicleController.Destroy then RacingAIVehicleController.Destroy(agent.fullPhysicsHandle) end
    agent.fullPhysicsHandle=nil
    local h=agent.proxy
    if h then
        if h.body and Physics and Physics.BodyExists and Physics.BodyExists(h.body) then Physics.DestroyBody(h.body) end
        if h.entity and Entity and Entity.Exists and Entity.Exists(h.entity) then Entity.Destroy(h.entity) end
    end
    agent.proxy=nil
end

local function spawnProxy(agent,gridMarker)
    if Entity==nil or Physics==nil then return nil end
    local entity=Entity.Create("Race Competitor "..tostring(agent.entrant.raceNumber or agent.entrant.id).." - "..tostring(agent.entrant.driverName or "AI"))
    if entity==0 then return nil end
    Entity.AddTag(entity,"RaceCompetitor"); Entity.AddTag(entity,"RacingAI")
    Entity.SetLocalScale(entity,1.8,1.35,4.4); Entity.SetDebugPrimitive(entity,"box",0.92,0.25,0.18)
    local body=Physics.CreateBody(entity,"kinematic",1450.0)
    if body==0 then Entity.Destroy(entity); return nil end
    Physics.CreateBoxCollider(body,0.85,0.55,2.05,0,0.05,0,0.75,0.02,false)
    local gx,gy,gz,heading
    if gridMarker then gx,gy,gz,heading=gridMarker.x or 0,gridMarker.y or 0,gridMarker.z or 0,gridMarker.headingDeg or 0
    else gx,gy,gz,heading=positionAtDistance(agent,0) end
    local x,y,z=Physics.GlobalToLocal(gx,gy,gz); if x~=nil then Physics.SetBodyPosition(body,x,y+0.7,z); Physics.SetBodyRotation(body,0,heading or 0,0) end
    return {entity=entity,body=body}
end

local function gridMarkers(event)
    local result=RacingGameplay.GetMarkersByType and RacingGameplay.GetMarkersByType("Grid Slot",event and event.layoutId or 0) or {}
    table.sort(result,function(a,b) return (a.slot or 9999)<(b.slot or 9999) end)
    return result
end

local function championshipScore(entrantId)
    local total=0.0
    for _,tableByEntrant in pairs(state.championship) do total=total+(tableByEntrant[entrantId] or 0.0) end
    return total
end

local function previousResultOrder(eventId)
    local result=state.lastResults[eventId] or {}
    local order={}; for i,row in ipairs(result) do order[row.entrantId or row.id]=i end
    return order
end

local function qualifyingOrder(eventId)
    local result=state.qualifying[eventId] or {}; local order={}
    for i,row in ipairs(result) do order[row.entrantId]=i end
    return order
end

function RacingMotorsport.BuildGrid(event,session)
    event=event or (RacingEvents and RacingEvents.GetSelectedEvent and RacingEvents.GetSelectedEvent())
    if not event then return {} end
    local entrants=RacingGameplay.GetEntrantsForEvent and RacingGameplay.GetEntrantsForEvent(event.id) or {}
    local maxEntrants=math.max(0,event.maxEntrants or #entrants)
    local clandestine=(event.type=="Clandestine Circuit" or event.type=="Clandestine Sprint")
    local rows={}; for _,entrant in ipairs(entrants) do if entrant.clandestine==clandestine or entrant.eventId==event.id then rows[#rows+1]=entrant end end
    local source=session and session.gridSource or "Event order"
    if source=="Qualifying" then
        local q=qualifyingOrder(event.id); table.sort(rows,function(a,b) return (q[a.id] or 9999)<(q[b.id] or 9999) end)
    elseif source=="Previous session" then
        local p=previousResultOrder(event.id); table.sort(rows,function(a,b) return (p[a.id] or 9999)<(p[b.id] or 9999) end)
    elseif source=="Championship" then
        table.sort(rows,function(a,b) return championshipScore(a.id)>championshipScore(b.id) end)
    else
        table.sort(rows,function(a,b)
            local ga=(a.gridOverride or 0)>0 and a.gridOverride or 9999; local gb=(b.gridOverride or 0)>0 and b.gridOverride or 9999
            if ga~=gb then return ga<gb end return (a.raceNumber or 9999)<(b.raceNumber or 9999)
        end)
    end
    if source=="Reverse top N" then
        local n=math.min(#rows,math.max(0,session and session.reverseTopN or 0)); local reversed={}
        for i=n,1,-1 do reversed[#reversed+1]=rows[i] end; for i=n+1,#rows do reversed[#reversed+1]=rows[i] end; rows=reversed
    end
    while #rows>maxEntrants do table.remove(rows) end
    local grid={}
    for i,entrant in ipairs(rows) do grid[#grid+1]={position=i,entrant=entrant,class=RacingGameplay.GetMotorsportClass and RacingGameplay.GetMotorsportClass(entrant.classId) or nil} end
    state.grid=grid; return grid
end

local function syntheticLapSeconds(agent,qualifying)
    local route=math.max(500.0,agent.routeLength)
    local baseSpeed=42.0
    local pace=clamp((qualifying and agent.entrant.qualifyingPace or agent.entrant.racePace or agent.entrant.aiSkill or cfg().defaultAiSkill or 0.8),0.05,1.0)
    local variation=(deterministic01(agent.entrant.id,agent.completedLaps+3)-0.5)*(1.0-(agent.entrant.consistency or 0.8))*0.08
    return route/(baseSpeed*(0.72+pace*0.38)*(1.0+variation))
end

function RacingMotorsport.OnEventStarted(event,session)
    RacingMotorsport.Stop(false)
    if RacingAIRacecraft and RacingAIRacecraft.ClearIncidentLog then RacingAIRacecraft.ClearIncidentLog() end
    if cfg().enabled==false or cfg().aiCompetitorsEnabled==false then return end
    state.active=true; state.event=event; state.session=session
    local venue,segments,total,aiSegments,aiTotal,wetSegments,wetTotal,pitSegments,pitTotal,pitEntryMain,pitExitMain,pitExitPit,pitLapLine,pitBoxes=routeGeometry(event); state.venue=venue
    local grid=RacingMotorsport.BuildGrid(event,session); local markers=gridMarkers(event); local physicalBudget=math.max(0,cfg().maxPhysicalCompetitors or 0)
    for i,row in ipairs(grid) do
        local entrant=row.entrant
        local agent={entrant=entrant,class=row.class,segments=segments,routeLength=total,aiSegments=aiSegments,aiRouteLength=aiTotal,wetSegments=wetSegments,wetRouteLength=wetTotal,hasWetLine=#wetSegments>0,pitSegments=pitSegments,pitRouteLength=pitTotal,
            closedLoop=venue and venue.venue and venue.venue.route and venue.venue.route.closedLoop==true or false,distance=0.0,completedLaps=0,lapTimeS=0.0,bestLapS=math.huge,lapStartElapsedS=0.0,targetLapS=0.0,
            finished=false,dnf=false,checkpointIndex=0,elapsedS=0.0,pitStops=0,serviceS=0.0,pitServiceRemainingS=0.0,currentMps=0.0,basePaceMps=0.0,finalLapTarget=nil,
            mechanicalHealth=1.0,lastPhysicalRouteDistance=nil,lastPhysicalPitDistance=nil,physicalBackend="Logical",physicalFeedback=nil,
            pitArmed=false,pitMode=nil,pitEntryMainDistance=pitEntryMain,pitExitMainDistance=pitExitMain,pitExitPitDistance=pitExitPit,pitLapLineDistance=pitLapLine,pitLapCounted=false}
        local pitBox=pitBoxes and pitBoxes[i] or nil
        if pitBox and #pitSegments>0 then local projected=projectPointToSegments(pitSegments,pitBox.x or 0.0,pitBox.y or 0.0,pitBox.z or 0.0); agent.pitServiceDistance=projected and projected.distanceM or nil end
        agent.targetLapS=syntheticLapSeconds(agent,false); agent.basePaceMps=agent.routeLength/math.max(10.0,agent.targetLapS); agent.currentMps=agent.basePaceMps*0.35
        if RacingAI and RacingAI.InitializeAgent then RacingAI.InitializeAgent(agent,session) end
        local id="ai:"..tostring(entrant.id); agent.participantId=id
        if RacingEvents and RacingEvents.RegisterParticipant then RacingEvents.RegisterParticipant(id,(entrant.driverName or "AI").." #"..tostring(entrant.raceNumber or 0)) end
        if i<=physicalBudget then
            local fullPhysics=(RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration().fullPhysicsCompetitors==true)
            if fullPhysics and RacingAIVehicleController and RacingAIVehicleController.Spawn then
                agent.fullPhysicsHandle=RacingAIVehicleController.Spawn(agent,markers[i])
                if agent.fullPhysicsHandle then agent.physicalBackend="Heritage Vehicle"; state.physicalCount=state.physicalCount+1; state.fullPhysicsCount=state.fullPhysicsCount+1 end
            end
            if not agent.fullPhysicsHandle then agent.proxy=spawnProxy(agent,markers[i]); if agent.proxy then agent.physicalBackend="Kinematic proxy"; state.physicalCount=state.physicalCount+1 end end
        end
        state.agents[#state.agents+1]=agent
    end
    if session and session.type=="Qualifying" then
        local q={}
        for _,agent in ipairs(state.agents) do
            local skill=clamp(agent.entrant.qualifyingPace or agent.entrant.aiSkill or 0.8,0.05,1.0)
            local spread=(cfg().qualifyingPaceSpreadPercent or 4.0)*0.01
            local jitter=(deterministic01(agent.entrant.id,77)-0.5)*2.0*spread
            local time=syntheticLapSeconds(agent,true)*(1.06-skill*0.06)*(1.0+jitter)
            q[#q+1]={entrantId=agent.entrant.id,timeS=time,driverName=agent.entrant.driverName}
        end
        table.sort(q,function(a,b) return a.timeS<b.timeS end); state.qualifying[event.id]=q
    end
    state.message=string.format("Built %d-car grid (%d physical, %d full Heritage Vehicle) with STUDIO23 solver-contact steward evidence.",#state.agents,state.physicalCount,state.fullPhysicsCount)
end

local function beginAiPitService(agent,session)
    local pitSkill=clamp(agent.entrant.pitSkill or 0.75,0,1)
    local service=math.max(session.minimumPitServiceSeconds or 0.0,2.0+(1.0-pitSkill)*4.0)
    agent.pitServiceRemainingS=service; agent.serviceS=0.0; agent.currentMps=0.0; agent.pitStops=(agent.pitStops or 0)+1
    agent.pendingTireService=session.tireChangesAllowed~=false; agent.pendingFuelService=session.refuelingAllowed~=false; agent.pendingPitService=false
    if agent.fullPhysicsHandle and agent.fullPhysicsHandle.vehicle and Vehicle and Vehicle.Exists and Vehicle.Exists(agent.fullPhysicsHandle.vehicle) then Vehicle.SetInputs(agent.fullPhysicsHandle.vehicle,0.0,1.0,0.0,0.0) end
end

local function processCompletedLap(agent,ai,session,raceState)
    agent.lapTimeS=agent.elapsedS-agent.lapStartElapsedS; agent.lapStartElapsedS=agent.elapsedS; agent.bestLapS=math.min(agent.bestLapS,agent.lapTimeS)
    agent.completedLaps=agent.completedLaps+1; agent.targetLapS=syntheticLapSeconds(agent,false); agent.basePaceMps=agent.routeLength/math.max(10.0,agent.targetLapS)
    if ai and ai.pitRequested and (session.refuelingAllowed~=false or session.tireChangesAllowed~=false) then
        -- Full-physics cars with an authored pit route arm during live running and peel off at the physical Pit Entry.
        -- Logical cars (and physical cars without a pit route) keep the cheap lap-boundary service fallback.
        if not (agent.fullPhysicsHandle and agent.pitSegments and #agent.pitSegments>0 and (agent.pitRouteLength or 0)>5.0) then beginAiPitService(agent,session) end
    end
    if session.type=="Race" and not session.timedRace and agent.completedLaps>=math.max(1,raceState.targetLaps or 1) then agent.finished=true end
    if session.type=="Race" and session.timedRace and raceState.timedRaceExpired then
        if session.timePlusOneLap then
            if not agent.finalLapTarget then agent.finalLapTarget=agent.completedLaps+1 elseif agent.completedLaps>=agent.finalLapTarget then agent.finished=true end
        else agent.finished=true end
    end
end

local function updateLogicalAgentMotion(agent,ai,session,raceState,dt,speedFactor,phase)
    local targetMps=((ai and (ai.targetSpeedKmh or 0)>0) and (ai.targetSpeedKmh/3.6) or agent.basePaceMps)*speedFactor
    local skill=clamp(agent.entrant.aiSkill or 0.8,0,1); local accel=4.5+skill*4.5; local brake=8.0+skill*7.0
    local delta=targetMps-agent.currentMps; agent.currentMps=agent.currentMps+clamp(delta,-brake*dt,accel*dt)
    agent.distance=agent.distance+math.max(0.0,agent.currentMps)*dt
    if agent.routeLength>1.0 and agent.distance>=agent.routeLength then
        agent.distance=agent.distance-agent.routeLength
        if phase=="Running" then processCompletedLap(agent,ai,session,raceState) end
    end
end

local function updatePhysicalAgentMotion(agent,ai,session,raceState,dt,phase,flags,speedFactor)
    local feedback=RacingAIVehicleController.Update(agent.fullPhysicsHandle,agent,{phase=phase,flag=flags,session=session,raceState=raceState,speedFactor=speedFactor},dt)
    if not feedback then return false end
    agent.physicalFeedback=feedback; agent.currentMps=feedback.speedMps or agent.currentMps; agent.mechanicalHealth=feedback.mechanicalHealth or agent.mechanicalHealth
    agent.tireTemperatureC=feedback.tireTemperatureC or agent.tireTemperatureC; agent.vehicleMassKg=feedback.vehicleMassKg or agent.vehicleMassKg; agent.fuelMassKg=feedback.fuelMassKg or agent.fuelMassKg; agent.componentHealth=feedback.componentHealth or agent.componentHealth
    if feedback.collisionBounds and RacingAIRacecraft and RacingAIRacecraft.SetPhysicalFootprint then RacingAIRacecraft.SetPhysicalFootprint(agent,feedback.collisionBounds) end
    if feedback.contactEvidence and feedback.contactEvidence.newContactEpisode~=false and RacingAIRacecraft and RacingAIRacecraft.ReportPhysicalContact then
        local other=nil; local otherBody=tonumber(feedback.contactEvidence.otherBody) or 0
        if otherBody>0 then
            for _,candidate in ipairs(state.agents) do
                if candidate~=agent and candidate.fullPhysicsHandle and candidate.fullPhysicsHandle.body==otherBody then other=candidate; break end
            end
        end
        feedback.incidentEvidence=RacingAIRacecraft.ReportPhysicalContact(agent,other,feedback.contactEvidence)
    end
    if agent.pitMode then
        local currentPit=clamp(feedback.routeDistanceM or agent.pitDistance or 0.0,0.0,math.max(0.0,agent.pitRouteLength or 0.0))
        local previousPit=agent.lastPhysicalPitDistance
        agent.pitDistance=currentPit
        if phase=="Running" and not agent.pitLapCounted and agent.pitLapLineDistance and crossedRouteDistance(previousPit,currentPit,agent.pitLapLineDistance,agent.pitRouteLength or 0.0,false) then
            processCompletedLap(agent,ai,session,raceState); agent.pitLapCounted=true
        end
        local serviceDistance=agent.pitServiceDistance or math.max(2.0,(agent.pitRouteLength or 0.0)*0.45)
        if agent.pendingPitService and not agent.pitServiceTriggered and currentPit>=serviceDistance then
            agent.pitServiceTriggered=true; beginAiPitService(agent,session)
        else
            local exitDistance=agent.pitExitPitDistance or math.max(2.0,(agent.pitRouteLength or 0.0)-2.0)
            if not agent.pendingPitService and (agent.pitServiceRemainingS or 0)<=0 and currentPit>=math.max(2.0,exitDistance-1.0) then
                if phase=="Running" and not agent.pitLapCounted and agent.closedLoop and (agent.pitEntryMainDistance or 0.0)>(agent.pitExitMainDistance or agent.routeLength) then
                    processCompletedLap(agent,ai,session,raceState); agent.pitLapCounted=true
                end
                local rejoin=clamp(agent.pitExitMainDistance or 0.0,0.0,math.max(0.0,agent.routeLength or 0.0))
                agent.pitMode=nil; agent.pitDistance=0.0; agent.distance=rejoin; agent.lastPhysicalRouteDistance=rejoin; agent.lastPhysicalPitDistance=nil; agent.pitServiceTriggered=false; agent.pitLapCounted=false
                if agent.ai then agent.ai.decision="Pit exit"; agent.ai.reason="Rejoining main race route at authored Pit Exit" end
            end
        end
        agent.lastPhysicalPitDistance=currentPit
        return true
    end
    local current=clamp(feedback.routeDistanceM or agent.distance,0.0,math.max(0.0,agent.routeLength))
    local previous=agent.lastPhysicalRouteDistance
    agent.distance=current
    if phase=="Running" and agent.pitArmed and agent.pitEntryMainDistance and crossedRouteDistance(previous,current,agent.pitEntryMainDistance,agent.routeLength or 0.0,agent.closedLoop) then
        agent.pitMode="Lane"; agent.pitDistance=0.0; agent.pendingPitService=true; agent.pitServiceTriggered=false; agent.pitLapCounted=false; agent.pitArmed=false; agent.lastPhysicalPitDistance=nil
        if agent.ai then agent.ai.decision="Pit entry"; agent.ai.reason="Taking authored Pit Entry onto the pit route" end
        return true
    end
    if previous~=nil and agent.routeLength>1.0 then
        if agent.closedLoop then
            if phase=="Running" and previous>agent.routeLength*0.72 and current<agent.routeLength*0.28 and (feedback.forwardAlignment or 0.0)>0.15 and agent.currentMps>2.0 then processCompletedLap(agent,ai,session,raceState) end
        elseif phase=="Running" and current>=agent.routeLength-2.0 and previous<current and not agent.finished then processCompletedLap(agent,ai,session,raceState); agent.finished=true end
    end
    agent.lastPhysicalRouteDistance=current
    local aiCfg=RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration() or {}
    if aiCfg.damageStrategyEnabled~=false and agent.mechanicalHealth<=(aiCfg.damageDnfThreshold or 0.16) then
        agent.dnf=true; agent.finished=true
        if agent.fullPhysicsHandle and agent.fullPhysicsHandle.vehicle and Vehicle and Vehicle.Exists and Vehicle.Exists(agent.fullPhysicsHandle.vehicle) then Vehicle.SetInputs(agent.fullPhysicsHandle.vehicle,0.0,1.0,0.0,0.0) end
    end
    return true
end

function RacingMotorsport.FixedUpdate(dt)
    if not state.active or not RacingEvents or not RacingEvents.state then return end
    local raceState=RacingEvents.state; local phase=raceState.phase or "Idle"
    if phase=="Idle" or phase=="Finished" or phase=="Results" then return end
    local session=state.session or {}; local flags=raceState.flag or "Green"
    local aiCfg=RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration() or {}
    local speedFactor=(flags=="Safety Car" and 0.55) or ((flags=="Full Course Yellow" or flags=="Virtual Safety Car") and 0.48) or (flags=="Red" and 0.0) or 1.0
    if phase=="Staging" or phase=="Countdown" then speedFactor=0.0
    elseif phase=="Formation" then speedFactor=math.min(speedFactor,(aiCfg.formationSpeedKmh or 80.0)/math.max(1.0,160.0))
    elseif phase=="Rolling Start" then speedFactor=math.min(speedFactor,(aiCfg.rollingStartSpeedKmh or 90.0)/math.max(1.0,160.0)) end
    for _,agent in ipairs(state.agents) do
        if not agent.finished and not agent.dnf then
            if phase=="Running" then agent.elapsedS=agent.elapsedS+dt end
            if agent.pitServiceRemainingS and agent.pitServiceRemainingS>0 then
                agent.pitServiceRemainingS=math.max(0.0,agent.pitServiceRemainingS-dt); agent.serviceS=(agent.serviceS or 0)+dt; agent.currentMps=0.0
                if agent.fullPhysicsHandle and agent.fullPhysicsHandle.vehicle and Vehicle.Exists(agent.fullPhysicsHandle.vehicle) then Vehicle.SetInputs(agent.fullPhysicsHandle.vehicle,0.0,1.0,0.0,0.0) end
                if agent.ai then agent.ai.decision="Pit service"; agent.ai.reason="Fuel / tires / mandatory service" end
                if agent.pitServiceRemainingS<=0 then
                    if RacingAI and RacingAI.OnPitServiceComplete then RacingAI.OnPitServiceComplete(agent,agent.pendingTireService,agent.pendingFuelService) end
                    if agent.fullPhysicsHandle and RacingAIVehicleController and RacingAIVehicleController.OnPitServiceComplete then RacingAIVehicleController.OnPitServiceComplete(agent.fullPhysicsHandle) end
                end
            else
                local ai=RacingAI and RacingAI.UpdateAgent and RacingAI.UpdateAgent(agent,{agents=state.agents,session=session,flag=flags,phase=phase,targetLaps=raceState.targetLaps or 1,timedRaceExpired=raceState.timedRaceExpired==true},dt) or nil
                if phase=="Running" and agent.fullPhysicsHandle and not agent.pitMode and ai and ai.pitRequested and agent.pitSegments and #agent.pitSegments>0 and (agent.pitRouteLength or 0)>5.0 then
                    agent.pitArmed=true
                    if agent.ai then agent.ai.decision="Pit armed"; agent.ai.reason="Pit request accepted; waiting for authored Pit Entry" end
                end
                local usedPhysical=false
                if agent.fullPhysicsHandle and RacingAIVehicleController then usedPhysical=updatePhysicalAgentMotion(agent,ai,session,raceState,dt,phase,flags,speedFactor) end
                if not usedPhysical and (phase=="Running" or phase=="Formation" or phase=="Rolling Start") then updateLogicalAgentMotion(agent,ai,session,raceState,dt,speedFactor,phase) end
                local dnfRate=math.max(0.0,cfg().baseMechanicalDnfChancePerHour or 0.0)/3600.0
                if phase=="Running" and dnfRate>0 and deterministic01(agent.entrant.id,math.floor(agent.elapsedS*2)) < dnfRate*dt then
                    agent.dnf=true; agent.finished=true
                    if agent.fullPhysicsHandle and agent.fullPhysicsHandle.vehicle and Vehicle and Vehicle.Exists and Vehicle.Exists(agent.fullPhysicsHandle.vehicle) then Vehicle.SetInputs(agent.fullPhysicsHandle.vehicle,0.0,1.0,0.0,0.0) end
                end
                if phase=="Running" and not agent.finished and session.type~="Race" and session.durationMinutes and session.durationMinutes>0 and agent.elapsedS>=session.durationMinutes*60 then agent.finished=true end
            end
            if RacingEvents.SetParticipantProgress then RacingEvents.SetParticipantProgress(agent.participantId,agent.completedLaps,agent.checkpointIndex,agent.elapsedS,agent.finished,agent.bestLapS<math.huge and agent.bestLapS or 0.0) end
            if agent.proxy and agent.proxy.body and Physics.BodyExists(agent.proxy.body) then
                local gx,gy,gz,heading=positionAtDistance(agent,agent.completedLaps*agent.routeLength+agent.distance)
                local x,y,z=Physics.GlobalToLocal(gx,gy,gz); if x~=nil then Physics.SetBodyPosition(agent.proxy.body,x,y+0.7,z); Physics.SetBodyRotation(agent.proxy.body,0,heading,0) end
            end
        end
    end
end

local function championshipForEvent(eventId)
    for _,championship in ipairs(RacingGameplay.data.motorsportChampionships or {}) do
        if championship.enabled then for _,round in ipairs(RacingGameplay.GetChampionshipRounds(championship.id)) do if round.eventId==eventId then return championship,round end end end
    end
    return nil,nil
end

local function roundPointKey(championshipId,roundId,entrantId)
    return "motorsport.championship."..tostring(championshipId)..".round."..tostring(roundId)..".entrant."..tostring(entrantId)..".points"
end
local function roundCompletedKey(championshipId,roundId)
    return "motorsport.championship."..tostring(championshipId)..".round."..tostring(roundId)..".completed"
end
local function getRoundPoints(championshipId,roundId,entrantId)
    local byChamp=state.roundPoints[championshipId]; local byRound=byChamp and byChamp[roundId]
    local value=byRound and byRound[entrantId]
    if value==nil and Save and cfg().championshipPersistence~=false then value=Save.GetNumber(roundPointKey(championshipId,roundId,entrantId),0.0) end
    return value or 0.0
end
local function roundCompleted(championshipId,roundId)
    local byChamp=state.roundCompleted[championshipId]; if byChamp and byChamp[roundId]~=nil then return byChamp[roundId] end
    if Save and cfg().championshipPersistence~=false then return Save.GetNumber(roundCompletedKey(championshipId,roundId),0.0)>0.5 end
    return false
end
local function setRoundPoints(championshipId,roundId,entrantId,points)
    state.roundPoints[championshipId]=state.roundPoints[championshipId] or {}; state.roundPoints[championshipId][roundId]=state.roundPoints[championshipId][roundId] or {}
    state.roundPoints[championshipId][roundId][entrantId]=points
    if Save and cfg().championshipPersistence~=false then Save.SetNumber(roundPointKey(championshipId,roundId,entrantId),points) end
end
local function recalculateChampionship(championship)
    local champId=championship.id; local ids={player=true}
    for _,entrant in ipairs(RacingGameplay.data.motorsportEntrants or {}) do if entrant.enabled and ((championship.classId or 0)==0 or entrant.classId==championship.classId) then ids[entrant.id]=true end end
    local totals={}; local drop=math.max(0,championship.dropWorstRounds or 0); local rounds=RacingGameplay.GetChampionshipRounds(champId)
    for entrantId,_ in pairs(ids) do
        local scores={}
        for _,round in ipairs(rounds) do if round.enabled and roundCompleted(champId,round.id) then scores[#scores+1]=getRoundPoints(champId,round.id,entrantId) end end
        table.sort(scores)
        local total=0.0; for i=math.min(drop,#scores)+1,#scores do total=total+scores[i] end
        totals[entrantId]=total
        if Save and cfg().championshipPersistence~=false then Save.SetNumber("motorsport.championship."..tostring(champId)..".entrant."..tostring(entrantId)..".points",total) end
    end
    state.championship[champId]=totals
end

function RacingMotorsport.OnEventFinished(event,session,result)
    if not event then return end
    local rows={}
    if result and result.standings then
        for _,standing in ipairs(result.standings) do
            local entrantId=tonumber(string.match(tostring(standing.id or ""),"ai:(%d+)"))
            local dnf=false; if entrantId then for _,agent in ipairs(state.agents) do if agent.entrant.id==entrantId then dnf=agent.dnf==true break end end end
            rows[#rows+1]={entrantId=entrantId,id=standing.id,name=standing.name,position=standing.position,elapsedS=standing.elapsedS,bestLapS=standing.bestLapS or 0.0,completedLaps=standing.completedLaps or 0,finished=standing.finished,dnf=dnf}
        end
    end
    state.lastResults[event.id]=rows
    local championship,round=championshipForEvent(event.id)
    if championship and round and session and session.type=="Race" then
        local scheme=parsePoints(championship.pointsScheme); local multiplier=round.pointsMultiplier or 1.0
        local leaderLaps=0; for _,standing in ipairs((result and result.standings) or {}) do leaderLaps=math.max(leaderLaps,standing.completedLaps or 0) end
        local classification=(session.classificationPercent or 75.0)*0.01
        local awards={}; local fastestId=nil; local fastest=math.huge
        for _,standing in ipairs((result and result.standings) or {}) do
            local id=standing.id=="player" and "player" or tonumber(string.match(tostring(standing.id or ""),"ai:(%d+)"))
            local lap=tonumber(standing.bestLapS) or 0.0
            if id and lap>0 and lap<fastest then fastest=lap; fastestId=id end
        end
        for _,row in ipairs(rows) do
            local standing=(result and result.standings and result.standings[row.position]) or nil
            row.classified=standing and not row.dnf and ((standing.completedLaps or 0)>=leaderLaps*classification) or false
            if row.entrantId then awards[row.entrantId]=row.classified and ((scheme[row.position] or 0)*multiplier) or 0.0 end
        end
        for _,standing in ipairs((result and result.standings) or {}) do if standing.id=="player" then
            local playerClassified=(standing.completedLaps or 0)>=leaderLaps*classification and not (result and result.dnf)
            awards.player=playerClassified and ((scheme[standing.position] or 0)*multiplier) or 0.0
        end end
        local q=state.qualifying[event.id] or {}; if q[1] and q[1].entrantId then awards[q[1].entrantId]=(awards[q[1].entrantId] or 0.0)+(championship.poleBonus or 0.0)*multiplier end
        if fastestId then awards[fastestId]=(awards[fastestId] or 0.0)+(championship.fastestLapBonus or 0.0)*multiplier end
        for entrantId,points in pairs(awards) do setRoundPoints(championship.id,round.id,entrantId,points) end
        state.roundCompleted[championship.id]=state.roundCompleted[championship.id] or {}; state.roundCompleted[championship.id][round.id]=true
        if Save and cfg().championshipPersistence~=false then Save.SetNumber(roundCompletedKey(championship.id,round.id),1.0) end
        recalculateChampionship(championship)
        if Save and cfg().championshipPersistence~=false then Save.Flush() end
    end
    state.message="Session result captured for grid/championship progression."
end

function RacingMotorsport.GetGrid() return state.grid end
function RacingMotorsport.GetQualifying(eventId) return state.qualifying[eventId or (state.event and state.event.id)] or {} end
function RacingMotorsport.GetChampionshipStandings(championshipId)
    local championship=RacingGameplay.GetChampionship and RacingGameplay.GetChampionship(championshipId) or nil; if not championship then return {} end
    recalculateChampionship(championship)
    local tableByEntrant=state.championship[championship.id] or {}; local rows={}
    for _,entrant in ipairs(RacingGameplay.data.motorsportEntrants or {}) do
        if entrant.enabled and ((championship.classId or 0)==0 or entrant.classId==championship.classId) then
            local points=tableByEntrant[entrant.id]
            if points==nil and Save then points=Save.GetNumber("motorsport.championship."..tostring(championship.id)..".entrant."..tostring(entrant.id)..".points",0.0) end
            rows[#rows+1]={entrant=entrant,points=points or 0.0}
        end
    end
    local playerPoints=tableByEntrant.player; if playerPoints==nil and Save then playerPoints=Save.GetNumber("motorsport.championship."..tostring(championship.id)..".entrant.player.points",0.0) end
    rows[#rows+1]={entrant={id="player",driverName="Player",teamName="Player",raceNumber=0},points=playerPoints or 0.0}
    table.sort(rows,function(a,b) return a.points>b.points end); for i,row in ipairs(rows) do row.position=i end; return rows
end
function RacingMotorsport.GetClassStandings(classId)
    local result={}
    for _,row in ipairs((state.lastResults[state.event and state.event.id or 0]) or {}) do
        if row.entrantId then
            local entrant=RacingGameplay.GetMotorsportEntrant and RacingGameplay.GetMotorsportEntrant(row.entrantId) or nil
            if entrant and (classId==nil or classId==0 or entrant.classId==classId) then result[#result+1]={position=#result+1,entrant=entrant,overallPosition=row.position,completedLaps=row.completedLaps,elapsedS=row.elapsedS,bestLapS=row.bestLapS,dnf=row.dnf} end
        end
    end
    return result
end
-- STUDIO24 compact, representation-independent replay source. Positions are
-- floating-origin-safe globals so a replay clip survives origin rebases.
function RacingMotorsport.GetReplaySnapshot()
    local rows={}
    for index,agent in ipairs(state.agents) do
        local gx,gy,gz,yaw=nil,nil,nil,0.0
        local body=nil
        if agent.fullPhysicsHandle and agent.fullPhysicsHandle.body and Physics and Physics.BodyExists(agent.fullPhysicsHandle.body) then body=agent.fullPhysicsHandle.body
        elseif agent.proxy and agent.proxy.body and Physics and Physics.BodyExists(agent.proxy.body) then body=agent.proxy.body end
        if body then
            local x,y,z=Physics.GetBodyPosition(body)
            if x~=nil then gx,gy,gz=Physics.LocalToGlobal(x,y,z) end
            local _,ry,_=Physics.GetBodyRotation(body); yaw=ry or 0.0
        else
            gx,gy,gz,yaw=positionAtDistance(agent,agent.distance or 0.0)
        end
        if gx~=nil then
            local ai=agent.ai or {}; local feedback=agent.physicalFeedback or {}
            rows[#rows+1]={participantId=agent.participantId or ("ai:"..tostring(agent.entrant.id)),entrantId=agent.entrant.id,
                raceNumber=agent.entrant.raceNumber or 0,driverName=agent.entrant.driverName or "AI",backend=agent.physicalBackend or "Logical",
                x=gx,y=gy,z=gz,yawDeg=yaw or 0.0,speedKmh=(agent.currentMps or 0.0)*3.6,distanceM=agent.distance or 0.0,
                completedLaps=agent.completedLaps or 0,lateralOffsetM=ai.lateralOffsetM or 0.0,
                throttle=feedback.throttle or 0.0,brake=feedback.brake or 0.0,steering=feedback.steering or 0.0,
                pitMode=agent.pitMode or "",finished=agent.finished==true,dnf=agent.dnf==true,runningPosition=index}
        end
    end
    return rows
end

function RacingMotorsport.GetAiTelemetry()
    local rows={}
    for _,agent in ipairs(state.agents) do
        if RacingAI and RacingAI.GetTelemetry then
            local row=RacingAI.GetTelemetry(agent)
            if row then
                row.physicalBackend=agent.physicalBackend or "Logical"; row.mechanicalHealth=agent.mechanicalHealth or 1.0; row.pitArmed=agent.pitArmed==true; row.pitMode=agent.pitMode or ""; row.pitDistanceM=agent.pitDistance or 0.0; row.pitRouteLengthM=agent.pitRouteLength or 0.0; row.pitServiceDistanceM=agent.pitServiceDistance or 0.0
                local physical=agent.fullPhysicsHandle and RacingAIVehicleController and RacingAIVehicleController.GetTelemetry and RacingAIVehicleController.GetTelemetry(agent.fullPhysicsHandle) or nil
                if physical then for k,v in pairs(physical) do row[k]=v end end
                local racecraft=RacingAIRacecraft and RacingAIRacecraft.GetTelemetry and RacingAIRacecraft.GetTelemetry(agent) or nil
                if racecraft then for k,v in pairs(racecraft) do row[k]=v end end
                rows[#rows+1]=row
            end
        end
    end
    table.sort(rows,function(a,b) return (a.progressM or 0)>(b.progressM or 0) end)
    for i,row in ipairs(rows) do row.runningPosition=i end
    return rows
end
function RacingMotorsport.GetAiControlIntent(entrantId)
    for _,agent in ipairs(state.agents) do if agent.entrant and agent.entrant.id==entrantId then return RacingAI and RacingAI.GetControlIntent and RacingAI.GetControlIntent(agent) or nil end end
    return nil
end
function RacingMotorsport.GetTelemetry()
    local running,finished,dnf,pitting,slipstreaming,overtaking=0,0,0,0,0,0
    for _,agent in ipairs(state.agents) do
        if agent.dnf then dnf=dnf+1 elseif agent.finished then finished=finished+1 else running=running+1 end
        if agent.pitServiceRemainingS and agent.pitServiceRemainingS>0 then pitting=pitting+1 end
        if agent.ai and agent.ai.slipstream then slipstreaming=slipstreaming+1 end
        if agent.ai and agent.ai.overtaking then overtaking=overtaking+1 end
    end
    return {active=state.active,gridSize=#state.grid,agents=#state.agents,physical=state.physicalCount,fullPhysics=state.fullPhysicsCount,running=running,finished=finished,dnf=dnf,pitting=pitting,slipstreaming=slipstreaming,overtaking=overtaking,message=state.message}
end
function RacingMotorsport.Stop(clearCompetition)
    for _,agent in ipairs(state.agents) do destroyAgentProxy(agent) end
    state.active=false; state.event=nil; state.session=nil; state.venue=nil; state.grid={}; state.agents={}; state.physicalCount=0; state.fullPhysicsCount=0
    if clearCompetition then state.qualifying={}; state.lastResults={}; state.championship={}; state.roundPoints={}; state.roundCompleted={} end
end
