-- Ray, overlap, sphere-cast and continuous-collision diagnostics.
function DrawPhysicsQueriesPanel()
    local changed = false
    SetPrototypeScenePreset("physics")

    UI.Text(string.format("Live %.2f m sphere cast hit / distance: %s / %.3f m",
        physicsCcdRadius, tostring(physicsSphereCastHit), physicsSphereCastDistance))
    if physicsSphereCastHit then
        UI.Text(string.format("Sphere cast point / normal: %.3f, %.3f, %.3f / %.3f, %.3f, %.3f",
            physicsSphereCastPointX, physicsSphereCastPointY, physicsSphereCastPointZ,
            physicsSphereCastNormalX, physicsSphereCastNormalY, physicsSphereCastNormalZ))
    end
    UI.Text(string.format("Sphere cast candidates / exact tests: %d / %d",
        physicsSphereCastCandidateCount, physicsSphereCastExactTestCount))

    UI.Text(string.format("CCD bodies / sweeps / hits / clamped: %d / %d / %d / %d",
        Physics.GetContinuousCollisionBodyCount(),
        Physics.GetContinuousCollisionSweepCount(),
        Physics.GetContinuousCollisionHitCount(),
        Physics.GetContinuousCollisionClampedBodyCount()))
    UI.Text("CCD unsupported enabled bodies: "
        .. tostring(Physics.GetContinuousCollisionUnsupportedBodyCount()))

    local ccdProjectileX = nil
    if physicsCcdProjectileBody ~= 0 and Physics.BodyExists(physicsCcdProjectileBody) then
        ccdProjectileX = Physics.GetBodyPosition(physicsCcdProjectileBody)
    end
    UI.Text(string.format("Fast sphere position X: %.3f m", ccdProjectileX or physicsCcdStartX))
    UI.TextWrapped("Fast sphere result: " .. physicsCcdOutcome)

    physicsCcdEnabled, changed = UI.Checkbox(
        "Fast sphere continuous collision enabled", physicsCcdEnabled)
    if changed and physicsCcdProjectileBody ~= 0 and Physics.BodyExists(physicsCcdProjectileBody) then
        Physics.SetBodyContinuousCollision(physicsCcdProjectileBody, physicsCcdEnabled)
        physicsMessage = physicsCcdEnabled
            and "Enabled swept-sphere continuous collision protection"
            or "Disabled protection; the 600 m/s sphere should tunnel through the 6 cm wall"
    end
    if UI.Button("LAUNCH 600 M/S FAST SPHERE") then
        if ResetFastCcdSphere(true) then
            physicsMessage = physicsCcdEnabled
                and "Launched with CCD: the sphere should stop at the thin wall"
                or "Launched without CCD: discrete collision will probably miss the thin wall"
        end
    end
    if UI.Button("RESET FAST SPHERE") then
        ResetFastCcdSphere(false)
        physicsMessage = "Reset the fast sphere at its launch position"
    end

    UI.Spacing()
    UI.Separator()
    UI.Spacing()
    UI.Text("Live downward ray hit: " .. tostring(physicsRayHit))
    if physicsRayHit then
        UI.Text(string.format("Ray body / collider / distance: %s / %s / %.3f m",
            tostring(physicsRayBody), tostring(physicsRayCollider), physicsRayDistance))
        UI.Text(string.format("Ray hit point: %.3f, %.3f, %.3f",
            physicsRayPointX, physicsRayPointY, physicsRayPointZ))
        UI.Text(string.format("Ray hit normal: %.3f, %.3f, %.3f",
            physicsRayNormalX, physicsRayNormalY, physicsRayNormalZ))
        UI.Text("Ray hit trigger: " .. tostring(physicsRayTrigger))
    end
    UI.Text(string.format("Ray candidates / exact tests: %d / %d",
        physicsRayCandidateCount, physicsRayExactTestCount))
    UI.Text(string.format("0.85 m overlap around probe: %d", physicsOverlapCount))
    UI.Text(string.format("Overlap candidates / exact tests: %d / %d",
        physicsOverlapCandidateCount, physicsOverlapExactTestCount))

    physicsRayIgnoreProbe, changed = UI.Checkbox(
        "Raycast ignores orange probe", physicsRayIgnoreProbe)
    if changed then
        physicsMessage = physicsRayIgnoreProbe
            and "Ray filter now ignores the orange probe and should hit the floor"
            or "Ray filter now reports the closest collider, including the orange probe"
    end
    UI.TextDisabled(physicsMessage)
end
