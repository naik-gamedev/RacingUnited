-- STUDIO18 section-practice loop. Captures a precise player-vehicle entry state
-- in floating-origin-safe global coordinates and repeatedly restores it after
-- crossing an authored-on-the-fly end gate.
RacingPracticeLoop = RacingPracticeLoop or {}

local loop = {
    startState = nil,
    endGate = nil,
    enabled = false,
    pendingRestartS = -1.0,
    previousGlobal = nil,
    attempts = 0,
    restarts = 0,
    lastAttemptS = 0.0,
    bestAttemptS = math.huge,
    currentAttemptS = 0.0,
    message = "F5 captures a practice-loop start; F6 captures the end and begins looping."
}

local function cfg()
    local value = RacingGameplay.GetEventExecutionConfiguration and RacingGameplay.GetEventExecutionConfiguration() or {}
    return value
end

local function playerBodyReady()
    return nativeVehicleBody ~= nil and nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody)
end

local function bodyGlobalPosition()
    if not playerBodyReady() then return nil end
    local x,y,z = Physics.GetBodyPosition(nativeVehicleBody)
    if x == nil then return nil end
    local gx,gy,gz = Physics.LocalToGlobal(x,y,z)
    if gx == nil then return nil end
    return {x=gx,y=gy,z=gz}
end

local function captureState()
    if not playerBodyReady() then return nil,"Player vehicle body is not available." end
    local px,py,pz = Physics.GetBodyPosition(nativeVehicleBody)
    local rx,ry,rz = Physics.GetBodyRotation(nativeVehicleBody)
    local vx,vy,vz = Physics.GetBodyLinearVelocity(nativeVehicleBody)
    local ax,ay,az = Physics.GetBodyAngularVelocity(nativeVehicleBody)
    if px == nil or rx == nil or vx == nil or ax == nil then return nil,"Could not read complete player rigid-body state." end
    local gx,gy,gz = Physics.LocalToGlobal(px,py,pz)
    if gx == nil then return nil,"Could not convert practice-loop start into global coordinates." end
    local gear = vehicleCurrentGear or 0
    if nativeVehicle ~= nil and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        local currentGear = Vehicle.GetDrivetrainState(nativeVehicle)
        if currentGear ~= nil then gear = currentGear end
    end
    return {
        gx=gx,gy=gy,gz=gz,
        rx=rx,ry=ry,rz=rz,
        vx=vx,vy=vy,vz=vz,
        ax=ax,ay=ay,az=az,
        gear=gear,
        capturedSpeedKmh=math.sqrt(vx*vx+vy*vy+vz*vz)*3.6
    }
end

local function restoreStart()
    local state = loop.startState
    if state == nil or not playerBodyReady() then return false end
    local x,y,z = Physics.GlobalToLocal(state.gx,state.gy,state.gz)
    if x == nil then loop.message="Practice-loop start is outside the current local-origin frame." return false end
    Physics.SetBodyPosition(nativeVehicleBody,x,y,z)
    Physics.SetBodyRotation(nativeVehicleBody,state.rx,state.ry,state.rz)
    Physics.SetBodyLinearVelocity(nativeVehicleBody,0.0,0.0,0.0)
    Physics.SetBodyAngularVelocity(nativeVehicleBody,0.0,0.0,0.0)
    Physics.ClearBodyForces(nativeVehicleBody)
    if nativeVehicle ~= nil and nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetInputs(nativeVehicle,0.0,0.0,0.0,0.0)
        if cfg().practiceLoopRestoreGear ~= false then
            local currentGear = Vehicle.GetDrivetrainState(nativeVehicle)
            if currentGear ~= state.gear then
                Vehicle.SetGear(nativeVehicle,0)
                Vehicle.SetGear(nativeVehicle,state.gear or 0)
            end
        end
    end
    Physics.SetBodyLinearVelocity(nativeVehicleBody,state.vx,state.vy,state.vz)
    if cfg().practiceLoopRestoreAngularVelocity ~= false then
        Physics.SetBodyAngularVelocity(nativeVehicleBody,state.ax,state.ay,state.az)
    end
    Physics.WakeBody(nativeVehicleBody)
    loop.previousGlobal = {x=state.gx,y=state.gy,z=state.gz}
    loop.currentAttemptS = 0.0
    loop.restarts = loop.restarts + 1
    loop.message = string.format("Loop restart #%d | restored %.1f km/h entry state",loop.restarts,state.capturedSpeedKmh or 0.0)
    return true
end

local function crossedEndGate(previous,current)
    local gate=loop.endGate
    if gate==nil or previous==nil or current==nil then return false end
    local yaw=math.rad(gate.headingDeg or 0.0)
    local nx,nz=math.sin(yaw),math.cos(yaw)
    local rx,rz=math.cos(yaw),-math.sin(yaw)
    local pside=(previous.x-gate.x)*nx+(previous.z-gate.z)*nz
    local cside=(current.x-gate.x)*nx+(current.z-gate.z)*nz
    if not (pside < 0.0 and cside >= 0.0) then return false end
    local lateral=math.abs((current.x-gate.x)*rx+(current.z-gate.z)*rz)
    local vertical=math.abs((current.y or gate.y)-(gate.y or current.y))
    return lateral <= math.max(1.0,(cfg().practiceLoopEndGateWidthM or 12.0)*0.5) and vertical <= 8.0
