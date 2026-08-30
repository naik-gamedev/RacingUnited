-- STUDIO21 native Heritage Vehicle controller for physical Racing AI competitors.
--
-- RacingAI.lua owns intent/racecraft/strategy. MotorsportWeekend.lua owns the
-- competition lifecycle and scalable logical fallback. This layer owns only the
-- dynamic chassis representation and converts RacingAI control intent + authored
-- route geometry into native Vehicle steering/throttle/brake commands.
RacingAIVehicleController = RacingAIVehicleController or {}

local function clamp(v,lo,hi) if v<lo then return lo elseif v>hi then return hi end return v end
local function lerp(a,b,t) return a+(b-a)*clamp(t,0.0,1.0) end
local function cfg() return RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration() or {} end
local function normalizeAngleDegrees(value)
    local a=value or 0.0
    while a>180.0 do a=a-360.0 end
    while a<-180.0 do a=a+360.0 end
    return a
end

local function vehicleMass(agent)
    local cls=agent.class or (agent.entrant and RacingGameplay.GetMotorsportClass and RacingGameplay.GetMotorsportClass(agent.entrant.classId)) or nil
    local ballast=cls and (cls.balanceBallastKg or 0.0) or 0.0
    return math.max(650.0,1350.0+ballast)
end

local function refreshCollisionFootprint(handle,agent)
    local c=cfg()
    if c.colliderBoundsAuthority==false or not Physics.GetBodyCollisionBounds then
        if agent then agent.spatialFootprint={widthM=1.82,lengthM=4.35,centerX=0.0,centerZ=0.0,colliderCount=0,source="Logical fallback"} end
        return nil
    end
    local bounds=Physics.GetBodyCollisionBounds(handle.body)
    if type(bounds)=="table" and (bounds.width or bounds.sizeX) and (bounds.length or bounds.sizeZ) then
        handle.collisionBounds=bounds
        if RacingAIRacecraft and RacingAIRacecraft.SetPhysicalFootprint then RacingAIRacecraft.SetPhysicalFootprint(agent,bounds) end
        return bounds
    end
    if agent then agent.spatialFootprint={widthM=1.82,lengthM=4.35,centerX=0.0,centerZ=0.0,colliderCount=0,source="Logical fallback"} end
    return nil
end

local function spawnPosition(agent,gridMarker)
    if gridMarker then return gridMarker.x or 0.0,gridMarker.y or 0.0,gridMarker.z or 0.0,gridMarker.headingDeg or 0.0 end
    if RacingMotorsport and RacingMotorsport.SampleAgentPath then return RacingMotorsport.SampleAgentPath(agent,0.0) end
    return 0.0,0.0,0.0,0.0
end

