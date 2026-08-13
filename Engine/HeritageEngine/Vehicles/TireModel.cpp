#include "TireModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-6;

bool finiteFloat(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar generalizedTireCurve(
    VehicleScalar slip,
    VehicleScalar stiffness,
    VehicleScalar peakForce,
    VehicleScalar shapeFactor,
    VehicleScalar curvatureFactor)
{
    if (peakForce <= kEpsilon || stiffness <= kEpsilon)
        return 0.0;

    const VehicleScalar B = stiffness
        / std::max(shapeFactor * peakForce, kEpsilon);
    const VehicleScalar Bx = B * slip;
    const VehicleScalar inner = Bx - curvatureFactor * (Bx - std::atan(Bx));
    return peakForce * std::sin(shapeFactor * std::atan(inner));
}

VehicleScalar lpNorm(VehicleScalar x, VehicleScalar y, VehicleScalar exponent)
{
    const VehicleScalar ax = std::abs(x);
    const VehicleScalar ay = std::abs(y);
    return std::pow(
        std::pow(ax, exponent) + std::pow(ay, exponent),
        1.0 / exponent);
}

TireForceResult evaluateLegacyGeneralizedRoadTire(
    const TireModelDescription& description,
    const TireContactInput& input)
{
    TireForceResult result;
    const VehicleScalar loadRatio = std::max(
        input.normalLoad / description.nominalLoad,
        0.05);
    result.effectiveFriction = std::clamp(
        description.peakFriction
            * std::max(input.frictionMultiplier, 0.0)
            * std::pow(loadRatio, -description.loadSensitivity),
        0.02,
        5.0);

    const VehicleScalar peakForce = result.effectiveFriction * input.normalLoad;
    if (peakForce <= kEpsilon)
        return result;

    const VehicleScalar stiffnessLoadScale = std::pow(
        loadRatio,
        description.stiffnessLoadExponent);
    const VehicleScalar stiffnessMultiplier = std::max(
        input.stiffnessMultiplier,
        0.0);
    const VehicleScalar longitudinalStiffness = description.longitudinalStiffness
        * stiffnessMultiplier * stiffnessLoadScale;
    const VehicleScalar corneringStiffness = description.corneringStiffness
        * stiffnessMultiplier * stiffnessLoadScale;

    result.pureLongitudinalForce = generalizedTireCurve(
        input.longitudinalSlip,
        longitudinalStiffness,
        peakForce,
        description.longitudinalShapeFactor,
        description.longitudinalCurvatureFactor);
    result.pureLateralForce = -generalizedTireCurve(
        input.slipAngleRadians,
        corneringStiffness,
        peakForce,
        description.lateralShapeFactor,
        description.lateralCurvatureFactor);

    const VehicleScalar normalizedLongitudinal = result.pureLongitudinalForce
        / peakForce;
    const VehicleScalar normalizedLateral = result.pureLateralForce / peakForce;
    const VehicleScalar requestedUtilization = lpNorm(
        normalizedLongitudinal,
        normalizedLateral,
        description.combinedSlipExponent);

    result.combinedSlipScale = requestedUtilization > 1.0
        ? 1.0 / requestedUtilization
        : 1.0;
    result.longitudinalForce = result.pureLongitudinalForce
        * result.combinedSlipScale;
    result.lateralForce = result.pureLateralForce
        * result.combinedSlipScale;
    result.gripUtilization = std::clamp(
        lpNorm(
            result.longitudinalForce / peakForce,
            result.lateralForce / peakForce,
            description.combinedSlipExponent),
        0.0,
        1.0);

    const VehicleScalar characteristicAngle = std::max(
        peakForce / std::max(corneringStiffness, kEpsilon),
        0.005);
    const VehicleScalar lateralSaturation = std::abs(input.slipAngleRadians)
        / characteristicAngle;
    const VehicleScalar longitudinalUse = std::clamp(
        std::abs(result.longitudinalForce) / peakForce,
        0.0,
        1.0);
    result.pneumaticTrail = description.pneumaticTrail
        * std::exp(-description.pneumaticTrailFalloff * lateralSaturation)
        * (1.0 - 0.35 * longitudinalUse);
    result.aligningTorque = -result.lateralForce * result.pneumaticTrail;
    result.longitudinalSlipStiffness = longitudinalStiffness;
    result.corneringStiffness = corneringStiffness;
    return result;
}

} // namespace

