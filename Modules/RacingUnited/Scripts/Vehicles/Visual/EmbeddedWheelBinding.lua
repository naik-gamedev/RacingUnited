-- CLEAN08: embedded GLB WH_* semantic wheel binding and tire deformation.
-- Blender bind pose remains authoritative; only native suspension/upright/spin deltas
-- and tire physical state are applied at runtime.

local Transform = VehicleVisualTransformMath
local WheelChassisLocalVectorToRenderWorld = Transform.ChassisLocalVectorToRenderWorld
local WheelUprightDeltaEulerDegrees = Transform.UprightDeltaEulerDegrees

local embeddedWheelCorners = { "FL", "FR", "RL", "RR" }

-- TIRE28C/VIS21: resolve visual WH_* semantics by PHYSICAL mount geometry, not
-- by the native vector index or its legacy left/right label.  The creator GLB is
-- authoritative: WH_FL/WH_RL are on the +X side of this vehicle because its nose
-- is authored toward Blender -Y.  TIRE27 live debugging proved that blindly using
-- index 1=FL, 2=FR routed a left-side sidewalk deformation to the opposite visual
-- tire.  Keep native simulation order internal and bind each visible corner to the
-- wheel that actually occupies that authored corner.
local function EmbeddedWheelNativeIndexForCorner(corner)
    local wheels = PrototypeCarDefinition and PrototypeCarDefinition.wheels or nil
    if wheels == nil or #wheels == 0 then return nil end

    local minX, maxX = math.huge, -math.huge
    local minZ, maxZ = math.huge, -math.huge
    for _, wheel in ipairs(wheels) do
        local mount = wheel.mount or { 0.0, 0.0, 0.0 }
        local x = tonumber(mount[1]) or 0.0
        local z = tonumber(mount[3]) or 0.0
        minX, maxX = math.min(minX, x), math.max(maxX, x)
        minZ, maxZ = math.min(minZ, z), math.max(maxZ, z)
    end

    local visualLeft = string.sub(corner, 2, 2) == "L"
    local visualFront = string.sub(corner, 1, 1) == "F"
    -- Authored GLB convention for this project: vehicle-left is +X, front is +Z
    -- after the Blender -> glTF/Heritage basis conversion.
    local targetX = visualLeft and maxX or minX
    local targetZ = visualFront and maxZ or minZ

    local bestIndex, bestScore = nil, math.huge
    for index, wheel in ipairs(wheels) do
        local mount = wheel.mount or { 0.0, 0.0, 0.0 }
        local dx = (tonumber(mount[1]) or 0.0) - targetX
        local dz = (tonumber(mount[3]) or 0.0) - targetZ
        local score = dx * dx + dz * dz
        if score < bestScore then
            bestScore = score
            bestIndex = index
        end
    end
    return bestIndex
end


local function WrappedWheelSpinDegrees(value)
    local wrapped = (value or 0.0) % 360.0
    if wrapped > 180.0 then
        wrapped = wrapped - 360.0
    end
    return wrapped
end

local function EmbeddedWheelReferenceSuspensionLength(wheel, telemetry)
    -- The embedded GLB wheel subtree is authored at the vehicle's reference
    -- suspension geometry. Do NOT capture a live compressed suspension length
    -- as the bind datum: doing so erases legitimate static spring travel and
    -- can leave the rendered tire several centimetres below the road.
    local shared = PrototypeCarDefinition ~= nil
        and PrototypeCarDefinition.wheelPhysics or nil
    local referenceLength = wheel ~= nil and wheel.restLengthM or nil
    if referenceLength == nil and shared ~= nil then
        referenceLength = shared.restLengthM
    end
    if referenceLength == nil or referenceLength <= 0.0 then
        -- Compatibility fallback for definitions that genuinely lack an
        -- authored rest datum. Current Racing United vehicles do provide one.
        referenceLength = telemetry ~= nil
            and telemetry.length or 0.0
    end
    return referenceLength
end

