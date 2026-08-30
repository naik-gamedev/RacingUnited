-- STUDIO21 Racing AI intelligence / strategy authority.
-- This layer owns decisions, racecraft and strategy. MotorsportWeekend owns grids/results and
-- scalable competitor lifecycle; RacingAIVehicleController optionally consumes GetControlIntent()
-- for real native Heritage Vehicle chassis without replacing this intelligence model.
RacingAI = RacingAI or {}

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function lerp(a,b,t) return a+(b-a)*clamp(t,0.0,1.0) end
local function deterministic01(id,salt)
    local x=((tonumber(id) or 1)*1664525 + (salt or 0)*1013904223 + 12345) % 2147483647
    return (x % 100000) / 100000.0
end
local function cfg() return RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration() or {} end
local function routeProgress(agent) return (agent.completedLaps or 0)*(agent.routeLength or 0)+(agent.distance or 0) end
local function speedKmh(agent) return math.max(0.0,(agent.currentMps or 0.0)*3.6) end

local function getWetness()
    if Physics and Physics.GetSurfaceEnvironment then
        local wet=Physics.GetSurfaceEnvironment()
        if type(wet)=="number" then return clamp(wet,0.0,1.0) end
    end
    local weather=RacingGameplay and RacingGameplay.data and RacingGameplay.data.weather
    return clamp(weather and weather.surfaceWetness or 0.0,0.0,1.0)
end

