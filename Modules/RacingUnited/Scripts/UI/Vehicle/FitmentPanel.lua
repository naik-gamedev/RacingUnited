-- ALIGN01 / FITMENT01: Forza-style alignment and wheel fitment setup over an
-- immutable authored reference assembly. Factory specifications are evidence,
-- never clamps: historical, oval, race, stance and damaged setups may exceed them.

local function FitmentSelectedCorner()
    return vehicleFitment.corners[vehicleFitment.selectedWheel]
end

local function FitmentCornerLabel(corner)
    if corner == nil then return "?" end
    return corner.code .. " / " .. string.upper(corner.axle)
end

local function RoundToIncrement(value, increment)
    if increment == nil or increment <= 0.0 then return value end
    local scaled = value / increment
    if scaled >= 0.0 then
        return math.floor(scaled + 0.5) * increment
    end
    return math.ceil(scaled - 0.5) * increment
end

local function ValueInRange(value, minimum, maximum)
    if minimum == nil or maximum == nil then return nil end
    return value >= minimum - 0.000001 and value <= maximum + 0.000001
end

local function FactoryRangeText(label, minimum, maximum, value)
    if minimum == nil or maximum == nil then
        UI.TextDisabled(label .. ": factory range unavailable")
        return
    end
    local inside = ValueInRange(value, minimum, maximum)
    UI.Text(string.format(
        "%s factory range: %+.3f to %+.3f deg  [%s]",
        label, minimum, maximum, inside and "IN SPEC" or "CUSTOM / OUTSIDE SPEC"))
end

local function DrawFitmentWheelSelector()
    local selected, changed = UI.InputInt(
        "Wheel (1=FL 2=FR 3=RL 4=RR)", vehicleFitment.selectedWheel, 1)
    if changed then
        SetVehicleFitmentSelectedWheel(selected)
    end
    local frontLinked, frontChanged = UI.Checkbox(
        "Link front L/R", vehicleFitment.linkFront)
    if frontChanged then SetVehicleFitmentLinkFront(frontLinked) end
    local rearLinked, rearChanged = UI.Checkbox(
        "Link rear L/R", vehicleFitment.linkRear)
    if rearChanged then SetVehicleFitmentLinkRear(rearLinked) end
    UI.TextDisabled("Disable linking for asymmetric oval/race setups.")
end

local function DrawFitmentReference(corner)
    UI.TextDisabled("REFERENCE ASSEMBLY (GLB / METADATA)")
    UI.Text("Corner: " .. FitmentCornerLabel(corner))
    UI.Text(string.format(
        "Wheel: %.1fx%.1f  reference ET %.1f mm",
        corner.rimDiameterIn, corner.rimWidthIn, corner.referenceOffsetEtMm))
    UI.Text(string.format(
        "Tire: %.0f/%.0f R%.1f  nominal radius %.2f mm",
        corner.tireWidthMm,
        corner.tireAspectRatio,
        corner.tireRimDiameterIn,
        (corner.nominalRadiusM or 0.0) * 1000.0))
    UI.TextDisabled("Source: " .. tostring(corner.referenceProvenance))
    if (corner.assetFitmentDatumCount or 0) > 0 then
        UI.Text(string.format(
            "Explicit GLB fitment datums: %d/3",
            corner.assetFitmentDatumCount or 0))
    else
        UI.TextDisabled("Explicit GLB fitment datums: none in current asset (metadata ET + reference wheel center remain authoritative)")
    end
    UI.TextWrapped("The reference wheel center and suspension hardpoints never move because you change setup values below.")
end

local function FitmentSlider(field, label, minimum, maximum, format, increment)
    local corner = FitmentSelectedCorner()
    local value, changed = UI.SliderFloat(
        label, corner[field], minimum, maximum, format)
    if changed then
        SetVehicleFitmentField(field, RoundToIncrement(value, increment))
    end
end

local function AlignmentControl(field, label, sliderMinimum, sliderMaximum,
        absoluteMinimum, absoluteMaximum)
    local corner = FitmentSelectedCorner()
    local sliderValue, sliderChanged = UI.SliderFloat(
        label .. " slider",
        corner[field], sliderMinimum, sliderMaximum, "%+.3f deg")
    if sliderChanged then
        SetVehicleFitmentField(field, RoundToIncrement(sliderValue, 0.01))
        corner = FitmentSelectedCorner()
    end

    local exactValue, exactChanged = UI.InputFloat(
        label .. " exact",
        corner[field], absoluteMinimum, absoluteMaximum,
        0.01, 0.10, "%+.4f deg")
    if exactChanged then
        -- Typed values are intentionally not quantized: 0.82, 0.8237, etc. are
        -- valid setup targets within the engine-safe range.
        SetVehicleFitmentField(field, exactValue)
    end
