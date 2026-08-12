-- CLEAN08: suspension hardpoint evidence/source ownership.
-- Measured/asset-authored/legacy/estimated points share one provenance-aware
-- contract. This file owns source priority, hardpoint records/readiness, and
-- GLB metadata import; estimation and gizmo presentation live elsewhere.

local function SuspensionAuthoringWheelDefinition(index)
    return PrototypeCarDefinition.wheels[index]
end

local function SuspensionAuthoringPhysicsForWheel(wheel)
    local shared = PrototypeCarDefinition.wheelPhysics
    return {
        restLengthM = wheel.restLengthM or shared.restLengthM,
        maximumCompressionM = wheel.maximumCompressionM
            or shared.maximumCompressionM,
        maximumDroopM = wheel.maximumDroopM or shared.maximumDroopM,
        suspensionDirection = wheel.suspensionDirection or { 0.0, -1.0, 0.0 },
        steeringAxis = wheel.steeringAxis or shared.steeringAxis
    }
end

local function SuspensionAuthoringAssembly(wheel)
    local architectures = PrototypeCarDefinition.suspensionArchitecture or {}
    return architectures[wheel.axle or ""]
end

local function SuspensionAuthoringAdd(a, b)
    return { a[1] + b[1], a[2] + b[2], a[3] + b[3] }
end

local function SuspensionAuthoringScale(v, scale)
    return { v[1] * scale, v[2] * scale, v[3] * scale }
end

local function SuspensionAuthoringNormalize(v)
    local magnitude = math.sqrt(v[1] * v[1] + v[2] * v[2] + v[3] * v[3])
    if magnitude <= 0.000001 then
        return { 0.0, 1.0, 0.0 }
    end
    return { v[1] / magnitude, v[2] / magnitude, v[3] / magnitude }
end

local function SuspensionAuthoringHardpointPosition(value)
    if type(value) ~= "table" then return nil end
    if type(value.position) == "table" then
        value = value.position
    end
    if #value < 3 then return nil end
    return value
end

local function SuspensionAuthoringHardpointProvenance(value)
    if type(value) == "table" and type(value.position) == "table" then
        return tostring(value.provenance or "unknown")
    end
    return SuspensionAuthoringHardpointPosition(value) and "legacy_authored"
        or "missing"
end

local function SuspensionAuthoringHardpointConfidence(value)
    if type(value) == "table" and type(value.position) == "table" then
        return math.max(0.0, math.min(1.0, tonumber(value.confidence) or 0.0))
    end
    return SuspensionAuthoringHardpointPosition(value) and 0.50 or 0.0
end

local function SuspensionAuthoringSourcePriority(provenance)
    if provenance == "measured" then return 4 end
    if provenance == "asset_authored" then return 3 end
    if provenance == "legacy_authored" then return 2 end
    if provenance == "estimated" then return 1 end
    return 0
end

local function SuspensionAuthoringMakeHardpoint(
    position, provenance, confidence, profile)
    return {
        position = { position[1], position[2], position[3] },
        provenance = provenance or "unknown",
        confidence = math.max(0.0, math.min(1.0, confidence or 0.0)),
        profile = profile or ""
    }
end

local function SuspensionAuthoringRequiredId(assembly, id)
    for _, requiredId in ipairs(assembly and assembly.requiredHardpoints or {}) do
        if requiredId == id then return true end
    end
    return false
end

local function SuspensionAuthoringSetHardpoint(
    assembly, cornerName, id, value, allowEqualPriority)
    if assembly == nil or not SuspensionAuthoringRequiredId(assembly, id) then
        return false
    end
    assembly.hardpointsByCorner = assembly.hardpointsByCorner or {}
    assembly.hardpointsByCorner[cornerName] =
        assembly.hardpointsByCorner[cornerName] or {}
    local corner = assembly.hardpointsByCorner[cornerName]
    local existing = corner[id]
    local existingPriority = SuspensionAuthoringSourcePriority(
        SuspensionAuthoringHardpointProvenance(existing))
    local incomingPriority = SuspensionAuthoringSourcePriority(
        SuspensionAuthoringHardpointProvenance(value))
    if existing ~= nil
        and (existingPriority > incomingPriority
            or (existingPriority == incomingPriority and not allowEqualPriority)) then
        return false
    end
    corner[id] = value
    return true
end

local function SuspensionAuthoringWheelCenter(wheel)
    local physics = SuspensionAuthoringPhysicsForWheel(wheel)
    local direction = SuspensionAuthoringNormalize(physics.suspensionDirection)
    return SuspensionAuthoringAdd(
        wheel.mount,
        SuspensionAuthoringScale(direction, physics.restLengthM))
end

local function SuspensionAuthoringRefreshRuntimeProvider(assembly, wheel)
    if assembly == nil or wheel == nil then return false end
    local authored, required = SuspensionAuthoringHardpointReadiness(assembly, wheel)
    if required > 0 and authored == required
        and assembly.preferredProvider ~= nil
        and assembly.preferredProvider ~= "" then
        assembly.runtimeProvider = assembly.preferredProvider
        return true
    end
    assembly.runtimeProvider = "linear_raycast_v1"
    return false
end

