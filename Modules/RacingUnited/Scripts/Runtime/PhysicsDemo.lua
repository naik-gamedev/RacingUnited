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

function DestroyPhysicsDemo()
    DestroySurfaceMaterialDemo()
    for _, constraint in ipairs(physicsSpringConstraints) do
        if constraint ~= 0 and Physics.ConstraintExists(constraint) then
            Physics.DestroyConstraint(constraint)
        end
    end
    physicsSpringConstraints = {}
    for _, entity in ipairs(physicsSpringAnchorEntities) do
        if entity ~= 0 and Entity.Exists(entity) then
            Entity.Destroy(entity)
        end
    end
    physicsSpringAnchorEntities = {}
    DestroyBodyAndEntity(physicsProbeBody, physicsProbeEntity)
    DestroyBodyAndEntity(physicsFloorBody, physicsFloorEntity)
    DestroyBodyAndEntity(physicsCcdProjectileBody, physicsCcdProjectileEntity)
    DestroyBodyAndEntity(physicsCcdWallBody, physicsCcdWallEntity)
    if physicsRayOriginEntity ~= 0 and Entity.Exists(physicsRayOriginEntity) then
        Entity.Destroy(physicsRayOriginEntity)
    end
    if physicsRayHitEntity ~= 0 and Entity.Exists(physicsRayHitEntity) then
        Entity.Destroy(physicsRayHitEntity)
    end
    if physicsSphereCastHitEntity ~= 0 and Entity.Exists(physicsSphereCastHitEntity) then
        Entity.Destroy(physicsSphereCastHitEntity)
    end
    physicsRayOriginEntity = 0
    physicsRayHitEntity = 0
    physicsSphereCastHitEntity = 0
    physicsCcdProjectileEntity = 0
    physicsCcdProjectileBody = 0
    physicsCcdProjectileCollider = 0
    physicsCcdWallEntity = 0
    physicsCcdWallBody = 0
    physicsCcdWallCollider = 0
    physicsCcdLaunched = false
    physicsProbeEntity = 0
    physicsProbeBody = 0
    physicsProbeCollider = 0
    physicsFloorEntity = 0
    physicsFloorBody = 0
    physicsFloorCollider = 0
end

function RemoveExistingPhysicsDemo()
    RemoveExistingSurfaceMaterialDemo()
    RemoveEntitiesByName("Step 28B Native Body Probe")
    RemoveEntitiesByName("Step 28C Native Collision Probe")
    RemoveEntitiesByName("Step 28C Native Collision Floor")
    RemoveEntitiesByName("Step 28D Angular Box Probe")
    RemoveEntitiesByName("Step 28D OBB Collision Floor")
    RemoveEntitiesByName("Step 28E Sleeping Box Probe")
    RemoveEntitiesByName("Step 28E Sleeping Collision Floor")
    RemoveEntitiesByName("Step 28F Query Box Probe")
    RemoveEntitiesByName("Step 28F Query Collision Floor")
    RemoveEntitiesByName("Step 28F Raycast Origin")
    RemoveEntitiesByName("Step 28F Raycast Hit")
    RemoveEntitiesByName("Step 28G Query Box Probe")
    RemoveEntitiesByName("Step 28G Query Collision Floor")
    RemoveEntitiesByName("Step 28G Raycast Origin")
    RemoveEntitiesByName("Step 28G Raycast Hit")
    RemoveEntitiesByName("Step 28G Fast Sphere")
    RemoveEntitiesByName("Step 28G Thin CCD Wall")
    RemoveEntitiesByName("Step 28G Sphere Cast Hit")
    RemoveEntitiesByName("Step 28H Suspended Chassis")
    RemoveEntitiesByName("Step 28H Suspension Floor")
    RemoveEntitiesByName("Step 28H Spring Anchor FL")
    RemoveEntitiesByName("Step 28H Spring Anchor FR")
    RemoveEntitiesByName("Step 28H Spring Anchor RL")
    RemoveEntitiesByName("Step 28H Spring Anchor RR")
    RemoveEntitiesByName("Step 29A Vehicle Test Ground")
    RemoveEntitiesByName("Step 29B Vehicle Test Ground")
end

