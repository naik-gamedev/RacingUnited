-- Creator-owned driveable world loaded from a single GLB scene container.
-- Visible geometry, static collision authoring, spawn metadata and surface
-- metadata can now live together in one Scene_*.glb under Assets/Scenes.

playerWorld = {
    visualEntity = 0,
    sceneAsset = "",
    visualAsset = "",
    collisionAsset = "",
    loadedAsset = "",
    loaded = false,
    collisionTriangleCount = 0,
    usingFallbackCollision = false,
    collisionMode = "none",
    spawnPosition = { 0.0, 0.05, 0.0 },
    spawnGlobalPosition = { 0.0, 0.05, 0.0 },
    spawnMode = "origin-fallback",
    detectedSceneCount = 0,
    latestSceneGlb = "",
    assetIndexRevision = -1,
    autoLoadPending = true,
    message = "Waiting for Scene_*.glb discovery under Assets/Scenes."
}

local function LatestDiscoveredSceneGlb()
    return Module.GetLatestAsset(".glb", "Scenes", "Scene_")
end

function RefreshPlayerWorldAssetDiscovery(forceRefresh)
    if forceRefresh then
        Module.RefreshAssetIndex()
    end

    local revision = Module.GetAssetIndexRevision()
    if not forceRefresh and revision == playerWorld.assetIndexRevision then
        return false
    end

    playerWorld.assetIndexRevision = revision
    playerWorld.detectedSceneCount = Module.GetAssetCount(
        ".glb", "Scenes", "Scene_")

    local latest = LatestDiscoveredSceneGlb()
    playerWorld.latestSceneGlb = latest or ""
    if latest == nil or latest == "" then
        playerWorld.message = "No Scene_*.glb detected under Assets/Scenes"
        return false
    end

    if playerWorld.sceneAsset == "" or not playerWorld.loaded then
        playerWorld.sceneAsset = tostring(latest)
        playerWorld.visualAsset = playerWorld.sceneAsset
        playerWorld.collisionAsset = playerWorld.sceneAsset
    end

    if playerWorld.loaded and playerWorld.loadedAsset ~= tostring(latest) then
        playerWorld.message = "New scene GLB detected: " .. tostring(latest)
            .. " (press LOAD / RELOAD to switch)"
    else
        playerWorld.message = "Detected scene GLB: " .. tostring(latest)
    end
    return true
end

local function DestroyPlayerWorldVisual()
    if playerWorld.visualEntity ~= 0 and Entity.Exists(playerWorld.visualEntity) then
        Entity.Destroy(playerWorld.visualEntity)
    end
    playerWorld.visualEntity = 0
end

local function DestroyPlayerWorldFallbackSurface()
    if physicsFloorBody ~= 0 or physicsFloorEntity ~= 0 then
        DestroyBodyAndEntity(physicsFloorBody, physicsFloorEntity)
    end
    physicsFloorEntity = 0
    physicsFloorBody = 0
    physicsFloorCollider = 0
end

local function CreateHiddenFallbackDriveSurface()
    DestroyPlayerWorldFallbackSurface()
    if not CreateCollisionFloor() then
        return false
    end
    if physicsFloorEntity ~= 0 and Entity.Exists(physicsFloorEntity) then
        Entity.SetDebugVisible(physicsFloorEntity, false)
    end
    return true
end

function DestroyPlayerWorld()
    Physics.UnloadStaticBoxScene()
    Physics.UnloadStaticTriangleScene()
    DestroyPlayerWorldFallbackSurface()
    DestroyPlayerWorldVisual()
    playerWorld.loaded = false
    playerWorld.loadedAsset = ""
    playerWorld.collisionTriangleCount = 0
    playerWorld.usingFallbackCollision = false
    playerWorld.collisionMode = "none"
end

function ResetVehicleAtPlayerWorldSpawn(message)
    local global = playerWorld.spawnGlobalPosition
    local x, y, z = Physics.GlobalToLocal(global[1], global[2], global[3])
    if x == nil then
        playerWorld.message = "PLAYER WORLD ERROR: spawn is outside the current local origin frame"
        return false
    end
    playerWorld.spawnPosition = { x, y, z }
    return ResetNativeVehicleAt(
        x, y, z,
        message or "Reset vehicle at Player World spawn")
end

local function CreatePlayerWorldVisual()
    DestroyPlayerWorldVisual()

    local entity = Entity.Create("Player Scene Visual")
    if entity == 0 then
        playerWorld.message = "PLAYER WORLD ERROR: could not create visual entity"
        return false
    end

    Entity.AddTag(entity, "PlayerWorld")
    Entity.AddTag(entity, "CreatorAuthored")
    -- Generic engine-facing render contract: this authored world geometry may
    -- receive the hydrology wetness atlas. The renderer does not hard-code a
    -- Racing United-specific entity/tag name.
    Entity.AddTag(entity, "SurfaceWetnessReceiver")
    local originX, originY, originZ = Physics.GetWorldOrigin()
    Entity.SetLocalPosition(
        entity,
        -(originX or 0.0),
        -(originY or 0.0),
        -(originZ or 0.0))
    Entity.SetLocalRotation(entity, 0.0, 0.0, 0.0)
    Entity.SetLocalScale(entity, 1.0, 1.0, 1.0)

    -- Blender's glTF exporter writes GLB in the engine/glTF axis convention;
    -- no legacy OBJ coordinate-conversion flag is required here.
    if not Entity.SetMesh(
        entity,
        playerWorld.visualAsset,
        1.0, 1.0, 1.0,
        false,
        true,
        false) then
        playerWorld.message = "PLAYER WORLD VISUAL ERROR: " .. Entity.GetLastError()
        Entity.Destroy(entity)
        return false
    end

    playerWorld.visualEntity = entity
    return true
