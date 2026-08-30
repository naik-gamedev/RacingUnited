-- STUDIO23 solver-contact steward evidence extends STUDIO22 collider-aware close racecraft.
--
-- The physics world's solid collider set is the primary chassis footprint authority.
-- Logical dimensions are only a fallback for competitors that do not currently own a
-- physical body. This layer predicts swept overlap in route/lateral space and adds
-- braking-battle, divebomb, switchback, blocking and pit-release decisions without
-- replacing the STUDIO20/21 strategic AI or native vehicle controller.
RacingAIRacecraft = RacingAIRacecraft or {}

local incidentLedger={}
local nextIncidentId=1

local function clamp(v,a,b) if v<a then return a elseif v>b then return b end return v end
local function cfg() return RacingGameplay.GetMotorsportAiConfiguration and RacingGameplay.GetMotorsportAiConfiguration() or {} end
local function routeProgress(agent) return (agent.completedLaps or 0)*(agent.routeLength or 0)+(agent.distance or 0) end
local function sameClass(a,b) return (a and a.entrant and a.entrant.classId or 0)==(b and b.entrant and b.entrant.classId or 0) end
local function lateral(agent) return agent and agent.ai and (agent.ai.lateralOffsetM or 0.0) or 0.0 end

local function footprint(agent)
    local c=cfg()
    local f=agent and agent.spatialFootprint or nil
    local width=(f and tonumber(f.widthM)) or 1.82
    local length=(f and tonumber(f.lengthM)) or 4.35
    local centerX=(f and tonumber(f.centerX)) or 0.0
    local centerZ=(f and tonumber(f.centerZ)) or 0.0
    return {
        widthM=math.max(0.5,width), lengthM=math.max(1.0,length), centerX=centerX, centerZ=centerZ,
        halfWidthM=math.max(0.25,width*0.5), halfLengthM=math.max(0.5,length*0.5),
        colliderCount=(f and f.colliderCount) or 0, source=(f and f.source) or "Logical fallback",
        marginM=math.max(0.0,c.collisionEnvelopeMarginM or 0.18)
    }
end

function RacingAIRacecraft.SetPhysicalFootprint(agent,bounds)
    if not agent or type(bounds)~="table" then return false end
    local width=tonumber(bounds.width or bounds.sizeX)
    local length=tonumber(bounds.length or bounds.sizeZ)
    if not width or not length or width<=0.0 or length<=0.0 then return false end
    agent.spatialFootprint={
        widthM=width,lengthM=length,centerX=tonumber(bounds.centerX) or 0.0,centerZ=tonumber(bounds.centerZ) or 0.0,
        colliderCount=tonumber(bounds.colliderCount) or 0,source="Physics colliders"
    }
    return true
end

local function signedGap(me,other)
    if not me or not other then return math.huge end
    local length=math.max(0.001,me.routeLength or 0.001)
    local d=(other.distance or 0.0)-(me.distance or 0.0)
    if me.closedLoop then
        while d>length*0.5 do d=d-length end
        while d<-length*0.5 do d=d+length end
    end
    return d
end

local function closestBehind(agent,agents,maxDistance)
    local best=nil; local bestGap=math.huge
    for _,other in ipairs(agents or {}) do
        if other~=agent and not other.finished and not other.dnf then
            local gap=signedGap(agent,other)
            if gap<0.0 and -gap<bestGap and -gap<=maxDistance then best=other; bestGap=-gap end
        end
    end
    return best,bestGap
end

local function closestAhead(agent,agents,maxDistance)
    local best=nil; local bestGap=math.huge
    for _,other in ipairs(agents or {}) do
        if other~=agent and not other.finished and not other.dnf then
            local gap=signedGap(agent,other)
            if gap>0.0 and gap<bestGap and gap<=maxDistance then best=other; bestGap=gap end
        end
    end
    return best,bestGap
end

