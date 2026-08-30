-- FITMENT01: reference assembly geometry stays immutable; installed wheel
-- fitment and alignment are per-corner setup data layered on top of it.
-- Human-facing camber is symmetric (- = top inward on either side). Human-facing
-- toe is +toe-in / -toe-out. Native local signs are mirrored per side.

local fitmentCorners = {
    { code = "FL", axle = "front", side = "left",  sideSign = -1.0, index = 1 },
    { code = "FR", axle = "front", side = "right", sideSign =  1.0, index = 2 },
    { code = "RL", axle = "rear",  side = "left",  sideSign = -1.0, index = 3 },
    { code = "RR", axle = "rear",  side = "right", sideSign =  1.0, index = 4 }
}

local function FitmentNumber(value, fallback)
    local number = tonumber(value)
    if number == nil then return fallback end
    return number
end

local function FitmentSavePrefix(code)
    return "vehicle.setup.fitment." .. string.lower(code) .. "."
end

local function FactoryAlignment(axle)
    local setup = PrototypeCarDefinition.factorySetup or {}
    return setup[axle] or {
        camberDegrees = 0.0,
        toeInDegrees = 0.0,
        casterDegrees = 0.0,
        casterAdjustable = false
    }
end

local function FactoryAlignmentSpecification(axle)
    local specification = PrototypeCarDefinition.factoryAlignmentSpecification or {}
    return specification[axle] or {}
end

local function SpecificationRange(specification, key)
    local range = specification[key] or {}
    return FitmentNumber(range.minimum, nil), FitmentNumber(range.maximum, nil)
end

local function ReferenceFitment()
    return PrototypeCarDefinition.referenceFitment or {
        provenance = "definition_reference",
        rimDiameterIn = 17.0,
        rimWidthIn = 7.0,
        offsetEtMm = 0.0,
        tireWidthMm = 205.0,
        tireAspectRatio = 40.0,
        tireRimDiameterIn = 17.0
    }
end

vehicleFitment = {
    selectedWheel = 1,
    linkFront = Save.GetBool("vehicle.setup.fitment.link_front", true),
    linkRear = Save.GetBool("vehicle.setup.fitment.link_rear", true),
    advancedAlignmentRange = Save.GetBool(
        "vehicle.setup.fitment.advanced_alignment_range", false),
    factorySpecProvenance = (PrototypeCarDefinition.factoryAlignmentSpecification or {}).provenance,
    factorySpecConfidence = (PrototypeCarDefinition.factoryAlignmentSpecification or {}).confidence or 0.0,
    corners = {},
    message = "FITMENT02 hub datums + scrub radius / mechanical trail diagnostics ready"
}

for _, descriptor in ipairs(fitmentCorners) do
    local reference = ReferenceFitment()
    local factory = FactoryAlignment(descriptor.axle)
    local factorySpec = FactoryAlignmentSpecification(descriptor.axle)
    local camberSpecMin, camberSpecMax = SpecificationRange(factorySpec, "camberDegrees")
    local toeSpecMin, toeSpecMax = SpecificationRange(factorySpec, "toeInPerWheelDegrees")
    local casterSpecMin, casterSpecMax = SpecificationRange(factorySpec, "casterDegrees")
    local totalToeSpecMin, totalToeSpecMax = SpecificationRange(factorySpec, "totalToeDegrees")
    local saiSpecMin, saiSpecMax = SpecificationRange(
        factorySpec, "steeringAxisInclinationDegrees")
    local prefix = FitmentSavePrefix(descriptor.code)
    vehicleFitment.corners[descriptor.index] = {
        code = descriptor.code,
        axle = descriptor.axle,
        side = descriptor.side,
        sideSign = descriptor.sideSign,
        referenceProvenance = reference.provenance or "definition_reference",
        referenceOffsetEtMm = reference.offsetEtMm,
        installedOffsetEtMm = Save.GetNumber(
            prefix .. "installed_et_mm", reference.offsetEtMm),
        spacerThicknessMm = Save.GetNumber(prefix .. "spacer_mm", 0.0),
        rimDiameterIn = reference.rimDiameterIn,
        rimWidthIn = reference.rimWidthIn,
        tireWidthMm = reference.tireWidthMm,
        tireAspectRatio = reference.tireAspectRatio,
        tireRimDiameterIn = reference.tireRimDiameterIn,
        camberDegrees = Save.GetNumber(
            prefix .. "camber_deg", factory.camberDegrees or 0.0),
        toeInDegrees = Save.GetNumber(
            prefix .. "toe_in_deg", factory.toeInDegrees or 0.0),
        referenceCasterDegrees = factory.casterDegrees or 0.0,
        casterDegrees = Save.GetNumber(
            prefix .. "caster_deg", factory.casterDegrees or 0.0),
        casterAdjustable = factory.casterAdjustable == true,
        factoryCamberMinimumDegrees = camberSpecMin,
        factoryCamberMaximumDegrees = camberSpecMax,
        factoryToeMinimumDegrees = toeSpecMin,
        factoryToeMaximumDegrees = toeSpecMax,
        factoryCasterMinimumDegrees = casterSpecMin,
        factoryCasterMaximumDegrees = casterSpecMax,
        factoryTotalToeMinimumDegrees = totalToeSpecMin,
        factoryTotalToeMaximumDegrees = totalToeSpecMax,
        factorySaiMinimumDegrees = saiSpecMin,
        factorySaiMaximumDegrees = saiSpecMax,
        nominalRadiusM = PrototypeCarDefinition.referenceGeometry.wheelRadiusM,
        outwardCenterlineDeltaM = 0.0,
        fitmentGeometry = nil,
        assetFitmentDatumCount = 0
    }