function SuspensionAuthoringHardpointReadiness(assembly, wheel)
    if assembly == nil or wheel == nil then
        return 0, 0
    end
    local authored = 0
    local required = #(assembly.requiredHardpoints or {})
    local hardpoints = assembly.hardpointsByCorner
        and assembly.hardpointsByCorner[wheel.name] or {}
    for _, id in ipairs(assembly.requiredHardpoints or {}) do
        if SuspensionAuthoringHardpointPosition(hardpoints[id]) ~= nil then
            authored = authored + 1
        end
    end
    return authored, required
end

function SuspensionAuthoringHardpointStatus(assembly, wheel, id)
    if assembly == nil or wheel == nil then
        return "MISSING", 0.0
    end
    local hardpoints = assembly.hardpointsByCorner
        and assembly.hardpointsByCorner[wheel.name] or {}
    local value = hardpoints[id]
    local position = SuspensionAuthoringHardpointPosition(value)
    if position == nil then return "MISSING", 0.0 end
    return string.upper(SuspensionAuthoringHardpointProvenance(value)),
        SuspensionAuthoringHardpointConfidence(value)
end

function SuspensionAuthoringImportHardpointsFromMetadata(metadata)
    if metadata == nil or type(metadata.suspension_hardpoints) ~= "table" then
        return 0
    end

    local imported = 0
    for _, hardpoint in pairs(metadata.suspension_hardpoints) do
        local corner = tostring(hardpoint.corner or "")
        local id = tostring(hardpoint.id or "")
        local axle = string.match(corner, "^front_") and "front"
            or (string.match(corner, "^rear_") and "rear" or nil)
        local assembly = axle and PrototypeCarDefinition.suspensionArchitecture
            and PrototypeCarDefinition.suspensionArchitecture[axle] or nil
        if assembly ~= nil and SuspensionAuthoringRequiredId(assembly, id) then
            local record = SuspensionAuthoringMakeHardpoint(
                {
                    tonumber(hardpoint.x) or 0.0,
                    tonumber(hardpoint.y) or 0.0,
                    tonumber(hardpoint.z) or 0.0
                },
                tostring(hardpoint.provenance or "asset_authored"),
                tonumber(hardpoint.confidence) or 0.75,
                "glb_node")
            if SuspensionAuthoringSetHardpoint(
                assembly, corner, id, record, true) then
                imported = imported + 1
            end
        end
    end

    for _, wheel in ipairs(PrototypeCarDefinition.wheels or {}) do
        SuspensionAuthoringRefreshRuntimeProvider(
            SuspensionAuthoringAssembly(wheel), wheel)
    end

    vehicleSuspensionAuthoring.assetImportedCount = imported
    if imported > 0 then
        vehicleSuspensionAuthoring.message = string.format(
            "Imported %d suspension hardpoints from the current GLB",
            imported)
        if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
            ApplySuspensionAuthoringGeometryToNativeVehicle()
        end
        if vehicleSuspensionAuthoring.enabled then
            RefreshSuspensionAuthoringGizmos()
        end
    end
    return imported
end

function ImportSuspensionHardpointsFromCurrentAsset()
    if vehicleVisual == nil then
        vehicleSuspensionAuthoring.message = "No current vehicle visual exists"
        return false
    end
    local path = tostring(vehicleVisual.assetPath or "")
    if not string.match(string.lower(path), "%.glb$") then
        vehicleSuspensionAuthoring.message =
            "Current vehicle visual is not a GLB; no hardpoint nodes can be imported"
        return false
    end
    local metadata, errorMessage = Vehicle.InspectAssetMetadata(path)
    if metadata == nil then
        vehicleSuspensionAuthoring.message =
            "GLB hardpoint import failed: " .. tostring(errorMessage)
        return false
    end
    local imported = SuspensionAuthoringImportHardpointsFromMetadata(metadata)
    if imported == 0 then
        vehicleSuspensionAuthoring.message =
            "GLB contains no recognized SUS_FL/SUS_FR/SUS_RL/SUS_RR hardpoint nodes"
        return false
    end
    return true
end


SuspensionAuthoringInternal = SuspensionAuthoringInternal or {}
SuspensionAuthoringInternal.WheelDefinition = SuspensionAuthoringWheelDefinition
SuspensionAuthoringInternal.PhysicsForWheel = SuspensionAuthoringPhysicsForWheel
SuspensionAuthoringInternal.Assembly = SuspensionAuthoringAssembly
SuspensionAuthoringInternal.Add = SuspensionAuthoringAdd
SuspensionAuthoringInternal.Scale = SuspensionAuthoringScale
SuspensionAuthoringInternal.Normalize = SuspensionAuthoringNormalize
SuspensionAuthoringInternal.HardpointPosition = SuspensionAuthoringHardpointPosition
SuspensionAuthoringInternal.HardpointProvenance = SuspensionAuthoringHardpointProvenance
SuspensionAuthoringInternal.HardpointConfidence = SuspensionAuthoringHardpointConfidence
SuspensionAuthoringInternal.SourcePriority = SuspensionAuthoringSourcePriority
SuspensionAuthoringInternal.MakeHardpoint = SuspensionAuthoringMakeHardpoint
SuspensionAuthoringInternal.RequiredId = SuspensionAuthoringRequiredId
SuspensionAuthoringInternal.SetHardpoint = SuspensionAuthoringSetHardpoint
SuspensionAuthoringInternal.WheelCenter = SuspensionAuthoringWheelCenter
SuspensionAuthoringInternal.RefreshRuntimeProvider = SuspensionAuthoringRefreshRuntimeProvider
