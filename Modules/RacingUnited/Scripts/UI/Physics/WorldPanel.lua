-- Fixed-world clock, contact and broadphase diagnostics.
function DrawPhysicsWorldPanel()
    local changed = false
    SetPrototypeScenePreset("physics")

    UI.Text("Physics service available: " .. tostring(Physics.IsAvailable()))
    UI.Text(string.format("Fixed world step: %.6f s (%.0f Hz)", Physics.GetFixedDelta(), Physics.GetTickRate()))
    UI.Text(string.format("World steps this rendered frame: %d / %d",
        Physics.GetLastWorldStepCount(), Physics.GetMaximumWorldStepsPerFrame()))
    UI.Text("Pending world steps: " .. tostring(Physics.GetPendingWorldStepCount()))
    UI.Text("Total fixed world steps: " .. tostring(Physics.GetStepCount()))
    UI.Text(string.format("Simulation time: %.3f s", Physics.GetSimulationTime()))
    UI.Text(string.format("Interpolation alpha: %.3f", Physics.GetInterpolationAlpha()))
    UI.Text(string.format("Recovery backlog: %.3f ms (peak %.3f ms)",
        Physics.GetBacklogTime() * 1000.0, Physics.GetPeakBacklogTime() * 1000.0))
    UI.Text("Overloaded last frame: " .. tostring(Physics.WasOverloadedLastFrame()))
    UI.Text("Overloaded rendered frames: " .. tostring(Physics.GetOverloadFrameCount()))
    UI.Text(string.format("Dropped backlog time: %.6f s", Physics.GetDroppedTime()))
    UI.Text(string.format("Clamped giant-stall time: %.6f s", Physics.GetClampedTime()))

    UI.Spacing()
    UI.Text(string.format("Bodies active / sleeping: %d / %d",
        Physics.GetActiveDynamicBodyCount(), Physics.GetSleepingBodyCount()))
    UI.Text(string.format("Bodies / colliders / constraints: %d / %d / %d",
        Physics.GetBodyCount(), Physics.GetColliderCount(), Physics.GetConstraintCount()))
    UI.Text(string.format("Simulation islands total / active / sleeping: %d / %d / %d",
        Physics.GetSimulationIslandCount(), Physics.GetActiveIslandCount(), Physics.GetSleepingIslandCount()))
    UI.Text(string.format("Contacts / warm-started / persistent: %d / %d / %d",
        Physics.GetContactCount(), Physics.GetWarmStartedContactCount(), Physics.GetPersistentContactCount()))
    UI.Text(string.format("Broadphase / narrowphase / resolved: %d / %d / %d",
        Physics.GetBroadphaseCandidateCount(), Physics.GetNarrowphaseTestCount(), Physics.GetResolvedContactCount()))

    local gravityX, gravityY, gravityZ = Physics.GetGravity()
    UI.Text(string.format("Gravity: %.3f, %.3f, %.3f m/s^2", gravityX, gravityY, gravityZ))

    physicsTickRate, changed = UI.SliderFloat(
        "Physics world tick rate", physicsTickRate, 30.0, 240.0, "%.0f Hz")
    if changed then
        if Physics.SetTickRate(physicsTickRate) then
            physicsMessage = "Changed the deterministic physics world tick rate"
        else
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        end
    end

    physicsTimeScale, changed = UI.SliderFloat(
        "Physics time scale", physicsTimeScale, 0.0, 2.0, "%.2f x")
    if changed then
        if Physics.SetTimeScale(physicsTimeScale) then
            physicsMessage = "Changed physics time scale"
        else
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        end
    end

    physicsPaused, changed = UI.Checkbox("Pause module physics", Physics.IsPaused())
    if changed then
        Physics.SetPaused(physicsPaused)
        physicsMessage = physicsPaused and "Paused the native physics clock" or "Resumed the native physics clock"
    end
    if UI.Button("SINGLE FIXED STEP") then
        Physics.RequestSingleStep()
        physicsMessage = "Queued one fixed physics step"
    end
    if UI.Button("RESET PHYSICS CLOCK STATISTICS") then
        Physics.ResetClock()
        physicsMessage = "Reset physics time, backlog and overload statistics"
    end
    UI.TextDisabled(physicsMessage)
end