end

local function SaveFitmentCorner(corner)
    local prefix = FitmentSavePrefix(corner.code)
    Save.SetNumber(prefix .. "installed_et_mm", corner.installedOffsetEtMm)
    Save.SetNumber(prefix .. "spacer_mm", corner.spacerThicknessMm)
    Save.SetNumber(prefix .. "camber_deg", corner.camberDegrees)
    Save.SetNumber(prefix .. "toe_in_deg", corner.toeInDegrees)
    Save.SetNumber(prefix .. "caster_deg", corner.casterDegrees)
end

local function MigrateFitmentFactoryDefaultsIfNeeded()
    local versionKey = "vehicle.setup.fitment.reference_data_version"
    local version = Save.GetNumber(versionKey, 1.0)
    if version >= 2.0 then return end

    -- FITMENT01 originally used a provisional 206-family reference. Preserve
    -- genuinely customized values, but migrate untouched legacy defaults to the
    -- midpoint of the user-supplied 206 RC MIN/MAX specification envelope.
    local legacy = {
        front = { camber = 0.0, toe = -0.116667, caster = 3.266667 },
        rear = { camber = -1.0, toe = 0.266667, caster = 0.0 }
    }
    for _, descriptor in ipairs(fitmentCorners) do
        local corner = vehicleFitment.corners[descriptor.index]
        local old = legacy[descriptor.axle]
        local currentFactory = FactoryAlignment(descriptor.axle)
        if corner ~= nil and old ~= nil then
            if math.abs(corner.camberDegrees - old.camber) < 0.00001 then
                corner.camberDegrees = currentFactory.camberDegrees or corner.camberDegrees
            end
            if math.abs(corner.toeInDegrees - old.toe) < 0.00001 then
                corner.toeInDegrees = currentFactory.toeInDegrees or corner.toeInDegrees
            end
            if math.abs(corner.casterDegrees - old.caster) < 0.00001 then
                corner.casterDegrees = currentFactory.casterDegrees or corner.casterDegrees
            end
            corner.referenceCasterDegrees = currentFactory.casterDegrees
                or corner.referenceCasterDegrees
            SaveFitmentCorner(corner)
        end
    end
    Save.SetNumber(versionKey, 2.0)
end

MigrateFitmentFactoryDefaultsIfNeeded()

-- TIRE45F: the previous prototype startup profile silently applied the midpoint
-- Peugeot alignment (front toe-out, rear -1 degree camber + toe-in). Those
-- values are useful as reference/spec evidence, but they made a neutral GLB
-- wheel look tilted/deformed even when the user had authored no camber. Migrate
-- only untouched legacy factory values; preserve any genuinely customized setup.
local function MigrateNeutralPrototypeAlignmentIfNeeded()
    local versionKey = "vehicle.setup.fitment.runtime_alignment_version"
    local version = Save.GetNumber(versionKey, 1.0)
    if version >= 2.0 then return end

    local oldFactory = {
        front = { camber = 0.0, toe = -0.060000 },
        rear = { camber = -1.0, toe = 0.260000 }
    }
    for _, descriptor in ipairs(fitmentCorners) do
        local corner = vehicleFitment.corners[descriptor.index]
        local old = oldFactory[descriptor.axle]
        if corner ~= nil and old ~= nil then
            if math.abs(corner.camberDegrees - old.camber) < 0.00001 then
                corner.camberDegrees = 0.0
            end
            if math.abs(corner.toeInDegrees - old.toe) < 0.00001 then
                corner.toeInDegrees = 0.0
            end
            SaveFitmentCorner(corner)
        end
    end
    Save.SetNumber(versionKey, 2.0)