end

function LoadPlayerWorld()
    if Scene.GetCurrent() ~= "prototype" then
        playerWorld.message = "PLAYER WORLD ERROR: open the prototype scene first"
        return false
    end

    if playerWorld.sceneAsset == "" then
        RefreshPlayerWorldAssetDiscovery(true)
    end
    if playerWorld.sceneAsset == "" then
        playerWorld.message = "PLAYER WORLD ERROR: no Scene_*.glb exists under Assets/Scenes"
        return false
    end

    playerWorld.visualAsset = playerWorld.sceneAsset
    playerWorld.collisionAsset = playerWorld.sceneAsset

    -- Scene geography is authored with the same GLB. If metadata is absent,
    -- Racing United keeps its module-level Ivarcko Jezero fallback instead of
    -- inventing location data from geometry.
    if Environment ~= nil and Environment.ApplySceneMetadata ~= nil then
        Environment.ApplySceneMetadata(playerWorld.sceneAsset)
    end

    -- Remove laboratory surfaces/probes while the creator world owns the scene.
    DestroyPhysicsDemo()
    Physics.UnloadStaticBoxScene()
    Physics.UnloadStaticTriangleScene()

    if not CreatePlayerWorldVisual() then
        CreatePhysicsDemo()
        return false
    end

    -- One GLB now owns both visuals and authored collision. The importer finds
    -- *_Collision / Collision_* nodes or heritage.role=collision_mesh extras.
    local count, spawnX, spawnGroundY, spawnZ, spawnMode =
        Physics.LoadStaticTriangleScene(
            playerWorld.collisionAsset,
            "",
            false)

    local collisionWarning = ""
    playerWorld.usingFallbackCollision = false
    playerWorld.collisionMode = "authored_triangle_scene"

    if count == nil or count < 0 then
        local collisionError = Physics.GetLastError()
        Physics.UnloadStaticTriangleScene()
        if not CreateHiddenFallbackDriveSurface() then
            playerWorld.message =
                "PLAYER WORLD COLLISION ERROR: " .. collisionError
                .. " | fallback floor failed: " .. Physics.GetLastError()
            DestroyPlayerWorldVisual()
            CreatePhysicsDemo()
            return false
        end

        playerWorld.usingFallbackCollision = true
        playerWorld.collisionMode = "hidden_flat_fallback_floor"
        playerWorld.collisionTriangleCount = 0
        playerWorld.spawnMode = "origin-fallback-floor"
        playerWorld.spawnPosition = {
            0.0,
            PrototypeCarDefinition.resetPosition[2],
            0.0
        }
        collisionWarning =
            " | collision fallback active (author a *_Collision mesh later)"
    else
        DestroyPlayerWorldFallbackSurface()
        playerWorld.collisionTriangleCount = count
        playerWorld.spawnMode = spawnMode or "origin-fallback"
        if spawnX ~= nil and spawnGroundY ~= nil and spawnZ ~= nil then
            playerWorld.spawnPosition = {
                spawnX,
                spawnGroundY + PrototypeCarDefinition.resetPosition[2],
                spawnZ
            }
        else
            playerWorld.spawnPosition = {
                0.0,
                PrototypeCarDefinition.resetPosition[2],
                0.0
            }
        end
    end

    playerWorld.loaded = true
    playerWorld.loadedAsset = playerWorld.sceneAsset
    playerWorld.autoLoadPending = false

    local globalSpawnX, globalSpawnY, globalSpawnZ = Physics.LocalToGlobal(
        playerWorld.spawnPosition[1],
        playerWorld.spawnPosition[2],
        playerWorld.spawnPosition[3])
    playerWorld.spawnGlobalPosition = {
        globalSpawnX, globalSpawnY, globalSpawnZ
    }

    SetPrototypeScenePreset("player_world")
    ResetVehicleAtPlayerWorldSpawn("Reset vehicle at Player World spawn")

    playerWorld.message = string.format(
        "GLB world loaded: %s | collision=%s | triangles=%d | spawn=%s%s",
        playerWorld.sceneAsset,
        playerWorld.collisionMode,
        playerWorld.collisionTriangleCount,
        playerWorld.spawnMode,
        collisionWarning)
    return true
end

function ReloadPlayerWorld()
    return LoadPlayerWorld()
end

function UseLatestDiscoveredSceneGlb()
    RefreshPlayerWorldAssetDiscovery(true)
    if playerWorld.latestSceneGlb == "" then
        return false
    end
    playerWorld.sceneAsset = playerWorld.latestSceneGlb
    playerWorld.visualAsset = playerWorld.sceneAsset
    playerWorld.collisionAsset = playerWorld.sceneAsset
    return LoadPlayerWorld()
end

function ReturnToPrototypeLab()
    DestroyPlayerWorld()
    CreatePhysicsDemo()
    SetPrototypeScenePreset("vehicle")
    ResetNativeVehicle()
    playerWorld.autoLoadPending = false
    playerWorld.message = "Returned to the prototype laboratory"
    return true
end
