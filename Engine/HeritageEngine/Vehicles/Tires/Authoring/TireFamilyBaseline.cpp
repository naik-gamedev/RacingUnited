#include "TireFamilyBaseline.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

bool finite(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

void enableSharedSurfaceState(heritage::vehicles::TireModelDescription& model)
{
    model.thermal.enabled = true;
    model.failure.enabled = true;
    model.wear.enabled = true;
    model.contamination.enabled = true;
    model.wetSurface.enabled = true;
    model.winterSurface.enabled = true;
    model.shallowGranularSurface.enabled = true;
    model.deformableTerrainSurface.enabled = true;
}

void configureCommonGeometry(
    heritage::vehicles::TireModelDescription& model,
    const TireFamilyBaselineInput& input)
{
    const VehicleScalar sidewallHeightM = input.sectionWidthM * input.aspectRatio;
    const VehicleScalar unloadedRadiusM = input.rimRadiusM + sidewallHeightM;

    model.nominalLoad = input.nominalLoadN;
    model.referenceInflationPressurePa = input.inflationPressurePa;
    model.inflationPressurePa = input.inflationPressurePa;
    model.thermal.referenceGaugePressurePa = input.inflationPressurePa;
    model.failure.containedAirVolumeM3 = estimatedTireContainedAirVolumeM3(
        unloadedRadiusM, input.sectionWidthM, input.rimRadiusM);
    model.contactGeometry.unloadedRadiusM = unloadedRadiusM;
    model.contactGeometry.nominalLoadN = input.nominalLoadN;
    model.contactGeometry.nominalWidthM = input.sectionWidthM;
    model.contactGeometry.rimRadiusM = input.rimRadiusM;
    model.contactGeometry.referenceSpeedMps = 16.6666666667;
    model.contactGeometry.verticalStiffnessNPerM = std::clamp(
        input.nominalLoadN / std::max(sidewallHeightM * 0.18, VehicleScalar{0.008}),
        VehicleScalar{80000.0},
        VehicleScalar{1500000.0});

    model.motorcycleProfile.tireWidthM = input.sectionWidthM;
    model.parameterSource = "Heritage tire-family baseline";
    model.parameterProvenance = "heritage_estimated_family_baseline";
    model.parameterConfidence = 0.15;
}

void configureFamily(
    heritage::vehicles::TireModelDescription& m,
    TireFamily family)
{
    // These are deliberately conservative clean-room authoring baselines, not
    // claims about any manufacturer/product. TIRE18 calibration may refine the
    // values; fitted/measured property files remain authoritative.
    switch (family)
    {
    case TireFamily::RoadSummerPerformance:
        m.peakFriction = 1.12;
        m.thermal.optimumTreadTemperatureC = 70.0;
        m.wear.initialTreadDepthM = 0.0070;
        m.wear.rubberSheddingPropensity = 0.85;
        m.wetSurface.treadVoidRatio = 0.30;
        m.wetSurface.drainageEfficiency = 0.82;
        m.winterSurface.winterCompoundEffectiveness = 0.10;
        break;

    case TireFamily::RacingSlick:
        m.peakFriction = 1.38;
        m.thermal.optimumTreadTemperatureC = 92.0;
        m.wear.initialTreadDepthM = 0.0030;
        m.wear.rubberSheddingPropensity = 1.45;
        m.wetSurface.treadVoidRatio = 0.015;
        m.wetSurface.drainageEfficiency = 0.10;
        m.winterSurface.winterCompoundEffectiveness = 0.02;
        m.shallowGranularSurface.openVoidRatio = 0.02;
        m.deformableTerrainSurface.openVoidRatio = 0.02;
        break;

    case TireFamily::RacingWet:
        m.peakFriction = 1.24;
        m.thermal.optimumTreadTemperatureC = 65.0;
        m.wear.initialTreadDepthM = 0.0060;
        m.wear.rubberSheddingPropensity = 1.20;
        m.wetSurface.treadVoidRatio = 0.40;
        m.wetSurface.drainageEfficiency = 0.96;
        m.wetSurface.drainageReferenceSpeedMps = 17.0;
        m.winterSurface.winterCompoundEffectiveness = 0.08;
        break;

    case TireFamily::Winter:
        m.peakFriction = 1.00;
        m.thermal.optimumTreadTemperatureC = 35.0;
        m.wear.initialTreadDepthM = 0.0090;
        m.wear.rubberSheddingPropensity = 0.75;
        m.wetSurface.treadVoidRatio = 0.34;
        m.wetSurface.drainageEfficiency = 0.86;
        m.winterSurface.winterCompoundEffectiveness = 0.86;
        m.winterSurface.sipingDensity = 0.74;
        m.winterSurface.snowTreadInterlock = 0.68;
        m.winterSurface.snowSelfCleaning = 0.68;
        break;

    case TireFamily::StuddedIce:
        m.peakFriction = 0.96;
        m.thermal.optimumTreadTemperatureC = 30.0;
        m.wear.initialTreadDepthM = 0.0100;
        m.wear.rubberSheddingPropensity = 0.72;
        m.wetSurface.treadVoidRatio = 0.35;
        m.winterSurface.winterCompoundEffectiveness = 0.92;
        m.winterSurface.sipingDensity = 0.80;
        m.winterSurface.snowTreadInterlock = 0.72;
        m.winterSurface.studsEnabled = true;
        m.winterSurface.studCount = 120;
        m.winterSurface.studProtrusionM = 0.0012;
        break;

    case TireFamily::RallyGravel:
        m.peakFriction = 1.08;
        m.thermal.optimumTreadTemperatureC = 60.0;
        m.wear.initialTreadDepthM = 0.0100;
        m.wear.rubberSheddingPropensity = 1.00;
        m.wetSurface.treadVoidRatio = 0.38;
        m.shallowGranularSurface.treadAggressiveness = 0.72;
        m.shallowGranularSurface.treadEdgeDensity = 0.72;
        m.shallowGranularSurface.openVoidRatio = 0.46;
        m.shallowGranularSurface.granularShearCoupling = 0.84;
        m.deformableTerrainSurface.treadAggressiveness = 0.62;
        m.deformableTerrainSurface.openVoidRatio = 0.46;
        break;

    case TireFamily::Motorcycle:
        m.provider = heritage::vehicles::TireProviderKind::MagicFormula62Motorcycle;
        m.peakFriction = 1.18;
        m.thermal.optimumTreadTemperatureC = 70.0;
        m.wear.initialTreadDepthM = 0.0050;
        m.wear.rubberSheddingPropensity = 0.90;
        m.motorcycleProfile.mcContourA = 0.50;
        m.motorcycleProfile.mcContourB = 0.50;
        break;

    case TireFamily::Kart:
        m.peakFriction = 1.30;
        m.thermal.optimumTreadTemperatureC = 78.0;
        m.wear.initialTreadDepthM = 0.0030;
        m.wear.rubberSheddingPropensity = 1.25;
        m.wetSurface.treadVoidRatio = 0.02;
        m.wetSurface.drainageEfficiency = 0.14;
        break;

    case TireFamily::CommercialTruck:
        m.peakFriction = 0.92;
        m.thermal.optimumTreadTemperatureC = 65.0;
        m.wear.initialTreadDepthM = 0.0140;
        m.wear.rubberSheddingPropensity = 0.50;
        m.wetSurface.treadVoidRatio = 0.28;
        m.wetSurface.drainageEfficiency = 0.76;
        m.shallowGranularSurface.treadAggressiveness = 0.26;
        m.deformableTerrainSurface.flotationCoupling = 0.42;
        break;

    case TireFamily::LowPressureOffRoad:
        m.peakFriction = 0.94;
        m.thermal.optimumTreadTemperatureC = 48.0;
        m.wear.initialTreadDepthM = 0.0150;
        m.wear.rubberSheddingPropensity = 0.70;
        m.wetSurface.treadVoidRatio = 0.48;
        m.shallowGranularSurface.treadAggressiveness = 0.82;
        m.shallowGranularSurface.treadEdgeDensity = 0.68;
        m.shallowGranularSurface.openVoidRatio = 0.56;
        m.shallowGranularSurface.granularShearCoupling = 0.90;
        m.deformableTerrainSurface.treadAggressiveness = 0.88;
        m.deformableTerrainSurface.treadEdgeDensity = 0.72;
        m.deformableTerrainSurface.openVoidRatio = 0.62;
        m.deformableTerrainSurface.soilShearCoupling = 0.92;
        m.deformableTerrainSurface.flotationCoupling = 0.78;
        break;
    }
}

} // namespace

