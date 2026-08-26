-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
local function EnterDefaultPlayerWorld()
    -- Asset discovery is intentionally lazy/failure-isolated. Keep the lab alive
    -- for the first moment after scene entry; OnUpdate will auto-load the newest
    -- Scene_*.glb as soon as the first Assets index is available.
    playerWorld.autoLoadPending = true
    playerWorld.loaded = false
    playerWorld.message = "Waiting for Scene_*.glb asset discovery..."
    SetPrototypeScenePreset("vehicle")
end

function OnStart()
    Engine.Log("Racing United Lua runtime started")

    -- VEG01: module reloads restart the vegetation registry cleanly. With no
    -- registered species/instances this has effectively zero runtime cost.
    if Vegetation.IsAvailable() then
        Vegetation.Reset()
    end

    -- On a Lua hot reload the active .hscene remains alive. Recover its safe
    -- handles by name instead of creating duplicate entities in script code.
    if Scene.GetCurrent() == "prototype" then
        Physics.UnloadStaticBoxScene()
        RemoveEntitiesByName("Player Scene Visual")
        RefreshSceneEntityHandles()
        CreatePhysicsDemo()
        VehicleOnPrototypeEnter()
        EnterDefaultPlayerWorld()
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

    -- The manifest also declares entry_scene = prototype. This fallback makes
    -- the script robust if a creator later removes that manifest line.
    if Scene.GetCurrent() == "" then
        LoadScene("prototype")
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
        EnterDefaultPlayerWorld()
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

    PhysicsDemoFixedUpdate(fixedDeltaTime)
end

function OnUpdate(deltaTime)
    VehicleUpdate(deltaTime)

    if Scene.GetCurrent() == "prototype" then
        RefreshVehicleAssetDiscovery(false, true)
        RefreshPlayerWorldAssetDiscovery(false)
        if playerWorld.autoLoadPending
            and Module.GetAssetIndexRevision() > 0
            and playerWorld.sceneAsset ~= "" then
            playerWorld.autoLoadPending = false
            if not LoadPlayerWorld() then
                SetPrototypeScenePreset("vehicle")
            end
        end
    end

    if Scene.GetCurrent() == "prototype" and Input.Pressed("Toggle 3D View") then
        showPrototypeControls = not showPrototypeControls
    end

    -- CAM10: a visible module control panel temporarily borrows the mouse from
    -- free-flight navigation. Hiding it gives the pointer straight back to the
    -- fly camera without changing/destroying the detached camera itself.
    if Camera.IsAvailable() then
        Camera.SetUiInteractionActive(
            Scene.GetCurrent() == "prototype" and showPrototypeControls)
    end

    if Input.Pressed("Confirm") then
        PlayUiConfirmation()
        inputMessage = "Confirm action pressed"
    end

    if Input.Pressed("Horn") then
        inputMessage = "Horn action pressed through " .. Input.GetBinding("Horn")
    end
end
