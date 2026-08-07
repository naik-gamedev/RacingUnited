-- Step 29J.4 creator-owned driveable scene bridge with triangle drive-surface queries.
-- Visual geometry is imported from Blender coordinates at authored 1:1 scale.
-- The temporary OBJ bridge now uses exact authored triangles for suspension/tire
-- ground queries, so a real Blender terrain no longer becomes one giant AABB.

playerWorld = {
    visualEntity = 0,
    visualAsset = "Scenes/Player/PlayerScene.obj",
    collisionAsset = "Scenes/Player/PlayerScene_Collision.obj",
    loaded = false,
    collisionTriangleCount = 0,
    spawnPosition = { 0.0, 0.05, 0.0 },
    spawnMode = "origin-fallback",
    message = "Player Scene has not been loaded in this process."
}

local function DestroyPlayerWorldVisual()
    if playerWorld.visualEntity ~= 0 and Entity.Exists(playerWorld.visualEntity) then
        Entity.Destroy(playerWorld.visualEntity)
    end
    playerWorld.visualEntity = 0
end

function DestroyPlayerWorld()
    Physics.UnloadStaticBoxScene()
    Physics.UnloadStaticTriangleScene()
    DestroyPlayerWorldVisual()
    playerWorld.loaded = false
    playerWorld.collisionTriangleCount = 0
end

function ResetVehicleAtPlayerWorldSpawn(message)
    local position = playerWorld.spawnPosition
    return ResetNativeVehicleAt(
        position[1], position[2], position[3],
        message or "Reset vehicle at Player Scene spawn")
end

local function CreatePlayerWorldVisual()
    DestroyPlayerWorldVisual()

    local entity = Entity.Create("Player Scene Visual")
    if entity == 0 then
        playerWorld.message = "PLAYER SCENE ERROR: could not create visual entity"
        return false
    end

    Entity.AddTag(entity, "PlayerWorld")
    Entity.AddTag(entity, "CreatorAuthored")
    Entity.SetLocalPosition(entity, 0.0, 0.0, 0.0)
    Entity.SetLocalRotation(entity, 0.0, 0.0, 0.0)
    Entity.SetLocalScale(entity, 1.0, 1.0, 1.0)

    -- Eighth parameter = Blender-coordinate import. Creator geometry remains
    -- X left/right, Y forward/backward, Z height in its authoring file.
    if not Entity.SetMesh(
        entity,
        playerWorld.visualAsset,
        0.38, 0.42, 0.47,
        false,
        true,
        true) then
        playerWorld.message = "PLAYER SCENE ERROR: " .. Entity.GetLastError()
        Entity.Destroy(entity)
        return false
    end

    playerWorld.visualEntity = entity
    return true
end

function LoadPlayerWorld()
    if Scene.GetCurrent() ~= "prototype" then
        playerWorld.message = "PLAYER SCENE ERROR: open the prototype scene first"
        return false
    end

    -- The laboratory owns its own large floor/surface runway and physics probes.
    -- Remove those while the player world is active so invisible demo colliders
    -- cannot interfere with creator-authored scene collision.
    DestroyPhysicsDemo()
    Physics.UnloadStaticBoxScene()
    Physics.UnloadStaticTriangleScene()

    if not CreatePlayerWorldVisual() then
        CreatePhysicsDemo()
        return false
    end

    local count, spawnX, spawnGroundY, spawnZ, spawnMode =
        Physics.LoadStaticTriangleScene(
            playerWorld.collisionAsset,
            playerWorld.visualAsset,
            true)
    if count == nil or count < 0 then
        playerWorld.message = "PLAYER SCENE COLLISION ERROR: " .. Physics.GetLastError()
        DestroyPlayerWorldVisual()
        CreatePhysicsDemo()
        return false
    end

    playerWorld.loaded = true
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

    SetPrototypeScenePreset("player_world")
    ResetVehicleAtPlayerWorldSpawn("Reset vehicle at Player Scene spawn")

    playerWorld.message = string.format(
        "Player Scene loaded at authored 1:1 scale with %d drive-surface triangles; spawn=%s",
        count, playerWorld.spawnMode)
    return true
end

function ReloadPlayerWorld()
    return LoadPlayerWorld()
end

function ReturnToPrototypeLab()
    DestroyPlayerWorld()
    CreatePhysicsDemo()
    SetPrototypeScenePreset("vehicle")
    ResetNativeVehicle()
    playerWorld.message = "Returned to the prototype laboratory"
    return true
end