end

local function DrawInstalledFitment(corner)
    UI.TextDisabled("INSTALLED WHEEL FITMENT")
    FitmentSlider("installedOffsetEtMm", "Wheel offset / ET", -80.0, 100.0, "ET %.1f mm", 0.1)
    FitmentSlider("spacerThicknessMm", "Spacer thickness", 0.0, 80.0, "%.1f mm", 0.1)
    UI.Text(string.format(
        "Resolved centerline shift: %+.1f mm outward",
        (corner.outwardCenterlineDeltaM or 0.0) * 1000.0))
    UI.TextDisabled("Lower ET or a thicker spacer moves the installed wheel outward. The chassis suspension geometry stays fixed.")
end

local function DrawFitmentGeometryDiagnostics(corner)
    local geometry = RefreshVehicleFitmentGeometry(vehicleFitment.selectedWheel)
    UI.TextDisabled("FITMENT02 HUB / STEERING GEOMETRY")
    if geometry == nil or geometry.hubReferenceValid ~= true then
        UI.TextDisabled("Hub reference geometry unavailable for this wheel.")
        return
    end

    UI.Text(string.format(
        "Tire envelope from reference hub: %.1f mm inboard / %.1f mm outboard",
        geometry.inboardTireExtensionFromReferenceHubMm or 0.0,
        geometry.outboardTireExtensionFromReferenceHubMm or 0.0))
    UI.TextDisabled("Positive ET is resolved from wheel centerline to hub mounting face; spacers move the installed mounting plane outward.")

    if geometry.steeringGroundGeometryValid == true then
        UI.Text(string.format(
            "Scrub radius: %+.2f mm  |  magnitude %.2f mm",
            geometry.signedScrubRadiusMm or 0.0,
            geometry.scrubRadiusMagnitudeMm or 0.0))
        UI.Text(string.format(
            "Mechanical trail: %+.2f mm",
            geometry.mechanicalTrailMm or 0.0))
        UI.TextDisabled("These are live values from the current steering axis and road contact point, so steering, suspension travel, camber and wheel offset can change them.")
    else
        UI.TextDisabled("Scrub radius / mechanical trail require a grounded wheel and a valid steering-axis datum.")
    end

    UI.TextWrapped("Current tire width is used only as a lateral fitment envelope. FITMENT02 does not pretend the provisional Peugeot tire mesh is a factory-accurate clearance surface.")
end

local function AxleTotalToe(corner)
    if corner == nil then return nil end
    if corner.axle == "front" then
        return (vehicleFitment.corners[1].toeInDegrees or 0.0)
            + (vehicleFitment.corners[2].toeInDegrees or 0.0)
    end
    return (vehicleFitment.corners[3].toeInDegrees or 0.0)
        + (vehicleFitment.corners[4].toeInDegrees or 0.0)
end

local function DrawFactoryAlignmentSpecification(corner)
    UI.TextDisabled("PEUGEOT 206 RC FACTORY SPECIFICATION REFERENCE")
    FactoryRangeText(
        "Camber",
        corner.factoryCamberMinimumDegrees,
        corner.factoryCamberMaximumDegrees,
        corner.camberDegrees)
    FactoryRangeText(
        "Toe per wheel (+in / -out)",
        corner.factoryToeMinimumDegrees,
        corner.factoryToeMaximumDegrees,
        corner.toeInDegrees)
    if corner.casterAdjustable then
        FactoryRangeText(
            "Caster",
            corner.factoryCasterMinimumDegrees,
            corner.factoryCasterMaximumDegrees,
            corner.casterDegrees)
    end

    local totalToe = AxleTotalToe(corner)
    if corner.factoryTotalToeMinimumDegrees ~= nil
        and corner.factoryTotalToeMaximumDegrees ~= nil then
        FactoryRangeText(
            string.upper(corner.axle) .. " total toe",
            corner.factoryTotalToeMinimumDegrees,
            corner.factoryTotalToeMaximumDegrees,
            totalToe)
    end

    if corner.factorySaiMinimumDegrees ~= nil
        and corner.factorySaiMaximumDegrees ~= nil then
        local referenceSai = ((PrototypeCarDefinition.referenceAlignment or {})
            .frontSteeringAxisInclinationDegrees) or 0.0
        FactoryRangeText(
            "Steering-axis inclination (reference geometry)",
            corner.factorySaiMinimumDegrees,
            corner.factorySaiMaximumDegrees,
            referenceSai)
    end

    UI.TextDisabled(string.format(
        "Source: %s  confidence %.0f%%",
        tostring(vehicleFitment.factorySpecProvenance or "unknown"),
        (vehicleFitment.factorySpecConfidence or 0.0) * 100.0))
    UI.TextWrapped("The supplied table contains MIN/MAX limits but no standard value. Heritage uses the midpoint only as the reset/default setup; these ranges never restrict custom tuning.")
