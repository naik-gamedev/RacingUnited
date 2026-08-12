-- CLEAN08: deterministic assisted suspension-hardpoint estimation.
-- Estimates are explicitly low-confidence authoring data and never derive chassis
-- pickup geometry from the currently installed wheel/tire fitment.

local I = SuspensionAuthoringInternal
local SuspensionAuthoringWheelCenter = I.WheelCenter
local SuspensionAuthoringHardpointProvenance = I.HardpointProvenance
local SuspensionAuthoringMakeHardpoint = I.MakeHardpoint
local SuspensionAuthoringSetHardpoint = I.SetHardpoint
local SuspensionAuthoringRefreshRuntimeProvider = I.RefreshRuntimeProvider

function EnsureSuspensionHardpointEstimates(forceRebuild)
    if Vehicle == nil or Vehicle.EstimateMacPhersonHardpoints == nil
        or Vehicle.EstimateTrailingArmHardpoints == nil then
        vehicleSuspensionAuthoring.message =
            "Native assisted suspension estimators are unavailable"
        return false
    end

    local architectures = PrototypeCarDefinition.suspensionArchitecture or {}
    local front = architectures.front
    local rear = architectures.rear
    if front == nil or rear == nil then return false end

    local alignment = PrototypeCarDefinition.referenceAlignment or {}
    local estimation = PrototypeCarDefinition.suspensionEstimation or {}
    local caster = tonumber(alignment.frontCasterDegrees) or 3.0
    local sai = tonumber(alignment.frontSteeringAxisInclinationDegrees) or 10.0
    -- IMPORTANT: these are immutable suspension-package reference dimensions.
    -- They intentionally do not read wheel.radiusM, so swapping wheels/tires
    -- cannot silently rewrite chassis suspension pickup points.
    local frontScale = tonumber(estimation.frontReferencePackageScaleM) or 0.30
    local rearScale = tonumber(estimation.rearReferencePackageScaleM) or 0.30
    local inserted = 0
    local frontInserted = 0
    local rearInserted = 0

    local function clearEstimatedPoint(assembly, cornerName, id)
        if not forceRebuild then return end
        local existing = assembly.hardpointsByCorner
            and assembly.hardpointsByCorner[cornerName]
            and assembly.hardpointsByCorner[cornerName][id]
        if SuspensionAuthoringHardpointProvenance(existing) == "estimated" then
            assembly.hardpointsByCorner[cornerName][id] = nil
        end
    end

    local function insertEstimate(assembly, wheel, estimate)
        local count = 0
        local estimatedPoints = estimate.hardpoints or {}
        for _, id in ipairs(assembly.requiredHardpoints or {}) do
            local point = estimatedPoints[id]
            if point ~= nil then
                clearEstimatedPoint(assembly, wheel.name, id)
                local record = SuspensionAuthoringMakeHardpoint(
                    { point.x, point.y, point.z },
                    "estimated",
                    tonumber(estimate.confidence) or 0.30,
                    tostring(estimate.profile_id or "estimated_suspension_v1"))
                if SuspensionAuthoringSetHardpoint(
                    assembly, wheel.name, id, record, forceRebuild == true) then
                    count = count + 1
                end
            end
        end
        SuspensionAuthoringRefreshRuntimeProvider(assembly, wheel)
        return count
    end

    for _, wheel in ipairs(PrototypeCarDefinition.wheels or {}) do
        local centre = SuspensionAuthoringWheelCenter(wheel)
        if wheel.axle == "front" then
            local estimate, errorMessage = Vehicle.EstimateMacPhersonHardpoints(
                centre[1], centre[2], centre[3],
                frontScale, caster, sai)
            if estimate == nil then
                vehicleSuspensionAuthoring.message =
                    "MacPherson estimate failed: " .. tostring(errorMessage)
                return false
            end
            local count = insertEstimate(front, wheel, estimate)
            frontInserted = frontInserted + count
            inserted = inserted + count
        elseif wheel.axle == "rear" then
            local estimate, errorMessage = Vehicle.EstimateTrailingArmHardpoints(
                centre[1], centre[2], centre[3], rearScale)
            if estimate == nil then
                vehicleSuspensionAuthoring.message =
                    "Trailing-arm estimate failed: " .. tostring(errorMessage)
                return false
            end
            local count = insertEstimate(rear, wheel, estimate)
            rearInserted = rearInserted + count
            inserted = inserted + count
        end
    end

    vehicleSuspensionAuthoring.estimateProfile =
        "estimated_macpherson_road_v1 + estimated_trailing_arm_torsion_bar_road_v1"
    vehicleSuspensionAuthoring.estimatedCount = inserted
    vehicleSuspensionAuthoring.message = string.format(
        "Assisted authoring supplied %d front + %d rear low-confidence hardpoints",
        frontInserted, rearInserted)
    return true
end
