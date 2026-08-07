-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function CreateCollisionFloor()
    physicsFloorEntity = Entity.Create("Step 29B Vehicle Test Ground")
    Entity.AddTag(physicsFloorEntity, "PhysicsFloor")
    Entity.SetLocalPosition(physicsFloorEntity, 0.0, -0.25, 0.0)
    Entity.SetLocalScale(physicsFloorEntity, 200.0, 0.5, 200.0)
    Entity.SetDebugPrimitive(physicsFloorEntity, "box", 0.16, 0.19, 0.24)

    physicsFloorBody = Physics.CreateBody(physicsFloorEntity, "static", 1.0)
    if physicsFloorBody == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        return false
    end

    physicsFloorCollider = Physics.CreateBoxCollider(
        physicsFloorBody,
        100.0, 0.25, 100.0,
        0.0, 0.0, 0.0,
        0.90, 0.35, false)
    if physicsFloorCollider == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        return false
    end
    if not Physics.SetColliderSurface(
        physicsFloorCollider, "asphalt", 0.0) then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        return false
    end
    return true
end

function ResetPhysicsProbe()
    if physicsProbeEntity == 0 or not Entity.Exists(physicsProbeEntity) then
        physicsProbeEntity = Entity.Create("Step 28H Suspended Chassis")
        Entity.AddTag(physicsProbeEntity, "PhysicsProbe")
        Entity.SetLocalScale(
            physicsProbeEntity,
            physicsProbeHalfX * 2.0,
            physicsProbeHalfY * 2.0,
            physicsProbeHalfZ * 2.0)
        Entity.SetDebugPrimitive(physicsProbeEntity, "box", 1.0, 0.42, 0.08)
    end

    physicsProbeBody = Physics.FindBodyByEntity(physicsProbeEntity)
    if physicsProbeBody == 0 or not Physics.BodyExists(physicsProbeBody) then
        physicsProbeBody = Physics.CreateBody(physicsProbeEntity, "dynamic", 1.0)
        if physicsProbeBody == 0 then
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            return false
        end
    end

    if Physics.GetBodyColliderCount(physicsProbeBody) == 0 then
        physicsProbeCollider = Physics.CreateBoxCollider(
            physicsProbeBody,
            physicsProbeHalfX, physicsProbeHalfY, physicsProbeHalfZ,
            0.0, 0.0, 0.0,
            0.78, 0.18, false)
        if physicsProbeCollider == 0 then
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            return false
        end
    end

    Physics.SetBodyMotionType(physicsProbeBody, "dynamic")
    Physics.SetBodyMass(physicsProbeBody, 250.0)
    Physics.SetBodyGravityFactor(physicsProbeBody, 1.0)
    Physics.SetBodyLinearDamping(physicsProbeBody, 0.08)
    Physics.SetBodyAngularDamping(physicsProbeBody, 0.35)
    Physics.SetBodyAllowSleep(physicsProbeBody, true)
    Physics.SetBodyPosition(
        physicsProbeBody,
        physicsProbeStartX,
        physicsProbeStartY,
        physicsProbeStartZ)
    Physics.SetBodyRotation(physicsProbeBody, 0.0, 0.0, 0.0)
    Physics.SetBodyLinearVelocity(physicsProbeBody, 0.0, 0.0, 0.0)
    Physics.SetBodyAngularVelocity(physicsProbeBody, 0.0, 0.0, 0.0)
    Physics.ClearBodyForces(physicsProbeBody)
    physicsMessage = "Reset the 250 kg suspended chassis and woke all four springs"
    return true
end

function ApplySpringProperties()
    local allWorked = true
    for _, constraint in ipairs(physicsSpringConstraints) do
        if constraint ~= 0 and Physics.ConstraintExists(constraint) then
            allWorked = Physics.SetSpringConstraintProperties(
                constraint,
                physicsSpringRestLength,
                physicsSpringStiffness,
                physicsSpringDamping,
                physicsSpringMaximumForce) and allWorked
            Physics.SetConstraintEnabled(constraint, physicsSpringsEnabled)
        end
    end
    return allWorked
end

function CreateSpringSuspensionDemo()
    physicsSpringConstraints = {}
    physicsSpringAnchorEntities = {}

    local anchors = {
        { "Step 28H Spring Anchor FL", -0.72, -0.42 },
        { "Step 28H Spring Anchor FR",  0.72, -0.42 },
        { "Step 28H Spring Anchor RL", -0.72,  0.42 },
        { "Step 28H Spring Anchor RR",  0.72,  0.42 }
    }

    for _, anchor in ipairs(anchors) do
        local name = anchor[1]
        local localX = anchor[2]
        local localZ = anchor[3]
        local worldX = physicsProbeStartX + localX
        local worldZ = physicsProbeStartZ + localZ

        local marker = Entity.Create(name)
        Entity.SetLocalPosition(marker, worldX, physicsSpringAnchorY, worldZ)
        Entity.SetLocalScale(marker, 0.13, 0.13, 0.13)
        Entity.SetDebugPrimitive(marker, "sphere", 0.12, 0.85, 1.0)
        table.insert(physicsSpringAnchorEntities, marker)

        local constraint = Physics.CreateSpringConstraint(
            physicsProbeBody, 0,
            localX, 0.0, localZ,
            worldX, physicsSpringAnchorY, worldZ,
            physicsSpringRestLength,
            physicsSpringStiffness,
            physicsSpringDamping,
            physicsSpringMaximumForce,
            physicsSpringsEnabled)
        if constraint == 0 then
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            return false
        end
        table.insert(physicsSpringConstraints, constraint)
    end

    return true