function RacingAIVehicleController.Spawn(agent,gridMarker)
    local c=cfg()
    if c.fullPhysicsCompetitors~=true or Entity==nil or Physics==nil or Vehicle==nil then return nil end
    local entrant=agent.entrant or {}
    local mass=vehicleMass(agent)
    local entity=Entity.Create("Racing AI Vehicle #"..tostring(entrant.raceNumber or entrant.id or 0).." - "..tostring(entrant.driverName or "AI"))
    if entity==0 then return nil end
    Entity.AddTag(entity,"RaceCompetitor"); Entity.AddTag(entity,"RacingAI"); Entity.AddTag(entity,"FullPhysicsRaceVehicle")
    -- Placeholder presentation remains deliberately simple; vehiclePreset is kept
    -- on the entrant for the eventual asset-backed factory without changing AI.
    Entity.SetLocalScale(entity,1.82,1.34,4.35)
    Entity.SetDebugPrimitive(entity,"box",0.90,0.22,0.16)

    local body=Physics.CreateBody(entity,"dynamic",mass)
    if body==0 then Entity.Destroy(entity); return nil end
    Physics.SetBodyGravityFactor(body,1.0)
    Physics.SetBodyLinearDamping(body,0.018)
    Physics.SetBodyAngularDamping(body,0.10)
    Physics.SetBodyAllowSleep(body,false)
    local collider=Physics.CreateBoxCollider(body,0.87,0.54,2.04,0.0,0.03,0.0,0.86,0.025,false)
    if collider==0 then Physics.DestroyBody(body); Entity.Destroy(entity); return nil end

    local highRate=clamp(c.physicsHighRateHz or 500.0,120.0,2000.0)
    local skill=clamp(entrant.aiSkill or 0.8,0.0,1.0)
    local driveForce=mass*(6.2+2.0*skill)
    local brakeForce=mass*(10.0+2.5*skill)
    local maxSteer=clamp(c.maximumSteerAngleDeg or 38.0,5.0,70.0)
    local vehicle=Vehicle.Create(body,highRate,driveForce,brakeForce,maxSteer,1.12,11.0,0.012)
    if vehicle==0 then Physics.DestroyBody(body); Entity.Destroy(entity); return nil end

    local wheelbase=2.58; local track=1.55; local radius=0.325; local mountY=-0.28
    local frontZ=wheelbase*0.5; local rearZ=-wheelbase*0.5
    local spring=mass*9.80665/0.30
    local bump=mass*1.55; local rebound=mass*2.15
    local function addWheel(x,z,drive,steer,brake,handbrake)
        return Vehicle.AddWheel(vehicle,x,mountY,z,0.0,-1.0,0.0,radius,0.23,0.10,0.08,spring,bump,rebound,drive,steer,brake,handbrake)
    end
    local ok=addWheel(-track*0.5,frontZ,0.0,1.0,0.25,0.0)
        and addWheel(track*0.5,frontZ,0.0,1.0,0.25,0.0)
        and addWheel(-track*0.5,rearZ,0.5,0.0,0.25,0.5)
        and addWheel(track*0.5,rearZ,0.5,0.0,0.25,0.5)
    if not ok then Vehicle.Destroy(vehicle); Physics.DestroyBody(body); Entity.Destroy(entity); return nil end
    if Vehicle.SetTireContactFidelity then Vehicle.SetTireContactFidelity(vehicle,1) end
    if Vehicle.SetSteeringGeometry then Vehicle.SetSteeringGeometry(vehicle,0.90,8.5,11.0,0.52,55.0) end
    if Vehicle.SetDriverAids then Vehicle.SetDriverAids(vehicle,true,true,0.16,0.12,2.5,18.0,3500.0) end
    Vehicle.SetGear(vehicle,1)

    local gx,gy,gz,heading=spawnPosition(agent,gridMarker)
    local x,y,z=Physics.GlobalToLocal(gx,gy,gz)
    if x==nil then x,y,z=gx,gy,gz end
    Physics.SetBodyPosition(body,x,(y or 0.0)+0.72,z)
    Physics.SetBodyRotation(body,0.0,heading or 0.0,0.0)
    Physics.SetBodyLinearVelocity(body,0.0,0.0,0.0)
    Physics.SetBodyAngularVelocity(body,0.0,0.0,0.0)

    local handle={
        entity=entity,body=body,collider=collider,vehicle=vehicle,physical=true,
        backend="Heritage Vehicle",mechanicalHealth=1.0,lastSpeedMps=0.0,
        componentHealth={aero=1.0,suspension=1.0,powertrain=1.0},
        collisionCooldownS=0.0,recoveries=0,lastThrottle=0.0,lastBrake=1.0,lastSteering=0.0,
        gripFactor=1.0,groundedWheels=0,maxSlipRatio=0.0,maxSlipAngleDeg=0.0,
        averageTireTemperatureC=0.0,minTireTemperatureC=0.0,maxTireTemperatureC=0.0,thermalGripFactor=1.0,
        baseDryMassKg=mass,lastBodyMassKg=mass,fuelMassKg=0.0,
        lateralErrorM=0.0,routeDistanceM=0.0,forwardAlignment=1.0,contactCount=0
    }
    refreshCollisionFootprint(handle,agent)
    return handle
end

function RacingAIVehicleController.OnPitServiceComplete(handle)
    if not handle then return end
    local c=cfg()
    if c.componentDamageStrategy~=false then
        handle.mechanicalHealth=math.max(handle.mechanicalHealth or 1.0,0.92)
        local ch=handle.componentHealth or {}
        ch.aero=math.max(ch.aero or 1.0,0.94); ch.suspension=math.max(ch.suspension or 1.0,0.92); ch.powertrain=math.max(ch.powertrain or 1.0,0.90)
        handle.componentHealth=ch
    end
end

function RacingAIVehicleController.Destroy(handle)
    if not handle then return end
    if handle.vehicle and handle.vehicle~=0 and Vehicle.Exists(handle.vehicle) then Vehicle.Destroy(handle.vehicle) end
    if handle.body and handle.body~=0 and Physics.BodyExists(handle.body) then Physics.DestroyBody(handle.body) end
    if handle.entity and handle.entity~=0 and Entity.Exists(handle.entity) then Entity.Destroy(handle.entity) end
end

