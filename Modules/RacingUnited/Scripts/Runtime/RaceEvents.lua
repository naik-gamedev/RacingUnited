-- STUDIO18 runtime execution authority for Studio-authored motorsport and
-- clandestine events. Consumes HRACE/HGAME data through RacingGameplay.
RacingEvents = RacingEvents or {}

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function distance2(a,b) local dx=(a.x or 0)-(b.x or 0); local dz=(a.z or 0)-(b.z or 0); return math.sqrt(dx*dx+dz*dz) end
local function playerBodyReady() return nativeVehicleBody~=nil and nativeVehicleBody~=0 and Physics.BodyExists(nativeVehicleBody) end
local function playerGlobalPosition()
    if not playerBodyReady() then return nil end
    local x,y,z=Physics.GetBodyPosition(nativeVehicleBody); if x==nil then return nil end
    local gx,gy,gz=Physics.LocalToGlobal(x,y,z); if gx==nil then return nil end
    return {x=gx,y=gy,z=gz}
end
local function playerSpeedKmh()
    if nativeVehicle~=nil and nativeVehicle~=0 and Vehicle.Exists(nativeVehicle) then return (Vehicle.GetSpeed(nativeVehicle) or 0.0)*3.6 end
    if playerBodyReady() then local x,y,z=Physics.GetBodyLinearVelocity(nativeVehicleBody); if x then return math.sqrt(x*x+y*y+z*z)*3.6 end end
    return 0.0
end
local function nowHour() if Environment and Environment.GetTimeOfDay then return Environment.GetTimeOfDay() or 12.0 end return 12.0 end
local function isNight(hour) return hour>=19.0 or hour<6.0 end
local function isCircuitType(t) return t=="Circuit Race" or t=="Clandestine Circuit" end
local function isClandestineType(t) return t=="Clandestine Circuit" or t=="Clandestine Sprint" end
local function isConeCourseType(t) return t=="Autoslalom" or t=="Gymkhana" end
local function isPointToPointType(t) return not isCircuitType(t) and not isConeCourseType(t) and t~="Cruise" and t~="Test Drive" end

local state = {
    phase="Idle", flag="Green", event=nil, venue=nil, session=nil, sessionIndex=1,
    stagingRemainingS=0.0,countdownRemainingS=0.0,sessionElapsedS=0.0,resultsRemainingS=0.0,formationArmed=false,
    completedLaps=0,currentLap=1,targetLaps=1,lapStartS=0.0,lastLapS=0.0,bestLapS=math.huge,
    expectedGateIndex=1,checkpoints={},sectorSplits={},lastGateId=0,gateDebounceS=0.0,startArmed=false,
    previousGlobal=nil,trackLimitWarnings=0,offTrackS=0.0,rejoinS=0.0,offTrackLatched=false,
    inPit=false,pitStops=0,pitServiceS=0.0,pitSpeedViolationCooldownS=0.0,pendingDriveThrough=false,
    falseStartIssued=false,totalPenaltyS=0.0,penalties={},finishAtNextLine=false,timedRaceExpired=false,plusOneLapPending=false,
    result=nil,message="No event running.",participants={},selectedEventIndex=1,selectedSessionIndex=1
}
RacingEvents.state=state

local function cfg() return RacingGameplay.GetEventExecutionConfiguration and RacingGameplay.GetEventExecutionConfiguration() or {} end
local function raceCfg() return RacingGameplay.GetRaceConfiguration and RacingGameplay.GetRaceConfiguration() or {} end
local function controlCfg() return RacingGameplay.GetRaceControl and RacingGameplay.GetRaceControl() or {} end

local function markerAppliesToEvent(marker,event)
    if RacingGameplay.MarkerAppliesToLayout then
        return RacingGameplay.MarkerAppliesToLayout(marker,event and (event.layoutId or 0) or 0)
    end
    local markerLayout=marker and (marker.layoutId or 0) or 0
    if markerLayout==0 then return true end
    local eventLayout=event and (event.layoutId or 0) or 0
    return eventLayout~=0 and markerLayout==eventLayout
end

local function eventMarker(markerId,event)
    local marker=RacingGameplay.GetRaceMarker(markerId)
    if markerAppliesToEvent(marker,event) then return marker end
    return nil
end