-- CLEAN08: fixed-step implementation belongs to the physics demo, not module lifecycle.
function PhysicsDemoFixedUpdate(fixedDeltaTime)
    if physicsProbeBody == 0 or not Physics.BodyExists(physicsProbeBody) then
        return
    end

    -- Native contacts should keep the probe above the floor. This is only a
    -- safety reset for a malformed hot reload or a future collision regression.
    local x, y, z = Physics.GetBodyPosition(physicsProbeBody)
    if y ~= nil and y < -5.0 then
        ResetPhysicsProbe()
        physicsMessage = "Safety-reset a probe that escaped the collision demo"
        return
    end

    local springCount = 0
    local lengthTotal = 0.0
    local extensionTotal = 0.0
    local forceTotal = 0.0
    local maximumAbsForce = 0.0
    for _, constraint in ipairs(physicsSpringConstraints) do
        if constraint ~= 0 and Physics.ConstraintExists(constraint) then
            local springLength, extension, _, force =
                Physics.GetSpringConstraintState(constraint)
            if springLength ~= nil then
                springCount = springCount + 1
                lengthTotal = lengthTotal + springLength
                extensionTotal = extensionTotal + extension
                forceTotal = forceTotal + force
                maximumAbsForce = math.max(maximumAbsForce, math.abs(force))
            end
        end
    end
    if springCount > 0 then
        physicsSpringAverageLength = lengthTotal / springCount
        physicsSpringAverageExtension = extensionTotal / springCount
        physicsSpringTotalForce = forceTotal
        physicsSpringMaximumAbsForce = maximumAbsForce
    end

    local ignoredBody = physicsRayIgnoreProbe and physicsProbeBody or 0
    physicsRayHit, physicsRayCollider, physicsRayBody, physicsRayDistance,
        physicsRayPointX, physicsRayPointY, physicsRayPointZ,
        physicsRayNormalX, physicsRayNormalY, physicsRayNormalZ,
        physicsRayTrigger = Physics.Raycast(
            physicsRayOriginX, physicsRayOriginY, physicsRayOriginZ,
            0.0, -1.0, 0.0,
            20.0, 4294967295, false, ignoredBody)
    physicsRayCandidateCount = Physics.GetLastQueryCandidateCount()
    physicsRayExactTestCount = Physics.GetLastQueryExactTestCount()

    if physicsRayHitEntity ~= 0 and Entity.Exists(physicsRayHitEntity) then
        Entity.SetDebugVisible(
            physicsRayHitEntity,
            prototypeScenePreset == "physics" and physicsRayHit)
        if physicsRayHit then
            Entity.SetLocalPosition(
                physicsRayHitEntity,
                physicsRayPointX, physicsRayPointY, physicsRayPointZ)
        end
    end

    physicsOverlapCount = Physics.OverlapSphereCount(
        x, y, z,
        0.85, 4294967295, false, physicsProbeBody)
    physicsOverlapCandidateCount = Physics.GetLastQueryCandidateCount()
    physicsOverlapExactTestCount = Physics.GetLastQueryExactTestCount()

    physicsSphereCastHit, _, _, physicsSphereCastDistance,
        physicsSphereCastPointX, physicsSphereCastPointY, physicsSphereCastPointZ,
        physicsSphereCastNormalX, physicsSphereCastNormalY, physicsSphereCastNormalZ =
        Physics.SphereCast(
            physicsCcdStartX,
            physicsCcdStartY,
            physicsCcdStartZ,
            physicsCcdRadius,
            1.0, 0.0, 0.0,
            8.0,
            4294967295,
            false,
            physicsCcdProjectileBody)
    physicsSphereCastCandidateCount = Physics.GetLastQueryCandidateCount()
    physicsSphereCastExactTestCount = Physics.GetLastQueryExactTestCount()

    if physicsSphereCastHitEntity ~= 0
        and Entity.Exists(physicsSphereCastHitEntity) then
        Entity.SetDebugVisible(
            physicsSphereCastHitEntity,
            prototypeScenePreset == "physics" and physicsSphereCastHit)
        if physicsSphereCastHit then
            Entity.SetLocalPosition(
                physicsSphereCastHitEntity,
                physicsSphereCastPointX,
                physicsSphereCastPointY,
                physicsSphereCastPointZ)
        end
    end

    if physicsCcdProjectileBody ~= 0
        and Physics.BodyExists(physicsCcdProjectileBody) then
        local projectileX = Physics.GetBodyPosition(physicsCcdProjectileBody)
        if physicsCcdLaunched and projectileX ~= nil then
            if physicsCcdEnabled
                and projectileX > 4.15
                and projectileX < 4.35 then
                physicsCcdLaunched = false
                physicsCcdOutcome =
                    "PROTECTED: stopped at the thin wall instead of tunnelling"
                physicsMessage =
                    "Continuous collision caught the 600 m/s sphere between fixed steps"
            elseif not physicsCcdEnabled and projectileX > 7.5 then
                Physics.SetBodyLinearVelocity(
                    physicsCcdProjectileBody,
                    0.0, 0.0, 0.0)
                physicsCcdLaunched = false
                physicsCcdOutcome =
                    "TUNNELLED: discrete collision missed the 6 cm wall"
                physicsMessage =
                    "Without CCD the fast sphere crossed the thin wall between fixed steps"
            end
        end
    end
end
