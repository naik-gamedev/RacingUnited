-- STUDIO28 shared traffic-cone / autoslalom / gymkhana runtime.
-- Physical cones are props; invisible course elements remain deterministic even
-- after their visible cones have been knocked away.
RacingConeCourse = RacingConeCourse or {}

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function sqr(v) return v*v end
local function distanceXZ(a,b) return math.sqrt(sqr((a.x or 0)-(b.x or 0))+sqr((a.z or 0)-(b.z or 0))) end
local function cfg()
    return RacingGameplay.GetConeCourseConfiguration and RacingGameplay.GetConeCourseConfiguration() or {}
end
local function playerBodyReady()
    return nativeVehicleBody~=nil and nativeVehicleBody~=0 and Physics and Physics.BodyExists and Physics.BodyExists(nativeVehicleBody)
end

local state={
    started=false,activeEventId=0,handles={},courseGates={},expectedIndex=1,elementState={},
    hitCount=0,penaltyCount=0,lastConeId=0,lastGateId=0,message="Cone course idle.",finished=false,dnf=false,
    splits={},lastSplit=nil
}
RacingConeCourse.state=state

local function globalToLocal(x,y,z)
    if Physics and Physics.GlobalToLocal then
        local lx,ly,lz=Physics.GlobalToLocal(x or 0,y or 0,z or 0)
        if lx~=nil then return lx,ly,lz end
    end
    return x or 0,y or 0,z or 0
end
local function localToGlobal(x,y,z)
    if Physics and Physics.LocalToGlobal then
        local gx,gy,gz=Physics.LocalToGlobal(x or 0,y or 0,z or 0)
        if gx~=nil then return gx,gy,gz end
    end
    return x or 0,y or 0,z or 0
end

local function resolvedAsset(cone)
    local path=cone and cone.assetPath or ""
    if path==nil or path=="" then path=cfg().defaultAssetPath or "Assets/Props/TrafficCone.glb" end
    return path
end

local function coneColor(cone)
    if cone.trafficMode=="Close Road" or cone.trafficMode=="Close Lane" then return 0.95,0.16,0.10 end
    if cone.role=="Start" then return 0.18,0.88,0.30 end
    if cone.role=="Finish" then return 0.92,0.92,0.92 end
    if cone.role=="Gate Left" or cone.role=="Slalom Left" then return 0.12,0.48,0.95 end
    return 0.96,0.35,0.07
end

local function destroyHandle(handle)
    if not handle then return end
    if handle.body and handle.body~=0 and Physics and Physics.BodyExists and Physics.BodyExists(handle.body) then Physics.DestroyBody(handle.body) end
    if handle.visual and handle.visual~=0 and Entity and Entity.Exists(handle.visual) then Entity.Destroy(handle.visual) end
    if handle.root and handle.root~=0 and Entity and Entity.Exists(handle.root) then Entity.Destroy(handle.root) end
end

