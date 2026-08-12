-- CLEAN08: high-level articulated/embedded wheel presentation coordinator.
-- Detailed transform math, separate-wheel presentation and embedded GLB binding
-- are owned by sibling files under Vehicles/Visual/.

local Articulated = VehicleArticulatedWheelInternal
local Embedded = VehicleEmbeddedWheelInternal
local VehicleWheelEntities = Articulated.Entities
local VehicleWheelVisualRotation = Articulated.VisualRotation
local RestoreProxyWheelScale = Articulated.RestoreProxyScale
local ApplyArticulatedWheelScale = Articulated.ApplyScale
local UpdateEmbeddedVehicleWheelNodes = Embedded.UpdateNodes

function UpdateVehicleWheelPresentation()
    local wheelEntities = VehicleWheelEntities()

    for index, entity in ipairs(wheelEntities) do
        local telemetry = vehicleWheelTelemetry[index]
        local wheel = PrototypeCarDefinition.wheels[index]
        if entity ~= 0
            and Entity.Exists(entity)
            and telemetry ~= nil
            and wheel ~= nil then

            -- Step 29J.1: do not reconstruct a visual wheel center from mount
            -- math in Lua. Use the center already solved by native suspension.
            Entity.SetWorldPosition(
                entity, telemetry.centerX, telemetry.centerY, telemetry.centerZ)

            if vehicleWheelVisual.enabled then
                ApplyArticulatedWheelScale(entity)
            else
                RestoreProxyWheelScale(entity)
            end

            -- Native geometry owns the complete upright orientation. Compose
            -- module-specific mesh facing, author offset and wheel spin after
            -- that pose without reconstructing steering/camber/toe in Lua.
            local rotationX, rotationY, rotationZ =
                VehicleWheelVisualRotation(telemetry, wheel)
            Entity.SetLocalRotation(
                entity, rotationX, rotationY, rotationZ)
        end
    end

    UpdateEmbeddedVehicleWheelNodes()
end
