-- Rigid-body rotation, sleep and impulse diagnostics.
function DrawPhysicsBodyPanel()
    SetPrototypeScenePreset("physics")

    if UI.Button("RESET ANGULAR BOX PROBE") then ResetPhysicsProbe() end
    if UI.Button("APPLY CENTRE UPWARD IMPULSE") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            Physics.ApplyBodyImpulse(physicsProbeBody, 0.0, 4.0, 0.0)
            physicsMessage = "Applied a centre-of-mass upward impulse"
        end
    end
    if UI.Button("APPLY OFF-CENTRE IMPULSE - MAKE IT SPIN") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            local px, py, pz = Physics.GetBodyPosition(physicsProbeBody)
            Physics.ApplyBodyImpulseAtPoint(
                physicsProbeBody, 0.0, 2.2, 0.35,
                px + physicsProbeHalfX, py, pz)
            physicsMessage = "Applied an impulse away from the centre of mass; inertia converted it into translation and rotation"
        end
    end
    if UI.Button("APPLY DIRECT ANGULAR IMPULSE") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            Physics.ApplyBodyAngularImpulse(physicsProbeBody, 0.0, 0.12, 0.10)
            physicsMessage = "Applied a direct angular impulse around two world axes"
        end
    end
    if UI.Button("PUT PROBE TO SLEEP - TEST") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            if Physics.SetBodySleeping(physicsProbeBody, true) then
                physicsMessage = "Explicitly put the probe to sleep; integration and contact solving can now skip it"
            else
                physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            end
        end
    end
    if UI.Button("WAKE PROBE WITHOUT IMPULSE") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            if Physics.WakeBody(physicsProbeBody) then
                physicsMessage = "Woke the probe without changing its velocity"
            else
                physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            end
        end
    end

    UI.Spacing()
    if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
        local probeX, probeY, probeZ = Physics.GetBodyPosition(physicsProbeBody)
        local velocityX, velocityY, velocityZ = Physics.GetBodyLinearVelocity(physicsProbeBody)
        local rotationX, rotationY, rotationZ = Physics.GetBodyRotation(physicsProbeBody)
        local angularX, angularY, angularZ = Physics.GetBodyAngularVelocity(physicsProbeBody)
        UI.Text("Native body handle: " .. tostring(physicsProbeBody))
        UI.Text("Attached colliders: " .. tostring(Physics.GetBodyColliderCount(physicsProbeBody)))
        UI.Text("Probe contacts: " .. tostring(Physics.GetBodyContactCount(physicsProbeBody)))
        UI.Text("Touching / sleeping: " .. tostring(Physics.IsBodyTouching(physicsProbeBody))
            .. " / " .. tostring(Physics.IsBodySleeping(physicsProbeBody)))
        UI.Text("Motion type: " .. tostring(Physics.GetBodyMotionType(physicsProbeBody)))
        UI.Text(string.format("Mass: %.2f kg", Physics.GetBodyMass(physicsProbeBody)))
        UI.Text(string.format("Position: %.2f, %.2f, %.2f", probeX, probeY, probeZ))
        UI.Text(string.format("Linear velocity: %.3f, %.3f, %.3f m/s", velocityX, velocityY, velocityZ))
        UI.Text(string.format("Rotation: %.1f, %.1f, %.1f deg", rotationX, rotationY, rotationZ))
        UI.Text(string.format("Angular velocity: %.2f, %.2f, %.2f deg/s", angularX, angularY, angularZ))
    end
    UI.TextDisabled(physicsMessage)
end