local function predictedOverlap(agent,other,gap,horizon,proposedLateral)
    if not other or gap==math.huge then return false,math.huge,math.huge end
    local meF=footprint(agent); local otherF=footprint(other); local c=cfg()
    local relative=(agent.currentMps or 0.0)-(other.currentMps or 0.0)
    local futureGap=gap-relative*math.max(0.0,horizon or 0.0)
    local longitudinalNeed=meF.halfLengthM+otherF.halfLengthM+meF.marginM
    local lateralGap=math.abs((proposedLateral or lateral(agent))-lateral(other))
    local lateralNeed=meF.halfWidthM+otherF.halfWidthM+meF.marginM+math.max(0.0,c.sideBySideOverlapToleranceM or 0.10)
    return futureGap<longitudinalNeed and lateralGap<lateralNeed, futureGap-longitudinalNeed, lateralGap-lateralNeed
end

local function applyStewardPenalty(agent,kind,seconds,reason)
    seconds=math.max(0.0,tonumber(seconds) or 0.0)
    if seconds<=0.0 or not agent then return false end
    agent.stewardPenaltyS=(agent.stewardPenaltyS or 0.0)+seconds
    if RacingEvents and RacingEvents.AddParticipantPenalty and agent.participantId then
        RacingEvents.AddParticipantPenalty(agent.participantId,kind,seconds,reason)
    end
    return true
end
RacingAIRacecraft.ApplyStewardPenalty=applyStewardPenalty

local function stateFor(agent)
    agent.ai=agent.ai or {}
    local s=agent.ai.racecraftState
    if not s then
        s={defensiveMoves=0,noPressureS=0.0,lastDefending=false,lastLateral=0.0,lastPitMode=agent.pitMode,
            blockingPenaltyIssued=false,unsafeRelease=false,unsafeReleaseCount=0,predictedCollision=false,
            divebombState="none",switchbackRemainingS=0.0,stewardPenaltyS=0.0,contactEvidenceCooldownS=0.0,
            incidentCount=0,lastLateralDelta=0.0,lastIncident=nil}
        agent.ai.racecraftState=s
    end
    return s
end

local function entrantName(agent)
    return agent and agent.entrant and (agent.entrant.driverName or ("#"..tostring(agent.entrant.raceNumber or agent.entrant.id or 0))) or "Unknown"
end

local function classifyPhysicalIncident(agent,other,evidence)
    local c=cfg(); local zone=evidence.contactZone or "Unknown"
    local impulse=math.max(0.0,tonumber(evidence.normalImpulseNs) or 0.0)
    local closing=math.max(0.0,tonumber(evidence.relativeClosingKmh) or 0.0)
    local significant=impulse>=math.max(0.0,c.incidentMinimumNormalImpulseNs or 180.0)
        or closing>=math.max(0.0,c.incidentMinimumClosingKmh or 4.0)
    if not significant then return nil end

    local classification="Physical contact"
    local fault=nil
    if not other then
        classification="Barrier / static-world contact"
    else
        local gap=signedGap(agent,other)
        if zone=="Front" and gap>0.0 then
            classification="Rear-end contact"; fault=agent
        elseif zone=="Rear" and gap<0.0 then
            classification="Rear-end contact"; fault=other
        elseif zone=="Left" or zone=="Right" then
            classification="Side-to-side contact"
            local meDelta=math.abs((stateFor(agent).lastLateralDelta or 0.0))
            local otherDelta=math.abs((stateFor(other).lastLateralDelta or 0.0))
            if meDelta>otherDelta+0.12 then fault=agent
            elseif otherDelta>meDelta+0.12 then fault=other end
        else
            classification="Front / crossing contact"
        end
    end

    local severe=impulse>=math.max(0.0,c.severeIncidentNormalImpulseNs or 3200.0)
        or closing>=math.max(0.0,c.severeIncidentClosingKmh or 35.0)
    local verdict=fault and (severe and "Severe avoidable contact" or "Avoidable contact") or "Racing incident"
    if not other then verdict="Driver contact with static world" end
    return classification,verdict,fault,severe
end

