-- Prototype-lab visibility presets.
-- These only change debug presentation. Physics colliders and simulation stay alive
-- so switching tabs never changes the state being tested.


local function SetDebugEntityVisible(entity, visible)
    if entity ~= 0 and Entity.Exists(entity) and Entity.HasDebugPrimitive(entity) then
        Entity.SetDebugVisible(entity, visible)
    end
end

function SetSurfaceMaterialDemoVisible(visible)
    for _, entity in ipairs(surfaceDemoEntities) do
        SetDebugEntityVisible(entity, visible)
    end
end

function SetPhysicsDemoVisible(visible)
    SetDebugEntityVisible(physicsProbeEntity, visible)
    SetDebugEntityVisible(physicsCcdProjectileEntity, visible)
    SetDebugEntityVisible(physicsCcdWallEntity, visible)
    SetDebugEntityVisible(physicsRayOriginEntity, visible)

    for _, entity in ipairs(physicsSpringAnchorEntities) do
        SetDebugEntityVisible(entity, visible)
    end

    if not visible then
        SetDebugEntityVisible(physicsRayHitEntity, false)
        SetDebugEntityVisible(physicsSphereCastHitEntity, false)
    else
        SetDebugEntityVisible(physicsRayHitEntity, physicsRayHit)
        SetDebugEntityVisible(physicsSphereCastHitEntity, physicsSphereCastHit)
    end
end

function SetPrototypeScenePreset(preset)
    -- While the creator-authored world is loaded, UI tabs must not replace its
    -- clean presentation with the laboratory's proxy/debug geometry. The
    -- explicit Return to Prototype Lab action unloads the world first.
    if playerWorld ~= nil and playerWorld.loaded and preset ~= "player_world" then
        preset = "player_world"
    end

    if preset == prototypeScenePreset then
        return
    end

    prototypeScenePreset = preset

    if preset == "player_world" then
        SetVehicleDebugVisible(false)
        SetSurfaceMaterialDemoVisible(false)
        SetPhysicsDemoVisible(false)
    elseif preset == "visual" then
        -- Show the authored vehicle mesh without proxy wheels, camera markers,
        -- physics probes or surface-test clutter.
        SetVehicleDebugVisible(false)
        SetSurfaceMaterialDemoVisible(false)
        SetPhysicsDemoVisible(false)
    elseif preset == "surface" then
        SetVehicleDebugVisible(true)
        SetSurfaceMaterialDemoVisible(true)
        SetPhysicsDemoVisible(false)
    elseif preset == "physics" then
        SetVehicleDebugVisible(false)
        SetSurfaceMaterialDemoVisible(false)
        SetPhysicsDemoVisible(true)
    elseif preset == "entity" then
        SetVehicleDebugVisible(true)
        SetSurfaceMaterialDemoVisible(false)
        SetPhysicsDemoVisible(false)
    elseif preset == "all" then
        SetVehicleDebugVisible(true)
        SetSurfaceMaterialDemoVisible(true)
        SetPhysicsDemoVisible(true)
    else
        -- Clean vehicle/default view.
        prototypeScenePreset = "vehicle"
        SetVehicleDebugVisible(true)
        SetSurfaceMaterialDemoVisible(false)
        SetPhysicsDemoVisible(false)
    end
end