local function currentTrackWidths(agent)
    local left,right=6.0,6.0
    if not agent.segments or #agent.segments==0 or (agent.routeLength or 0)<=0 then return left,right end
    local d=(agent.distance or 0)%(agent.routeLength or 1)
    local seg=agent.segments[#agent.segments]
    for _,candidate in ipairs(agent.segments) do if d<=candidate.start+candidate.length then seg=candidate break end end
    left=math.max(1.0,tonumber(seg.a and seg.a.leftWidthM) or left)
    right=math.max(1.0,tonumber(seg.a and seg.a.rightWidthM) or right)
    return left,right
end

local function authoredSpeedAhead(agent,lookaheadM)
    if not agent.segments or #agent.segments==0 or (agent.routeLength or 0)<=0 then return 0.0 end
    local best=math.huge
    local samples=8
    for i=0,samples do
        local d=(agent.distance or 0)+lookaheadM*(i/samples)
        if agent.closedLoop then d=d%agent.routeLength else d=math.min(agent.routeLength,d) end
        local seg=agent.segments[#agent.segments]
        for _,candidate in ipairs(agent.segments) do if d<=candidate.start+candidate.length then seg=candidate break end end
        local a=tonumber(seg.a and seg.a.targetSpeedKmh) or 0.0
        local b=tonumber(seg.b and seg.b.targetSpeedKmh) or 0.0
        local localT=clamp((d-seg.start)/math.max(0.001,seg.length),0.0,1.0)
        local target=(a>0 and b>0) and lerp(a,b,localT) or math.max(a,b)
        if target>0 then best=math.min(best,target) end
    end
    return best<math.huge and best or 0.0
end

local function isOvertakingPreferred(agent)
    if not agent.segments or #agent.segments==0 or (agent.routeLength or 0)<=0 then return false end
    local d=(agent.distance or 0)%agent.routeLength
    for _,seg in ipairs(agent.segments) do if d<=seg.start+seg.length then return seg.a and seg.a.overtakingPreferred==true end end
    return false
end

local function opponentContext(agent,allAgents,awarenessM)
    local ahead=nil; local behind=nil; local aheadGap=math.huge; local behindGap=math.huge
    local length=math.max(0.001,agent.routeLength or 0.001); local meDistance=agent.distance or 0.0
    for _,other in ipairs(allAgents or {}) do
        if other~=agent and not other.finished and not other.dnf then
            local forward=(other.distance or 0.0)-meDistance
            if agent.closedLoop then forward=forward%length end
            local backward=agent.closedLoop and ((meDistance-(other.distance or 0.0))%length) or -forward
            if forward>0.001 and forward<aheadGap and forward<=awarenessM then ahead=other; aheadGap=forward end
            if backward>0.001 and backward<behindGap and backward<=awarenessM then behind=other; behindGap=backward end
        end
    end
    return ahead,aheadGap,behind,behindGap
end

local function sameClass(a,b) return (a.entrant and a.entrant.classId or 0)==(b.entrant and b.entrant.classId or 0) end

function RacingAI.InitializeAgent(agent,session)
    local entrant=agent.entrant or {}
    local startPct=clamp(session and session.startingFuelPercent or 100.0,0.0,100.0)
    agent.ai={
        decision="Racing line", reason="Initial race state", line="Dry racing line", lateralOffsetM=0.0, wetLineBlend=0.0,
        targetSpeedKmh=0.0, brakeDemand=0.0, slipstream=false, defending=false, yielding=false, overtaking=false,
        aheadName="", aheadGapM=0.0, behindName="", behindGapM=0.0, pitRequested=false, pitReason="", pitStops=0,
        fuelCapacityLiters=100.0, fuelLiters=startPct, tireLife=1.0, tireCompound="Dry", fuelLaps=0.0,
        stintElapsedS=0.0, mistakeRemainingS=0.0, mistakes=0, mistakeClockS=0.0, lastMistakeCheck=-1,
        forecastWetness=0.0, wetnessTrendPerSecond=0.0, lastWetness=nil,
        controlIntent={steeringBias=0.0,targetSpeedKmh=0.0,brakeDemand=0.0,lineOffsetM=0.0,pitRequested=false,wetLineBlend=0.0,forecastWetness=0.0},
        reactionDelayS=math.max(0.05,entrant.reactionTimeS or 0.25), updateAccumulator=0.0
    }
    return agent.ai
end

function RacingAI.OnPitServiceComplete(agent,changedTires,refueled)
    if not agent.ai then return end
    if changedTires then agent.ai.tireLife=1.0; agent.ai.tireCompound=agent.ai.wetLineBlend>0.5 and "Wet" or "Dry" end
    if refueled then agent.ai.fuelLiters=agent.ai.fuelCapacityLiters end
    agent.ai.pitRequested=false; agent.ai.pitReason=""; agent.ai.stintElapsedS=0.0; agent.ai.pitStops=(agent.ai.pitStops or 0)+1
end

function RacingAI.UpdateAgent(agent,context,dt)
    local c=cfg(); if c.enabled==false then return agent.ai or RacingAI.InitializeAgent(agent,context and context.session) end
    local ai=agent.ai or RacingAI.InitializeAgent(agent,context and context.session)
    ai.updateAccumulator=(ai.updateAccumulator or 0)+dt
    local hz=math.max(1.0,c.updateHz or 20.0); local step=1.0/hz
    -- Fuel/tire/stint simulation must remain continuous even if decision updates are throttled.
    local travelled=math.max(0.0,(agent.currentMps or 0.0)*dt)
    local entrant=agent.entrant or {}
    local fuelManagement=clamp(entrant.fuelManagement or 0.7,0.0,1.0)
    local tireManagement=clamp(entrant.tireManagement or 0.7,0.0,1.0)
    local fuelPerM=(math.max(0.0,c.fuelUseLitersPer100Km or 35.0)/100000.0)*(1.08-0.16*fuelManagement)
    local wearPerM=(math.max(0.0,c.tireWearPer100Km or 0.18)/100000.0)*(1.12-0.24*tireManagement)
    local wet=getWetness()
    if ai.lastWetness~=nil and dt>0.0001 then
        local instantaneous=(wet-ai.lastWetness)/dt
        ai.wetnessTrendPerSecond=lerp(ai.wetnessTrendPerSecond or 0.0,instantaneous,clamp(dt*0.75,0.0,1.0))
    end
    ai.lastWetness=wet
    local horizon=(c.weatherForecastEnabled~=false) and math.max(0.0,c.weatherForecastSeconds or 60.0) or 0.0
    ai.forecastWetness=clamp(wet+(ai.wetnessTrendPerSecond or 0.0)*horizon,0.0,1.0)
    ai.fuelLiters=math.max(0.0,(ai.fuelLiters or 0.0)-travelled*fuelPerM)
    ai.tireLife=clamp((ai.tireLife or 1.0)-travelled*wearPerM*(1.0+wet*0.10),0.0,1.0)
    ai.stintElapsedS=(ai.stintElapsedS or 0.0)+dt
    ai.mistakeClockS=(ai.mistakeClockS or 0.0)+dt
    if ai.mistakeRemainingS>0 then ai.mistakeRemainingS=math.max(0.0,ai.mistakeRemainingS-dt) end
    if ai.updateAccumulator<step then return ai end
    local decisionDt=ai.updateAccumulator; ai.updateAccumulator=0.0

    local awareness=math.max(10.0,(c.opponentAwarenessM or 100.0)*(0.55+0.65*clamp(entrant.awareness or 0.8,0,1)))
    local ahead,aheadGap,behind,behindGap=opponentContext(agent,context and context.agents or {},awareness)
    ai.aheadName=ahead and (ahead.entrant.driverName or "Opponent") or ""; ai.aheadGapM=ahead and aheadGap or 0.0
    ai.behindName=behind and (behind.entrant.driverName or "Opponent") or ""; ai.behindGapM=behind and behindGap or 0.0

    local baseKmh=math.max(30.0,(agent.basePaceMps or agent.currentMps or 40.0)*3.6)
    local lookahead=lerp(c.lookaheadMinimumM or 18.0,c.lookaheadMaximumM or 120.0,clamp(speedKmh(agent)/300.0,0,1))
    local authored=authoredSpeedAhead(agent,math.max(lookahead,c.brakingLookaheadM or 180.0))
    local targetKmh=authored>0 and math.min(baseKmh,authored*(0.90+0.12*clamp(entrant.aiSkill or 0.8,0,1))) or baseKmh
    local brakeDemand=authored>0 and clamp((speedKmh(agent)-authored)/math.max(20.0,authored),0,1) or 0.0

    local wetThreshold=clamp(c.wetLineThreshold or 0.2,0,1); local wetBlend=0.0
    local decisionWet=math.max(wet,(c.weatherForecastEnabled~=false) and (ai.forecastWetness or wet)*0.88 or wet)
    if c.wetLineEnabled~=false and decisionWet>wetThreshold and agent.hasWetLine then
        wetBlend=clamp((decisionWet-wetThreshold)/math.max(0.01,1.0-wetThreshold),0,1)*(0.55+0.45*clamp(entrant.wetSkill or 0.75,0,1))
        ai.line="Wet line"
    else ai.line="Dry racing line" end
    ai.wetLineBlend=wetBlend
    local wetPenalty=(c.maximumWetSpeedPenalty or 0.24)*wet*(1.08-0.40*clamp(entrant.wetSkill or 0.75,0,1))
    targetKmh=targetKmh*(1.0-clamp(wetPenalty,0,0.75))

    local decision="Racing line"; local reason="Following authored target speed / lookahead"; local lateral=clamp(entrant.preferredLineBias or 0.0,-1,1)*0.5
    ai.slipstream=false; ai.defending=false; ai.yielding=false; ai.overtaking=false
    local racecraft=clamp(entrant.racecraft or 0.75,0,1); local aggression=clamp(entrant.aggression or 0.5,0,1)
    local leftW,rightW=currentTrackWidths(agent)
    local trackSafety=(c.trackLimitAwarePassing~=false) and math.max(0.0,c.trackLimitSafetyM or 0.65) or 0.0
    local safeLeft=math.max(0.35,leftW-trackSafety); local safeRight=math.max(0.35,rightW-trackSafety)
    local maxOffset=math.max(0.35,math.min(safeLeft,safeRight)*0.72)

    if behind and c.multiclassNegotiation~=false and not sameClass(agent,behind) and (behind.basePaceMps or 0)>(agent.basePaceMps or 0)*1.06 and behindGap<(c.blueFlagYieldGapM or 40.0) then
        decision="Multi-class yield"; reason="Faster-class car approaching"; ai.yielding=true; lateral=-maxOffset*0.55; targetKmh=targetKmh*0.97
    elseif behind and (behind.completedLaps or 0)>(agent.completedLaps or 0) and behindGap<(c.blueFlagYieldGapM or 40.0) then
        decision="Blue flag yield"; reason="Lapping competitor approaching"; ai.yielding=true; lateral=-maxOffset*0.65; targetKmh=targetKmh*0.96
    elseif ahead then
        local closingKmh=((agent.basePaceMps or 0)-(ahead.currentMps or ahead.basePaceMps or 0))*3.6
        local slipMin=c.slipstreamMinimumGapM or 4.0; local slipMax=c.slipstreamMaximumGapM or 32.0
        if c.slipstreamEnabled~=false and aheadGap>=slipMin and aheadGap<=slipMax and sameClass(agent,ahead) then
            ai.slipstream=true; targetKmh=targetKmh*(1.0+0.015+0.018*racecraft); decision="Slipstream"; reason="Using tow before pass"
        end
        local passWanted=closingKmh>(c.overtakeMinimumClosingKmh or 5.0) and (isOvertakingPreferred(agent) or aggression>0.42 or not sameClass(agent,ahead))
        if passWanted and aheadGap<math.max(12.0,lookahead*0.55) then
            ai.overtaking=true; decision=not sameClass(agent,ahead) and "Multi-class pass" or "Overtake"; reason="Closing speed + available passing opportunity"
            local side=((deterministic01(entrant.id,math.floor(agent.elapsedS or 0))>0.5) and 1 or -1)
            if (entrant.preferredLineBias or 0)~=0 then side=(entrant.preferredLineBias or 0)>0 and 1 or -1 end
            lateral=side*maxOffset*(0.55+0.30*racecraft); targetKmh=targetKmh*(1.0+0.012*aggression)
        elseif aheadGap<10.0 then targetKmh=math.min(targetKmh,speedKmh(ahead)*0.995); decision="Following"; reason="Opponent ahead inside safe passing window" end
    end

    if c.spatialAvoidance~=false and ahead and aheadGap<math.max(4.5,c.sideBySideSafetyM or 1.25)*1.8 then
        local otherOffset=ahead.ai and ahead.ai.lateralOffsetM or 0.0
        local separation=math.abs(lateral-otherOffset)
        local required=math.max(0.5,(c.sideBySideSafetyM or 1.25)*1.6)
        if separation<required then
            local availableLeft=safeLeft-lateral; local availableRight=safeRight+lateral
            local escape=(availableLeft>availableRight) and 1.0 or -1.0
            lateral=clamp(lateral+escape*(required-separation),-safeRight,safeLeft)
            targetKmh=targetKmh*0.985
            if not ai.overtaking then decision="Side-by-side spacing"; reason="Maintaining physical overlap margin" end
        end
    end

    if behind and c.defendingEnabled~=false and not ai.yielding and sameClass(agent,behind) and behindGap<(c.defensiveTriggerGapM or 22.0) and clamp(entrant.defending or 0.55,0,1)>0.25 then
        local defendStrength=clamp((entrant.defending or 0.55)*0.75+aggression*0.25,0,1)
        if not ai.overtaking then decision="Defending"; reason="Same-class opponent inside defensive trigger" end
        ai.defending=true; lateral=lateral + maxOffset*0.45*defendStrength*((entrant.preferredLineBias or 0)>=0 and 1 or -1)
    end

    local flag=context and context.flag or "Green"
    if flag=="Red" then targetKmh=0.0; decision="Red flag"; reason="Race control stop"
    elseif flag=="Safety Car" then decision="Safety Car delta"; reason="Holding Safety Car pace / no attack"; lateral=0.0
    elseif flag=="Full Course Yellow" or flag=="Virtual Safety Car" then decision=flag.." delta"; reason="Race-control speed delta"; lateral=0.0 end

    -- Strategy planning: fuel, tires, weather crossover and maximum stint.
    local lapKm=math.max(0.1,(agent.routeLength or 1000.0)/1000.0)
    local fuelPerLap=math.max(0.01,(c.fuelUseLitersPer100Km or 35.0)*lapKm/100.0*(1.08-0.16*fuelManagement))
    ai.fuelLaps=(ai.fuelLiters or 0.0)/fuelPerLap
    ai.pitRequested=false; ai.pitReason=""
    if c.strategyEnabled~=false and context and context.session then
        local session=context.session; local risk=clamp(entrant.strategyRisk or 0.5,0,1)
        local remaining=math.max(0,(context.targetLaps or 0)-(agent.completedLaps or 0))
        if session.refuelingAllowed~=false and remaining>0 and ai.fuelLaps<math.min(remaining,(c.fuelReserveLaps or 1.25)*(1.15-0.30*risk)) then ai.pitRequested=true; ai.pitReason="Fuel window" end
        if session.tireChangesAllowed~=false and ai.tireLife<(c.tirePitThreshold or 0.32)*(1.10-0.25*risk) then ai.pitRequested=true; ai.pitReason=ai.pitReason~="" and (ai.pitReason.." + tires") or "Tire wear" end
        if session.maximumStintMinutes and session.maximumStintMinutes>0 and ai.stintElapsedS>=session.maximumStintMinutes*60.0 then ai.pitRequested=true; ai.pitReason="Maximum stint" end
        local mandatory=math.max(0,session.mandatoryPitStops or 0); local completedStops=math.max(agent.pitStops or 0,ai.pitStops or 0)
        if mandatory>completedStops and remaining<=math.max(2,(mandatory-completedStops)*2) then ai.pitRequested=true; ai.pitReason="Mandatory pit stop" end
        local strategyWet=(c.weatherForecastEnabled~=false) and (ai.forecastWetness or wet) or wet
        local wantsWet=strategyWet>wetThreshold+0.12; local wetTire=ai.tireCompound=="Wet"
        if session.tireChangesAllowed~=false and wantsWet~=wetTire and math.abs(strategyWet-wetThreshold)>0.16 then ai.pitRequested=true; ai.pitReason=wantsWet and ((strategyWet>wet+0.05) and "Forecast wet-tire crossover" or "Wet-tire crossover") or ((strategyWet<wet-0.05) and "Forecast dry-tire crossover" or "Dry-tire crossover") end
        local health=clamp(agent.mechanicalHealth or 1.0,0.0,1.0)
        if c.damageStrategyEnabled~=false and health<(c.damagePitThreshold or 0.62) and health>(c.damageDnfThreshold or 0.16) then ai.pitRequested=true; ai.pitReason=ai.pitReason~="" and (ai.pitReason.." + damage") or "Damage repair" end
        if c.tireThermalStrategy~=false and agent.tireTemperatureC then
            ai.tireTemperatureC=agent.tireTemperatureC
            local minT=c.tireOptimalMinimumC or 75.0; local maxT=c.tireOptimalMaximumC or 105.0
            if ai.tireTemperatureC<minT then targetKmh=targetKmh*(1.0-clamp((minT-ai.tireTemperatureC)/80.0,0.0,0.12))
            elseif ai.tireTemperatureC>maxT then
                targetKmh=targetKmh*(1.0-clamp((ai.tireTemperatureC-maxT)/100.0,0.0,0.18))
                if session.tireChangesAllowed~=false and ai.tireTemperatureC>maxT+25.0 then ai.pitRequested=true; ai.pitReason=ai.pitReason~="" and (ai.pitReason.." + tire temperature") or "Tire overheating" end
            end
        end
        if c.componentDamageStrategy~=false and agent.componentHealth then
            local ch=agent.componentHealth
            local worst=math.min(ch.aero or 1.0,ch.suspension or 1.0,ch.powertrain or 1.0)
            if worst<(c.damagePitThreshold or 0.62) and worst>(c.damageDnfThreshold or 0.16) then ai.pitRequested=true; ai.pitReason=ai.pitReason~="" and (ai.pitReason.." + component damage") or "Component damage" end
        end
        if ai.pitRequested then decision="Pit strategy"; reason=ai.pitReason end
    end

    -- Driver mistakes. The deterministic clock makes replays/tests stable while preserving personality variation.
    if c.mistakesEnabled~=false and ai.mistakeRemainingS<=0 then
        local check=math.floor(ai.mistakeClockS)
        if check~=ai.lastMistakeCheck then
            ai.lastMistakeCheck=check
            local rate=math.max(0.0,entrant.mistakeRatePerHour or 0.18)*(1.30-0.65*clamp(entrant.consistency or 0.8,0,1))*(1.0+wet*0.9+aggression*0.25)
            local chance=rate/3600.0
            if deterministic01(entrant.id,10000+check)<chance then ai.mistakeRemainingS=(c.mistakeRecoverySeconds or 2.0)*(1.25-0.50*racecraft); ai.mistakes=(ai.mistakes or 0)+1 end
        end
    end
    if ai.mistakeRemainingS>0 then
        decision="Recovering mistake"; reason="Driver error / correction"; targetKmh=targetKmh*(0.58+0.22*racecraft); lateral=lateral+(deterministic01(entrant.id,ai.mistakes)-0.5)*maxOffset; brakeDemand=math.max(brakeDemand,0.25)
    end

    if RacingAIRacecraft and RacingAIRacecraft.Evaluate then
        local rc=RacingAIRacecraft.Evaluate(agent,context,{decision=decision,reason=reason,lateralOffsetM=lateral,targetSpeedKmh=targetKmh,brakeDemand=brakeDemand,
            ahead=ahead,aheadGapM=aheadGap,behind=behind,behindGapM=behindGap,maxOffset=maxOffset,safeLeft=safeLeft,safeRight=safeRight,overtaking=ai.overtaking,defending=ai.defending},decisionDt)
        if rc then
            decision=rc.decision or decision; reason=rc.reason or reason; lateral=rc.lateralOffsetM or lateral
            targetKmh=(rc.targetSpeedKmh or targetKmh)*(rc.speedFactor or 1.0); brakeDemand=math.max(brakeDemand,rc.brakeDemand or 0.0)
        end
    end

    lateral=clamp(lateral,-safeRight,safeLeft)
    ai.decision=decision; ai.reason=reason; ai.lateralOffsetM=lateral; ai.targetSpeedKmh=math.max(0.0,targetKmh); ai.brakeDemand=clamp(brakeDemand,0,1)
    ai.controlIntent={steeringBias=clamp(lateral/math.max(1.0,maxOffset),-1,1),targetSpeedKmh=ai.targetSpeedKmh,brakeDemand=ai.brakeDemand,lineOffsetM=lateral,pitRequested=ai.pitRequested,wetLineBlend=ai.wetLineBlend,forecastWetness=ai.forecastWetness or wet}
    return ai
end

function RacingAI.GetControlIntent(agent) return agent and agent.ai and agent.ai.controlIntent or {steeringBias=0,targetSpeedKmh=0,brakeDemand=0,lineOffsetM=0,pitRequested=false,wetLineBlend=0,forecastWetness=0} end
function RacingAI.GetTelemetry(agent)
    if not agent then return nil end
    local ai=agent.ai or {}
    return {
        entrantId=agent.entrant and agent.entrant.id or 0, driverName=agent.entrant and agent.entrant.driverName or "AI", raceNumber=agent.entrant and agent.entrant.raceNumber or 0,
        decision=ai.decision or "", reason=ai.reason or "", line=ai.line or "", targetSpeedKmh=ai.targetSpeedKmh or 0.0, speedKmh=speedKmh(agent), brakeDemand=ai.brakeDemand or 0.0,
        lateralOffsetM=ai.lateralOffsetM or 0.0, wetLineBlend=ai.wetLineBlend or 0.0, slipstream=ai.slipstream==true, overtaking=ai.overtaking==true, defending=ai.defending==true, yielding=ai.yielding==true,
        aheadName=ai.aheadName or "", aheadGapM=ai.aheadGapM or 0.0, behindName=ai.behindName or "", behindGapM=ai.behindGapM or 0.0,
        fuelLiters=ai.fuelLiters or 0.0, fuelLaps=ai.fuelLaps or 0.0, tireLife=ai.tireLife or 1.0, tireCompound=ai.tireCompound or "Dry", pitRequested=ai.pitRequested==true, pitReason=ai.pitReason or "",
        forecastWetness=ai.forecastWetness or 0.0, wetnessTrendPerSecond=ai.wetnessTrendPerSecond or 0.0, mechanicalHealth=agent.mechanicalHealth or 1.0,
        tireTemperatureC=ai.tireTemperatureC or agent.tireTemperatureC or 0.0, vehicleMassKg=agent.vehicleMassKg or 0.0, fuelMassKg=agent.fuelMassKg or 0.0, componentHealth=agent.componentHealth,
        mistakeRemainingS=ai.mistakeRemainingS or 0.0, mistakes=ai.mistakes or 0, completedLaps=agent.completedLaps or 0, progressM=routeProgress(agent), dnf=agent.dnf==true, finished=agent.finished==true
    }
end
