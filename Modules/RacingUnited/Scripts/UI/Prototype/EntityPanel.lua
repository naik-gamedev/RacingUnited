-- Entity hierarchy and prefab diagnostics.
function DrawPrototypeEntityPanel()
    SetPrototypeScenePreset("entity")
    UI.Text("Entity service available: " .. tostring(Entity.IsAvailable()))
    UI.Text("Prefab service available: " .. tostring(Prefab.IsAvailable()))
    UI.Text("Alive module entities: " .. tostring(Entity.Count()))
    UI.TextWrapped("prototype.hscene now places one reusable .hprefab vehicle hierarchy. The same prefab can also be instantiated again at runtime without duplicating its entity definitions or OBJ asset.")

    if playerEntity ~= 0 and Entity.Exists(playerEntity) then
        local px, py, pz = Entity.GetWorldPosition(playerEntity)
        local rx, ry, rz = Entity.GetWorldRotation(playerEntity)
        UI.TextWrapped("Vehicle root: " .. Entity.GetName(playerEntity))
        UI.Text("Fresh instance ID: " .. tostring(Entity.GetPersistentId(playerEntity)))
        UI.Text("Children attached: " .. tostring(Entity.GetChildCount(playerEntity)))
        UI.Text(string.format("Root world position: %.2f, %.2f, %.2f", px, py, pz))
        UI.Text(string.format("Root world rotation: %.1f, %.1f, %.1f", rx, ry, rz))

        if chassisEntity ~= 0 and Entity.Exists(chassisEntity) and Entity.HasMesh(chassisEntity) then
            local meshPath, cr, cg, cb, meshVisible, meshNormalized, meshDoubleSided = Entity.GetMesh(chassisEntity)
            UI.TextWrapped("Body mesh: " .. tostring(meshPath))
            UI.Text(string.format("Mesh tint: %.2f, %.2f, %.2f", cr, cg, cb))
            UI.Text("Authored scale preserved: " .. tostring(not meshNormalized))
            if UI.Button(meshVisible and "HIDE BODY MESH" or "SHOW BODY MESH") then
                Entity.SetMeshVisible(chassisEntity, not meshVisible)
                entityMessage = meshVisible and "Hid the OBJ Mesh component" or "Restored the OBJ Mesh component"
            end
        end

        if wheelFrontLeft ~= 0 and Entity.Exists(wheelFrontLeft) then
            local lx, ly, lz = Entity.GetLocalPosition(wheelFrontLeft)
            local wx, wy, wz = Entity.GetWorldPosition(wheelFrontLeft)
            UI.TextWrapped("Front-left wheel parent: "
                .. tostring(Entity.GetParent(wheelFrontLeft)))
            UI.Text(string.format("Wheel local: %.2f, %.2f, %.2f", lx, ly, lz))
            UI.Text(string.format("Wheel world: %.2f, %.2f, %.2f", wx, wy, wz))
            UI.Text("Wheel is descendant of root: "
                .. tostring(Entity.IsDescendantOf(wheelFrontLeft, playerEntity)))
        end

        if UI.Button("MOVE VEHICLE ROOT +1 X") then
            Entity.SetWorldPosition(playerEntity, px + 1.0, py, pz)
            entityMessage = "Moved the root; attached wheels and camera inherited the movement"
        end

        if UI.Button("ROTATE VEHICLE ROOT +15 Y") then
            Entity.SetWorldRotation(playerEntity, rx, ry + 15.0, rz)
            entityMessage = "Rotated the root; child world positions followed the hierarchy"
        end

        if wheelFrontLeft ~= 0 and Entity.Exists(wheelFrontLeft) then
            if Entity.GetParent(wheelFrontLeft) == 0 then
                if UI.Button("ATTACH FRONT-LEFT WHEEL - KEEP WORLD") then
                    Entity.SetParent(wheelFrontLeft, playerEntity, true)
                    entityMessage = "Reattached the wheel without making it jump in world space"
                end
            else
                if UI.Button("DETACH FRONT-LEFT WHEEL - KEEP WORLD") then
                    Entity.ClearParent(wheelFrontLeft, true)
                    entityMessage = "Detached the wheel while preserving its world transform"
                end
            end
        end

        if UI.Button("RESTORE HIERARCHY FROM PROTOTYPE.HSCENE") then
            if Scene.Reload() then
                entityMessage = "Queued a clean reload from prototype.hscene"
            else
                entityMessage = "SCENE ERROR: " .. Scene.GetLastError()
            end
        end

        if prefabCloneEntity == 0 or not Entity.Exists(prefabCloneEntity) then
            if UI.Button("SPAWN SECOND PREFAB INSTANCE") then
                local root, count = Prefab.Instantiate(
                    "Vehicles/Step27F_PrototypeVehicle.hprefab",
                    "Runtime Prefab Clone",
                    4.5, 0.5, 0.0,
                    0.0, -25.0, 0.0,
                    1.0, 1.0, 1.0,
                    "Clone / ")
                prefabCloneEntity = root
                if root ~= 0 then
                    entityMessage = "Instantiated a second reusable prefab with "
                        .. tostring(count) .. " entities"
                else
                    entityMessage = "PREFAB ERROR: " .. Prefab.GetLastError()
                end
            end
        else
            if UI.Button("DESTROY SECOND PREFAB INSTANCE") then
                Entity.Destroy(prefabCloneEntity)
                prefabCloneEntity = 0
                entityMessage = "Destroyed the runtime prefab instance as one hierarchy"
            end
        end

        UI.TextDisabled("Hierarchy children:")
        for childIndex = 1, Entity.GetChildCount(playerEntity) do
            local child = Entity.GetChildAt(playerEntity, childIndex)
            if child ~= 0 then
                UI.TextDisabled("  " .. tostring(childIndex) .. ". " .. Entity.GetName(child))
            end
        end
    else
        UI.TextColored("Player entity is missing: " .. Entity.GetLastError(), 1.0, 0.35, 0.35, 1.0)
        if UI.Button("RELOAD PROTOTYPE.HSCENE") then
            if not Scene.Reload() then
                entityMessage = "SCENE ERROR: " .. Scene.GetLastError()
            end
        end
    end

    if playerEntity ~= 0 and Entity.Exists(playerEntity)
        and UI.Button("CREATE TEMPORARY LUA CHILD ENTITY") then
        temporaryEntity = Entity.Create("Temporary Physics Candidate")
        Entity.AddTag(temporaryEntity, "Temporary")
        Entity.SetParent(temporaryEntity, playerEntity, false)
        Entity.SetLocalPosition(temporaryEntity, 0.0, 2.2, 0.0)
        Entity.SetLocalScale(temporaryEntity, 0.45, 0.45, 0.45)
        Entity.SetDebugPrimitive(temporaryEntity, "sphere", 0.95, 0.25, 0.75)
        entityMessage = "Created one runtime-only Lua child beside the scene-owned entities"
    end

    if temporaryEntity ~= 0 and Entity.Exists(temporaryEntity) then
        if UI.Button("DESTROY TEMPORARY CHILD ENTITY") then
            destroyedEntityHandle = temporaryEntity
            Entity.Destroy(temporaryEntity)
            temporaryEntity = 0
            entityMessage = "Destroyed the temporary child and removed it from the parent list"
        end
    end

    if destroyedEntityHandle ~= 0 then
        UI.Text("Destroyed handle still exists: " .. tostring(Entity.Exists(destroyedEntityHandle)))
    end

    UI.TextDisabled(entityMessage)
    UI.TextDisabled("Scene source: Modules/RacingUnited/Scenes/prototype.hscene")
    UI.TextDisabled("Prefab source: Modules/RacingUnited/Prefabs/Vehicles/Step27F_PrototypeVehicle.hprefab")
    UI.TextDisabled("Mesh source: Modules/RacingUnited/Assets/Vehicles/Step27E_LowPolyHatchback.obj")
end