bool validTireModelDescription(const TireModelDescription& value)
{
    const bool legacyValid = finiteFloat(value.nominalLoad)
        && value.nominalLoad >= 100.0
        && value.nominalLoad <= 100000.0
        && finiteFloat(value.peakFriction)
        && value.peakFriction >= 0.02
        && value.peakFriction <= 5.0
        && finiteFloat(value.longitudinalStiffness)
        && value.longitudinalStiffness >= 100.0
        && value.longitudinalStiffness <= 2000000.0
        && finiteFloat(value.corneringStiffness)
        && value.corneringStiffness >= 100.0
        && value.corneringStiffness <= 2000000.0
        && finiteFloat(value.loadSensitivity)
        && value.loadSensitivity >= 0.0
        && value.loadSensitivity <= 0.75
        && finiteFloat(value.longitudinalRelaxationLength)
        && value.longitudinalRelaxationLength >= 0.01
        && value.longitudinalRelaxationLength <= 10.0
        && finiteFloat(value.lateralRelaxationLength)
        && value.lateralRelaxationLength >= 0.01
        && value.lateralRelaxationLength <= 10.0
        && finiteFloat(value.wheelInertia)
        && value.wheelInertia >= 0.01
        && value.wheelInertia <= 100.0
        && finiteFloat(value.pneumaticTrail)
        && value.pneumaticTrail >= 0.0
        && value.pneumaticTrail <= 1.0
        && finiteFloat(value.stiffnessLoadExponent)
        && value.stiffnessLoadExponent >= 0.20
        && value.stiffnessLoadExponent <= 1.50
        && finiteFloat(value.longitudinalShapeFactor)
        && value.longitudinalShapeFactor >= 0.80
        && value.longitudinalShapeFactor <= 2.00
        && finiteFloat(value.lateralShapeFactor)
        && value.lateralShapeFactor >= 0.80
        && value.lateralShapeFactor <= 2.00
        && finiteFloat(value.longitudinalCurvatureFactor)
        && value.longitudinalCurvatureFactor >= -2.0
        && value.longitudinalCurvatureFactor <= 0.99
        && finiteFloat(value.lateralCurvatureFactor)
        && value.lateralCurvatureFactor >= -2.0
        && value.lateralCurvatureFactor <= 0.99
        && finiteFloat(value.combinedSlipExponent)
        && value.combinedSlipExponent >= 1.10
        && value.combinedSlipExponent <= 6.0
        && finiteFloat(value.pneumaticTrailFalloff)
        && value.pneumaticTrailFalloff >= 0.0
        && value.pneumaticTrailFalloff <= 10.0
        && finiteFloat(value.referenceInflationPressurePa)
        && value.referenceInflationPressurePa >= 20000.0
        && value.referenceInflationPressurePa <= 2000000.0
        && finiteFloat(value.inflationPressurePa)
        && value.inflationPressurePa >= 0.0
        && value.inflationPressurePa <= 2000000.0;

    if (!legacyValid)
        return false;

    if (value.thermal.enabled
        && !tires::validTireThermalDescription(value.thermal))
        return false;
    if (value.wear.enabled
        && !tires::validTireWearDescription(value.wear))
        return false;
    if (value.contamination.enabled
        && (!value.wear.enabled
            || !tires::validTireContaminationDescription(value.contamination)))
        return false;

    if (value.wetSurface.enabled
        && (!value.wear.enabled
            || !tires::validTireWetSurfaceDescription(value.wetSurface)))
        return false;
    if (value.winterSurface.enabled
        && (!value.wear.enabled
            || !tires::validTireWinterSurfaceDescription(value.winterSurface)))
        return false;
    if (value.shallowGranularSurface.enabled
        && !tires::validTireShallowGranularDescription(value.shallowGranularSurface))
        return false;
    if (value.deformableTerrainSurface.enabled
        && !tires::validTireDeformableTerrainDescription(value.deformableTerrainSurface))
        return false;

    if (value.provider == TireProviderKind::LegacyGeneralizedRoad)
        return true;

    if (value.rigidRing.enabled
        && !tires::validTireRigidRingDescription(value.rigidRing))
        return false;
    if (value.roadEnveloping.enabled
        && !tires::validTireRoadEnvelopingDescription(value.roadEnveloping))
        return false;

    if (!value.magicFormulaUsesLegacySeed
        && !tires::validMagicFormula62Parameters(value.magicFormula))
    {
        return false;
    }

    if (value.provider == TireProviderKind::MagicFormula62Motorcycle)
    {
        const VehicleScalar radius = value.magicFormulaUsesLegacySeed
            ? std::max(value.magicFormula.unloadedRadiusM, VehicleScalar{0.05})
            : value.magicFormula.unloadedRadiusM;
        return tires::validMotorcycleTireProfile(value.motorcycleProfile, radius);
    }

    return true;
}