function RacingAIRacecraft.ReportPhysicalContact(agent,other,evidence)
    if not agent or type(evidence)~="table" then return nil end
    local c=cfg(); if c.contactEvidenceEnabled==false then return nil end
    local s=stateFor(agent); local os=other and stateFor(other) or nil
    if (s.contactEvidenceCooldownS or 0.0)>0.0 or (os and (os.contactEvidenceCooldownS or 0.0)>0.0) then return nil end

    local classification,verdict,fault,severe=classifyPhysicalIncident(agent,other,evidence)
    if not classification then return nil end
    local cooldown=math.max(0.05,c.contactEvidenceCooldownSeconds or 0.85)
    s.contactEvidenceCooldownS=cooldown; if os then os.contactEvidenceCooldownS=cooldown end

    local penalty=0.0
    if fault and c.incidentStewardingEnabled~=false then
        penalty=severe and math.max(0.0,c.severeContactPenaltySeconds or 10.0) or math.max(0.0,c.avoidableContactPenaltySeconds or 5.0)
        if penalty>0.0 then
            local reason=classification.." with "..entrantName(fault==agent and other or agent)
            if applyStewardPenalty(fault,severe and "Severe Avoidable Contact" or "Avoidable Contact",penalty,reason) then
                local fs=stateFor(fault); fs.stewardPenaltyS=(fs.stewardPenaltyS or 0.0)+penalty
            else penalty=0.0 end
        end
    end

    local record={
        id=nextIncidentId,classification=classification,verdict=verdict,severe=severe==true,
        driverName=entrantName(agent),otherName=entrantName(other),faultName=entrantName(fault),
        driverEntrantId=agent.entrant and agent.entrant.id or 0,otherEntrantId=other and other.entrant and other.entrant.id or 0,
        faultEntrantId=fault and fault.entrant and fault.entrant.id or 0,
        contactZone=evidence.contactZone or "Unknown",normalImpulseNs=tonumber(evidence.normalImpulseNs) or 0.0,
        tangentImpulseNs=tonumber(evidence.tangentImpulseNs) or 0.0,relativeClosingKmh=tonumber(evidence.relativeClosingKmh) or 0.0,
        penetrationM=tonumber(evidence.penetrationM) or 0.0,penaltyS=penalty,
        globalX=tonumber(evidence.globalX) or 0.0,globalY=tonumber(evidence.globalY) or 0.0,globalZ=tonumber(evidence.globalZ) or 0.0
    }
    nextIncidentId=nextIncidentId+1
    incidentLedger[#incidentLedger+1]=record
    local retain=math.max(1,math.min(256,math.floor(c.retainedIncidentEvidence or 32)))
    while #incidentLedger>retain do table.remove(incidentLedger,1) end
    s.lastIncident=record; s.incidentCount=(s.incidentCount or 0)+1
    if os then os.lastIncident=record; os.incidentCount=(os.incidentCount or 0)+1 end
    if RacingReplay and RacingReplay.MarkIncident then record.replayClipId=RacingReplay.MarkIncident(record) or 0 end
    return record
end

function RacingAIRacecraft.GetIncidentLog(maxCount)
    local count=math.max(1,math.floor(tonumber(maxCount) or #incidentLedger))
    local out={}; local first=math.max(1,#incidentLedger-count+1)
    for i=#incidentLedger,first,-1 do out[#out+1]=incidentLedger[i] end
    return out
end

function RacingAIRacecraft.ClearIncidentLog()
    incidentLedger={}; nextIncidentId=1
end

function RacingAIRacecraft.Evaluate(agent,context,proposal,dt)
    if not agent or not proposal then return nil end
    local c=cfg(); local s=stateFor(agent); dt=math.max(0.0,dt or 0.0)
    s.contactEvidenceCooldownS=math.max(0.0,(s.contactEvidenceCooldownS or 0.0)-dt)
    local agents=context and context.agents or {}
    local maxAwareness=math.max(20.0,c.opponentAwarenessM or 100.0)
    local ahead,aheadGap=proposal.ahead,proposal.aheadGapM
    local behind,behindGap=proposal.behind,proposal.behindGapM
    if not ahead then ahead,aheadGap=closestAhead(agent,agents,maxAwareness) end
    if not behind then behind,behindGap=closestBehind(agent,agents,maxAwareness) end
    aheadGap=ahead and (aheadGap or signedGap(agent,ahead)) or math.huge
    behindGap=behind and (behindGap or -signedGap(agent,behind)) or math.huge

    local result={decision=proposal.decision,reason=proposal.reason,lateralOffsetM=proposal.lateralOffsetM,
        targetSpeedKmh=proposal.targetSpeedKmh,brakeDemand=proposal.brakeDemand,speedFactor=1.0}
    local meF=footprint(agent); local horizon=math.max(0.05,c.sweptEnvelopeSeconds or 0.70)
    s.predictedCollision=false; s.divebombState="none"; s.unsafeRelease=false

    -- Reset defensive-move accounting once the attacking car has genuinely gone away.
    if behind and behindGap<math.max(30.0,c.defensiveTriggerGapM or 22.0)*1.6 then s.noPressureS=0.0
    else
        s.noPressureS=s.noPressureS+dt
        if s.noPressureS>3.0 then s.defensiveMoves=0; s.blockingPenaltyIssued=false end
    end

    local defendingNow=proposal.defending==true or proposal.decision=="Defending"
    if defendingNow and not s.lastDefending then
        s.defensiveMoves=s.defensiveMoves+1
        local maxMoves=math.max(0,math.floor(c.maximumDefensiveMovesPerStraight or 1))
        if c.blockingRules~=false and s.defensiveMoves>maxMoves and not s.blockingPenaltyIssued then
            local seconds=math.max(0.0,c.blockingPenaltySeconds or 5.0)
            if applyStewardPenalty(agent,"Blocking",seconds,"More than one defensive move on the straight") then
                s.stewardPenaltyS=s.stewardPenaltyS+seconds; s.blockingPenaltyIssued=true
                result.decision="Stewarded blocking"; result.reason="Exceeded authored defensive-move limit"
            end
        end
    end
    s.lastDefending=defendingNow

    -- Predict the envelope of a closing car rather than waiting for actual collider contact.
    if c.predictiveCollisionAvoidance~=false and ahead then
        local overlap,longitudinalMargin,lateralMargin=predictedOverlap(agent,ahead,aheadGap,horizon,result.lateralOffsetM)
        s.predictedCollision=overlap; s.longitudinalMarginM=longitudinalMargin; s.lateralMarginM=lateralMargin
        if overlap then
            local otherOffset=lateral(ahead); local required=meF.halfWidthM+footprint(ahead).halfWidthM+meF.marginM
            local desiredSide=(result.lateralOffsetM or 0.0)>=otherOffset and 1.0 or -1.0
            local target=otherOffset+desiredSide*required
            local left=math.max(0.5,proposal.safeLeft or 6.0); local right=math.max(0.5,proposal.safeRight or 6.0)
            if target<=left and target>=-right then
                result.lateralOffsetM=clamp(target,-right,left); result.speedFactor=0.985
                result.decision="Swept-envelope avoidance"; result.reason="Collider footprints predict side-by-side overlap"
            else
                result.brakeDemand=math.max(result.brakeDemand or 0.0,0.68); result.speedFactor=0.94
                result.decision="Collision avoidance"; result.reason="No safe lateral room for predicted swept overlap"
            end
        end
    end

    -- Braking-battle / divebomb judgement. A closing car may commit only if its collider
    -- envelope can be placed alongside the opponent; otherwise it aborts before contact.
    if c.divebombJudgement~=false and ahead and aheadGap<math.max(2.0,c.divebombCommitGapM or 12.0) then
        local closingKmh=((agent.currentMps or 0.0)-(ahead.currentMps or 0.0))*3.6
        if closingKmh>=(c.divebombClosingThresholdKmh or 18.0) then
            local overlap=predictedOverlap(agent,ahead,aheadGap,horizon,result.lateralOffsetM)
            if overlap then
                s.divebombState="aborted"; result.brakeDemand=math.max(result.brakeDemand or 0.0,0.76); result.speedFactor=math.min(result.speedFactor,0.91)
                result.decision="Divebomb aborted"; result.reason="Predicted collider envelope cannot clear opponent"
            else
                s.divebombState="committed"; result.decision="Divebomb committed"; result.reason="Closing-speed threshold met with a clear side-by-side envelope"
            end
        end
    end

    -- Switchback/crossover: if the defender covers the current side at low gap, delay and
    -- cross to the opposite side rather than endlessly mirroring the block.
    if ahead and aheadGap<10.0 and ahead.ai and ahead.ai.defending and proposal.overtaking then
        s.switchbackRemainingS=math.max(s.switchbackRemainingS,c.switchbackWindowS or 1.5)
    end
    if s.switchbackRemainingS>0.0 then
        s.switchbackRemainingS=math.max(0.0,s.switchbackRemainingS-dt)
        if ahead and s.switchbackRemainingS<math.max(0.1,(c.switchbackWindowS or 1.5)*0.55) then
            local side=(lateral(ahead)>=0.0) and -1.0 or 1.0
            local magnitude=math.min(math.max(1.0,meF.widthM*0.75),math.max(1.0,proposal.maxOffset or 3.0))
            result.lateralOffsetM=side*magnitude; result.decision="Switchback / crossover"; result.reason="Defender committed to first line"
        end
    end

    -- Plan multi-class traffic before the faster car reaches bumper distance.
    if c.multiclassNegotiation~=false and ahead and not sameClass(agent,ahead) then
        local closing=math.max(0.001,(agent.currentMps or 0.0)-(ahead.currentMps or 0.0))
        local catchS=aheadGap/closing
        if catchS<math.max(0.1,c.multiclassPassHorizonS or 2.5) and (agent.currentMps or 0.0)>(ahead.currentMps or 0.0) then
            local side=(lateral(ahead)>=0.0) and -1.0 or 1.0
            local magnitude=math.min(math.max(1.0,meF.widthM*0.70),math.max(1.0,proposal.maxOffset or 3.0))
            result.lateralOffsetM=side*magnitude; result.decision="Multi-class pass plan"; result.reason="Predicted catch inside authored planning horizon"
        end
    end

    -- Unsafe pit release stewarding. The pit path transition itself remains owned by
    -- MotorsportWeekend; this layer only judges traffic proximity at the rejoin.
    local previousPit=s.lastPitMode
    local currentPit=agent.pitMode
    if c.unsafeReleaseStewarding~=false and previousPit and previousPit~="" and not currentPit then
        local nearestBehind,releaseGap=closestBehind(agent,agents,math.max(5.0,c.pitReleaseLookaheadM or 55.0))
        if nearestBehind and releaseGap<(c.pitReleaseLookaheadM or 55.0) then
            local seconds=math.max(0.0,c.unsafeReleasePenaltySeconds or 7.0)
            if applyStewardPenalty(agent,"Unsafe Release",seconds,"Unsafe pit release into approaching traffic") then
                s.stewardPenaltyS=s.stewardPenaltyS+seconds; s.unsafeRelease=true; s.unsafeReleaseCount=s.unsafeReleaseCount+1
                result.decision="Unsafe pit release"; result.reason="Approaching competitor inside pit-release lookahead"
            end
        end
    end
    s.lastPitMode=currentPit
    local currentLateral=result.lateralOffsetM or 0.0
    s.lastLateralDelta=currentLateral-(s.lastLateral or 0.0)
    s.lastLateral=currentLateral
    return result
end

function RacingAIRacecraft.GetTelemetry(agent)
    if not agent then return nil end
    local f=footprint(agent); local s=agent.ai and agent.ai.racecraftState or {}
    return {
        footprintSource=f.source,footprintWidthM=f.widthM,footprintLengthM=f.lengthM,footprintColliderCount=f.colliderCount,
        predictedCollision=s.predictedCollision==true,longitudinalMarginM=s.longitudinalMarginM or 0.0,lateralMarginM=s.lateralMarginM or 0.0,
        divebombState=s.divebombState or "none",defensiveMoves=s.defensiveMoves or 0,switchbackRemainingS=s.switchbackRemainingS or 0.0,
        stewardPenaltyS=agent.stewardPenaltyS or s.stewardPenaltyS or 0.0,unsafeRelease=s.unsafeRelease==true,unsafeReleaseCount=s.unsafeReleaseCount or 0,
        incidentCount=s.incidentCount or 0,lastIncident=s.lastIncident,contactEvidenceCooldownS=s.contactEvidenceCooldownS or 0.0
    }
end