local function spawnCone(cone)
    if not Entity or not Physics or cone.enabled==false then return nil end
    local root=Entity.Create("Course Cone "..tostring(cone.id).." - "..tostring(cone.name or "Traffic Cone"))
    if root==0 then return nil end
    Entity.AddTag(root,"CourseCone")
    if (cone.trafficMode or "None")~="None" then Entity.AddTag(root,"TrafficControl") end
    if (cone.eventId or 0)~=0 then Entity.AddTag(root,"EventOverlay") end
    local height=math.max(0.15,tonumber(cone.heightM) or 0.70)
    local radius=math.max(0.04,tonumber(cone.baseRadiusM) or 0.18)
    local lx,ly,lz=globalToLocal(cone.x,cone.y,cone.z)
    Entity.SetLocalPosition(root,lx,ly+height*0.5,lz)
    Entity.SetLocalRotation(root,0.0,cone.headingDeg or 0.0,0.0)

    local body=0
    if cone.physical~=false then
        body=Physics.CreateBody(root,"dynamic",math.max(0.08,tonumber(cone.massKg) or 1.2))
        if body~=0 then
            Physics.SetBodyPosition(body,lx,ly+height*0.5,lz)
            Physics.SetBodyRotation(body,0.0,cone.headingDeg or 0.0,0.0)
            Physics.SetBodyAllowSleep(body,true)
            if Physics.SetBodyLinearDamping then Physics.SetBodyLinearDamping(body,0.18) end
            if Physics.SetBodyAngularDamping then Physics.SetBodyAngularDamping(body,0.12) end
            local friction=clamp(tonumber(cone.friction) or 0.72,0.0,2.0)
            local restitution=clamp(tonumber(cone.restitution) or 0.04,0.0,1.0)
            -- Broad low base + narrow upper body. Two boxes are inexpensive and
            -- make a light cone tumble/slide without a costly soft-body model.
            Physics.CreateBoxCollider(body,radius,math.max(0.025,height*0.055),radius,0.0,-height*0.445,0.0,friction,restitution,false)
            Physics.CreateBoxCollider(body,radius*0.40,height*0.38,radius*0.40,0.0,-height*0.03,0.0,friction,restitution,false)
        end
    end

    local visual=Entity.Create("Course Cone Visual "..tostring(cone.id))
    if visual~=0 then
        Entity.AddTag(visual,"CourseConeVisual")
        Entity.SetParent(visual,root,false)
        -- Authoring contract: cone GLB origin sits at the centre of its base,
        -- which matches the straightforward Blender modelling workflow.
        Entity.SetLocalPosition(visual,0.0,-height*0.5,0.0)
        Entity.SetLocalRotation(visual,0.0,0.0,0.0)
        local scale=math.max(0.01,tonumber(cone.visualScale) or 1.0)
        Entity.SetLocalScale(visual,scale,scale,scale)
        local asset=resolvedAsset(cone)
        local meshOk=false
        if asset~="" and Entity.SetMesh then meshOk=Entity.SetMesh(visual,asset,1.0,1.0,1.0,false,true,false)==true end
        if not meshOk then
            local r,g,b=coneColor(cone)
            Entity.SetLocalPosition(visual,0.0,0.0,0.0)
            Entity.SetLocalScale(visual,radius*2.0,height,radius*2.0)
            Entity.SetDebugPrimitive(visual,"box",r,g,b)
        end
    end

    return {cone=cone,root=root,visual=visual,body=body,start={x=cone.x or 0,y=cone.y or 0,z=cone.z or 0,headingDeg=cone.headingDeg or 0},
        penalized=false,playerContactPending=false,peakImpulseNs=0.0,lastContact=nil}
end

local function shouldExist(cone)
    if cone.enabled==false then return false end
    local eventId=cone.eventId or 0
    if eventId==0 then return true end
    if cfg().eventConesVisibleOnlyWhileActive==false then return true end
    return state.activeEventId~=0 and eventId==state.activeEventId
end

function RacingConeCourse.RefreshWorld()
    local wanted={}
    for _,cone in ipairs(RacingGameplay.data.courseCones or {}) do
        if shouldExist(cone) then
            wanted[cone.id]=true
            if not state.handles[cone.id] then state.handles[cone.id]=spawnCone(cone) end
        end
    end
    for id,handle in pairs(state.handles) do
        if not wanted[id] then destroyHandle(handle); state.handles[id]=nil end
    end
end

function RacingConeCourse.Start()
    if cfg().enabled==false then RacingConeCourse.Stop(); state.message="Cone system disabled by Heritage Studio."; return false end
    state.started=true
    RacingConeCourse.RefreshWorld()
    state.message="Traffic/course cones ready."
    return true
end

function RacingConeCourse.Stop()
    for _,handle in pairs(state.handles) do destroyHandle(handle) end
    state.handles={}; state.started=false; state.activeEventId=0; state.courseGates={}; state.expectedIndex=1; state.elementState={}; state.finished=false; state.dnf=false; state.splits={}; state.lastSplit=nil
end