end

local function DrawAlignment(corner)
    UI.TextDisabled("ALIGNMENT SETUP")
    local advanced, advancedChanged = UI.Checkbox(
        "Advanced alignment slider range", vehicleFitment.advancedAlignmentRange)
    if advancedChanged then
        SetVehicleFitmentAdvancedAlignmentRange(advanced)
    end

    local camberSliderLimit = vehicleFitment.advancedAlignmentRange and 45.0 or 12.0
    local toeSliderLimit = vehicleFitment.advancedAlignmentRange and 20.0 or 3.0
    local casterMin = vehicleFitment.advancedAlignmentRange and -20.0 or -5.0
    local casterMax = vehicleFitment.advancedAlignmentRange and 30.0 or 15.0

    AlignmentControl(
        "camberDegrees", "Camber",
        -camberSliderLimit, camberSliderLimit, -45.0, 45.0)
    AlignmentControl(
        "toeInDegrees", "Toe (+in / -out)",
        -toeSliderLimit, toeSliderLimit, -20.0, 20.0)
    if corner.casterAdjustable then
        AlignmentControl(
            "casterDegrees", "Caster",
            casterMin, casterMax, -20.0, 30.0)
        UI.TextDisabled(string.format(
            "Reference caster: %+.3f deg (native override stays off at this value)",
            corner.referenceCasterDegrees or 0.0))
    else
        UI.TextDisabled("Caster: not adjustable for this rear suspension architecture")
    end
    UI.TextDisabled("Sliders snap to 0.01 deg. Exact boxes accept finer typed values and keep them as entered within the engine-safe range.")
    UI.TextDisabled("Camber/toe use symmetric setup signs. Heritage mirrors the native local sign automatically for the opposite side.")
end

function DrawVehicleFitmentPanel()
    SetPrototypeScenePreset("visual")
    UI.TextWrapped("FITMENT02 keeps the modeled reference assembly immutable while resolving hub mounting face, installed wheel centerline and live steering-ground geometry. ET, spacers and alignment remain per-corner setup values; suspension hardpoints never move to make aftermarket parts fit.")
    UI.Spacing()

    DrawFitmentWheelSelector()
    local corner = FitmentSelectedCorner()
    if corner == nil then return end

    UI.Spacing()
    UI.Separator()
    DrawFitmentReference(corner)

    UI.Spacing()
    UI.Separator()
    DrawInstalledFitment(corner)

    UI.Spacing()
    UI.Separator()
    DrawFitmentGeometryDiagnostics(corner)

    UI.Spacing()
    UI.Separator()
    DrawFactoryAlignmentSpecification(corner)

    UI.Spacing()
    UI.Separator()
    DrawAlignment(corner)

    UI.Spacing()
    UI.Separator()
    if UI.Button("RESET FACTORY / REFERENCE SETUP", UI.GetAvailableWidth(), 38.0, false) then
        ResetVehicleFitmentSetup()
    end
    if UI.Button("REFRESH REFERENCE FROM CURRENT GLB", UI.GetAvailableWidth(), 38.0, false) then
        if vehicleAssetMetadata ~= nil then
            VehicleFitmentImportReferenceFromMetadata(vehicleAssetMetadata)
        else
            RefreshVehicleAssetMetadata()
        end
    end
    UI.TextWrapped(tostring(vehicleFitment.message or ""))
end