local function wheelGrip(handle)
    local c=cfg(); local vehicle=handle.vehicle
    local count=Vehicle.GetWheelCount and Vehicle.GetWheelCount(vehicle) or 4
    local grounded=0; local maxRatio=0.0; local maxAngle=0.0; local wetSum=0.0; local tempSum=0.0; local tempMin=math.huge; local tempMax=-math.huge; local tempCount=0
    for i=1,count do
        local w=Vehicle.GetWheelTelemetry and (Vehicle.GetWheelTelemetry(vehicle,i) or {}) or {}
        if w.grounded then grounded=grounded+1 end
        maxRatio=math.max(maxRatio,math.abs(tonumber(w.slipRatio) or 0.0))
        maxAngle=math.max(maxAngle,math.abs(tonumber(w.slipAngleDegrees) or 0.0))
        wetSum=wetSum+clamp(tonumber(w.surfaceWetness) or 0.0,0.0,1.0)
        local temp=tonumber(w.tireTreadTemperatureC or w.surfaceTemperatureC)
        if temp then tempSum=tempSum+temp; tempMin=math.min(tempMin,temp); tempMax=math.max(tempMax,temp); tempCount=tempCount+1 end
    end
    local contactFactor=count>0 and clamp(grounded/math.max(1,count),0.15,1.0) or 1.0
    local ratioLimit=math.max(0.02,c.gripSlipRatioLimit or 0.18)
    local angleLimit=math.max(1.0,c.gripSlipAngleDeg or 9.0)
    local slipPenalty=clamp(math.max(maxRatio/ratioLimit,maxAngle/angleLimit)-1.0,0.0,1.0)
    local grip=contactFactor*(1.0-0.45*slipPenalty)
    local averageTemp=tempCount>0 and tempSum/tempCount or 0.0
    local thermal=1.0
    if c.tireThermalStrategy~=false and tempCount>0 then
        local minT=c.tireOptimalMinimumC or 75.0; local maxT=c.tireOptimalMaximumC or 105.0
        if averageTemp<minT then thermal=1.0-clamp((minT-averageTemp)/90.0,0.0,0.30)
        elseif averageTemp>maxT then thermal=1.0-clamp((averageTemp-maxT)/100.0,0.0,0.38) end
        grip=grip*thermal
    end
    handle.groundedWheels=grounded; handle.maxSlipRatio=maxRatio; handle.maxSlipAngleDeg=maxAngle; handle.surfaceWetness=count>0 and wetSum/count or 0.0
    handle.averageTireTemperatureC=averageTemp; handle.minTireTemperatureC=tempCount>0 and tempMin or 0.0; handle.maxTireTemperatureC=tempCount>0 and tempMax or 0.0; handle.thermalGripFactor=thermal
    handle.gripFactor=clamp(grip,0.2,1.0)
    return handle.gripFactor
end

local function updateDamage(handle,speedMps,dt)
    local c=cfg(); handle.collisionCooldownS=math.max(0.0,(handle.collisionCooldownS or 0.0)-dt)
    local contacts=Physics.GetBodyContactCount and Physics.GetBodyContactCount(handle.body) or 0
    handle.contactCount=contacts or 0
    local previous=handle.lastSpeedMps or speedMps
    local speedLoss=math.max(0.0,previous-speedMps)
    if (contacts or 0)>0 and speedLoss>4.0 and handle.collisionCooldownS<=0.0 then
        local severity=clamp(speedLoss/28.0,0.0,1.0)
        local damage=severity*(c.collisionDamageScale or 0.10)
        handle.mechanicalHealth=clamp((handle.mechanicalHealth or 1.0)-damage,0.0,1.0)
        local components=handle.componentHealth or {aero=1.0,suspension=1.0,powertrain=1.0}
        components.aero=clamp((components.aero or 1.0)-damage*0.70,0.0,1.0)
        components.suspension=clamp((components.suspension or 1.0)-damage*(0.90+0.10*clamp(contacts/4.0,0.0,1.0)),0.0,1.0)
        components.powertrain=clamp((components.powertrain or 1.0)-damage*0.35,0.0,1.0)
        handle.componentHealth=components
        handle.collisionCooldownS=0.8
    end
    handle.lastSpeedMps=speedMps
end

