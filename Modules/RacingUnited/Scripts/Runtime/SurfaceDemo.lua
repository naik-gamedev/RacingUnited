-- Step 29F surface-material test geometry.
-- Collider surfaces are world data. Tires read the material and wetness from
-- the exact collider hit by each suspension ray; this file only builds a small
-- diagnostic runway so the behavior can be verified interactively.

surfaceDemoEntities = {}
surfaceDemoBodies = {}
surfaceDemoColliders = {}

surfaceDemoDefinitions = {
    -- Split-grip pad: left tires on dry asphalt, right tires on ice.
    {
        name = "Step 29F Split Grip Left Asphalt",
        x = -3.0, z = 12.0, halfX = 3.0, halfZ = 5.0,
        material = "asphalt", wetness = 0.0,
        color = { 0.20, 0.22, 0.25 }
    },
    {
        name = "Step 29F Split Grip Right Ice",
        x = 3.0, z = 12.0, halfX = 3.0, halfZ = 5.0,
        material = "ice", wetness = 0.0,
        color = { 0.62, 0.84, 1.00 }
    },

    -- Full-width runway bands. Drive in +Z after the split pad.
    {
        name = "Step 29F Wet Asphalt Band",
        x = 0.0, z = 23.0, halfX = 6.0, halfZ = 3.0,
        material = "asphalt", wetness = 1.0,
        color = { 0.11, 0.14, 0.18 }
    },
    {
        name = "Step 29F Gravel Band",
        x = 0.0, z = 29.0, halfX = 6.0, halfZ = 3.0,
        material = "gravel", wetness = 0.0,
        color = { 0.48, 0.43, 0.35 }
    },
    {
        name = "Step 29F Dirt Band",
        x = 0.0, z = 35.5, halfX = 6.0, halfZ = 3.0,
        material = "dirt", wetness = 0.0,
        color = { 0.38, 0.24, 0.14 }
    },
    {
        name = "Step 29F Grass Band",
        x = 0.0, z = 42.0, halfX = 6.0, halfZ = 3.0,
        material = "grass", wetness = 0.0,
        color = { 0.16, 0.42, 0.15 }
    },
    {
        name = "Step 29F Snow Band",
        x = 0.0, z = 48.5, halfX = 6.0, halfZ = 3.0,
        material = "snow", wetness = 0.0,
        color = { 0.90, 0.93, 0.96 }
    },
    {
        name = "Step 29F Ice Band",
        x = 0.0, z = 55.0, halfX = 6.0, halfZ = 3.0,
        material = "ice", wetness = 0.0,
        color = { 0.66, 0.86, 1.00 }
    },

    -- Narrow examples for future kerb/road-paint contact work.
    {
        name = "Step 29F Kerb Strip",
        x = -7.0, z = 35.5, halfX = 0.55, halfZ = 9.0,
        material = "kerb", wetness = 0.0,
        color = { 0.80, 0.20, 0.16 }
    },
    {
        name = "Step 29F Painted Line Strip",
        x = 7.0, z = 35.5, halfX = 0.22, halfZ = 9.0,
        material = "painted_line", wetness = 0.0,
        color = { 0.95, 0.95, 0.95 }
    }
}

function DestroySurfaceMaterialDemo()
    for _, body in ipairs(surfaceDemoBodies) do
        if body ~= 0 and Physics.BodyExists(body) then
            Physics.DestroyBody(body)
        end
    end
    for _, entity in ipairs(surfaceDemoEntities) do
        if entity ~= 0 and Entity.Exists(entity) then
            Entity.Destroy(entity)
        end
    end
    surfaceDemoEntities = {}
    surfaceDemoBodies = {}
    surfaceDemoColliders = {}
end

function RemoveExistingSurfaceMaterialDemo()
    for _, definition in ipairs(surfaceDemoDefinitions) do
        RemoveEntitiesByName(definition.name)
    end
end

function CreateSurfaceMaterialPatch(definition)
    local entity = Entity.Create(definition.name)
    Entity.AddTag(entity, "SurfaceMaterialDemo")
    Entity.SetLocalPosition(entity, definition.x, 0.001, definition.z)
    Entity.SetLocalScale(
        entity,
        definition.halfX * 2.0,
        0.002,
        definition.halfZ * 2.0)
    Entity.SetDebugPrimitive(
        entity,
        "box",
        definition.color[1],
        definition.color[2],
        definition.color[3])

    local body = Physics.CreateBody(entity, "static", 1.0)
    if body == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        Entity.Destroy(entity)
        return false
    end

    local collider = Physics.CreateBoxCollider(
        body,
        definition.halfX, 0.001, definition.halfZ,
        0.0, 0.0, 0.0,
        0.90, 0.02, false)
    if collider == 0 then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        Physics.DestroyBody(body)
        Entity.Destroy(entity)
        return false
    end

    if not Physics.SetColliderSurface(
        collider,
        definition.material,
        definition.wetness) then
        physicsMessage = "PHYSICS ERROR: " .. Physics.GetLastError()
        Physics.DestroyBody(body)
        Entity.Destroy(entity)
        return false
    end

    table.insert(surfaceDemoEntities, entity)
    table.insert(surfaceDemoBodies, body)
    table.insert(surfaceDemoColliders, collider)
    return true
end

function CreateSurfaceMaterialDemo()
    DestroySurfaceMaterialDemo()
    RemoveExistingSurfaceMaterialDemo()

    for _, definition in ipairs(surfaceDemoDefinitions) do
        if not CreateSurfaceMaterialPatch(definition) then
            return false
        end
    end
    return true
end
