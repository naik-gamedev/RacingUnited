-- STUDIO24 incident replay recorder + STUDIO25/26/27 replay/broadcast camera director.
-- Keeps a bounded rolling pre-impact buffer, seals short post-impact clips from
-- STUDIO23 solver-contact evidence, renders scrub-able non-physical ghosts, and
-- can direct a floating-origin-safe FP64 review camera without touching race authority.
RacingReplay = RacingReplay or {}

local state = {
    recording=false,event=nil,session=nil,timeS=0.0,sampleAccumulatorS=0.0,
    rollingFrames={},clips={},activeClips={},nextClipId=1,
    selectedClipId=0,reviewOffsetS=0.0,reviewPlaying=false,reviewEnabled=false,
    ghostEntities={},cameraMode="Off",cameraActive=false,cameraPose=nil,authoredCameraId=0,authoredCameraPathId=0,
    message="No incident replay session recorded yet."
}
RacingReplay.state=state

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function cfg()
    if RacingGameplay and RacingGameplay.GetMotorsportReplayConfiguration then return RacingGameplay.GetMotorsportReplayConfiguration() or {} end
    return {}
end

local function destroyGhosts()
    if Entity then
        for _,entity in pairs(state.ghostEntities) do
            if entity and entity~=0 and Entity.Exists and Entity.Exists(entity) then Entity.Destroy(entity) end
        end
    end
    state.ghostEntities={}
end

local function disableReviewCamera()
    if state.cameraActive and Camera and Camera.IsAvailable and Camera.IsAvailable() and Camera.SetWorldViewActive then
        Camera.SetWorldViewActive(false)
    end
    state.cameraActive=false
    state.cameraPose=nil
    state.authoredCameraId=0
    state.authoredCameraPathId=0
end

local function playerSnapshot()
    local c=cfg()
    if c.capturePlayer==false or not Physics or nativeVehicleBody==nil or nativeVehicleBody==0 or not Physics.BodyExists(nativeVehicleBody) then return nil end
    local x,y,z=Physics.GetBodyPosition(nativeVehicleBody); if x==nil then return nil end
    local gx,gy,gz=Physics.LocalToGlobal(x,y,z); if gx==nil then return nil end
    local rx,ry,rz=Physics.GetBodyRotation(nativeVehicleBody); local vx,vy,vz=Physics.GetBodyLinearVelocity(nativeVehicleBody)
    local speed=0.0; if vx~=nil then speed=math.sqrt(vx*vx+vy*vy+vz*vz)*3.6 end
    local throttle,brake,steering=0.0,0.0,0.0
    if c.captureControls~=false then
        if ReadVehicleDriveInputs then throttle,brake=ReadVehicleDriveInputs() end
        if ReadVehicleSteeringInput then steering=ReadVehicleSteeringInput() end
    end
    return {participantId="player",entrantId=0,raceNumber=0,driverName="Player",backend="Player Heritage Vehicle",
        x=gx,y=gy,z=gz,yawDeg=ry or 0.0,speedKmh=speed,distanceM=0.0,completedLaps=0,lateralOffsetM=0.0,
        throttle=throttle or 0.0,brake=brake or 0.0,steering=steering or 0.0,pitMode="",finished=false,dnf=false}
end

