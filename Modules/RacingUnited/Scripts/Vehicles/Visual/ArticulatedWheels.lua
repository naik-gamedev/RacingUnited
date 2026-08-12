-- CLEAN08: articulated proxy/separate-wheel presentation ownership.
-- Wheel centers/upright/spin come from native telemetry; this file only presents them.

local Transform = VehicleVisualTransformMath
local WheelQuaternionMultiply = Transform.QuaternionMultiply
local WheelQuaternionFromEulerDegrees = Transform.QuaternionFromEulerDegrees
local WheelEulerDegreesFromQuaternion = Transform.EulerDegreesFromQuaternion

local function VehicleWheelEntities()
    return {
        wheelFrontLeft,
        wheelFrontRight,
        wheelRearLeft,
        wheelRearRight
    }
end


local function VehicleWheelVisualRotation(telemetry, wheel)
    local upright = WheelQuaternionFromEulerDegrees(
        telemetry.uprightRotationX or 0.0,
        telemetry.uprightRotationY or 0.0,
        telemetry.uprightRotationZ or 0.0)
    local face = WheelQuaternionFromEulerDegrees(
        0.0, wheel.visualFaceYawDegrees or 0.0, 0.0)
    local offset = WheelQuaternionFromEulerDegrees(
        vehicleWheelVisual.rotationOffsetDegrees[1],
        vehicleWheelVisual.rotationOffsetDegrees[2],
        vehicleWheelVisual.rotationOffsetDegrees[3])
    local spin = WheelQuaternionFromEulerDegrees(
        telemetry.rotationDegrees * (wheel.visualSpinSign or 1.0),
        0.0,
        0.0)
    return WheelEulerDegreesFromQuaternion(
        WheelQuaternionMultiply(
            WheelQuaternionMultiply(
                WheelQuaternionMultiply(upright, face), offset),
            spin))
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
                Entity.SetMeshVisible(
                    entity,
                    vehicleWheelVisual.enabled and not vehicleEmbeddedWheelBinding.active)
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


VehicleArticulatedWheelInternal = VehicleArticulatedWheelInternal or {}
VehicleArticulatedWheelInternal.Entities = VehicleWheelEntities
VehicleArticulatedWheelInternal.VisualRotation = VehicleWheelVisualRotation
VehicleArticulatedWheelInternal.RestoreProxyScale = RestoreProxyWheelScale
VehicleArticulatedWheelInternal.ApplyScale = ApplyArticulatedWheelScale