local function markerByType(markerType,event)
    local result={}
    for _,m in ipairs(RacingGameplay.data.raceMarkers or {}) do
        if m.type==markerType and markerAppliesToEvent(m,event) then result[#result+1]=m end
    end
    table.sort(result,function(a,b) return (a.order or 0)<(b.order or 0) end)
    return result
end

local function gateCrossed(marker,previous,current)
    if marker==nil or previous==nil or current==nil then return false end
    local dx=current.x-(marker.x or 0); local dz=current.z-(marker.z or 0)
    local radius=math.max(0.5,marker.radiusM or 4.0)
    if marker.directionRequired==false then
        local pdx=previous.x-(marker.x or 0); local pdz=previous.z-(marker.z or 0)
        return math.sqrt(pdx*pdx+pdz*pdz)>radius and math.sqrt(dx*dx+dz*dz)<=radius
    end
    local yaw=math.rad(marker.headingDeg or 0.0); local nx,nz=math.sin(yaw),math.cos(yaw); local rx,rz=math.cos(yaw),-math.sin(yaw)
    local pside=(previous.x-(marker.x or 0))*nx+(previous.z-(marker.z or 0))*nz
    local cside=dx*nx+dz*nz
    if not (pside<0.0 and cside>=0.0) then return false end
    local lateral=math.abs(dx*rx+dz*rz); local vertical=math.abs((current.y or marker.y or 0)-(marker.y or current.y or 0))
    return lateral<=math.max(radius,(marker.gateWidthM or 12.0)*0.5) and vertical<=math.max(2.0,(marker.gateHeightM or 4.0)*0.5+1.5)
end

local function stageAtMarker(marker)
    if marker==nil or not playerBodyReady() then return false end
    local x,y,z=Physics.GlobalToLocal(marker.x or 0,marker.y or 0,marker.z or 0); if x==nil then return false end
    local lift=(PrototypeCarDefinition and PrototypeCarDefinition.resetPosition and PrototypeCarDefinition.resetPosition[2]) or 0.15
    return ResetNativeVehicleAt(x,y+lift,z,"STUDIO18 event staging",0.0,marker.headingDeg or 0.0,0.0)
end

local function chooseStageMarker(event)
    local grids=markerByType("Grid Slot",event)
    if #grids>0 then
        table.sort(grids,function(a,b) return (a.slot or 0)<(b.slot or 0) end)
        return grids[1]
    end
    return eventMarker(event.startMarkerId,event) or (state.venue and state.venue.startFinish)
        or (isConeCourseType(event.type) and RacingConeCourse and RacingConeCourse.GetStartReference and RacingConeCourse.GetStartReference(event.id) or nil)
end

local function buildCheckpointSequence(event)
    local result={}
    for _,m in ipairs(RacingGameplay.data.raceMarkers or {}) do
        if markerAppliesToEvent(m,event) and m.id~=event.startMarkerId and m.id~=event.finishMarkerId
            and (m.type=="Checkpoint" or m.type=="Sector" or m.type=="Timing Loop") then result[#result+1]=m end
    end
    table.sort(result,function(a,b) return (a.order or 0)<(b.order or 0) end)
    return result
end

local function closestRouteCorridor(pos)
    local venue=state.venue
    if venue==nil or venue.routeNodes==nil or #venue.routeNodes<2 or pos==nil then return nil end
    local nodes=venue.routeNodes; local best=nil; local bestDist=math.huge; local count=#nodes
    local segmentCount=count-1
    if venue.route and venue.route.closedLoop then segmentCount=count end
    for i=1,segmentCount do
        local a=nodes[i]; local b=nodes[(i % count)+1]
        local sx=(b.x or 0)-(a.x or 0); local sz=(b.z or 0)-(a.z or 0); local len2=sx*sx+sz*sz
        if len2>0.0001 then
            local t=clamp(((pos.x-(a.x or 0))*sx+(pos.z-(a.z or 0))*sz)/len2,0.0,1.0)
            local px=(a.x or 0)+sx*t; local pz=(a.z or 0)+sz*t; local ox=pos.x-px; local oz=pos.z-pz; local d=math.sqrt(ox*ox+oz*oz)
            if d<bestDist then
                local len=math.sqrt(len2); local rightX,rightZ=sz/len,-sx/len; local signed=ox*rightX+oz*rightZ
                local left=(a.leftWidthM or 6.0)+((b.leftWidthM or a.leftWidthM or 6.0)-(a.leftWidthM or 6.0))*t
                local right=(a.rightWidthM or 6.0)+((b.rightWidthM or a.rightWidthM or 6.0)-(a.rightWidthM or 6.0))*t
                if venue.layout and venue.layout.reverse then left,right=right,left end
                bestDist=d; best={signed=signed,left=left,right=right,distance=d,nodeA=a,nodeB=b}
            end
        end
    end
    return best
end

local function addPenalty(kind,seconds,reason)
    seconds=math.max(0.0,seconds or 0.0); state.totalPenaltyS=state.totalPenaltyS+seconds
    state.penalties[#state.penalties+1]={kind=kind or "Time",seconds=seconds,reason=reason or kind or "Penalty",atS=state.sessionElapsedS}
    state.message=string.format("Penalty: %s +%.1fs",reason or kind or "Penalty",seconds)
end
RacingEvents.AddPenalty=addPenalty

local function saveBestLap()
    if state.event==nil or state.bestLapS==math.huge or cfg().autoSavePersonalBests==false then return end
    local key="race.event."..tostring(state.event.id)..".best_lap_s"
    local old=Save.GetNumber(key,0.0)
    if old<=0.0 or state.bestLapS<old then Save.SetNumber(key,state.bestLapS); Save.Flush() end
end

local function standings()
    local list={}
    for id,p in pairs(state.participants) do local copy={}; for k,v in pairs(p) do copy[k]=v end; copy.id=id; list[#list+1]=copy end
    table.sort(list,function(a,b)
        if (a.finished==true)~=(b.finished==true) then return a.finished==true end
        if (a.completedLaps or 0)~=(b.completedLaps or 0) then return (a.completedLaps or 0)>(b.completedLaps or 0) end
        if (a.checkpointIndex or 0)~=(b.checkpointIndex or 0) then return (a.checkpointIndex or 0)>(b.checkpointIndex or 0) end
        return (a.elapsedS or math.huge)<(b.elapsedS or math.huge)
    end)
    for i,p in ipairs(list) do p.position=i end
    return list
end

local function updatePlayerParticipant()
    local p=state.participants.player
    if p then p.completedLaps=state.completedLaps; p.checkpointIndex=state.expectedGateIndex-1; p.elapsedS=state.sessionElapsedS+state.totalPenaltyS; p.bestLapS=state.bestLapS<math.huge and state.bestLapS or 0.0; p.finished=state.phase=="Finished" or state.phase=="Results" end
end

local function finishEvent(reason,dnf)
    if state.phase=="Idle" or state.phase=="Results" then return end
    local mandatory=state.session and (state.session.mandatoryPitStops or 0) or 0
    if not dnf and state.session and state.session.type=="Race" and not isConeCourseType(state.event and state.event.type or "") and state.pitStops<mandatory then dnf=true; reason="mandatory pit service not completed" end
    state.phase="Finished"; state.flag="Chequered"; state.resultsRemainingS=math.max(0.0,cfg().resultsHoldSeconds or 12.0)
    saveBestLap(); updatePlayerParticipant()
    local adjusted=state.sessionElapsedS+state.totalPenaltyS
    local coneTelemetry=isConeCourseType(state.event and state.event.type or "") and RacingConeCourse and RacingConeCourse.GetTelemetry and RacingConeCourse.GetTelemetry() or nil
    local personalBestS,newPersonalBest=0.0,false
    if not dnf and coneTelemetry and cfg().autoSavePersonalBests~=false then
        local key="race.event."..tostring(state.event.id)..".best_time_s"; local old=Save.GetNumber(key,0.0)
        if old<=0.0 or adjusted<old then
            Save.SetNumber(key,adjusted)
            if RacingConeCourse and RacingConeCourse.CommitPersonalBest then RacingConeCourse.CommitPersonalBest(state.event.id) elseif Save.Flush then Save.Flush() end
            personalBestS=adjusted; newPersonalBest=true
        else personalBestS=old end
    end
    state.result={reason=reason or "Finished",dnf=dnf==true,elapsedS=state.sessionElapsedS,penaltyS=state.totalPenaltyS,adjustedS=adjusted,completedLaps=state.completedLaps,bestLapS=state.bestLapS<math.huge and state.bestLapS or 0.0,pitStops=state.pitStops,trackLimitWarnings=state.trackLimitWarnings,coneHits=coneTelemetry and coneTelemetry.coneHits or 0,courseGateCount=coneTelemetry and coneTelemetry.gateCount or 0,personalBestS=personalBestS,newPersonalBest=newPersonalBest,standings=standings()}
    if RacingMotorsport and RacingMotorsport.OnEventFinished then RacingMotorsport.OnEventFinished(state.event,state.session,state.result) end
    if RacingReplay and RacingReplay.OnEventFinished then RacingReplay.OnEventFinished(reason or "Event finished") end
    state.message=(dnf and "DNF: " or "Finished: ")..tostring(reason or "event complete")
end
RacingEvents.FinishEvent=finishEvent

local function completeLap()
    local lapTime=state.sessionElapsedS-state.lapStartS
    state.lastLapS=lapTime; state.bestLapS=math.min(state.bestLapS,lapTime); state.completedLaps=state.completedLaps+1; state.currentLap=state.completedLaps+1; state.lapStartS=state.sessionElapsedS
    state.expectedGateIndex=1; state.startArmed=false; state.sectorSplits={}
    state.message=string.format("Lap %d complete: %.3fs",state.completedLaps,lapTime)
    local sessionType=state.session and state.session.type or "Race"
    if sessionType=="Race" and state.session and state.session.timedRace then
        if state.finishAtNextLine then
            if state.plusOneLapPending then state.plusOneLapPending=false; state.message="Time expired: FINAL LAP" else finishEvent("timed race complete",false) end
        end
    elseif (sessionType=="Race" or sessionType=="Time Attack") and state.completedLaps>=state.targetLaps then finishEvent("distance complete",false) end
end

local function processTimingGates(previous,current)
    state.gateDebounceS=math.max(0.0,state.gateDebounceS)
    if state.gateDebounceS>0.0 then return end
    local expected=state.checkpoints[state.expectedGateIndex]
    if expected and gateCrossed(expected,previous,current) then
        state.lastGateId=expected.id; state.gateDebounceS=math.max(0.05,cfg().gateDebounceSeconds or 0.35)
        if expected.type=="Sector" then state.sectorSplits[#state.sectorSplits+1]={id=expected.id,timeS=state.sessionElapsedS-state.lapStartS} end
        state.expectedGateIndex=state.expectedGateIndex+1
        if state.expectedGateIndex>#state.checkpoints then state.startArmed=true end
        return
    end
    local startMarker=eventMarker(state.event.startMarkerId,state.event) or (state.venue and state.venue.startFinish)
    local finishMarker=eventMarker(state.event.finishMarkerId,state.event) or startMarker
    if startMarker and distance2(current,startMarker)>math.max(30.0,(startMarker.gateWidthM or 12.0)*2.0) and #state.checkpoints==0 then state.startArmed=true end
    if isCircuitType(state.event.type) then
        if state.startArmed and startMarker and gateCrossed(startMarker,previous,current) then
            state.gateDebounceS=math.max(0.05,cfg().gateDebounceSeconds or 0.35); state.lastGateId=startMarker.id
            completeLap()
        end
    elseif isPointToPointType(state.event.type) then
        local armed=(state.expectedGateIndex>#state.checkpoints) or #state.checkpoints==0
        if armed and finishMarker and gateCrossed(finishMarker,previous,current) then state.lastGateId=finishMarker.id; finishEvent("finish gate",false) end
    end
end

local function processPits(previous,current,dt)
    local pitEntry=markerByType("Pit Entry",state.event)[1]; local pitExit=markerByType("Pit Exit",state.event)[1]
    if not state.inPit and pitEntry and gateCrossed(pitEntry,previous,current) then state.inPit=true; state.pitServiceS=0.0; state.message="Pit lane entered." end
    if state.inPit and playerSpeedKmh()<2.0 then state.pitServiceS=state.pitServiceS+(dt or 0.0) end
    if state.inPit and pitExit and gateCrossed(pitExit,previous,current) then
        state.inPit=false
        if state.pendingDriveThrough then state.pendingDriveThrough=false; state.message="Drive-through penalty served."
        else
            local minimum=state.session and (state.session.minimumPitServiceSeconds or 0.0) or 0.0
            if state.pitServiceS+0.001>=minimum then state.pitStops=state.pitStops+1; state.message=string.format("Pit stop registered (%.1fs service).",state.pitServiceS)
            else state.message=string.format("Pit stop too short: %.1fs / %.1fs; mandatory stop not counted.",state.pitServiceS,minimum) end
        end
        state.pitServiceS=0.0
    end
    if state.inPit and state.pitSpeedViolationCooldownS<=0.0 then
        local limit=(raceCfg().pitSpeedKmh or 60.0); local speed=playerSpeedKmh()
        if speed>limit+1.5 then addPenalty("Pit Speed",5.0,string.format("Pit speed %.1f / %.1f km/h",speed,limit)); state.pitSpeedViolationCooldownS=5.0 end
    end
end

local function processTrackLimits(dt,pos)
    if raceCfg().trackLimitsEnabled==false or state.venue==nil then return end
    local corridor=closestRouteCorridor(pos); if corridor==nil then return end
    local outside=corridor.signed>corridor.right+0.6 or corridor.signed<-(corridor.left+0.6)
    if outside then
        state.rejoinS=0.0; state.offTrackS=state.offTrackS+dt
        if not state.offTrackLatched and state.offTrackS>=math.max(0.0,cfg().trackLimitGraceSeconds or 1.0) then
            state.offTrackLatched=true; state.trackLimitWarnings=state.trackLimitWarnings+1; state.message="Track limits warning "..tostring(state.trackLimitWarnings)
            local rc=controlCfg(); local maxWarnings=rc.maxTrackLimitWarnings or 3; local driveAfter=rc.driveThroughAfterWarnings or 5
            if raceCfg().penaltiesEnabled~=false and state.trackLimitWarnings>maxWarnings then addPenalty("Track Limits",5.0,"Track limits") end
            if raceCfg().penaltiesEnabled~=false and driveAfter>0 and state.trackLimitWarnings>=driveAfter then state.pendingDriveThrough=true end
        end
    else
        state.offTrackS=0.0
        if state.offTrackLatched then state.rejoinS=state.rejoinS+dt; if state.rejoinS>=math.max(0.0,cfg().trackLimitRejoinSeconds or 0.5) then state.offTrackLatched=false; state.rejoinS=0.0 end end
    end
end

local function processFlagSpeed(dt)
    local cap=nil
    if state.flag=="Full Course Yellow" then cap=cfg().fullCourseYellowSpeedKmh or 80.0
    elseif state.flag=="Virtual Safety Car" then cap=cfg().virtualSafetyCarSpeedKmh or 80.0
    elseif state.flag=="Safety Car" then cap=cfg().safetyCarSpeedKmh or 100.0
    elseif state.flag=="Red" then cap=5.0 end
    if cap and playerSpeedKmh()>cap+5.0 then
        state.flagViolationS=(state.flagViolationS or 0.0)+dt
        if state.flagViolationS>=2.0 then addPenalty("Flag Speed",5.0,state.flag.." speed violation"); state.flagViolationS=0.0 end
    else state.flagViolationS=0.0 end
end

function RacingEvents.RegisterParticipant(id,name)
    id=tostring(id or "participant")
    state.participants[id]={name=name or id,completedLaps=0,checkpointIndex=0,elapsedS=0.0,rawElapsedS=0.0,timePenaltyS=0.0,penalties={},bestLapS=0.0,finished=false}
    return id
end

function RacingEvents.AddParticipantPenalty(id,kind,seconds,reason)
    local p=state.participants[tostring(id)]; if not p then return false end
    seconds=math.max(0.0,tonumber(seconds) or 0.0)
    p.timePenaltyS=(p.timePenaltyS or 0.0)+seconds
    p.penalties=p.penalties or {}
    p.penalties[#p.penalties+1]={kind=kind or "Time",seconds=seconds,reason=reason or kind or "Penalty",atS=state.sessionElapsedS}
    p.elapsedS=(p.rawElapsedS or p.elapsedS or 0.0)+(p.timePenaltyS or 0.0)
    state.message=string.format("Steward: %s %s +%.1fs",p.name or tostring(id),reason or kind or "Penalty",seconds)
    return true
end

function RacingEvents.SetParticipantProgress(id,completedLaps,checkpointIndex,elapsedS,finished,bestLapS)
    local p=state.participants[tostring(id)]; if not p then return false end
    p.completedLaps=completedLaps or p.completedLaps; p.checkpointIndex=checkpointIndex or p.checkpointIndex
    p.rawElapsedS=elapsedS or p.rawElapsedS or p.elapsedS or 0.0; p.elapsedS=p.rawElapsedS+(p.timePenaltyS or 0.0)
    p.bestLapS=bestLapS or p.bestLapS; p.finished=finished==true; return true
end
function RacingEvents.GetStandings() updatePlayerParticipant(); return standings() end

local function resolveSession(sessionRef)
    local sessions=RacingGameplay.GetSessionChain and RacingGameplay.GetSessionChain() or {}
    if #sessions==0 then return {id=0,name="Race",type="Race",laps=0,durationMinutes=0,mandatoryPitStops=0,formationLap=false,rollingStart=false} end
    if sessionRef==nil then return sessions[clamp(state.selectedSessionIndex,1,#sessions)],clamp(state.selectedSessionIndex,1,#sessions) end
    if type(sessionRef)=="number" then
        for i,s in ipairs(sessions) do if s.id==sessionRef then return s,i end end
        return sessions[clamp(math.floor(sessionRef),1,#sessions)],clamp(math.floor(sessionRef),1,#sessions)
    end
    for i,s in ipairs(sessions) do if s.name==sessionRef then return s,i end end
    return sessions[1],1
end

function RacingEvents.StartEvent(eventRef,sessionRef)
    if cfg().enabled==false then return false,"Event execution is disabled in Heritage Studio gameplay policy." end
    local event=type(eventRef)=="table" and eventRef or RacingGameplay.GetEvent(eventRef)
    if event==nil or event.enabled==false then return false,"Event not found or disabled." end
    if event.nightOnly and not isNight(nowHour()) then return false,"This event is authored as night-only." end
    local venue=RacingGameplay.ResolveEventVenue(event); local session,sessionIndex=resolveSession(sessionRef)
    local resolvedVenue=venue and venue.venue or nil
    local startMarker=venue and venue.startMarker or nil
    if startMarker==nil and (event.startMarkerId or 0)==0 then startMarker=resolvedVenue and resolvedVenue.startFinish or nil end
    if startMarker==nil and isConeCourseType(event.type) and RacingConeCourse and RacingConeCourse.GetStartReference then startMarker=RacingConeCourse.GetStartReference(event.id) end
    local finishMarker=venue and venue.finishMarker or nil
    if startMarker==nil and event.type~="Cruise" and event.type~="Test Drive" then
        return false,isConeCourseType(event.type) and "Cone-course event has no start marker or Start cone." or "Event has no Start / Finish marker valid for its authored layout."
    end
    if isPointToPointType(event.type) and finishMarker==nil then
        return false,"Point-to-point event has no finish marker valid for its authored layout."
    end
    if isConeCourseType(event.type) then
        local gates=RacingGameplay.GetConeCourseGates and RacingGameplay.GetConeCourseGates(event.id) or {}
        if #gates==0 then return false,"Autoslalom/gymkhana event has no authored cone-course elements." end
        local hasFinish=false; for _,g in ipairs(gates) do if g.type=="Finish" then hasFinish=true break end end
        if not hasFinish then return false,"Autoslalom/gymkhana event needs a Finish course element." end
    end
    state.phase="Staging"; state.flag="Green"; state.event=event; state.venue=resolvedVenue; state.session=session; state.sessionIndex=sessionIndex or 1
    state.stagingRemainingS=math.max(0.0,cfg().gridSettleSeconds or 1.5); state.countdownRemainingS=math.max(0.0,cfg().countdownSeconds or 3.0); state.sessionElapsedS=0.0; state.resultsRemainingS=0.0
    state.completedLaps=0; state.currentLap=1; state.targetLaps=math.max(1,(session and session.laps or 0)>0 and session.laps or event.laps or (state.venue and state.venue.layout and state.venue.layout.defaultLaps) or 1)
    state.lapStartS=0.0; state.lastLapS=0.0; state.bestLapS=math.huge; state.checkpoints=buildCheckpointSequence(event); state.expectedGateIndex=1; state.sectorSplits={}; state.lastGateId=0; state.gateDebounceS=0.0; state.startArmed=false
    state.trackLimitWarnings=0; state.offTrackS=0.0; state.rejoinS=0.0; state.offTrackLatched=false; state.inPit=false; state.pitStops=0; state.pitServiceS=0.0; state.pitSpeedViolationCooldownS=0.0; state.pendingDriveThrough=false
    state.falseStartIssued=false; state.totalPenaltyS=0.0; state.penalties={}; state.finishAtNextLine=false; state.timedRaceExpired=false; state.plusOneLapPending=false; state.formationArmed=false; state.result=nil; state.participants={}; RacingEvents.RegisterParticipant("player","Player")
    if isConeCourseType(event.type) and RacingConeCourse and RacingConeCourse.BeginEvent then
        local ok,message=RacingConeCourse.BeginEvent(event); if not ok then state.phase="Idle"; state.event=nil; return false,message or "Could not arm cone course." end
    elseif RacingConeCourse and RacingConeCourse.EndEvent then RacingConeCourse.EndEvent(true) end
    if cfg().autoStagePlayer~=false then stageAtMarker(chooseStageMarker(event)) end
    state.previousGlobal=playerGlobalPosition(); state.message="Staging "..event.name.." / "..tostring(session and session.name or "Race")
    if isClandestineType(event.type) and RacingPolice and RacingPolice.BeginClandestineEvent then RacingPolice.BeginClandestineEvent(event.id) end
    if RacingMotorsport and RacingMotorsport.OnEventStarted then RacingMotorsport.OnEventStarted(event,session) end
    if RacingReplay and RacingReplay.OnEventStarted then RacingReplay.OnEventStarted(event,session) end
    return true,state.message
end

function RacingEvents.AbortEvent(reason)
    if RacingConeCourse and RacingConeCourse.EndEvent then RacingConeCourse.EndEvent(true) end
    if RacingReplay and RacingReplay.OnEventFinished then RacingReplay.OnEventFinished(reason or "Event aborted") end
    if RacingMotorsport and RacingMotorsport.Stop then RacingMotorsport.Stop(false) end
    state.phase="Idle"; state.flag="Green"; state.event=nil; state.venue=nil; state.session=nil; state.result=nil; state.message=reason or "Event aborted."
end

function RacingEvents.SetFlag(flag)
    local allowed={Green=true,["Local Yellow"]=true,["Full Course Yellow"]=true,["Virtual Safety Car"]=true,["Safety Car"]=true,Red=true,Chequered=true}
    if not allowed[flag] then return false end
    state.flag=flag; state.message="Race control: "..flag
    if flag=="Chequered" then state.finishAtNextLine=true end
    return true
end

function RacingEvents.AdvanceSession()
    if state.event==nil then return false,"No current event." end
    local sessions=RacingGameplay.GetSessionChain and RacingGameplay.GetSessionChain() or {}
    local nextIndex=(state.sessionIndex or 1)+1
    if nextIndex>#sessions then return false,"No next authored session." end
    state.selectedSessionIndex=nextIndex
    return RacingEvents.StartEvent(state.event,sessions[nextIndex].id)
end

function RacingEvents.FixedUpdate(dt)
    if state.gateDebounceS>0.0 then state.gateDebounceS=math.max(0.0,state.gateDebounceS-dt) end
    if state.pitSpeedViolationCooldownS>0.0 then state.pitSpeedViolationCooldownS=math.max(0.0,state.pitSpeedViolationCooldownS-dt) end
    if state.phase=="Idle" or state.event==nil then state.previousGlobal=playerGlobalPosition(); return end
    local pos=playerGlobalPosition(); if pos==nil then return end
    local previous=state.previousGlobal or pos
    if state.phase=="Staging" then
        state.stagingRemainingS=state.stagingRemainingS-dt
        if state.stagingRemainingS<=0.0 then
            if isConeCourseType(state.event.type) then state.phase="Countdown"; state.message="Cone course countdown"
            elseif state.session and state.session.formationLap then state.phase="Formation"; state.formationArmed=false; state.message="FORMATION LAP"
            elseif state.session and state.session.rollingStart then state.phase="Rolling Start"; state.countdownRemainingS=math.max(1.0,cfg().countdownSeconds or 3.0); state.message="ROLLING START"
            else state.phase="Countdown"; state.message="Countdown" end
        end
    elseif state.phase=="Formation" then
        local startMarker=eventMarker(state.event.startMarkerId,state.event) or (state.venue and state.venue.startFinish)
        if startMarker then
            if distance2(pos,startMarker)>math.max(30.0,(startMarker.gateWidthM or 12.0)*2.0) then state.formationArmed=true end
            if state.formationArmed and gateCrossed(startMarker,previous,pos) then
                state.formationArmed=false
                if state.session and state.session.rollingStart then state.phase="Rolling Start"; state.countdownRemainingS=math.max(1.0,cfg().countdownSeconds or 3.0); state.message="ROLLING START"
                else state.phase="Countdown"; state.countdownRemainingS=math.max(0.0,cfg().countdownSeconds or 3.0); state.message="Grid formed - countdown" end
            end
        end
    elseif state.phase=="Rolling Start" then
        state.countdownRemainingS=state.countdownRemainingS-dt
        if state.countdownRemainingS<=0.0 then state.phase="Running"; state.countdownRemainingS=0.0; state.sessionElapsedS=0.0; state.lapStartS=0.0; state.previousGlobal=pos; state.message="GREEN FLAG - rolling start" end
    elseif state.phase=="Countdown" then
        state.countdownRemainingS=state.countdownRemainingS-dt
        if not state.falseStartIssued and raceCfg().falseStartPenalty~=false and playerSpeedKmh()>math.max(0.0,cfg().falseStartSpeedKmh or 1.0) then state.falseStartIssued=true; addPenalty("False Start",5.0,"False start") end
        if state.countdownRemainingS<=0.0 then state.phase="Running"; state.countdownRemainingS=0.0; state.sessionElapsedS=0.0; state.lapStartS=0.0; state.previousGlobal=pos; state.message="GREEN FLAG" end
    elseif state.phase=="Running" then
        if state.flag~="Red" then state.sessionElapsedS=state.sessionElapsedS+dt end
        if isConeCourseType(state.event.type) and RacingConeCourse and RacingConeCourse.ProcessEvent then
            local course=RacingConeCourse.ProcessEvent(previous,pos,playerSpeedKmh(),dt,state.sessionElapsedS)
            if course and course.dnf then finishEvent(course.reason or "missed course element",true)
            elseif course and course.finished then finishEvent(course.reason or "cone course complete",false) end
        else
            processTimingGates(previous,pos)
            if state.phase=="Running" then processPits(previous,pos,dt); processTrackLimits(dt,pos) end
        end
        if state.phase=="Running" then processFlagSpeed(dt) end
        local duration=(state.session and state.session.durationMinutes or 0)*60.0
        local st=state.session and state.session.type or "Race"
        if state.phase=="Running" and duration>0.0 and state.sessionElapsedS>=duration and (st=="Practice" or st=="Qualifying" or st=="Warm-up" or st=="Time Attack") then finishEvent("session time expired",false) end
        if state.phase=="Running" and st=="Race" and state.session and state.session.timedRace and duration>0.0 and state.sessionElapsedS>=duration and not state.timedRaceExpired then
            state.timedRaceExpired=true; state.finishAtNextLine=true; state.plusOneLapPending=state.session.timePlusOneLap==true; state.flag="Chequered"; state.message=state.plusOneLapPending and "TIME EXPIRED - one lap after next crossing" or "TIME EXPIRED - finish at line"
        end
        if state.phase=="Running" and state.finishAtNextLine and not isCircuitType(state.event.type) then
            local startMarker=eventMarker(state.event.startMarkerId,state.event) or (state.venue and state.venue.startFinish)
            if startMarker and gateCrossed(startMarker,previous,pos) then finishEvent("chequered flag",false) end
        end
        if state.phase=="Running" and isClandestineType(state.event.type) and RacingPolice and RacingPolice.GetTelemetry then local p=RacingPolice.GetTelemetry(); if p and p.state=="Busted" then finishEvent("busted by police",true) end end
        updatePlayerParticipant()
    elseif state.phase=="Finished" then
        state.resultsRemainingS=state.resultsRemainingS-dt
        if state.resultsRemainingS<=0.0 then state.phase="Results"; if RacingConeCourse and RacingConeCourse.EndEvent then RacingConeCourse.EndEvent(true) end end
    end
    state.previousGlobal=pos
end

function RacingEvents.GetTelemetry()
    local best=state.bestLapS<math.huge and state.bestLapS or 0.0
    local currentLapTime=state.phase=="Running" and (state.sessionElapsedS-state.lapStartS) or 0.0
    local sessionCount=#(RacingGameplay.GetSessionChain and RacingGameplay.GetSessionChain() or {})
    local mandatory=state.session and state.session.mandatoryPitStops or 0
    local cone=isConeCourseType(state.event and state.event.type or "") and RacingConeCourse and RacingConeCourse.GetTelemetry and RacingConeCourse.GetTelemetry() or nil
    return {
        phase=state.phase,flag=state.flag,eventId=state.event and state.event.id or 0,eventName=state.event and state.event.name or "",eventType=state.event and state.event.type or "",
        sessionName=state.session and state.session.name or "",sessionType=state.session and state.session.type or "",sessionIndex=state.sessionIndex or 0,sessionCount=sessionCount,
        stagingRemainingS=math.max(0.0,state.stagingRemainingS),countdownRemainingS=math.max(0.0,state.countdownRemainingS),formationArmed=state.formationArmed,elapsedS=state.sessionElapsedS,
        currentLap=state.currentLap,targetLaps=state.targetLaps,completedLaps=state.completedLaps,currentLapTimeS=currentLapTime,lastLapS=state.lastLapS,bestLapS=best,
        checkpointIndex=cone and math.min(cone.gateIndex,cone.gateCount+1) or math.min(state.expectedGateIndex,#state.checkpoints+1),checkpointCount=cone and cone.gateCount or #state.checkpoints,lastGateId=cone and cone.lastGateId or state.lastGateId,sectorSplits=state.sectorSplits,
        coneCourse=cone,coneHits=cone and cone.coneHits or 0,
        trackLimitWarnings=state.trackLimitWarnings,inPit=state.inPit,pitStops=state.pitStops,pitServiceS=state.pitServiceS,mandatoryPitStops=mandatory,pendingDriveThrough=state.pendingDriveThrough,timedRaceExpired=state.timedRaceExpired,plusOneLapPending=state.plusOneLapPending,
        totalPenaltyS=state.totalPenaltyS,penaltyCount=#state.penalties,penalties=state.penalties,result=state.result,message=state.message,standings=standings()
    }
end

function RacingEvents.GetEnabledEvents() return RacingGameplay.GetEnabledEvents and RacingGameplay.GetEnabledEvents() or {} end
function RacingEvents.GetSelectedEvent()
    local events=RacingEvents.GetEnabledEvents(); if #events==0 then return nil end
    state.selectedEventIndex=clamp(state.selectedEventIndex,1,#events); return events[state.selectedEventIndex]
end
function RacingEvents.SelectNextEvent(delta)
    local events=RacingEvents.GetEnabledEvents(); if #events==0 then state.selectedEventIndex=1 return nil end
    local index=((state.selectedEventIndex-1+(delta or 1)) % #events)+1; state.selectedEventIndex=index; return events[index]
end
function RacingEvents.GetSelectedSession()
    local sessions=RacingGameplay.GetSessionChain and RacingGameplay.GetSessionChain() or {}; if #sessions==0 then return nil end
    state.selectedSessionIndex=clamp(state.selectedSessionIndex,1,#sessions); return sessions[state.selectedSessionIndex]
end
function RacingEvents.SelectNextSession(delta)
    local sessions=RacingGameplay.GetSessionChain and RacingGameplay.GetSessionChain() or {}; if #sessions==0 then state.selectedSessionIndex=1 return nil end
    local index=((state.selectedSessionIndex-1+(delta or 1)) % #sessions)+1; state.selectedSessionIndex=index; return sessions[index]
end
