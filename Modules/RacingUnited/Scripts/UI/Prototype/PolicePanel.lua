local policeDebugMessage = ""

function DrawPrototypePolicePanel()
    UI.TextDisabled("POLICE / CLANDESTINE FREE-ROAM")
    UI.TextWrapped("STUDIO17 pursuit authority: witnessing, heat, dispatch, patrol coverage, search/cooldown, roadblocks, escape zones, speed enforcement and underground meet activity.")
    UI.Spacing()
    local cfg = RacingPolice.GetConfiguration()
    local t = RacingPolice.GetTelemetry()
    UI.Text(string.format("Police gameplay: %s | State: %s | Heat %.2f / %d [level %d]", cfg.enabled and "ON" or "OFF", tostring(t.state), t.heat or 0.0, cfg.maxHeatLevel or 5, t.heatLevel or 0))
    UI.Text(string.format("Pursuit units %d / %d | Active roadblock %s | Patrol coverage %.2f (%d zones)", t.units or 0, cfg.maxPursuitUnits or 0, tostring(t.roadblockId or 0), t.patrolWeight or 0.0, t.patrolZoneCount or 0))
    UI.Text(string.format("Player %.1f km/h | nearby limit %.1f km/h | witnessed %s | escape zone %s", t.playerSpeedKmh or 0.0, t.playerSpeedLimitKmh or 0.0, t.witness and "YES" or "NO", t.escapeZone and "YES" or "NO"))
    UI.Text(string.format("Infraction %s | search %.1fs | cooldown %.1fs | meet %s risk %.2f", tostring(t.lastInfraction or "none"), t.searchRemainingS or 0.0, t.cooldownRemainingS or 0.0, tostring(t.activeMeetId or 0), t.activeMeetRisk or 0.0))
    UI.Text(string.format("Totals: infractions %d | pursuits %d | escapes %d | busts %d | roadblocks %d | dispatched %d", t.infractions or 0, t.pursuits or 0, t.escapes or 0, t.busts or 0, t.roadblocks or 0, t.dispatched or 0))
    UI.Spacing()
    if UI.Button("REPORT SPEEDING", 170.0, 28.0, false) then local ok,msg=RacingPolice.ReportInfraction("speeding",1.0,nil,true); policeDebugMessage=msg or tostring(ok) end
    UI.SameLine()
    if UI.Button("REPORT COLLISION", 180.0, 28.0, false) then local ok,msg=RacingPolice.ReportInfraction("collision",1.0,nil,true); policeDebugMessage=msg or tostring(ok) end
    UI.SameLine()
    if UI.Button("HEAT +1", 120.0, 28.0, false) then policeDebugMessage="Heat "..string.format("%.2f",RacingPolice.DebugSetHeat((t.heat or 0)+1.0)) end
    if UI.Button("FORCE ESCAPE / SEARCH", 210.0, 28.0, false) then RacingPolice.DebugEscape(); policeDebugMessage="Forced pursuit into search/escape test." end
    UI.SameLine()
    if UI.Button("CLEAR POLICE STATE", 190.0, 28.0, false) then RacingPolice.DebugClear(); policeDebugMessage="Police runtime state cleared." end
    if policeDebugMessage~="" then UI.TextWrapped(policeDebugMessage) end
    if t.debugMessage and t.debugMessage~="" then UI.TextDisabled(t.debugMessage) end
end
