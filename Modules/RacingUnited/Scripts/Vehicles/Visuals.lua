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

local function IsLegacyDefaultVehicleAsset(path)
    return tostring(path or "") == tostring(PrototypeCarDefinition.visual.bodyAsset or "")
end

local function LatestDiscoveredVehicleGlb()
    return Module.GetLatestAsset(".glb", "Vehicles", "Vehicle_")
end

function RefreshVehicleAssetDiscovery(forceRefresh, applyWhenChanged)
    if forceRefresh then
        Module.RefreshAssetIndex()
    end

    local revision = Module.GetAssetIndexRevision()
    if not forceRefresh and revision == vehicleAssetDiscovery.lastRevision then
        return false
    end
    vehicleAssetDiscovery.lastRevision = revision
    vehicleAssetDiscovery.detectedCount = Module.GetAssetCount(
        ".glb", "Vehicles", "Vehicle_")

    local latest = LatestDiscoveredVehicleGlb()
    vehicleAssetDiscovery.latestVehicleGlb = latest or ""
    if latest == nil or latest == "" then
        vehicleAssetDiscovery.message = "No Vehicle_*.glb detected under Assets/Vehicles"
        return false
    end

    vehicleAssetDiscovery.message = "Detected latest vehicle GLB: " .. tostring(latest)

    if vehicleAssetDiscovery.enabled then
        local current = tostring(vehicleVisual.assetPath or "")
        local autoOwnsCurrent = vehicleAssetDiscovery.autoOwnedPath ~= ""
            and current == vehicleAssetDiscovery.autoOwnedPath
        if current == tostring(latest) then
            vehicleAssetDiscovery.autoOwnedPath = tostring(latest)
            RefreshVehicleAssetMetadata()
            return false
        end

        if IsLegacyDefaultVehicleAsset(current) or autoOwnsCurrent then
            vehicleVisual.assetPath = tostring(latest)
            vehicleVisual.usingFallback = false
            vehicleAssetDiscovery.autoOwnedPath = tostring(latest)
            vehicleAssetDiscovery.message = "Auto-loaded discovered vehicle GLB: "
                .. tostring(latest)
            if applyWhenChanged then
                return ApplyVehicleVisualMesh()
            end
            return true
        end
    end

    return false
end

function SetVehicleAssetAutoDiscoveryEnabled(enabled)
    vehicleAssetDiscovery.enabled = enabled
    Save.SetBool("vehicle.visual.auto_discover_vehicle_glb", enabled)
    if enabled then
        RefreshVehicleAssetDiscovery(true, true)
    else
        vehicleAssetDiscovery.message = "Automatic Vehicle_*.glb loading disabled"
    end
end

function UseLatestDiscoveredVehicleGlb()
    local latest = LatestDiscoveredVehicleGlb()
    if latest == nil or latest == "" then
        vehicleVisualMessage = "No Vehicle_*.glb exists under Assets/Vehicles"
        return false
    end
    vehicleVisual.assetPath = tostring(latest)
    vehicleVisual.usingFallback = false
    vehicleAssetDiscovery.autoOwnedPath = tostring(latest)
    return ApplyVehicleVisualMesh()
end

local function VehicleVisualRotationForCurrentAsset()
    local path = string.lower(tostring(vehicleVisual.assetPath or ""))
    if string.match(path, "%.glb$") then
        -- Blender glTF export already converts the Racing United authoring
        -- convention (X=width, Y=length, Z=height, nose toward -Y) into
        -- glTF/Heritage coordinates. Do not apply the old OBJ-only 180-degree
        -- yaw bridge a second time. The complete GLB should therefore appear
        -- in Heritage with the same fore/aft orientation authored in Blender.
        return 0.0, 0.0, 0.0
    end
    return vehicleVisual.rotationDegrees[1],
        vehicleVisual.rotationDegrees[2],
        vehicleVisual.rotationDegrees[3]
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
    local rotationX, rotationY, rotationZ =
        VehicleVisualRotationForCurrentAsset()
    Entity.SetLocalRotation(
        chassisEntity, rotationX, rotationY, rotationZ)
    Entity.SetLocalScale(
        chassisEntity,
        vehicleVisual.scale,
        vehicleVisual.scale,
        vehicleVisual.scale)
    return true
end