end

MigrateNeutralPrototypeAlignmentIfNeeded()

local function NativeAlignmentSigns(corner)
    -- Friendly symmetric setup -> native local rotation convention.
    local localCamber = -corner.sideSign * corner.camberDegrees
    local localToe = -corner.sideSign * corner.toeInDegrees
    return localCamber, localToe
end

function RefreshVehicleFitmentGeometry(index)
    local corner = vehicleFitment.corners[index]
    if corner == nil or nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return nil
    end
    local geometry = Vehicle.GetWheelFitmentGeometry(nativeVehicle, index)
    if geometry ~= nil then
        corner.fitmentGeometry = geometry
    end
    return corner.fitmentGeometry
end

function ApplyVehicleFitmentCorner(index)
    local corner = vehicleFitment.corners[index]
    if corner == nil or nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end

    if not Vehicle.SetWheelFitment(
        nativeVehicle,
        index,
        true,
        corner.referenceOffsetEtMm,
        corner.installedOffsetEtMm,
        corner.spacerThicknessMm,
        corner.rimDiameterIn,
        corner.rimWidthIn,
        corner.tireWidthMm,
        corner.tireAspectRatio,
        corner.tireRimDiameterIn) then
        vehicleFitment.message = "FITMENT ERROR: " .. Vehicle.GetLastError()
        return false
    end

    local localCamber, localToe = NativeAlignmentSigns(corner)
    -- Preserve the authored/hardpoint steering axis exactly at the reference
    -- setup. Only activate the native caster override when the user actually
    -- changes caster away from that reference value.
    local casterOverride = corner.casterAdjustable
        and math.abs(corner.casterDegrees - corner.referenceCasterDegrees) > 0.0001
    if not Vehicle.SetWheelAlignment(
        nativeVehicle,
        index,
        localCamber,
        localToe,
        casterOverride,
        corner.casterDegrees) then
        vehicleFitment.message = "ALIGNMENT ERROR: " .. Vehicle.GetLastError()
        return false
    end

    local enabled, _, _, _, _, _, _, _, _, radius, outward =
        Vehicle.GetWheelFitment(nativeVehicle, index)
    if enabled ~= nil then
        corner.nominalRadiusM = radius or corner.nominalRadiusM
        corner.outwardCenterlineDeltaM = outward or 0.0
    end
    RefreshVehicleFitmentGeometry(index)
    SaveFitmentCorner(corner)
    return true
end

function ApplyVehicleFitmentSetup()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    for index = 1, #vehicleFitment.corners do
        if not ApplyVehicleFitmentCorner(index) then
            return false
        end
    end
    vehicleFitment.message = "Applied fitment/alignment; hub-face geometry resolved and suspension hardpoints unchanged"
    return true
end

local function LinkedPartnerIndex(index)
    if index == 1 and vehicleFitment.linkFront then return 2 end
    if index == 2 and vehicleFitment.linkFront then return 1 end
    if index == 3 and vehicleFitment.linkRear then return 4 end
    if index == 4 and vehicleFitment.linkRear then return 3 end
    return nil
end

function SetVehicleFitmentSelectedWheel(index)
    vehicleFitment.selectedWheel = math.max(1, math.min(4, index or 1))
end

function SetVehicleFitmentLinkFront(enabled)
    vehicleFitment.linkFront = enabled
    Save.SetBool("vehicle.setup.fitment.link_front", enabled)
end

function SetVehicleFitmentLinkRear(enabled)
    vehicleFitment.linkRear = enabled
    Save.SetBool("vehicle.setup.fitment.link_rear", enabled)
end

function SetVehicleFitmentAdvancedAlignmentRange(enabled)
    vehicleFitment.advancedAlignmentRange = enabled
    Save.SetBool("vehicle.setup.fitment.advanced_alignment_range", enabled)
end

