-- CLEAN08: creator-only suspension hardpoint/travel gizmo presentation.
-- This file owns debug marker entities; it does not own suspension simulation.

local I = SuspensionAuthoringInternal
local SuspensionAuthoringWheelDefinition = I.WheelDefinition
local SuspensionAuthoringPhysicsForWheel = I.PhysicsForWheel
local SuspensionAuthoringAssembly = I.Assembly
local SuspensionAuthoringAdd = I.Add
local SuspensionAuthoringScale = I.Scale
local SuspensionAuthoringNormalize = I.Normalize
local SuspensionAuthoringHardpointPosition = I.HardpointPosition
local SuspensionAuthoringHardpointProvenance = I.HardpointProvenance

local function SuspensionAuthoringDestroyEntity(entity)
    if entity ~= 0 and Entity.Exists(entity) then
        Entity.Destroy(entity)
    end
end

function DestroySuspensionAuthoringGizmos()
    for _, entity in ipairs(vehicleSuspensionAuthoring.gizmoEntities) do
        SuspensionAuthoringDestroyEntity(entity)
    end
    vehicleSuspensionAuthoring.gizmoEntities = {}
end

local function SuspensionAuthoringCreateMarker(name, position, scale, color)
    if playerEntity == 0 or not Entity.Exists(playerEntity) then
        return 0
    end

    local entity = Entity.Create(name)
    if entity == 0 then
        return 0
    end
    Entity.SetParent(entity, playerEntity, false)
    Entity.SetLocalPosition(entity, position[1], position[2], position[3])
    Entity.SetLocalScale(entity, scale, scale, scale)
    Entity.SetDebugPrimitive(entity, "sphere", color[1], color[2], color[3])
    table.insert(vehicleSuspensionAuthoring.gizmoEntities, entity)
    return entity
end

local function SuspensionAuthoringHardpointColor(value)
    local provenance = SuspensionAuthoringHardpointProvenance(value)
    if provenance == "measured" then return { 0.20, 1.00, 0.35 } end
    if provenance == "asset_authored" then return { 0.95, 0.28, 0.92 } end
    if provenance == "estimated" then return { 1.00, 0.65, 0.12 } end
    return { 0.75, 0.75, 0.75 }
end

local function SuspensionAuthoringCreateWheelGizmos(index)
    local wheel = SuspensionAuthoringWheelDefinition(index)
    if wheel == nil then
        return
    end

    local physics = SuspensionAuthoringPhysicsForWheel(wheel)
    local direction = SuspensionAuthoringNormalize(physics.suspensionDirection)
    local axis = SuspensionAuthoringNormalize(physics.steeringAxis)
    local mount = wheel.mount
    local restCenter = SuspensionAuthoringAdd(
        mount, SuspensionAuthoringScale(direction, physics.restLengthM))
    local bumpCenter = SuspensionAuthoringAdd(
        mount,
        SuspensionAuthoringScale(
            direction, physics.restLengthM - physics.maximumCompressionM))
    local droopCenter = SuspensionAuthoringAdd(
        mount,
        SuspensionAuthoringScale(
            direction, physics.restLengthM + physics.maximumDroopM))

    local base = vehicleSuspensionAuthoring.markerScale
    SuspensionAuthoringCreateMarker(
        "SUS03 Wheel Centre " .. tostring(index),
        restCenter, base * 1.20, { 0.10, 0.95, 0.95 })
    SuspensionAuthoringCreateMarker(
        "SUS03 Bump Limit " .. tostring(index),
        bumpCenter, base, { 1.00, 0.32, 0.16 })
    SuspensionAuthoringCreateMarker(
        "SUS03 Droop Limit " .. tostring(index),
        droopCenter, base, { 0.25, 0.48, 1.00 })

    local axisHalfLength = 0.32
    SuspensionAuthoringCreateMarker(
        "SUS03 Steering Axis Top " .. tostring(index),
        SuspensionAuthoringAdd(restCenter, SuspensionAuthoringScale(axis, axisHalfLength)),
        base * 0.82, { 1.00, 0.85, 0.12 })
    SuspensionAuthoringCreateMarker(
        "SUS03 Steering Axis Bottom " .. tostring(index),
        SuspensionAuthoringAdd(restCenter, SuspensionAuthoringScale(axis, -axisHalfLength)),
        base * 0.82, { 1.00, 0.85, 0.12 })

    local assembly = SuspensionAuthoringAssembly(wheel)
    local hardpoints = assembly and assembly.hardpointsByCorner
        and assembly.hardpointsByCorner[wheel.name] or nil
    if hardpoints then
        for hardpointId, value in pairs(hardpoints) do
            local position = SuspensionAuthoringHardpointPosition(value)
            if position ~= nil then
                SuspensionAuthoringCreateMarker(
                    "SUS03 " .. tostring(hardpointId) .. " " .. tostring(index),
                    position, base * 0.90,
                    SuspensionAuthoringHardpointColor(value))
            end
        end
    end
end

function RefreshSuspensionAuthoringGizmos()
    DestroySuspensionAuthoringGizmos()
    if not vehicleSuspensionAuthoring.enabled then
        vehicleSuspensionAuthoring.message = "Suspension authoring gizmos are hidden"
        return true
    end
    if playerEntity == 0 or not Entity.Exists(playerEntity) then
        vehicleSuspensionAuthoring.message = "Player Vehicle Root is not available"
        return false
    end

    if vehicleSuspensionAuthoring.selectedWheelOnly then
        SuspensionAuthoringCreateWheelGizmos(vehicleSuspension.selectedWheel)
    else
        for index = 1, #PrototypeCarDefinition.wheels do
            SuspensionAuthoringCreateWheelGizmos(index)
        end
    end

    vehicleSuspensionAuthoring.message = string.format(
        "Showing %d suspension authoring markers",
        #vehicleSuspensionAuthoring.gizmoEntities)
    return true
end
