-- Shared build-identity and lifetime-safety controls.
function DrawSafetyNetPanel()
    UI.Text("Build step: " .. Engine.GetBuildStep())
    UI.Text("Configuration: " .. Engine.GetBuildConfiguration())
    UI.Text("Git commit: " .. Engine.GetGitCommit())
    UI.TextWrapped("Build identity: " .. Engine.GetBuildIdentity())
    UI.Text("Live Lua API functions: " .. tostring(Engine.GetLuaApiCount()))
    UI.Spacing()

    if UI.Button("DUMP LIVE LUA API TO CONSOLE") then
        local count = Engine.DumpLuaAPI()
        safetyNetMessage = "Dumped " .. tostring(count)
            .. " exact Lua API names. Runtime manifests were refreshed in Build/Reports."
    end

    if UI.Button("RUN ENGINE SAFETY SMOKE TESTS") then
        local passed, summary, reportPath = Engine.RunSafetySmokeTests()
        safetyNetMessage = summary
        safetyNetReportPath = reportPath or ""
        if passed then
            saveMessage = "Engine safety smoke tests passed"
        else
            saveMessage = "Engine safety smoke tests failed - send the report"
        end
    end

    UI.Spacing()
    if string.sub(safetyNetMessage, 1, 5) == "FAIL:" then
        UI.TextColored(safetyNetMessage, 1.0, 0.35, 0.35, 1.0)
    else
        UI.TextWrapped(safetyNetMessage)
    end
    if safetyNetReportPath ~= "" then
        UI.TextDisabled("Report: " .. safetyNetReportPath)
    end
end