function RacingConeCourse.ResetCone(coneId)
    local handle=state.handles[tonumber(coneId) or 0]
    if not handle then return false end
    local cone=handle.cone; local height=math.max(0.15,tonumber(cone.heightM) or 0.70)
    local x,y,z=globalToLocal(handle.start.x,handle.start.y,handle.start.z)
    if handle.body and handle.body~=0 and Physics.BodyExists(handle.body) then
        Physics.SetBodyPosition(handle.body,x,y+height*0.5,z); Physics.SetBodyRotation(handle.body,0.0,handle.start.headingDeg,0.0)
        if Physics.SetBodyLinearVelocity then Physics.SetBodyLinearVelocity(handle.body,0.0,0.0,0.0) end
        if Physics.SetBodyAngularVelocity then Physics.SetBodyAngularVelocity(handle.body,0.0,0.0,0.0) end
    elseif handle.root and Entity.Exists(handle.root) then
        Entity.SetLocalPosition(handle.root,x,y+height*0.5,z); Entity.SetLocalRotation(handle.root,0.0,handle.start.headingDeg,0.0)
    end
    handle.penalized=false; handle.playerContactPending=false; handle.peakImpulseNs=0.0; handle.lastContact=nil
    return true
end

function RacingConeCourse.ResetActiveEventCones()
    for id,handle in pairs(state.handles) do if (handle.cone.eventId or 0)==state.activeEventId then RacingConeCourse.ResetCone(id) end end
end