local function classifyContactZone(yawDeg,nx,nz)
    local yaw=math.rad(yawDeg or 0.0)
    local fx=math.sin(yaw); local fz=math.cos(yaw)
    local rx=math.cos(yaw); local rz=-math.sin(yaw)
    local forward=fx*(nx or 0.0)+fz*(nz or 0.0)
    local right=rx*(nx or 0.0)+rz*(nz or 0.0)
    if forward>0.55 then return "Front" end
    if forward<-0.55 then return "Rear" end
    if right>=0.0 then return "Right" end
    return "Left"
end

local function strongestBodyContact(handle,yawDeg)
    local c=cfg()
    if c.contactEvidenceEnabled==false or not Physics.GetBodyContact or not Physics.GetBodyContactCount then
        handle.contactEvidence=nil
        return nil
    end
    local count=Physics.GetBodyContactCount(handle.body) or 0
    if count<=0 then handle.contactEvidence=nil; handle.contactKeys={}; return nil end
    local previousKeys=handle.contactKeys or {}; local currentKeys={}
    local best=nil; local bestImpulse=-1.0
    for i=1,count do
        local contact=Physics.GetBodyContact(handle.body,i)
        if type(contact)=="table" and contact.trigger~=true then
            local otherBody=tonumber(contact.otherBody) or 0; local otherCollider=tonumber(contact.otherCollider) or 0
            local key=otherBody>0 and ("body:"..tostring(otherBody)) or ("collider:"..tostring(otherCollider))
            currentKeys[key]=true; contact._episodeKey=key
            local impulse=math.abs(tonumber(contact.normalImpulseNs) or 0.0)
            if impulse>bestImpulse then best=contact; bestImpulse=impulse end
        end
    end
    handle.contactKeys=currentKeys
    if not best then handle.contactEvidence=nil; return nil end

    local nx=tonumber(best.normalX) or 0.0; local ny=tonumber(best.normalY) or 0.0; local nz=tonumber(best.normalZ) or 0.0
    local selfVx,selfVy,selfVz=Physics.GetBodyLinearVelocity(handle.body)
    selfVx=tonumber(selfVx) or 0.0; selfVy=tonumber(selfVy) or 0.0; selfVz=tonumber(selfVz) or 0.0
    local otherBody=tonumber(best.otherBody) or 0
    local otherVx,otherVy,otherVz=0.0,0.0,0.0
    if otherBody>0 and Physics.BodyExists and Physics.BodyExists(otherBody) then
        otherVx,otherVy,otherVz=Physics.GetBodyLinearVelocity(otherBody)
        otherVx=tonumber(otherVx) or 0.0; otherVy=tonumber(otherVy) or 0.0; otherVz=tonumber(otherVz) or 0.0
    end
    local closingMps=math.max(0.0,(selfVx-otherVx)*nx+(selfVy-otherVy)*ny+(selfVz-otherVz)*nz)
    local px=tonumber(best.pointX) or 0.0; local py=tonumber(best.pointY) or 0.0; local pz=tonumber(best.pointZ) or 0.0
    local gx,gy,gz=Physics.LocalToGlobal(px,py,pz)
    if gx==nil then gx,gy,gz=px,py,pz end
    local evidence={
        otherBody=otherBody,selfCollider=tonumber(best.selfCollider) or 0,otherCollider=tonumber(best.otherCollider) or 0,
        pointX=px,pointY=py,pointZ=pz,globalX=gx,globalY=gy,globalZ=gz,
        normalX=nx,normalY=ny,normalZ=nz,penetrationM=tonumber(best.penetrationM) or 0.0,
        normalImpulseNs=math.abs(tonumber(best.normalImpulseNs) or 0.0),tangentImpulseNs=math.abs(tonumber(best.tangentImpulseNs) or 0.0),
        relativeClosingKmh=closingMps*3.6,contactZone=classifyContactZone(yawDeg,nx,nz),warmStarted=best.warmStarted==true,
        newContactEpisode=previousKeys[best._episodeKey]~=true
    }
    handle.contactEvidence=evidence
    return evidence
end

local function automaticGear(handle)
    if not Vehicle.GetDrivetrainState then return end
    local current,_,shifting,_,rpm=Vehicle.GetDrivetrainState(handle.vehicle)
    current=current or 1; rpm=rpm or 0.0
    if shifting then return end
    local count=Vehicle.GetForwardGearCount and Vehicle.GetForwardGearCount(handle.vehicle) or 1
    if rpm>6500.0 and current>0 and current<count and Vehicle.ShiftUp then Vehicle.ShiftUp(handle.vehicle)
    elseif rpm>0.0 and rpm<2300.0 and current>1 and Vehicle.ShiftDown then Vehicle.ShiftDown(handle.vehicle) end
end