function RefreshVehicleAssetMetadata()
    vehicleAssetMetadata = nil
    local path = string.lower(tostring(vehicleVisual.assetPath or ""))
    if not string.match(path, "%.glb$") then
        vehicleAssetMetadataMessage = "Current visual is not GLB; semantic Custom Properties are available on GLB assets."
        if VehicleGeometryImportAssetGeometry ~= nil then
            VehicleGeometryImportAssetGeometry(nil)
        end
        if RefreshEmbeddedVehicleWheelBinding ~= nil then
            RefreshEmbeddedVehicleWheelBinding()
        end
        return false
    end

    local metadata, errorMessage = Vehicle.InspectAssetMetadata(vehicleVisual.assetPath)
    if metadata == nil then
        vehicleAssetMetadataMessage = "METADATA ERROR: " .. tostring(errorMessage)
        if VehicleGeometryImportAssetGeometry ~= nil then
            VehicleGeometryImportAssetGeometry(nil)
        end
        if RefreshEmbeddedVehicleWheelBinding ~= nil then
            RefreshEmbeddedVehicleWheelBinding()
        end
        return false
    end

    vehicleAssetMetadata = metadata
    vehicleAssetMetadataMessage = "Heritage discovered "
        .. tostring(metadata.part_count or 0)
        .. " semantic parts + "
        .. tostring(metadata.suspension_hardpoint_count or 0)
        .. " suspension hardpoints from GLB metadata"
    if SuspensionAuthoringImportHardpointsFromMetadata ~= nil then
        SuspensionAuthoringImportHardpointsFromMetadata(metadata)
    end
    if VehicleFitmentImportReferenceFromMetadata ~= nil then
        VehicleFitmentImportReferenceFromMetadata(metadata)
    end
    if RideHeightImportAssetGeometry ~= nil then
        RideHeightImportAssetGeometry(metadata)
    end
    if VehicleGeometryImportAssetGeometry ~= nil then
        VehicleGeometryImportAssetGeometry(metadata)
    end
    if RefreshEmbeddedVehicleWheelBinding ~= nil then
        RefreshEmbeddedVehicleWheelBinding()
    end
    return true
end

local function ApplyVehicleVisualNodeFilter()
    if chassisEntity == 0 or not Entity.Exists(chassisEntity)
        or not Entity.HasMesh(chassisEntity) then
        return false
    end

    local prefix = vehicleVisual.isolateWheelAssembly and "WH_" or ""
    if not Entity.SetMeshNodePrefixFilter(chassisEntity, prefix) then
        vehicleVisualMessage = "VISUAL FILTER ERROR: " .. Entity.GetLastError()
        return false
    end
    return true
end

function SetVehicleWheelAssemblyIsolation(enabled)
    vehicleVisual.isolateWheelAssembly = enabled
    local ok = ApplyVehicleVisualNodeFilter()
    if ok then
        vehicleVisualMessage = enabled
            and "Diagnostic isolation: showing only WH_* wheel/tire/brake nodes"
            or "Diagnostic isolation disabled: showing complete vehicle GLB"
    end
    return ok
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
    ApplyVehicleVisualNodeFilter()
    ApplyVehicleVisualTransform()
    SetVehicleDebugVisible(prototypeScenePreset ~= "visual")
    vehicleVisualMessage = "Using vehicle asset: " .. vehicleVisual.assetPath
    RefreshVehicleAssetMetadata()
    return true
end

function UsePlayerVehicleVisual()
    vehicleAssetDiscovery.autoOwnedPath = ""
    vehicleVisual.assetPath = PrototypeCarDefinition.visual.bodyAsset
    vehicleVisual.usingFallback = false
    return ApplyVehicleVisualMesh()
end

function UseFallbackVehicleVisual()
    vehicleAssetDiscovery.autoOwnedPath = ""
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
    -- AS01A: do not force a filesystem scan while the scene is entering.
    -- The engine-level registry performs its first lazy scan after startup,
    -- then OnUpdate reacts to the revision normally.
    vehicleAssetDiscovery.lastRevision = Module.GetAssetIndexRevision()
    ApplyVehicleVisualMesh()
    ApplyVehicleWheelMeshes()
    SetArticulatedWheelVisualsEnabled(vehicleWheelVisual.enabled)
end
