-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function OnStart()
    Engine.Log("Racing United Lua runtime started")

    -- On a Lua hot reload the active .hscene remains alive. Recover its safe
    -- handles by name instead of creating duplicate entities in script code.
    if Scene.GetCurrent() == "prototype" then
        Physics.UnloadStaticBoxScene()
        RemoveEntitiesByName("Player Scene Visual")
        RefreshSceneEntityHandles()
        CreatePhysicsDemo()
        VehicleOnPrototypeEnter()
        SetPrototypeScenePreset("vehicle")
    else
        ClearSceneEntityHandles()
        physicsProbeEntity = 0
        physicsProbeBody = 0
        physicsProbeCollider = 0
        physicsFloorEntity = 0
        physicsFloorBody = 0
        physicsFloorCollider = 0
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
        ClearVehicleRuntimeHandles()
    end

    -- The manifest also declares entry_scene = main_menu. This fallback makes
    -- the script robust if a creator later removes that manifest line.
    if Scene.GetCurrent() == "" then
        LoadScene("main_menu")
    end
end

function OnSceneEnter(sceneName)
    transitionMessage = "Entered scene: " .. sceneName
    if sceneName == "prototype" then
        Physics.UnloadStaticBoxScene()
        RemoveEntitiesByName("Player Scene Visual")
        RefreshSceneEntityHandles()
        SetVehicleDebugVisible(true)
        CreatePhysicsDemo()
        VehicleOnPrototypeEnter()
        SetPrototypeScenePreset("vehicle")
    else
        VehicleOnPrototypeExit()
        ClearSceneEntityHandles()
        DestroyPhysicsDemo()
    end
    Engine.Log(transitionMessage)
end

function OnSceneExit(sceneName)
    Engine.Log("Leaving scene: " .. sceneName)
    if sceneName == "prototype" then
        DestroyPlayerWorld()
        if temporaryEntity ~= 0 and Entity.Exists(temporaryEntity) then
            Entity.Destroy(temporaryEntity)
        end
        if prefabCloneEntity ~= 0 and Entity.Exists(prefabCloneEntity) then
            Entity.Destroy(prefabCloneEntity)
        end
        temporaryEntity = 0
        prefabCloneEntity = 0
        VehicleOnPrototypeExit()
        DestroyPhysicsDemo()
        ClearSceneEntityHandles()
    end
end

function OnSceneError(sceneName, errorText)
    transitionMessage = "Could not load " .. sceneName .. ": " .. errorText
end

function OnFixedUpdate(fixedDeltaTime)
    if Scene.GetCurrent() ~= "prototype" then
        return
    end

    VehicleFixedUpdate(fixedDeltaTime)

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

function OnUpdate(deltaTime)
    VehicleUpdate(deltaTime)

    if Scene.GetCurrent() == "prototype" and Input.Pressed("Toggle 3D View") then
        showPrototypeControls = not showPrototypeControls
    end

    if Input.Pressed("Confirm") then
        PlayUiConfirmation()
        inputMessage = "Confirm action pressed"
    end

    if Input.Pressed("Horn") then
        inputMessage = "Horn action pressed through " .. Input.GetBinding("Horn")
    end
end