bool validTireFamilyBaselineInput(const TireFamilyBaselineInput& input)
{
    return finite(input.sectionWidthM)
        && input.sectionWidthM >= 0.08
        && input.sectionWidthM <= 0.80
        && finite(input.aspectRatio)
        && input.aspectRatio >= 0.15
        && input.aspectRatio <= 1.20
        && finite(input.rimRadiusM)
        && input.rimRadiusM >= 0.08
        && input.rimRadiusM <= 0.60
        && finite(input.nominalLoadN)
        && input.nominalLoadN >= 100.0
        && input.nominalLoadN <= 100000.0
        && finite(input.inflationPressurePa)
        && input.inflationPressurePa >= 20000.0
        && input.inflationPressurePa <= 2000000.0;
}

TireFamilyBaselineResult buildTireFamilyBaseline(
    TireFamily family,
    const TireFamilyBaselineInput& input,
    const heritage::vehicles::TireModelDescription& seed)
{
    return buildTireFamilyBaseline(family, input, TirePerformanceBias{}, seed);
}

TireFamilyBaselineResult buildTireFamilyBaseline(
    TireFamily family,
    const TireFamilyBaselineInput& input,
    const TirePerformanceBias& performanceBias,
    const heritage::vehicles::TireModelDescription& seed)
{
    TireFamilyBaselineResult result;
    if (!validTireFamily(family))
    {
        result.errorMessage = "Unknown tire family.";
        return result;
    }
    if (!validTireFamilyBaselineInput(input))
    {
        result.errorMessage = "Tire-family baseline dimensions/load/pressure are outside supported bounds.";
        return result;
    }

    result.model = seed;
    enableSharedSurfaceState(result.model);
    configureCommonGeometry(result.model, input);
    configureFamily(result.model, family);

    // Keep the coarse seed stiffness coherent with physical scale. These are
    // generator inputs for the MF seed bridge, not final force multipliers.
    const VehicleScalar loadRatio = input.nominalLoadN / 3500.0;
    const VehicleScalar widthRatio = input.sectionWidthM / 0.205;
    result.model.longitudinalStiffness = std::clamp(
        90000.0 * loadRatio * std::sqrt(widthRatio), 10000.0, 1200000.0);
    result.model.corneringStiffness = std::clamp(
        80000.0 * loadRatio * std::sqrt(widthRatio), 10000.0, 1200000.0);

    const TirePerformanceBiasApplicationResult biasResult =
        applyTirePerformanceBias(result.model, performanceBias);
    if (!biasResult.valid)
    {
        result.errorMessage = biasResult.errorMessage;
        return result;
    }
    result.model = biasResult.model;

    if (!heritage::vehicles::validTireModelDescription(result.model))
    {
        result.errorMessage = "Generated family baseline failed TireModelDescription validation.";
        return result;
    }

    result.valid = true;
    return result;
}

} // namespace heritage::vehicles::tires