tires::MagicFormula62Parameters seededMagicFormula62Parameters(
    const TireModelDescription& d,
    VehicleScalar wheelRadiusM)
{
    tires::MagicFormula62Parameters p = d.magicFormula;
    const VehicleScalar fz0 = std::max(d.nominalLoad, VehicleScalar{100.0});
    p.nominalLoadN = fz0;
    p.nominalPressurePa = d.referenceInflationPressurePa;
    p.unloadedRadiusM = wheelRadiusM > 0.05
        ? wheelRadiusM
        : std::max(p.unloadedRadiusM, VehicleScalar{0.05});

    p.pCx1 = d.longitudinalShapeFactor;
    p.pDx1 = d.peakFriction;
    p.pDx2 = -d.peakFriction * d.loadSensitivity;
    p.pEx1 = d.longitudinalCurvatureFactor;
    p.pKx1 = d.longitudinalStiffness / fz0;
    p.pKx2 = p.pKx1 * (d.stiffnessLoadExponent - 1.0);

    p.pCy1 = d.lateralShapeFactor;
    p.pDy1 = d.peakFriction;
    p.pDy2 = -d.peakFriction * d.loadSensitivity;
    p.pEy1 = d.lateralCurvatureFactor;

    // At nominal load the public MF lateral stiffness expression is
    // PKY1*FZ0*sin(PKY4*atan(1/PKY2)). Solve PKY1 so the bridge preserves the
    // long-standing Heritage cornering-stiffness control at that datum.
    const VehicleScalar lateralShape = std::sin(
        p.pKy4 * std::atan(1.0 / std::max(p.pKy2, VehicleScalar{0.01})));
    p.pKy1 = d.corneringStiffness
        / std::max(fz0 * std::abs(lateralShape), kEpsilon);

    // Preserve the old nominal pneumatic trail as the MF trail amplitude.
    p.qDz1 = d.pneumaticTrail / std::max(p.unloadedRadiusM, VehicleScalar{0.05});
    return p;
}