local function HasEmbeddedSemanticPart(name)
    return vehicleAssetMetadata ~= nil
        and vehicleAssetMetadata.parts ~= nil
        and vehicleAssetMetadata.parts[name] ~= nil
end

function RefreshEmbeddedVehicleWheelBinding()
    vehicleEmbeddedWheelBinding.active = false
    vehicleEmbeddedWheelBinding.detectedCorners = 0
    vehicleEmbeddedWheelBinding.bindPose = {}

    if chassisEntity == 0 or not Entity.Exists(chassisEntity) then
        vehicleEmbeddedWheelBinding.message = "VA02 waiting for Player Chassis"
        return false
    end

    local path = string.lower(tostring(vehicleVisual.assetPath or ""))
    if not string.match(path, "%.glb$") or vehicleAssetMetadata == nil then
        Entity.ClearMeshNodeOverrides(chassisEntity)
        vehicleEmbeddedWheelBinding.message = "VA02 requires a GLB with WH_* semantic node metadata"
        RefreshVehicleWheelVisibility()
        return false
    end

    for _, corner in ipairs(embeddedWheelCorners) do
        local hasRoot = HasEmbeddedSemanticPart("WH_" .. corner .. "_Root")
        local hasPivot = HasEmbeddedSemanticPart("WH_" .. corner .. "_Pivot")
        if hasRoot and hasPivot then
            vehicleEmbeddedWheelBinding.detectedCorners =
                vehicleEmbeddedWheelBinding.detectedCorners + 1
        end
    end

    vehicleEmbeddedWheelBinding.active = vehicleEmbeddedWheelBinding.enabled
        and vehicleEmbeddedWheelBinding.detectedCorners == 4

    if vehicleEmbeddedWheelBinding.active then
        Entity.ClearMeshNodeOverrides(chassisEntity)
        vehicleEmbeddedWheelBinding.message =
            "VA02H active: Blender bind pose + authored suspension rest datum are authoritative; embedded wheels use local suspension/upright/spin deltas"
    elseif not vehicleEmbeddedWheelBinding.enabled then
        Entity.ClearMeshNodeOverrides(chassisEntity)
        vehicleEmbeddedWheelBinding.message = "VA02 embedded GLB wheel binding disabled"
    else
        Entity.ClearMeshNodeOverrides(chassisEntity)
        vehicleEmbeddedWheelBinding.message = string.format(
            "VA02 detected %d/4 complete WH_* Root+Pivot corners",
            vehicleEmbeddedWheelBinding.detectedCorners)
    end

    RefreshVehicleWheelVisibility()
    return vehicleEmbeddedWheelBinding.active
end

function SetEmbeddedVehicleWheelBindingEnabled(enabled)
    vehicleEmbeddedWheelBinding.enabled = enabled
    Save.SetBool("vehicle.visual.embedded_glb_wheel_binding", enabled)
    return RefreshEmbeddedVehicleWheelBinding()
end

