-- Spring-damper constraint laboratory.
function DrawPhysicsSuspensionPanel()
    local changed = false
    SetPrototypeScenePreset("physics")

    UI.Text(string.format("Enabled / active spring constraints: %d / %d",
        Physics.GetEnabledConstraintCount(), Physics.GetActiveConstraintCount()))
    UI.Text(string.format("Average spring length / extension: %.3f m / %.3f m",
        physicsSpringAverageLength, physicsSpringAverageExtension))
    UI.Text(string.format("Combined signed spring force / peak corner: %.1f N / %.1f N",
        physicsSpringTotalForce, physicsSpringMaximumAbsForce))

    physicsSpringsEnabled, changed = UI.Checkbox(
        "Enable four chassis springs", physicsSpringsEnabled)
    if changed then
        ApplySpringProperties()
        physicsMessage = physicsSpringsEnabled
            and "Enabled the four suspension springs"
            or "Disabled the springs; the chassis will fall onto the floor"
    end

    physicsSpringStiffness, changed = UI.SliderFloat(
        "Spring stiffness per corner", physicsSpringStiffness, 500.0, 12000.0, "%.0f N/m")
    if changed then
        ApplySpringProperties()
        physicsMessage = "Changed spring stiffness on all four corners"
    end

    physicsSpringDamping, changed = UI.SliderFloat(
        "Damper rate per corner", physicsSpringDamping, 0.0, 1800.0, "%.0f N s/m")
    if changed then
        ApplySpringProperties()
        physicsMessage = "Changed damping on all four corners"
    end

    if UI.Button("RESET SUSPENDED CHASSIS") then
        ResetPhysicsProbe()
        ApplySpringProperties()
    end

    if UI.Button("BUMP LEFT SIDE - TEST BODY ROLL") then
        if physicsProbeBody ~= 0 and Physics.BodyExists(physicsProbeBody) then
            local px, py, pz = Physics.GetBodyPosition(physicsProbeBody)
            Physics.ApplyBodyImpulseAtPoint(
                physicsProbeBody, 0.0, 950.0, 0.0,
                px - physicsProbeHalfX, py, pz)
            physicsMessage = "Applied an upward road-bump impulse at the left side; the springs should create roll and recovery"
        end
    end
    UI.TextDisabled(physicsMessage)
end
