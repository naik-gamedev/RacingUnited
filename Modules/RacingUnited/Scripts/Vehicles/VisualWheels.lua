-- Step 29P articulated wheel presentation.
-- Rendered wheel entities consume authoritative native center/upright telemetry.
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

local function WheelQuaternionMultiply(a, b)
    return {
        a[1] * b[1] - a[2] * b[2] - a[3] * b[3] - a[4] * b[4],
        a[1] * b[2] + a[2] * b[1] + a[3] * b[4] - a[4] * b[3],
        a[1] * b[3] - a[2] * b[4] + a[3] * b[1] + a[4] * b[2],
        a[1] * b[4] + a[2] * b[3] - a[3] * b[2] + a[4] * b[1]
    }
end

local function WheelQuaternionFromEulerDegrees(x, y, z)
    local halfX = math.rad(x) * 0.5
    local halfY = math.rad(y) * 0.5
    local halfZ = math.rad(z) * 0.5
    local cx, sx = math.cos(halfX), math.sin(halfX)
    local cy, sy = math.cos(halfY), math.sin(halfY)
    local cz, sz = math.cos(halfZ), math.sin(halfZ)
    return {
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    }
end

local function WheelEulerDegreesFromQuaternion(q)
    local w, x, y, z = q[1], q[2], q[3], q[4]
    local rightX = 1.0 - 2.0 * (y * y + z * z)
    local rightY = 2.0 * (x * y + w * z)
    local rightZ = 2.0 * (x * z - w * y)
    local upZ = 2.0 * (y * z + w * x)
    local forwardZ = 1.0 - 2.0 * (x * x + y * y)
    local rotationY = math.asin(math.max(-1.0, math.min(1.0, -rightZ)))
    local rotationX = math.atan(upZ, forwardZ)
    local rotationZ = math.atan(rightY, rightX)
    return math.deg(rotationX), math.deg(rotationY), math.deg(rotationZ)
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

            -- Native geometry owns the complete upright orientation. Compose
            -- module-specific mesh facing, author offset and wheel spin after
            -- that pose without reconstructing steering/camber/toe in Lua.
            local rotationX, rotationY, rotationZ =
                VehicleWheelVisualRotation(telemetry, wheel)
            Entity.SetLocalRotation(
                entity, rotationX, rotationY, rotationZ)
        end
    end
end