end

function ResetFastCcdSphere(launchNow)
    if physicsCcdProjectileEntity == 0
        or not Entity.Exists(physicsCcdProjectileEntity) then
        physicsCcdProjectileEntity = Entity.Create("Step 28G Fast Sphere")
        Entity.AddTag(physicsCcdProjectileEntity, "FastCcdProbe")
        Entity.SetLocalScale(
            physicsCcdProjectileEntity,
            physicsCcdRadius * 2.0,
            physicsCcdRadius * 2.0,
            physicsCcdRadius * 2.0)
        Entity.SetDebugPrimitive(
            physicsCcdProjectileEntity,
            "sphere",
            1.0, 0.12, 0.55)
    end

    physicsCcdProjectileBody = Physics.FindBodyByEntity(
        physicsCcdProjectileEntity)
    if physicsCcdProjectileBody == 0
        or not Physics.BodyExists(physicsCcdProjectileBody) then
        physicsCcdProjectileBody = Physics.CreateBody(
            physicsCcdProjectileEntity,
            "dynamic",
            0.25)
        if physicsCcdProjectileBody == 0 then
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            return false
        end
    end

    if Physics.GetBodyColliderCount(physicsCcdProjectileBody) == 0 then
        physicsCcdProjectileCollider = Physics.CreateSphereCollider(
            physicsCcdProjectileBody,
            physicsCcdRadius,
            0.0, 0.0, 0.0,
            0.05, 0.0, false)
        if physicsCcdProjectileCollider == 0 then
            physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
            return false
        end
    end

    Physics.SetBodyMotionType(physicsCcdProjectileBody, "dynamic")
    Physics.SetBodyMass(physicsCcdProjectileBody, 0.25)
    Physics.SetBodyGravityFactor(physicsCcdProjectileBody, 0.0)
    Physics.SetBodyLinearDamping(physicsCcdProjectileBody, 0.0)
    Physics.SetBodyAngularDamping(physicsCcdProjectileBody, 0.0)
    Physics.SetBodyAllowSleep(physicsCcdProjectileBody, false)
    Physics.SetBodyContinuousCollision(
        physicsCcdProjectileBody,
        physicsCcdEnabled)
    Physics.SetBodyPosition(
        physicsCcdProjectileBody,
        physicsCcdStartX,
        physicsCcdStartY,
        physicsCcdStartZ)
    Physics.SetBodyRotation(physicsCcdProjectileBody, 0.0, 0.0, 0.0)
    Physics.SetBodyLinearVelocity(
        physicsCcdProjectileBody,
        launchNow and physicsCcdLaunchSpeed or 0.0,
        0.0,
        0.0)
    Physics.SetBodyAngularVelocity(physicsCcdProjectileBody, 0.0, 0.0, 0.0)
    Physics.ClearBodyForces(physicsCcdProjectileBody)
    physicsCcdLaunched = launchNow
    physicsCcdOutcome = launchNow
        and (physicsCcdEnabled
            and "In flight with continuous collision enabled"
            or "In flight with discrete collision only")
        or "Ready to launch"
    return true
end

function CreateFastCcdDemo()
    physicsCcdWallEntity = Entity.Create("Step 28G Thin CCD Wall")
    Entity.AddTag(physicsCcdWallEntity, "CcdWall")
    Entity.SetLocalPosition(
        physicsCcdWallEntity,
        physicsCcdWallX,
        physicsCcdStartY,
        physicsCcdStartZ)
    Entity.SetLocalScale(physicsCcdWallEntity, 0.06, 2.0, 2.0)
    Entity.SetDebugPrimitive(
        physicsCcdWallEntity,
        "box",
        0.10, 0.78, 0.95)

    physicsCcdWallBody = Physics.CreateBody(
        physicsCcdWallEntity,
        "static",
        1.0)
    if physicsCcdWallBody == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        return false
    end

    physicsCcdWallCollider = Physics.CreateBoxCollider(
        physicsCcdWallBody,
        0.03, 1.0, 1.0,
        0.0, 0.0, 0.0,
        0.05, 0.0, false)
    if physicsCcdWallCollider == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        return false
    end

    physicsSphereCastHitEntity = Entity.Create("Step 28G Sphere Cast Hit")
    Entity.SetLocalScale(physicsSphereCastHitEntity, 0.15, 0.15, 0.15)
    Entity.SetDebugPrimitive(
        physicsSphereCastHitEntity,
        "sphere",
        0.30, 1.0, 0.90)
    Entity.SetDebugVisible(physicsSphereCastHitEntity, false)

    return ResetFastCcdSphere(false)
end
