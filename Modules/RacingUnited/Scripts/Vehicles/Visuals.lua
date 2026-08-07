-- Vehicle visual presentation for the current player vehicle.
-- Physics remains native and independent: these functions only change the mesh
-- attached to Player Chassis plus its visual transform/debug presentation.

local function CopyVisualDefinitionToRuntime()
    local visual = PrototypeCarDefinition.visual
    vehicleVisual.assetPath = visual.bodyAsset
    vehicleVisual.fallbackAssetPath = visual.fallbackBodyAsset
    vehicleVisual.normalize = visual.normalize
    vehicleVisual.doubleSided = visual.doubleSided
    vehicleVisual.color[1] = visual.color[1]
    vehicleVisual.color[2] = visual.color[2]
    vehicleVisual.color[3] = visual.color[3]
    vehicleVisual.offset[1] = visual.offset[1]
    vehicleVisual.offset[2] = visual.offset[2]
    vehicleVisual.offset[3] = visual.offset[3]
    vehicleVisual.rotationDegrees[1] = visual.rotationDegrees[1]
    vehicleVisual.rotationDegrees[2] = visual.rotationDegrees[2]
    vehicleVisual.rotationDegrees[3] = visual.rotationDegrees[3]
    vehicleVisual.scale = visual.scale
    vehicleVisual.hideProxyWheels = visual.hideProxyWheels
    vehicleVisual.usingFallback = false
end

function ApplyVehicleVisualTransform()
    if chassisEntity == 0 or not Entity.Exists(chassisEntity) then
        vehicleVisualMessage = "VISUAL ERROR: Player Chassis is missing"
        return false
    end

    Entity.SetLocalPosition(
        chassisEntity,
        vehicleVisual.offset[1],
        vehicleVisual.offset[2],
        vehicleVisual.offset[3])
    Entity.SetLocalRotation(
        chassisEntity,
        vehicleVisual.rotationDegrees[1],
        vehicleVisual.rotationDegrees[2],
        vehicleVisual.rotationDegrees[3])
    Entity.SetLocalScale(
        chassisEntity,
        vehicleVisual.scale,
        vehicleVisual.scale,
        vehicleVisual.scale)
    return true
end

function ApplyVehicleVisualMesh()
    if chassisEntity == 0 or not Entity.Exists(chassisEntity) then
        vehicleVisualMessage = "VISUAL ERROR: Player Chassis is missing"
        return false
    end

    if not Entity.SetMesh(
        chassisEntity,
        vehicleVisual.assetPath,
        vehicleVisual.color[1],
        vehicleVisual.color[2],
        vehicleVisual.color[3],
        vehicleVisual.normalize,
        vehicleVisual.doubleSided,
        not vehicleVisual.usingFallback) then
        vehicleVisualMessage = "VISUAL ERROR: " .. Entity.GetLastError()
        return false
    end

    Entity.SetMeshVisible(chassisEntity, true)
    ApplyVehicleVisualTransform()
    SetVehicleDebugVisible(prototypeScenePreset ~= "visual")
    vehicleVisualMessage = "Using vehicle OBJ: " .. vehicleVisual.assetPath
    return true
end

function UsePlayerVehicleVisual()
    vehicleVisual.assetPath = PrototypeCarDefinition.visual.bodyAsset
    vehicleVisual.usingFallback = false
    return ApplyVehicleVisualMesh()
end

function UseFallbackVehicleVisual()
    vehicleVisual.assetPath = PrototypeCarDefinition.visual.fallbackBodyAsset
    vehicleVisual.usingFallback = true
    return ApplyVehicleVisualMesh()
end

function ResetVehicleVisualTuning()
    CopyVisualDefinitionToRuntime()
    ApplyVehicleVisualMesh()
    vehicleVisualMessage = "Reset player-car visual transform to the vehicle definition"
end

function SetVehicleProxyWheelPreference(hideProxyWheels)
    vehicleVisual.hideProxyWheels = hideProxyWheels
    RefreshVehicleWheelVisibility()
end

function VehicleVisualOnPrototypeEnter()
    CopyVisualDefinitionToRuntime()
    ApplyVehicleVisualMesh()
    ApplyVehicleWheelMeshes()
    SetArticulatedWheelVisualsEnabled(vehicleWheelVisual.enabled)
end