function SetVehicleFitmentField(field, value)
    local index = vehicleFitment.selectedWheel
    local corner = vehicleFitment.corners[index]
    if corner == nil or corner[field] == nil then return false end
    if field == "casterDegrees" and not corner.casterAdjustable then return false end

    corner[field] = value
    local partnerIndex = LinkedPartnerIndex(index)
    if partnerIndex ~= nil then
        local partner = vehicleFitment.corners[partnerIndex]
        if partner ~= nil and (field ~= "casterDegrees" or partner.casterAdjustable) then
            partner[field] = value
            SaveFitmentCorner(partner)
            if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
                ApplyVehicleFitmentCorner(partnerIndex)
            end
        end
    end
    SaveFitmentCorner(corner)
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        return ApplyVehicleFitmentCorner(index)
    end
    return true
end

function ResetVehicleFitmentSetup()
    local reference = ReferenceFitment()
    for _, descriptor in ipairs(fitmentCorners) do
        local corner = vehicleFitment.corners[descriptor.index]
        local factory = FactoryAlignment(descriptor.axle)
        corner.installedOffsetEtMm = corner.referenceOffsetEtMm
            or reference.offsetEtMm
        corner.spacerThicknessMm = 0.0
        corner.camberDegrees = factory.camberDegrees or 0.0
        corner.toeInDegrees = factory.toeInDegrees or 0.0
        corner.casterDegrees = factory.casterDegrees or 0.0
        SaveFitmentCorner(corner)
    end
    ApplyVehicleFitmentSetup()
    vehicleFitment.message = "Restored factory/reference setup"
end

local function MetadataNumber(part, key, fallback)
    if part == nil or part.properties == nil then return fallback end
    return FitmentNumber(part.properties[key], fallback)
end

function VehicleFitmentImportReferenceFromMetadata(metadata)
    if metadata == nil or metadata.parts == nil then return 0 end
    local imported = 0
    for _, descriptor in ipairs(fitmentCorners) do
        local wheelPart = metadata.parts["WH_" .. descriptor.code]
        local tirePart = metadata.parts["WH_" .. descriptor.code .. "_Tire"]
        local corner = vehicleFitment.corners[descriptor.index]
        if corner ~= nil and wheelPart ~= nil and tirePart ~= nil then
            local oldReferenceEt = corner.referenceOffsetEtMm
            local wasReferenceWheel = math.abs(
                corner.installedOffsetEtMm - oldReferenceEt) < 0.0001
            corner.referenceOffsetEtMm = MetadataNumber(
                wheelPart, "wheel.offset_et_mm", corner.referenceOffsetEtMm)
            corner.rimDiameterIn = MetadataNumber(
                wheelPart, "wheel.diameter_in", corner.rimDiameterIn)
            corner.rimWidthIn = MetadataNumber(
                wheelPart, "wheel.width_in", corner.rimWidthIn)
            corner.tireWidthMm = MetadataNumber(
                tirePart, "tire.width_mm", corner.tireWidthMm)
            corner.tireAspectRatio = MetadataNumber(
                tirePart, "tire.aspect_ratio", corner.tireAspectRatio)
            corner.tireRimDiameterIn = MetadataNumber(
                tirePart, "tire.rim_diameter_in", corner.tireRimDiameterIn)
            corner.referenceProvenance = "glb_custom_properties"
            local datumCorner = descriptor.axle .. "_" .. descriptor.side
            local datums = metadata.wheel_fitment_datums or {}
            corner.assetHubFaceDatum = datums[datumCorner .. ":hub_face_center"]
            corner.assetWheelCenterlineDatum = datums[datumCorner .. ":wheel_centerline"]
            corner.assetSpinAxisDatum = datums[datumCorner .. ":wheel_spin_axis"]
            corner.assetFitmentDatumCount = 0
            if corner.assetHubFaceDatum ~= nil then corner.assetFitmentDatumCount = corner.assetFitmentDatumCount + 1 end
            if corner.assetWheelCenterlineDatum ~= nil then corner.assetFitmentDatumCount = corner.assetFitmentDatumCount + 1 end
            if corner.assetSpinAxisDatum ~= nil then corner.assetFitmentDatumCount = corner.assetFitmentDatumCount + 1 end
            if wasReferenceWheel then
                corner.installedOffsetEtMm = corner.referenceOffsetEtMm
            end
            imported = imported + 1
        end
    end
    if imported > 0 then
        if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
            ApplyVehicleFitmentSetup()
        end
        vehicleFitment.message = string.format(
            "Imported authoritative wheel/tire metadata for %d/4 corners",
            imported)
    end
    return imported
end