local function recoverToRoute(handle,agent,projection)
    if not RacingMotorsport or not RacingMotorsport.SampleAgentPath then return false end
    local gx,gy,gz,heading=RacingMotorsport.SampleAgentPath(agent,projection.distanceM or agent.distance or 0.0)
    local x,y,z=Physics.GlobalToLocal(gx,gy,gz); if x==nil then return false end
    Physics.SetBodyPosition(handle.body,x,y+0.72,z)
    Physics.SetBodyRotation(handle.body,0.0,heading or 0.0,0.0)
    Physics.SetBodyLinearVelocity(handle.body,0.0,0.0,0.0)
    Physics.SetBodyAngularVelocity(handle.body,0.0,0.0,0.0)
    Physics.ClearBodyForces(handle.body); Physics.WakeBody(handle.body)
    handle.recoveries=(handle.recoveries or 0)+1
    return true
end

local function updateFuelMass(handle,agent)
    local c=cfg(); if c.fuelMassAwareness==false or not Physics.SetBodyMass then return end
    local liters=agent and agent.ai and math.max(0.0,agent.ai.fuelLiters or 0.0) or 0.0
    local fuelMass=liters*math.max(0.1,c.fuelDensityKgPerLiter or 0.745)
    local target=math.max(100.0,(handle.baseDryMassKg or 1350.0)+fuelMass)
    if math.abs(target-(handle.lastBodyMassKg or 0.0))>0.20 then
        Physics.SetBodyMass(handle.body,target); handle.lastBodyMassKg=target
    end
    handle.fuelMassKg=fuelMass
end

