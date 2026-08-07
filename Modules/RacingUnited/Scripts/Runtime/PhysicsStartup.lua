-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function CreatePhysicsDemo()
    DestroyPhysicsDemo()
    RemoveExistingPhysicsDemo()
    if not CreateCollisionFloor() then
        return
    end
    if not CreateSurfaceMaterialDemo() then
        return
    end
    ResetPhysicsProbe()
    if not CreateSpringSuspensionDemo() then
        return
    end
    if not CreateFastCcdDemo() then
        return
    end

    physicsRayOriginEntity = Entity.Create("Step 28G Raycast Origin")
    Entity.SetLocalPosition(
        physicsRayOriginEntity,
        physicsRayOriginX, physicsRayOriginY, physicsRayOriginZ)
    Entity.SetLocalScale(physicsRayOriginEntity, 0.14, 0.14, 0.14)
    Entity.SetDebugPrimitive(physicsRayOriginEntity, "sphere", 0.10, 0.85, 1.0)

    physicsRayHitEntity = Entity.Create("Step 28G Raycast Hit")
    Entity.SetLocalScale(physicsRayHitEntity, 0.18, 0.18, 0.18)
    Entity.SetDebugPrimitive(physicsRayHitEntity, "sphere", 0.20, 1.0, 0.35)
    Entity.SetDebugVisible(physicsRayHitEntity, false)
end