TireModelDescription tireModelDescriptionFromPropertyFile(
    const tires::TirePropertyFileData& propertyFile,
    TireProviderKind provider,
    const std::string& source,
    const std::string& provenance,
    VehicleScalar confidence,
    const TireModelDescription& fallback)
{
    TireModelDescription value = fallback;
    value.provider = provider;
    if (provider != TireProviderKind::LegacyGeneralizedRoad
        && propertyFile.hasMotorcycleContour)
    {
        value.provider = TireProviderKind::MagicFormula62Motorcycle;
    }

    value.magicFormulaUsesLegacySeed = false;
    value.magicFormula = propertyFile.magicFormula;
    value.referenceInflationPressurePa =
        propertyFile.magicFormula.nominalPressurePa > 0.0
            ? propertyFile.magicFormula.nominalPressurePa
            : fallback.referenceInflationPressurePa;
    value.inflationPressurePa = propertyFile.inflationPressurePa > 0.0
        ? propertyFile.inflationPressurePa
        : propertyFile.magicFormula.nominalPressurePa;
    value.nominalLoad = propertyFile.magicFormula.nominalLoadN;
    value.slipDynamicsCoefficients.pTx1 = propertyFile.pTx1;
    value.slipDynamicsCoefficients.pTx2 = propertyFile.pTx2;
    value.slipDynamicsCoefficients.pTx3 = propertyFile.pTx3;
    value.slipDynamicsCoefficients.pTy1 = propertyFile.pTy1;
    value.slipDynamicsCoefficients.pTy2 = propertyFile.pTy2;
    value.slipDynamicsCoefficients.lSgKappa = propertyFile.lSgKappa;
    value.slipDynamicsCoefficients.lSgAlpha = propertyFile.lSgAlpha;

    value.contactGeometry.unloadedRadiusM =
        propertyFile.magicFormula.unloadedRadiusM;
    value.contactGeometry.nominalLoadN =
        propertyFile.magicFormula.nominalLoadN;
    value.contactGeometry.verticalStiffnessNPerM =
        propertyFile.verticalStiffnessNPerM;
    value.contactGeometry.nominalWidthM = propertyFile.widthM;
    value.contactGeometry.rimRadiusM = propertyFile.rimRadiusM;
    value.contactGeometry.referenceSpeedMps =
        propertyFile.magicFormula.referenceSpeedMps;
    value.contactGeometry.useMagicFormulaEffectiveRadius =
        propertyFile.hasEffectiveRollingRadiusModel;
    value.contactGeometry.bReff = propertyFile.bReff;
    value.contactGeometry.dReff = propertyFile.dReff;
    value.contactGeometry.fReff = propertyFile.fReff;
    value.contactGeometry.qRe0 = propertyFile.qRe0;
    value.contactGeometry.qV1 = propertyFile.qV1;
    value.contactGeometry.useMagicFormulaContactLength =
        propertyFile.hasContactPatchLengthModel;
    value.contactGeometry.qRa1 = propertyFile.qRa1;
    value.contactGeometry.qRa2 = propertyFile.qRa2;
    value.contactGeometry.qRb1 = propertyFile.qRb1;
    value.contactGeometry.qRb2 = propertyFile.qRb2;

    value.rigidRing.enabled = propertyFile.hasRigidRingModel;
    value.rigidRing.longitudinalStiffnessNPerM =
        propertyFile.structuralLongitudinalStiffnessNPerM;
    value.rigidRing.lateralStiffnessNPerM =
        propertyFile.structuralLateralStiffnessNPerM;
    value.rigidRing.yawStiffnessNmPerRad =
        propertyFile.structuralYawStiffnessNmPerRad;
    value.rigidRing.longitudinalFrequencyHz =
        propertyFile.structuralFrequencyLongHz;
    value.rigidRing.lateralFrequencyHz =
        propertyFile.structuralFrequencyLatHz;
    value.rigidRing.radialFrequencyHz =
        propertyFile.structuralFrequencyLongHz;
    value.rigidRing.yawFrequencyHz =
        propertyFile.structuralFrequencyYawHz;
    value.rigidRing.windupFrequencyHz =
        propertyFile.structuralFrequencyWindupHz;
    value.rigidRing.longitudinalDampingRatio =
        propertyFile.structuralDampingLong;
    value.rigidRing.lateralDampingRatio =
        propertyFile.structuralDampingLat;
    value.rigidRing.radialDampingRatio =
        propertyFile.structuralDampingLong;
    value.rigidRing.yawDampingRatio =
        propertyFile.structuralDampingYaw;
    value.rigidRing.windupDampingRatio =
        propertyFile.structuralDampingWindup;
    value.rigidRing.residualDampingRatio =
        propertyFile.structuralResidualDamping;
    value.rigidRing.lowSpeedAdditionalDampingRatio =
        propertyFile.structuralLowSpeedDamping;
    value.rigidRing.lowSpeedDampingScale =
        propertyFile.rigidRingLowSpeedDampingScale > 0.0
            ? propertyFile.rigidRingLowSpeedDampingScale : VehicleScalar{1.0};
    value.rigidRing.lowSpeedThresholdMps =
        propertyFile.rigidRingLowSpeedThresholdMps > 0.05
            ? propertyFile.rigidRingLowSpeedThresholdMps : VehicleScalar{1.0};
    value.rigidRing.beltMassKg = propertyFile.beltMassKg;
    value.rigidRing.beltDiametralInertiaKgM2 =
        propertyFile.beltDiametralInertiaKgM2;
    value.rigidRing.beltPolarInertiaKgM2 =
        propertyFile.beltPolarInertiaKgM2;

    value.roadEnveloping.enabled = propertyFile.hasRoadEnvelopingModel;
    value.roadEnveloping.ellipseShiftScale = propertyFile.ellipseShiftScale;
    value.roadEnveloping.ellipseLengthM = propertyFile.ellipseLengthM;
    value.roadEnveloping.ellipseHeightM = propertyFile.ellipseHeightM;
    value.roadEnveloping.ellipseOrder = propertyFile.ellipseOrder;
    value.roadEnveloping.maximumRoadStepM = propertyFile.ellipseMaximumStepM;
    value.roadEnveloping.widthCamCount = std::max(propertyFile.ellipseWidthCount, 1);
    value.roadEnveloping.sideCamCount = std::max(propertyFile.ellipseSideCount, 1);
    value.roadEnveloping.effectiveHeightAttenuation =
        propertyFile.envelopeHeightAttenuation;
    value.roadEnveloping.effectivePlaneAngleAttenuation =
        propertyFile.envelopePlaneAngleAttenuation;

    value.thermal = propertyFile.thermal;
    value.thermal.enabled = propertyFile.hasHeritageThermalModel;
    value.thermal.referenceGaugePressurePa = value.inflationPressurePa;
    value.wear = propertyFile.wear;
    value.wear.enabled = propertyFile.hasHeritageTreadState;
    value.contamination = propertyFile.contamination;
    value.contamination.enabled = propertyFile.hasHeritageContaminationModel;

    value.wetSurface = propertyFile.wetSurface;
    value.wetSurface.enabled = propertyFile.hasHeritageWetSurfaceModel;
    value.winterSurface = propertyFile.winterSurface;
    value.winterSurface.enabled = propertyFile.hasHeritageWinterSurfaceModel;
    value.shallowGranularSurface = propertyFile.shallowGranularSurface;
    value.shallowGranularSurface.enabled = propertyFile.hasHeritageShallowGranularModel;
    value.deformableTerrainSurface = propertyFile.deformableTerrainSurface;
    value.deformableTerrainSurface.enabled = propertyFile.hasHeritageDeformableTerrainModel;

    // Keep the compatibility/debug controls coherent with the imported datum.
    // They are no longer used by the MF evaluator once magicFormulaUsesLegacySeed
    // is false, but the existing Workshop UI can still display sensible values.
    value.peakFriction = std::clamp(
        std::max(propertyFile.magicFormula.pDx1, propertyFile.magicFormula.pDy1),
        VehicleScalar{0.02}, VehicleScalar{5.0});
    value.longitudinalShapeFactor = std::clamp(
        propertyFile.magicFormula.pCx1, VehicleScalar{0.80}, VehicleScalar{2.00});
    value.lateralShapeFactor = std::clamp(
        propertyFile.magicFormula.pCy1, VehicleScalar{0.80}, VehicleScalar{2.00});
    value.longitudinalCurvatureFactor = std::clamp(
        propertyFile.magicFormula.pEx1, VehicleScalar{-2.0}, VehicleScalar{0.99});
    value.lateralCurvatureFactor = std::clamp(
        propertyFile.magicFormula.pEy1, VehicleScalar{-2.0}, VehicleScalar{0.99});
    value.longitudinalStiffness = std::clamp(
        std::abs(propertyFile.magicFormula.pKx1 * value.nominalLoad),
        VehicleScalar{100.0}, VehicleScalar{2000000.0});
    const VehicleScalar kyShape = std::sin(
        propertyFile.magicFormula.pKy4
        * std::atan(1.0 / std::max(
            std::abs(propertyFile.magicFormula.pKy2), VehicleScalar{0.01})));
    value.corneringStiffness = std::clamp(
        std::abs(propertyFile.magicFormula.pKy1 * value.nominalLoad * kyShape),
        VehicleScalar{100.0}, VehicleScalar{2000000.0});

    if (propertyFile.hasMotorcycleContour)
        value.motorcycleProfile = propertyFile.motorcycleProfile;

    value.importedPropertyFile = true;
    value.importedFitType = propertyFile.fitType;
    value.parameterSource = source;
    value.parameterProvenance = provenance;
    value.parameterTireSide = propertyFile.tireSide;
    value.parameterConfidence = std::clamp(confidence, VehicleScalar{0.0}, VehicleScalar{1.0});
    value.importedMappedParameterCount = propertyFile.mappedAssignmentCount;
    value.importedUnsupportedParameterCount = propertyFile.unsupportedAssignmentCount;
    return value;
}