function RacingAIVehicleController.Update(handle,agent,context,dt)
    if not handle or not handle.vehicle or not Vehicle.Exists(handle.vehicle) or not Physics.BodyExists(handle.body) then return nil end
    local c=cfg(); local intent=RacingAI and RacingAI.GetControlIntent and RacingAI.GetControlIntent(agent) or {}
    local px,py,pz=Physics.GetBodyPosition(handle.body); if px==nil then return nil end
    local gx,gy,gz=Physics.LocalToGlobal(px,py,pz); if gx==nil then gx,gy,gz=px,py,pz end
    local _,yaw=Physics.GetBodyRotation(handle.body); yaw=yaw or 0.0
    local projection=RacingMotorsport and RacingMotorsport.ProjectAgentToRoute and RacingMotorsport.ProjectAgentToRoute(agent,gx,gy,gz) or {distanceM=agent.distance or 0.0,lateralErrorM=0.0,nearestX=gx,nearestY=gy,nearestZ=gz,headingDeg=yaw}
    local speed=math.max(0.0,Vehicle.GetSpeed(handle.vehicle) or 0.0); local speedKmh=speed*3.6
    local grip=wheelGrip(handle); updateDamage(handle,speed,dt); updateFuelMass(handle,agent)
    local contactEvidence=strongestBodyContact(handle,yaw)
    if c.colliderBoundsAuthority~=false and handle.collisionBounds==nil then refreshCollisionFootprint(handle,agent) end

    local phase=context and context.phase or "Running"
    local targetKmh=math.max(0.0,tonumber(intent.targetSpeedKmh) or 0.0)*(context and context.speedFactor or 1.0)
    if phase=="Staging" or phase=="Countdown" then targetKmh=0.0
    elseif phase=="Formation" then targetKmh=math.min(targetKmh>0 and targetKmh or (c.formationSpeedKmh or 80.0),c.formationSpeedKmh or 80.0)
    elseif phase=="Rolling Start" then targetKmh=math.min(targetKmh>0 and targetKmh or (c.rollingStartSpeedKmh or 90.0),c.rollingStartSpeedKmh or 90.0) end
    if agent.pitMode then targetKmh=math.min(targetKmh,c.pitLaneSpeedKmh or 60.0) end
    if c.gripAwareBraking~=false then targetKmh=targetKmh*(0.78+0.22*grip) end

    local lookahead=math.max(4.0,speed*math.max(0.1,c.steeringLookaheadSeconds or 0.70)+6.0)
    lookahead=clamp(lookahead,c.lookaheadMinimumM or 18.0,c.lookaheadMaximumM or 120.0)
    local tx,ty,tz,targetHeading=RacingMotorsport.SampleAgentPath(agent,(projection.distanceM or 0.0)+lookahead)
    local headingToTarget=math.deg(math.atan((tx or gx)-gx,(tz or gz)-gz))
    if tx==nil then headingToTarget=targetHeading or yaw end
    local headingError=normalizeAngleDegrees(headingToTarget-yaw)
    local desiredOffset=tonumber(intent.lineOffsetM) or 0.0
    local lateralError=(projection.lateralErrorM or 0.0)-desiredOffset
    local speedScale=math.max(4.0,speed*0.55)
    local steering=(headingError/math.max(5.0,c.maximumSteerAngleDeg or 38.0))*(c.steeringGain or 1.10)
        - (lateralError/speedScale)*(c.crossTrackGain or 0.22)
    steering=clamp(steering,-1.0,1.0)

    local speedError=targetKmh-speedKmh
    local throttle=clamp(speedError*(c.throttleGain or 0.11),0.0,1.0)
    local brake=math.max(clamp(-speedError*(c.brakeGain or 0.16),0.0,1.0),clamp(intent.brakeDemand or 0.0,0.0,1.0))
    if c.gripAwareBraking~=false then
        local tractionScale=0.45+0.55*grip
        throttle=throttle*tractionScale
        brake=brake*(0.55+0.45*grip)
    end
    if phase=="Staging" or phase=="Countdown" then throttle=0.0; brake=1.0 end
    if handle.mechanicalHealth<0.35 then throttle=throttle*clamp(handle.mechanicalHealth/0.35,0.35,1.0) end
    Vehicle.SetInputs(handle.vehicle,throttle,brake,steering,0.0)
    automaticGear(handle)

    local pathHeading=projection.headingDeg or yaw
    local alignment=math.cos(math.rad(normalizeAngleDegrees(yaw-pathHeading)))
    handle.lastThrottle=throttle; handle.lastBrake=brake; handle.lastSteering=steering
    handle.lateralErrorM=lateralError; handle.routeDistanceM=projection.distanceM or 0.0; handle.forwardAlignment=alignment

    local recoveryDistance=math.max(5.0,c.physicalRecoveryDistanceM or 28.0)
    if math.abs(projection.lateralErrorM or 0.0)>recoveryDistance or gy < (projection.nearestY or gy)-20.0 then recoverToRoute(handle,agent,projection) end

    return {
        physical=true,backend=handle.backend,speedMps=speed,routeDistanceM=handle.routeDistanceM,lateralErrorM=lateralError,
        forwardAlignment=alignment,gripFactor=grip,groundedWheels=handle.groundedWheels,maxSlipRatio=handle.maxSlipRatio,
        maxSlipAngleDeg=handle.maxSlipAngleDeg,surfaceWetness=handle.surfaceWetness or 0.0,mechanicalHealth=handle.mechanicalHealth or 1.0,
        tireTemperatureC=handle.averageTireTemperatureC or 0.0,thermalGripFactor=handle.thermalGripFactor or 1.0,vehicleMassKg=handle.lastBodyMassKg or handle.baseDryMassKg or 0.0,fuelMassKg=handle.fuelMassKg or 0.0,componentHealth=handle.componentHealth,
        collisionBounds=handle.collisionBounds,contactEvidence=contactEvidence,contacts=handle.contactCount or 0,throttle=throttle,brake=brake,steering=steering,recoveries=handle.recoveries or 0
    }
end

function RacingAIVehicleController.GetTelemetry(handle)
    if not handle then return nil end
    return {backend=handle.backend or "",mechanicalHealth=handle.mechanicalHealth or 1.0,gripFactor=handle.gripFactor or 1.0,
        groundedWheels=handle.groundedWheels or 0,maxSlipRatio=handle.maxSlipRatio or 0.0,maxSlipAngleDeg=handle.maxSlipAngleDeg or 0.0,
        lateralErrorM=handle.lateralErrorM or 0.0,routeDistanceM=handle.routeDistanceM or 0.0,throttle=handle.lastThrottle or 0.0,
        brake=handle.lastBrake or 0.0,steering=handle.lastSteering or 0.0,contacts=handle.contactCount or 0,recoveries=handle.recoveries or 0,
        tireTemperatureC=handle.averageTireTemperatureC or 0.0,minTireTemperatureC=handle.minTireTemperatureC or 0.0,maxTireTemperatureC=handle.maxTireTemperatureC or 0.0,thermalGripFactor=handle.thermalGripFactor or 1.0,
        vehicleMassKg=handle.lastBodyMassKg or handle.baseDryMassKg or 0.0,fuelMassKg=handle.fuelMassKg or 0.0,componentHealth=handle.componentHealth,collisionBounds=handle.collisionBounds,
        contactEvidence=handle.contactEvidence}
end