end

function RacingPracticeLoop.CaptureStart()
    if cfg().practiceLoopEnabled == false then loop.message="Practice Loop is disabled in Heritage Studio gameplay policy." return false,loop.message end
    local state,msg=captureState(); if not state then loop.message=msg return false,msg end
    loop.startState=state; loop.endGate=nil; loop.enabled=false; loop.pendingRestartS=-1.0; loop.previousGlobal={x=state.gx,y=state.gy,z=state.gz}; loop.attempts=0; loop.restarts=0; loop.bestAttemptS=math.huge; loop.currentAttemptS=0.0
    loop.message=string.format("Practice-loop START captured | %.1f km/h | gear %s",state.capturedSpeedKmh or 0.0,tostring(state.gear or 0))
    return true,loop.message
end

function RacingPracticeLoop.CaptureEnd()
    if loop.startState==nil then loop.message="Capture a practice-loop start first (F5)." return false,loop.message end
    local state,msg=captureState(); if not state then loop.message=msg return false,msg end
    loop.endGate={x=state.gx,y=state.gy,z=state.gz,headingDeg=state.ry or 0.0}
    loop.enabled=true; loop.attempts=0; loop.currentAttemptS=0.0
    loop.pendingRestartS=math.max(0.0,cfg().practiceLoopRestoreDelayS or 0.15)
    loop.message=string.format("Practice-loop END captured | %.1f m gate | looping ACTIVE",cfg().practiceLoopEndGateWidthM or 12.0)
    return true,loop.message
end

function RacingPracticeLoop.SetEnabled(value)
    if value and (loop.startState==nil or loop.endGate==nil) then loop.message="Practice loop needs both START and END." return false end
    loop.enabled=value==true; loop.pendingRestartS=-1.0
    loop.message=loop.enabled and "Practice loop playback enabled." or "Practice loop playback paused."
    return true
end

function RacingPracticeLoop.Toggle()
    return RacingPracticeLoop.SetEnabled(not loop.enabled)
end

function RacingPracticeLoop.RestartNow()
    if loop.startState==nil then loop.message="No practice-loop start captured." return false end
    return restoreStart()
end

function RacingPracticeLoop.Clear()
    loop.startState=nil; loop.endGate=nil; loop.enabled=false; loop.pendingRestartS=-1.0; loop.previousGlobal=nil; loop.currentAttemptS=0.0; loop.attempts=0; loop.restarts=0; loop.bestAttemptS=math.huge
    loop.message="Practice loop cleared."
end

function RacingPracticeLoop.HandleInput()
    if cfg().practiceLoopEnabled == false then return end
    if Input.Pressed("Practice Loop Start") then RacingPracticeLoop.CaptureStart() end
    if Input.Pressed("Practice Loop End") then RacingPracticeLoop.CaptureEnd() end
    if Input.Pressed("Practice Loop Toggle") then RacingPracticeLoop.Toggle() end
    if Input.Pressed("Practice Loop Restart") then RacingPracticeLoop.RestartNow() end
end

function RacingPracticeLoop.FixedUpdate(dt)
    if loop.pendingRestartS>=0.0 then
        loop.pendingRestartS=loop.pendingRestartS-dt
        if loop.pendingRestartS<=0.0 then loop.pendingRestartS=-1.0; restoreStart() end
        return
    end
    local current=bodyGlobalPosition(); if current==nil then return end
    if loop.enabled and loop.startState and loop.endGate then
        loop.currentAttemptS=loop.currentAttemptS+dt
        if crossedEndGate(loop.previousGlobal,current) then
            loop.attempts=loop.attempts+1; loop.lastAttemptS=loop.currentAttemptS; loop.bestAttemptS=math.min(loop.bestAttemptS,loop.currentAttemptS)
            loop.message=string.format("Loop attempt %d: %.3fs | best %.3fs",loop.attempts,loop.lastAttemptS,loop.bestAttemptS)
            if cfg().practiceLoopAutoRestart ~= false then loop.pendingRestartS=math.max(0.0,cfg().practiceLoopRestoreDelayS or 0.15) else loop.enabled=false end
        end
    end
    loop.previousGlobal=current
end

function RacingPracticeLoop.GetTelemetry()
    return {
        hasStart=loop.startState~=nil,hasEnd=loop.endGate~=nil,enabled=loop.enabled,pendingRestartS=loop.pendingRestartS,
        attempts=loop.attempts,restarts=loop.restarts,lastAttemptS=loop.lastAttemptS,bestAttemptS=(loop.bestAttemptS<math.huge and loop.bestAttemptS or 0.0),currentAttemptS=loop.currentAttemptS,
        startSpeedKmh=loop.startState and loop.startState.capturedSpeedKmh or 0.0,startGear=loop.startState and loop.startState.gear or 0,message=loop.message,
        start=loop.startState and {x=loop.startState.gx,y=loop.startState.gy,z=loop.startState.gz} or nil,endGate=loop.endGate
    }
end
