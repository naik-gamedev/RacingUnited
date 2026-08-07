-- Step 29J.1 articulated wheel presentation.
-- Rendered wheel entities consume authoritative native wheel-center telemetry.
-- Rotation never changes wheel position: every mesh is translated to its own
-- native center, then steered/spun around that entity origin.

local function VehicleWheelEntities()
    return {
        wheelFrontLeft,
        wheelFrontRight,
        wheelRearLeft,
        wheelRearRight
    }
end

local function RestoreProxyWheelScale(entity)
    Entity.SetLocalScale(entity, 0.42, 0.80, 0.80)
end

local function ApplyArticulatedWheelScale(entity)
    Entity.SetLocalScale(
        entity,
        vehicleWheelVisual.widthScale,
        vehicleWheelVisual.radiusScale,
        vehicleWheelVisual.radiusScale)
end

function RefreshVehicleWheelVisibility()
    local debugSceneVisible = prototypeScenePreset == "vehicle"
        or prototypeScenePreset == "surface"
        or prototypeScenePreset == "entity"
        or prototypeScenePreset == "all"
    local showProxy = debugSceneVisible
        and not vehicleVisual.hideProxyWheels
        and not vehicleWheelVisual.enabled

    for _, entity in ipairs(VehicleWheelEntities()) do
        if entity ~= 0 and Entity.Exists(entity) then
            if Entity.HasDebugPrimitive(entity) then
                Entity.SetDebugVisible(entity, showProxy)
            end
            if Entity.HasMesh(entity) then
                Entity.SetMeshVisible(entity, vehicleWheelVisual.enabled)
            end
        end
    end
end

function ApplyVehicleWheelMeshes()
    local wheelEntities = VehicleWheelEntities()
    local applied = 0

    for index, entity in ipairs(wheelEntities) do
        if entity ~= 0 and Entity.Exists(entity) then
            local assetPath = vehicleWheelVisual.assetPaths[index]
            if assetPath ~= nil and assetPath ~= "" then
                local ok = Entity.SetMesh(
                    entity,
                    assetPath,
                    vehicleWheelVisual.color[1],
                    vehicleWheelVisual.color[2],
                    vehicleWheelVisual.color[3],
                    vehicleWheelVisual.normalize,
                    vehicleWheelVisual.doubleSided,
                    true)
                if ok then
                    applied = applied + 1
                else
                    vehicleWheelVisualMessage = "WHEEL VISUAL ERROR: "
                        .. Entity.GetLastError()
                end
            end
        end
    end

    RefreshVehicleWheelVisibility()
    if applied == #wheelEntities then
        vehicleWheelVisualMessage = vehicleWheelVisual.enabled
            and "Articulated wheel meshes are active at native wheel centers"
            or "Articulated wheel meshes are loaded but hidden"
        return true
    end
    return false
end

function SetArticulatedWheelVisualsEnabled(enabled)
    vehicleWheelVisual.enabled = enabled
    local wheelEntities = VehicleWheelEntities()
    for _, entity in ipairs(wheelEntities) do
        if entity ~= 0 and Entity.Exists(entity) then
            if enabled then
                ApplyArticulatedWheelScale(entity)
            else
                RestoreProxyWheelScale(entity)
            end
        end
    end
    RefreshVehicleWheelVisibility()
    vehicleWheelVisualMessage = enabled
        and "Articulated wheels enabled: exact native centers + per-side orientation"
        or "Articulated wheels hidden: whole-car OBJ mode"
end

function ResetVehicleWheelVisualTuning()
    local definition = PrototypeCarDefinition.visual.articulatedWheels
    vehicleWheelVisual.enabled = definition.enabled
    vehicleWheelVisual.normalize = definition.normalize
    vehicleWheelVisual.doubleSided = definition.doubleSided
    vehicleWheelVisual.color[1] = definition.color[1]
    vehicleWheelVisual.color[2] = definition.color[2]
    vehicleWheelVisual.color[3] = definition.color[3]
    vehicleWheelVisual.radiusScale = definition.radiusScale
    vehicleWheelVisual.widthScale = definition.widthScale
    vehicleWheelVisual.rotationOffsetDegrees[1] = definition.rotationOffsetDegrees[1]
    vehicleWheelVisual.rotationOffsetDegrees[2] = definition.rotationOffsetDegrees[2]
    vehicleWheelVisual.rotationOffsetDegrees[3] = definition.rotationOffsetDegrees[3]
    for index, wheel in ipairs(PrototypeCarDefinition.wheels) do
        vehicleWheelVisual.assetPaths[index] = wheel.visualAsset
            or definition.defaultAsset
    end
    ApplyVehicleWheelMeshes()
    SetArticulatedWheelVisualsEnabled(vehicleWheelVisual.enabled)
    vehicleWheelVisualMessage = "Reset wheel presentation to exact reference geometry"
end

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

            local faceYaw = wheel.visualFaceYawDegrees or 0.0
            local spinSign = wheel.visualSpinSign or 1.0
            -- Visual steering is explicitly gated by the authored wheel role.
            -- Rear/non-steering wheel meshes must remain aligned with the
            -- chassis even if telemetry ever contains a tiny stale/non-zero
            -- steering value during reset or hot reload.
            local visualSteerAngle = 0.0
            if math.abs(wheel.steerFactor or 0.0) > 0.0001 then
                visualSteerAngle = telemetry.steerAngle or 0.0
            end
            Entity.SetLocalRotation(
                entity,
                telemetry.rotationDegrees * spinSign
                    + vehicleWheelVisual.rotationOffsetDegrees[1],
                visualSteerAngle + faceYaw
                    + vehicleWheelVisual.rotationOffsetDegrees[2],
                vehicleWheelVisual.rotationOffsetDegrees[3])
        end
    end
end