function RacingConeCourse.GetStartReference(eventId)
    local exact,persistent={},{}
    for _,cone in ipairs(RacingGameplay.data.courseCones or {}) do
        if cone.enabled~=false and cone.role=="Start" then
            if (cone.eventId or 0)==(eventId or 0) and (eventId or 0)~=0 then exact[#exact+1]=cone
            elseif (cone.eventId or 0)==0 then persistent[#persistent+1]=cone end
        end
    end
    local cones=#exact>0 and exact or persistent; if #cones==0 then return nil end
    local x,y,z=0.0,0.0,0.0; for _,cone in ipairs(cones) do x=x+(cone.x or 0); y=y+(cone.y or 0); z=z+(cone.z or 0) end
    x=x/#cones; y=y/#cones; z=z/#cones
    local heading=cones[1].headingDeg or 0.0; local yaw=math.rad(heading)
    -- Stage one car length behind the authored start line/cluster instead of
    -- spawning the chassis on top of a physical cone.
    x=x-math.sin(yaw)*3.0; z=z-math.cos(yaw)*3.0
    return {id=cones[1].id,name="Cone Course Start",type="Cone Start",x=x,y=y,z=z,headingDeg=heading,radiusM=2.0,gateWidthM=4.0,directionRequired=true}
end

function RacingConeCourse.BeginEvent(event)
    if not event then return false,"No event." end
    state.activeEventId=event.id or 0; state.courseGates=RacingGameplay.GetConeCourseGates and RacingGameplay.GetConeCourseGates(state.activeEventId) or {}
    state.expectedIndex=1; state.elementState={}; state.hitCount=0; state.penaltyCount=0; state.lastConeId=0; state.lastGateId=0; state.finished=false; state.dnf=false; state.splits={}; state.lastSplit=nil
    RacingConeCourse.RefreshWorld()
    if cfg().resetEventConesOnStart~=false then RacingConeCourse.ResetActiveEventCones() end
    state.message=string.format("Cone course armed: %d element(s).",#state.courseGates)
    return #state.courseGates>0,state.message
end

function RacingConeCourse.EndEvent(clearEventCones)
    state.activeEventId=0; state.courseGates={}; state.expectedIndex=1; state.elementState={}; state.finished=false; state.dnf=false
    if clearEventCones~=false then RacingConeCourse.RefreshWorld() end
end

local function planeCoordinates(gate,pos)
    local yaw=math.rad(gate.headingDeg or 0.0); local fx,fz=math.sin(yaw),math.cos(yaw); local rx,rz=math.cos(yaw),-math.sin(yaw)
    local dx=(pos.x or 0)-(gate.x or 0); local dz=(pos.z or 0)-(gate.z or 0)
    return dx*fx+dz*fz,dx*rx+dz*rz
end

local function basicGateCross(gate,previous,current)
    if not previous or not current then return false,false,0.0 end
    local pf,plat=planeCoordinates(gate,previous); local cf,clat=planeCoordinates(gate,current)
    local crossed
    if gate.directionRequired==false then crossed=(pf<0 and cf>=0) or (pf>0 and cf<=0) else crossed=pf<0 and cf>=0 end
    if not crossed then return false,false,clat end
    local within=math.abs(clat)<=math.max(0.25,(gate.widthM or 4.0)*0.5)
    return true,within,clat
end

-- Public pure helper used by regression tests and future multiplayer/course validation.
function RacingConeCourse.EvaluateGateCrossing(gate,previous,current,speedKmh,dt,memory)
    memory=memory or {}
    local kind=gate.type or "Gate"
    if kind=="Stop Box" then
        local f,l=planeCoordinates(gate,current)
        local inside=math.abs(l)<=math.max(0.25,(gate.widthM or 4.0)*0.5) and math.abs(f)<=math.max(0.25,(gate.lengthM or 4.0)*0.5)
        if inside and (speedKmh or 0.0)<=math.max(0.0,gate.stopSpeedKmh or 1.0) then memory.dwell=(memory.dwell or 0.0)+math.max(0.0,dt or 0.0) else memory.dwell=0.0 end
        if memory.dwell+1e-6>=math.max(0.0,gate.stopDwellS or 0.25) then return "passed",memory end
        return nil,memory
    end
    if kind=="Turnaround Left" or kind=="Turnaround Right" then
        local pf,plat=planeCoordinates(gate,previous or current); local cf,clat=planeCoordinates(gate,current)
        local half=math.max(0.5,(gate.widthM or 5.0)*0.5)
        if not memory.entered and pf<0 and cf>=0 and math.abs(clat)<=half then
            memory.entered=true; memory.wrongSide=(kind=="Turnaround Left" and clat>-(gate.sideClearanceM or 0.35)) or (kind=="Turnaround Right" and clat<(gate.sideClearanceM or 0.35))
        end
        if memory.entered and cf< -math.max(0.5,(gate.lengthM or 4.0)*0.5) then
            return memory.wrongSide and "wrong" or "passed",memory
        end
        return nil,memory
    end
    if kind=="360 Circle Left" or kind=="360 Circle Right" then
        -- A gymkhana circle is centered on the authored element (normally a
        -- single cone). Accumulate signed polar angle while the vehicle stays
        -- near the authored orbit. This makes a full 360 deterministic without
        -- requiring a dense necklace of timing gates.
        local dx=(current.x or 0)-(gate.x or 0); local dz=(current.z or 0)-(gate.z or 0)
        local distance=math.sqrt(dx*dx+dz*dz); local radius=math.max(0.5,(gate.widthM or 8.0)*0.5)
        local tolerance=math.max(0.5,gate.lengthM or 2.0)
        if distance<=radius+tolerance and distance>=math.max(0.35,radius-tolerance) then
            local angle=math.atan(dz,dx)
            if memory.circleLastAngle~=nil then
                local delta=angle-memory.circleLastAngle
                while delta>math.pi do delta=delta-2.0*math.pi end
                while delta< -math.pi do delta=delta+2.0*math.pi end
                memory.circleAccum=(memory.circleAccum or 0.0)+delta
            end
            memory.circleLastAngle=angle; memory.circleEntered=true
        elseif memory.circleEntered and distance>radius+tolerance*2.0 then
            -- Leaving the maneuver by a large margin before a revolution resets
            -- accumulated progress, so cutting across the world cannot complete it.
            memory.circleLastAngle=nil; memory.circleAccum=0.0; memory.circleEntered=false
        end
        local required=math.pi*2.0*0.90 -- tolerate sampling/trajectory noise near closure.
        local accum=memory.circleAccum or 0.0
        if kind=="360 Circle Left" then
            if accum>=required then return "passed",memory end
            if accum<=-math.pi*0.75 then return "wrong",memory end
        else
            if accum<=-required then return "passed",memory end
            if accum>=math.pi*0.75 then return "wrong",memory end
        end
        return nil,memory
    end
    local crossed,within,lateral=basicGateCross(gate,previous,current)
    if not crossed then return nil,memory end
    if not within then return "outside",memory end
    local clearance=math.max(0.0,gate.sideClearanceM or 0.35)
    if kind=="Slalom Left" and lateral> -clearance then return "wrong",memory end
    if kind=="Slalom Right" and lateral< clearance then return "wrong",memory end
    return "passed",memory
end

local function addCoursePenalty(gate,reason)
    local seconds=math.max(0.0,tonumber(gate and gate.wrongElementPenaltySeconds) or tonumber(cfg().wrongElementPenaltySeconds) or 10.0)
    if RacingEvents and RacingEvents.AddPenalty and seconds>0 then RacingEvents.AddPenalty("Cone Course",seconds,reason) end
    state.penaltyCount=state.penaltyCount+1; state.message=reason..string.format(" (+%.1fs)",seconds)
end

local function splitSaveKey(eventId,gateId)
    return "race.event."..tostring(eventId or 0)..".cone_split."..tostring(gateId or 0).."_s"
end

local function adjustedElapsed(rawElapsedS)
    local penalty=RacingEvents and RacingEvents.state and tonumber(RacingEvents.state.totalPenaltyS) or 0.0
    return math.max(0.0,tonumber(rawElapsedS) or 0.0)+math.max(0.0,penalty or 0.0)
end

local function recordSplit(gate,rawElapsedS,status)
    if not gate then return nil end
    local timeS=adjustedElapsed(rawElapsedS)
    local pb=0.0
    if Save and Save.GetNumber then pb=tonumber(Save.GetNumber(splitSaveKey(state.activeEventId,gate.id),0.0)) or 0.0 end
    local split={gateId=gate.id or 0,gateName=gate.name or gate.type or "Course element",gateType=gate.type or "Gate",status=status or "passed",timeS=timeS,pbTimeS=pb,deltaToPbS=(pb>0.0) and (timeS-pb) or 0.0,hasPb=pb>0.0}
    state.splits[#state.splits+1]=split; state.lastSplit=split
    return split
end

-- Called only when the event runtime confirms a new overall PB. Splits are
-- therefore one coherent reference run instead of unrelated per-gate records.
function RacingConeCourse.CommitPersonalBest(eventId)
    if not Save or not Save.SetNumber then return false end
    local id=tonumber(eventId) or state.activeEventId or 0; if id==0 or #state.splits==0 then return false end
    for _,split in ipairs(state.splits) do if (split.gateId or 0)~=0 then Save.SetNumber(splitSaveKey(id,split.gateId),split.timeS or 0.0) end end
    if Save.Flush then Save.Flush() end
    return true
end

function RacingConeCourse.ProcessEvent(previous,current,speedKmh,dt,elapsedS)
    if state.activeEventId==0 or #state.courseGates==0 or state.finished or state.dnf then return nil end
    local expected=state.courseGates[state.expectedIndex]
    if not expected then state.finished=true; return {finished=true,reason="cone course complete"} end
    local memory=state.elementState[expected.id] or {}; state.elementState[expected.id]=memory
    local result; result,memory=RacingConeCourse.EvaluateGateCrossing(expected,previous,current,speedKmh,dt,memory); state.elementState[expected.id]=memory
    if result=="passed" then
        local split=recordSplit(expected,elapsedS,"passed")
        state.lastGateId=expected.id; state.expectedIndex=state.expectedIndex+1; state.message=string.format("%s complete (%d/%d).",expected.name or expected.type,state.expectedIndex-1,#state.courseGates)
        if expected.type=="Finish" or state.expectedIndex>#state.courseGates then state.finished=true; return {finished=true,reason="cone course finish",split=split} end
        return {passed=true,gate=expected,split=split}
    elseif result=="wrong" then
        addCoursePenalty(expected,"Wrong side / direction at "..tostring(expected.name or expected.type)); local split=recordSplit(expected,elapsedS,"penalized"); state.lastGateId=expected.id; state.expectedIndex=state.expectedIndex+1
        if expected.type=="Finish" or state.expectedIndex>#state.courseGates then state.finished=true; return {finished=true,reason="cone course finish with penalty",split=split} end
        return {wrong=true,gate=expected,split=split}
    end

    -- Crossing a later element proves the expected one was skipped. This is
    -- robust against a cone itself being displaced because gates are invisible.
    for later=state.expectedIndex+1,#state.courseGates do
        local gate=state.courseGates[later]; local temp={}
        local laterResult=RacingConeCourse.EvaluateGateCrossing(gate,previous,current,speedKmh,dt,temp)
        if laterResult=="passed" or laterResult=="wrong" then
            local reason="Missed course element: "..tostring(expected.name or expected.type)
            if expected.dnfOnMiss==true then state.dnf=true; state.message=reason; return {dnf=true,reason=reason,gate=expected} end
            addCoursePenalty(expected,reason)
            state.expectedIndex=later+1; state.lastGateId=gate.id
            if laterResult=="wrong" then addCoursePenalty(gate,"Wrong side / direction at "..tostring(gate.name or gate.type)) end
            local split=recordSplit(gate,elapsedS,laterResult=="wrong" and "penalized" or "missed previous")
            if gate.type=="Finish" or state.expectedIndex>#state.courseGates then state.finished=true; return {finished=true,reason="cone course finish after missed element",split=split} end
            return {missed=true,gate=expected,split=split}
        end
    end
    return nil
end

local function coneGlobalPosition(handle)
    if not handle then return nil end
    if handle.body and handle.body~=0 and Physics.BodyExists(handle.body) then
        local x,y,z=Physics.GetBodyPosition(handle.body); if x~=nil then local gx,gy,gz=localToGlobal(x,y,z); return {x=gx,y=gy,z=gz} end
    end
    return handle.start
end

local function issueConePenalty(handle,contact)
    if handle.penalized then return end
    handle.penalized=true; state.hitCount=state.hitCount+1; state.lastConeId=handle.cone.id
    local seconds=math.max(0.0,tonumber(handle.cone.hitPenaltySeconds) or tonumber(cfg().defaultHitPenaltySeconds) or 2.0)
    if RacingEvents and RacingEvents.AddPenalty and seconds>0 then RacingEvents.AddPenalty("Cone",seconds,"Cone "..tostring(handle.cone.name or handle.cone.id)) end
    state.penaltyCount=state.penaltyCount+(seconds>0 and 1 or 0); state.message=string.format("Cone %s displaced/struck%s",tostring(handle.cone.name or handle.cone.id),seconds>0 and string.format(" (+%.1fs)",seconds) or "")
    if cfg().recordConeHitsToReplay~=false and RacingReplay and RacingReplay.MarkIncident then
        local p=coneGlobalPosition(handle) or handle.start
        RacingReplay.MarkIncident({id=10000000+(handle.cone.id or 0),classification="Cone strike",verdict="Course penalty",faultEntrantId=0,driverName="Player",otherName=handle.cone.name or "Traffic Cone",globalX=p.x,globalY=p.y,globalZ=p.z,normalImpulseNs=contact and contact.normalImpulseNs or handle.peakImpulseNs})
    end
end

local function inspectPlayerContact(handle)
    if not playerBodyReady() or not handle.body or handle.body==0 or not Physics.GetBodyContactCount or not Physics.GetBodyContact then return nil end
    local minImpulse=math.max(0.0,tonumber(cfg().minimumContactImpulseNs) or 1.0)
    local count=Physics.GetBodyContactCount(handle.body) or 0; local best=nil; local bestImpulse=0.0
    for i=1,count do
        local c=Physics.GetBodyContact(handle.body,i)
        if type(c)=="table" and c.trigger~=true and tonumber(c.otherBody)==nativeVehicleBody then
            local impulse=math.abs(tonumber(c.normalImpulseNs) or 0.0)
            if impulse>=minImpulse and impulse>=bestImpulse then best=c; bestImpulse=impulse end
        end
    end
    if best then handle.playerContactPending=true; handle.peakImpulseNs=math.max(handle.peakImpulseNs,bestImpulse); handle.lastContact=best end
    return best
end

function RacingConeCourse.FixedUpdate(dt)
    if not state.started or cfg().enabled==false then return end
    -- Only event-associated cones issue competition penalties. Persistent
    -- traffic-control cones remain fully physical without turning free roam
    -- into a penalty session.
    if state.activeEventId==0 then return end
    for _,handle in pairs(state.handles) do
        local cone=handle.cone
        if (cone.eventId or 0)==state.activeEventId and cone.physical~=false and not handle.penalized then
            local contact=inspectPlayerContact(handle)
            local mode=cone.penaltyMode or "Displaced"
            if mode=="Contact" and contact then issueConePenalty(handle,contact)
            elseif handle.playerContactPending and mode=="Displaced" then
                local p=coneGlobalPosition(handle); if p and distanceXZ(p,handle.start)>=math.max(0.01,tonumber(cone.displacementToleranceM) or tonumber(cfg().defaultDisplacementToleranceM) or 0.12) then issueConePenalty(handle,handle.lastContact) end
            elseif handle.playerContactPending and mode=="Knocked Down" and handle.body and Physics.BodyExists(handle.body) then
                local rx,_,rz=Physics.GetBodyRotation(handle.body); if math.max(math.abs(rx or 0),math.abs(rz or 0))>=50.0 then issueConePenalty(handle,handle.lastContact) end
            end
        end
    end
end

local function coneTargetsLink(cone,link)
    if not cone or not link then return false end
    if (cone.linkId or 0)~=0 then return cone.linkId==link.id end
    if (cone.roadId or 0)==0 then return false end
    local a=RacingGameplay.GetTrafficNode and RacingGameplay.GetTrafficNode(link.fromNodeId) or nil
    local b=RacingGameplay.GetTrafficNode and RacingGameplay.GetTrafficNode(link.toNodeId) or nil
    if not ((a and a.roadId==cone.roadId) or (b and b.roadId==cone.roadId)) then return false end
    if (cone.laneIndex or -1)>=0 then
        return (a and a.laneIndex==cone.laneIndex) or (b and b.laneIndex==cone.laneIndex)
    end
    return true
end

function RacingConeCourse.GetTrafficControlsForLink(link)
    local result={}
    for _,cone in ipairs(RacingGameplay.data.courseCones or {}) do
        local eventId=cone.eventId or 0
        local active=eventId==0 or (state.activeEventId~=0 and eventId==state.activeEventId)
        if active and cone.enabled~=false and (cone.trafficMode or "None")~="None" and coneTargetsLink(cone,link) then result[#result+1]=cone end
    end
    return result
end

function RacingConeCourse.ApplyTrafficControls(link,cost,speedLimit)
    local resultCost=cost; local resultSpeed=speedLimit
    for _,cone in ipairs(RacingConeCourse.GetTrafficControlsForLink(link)) do
        local mode=cone.trafficMode or "None"
        if mode=="Close Road" or mode=="Close Lane" then return nil,nil,cone end
        if mode=="Guide" or mode=="Guide / Discourage" or mode=="Slow" then resultCost=resultCost*math.max(0.001,tonumber(cone.routeCostMultiplier) or 1.0) end
        if (mode=="Slow" or mode=="Guide" or mode=="Guide / Discourage") and (tonumber(cone.trafficSpeedLimitKmh) or 0)>0 then resultSpeed=math.min(resultSpeed,cone.trafficSpeedLimitKmh) end
    end
    return resultCost,resultSpeed,nil
end

function RacingConeCourse.GetTelemetry()
    local expected=state.courseGates[state.expectedIndex]
    local last=state.lastSplit
    return {activeEventId=state.activeEventId,active=state.activeEventId~=0,gateIndex=math.min(state.expectedIndex,#state.courseGates+1),gateCount=#state.courseGates,
        expectedGateId=expected and expected.id or 0,expectedGateName=expected and expected.name or "",lastGateId=state.lastGateId,lastConeId=state.lastConeId,
        coneHits=state.hitCount,penaltyCount=state.penaltyCount,finished=state.finished,dnf=state.dnf,message=state.message,
        splitCount=#state.splits,lastSplit=last,lastSplitName=last and last.gateName or "",lastSplitTimeS=last and last.timeS or 0.0,lastSplitDeltaS=last and last.deltaToPbS or 0.0,lastSplitHasPb=last and last.hasPb or false}
end
