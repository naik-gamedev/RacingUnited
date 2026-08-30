local racePanelMessage = ""
local raceAiSelectedIndex = 1

local function formatRaceTime(seconds)
    seconds=tonumber(seconds) or 0.0
    if seconds<=0.0 then return "--:--.---" end
    local minutes=math.floor(seconds/60.0); local remain=seconds-minutes*60.0
    return string.format("%02d:%06.3f",minutes,remain)
end

local function formatSignedDelta(seconds)
    seconds=tonumber(seconds) or 0.0
    return string.format("%+.3f s",seconds)
end

function DrawPrototypeRacePanel()
    UI.TextDisabled("EVENT EXECUTION / RACING AI / CONE COURSES / SECTION PRACTICE")
    UI.TextWrapped("STUDIO28 adds physical traffic cones and deterministic Autoslalom/Gymkhana course elements to the existing authored motorsport/clandestine event runtime. Persistent cones can also guide, slow or close free-roam traffic links.")
    UI.Spacing()

    local selected=RacingEvents.GetSelectedEvent()
    local selectedSession=RacingEvents.GetSelectedSession()
    UI.Text("Selected event: "..(selected and (selected.name.." ["..tostring(selected.type).."]") or "(no enabled events)"))
    UI.Text("Selected session: "..(selectedSession and (selectedSession.name.." ["..tostring(selectedSession.type).."]") or "default race session"))
    if UI.Button("PREV EVENT",120.0,27.0,false) then RacingEvents.SelectNextEvent(-1) end
    UI.SameLine(); if UI.Button("NEXT EVENT",120.0,27.0,false) then RacingEvents.SelectNextEvent(1) end
    UI.SameLine(); if UI.Button("PREV SESSION",135.0,27.0,false) then RacingEvents.SelectNextSession(-1) end
    UI.SameLine(); if UI.Button("NEXT SESSION",135.0,27.0,false) then RacingEvents.SelectNextSession(1) end
    if UI.Button("STAGE / START SELECTED",210.0,30.0,false) then
        if selected then local ok,msg=RacingEvents.StartEvent(selected,selectedSession and selectedSession.id or nil); racePanelMessage=msg or tostring(ok) else racePanelMessage="No enabled event is authored." end
    end
    UI.SameLine(); if UI.Button("ABORT EVENT",130.0,30.0,false) then RacingEvents.AbortEvent("Event aborted from debug panel."); racePanelMessage="Event aborted." end
    UI.SameLine(); if UI.Button("ADVANCE SESSION",150.0,30.0,false) then local ok,msg=RacingEvents.AdvanceSession(); racePanelMessage=msg or tostring(ok) end

    local t=RacingEvents.GetTelemetry()
    UI.Spacing(); UI.Separator(); UI.Spacing()
    UI.Text(string.format("Phase %s | Flag %s | %s / %s",t.phase or "Idle",t.flag or "Green",t.eventName~="" and t.eventName or "no event",t.sessionName~="" and t.sessionName or "no session"))
    if t.phase=="Staging" then UI.Text(string.format("Staging %.2fs",t.stagingRemainingS or 0.0)) end
    if t.phase=="Countdown" then UI.Text(string.format("START IN %.2f",t.countdownRemainingS or 0.0)) end
    UI.Text(string.format("Lap %d / %d | completed %d | checkpoints %d / %d",t.currentLap or 1,t.targetLaps or 1,t.completedLaps or 0,t.checkpointIndex or 0,t.checkpointCount or 0))
    UI.Text(string.format("Current %s | Last %s | Best %s",formatRaceTime(t.currentLapTimeS),formatRaceTime(t.lastLapS),formatRaceTime(t.bestLapS)))
    UI.Text(string.format("Elapsed %s | Penalties +%.1fs (%d) | Track limits %d",formatRaceTime(t.elapsedS),t.totalPenaltyS or 0.0,t.penaltyCount or 0,t.trackLimitWarnings or 0))
    UI.Text(string.format("Pit %s | Stops %d / %d | Drive-through %s",t.inPit and "IN" or "OUT",t.pitStops or 0,t.mandatoryPitStops or 0,t.pendingDriveThrough and "PENDING" or "none"))
    if t.coneCourse then
        local c=t.coneCourse
        UI.TextDisabled("STUDIO28 AUTOSLALOM / GYMKHANA")
        UI.Text(string.format("Element %d / %d | next %s | cone hits %d | course penalties %d",c.gateIndex or 0,c.gateCount or 0,c.expectedGateName~="" and c.expectedGateName or "finish",c.coneHits or 0,c.penaltyCount or 0))
        if (c.splitCount or 0)>0 then
            if c.lastSplitHasPb then
                UI.Text(string.format("Last split: %s  %s  | PB delta %s",c.lastSplitName or "element",formatRaceTime(c.lastSplitTimeS),formatSignedDelta(c.lastSplitDeltaS)))
            else
                UI.Text(string.format("Last split: %s  %s  | no PB reference yet",c.lastSplitName or "element",formatRaceTime(c.lastSplitTimeS)))
            end
        end
        UI.TextDisabled(c.message or "")
        if UI.Button("RESET EVENT CONES",170.0,26.0,false) then RacingConeCourse.ResetActiveEventCones(); racePanelMessage="Event cones restored to authored transforms." end
    end
    if t.result then
        UI.Text(string.format("RESULT: %s | adjusted %s | %s",t.result.reason or "Finished",formatRaceTime(t.result.adjustedS),t.result.dnf and "DNF" or "classified"))
        if (t.result.personalBestS or 0)>0 then UI.Text(string.format("Course PB %s%s",formatRaceTime(t.result.personalBestS),t.result.newPersonalBest and "  NEW BEST" or "")) end
    end

    UI.Spacing(); UI.Separator(); UI.Spacing()
    UI.TextDisabled("STUDIO23 MOTORSPORT / SOLVER-CONTACT STEWARDING")
    local mt=RacingMotorsport.GetTelemetry()
    UI.Text(string.format("Grid %d | AI %d | physical %d (%d native) | running %d | finished %d | DNF %d",mt.gridSize or 0,mt.agents or 0,mt.physical or 0,mt.fullPhysics or 0,mt.running or 0,mt.finished or 0,mt.dnf or 0))
    UI.Text(string.format("Overtaking %d | slipstreaming %d | pit service %d",mt.overtaking or 0,mt.slipstreaming or 0,mt.pitting or 0))
    if UI.Button("REBUILD GRID",135.0,27.0,false) then
        if selected then local grid=RacingMotorsport.BuildGrid(selected,selectedSession); racePanelMessage="Built "..tostring(#grid).."-entrant grid." else racePanelMessage="No event selected." end
    end
    local q=selected and RacingMotorsport.GetQualifying(selected.id) or {}
    if #q>0 then
        UI.TextDisabled("QUALIFYING ORDER")
        for i=1,math.min(#q,8) do local row=q[i]; UI.Text(string.format("P%d  %s  %s",i,row.driverName or "AI",formatRaceTime(row.timeS))) end
    end
    local championships=RacingGameplay.data.motorsportChampionships or {}
    if #championships>0 then
        local standings=RacingMotorsport.GetChampionshipStandings(championships[1].id)
        UI.TextDisabled("CHAMPIONSHIP: "..tostring(championships[1].name or "Series"))
        for i=1,math.min(#standings,8) do local row=standings[i]; UI.Text(string.format("P%d  %s  %.1f pts",i,row.entrant.driverName or "Player",row.points or 0.0)) end
    end
    UI.TextDisabled(mt.message or "")

    local aiRows=RacingMotorsport.GetAiTelemetry and RacingMotorsport.GetAiTelemetry() or {}
    if #aiRows>0 then
        raceAiSelectedIndex=math.max(1,math.min(raceAiSelectedIndex,#aiRows))
        UI.Spacing(); UI.TextDisabled("STUDIO23 LIVE RACING AI / PHYSICAL INCIDENT EVIDENCE")
        if UI.Button("PREV AI",95.0,25.0,false) then raceAiSelectedIndex=math.max(1,raceAiSelectedIndex-1) end
        UI.SameLine(); if UI.Button("NEXT AI",95.0,25.0,false) then raceAiSelectedIndex=math.min(#aiRows,raceAiSelectedIndex+1) end
        local ai=aiRows[raceAiSelectedIndex]
        UI.SameLine(); UI.Text(string.format("P%d  #%d %s",ai.runningPosition or raceAiSelectedIndex,ai.raceNumber or 0,ai.driverName or "AI"))
        UI.Text(string.format("Decision: %s | %s",ai.decision or "",ai.reason or ""))
        UI.Text(string.format("Line: %s | offset %+.2fm | wet blend %.0f%%",ai.line or "",ai.lateralOffsetM or 0.0,(ai.wetLineBlend or 0.0)*100.0))
        UI.Text(string.format("Speed %.1f -> %.1f km/h | brake intent %.0f%%",ai.speedKmh or 0.0,ai.targetSpeedKmh or 0.0,(ai.brakeDemand or 0.0)*100.0))
        UI.Text(string.format("Backend %s | throttle %.0f%% brake %.0f%% steer %+.0f%%",ai.physicalBackend or "Logical",(ai.throttle or 0.0)*100.0,(ai.brake or 0.0)*100.0,(ai.steering or 0.0)*100.0))
        UI.Text(string.format("Grip %.0f%% | grounded %d | slip ratio %.3f | slip angle %.1f deg",(ai.gripFactor or 1.0)*100.0,ai.groundedWheels or 0,ai.maxSlipRatio or 0.0,ai.maxSlipAngleDeg or 0.0))
        UI.Text(string.format("Cross-track %+.2fm | mechanical health %.0f%% | contacts %d | recoveries %d",ai.lateralErrorM or 0.0,(ai.mechanicalHealth or 1.0)*100.0,ai.contacts or 0,ai.recoveries or 0))
        local contact=ai.contactEvidence
        if contact then
            UI.Text(string.format("Solver contact %s | impulse %.0f N s | closing %.1f km/h | penetration %.1f mm",contact.contactZone or "?",contact.normalImpulseNs or 0.0,contact.relativeClosingKmh or 0.0,(contact.penetrationM or 0.0)*1000.0))
            UI.Text(string.format("Contact normal [%+.2f %+.2f %+.2f] | other body %s",contact.normalX or 0.0,contact.normalY or 0.0,contact.normalZ or 0.0,tostring(contact.otherBody or 0)))
        else UI.TextDisabled("Solver contact: none this physics step") end
        UI.Text(string.format("Collider footprint %s | %.2fm x %.2fm | %d solid collider(s)",ai.footprintSource or "--",ai.footprintWidthM or 0.0,ai.footprintLengthM or 0.0,ai.footprintColliderCount or 0))
        UI.Text(string.format("Swept collision %s | long margin %+.2fm | lateral margin %+.2fm | divebomb %s",ai.predictedCollision and "RISK" or "clear",ai.longitudinalMarginM or 0.0,ai.lateralMarginM or 0.0,ai.divebombState or "none"))
        UI.Text(string.format("Defensive moves %d | steward +%.1fs | unsafe release %s",ai.defensiveMoves or 0,ai.stewardPenaltyS or 0.0,ai.unsafeRelease and "YES" or "no"))
        UI.Text(string.format("Tire %.1f C | thermal grip %.0f%% | vehicle %.1f kg (fuel %.1f kg)",ai.tireTemperatureC or 0.0,(ai.thermalGripFactor or 1.0)*100.0,ai.vehicleMassKg or 0.0,ai.fuelMassKg or 0.0))
        local ch=ai.componentHealth or {}; UI.Text(string.format("Components: aero %.0f%% | suspension %.0f%% | powertrain %.0f%%",(ch.aero or 1.0)*100.0,(ch.suspension or 1.0)*100.0,(ch.powertrain or 1.0)*100.0))
        UI.Text(string.format("Weather forecast wetness %.0f%% | trend %+.4f/s",(ai.forecastWetness or 0.0)*100.0,ai.wetnessTrendPerSecond or 0.0))
        UI.Text(string.format("Ahead %s %.1fm | Behind %s %.1fm",ai.aheadName~="" and ai.aheadName or "--",ai.aheadGapM or 0.0,ai.behindName~="" and ai.behindName or "--",ai.behindGapM or 0.0))
        UI.Text(string.format("Slipstream %s | overtake %s | defend %s | yield %s",ai.slipstream and "YES" or "no",ai.overtaking and "YES" or "no",ai.defending and "YES" or "no",ai.yielding and "YES" or "no"))
        UI.Text(string.format("Fuel %.1f L / %.2f laps | Tire %.0f%% %s | Pit %s",ai.fuelLiters or 0.0,ai.fuelLaps or 0.0,(ai.tireLife or 0.0)*100.0,ai.tireCompound or "Dry",ai.pitRequested and (ai.pitReason~="" and ai.pitReason or "REQUESTED") or "no"))
        local incident=ai.lastIncident
        if incident then
            UI.Text(string.format("Last steward evidence #%d: %s -> %s",incident.id or 0,incident.classification or "Contact",incident.verdict or "Review"))
            UI.Text(string.format("Fault %s | penalty +%.1fs | impulse %.0f N s | closing %.1f km/h",incident.faultEntrantId and incident.faultEntrantId~=0 and (incident.faultName or "?") or "none",incident.penaltyS or 0.0,incident.normalImpulseNs or 0.0,incident.relativeClosingKmh or 0.0))
        end
        if ai.pitMode and ai.pitMode~="" then UI.Text(string.format("Physical pit route: %s  %.1f / %.1f m",ai.pitMode,ai.pitDistanceM or 0.0,ai.pitRouteLengthM or 0.0)) end
        UI.ProgressBar(ai.tireLife or 0.0,420,15,"Tire life")
        UI.Text(string.format("Mistakes %d | recovery %.2fs",ai.mistakes or 0,ai.mistakeRemainingS or 0.0))

        local incidents=RacingAIRacecraft and RacingAIRacecraft.GetIncidentLog and RacingAIRacecraft.GetIncidentLog(5) or {}
        if #incidents>0 then
            UI.Spacing(); UI.TextDisabled("RECENT STEWARD EVIDENCE")
            for _,ev in ipairs(incidents) do
                UI.Text(string.format("#%d %s: %s / %s / %.0f N s / +%.1fs",ev.id or 0,ev.driverName or "AI",ev.classification or "Contact",ev.verdict or "Review",ev.normalImpulseNs or 0.0,ev.penaltyS or 0.0))
            end
        end
    end

    UI.Spacing(); UI.Separator(); UI.Spacing()
    UI.TextDisabled("STUDIO27 INCIDENT REPLAY / STATIC + MOVING BROADCAST REVIEW")
    local replay=RacingReplay and RacingReplay.GetTelemetry and RacingReplay.GetTelemetry() or {}
    UI.Text(string.format("Capture %s | %.1f Hz | pre-buffer %d frames | clips %d (%d capturing)",replay.recording and "ARMED" or "idle",replay.sampleHz or 0.0,replay.bufferedFrames or 0,replay.clips or 0,replay.activeClips or 0))
    if (replay.clips or 0)>0 then
        if UI.Button("PREV REPLAY",115.0,26.0,false) then RacingReplay.SelectRelative(-1) end
        UI.SameLine(); if UI.Button("NEXT REPLAY",115.0,26.0,false) then RacingReplay.SelectRelative(1) end
        UI.SameLine(); if UI.Button(replay.reviewEnabled and "HIDE GHOSTS" or "SHOW GHOSTS",125.0,26.0,false) then RacingReplay.ToggleReview() end
        UI.SameLine(); if UI.Button(replay.reviewPlaying and "PAUSE" or "PLAY",90.0,26.0,false) then RacingReplay.SetReviewPlaying(not replay.reviewPlaying) end
        if replay.broadcastDirectorEnabled~=false then
            local authoredId=(replay.authoredCameraPathId or 0)~=0 and (" path #"..tostring(replay.authoredCameraPathId)) or ((replay.authoredCameraId or 0)~=0 and (" #"..tostring(replay.authoredCameraId)) or "")
            UI.Text(string.format("Replay camera: %s%s | %s%s",replay.cameraMode or "Off",replay.cameraActive and " [ACTIVE]" or "",replay.cameraSource or "Procedural",authoredId))
            if UI.Button("INCIDENT CAM",115.0,25.0,false) then RacingReplay.SetReviewEnabled(true); RacingReplay.SetCameraMode("Incident") end
            UI.SameLine(); if UI.Button("TRACKSIDE",105.0,25.0,false) then RacingReplay.SetReviewEnabled(true); RacingReplay.SetCameraMode("Trackside") end
            UI.SameLine(); if UI.Button("MOVING TV",105.0,25.0,false) then RacingReplay.SetReviewEnabled(true); RacingReplay.SetCameraMode("Spline") end
            UI.SameLine(); if UI.Button("CHASE CAM",105.0,25.0,false) then RacingReplay.SetReviewEnabled(true); RacingReplay.SetCameraMode("Chase") end
            UI.SameLine(); if UI.Button("HELICOPTER",105.0,25.0,false) then RacingReplay.SetReviewEnabled(true); RacingReplay.SetCameraMode("Helicopter") end
            UI.SameLine(); if UI.Button("CAM OFF",85.0,25.0,false) then RacingReplay.SetCameraMode("Off") end
        end
        if UI.Button("-1.0s",75.0,25.0,false) then RacingReplay.StepReview(-1.0) end
        UI.SameLine(); if UI.Button("-0.1s",75.0,25.0,false) then RacingReplay.StepReview(-0.1) end
        UI.SameLine(); if UI.Button("IMPACT",90.0,25.0,false) then RacingReplay.JumpToIncident() end
        UI.SameLine(); if UI.Button("+0.1s",75.0,25.0,false) then RacingReplay.StepReview(0.1) end
        UI.SameLine(); if UI.Button("+1.0s",75.0,25.0,false) then RacingReplay.StepReview(1.0) end
        local duration=math.max(0.01,replay.selectedDurationS or 0.01); local seek,changed=UI.SliderFloat("Replay timeline",replay.reviewOffsetS or 0.0,0.0,duration,"%.2f s")
        if changed then RacingReplay.SetReviewOffset(seek) end
        local snap=RacingReplay.GetReviewSnapshot()
        if snap then
            local clip=snap.clip or {}
            UI.Text(string.format("Replay #%d / incident #%d | %s | %s | t %+0.2fs from impact",clip.id or 0,clip.incidentId or 0,clip.classification or "Contact",clip.verdict or "Review",snap.timeFromIncidentS or 0.0))
            UI.Text(string.format("Recorded phase %s | flag %s | %d vehicle states",snap.phase or "",snap.flag or "",#(snap.racers or {})))
            for i=1,math.min(8,#(snap.racers or {})) do
                local r=snap.racers[i]
                UI.Text(string.format("  %s #%d | %s | %.1f km/h | lap %d | offset %+.2fm",r.driverName or r.participantId or "car",r.raceNumber or 0,r.backend or "",r.speedKmh or 0.0,r.completedLaps or 0,r.lateralOffsetM or 0.0))
            end
        end
    else
        UI.TextDisabled("No accepted STUDIO23 physical incident has created a replay clip yet.")
    end
    UI.TextDisabled(replay.message or "")

    UI.Spacing()
    UI.TextDisabled("RACE CONTROL DEBUG")
    if UI.Button("GREEN",95.0,26.0,false) then RacingEvents.SetFlag("Green") end
    UI.SameLine(); if UI.Button("FCY",80.0,26.0,false) then RacingEvents.SetFlag("Full Course Yellow") end
    UI.SameLine(); if UI.Button("VSC",80.0,26.0,false) then RacingEvents.SetFlag("Virtual Safety Car") end
    UI.SameLine(); if UI.Button("SAFETY CAR",120.0,26.0,false) then RacingEvents.SetFlag("Safety Car") end
    UI.SameLine(); if UI.Button("RED",80.0,26.0,false) then RacingEvents.SetFlag("Red") end
    UI.SameLine(); if UI.Button("CHEQUERED",120.0,26.0,false) then RacingEvents.SetFlag("Chequered") end

    UI.Spacing(); UI.Separator(); UI.Spacing()
    local loop=RacingPracticeLoop.GetTelemetry()
    UI.TextDisabled("PORTAL-STYLE PRACTICE LOOP")
    UI.Text(string.format("Start %s | End %s | Playback %s | entry %.1f km/h gear %s",loop.hasStart and "SET" or "--",loop.hasEnd and "SET" or "--",loop.enabled and "ACTIVE" or "OFF",loop.startSpeedKmh or 0.0,tostring(loop.startGear or 0)))
    UI.Text(string.format("Attempt %d | current %s | last %s | best %s",loop.attempts or 0,formatRaceTime(loop.currentAttemptS),formatRaceTime(loop.lastAttemptS),formatRaceTime(loop.bestAttemptS)))
    if UI.Button("CAPTURE START [F5]",165.0,28.0,false) then local _,msg=RacingPracticeLoop.CaptureStart(); racePanelMessage=msg or "" end
    UI.SameLine(); if UI.Button("CAPTURE END [F6]",165.0,28.0,false) then local _,msg=RacingPracticeLoop.CaptureEnd(); racePanelMessage=msg or "" end
    UI.SameLine(); if UI.Button("RESTART [F3]",130.0,28.0,false) then RacingPracticeLoop.RestartNow() end
    UI.SameLine(); if UI.Button("TOGGLE [F4]",125.0,28.0,false) then RacingPracticeLoop.Toggle() end
    if UI.Button("CLEAR PRACTICE LOOP",190.0,27.0,false) then RacingPracticeLoop.Clear(); racePanelMessage="Practice loop cleared." end
    UI.TextDisabled(loop.message or "")
    if racePanelMessage~="" then UI.TextWrapped(racePanelMessage) end
end