TireForceResult evaluateAdvancedRoadTire(
    const TireModelDescription& description,
    const TireContactInput& input)
{
    TireForceResult result;
    if (input.normalLoad <= kEpsilon || !validTireModelDescription(description))
        return result;

    if (description.provider == TireProviderKind::LegacyGeneralizedRoad)
        return evaluateLegacyGeneralizedRoadTire(description, input);

    tires::MagicFormula62Parameters p = description.magicFormulaUsesLegacySeed
        ? seededMagicFormula62Parameters(description, input.wheelRadiusM)
        : description.magicFormula;
    if (input.wheelRadiusM > 0.05)
        p.unloadedRadiusM = input.wheelRadiusM;

    tires::MagicFormula62Input mfInput;
    mfInput.normalLoadN = input.normalLoad;
    mfInput.longitudinalSlip = input.longitudinalSlip;
    mfInput.slipAngleRadians = input.slipAngleRadians;
    mfInput.camberAngleRadians = input.camberAngleRadians;
    const VehicleScalar actualInflationPressurePa =
        input.inflationPressurePa >= 0.0
        ? input.inflationPressurePa
        : description.inflationPressurePa;
    mfInput.inflationPressurePa = std::clamp(
        actualInflationPressurePa,
        p.minimumPressurePa,
        p.maximumPressurePa);
    mfInput.forwardSpeedMps = input.forwardSpeedMps;
    mfInput.turnSlipPerM = input.turnSlipPerM;
    mfInput.frictionScale = std::max(input.frictionMultiplier, 0.0);
    mfInput.stiffnessScale = std::max(input.stiffnessMultiplier, 0.0);

    const tires::MagicFormula62Result mf = tires::evaluateMagicFormula62(
        p,
        mfInput);
    result.longitudinalForce = mf.longitudinalForceN;
    result.lateralForce = mf.lateralForceN;
    result.pureLongitudinalForce = mf.pureLongitudinalForceN;
    result.pureLateralForce = mf.pureLateralForceN;
    result.effectiveFriction = std::max(
        mf.longitudinalFrictionCoefficient,
        mf.lateralFrictionCoefficient);
    result.pneumaticTrail = mf.pneumaticTrailM;
    result.turnSlipMoment = mf.turnSlipMomentNm;
    result.aligningTorque = mf.aligningMomentNm
        + input.contactPatchTurnMomentNm;
    result.overturningMoment = mf.overturningMomentNm;
    result.rollingResistanceMoment = mf.rollingResistanceMomentNm;
    result.residualAligningTorque = mf.residualAligningMomentNm;
    result.longitudinalSlipStiffness = mf.longitudinalSlipStiffnessN;
    result.corneringStiffness = mf.corneringStiffnessNPerRad;
    result.camberStiffness = mf.camberStiffnessNPerRad;
    result.combinedLongitudinalWeight = mf.combinedLongitudinalWeight;
    result.combinedLateralWeight = mf.combinedLateralWeight;
    result.normalizedTurnSlip = mf.normalizedTurnSlip;
    result.turnSlipLongitudinalReduction = mf.turnSlipLongitudinalReduction;
    result.turnSlipLateralReduction = mf.turnSlipLateralReduction;
    result.turnSlipCorneringReduction = mf.turnSlipCorneringReduction;
    result.turnSlipTrailReduction = mf.turnSlipTrailReduction;

    // MF coefficients are evaluated only inside their identified pressure
    // range. Below that range, a separate carcass-serviceability envelope
    // represents the loss of belt control in a flat tire without extrapolating
    // the fitted polynomial to zero pressure. A flat casing retains some raw
    // rubber friction, but only a small part of its intended force/moment
    // capability and directional stiffness.
    const VehicleScalar lowPressureT = std::clamp(
        actualInflationPressurePa
            / std::max(p.minimumPressurePa, VehicleScalar{1.0}),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar lowPressureSmooth = lowPressureT * lowPressureT
        * (VehicleScalar{3.0} - VehicleScalar{2.0} * lowPressureT);
    const VehicleScalar forceServiceability = VehicleScalar{0.35}
        + VehicleScalar{0.65} * lowPressureSmooth;
    const VehicleScalar stiffnessServiceability = VehicleScalar{0.08}
        + VehicleScalar{0.92} * lowPressureSmooth;
    result.longitudinalForce *= forceServiceability;
    result.lateralForce *= forceServiceability;
    result.pureLongitudinalForce *= forceServiceability;
    result.pureLateralForce *= forceServiceability;
    result.effectiveFriction *= forceServiceability;
    result.pneumaticTrail *= stiffnessServiceability;
    result.turnSlipMoment *= stiffnessServiceability;
    result.aligningTorque *= stiffnessServiceability;
    result.overturningMoment *= stiffnessServiceability;
    result.residualAligningTorque *= stiffnessServiceability;
    result.longitudinalSlipStiffness *= stiffnessServiceability;
    result.corneringStiffness *= stiffnessServiceability;
    result.camberStiffness *= stiffnessServiceability;

    const VehicleScalar forceLimit = std::max(
        result.effectiveFriction * input.normalLoad,
        kEpsilon);
    const VehicleScalar pureMagnitude = std::hypot(
        result.pureLongitudinalForce,
        result.pureLateralForce);
    const VehicleScalar combinedMagnitude = std::hypot(
        result.longitudinalForce,
        result.lateralForce);
    result.combinedSlipScale = pureMagnitude > kEpsilon
        ? std::clamp(combinedMagnitude / pureMagnitude, 0.0, 1.0)
        : 1.0;
    result.gripUtilization = std::clamp(
        combinedMagnitude / forceLimit,
        0.0,
        1.0);

    if (description.provider == TireProviderKind::MagicFormula62Motorcycle)
    {
        const auto contour = tires::evaluateMotorcycleTireProfile(
            description.motorcycleProfile,
            p.unloadedRadiusM,
            input.camberAngleRadians);
        result.motorcycleContourValid = contour.valid;
        result.motorcycleContactLateralOffset = contour.lateralContactOffsetM;
        result.motorcycleCenterToRoad = contour.centerToRoadM;
    }

    return result;
}

} // namespace heritage::vehicles