local function UpdateEmbeddedVehicleWheelNodes()
    if not vehicleEmbeddedWheelBinding.active
        or chassisEntity == 0
        or not Entity.Exists(chassisEntity) then
        return false
    end

    for index, corner in ipairs(embeddedWheelCorners) do
        local nativeIndex = EmbeddedWheelNativeIndexForCorner(corner) or index
        local telemetry = vehicleWheelTelemetry[nativeIndex]
        local wheel = PrototypeCarDefinition.wheels[nativeIndex]
        if telemetry ~= nil and wheel ~= nil then
            local rootName = "WH_" .. corner .. "_Root"
            local pivotName = "WH_" .. corner .. "_Pivot"
            local baseline = vehicleEmbeddedWheelBinding.bindPose[corner]

            if baseline == nil then
                baseline = {
                    -- VA02 bind position is the authored suspension datum, not
                    -- an arbitrary live state sampled after the car has settled.
                    suspensionLength = EmbeddedWheelReferenceSuspensionLength(
                        wheel, telemetry),
                    uprightRotationX = telemetry.uprightRotationX or 0.0,
                    uprightRotationY = telemetry.uprightRotationY or 0.0,
                    uprightRotationZ = telemetry.uprightRotationZ or 0.0,
                    rotationDegrees = telemetry.rotationDegrees or 0.0
                }
                vehicleEmbeddedWheelBinding.bindPose[corner] = baseline
            end

            -- IMPORTANT: the complete GLB is already positioned exactly
            -- in Blender. Embedded presentation therefore does NOT use
            -- WheelState.worldCenter at all.
            --
            -- VehicleSystem computes worldCenter BEFORE RigidBodySystem
            -- integrates the chassis for the 240 Hz world step. At 100 m/s,
            -- one 240 Hz step is ~0.417 m, which is almost exactly the
            -- "wheels run 35-40 cm ahead" artifact we observed.
            --
            -- The only translational wheel motion we actually need on top of
            -- the authored GLB bind pose is suspension travel RELATIVE to the
            -- chassis. The native suspension direction for this prototype is
            -- local (0,-1,0), so:
            --     delta = direction * (currentLength - bindLength)
            -- Telemetry.lua exposes this value as `length`. Do not use the
            -- nonexistent old suspension-length field name.
            local lengthDelta =
                (telemetry.length or baseline.suspensionLength)
                - baseline.suspensionLength
            local deltaLocalX = 0.0
            local deltaLocalY = -lengthDelta
            local deltaLocalZ = 0.0

            local deltaX, deltaY, deltaZ =
                WheelChassisLocalVectorToRenderWorld(
                    deltaLocalX,
                    deltaLocalY,
                    deltaLocalZ)

            local deltaRotX, deltaRotY, deltaRotZ =
                WheelUprightDeltaEulerDegrees(telemetry, baseline)

            Entity.SetMeshNodeAnchoredWorldDelta(
                chassisEntity,
                rootName,
                pivotName,
                deltaX,
                deltaY,
                deltaZ,
                deltaRotX,
                deltaRotY,
                deltaRotZ)

            Entity.SetMeshNodeLocalRotationOffset(
                chassisEntity,
                pivotName,
                WrappedWheelSpinDegrees(
                    (telemetry.rotationDegrees or 0.0)
                    - baseline.rotationDegrees),
                0.0,
                0.0)

            -- TIRE09/VIS01: drive the actual WH_*_Tire mesh from native tire
            -- state. The renderer auto-infers bead/sidewall/tread geometry
            -- from the authored GLB node, so this current Peugeot mesh needs
            -- no extra bones, vertex colours or Blender-side deformation rig.
            local tireNodeName = "WH_" .. corner .. "_Tire"
            -- VIS02 keeps the authored/nominal mesh radius as the metre-to-local
            -- scale. Dynamic loaded radius, tread wear and flat spots belong to
            -- the physics/contact state and must not silently rescale the GLB.
            local referenceRadius = wheel.radiusM
                or PrototypeCarDefinition.referenceGeometry.wheelRadiusM
                or telemetry.tireFreeRollingRadius
                or 0.30
            if referenceRadius <= 0.02 then
                referenceRadius = 0.30
            end

            local contactNormalX = telemetry.contactNormalX or 0.0
            local contactNormalY = telemetry.contactNormalY or 1.0
            local contactNormalZ = telemetry.contactNormalZ or 0.0
            local normalLength = math.sqrt(
                contactNormalX * contactNormalX
                + contactNormalY * contactNormalY
                + contactNormalZ * contactNormalZ)
            if normalLength > 0.000001 then
                contactNormalX = contactNormalX / normalLength
                contactNormalY = contactNormalY / normalLength
                contactNormalZ = contactNormalZ / normalLength
            else
                contactNormalX, contactNormalY, contactNormalZ = 0.0, 1.0, 0.0
            end

            local centerToPlane = referenceRadius
                - math.max(telemetry.tireDeflection or 0.0, 0.0)
            if telemetry.grounded == true then
                local dx = (telemetry.centerX or 0.0) - (telemetry.contactX or 0.0)
                local dy = (telemetry.centerY or 0.0) - (telemetry.contactY or 0.0)
                local dz = (telemetry.centerZ or 0.0) - (telemetry.contactZ or 0.0)
                local measured = dx * contactNormalX
                    + dy * contactNormalY
                    + dz * contactNormalZ
                if measured > 0.02 then
                    centerToPlane = measured
                end
            end

            local longitudinalSpeed = telemetry.longitudinalSpeed or 0.0
            local lateralSpeed = telemetry.lateralSpeed or 0.0
            local roadMotionSpeed = math.sqrt(
                longitudinalSpeed * longitudinalSpeed
                + lateralSpeed * lateralSpeed)
            local rollingRadius = telemetry.tireEffectiveRollingRadius or referenceRadius
            local wheelSurfaceSpeed = math.abs(telemetry.angularVelocity or 0.0)
                * math.max(rollingRadius, 0.0)
            local visualMotionSpeed = math.max(roadMotionSpeed, wheelSurfaceSpeed)

            Entity.SetMeshNodeTireDeformation(
                chassisEntity,
                tireNodeName,
                telemetry.grounded == true,
                referenceRadius,
                math.max(telemetry.tireDeflection or 0.0, 0.0),
                math.max(telemetry.tireContactPatchLength or 0.0, 0.0),
                math.max(telemetry.tireContactPatchWidth or 0.0, 0.0),
                telemetry.tireRingRadialOffset or 0.0,
                telemetry.tireRingLongitudinalOffset or 0.0,
                telemetry.tireRingLateralOffset or 0.0,
                telemetry.tireRingYawDegrees or 0.0,
                telemetry.tireRingWindupDegrees or 0.0,
                math.max((telemetry.tireFlatSpotDepthMm or 0.0) * 0.001, 0.0),
                telemetry.tireFlatSpotSector or 0.0,
                contactNormalX,
                contactNormalY,
                contactNormalZ,
                math.max(centerToPlane, 0.0),
                telemetry.tireVisualSupportGridValid == true,
                math.max(telemetry.tireVisualSupportHalfLengthM or 0.0, 0.0),
                math.max(telemetry.tireVisualSupportHalfWidthM or 0.0, 0.0),
                telemetry.tireVisualSupportH00 or 0.0,
                telemetry.tireVisualSupportH01 or 0.0,
                telemetry.tireVisualSupportH02 or 0.0,
                telemetry.tireVisualSupportH10 or 0.0,
                telemetry.tireVisualSupportH11 or 0.0,
                telemetry.tireVisualSupportH12 or 0.0,
                telemetry.tireVisualSupportH20 or 0.0,
                telemetry.tireVisualSupportH21 or 0.0,
                telemetry.tireVisualSupportH22 or 0.0,
                telemetry.wheelForwardX or 0.0,
                telemetry.wheelForwardY or 0.0,
                telemetry.wheelForwardZ or 1.0,
                telemetry.wheelRightX or 1.0,
                telemetry.wheelRightY or 0.0,
                telemetry.wheelRightZ or 0.0,
                math.max(telemetry.normalForce or 0.0, 0.0),
                telemetry.longitudinalForce or 0.0,
                telemetry.lateralForce or 0.0,
                visualMotionSpeed)

            -- TIRE17C7/VIS10 native-to-native bridge for the exact nearby
            -- static collision triangles. Bulk collider geometry never
            -- passes through Lua telemetry.
            Entity.SetMeshNodeTireColliderTrianglesFromWheel(
                chassisEntity, tireNodeName, nativeVehicle, nativeIndex)
        end
    end
    return true
end


VehicleEmbeddedWheelInternal = VehicleEmbeddedWheelInternal or {}
VehicleEmbeddedWheelInternal.UpdateNodes = UpdateEmbeddedVehicleWheelNodes