local function captureFrame()
    local racers={}
    local player=playerSnapshot(); if player then racers[#racers+1]=player end
    local available=(RacingMotorsport and RacingMotorsport.GetReplaySnapshot and RacingMotorsport.GetReplaySnapshot()) or {}
    local maximum=math.max(1,math.min(200,math.floor(cfg().maximumRecordedCompetitors or 32)))
    local added=0
    for _,row in ipairs(available) do
        if added>=maximum then break end
        if cfg().captureControls==false then row.throttle=nil; row.brake=nil; row.steering=nil end
        racers[#racers+1]=row; added=added+1
    end
    local rt=(RacingEvents and RacingEvents.GetTelemetry and RacingEvents.GetTelemetry()) or {}
    return {timeS=state.timeS,phase=rt.phase or "",flag=rt.flag or "Green",racers=racers}
end

local function rollingFrameLimit()
    local c=cfg(); local hz=clamp(tonumber(c.sampleHz) or 12.0,1.0,60.0)
    return math.max(2,math.ceil(math.max(0.5,tonumber(c.preRollSeconds) or 8.0)*hz)+2)
end

local function trimClips()
    local keep=math.max(1,math.min(64,math.floor(cfg().maximumIncidentClips or 12)))
    while #state.clips>keep do
        local removed=table.remove(state.clips,1)
        if removed then
            for i=#state.activeClips,1,-1 do if state.activeClips[i]==removed then table.remove(state.activeClips,i) end end
            if removed.id==state.selectedClipId then state.selectedClipId=0; state.reviewEnabled=false; state.reviewPlaying=false; destroyGhosts(); disableReviewCamera() end
        end
    end
end

local function finalizeClip(clip)
    clip.capturing=false
    clip.endTimeS=(clip.frames[#clip.frames] and clip.frames[#clip.frames].timeS) or clip.incidentTimeS
    clip.durationS=math.max(0.0,clip.endTimeS-clip.startTimeS)
    state.message=string.format("Incident replay #%d sealed: %.1fs around %s.",clip.id,clip.durationS,clip.classification or "contact")
    trimClips()
end

local function sampleNow()
    local frame=captureFrame()
    state.rollingFrames[#state.rollingFrames+1]=frame
    local limit=rollingFrameLimit(); while #state.rollingFrames>limit do table.remove(state.rollingFrames,1) end
    for i=#state.activeClips,1,-1 do
        local clip=state.activeClips[i]
        clip.frames[#clip.frames+1]=frame
        if frame.timeS >= clip.postUntilTimeS then
            finalizeClip(clip); table.remove(state.activeClips,i)
        end
    end
end

function RacingReplay.OnEventStarted(event,session)
    destroyGhosts(); disableReviewCamera()
    state.recording=cfg().enabled~=false
    state.event=event; state.session=session; state.timeS=0.0; state.sampleAccumulatorS=0.0
    state.rollingFrames={}; state.clips={}; state.activeClips={}; state.nextClipId=1
    state.selectedClipId=0; state.reviewOffsetS=0.0; state.reviewPlaying=false; state.reviewEnabled=false; state.cameraMode="Off"; state.cameraPose=nil; state.authoredCameraId=0; state.authoredCameraPathId=0
    state.message=state.recording and "STUDIO27 incident replay + static/moving authored broadcast cameras armed." or "Incident replay capture is disabled in Heritage Studio."
    if state.recording then sampleNow() end
end

function RacingReplay.OnEventFinished(reason)
    state.recording=false
    for i=#state.activeClips,1,-1 do finalizeClip(state.activeClips[i]); table.remove(state.activeClips,i) end
    if #state.clips>0 then state.message="Race/session ended; incident replay clips remain available for review."
    else state.message=reason or "Race/session ended with no retained incident replay clips." end
end

function RacingReplay.Clear()
    destroyGhosts(); disableReviewCamera(); state.recording=false; state.event=nil; state.session=nil; state.timeS=0.0; state.sampleAccumulatorS=0.0
    state.rollingFrames={}; state.clips={}; state.activeClips={}; state.nextClipId=1; state.selectedClipId=0
    state.reviewOffsetS=0.0; state.reviewPlaying=false; state.reviewEnabled=false; state.cameraMode="Off"; state.cameraPose=nil; state.authoredCameraId=0; state.authoredCameraPathId=0; state.message="Incident replay memory cleared."
end

function RacingReplay.MarkIncident(incident)
    if not state.recording or not incident or cfg().enabled==false then return nil end
    local clip={id=state.nextClipId,incidentId=incident.id or 0,classification=incident.classification or "Contact",verdict=incident.verdict or "Review",
        faultEntrantId=incident.faultEntrantId or 0,driverName=incident.driverName or "",otherName=incident.otherName or "",
        incidentTimeS=state.timeS,startTimeS=state.timeS,postUntilTimeS=state.timeS+math.max(0.5,tonumber(cfg().postRollSeconds) or 5.0),
        frames={},capturing=true,incident=incident}
    state.nextClipId=state.nextClipId+1
    for _,frame in ipairs(state.rollingFrames) do clip.frames[#clip.frames+1]=frame end
    if #clip.frames>0 then clip.startTimeS=clip.frames[1].timeS end
    state.clips[#state.clips+1]=clip; state.activeClips[#state.activeClips+1]=clip; trimClips()
    if state.selectedClipId==0 then state.selectedClipId=clip.id; state.reviewOffsetS=math.max(0.0,clip.incidentTimeS-clip.startTimeS) end
    state.message=string.format("Capturing incident replay #%d: %.1fs pre-roll + %.1fs post-roll.",clip.id,math.max(0.0,clip.incidentTimeS-clip.startTimeS),math.max(0.5,tonumber(cfg().postRollSeconds) or 5.0))
    return clip.id
end

function RacingReplay.FixedUpdate(dt)
    if not state.recording or cfg().enabled==false then return end
    dt=math.max(0.0,tonumber(dt) or 0.0); state.timeS=state.timeS+dt; state.sampleAccumulatorS=state.sampleAccumulatorS+dt
    local interval=1.0/clamp(tonumber(cfg().sampleHz) or 12.0,1.0,60.0)
    local samples=0
    while state.sampleAccumulatorS>=interval and samples<4 do
        state.sampleAccumulatorS=state.sampleAccumulatorS-interval; sampleNow(); samples=samples+1
    end
end

local function clipById(id)
    for _,clip in ipairs(state.clips) do if clip.id==id then return clip end end
    return nil
end

local function clipDuration(clip)
    if not clip then return 0.0 end
    if clip.durationS~=nil then return math.max(0.0,clip.durationS) end
    local last=clip.frames and clip.frames[#clip.frames] or nil
    local endTime=(last and last.timeS) or clip.endTimeS or clip.incidentTimeS or clip.startTimeS
    return math.max(0.0,endTime-(clip.startTimeS or endTime))
end

function RacingReplay.GetClips()
    local out={}
    for i=#state.clips,1,-1 do out[#out+1]=state.clips[i] end
    return out
end

function RacingReplay.SelectClip(id)
    local clip=clipById(tonumber(id) or 0); if not clip then return false end
    state.selectedClipId=clip.id; state.reviewOffsetS=clamp(clip.incidentTimeS-clip.startTimeS,0.0,clipDuration(clip)); state.reviewPlaying=false; state.cameraPose=nil
    state.message=string.format("Selected replay #%d (%s).",clip.id,clip.classification or "Contact")
    return true
end

function RacingReplay.SelectRelative(delta)
    if #state.clips==0 then return false end
    local index=1
    for i,clip in ipairs(state.clips) do if clip.id==state.selectedClipId then index=i break end end
    index=((index-1+(delta or 0))%#state.clips)+1
    return RacingReplay.SelectClip(state.clips[index].id)
end

function RacingReplay.SetReviewEnabled(value)
    if value and not clipById(state.selectedClipId) then if #state.clips==0 then return false end; RacingReplay.SelectClip(state.clips[#state.clips].id) end
    state.reviewEnabled=value==true and cfg().ghostReviewEnabled~=false
    if state.reviewEnabled and cfg().broadcastDirectorEnabled~=false and cfg().autoIncidentCamera~=false then
        state.cameraMode="Incident"; state.cameraPose=nil
    end
    if not state.reviewEnabled then state.reviewPlaying=false; destroyGhosts(); disableReviewCamera() end
    return state.reviewEnabled
end

function RacingReplay.ToggleReview() return RacingReplay.SetReviewEnabled(not state.reviewEnabled) end
function RacingReplay.SetReviewPlaying(value)
    if value==true and not state.reviewEnabled then RacingReplay.SetReviewEnabled(true) end
    state.reviewPlaying=value==true and state.reviewEnabled==true
end
function RacingReplay.SetReviewOffset(seconds)
    local clip=clipById(state.selectedClipId); if not clip then return false end
    local duration=clipDuration(clip); state.reviewOffsetS=clamp(tonumber(seconds) or 0.0,0.0,duration); state.reviewPlaying=false; return true
end
function RacingReplay.StepReview(seconds) return RacingReplay.SetReviewOffset(state.reviewOffsetS+(tonumber(seconds) or 0.0)) end
function RacingReplay.JumpToIncident()
    local clip=clipById(state.selectedClipId); if not clip then return false end
    return RacingReplay.SetReviewOffset(clip.incidentTimeS-clip.startTimeS)
end

local cameraModes={"Incident","Trackside","Spline","Chase","Helicopter","Off"}
function RacingReplay.SetCameraMode(mode)
    mode=tostring(mode or "Incident")
    local valid=false; for _,v in ipairs(cameraModes) do if v==mode then valid=true break end end
    if not valid then return false end
    state.cameraMode=mode; state.cameraPose=nil; state.authoredCameraId=0; state.authoredCameraPathId=0
    if mode=="Off" then disableReviewCamera() end
    return true
end
function RacingReplay.CycleCameraMode(delta)
    local index=1; for i,v in ipairs(cameraModes) do if v==state.cameraMode then index=i break end end
    index=((index-1+(delta or 1))%#cameraModes)+1
    return RacingReplay.SetCameraMode(cameraModes[index])
end

local function framePair(clip,absoluteTime)
    if not clip or #clip.frames==0 then return nil,nil,0.0 end
    local a=clip.frames[1]; local b=a
    for i=2,#clip.frames do
        b=clip.frames[i]
        if b.timeS>=absoluteTime then
            local span=math.max(0.0001,b.timeS-a.timeS); return a,b,clamp((absoluteTime-a.timeS)/span,0.0,1.0)
        end
        a=b
    end
    return a,a,0.0
end

local function stateMap(frame)
    local map={}; if not frame then return map end
    for _,r in ipairs(frame.racers or {}) do map[tostring(r.participantId or r.entrantId or #map+1)]=r end
    return map
end
local function lerp(a,b,t) return a+(b-a)*t end
local function lerpAngle(a,b,t)
    local d=((b-a+180.0)%360.0)-180.0; return a+d*t
end

local function interpolatedRacer(ra,rb,t)
    rb=rb or ra
    return {
        participantId=ra.participantId,entrantId=ra.entrantId or 0,driverName=ra.driverName or "",raceNumber=ra.raceNumber or 0,
        x=lerp(ra.x or 0.0,rb.x or ra.x or 0.0,t),y=lerp(ra.y or 0.0,rb.y or ra.y or 0.0,t),z=lerp(ra.z or 0.0,rb.z or ra.z or 0.0,t),
        yawDeg=lerpAngle(ra.yawDeg or 0.0,rb.yawDeg or ra.yawDeg or 0.0,t),
        speedKmh=lerp(ra.speedKmh or 0.0,rb.speedKmh or ra.speedKmh or 0.0,t)
    }
end

local function racerForEntrant(ma,mb,entrantId,t)
    entrantId=tonumber(entrantId) or 0; if entrantId==0 then return nil end
    local key="ai:"..tostring(entrantId); local ra=ma[key]
    if not ra then for _,candidate in pairs(ma) do if tonumber(candidate.entrantId) == entrantId then ra=candidate; key=tostring(candidate.participantId or key); break end end end
    if not ra then return nil end
    return interpolatedRacer(ra,mb[key] or ra,t)
end

local function cameraLookAngles(px,py,pz,tx,ty,tz)
    local dx,dy,dz=tx-px,ty-py,tz-pz
    local horizontal=math.sqrt(dx*dx+dz*dz); local length=math.sqrt(horizontal*horizontal+dy*dy)
    if length<0.001 then return 0.0,0.0 end
    return math.deg(math.atan(dy,math.max(0.0001,horizontal))),math.deg(math.atan(dx,dz))
end

local function pointSegmentDistance2(px,pz,ax,az,bx,bz)
    local vx,vz=bx-ax,bz-az; local wx,wz=px-ax,pz-az
    local denom=vx*vx+vz*vz
    local t=denom>0.000001 and clamp((wx*vx+wz*vz)/denom,0.0,1.0) or 0.0
    local dx,dz=px-(ax+vx*t),pz-(az+vz*t)
    return dx*dx+dz*dz
end

local function authoredBroadcastPath(tx,tz)
    if not RacingGameplay or not RacingGameplay.GetBroadcastCameraPaths or not RacingGameplay.GetBroadcastCameraPathNodes then return nil,nil end
    local layoutId=(state.event and tonumber(state.event.layoutId)) or 0
    local best,bestNodes,bestDistance2=nil,nil,nil
    for _,path in ipairs(RacingGameplay.GetBroadcastCameraPaths(layoutId) or {}) do
        local nodes=RacingGameplay.GetBroadcastCameraPathNodes(path.id) or {}
        if #nodes>=2 then
            local distance2=nil
            for i=1,#nodes-1 do
                local a,b=nodes[i],nodes[i+1]
                local d2=pointSegmentDistance2(tx,tz,tonumber(a.x) or 0.0,tonumber(a.z) or 0.0,tonumber(b.x) or 0.0,tonumber(b.z) or 0.0)
                if distance2==nil or d2<distance2 then distance2=d2 end
            end
            local radius=math.max(1.0,tonumber(path.activationRadiusM) or 180.0)
            if distance2~=nil and distance2<=radius*radius and (bestDistance2==nil or distance2<bestDistance2) then
                best,bestNodes,bestDistance2=path,nodes,distance2
            end
        end
    end
    return best,bestNodes
end

local function catmullCoordinate(p0,p1,p2,p3,t)
    local t2,t3=t*t,t*t*t
    return 0.5*((2.0*p1)+(-p0+p2)*t+(2.0*p0-5.0*p1+4.0*p2-p3)*t2+(-p0+3.0*p1-3.0*p2+p3)*t3)
end

local function evaluateBroadcastPath(path,nodes,absoluteTime,incidentTime)
    if not path or not nodes or #nodes<2 then return nil end
    local duration=math.max(0.5,tonumber(path.durationSeconds) or 6.0)
    local u=clamp(0.5+((absoluteTime or 0.0)-(incidentTime or 0.0))/duration,0.0,1.0)
    local smooth=u*u*(3.0-2.0*u); local easing=clamp(tonumber(path.easing) or 0.65,0.0,1.0)
    u=lerp(u,smooth,easing); if path.reverse==true then u=1.0-u end
    local segments=#nodes-1; local scaled=u*segments; local segment=math.min(segments-1,math.floor(scaled)); local t=scaled-segment
    local i1=segment+1; local i2=math.min(#nodes,i1+1); local i0=math.max(1,i1-1); local i3=math.min(#nodes,i2+1)
    local p0,p1,p2,p3=nodes[i0],nodes[i1],nodes[i2],nodes[i3]
    return catmullCoordinate(tonumber(p0.x) or 0.0,tonumber(p1.x) or 0.0,tonumber(p2.x) or 0.0,tonumber(p3.x) or 0.0,t),
           catmullCoordinate(tonumber(p0.y) or 0.0,tonumber(p1.y) or 0.0,tonumber(p2.y) or 0.0,tonumber(p3.y) or 0.0,t),
           catmullCoordinate(tonumber(p0.z) or 0.0,tonumber(p1.z) or 0.0,tonumber(p2.z) or 0.0,tonumber(p3.z) or 0.0,t)
end

local function authoredTracksideCamera(tx,ty,tz)
    if not RacingGameplay or not RacingGameplay.GetMarkersByType then return nil end
    local layoutId=(state.event and tonumber(state.event.layoutId)) or 0
    local cameras=RacingGameplay.GetMarkersByType("Replay Camera",layoutId) or {}
    local best=nil; local bestDistance2=nil
    for _,camera in ipairs(cameras) do
        local cx,cy,cz=tonumber(camera.x),tonumber(camera.y),tonumber(camera.z)
        if cx~=nil and cy~=nil and cz~=nil then
            local dx,dz=tx-cx,tz-cz
            local distance2=dx*dx+dz*dz
            local radius=math.max(1.0,tonumber(camera.radiusM) or 120.0)
            if distance2<=radius*radius and (bestDistance2==nil or distance2<bestDistance2) then
                best=camera; bestDistance2=distance2
            end
        end
    end
    return best
end

local function updateReviewCamera(dt)
    local c=cfg()
    if not state.reviewEnabled or c.broadcastDirectorEnabled==false or state.cameraMode=="Off" or not Camera or not Camera.IsAvailable or not Camera.IsAvailable() or not Camera.SetWorldPose then
        disableReviewCamera(); return
    end
    local clip=clipById(state.selectedClipId); if not clip then disableReviewCamera(); return end
    local absolute=clip.startTimeS+state.reviewOffsetS; local a,b,t=framePair(clip,absolute); if not a then return end
    local ma,mb=stateMap(a),stateMap(b); local incident=clip.incident or {}
    local primary=racerForEntrant(ma,mb,incident.driverEntrantId,t)
    local secondary=racerForEntrant(ma,mb,incident.otherEntrantId,t)
    if not primary then
        for key,ra in pairs(ma) do primary=interpolatedRacer(ra,mb[key] or ra,t); break end
    end
    if not primary then return end

    local tx,ty,tz=primary.x,primary.y+0.9,primary.z
    if secondary then tx=(primary.x+secondary.x)*0.5; ty=(primary.y+secondary.y)*0.5+0.9; tz=(primary.z+secondary.z)*0.5 end
    local yaw=math.rad(primary.yawDeg or 0.0); local fx,fz=math.sin(yaw),math.cos(yaw); local rx,rz=math.cos(yaw),-math.sin(yaw)
    local distance=math.max(2.0,tonumber(c.incidentCameraDistanceM) or 13.0)
    local height=math.max(0.5,tonumber(c.incidentCameraHeightM) or 4.5)
    local px,py,pz
    if state.cameraMode~="Trackside" then state.authoredCameraId=0 end
    if state.cameraMode~="Spline" then state.authoredCameraPathId=0 end
    if state.cameraMode=="Spline" then
        local path,nodes=authoredBroadcastPath(tx,tz)
        if path then
            px,py,pz=evaluateBroadcastPath(path,nodes,absolute,clip.incidentTimeS)
            state.authoredCameraPathId=tonumber(path.id) or 0
        else
            state.authoredCameraPathId=0
            local lead=math.max(2.0,tonumber(c.tracksideCameraLeadM) or 22.0); local side=(clip.id%2==0) and 1.0 or -1.0
            local ix=tonumber(incident.globalX) or tx; local iy=tonumber(incident.globalY) or (ty-0.9); local iz=tonumber(incident.globalZ) or tz
            px=ix+fx*lead+rx*distance*side; py=iy+height; pz=iz+fz*lead+rz*distance*side
        end
    elseif state.cameraMode=="Chase" then
        px=primary.x-fx*distance*0.78; py=primary.y+height*0.58; pz=primary.z-fz*distance*0.78
        tx=primary.x+fx*5.0; ty=primary.y+1.0; tz=primary.z+fz*5.0
    elseif state.cameraMode=="Trackside" then
        local previousAuthoredCameraId=state.authoredCameraId or 0
        local authored=authoredTracksideCamera(tx,ty,tz)
        if authored then
            px=tonumber(authored.x) or tx; py=tonumber(authored.y) or (ty+height); pz=tonumber(authored.z) or tz
            state.authoredCameraId=tonumber(authored.id) or 0
        else
            state.authoredCameraId=0
            local lead=math.max(2.0,tonumber(c.tracksideCameraLeadM) or 22.0); local side=(clip.id%2==0) and 1.0 or -1.0
            local ix=tonumber(incident.globalX) or tx; local iy=tonumber(incident.globalY) or (ty-0.9); local iz=tonumber(incident.globalZ) or tz
            px=ix+fx*lead+rx*distance*side; py=iy+height; pz=iz+fz*lead+rz*distance*side
        end
        if previousAuthoredCameraId~=(state.authoredCameraId or 0) then state.cameraPose=nil end
    elseif state.cameraMode=="Helicopter" then
        local heli=math.max(5.0,tonumber(c.helicopterCameraHeightM) or 28.0)
        px=tx-fx*distance*0.45; py=ty+heli; pz=tz-fz*distance*0.45
    else
        local side=(clip.id%2==0) and 1.0 or -1.0
        px=tx-fx*distance*0.45+rx*distance*0.72*side; py=ty+height; pz=tz-fz*distance*0.45+rz*distance*0.72*side
    end

    local pitchDeg,yawDeg=cameraLookAngles(px,py,pz,tx,ty,tz)
    local desired={x=px,y=py,z=pz,pitch=pitchDeg,yaw=yawDeg}
    local smoothing=math.max(0.0,tonumber(c.cameraSmoothing) or 9.0)
    if state.cameraPose and smoothing>0.0 then
        local alpha=1.0-math.exp(-smoothing*math.max(0.0,tonumber(dt) or 0.0))
        desired.x=lerp(state.cameraPose.x,desired.x,alpha); desired.y=lerp(state.cameraPose.y,desired.y,alpha); desired.z=lerp(state.cameraPose.z,desired.z,alpha)
        desired.pitch=lerpAngle(state.cameraPose.pitch,desired.pitch,alpha); desired.yaw=lerpAngle(state.cameraPose.yaw,desired.yaw,alpha)
    end
    state.cameraPose=desired
    if Camera.SetWorldPose(desired.x,desired.y,desired.z,desired.pitch,desired.yaw,0.0) then state.cameraActive=true end
end

local function ensureGhost(key,row,index)
    local entity=state.ghostEntities[key]
    if entity and entity~=0 and Entity.Exists(entity) then return entity end
    entity=Entity.Create("Replay Ghost "..tostring(row.driverName or key)); if entity==0 then return nil end
    Entity.AddTag(entity,"ReplayGhost"); Entity.SetLocalScale(entity,1.8,1.15,4.4)
    local r=0.20+((index*37)%50)/100.0; local g=0.45+((index*19)%40)/100.0; local b=0.80
    Entity.SetDebugPrimitive(entity,"box",r,g,b); state.ghostEntities[key]=entity; return entity
end

local function updateGhosts()
    if not state.reviewEnabled or cfg().ghostReviewEnabled==false then destroyGhosts(); return end
    local clip=clipById(state.selectedClipId); if not clip then destroyGhosts(); return end
    local absolute=clip.startTimeS+state.reviewOffsetS; local a,b,t=framePair(clip,absolute); if not a then return end
    local ma,mb=stateMap(a),stateMap(b); local visible={}; local maximum=math.max(1,math.min(64,math.floor(cfg().maximumGhostVehicles or 16)))
    local count=0
    for key,ra in pairs(ma) do
        if count>=maximum then break end
        local rb=mb[key] or ra; local entity=ensureGhost(key,ra,count+1)
        if entity then
            local gx=lerp(ra.x or 0.0,rb.x or ra.x or 0.0,t); local gy=lerp(ra.y or 0.0,rb.y or ra.y or 0.0,t); local gz=lerp(ra.z or 0.0,rb.z or ra.z or 0.0,t)
            local x,y,z=Physics.GlobalToLocal(gx,gy,gz)
            if x~=nil then Entity.SetLocalPosition(entity,x,y+0.10,z); Entity.SetLocalRotation(entity,0.0,lerpAngle(ra.yawDeg or 0.0,rb.yawDeg or ra.yawDeg or 0.0,t),0.0) end
            visible[key]=true; count=count+1
        end
    end
    for key,entity in pairs(state.ghostEntities) do
        if not visible[key] then if Entity.Exists(entity) then Entity.Destroy(entity) end; state.ghostEntities[key]=nil end
    end
end

function RacingReplay.Update(dt)
    if state.reviewPlaying and state.reviewEnabled then
        local clip=clipById(state.selectedClipId)
        if clip then
            local duration=clipDuration(clip)
            state.reviewOffsetS=state.reviewOffsetS+math.max(0.0,tonumber(dt) or 0.0)
            if state.reviewOffsetS>=duration then state.reviewOffsetS=duration; state.reviewPlaying=false end
        end
    end
    updateGhosts()
    updateReviewCamera(dt)
end

function RacingReplay.GetReviewSnapshot()
    local clip=clipById(state.selectedClipId); if not clip then return nil end
    local absolute=clip.startTimeS+state.reviewOffsetS; local a,b,t=framePair(clip,absolute); if not a then return nil end
    return {clip=clip,offsetS=state.reviewOffsetS,timeFromIncidentS=absolute-clip.incidentTimeS,phase=a.phase or "",flag=a.flag or "Green",racers=a.racers or {},playing=state.reviewPlaying,enabled=state.reviewEnabled,blend=t}
end

function RacingReplay.GetTelemetry()
    local clip=clipById(state.selectedClipId)
    return {recording=state.recording,bufferedFrames=#state.rollingFrames,clips=#state.clips,activeClips=#state.activeClips,
        selectedClipId=state.selectedClipId,reviewOffsetS=state.reviewOffsetS,reviewEnabled=state.reviewEnabled,reviewPlaying=state.reviewPlaying,
        selectedDurationS=clipDuration(clip),cameraMode=state.cameraMode,cameraActive=state.cameraActive,
        broadcastDirectorEnabled=cfg().broadcastDirectorEnabled~=false,authoredCameraId=state.authoredCameraId or 0,authoredCameraPathId=state.authoredCameraPathId or 0,
        cameraSource=(state.authoredCameraPathId or 0)~=0 and "Authored Camera Path" or ((state.authoredCameraId or 0)~=0 and "Authored Replay Camera" or "Procedural"),
        sampleHz=clamp(tonumber(cfg().sampleHz) or 12.0,1.0,60.0),message=state.message}
end
