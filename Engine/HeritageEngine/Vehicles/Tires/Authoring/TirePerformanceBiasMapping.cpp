#include "TirePerformanceBiasMapping.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

VehicleScalar positivePart(VehicleScalar value)
{
    return std::max(value, VehicleScalar{0.0});
}

VehicleScalar negativePart(VehicleScalar value)
{
    return std::max(-value, VehicleScalar{0.0});
}

VehicleScalar asymmetricScale(
    VehicleScalar value,
    VehicleScalar negativeAtMinusOne,
    VehicleScalar positiveAtPlusOne)
{
    const VehicleScalar positive = positivePart(value);
    const VehicleScalar negative = negativePart(value);
    return VehicleScalar{1.0}
        + positive * (positiveAtPlusOne - VehicleScalar{1.0})
        + negative * (negativeAtMinusOne - VehicleScalar{1.0});
}

VehicleScalar clampScaled(
    VehicleScalar base,
    VehicleScalar scale,
    VehicleScalar minimum,
    VehicleScalar maximum)
{
    return std::clamp(base * scale, minimum, maximum);
}

void applyDryBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    // Residual compound/carcass calibration. Peak friction is a model input,
    // not a post-solver force multiplier. Keep the range deliberately small.
    const VehicleScalar gripScale = asymmetricScale(bias, 0.94, 1.07);
    const VehicleScalar stiffnessScale = asymmetricScale(bias, 0.97, 1.04);
    model.peakFriction = clampScaled(model.peakFriction, gripScale, 0.35, 2.20);
    model.longitudinalStiffness = clampScaled(
        model.longitudinalStiffness, stiffnessScale, 1000.0, 2000000.0);
    model.corneringStiffness = clampScaled(
        model.corneringStiffness, stiffnessScale, 1000.0, 2000000.0);

    // A dry-biased compound is allowed a modestly hotter operating target;
    // a negative bias moves the target slightly colder rather than changing
    // grip by an arbitrary final-force scalar.
    model.thermal.optimumTreadTemperatureC = std::clamp(
        model.thermal.optimumTreadTemperatureC
            + positivePart(bias) * VehicleScalar{8.0}
            - negativePart(bias) * VehicleScalar{6.0},
        VehicleScalar{-20.0}, VehicleScalar{180.0});
}

void applyWetBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& wet = model.wetSurface;
    wet.treadVoidRatio = clampScaled(
        wet.treadVoidRatio,
        asymmetricScale(bias, 0.82, 1.20),
        0.0, 0.75);
    wet.drainageEfficiency = clampScaled(
        wet.drainageEfficiency,
        asymmetricScale(bias, 0.84, 1.16),
        0.02, 1.50);
    wet.drainageReferenceSpeedMps = clampScaled(
        wet.drainageReferenceSpeedMps,
        asymmetricScale(bias, 0.82, 1.20),
        0.2, 90.0);

    // Better wet-compound calibration loses less friction in thin films. The
    // hydroplaning solver remains responsible for actual contact loss.
    wet.thinFilmMaximumFrictionLoss = clampScaled(
        wet.thinFilmMaximumFrictionLoss,
        asymmetricScale(bias, 1.18, 0.82),
        0.01, 0.75);
    wet.retainedWaterReleaseRateHz = clampScaled(
        wet.retainedWaterReleaseRateHz,
        asymmetricScale(bias, 0.90, 1.12),
        0.05, 20.0);
}

void applySnowIceBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& winter = model.winterSurface;
    const VehicleScalar positive = positivePart(bias);
    const VehicleScalar negative = negativePart(bias);

    winter.winterCompoundEffectiveness = clamp01(
        winter.winterCompoundEffectiveness
        + positive * VehicleScalar{0.18}
        - negative * VehicleScalar{0.18});
    winter.sipingDensity = clamp01(
        winter.sipingDensity
        + positive * VehicleScalar{0.16}
        - negative * VehicleScalar{0.14});
    winter.snowTreadInterlock = clamp01(
        winter.snowTreadInterlock
        + positive * VehicleScalar{0.14}
        - negative * VehicleScalar{0.12});
    winter.snowSelfCleaning = clamp01(
        winter.snowSelfCleaning
        + positive * VehicleScalar{0.12}
        - negative * VehicleScalar{0.10});

    // The simple bias never invents physical studs. Explicit stud count and
    // protrusion remain engineering metadata/Advanced authoring inputs.
    model.thermal.optimumTreadTemperatureC = std::clamp(
        model.thermal.optimumTreadTemperatureC
            - positive * VehicleScalar{8.0}
            + negative * VehicleScalar{5.0},
        VehicleScalar{-20.0}, VehicleScalar{180.0});
}

void applyMudBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& terrain = model.deformableTerrainSurface;
    const VehicleScalar traitScale = asymmetricScale(bias, 0.82, 1.18);
    terrain.treadAggressiveness = clampScaled(
        terrain.treadAggressiveness, traitScale, 0.0, 2.0);
    terrain.treadEdgeDensity = clampScaled(
        terrain.treadEdgeDensity, asymmetricScale(bias, 0.86, 1.14), 0.0, 2.0);
    terrain.openVoidRatio = clampScaled(
        terrain.openVoidRatio, asymmetricScale(bias, 0.80, 1.22), 0.0, 0.95);
    terrain.soilShearCoupling = clampScaled(
        terrain.soilShearCoupling, asymmetricScale(bias, 0.86, 1.16), 0.0, 2.0);
    terrain.bulldozingCoupling = clampScaled(
        terrain.bulldozingCoupling, asymmetricScale(bias, 0.90, 1.10), 0.0, 2.0);

    // Open/self-cleaning tread retains less mud film once back on hard road.
    model.contamination.mudRetention = clampScaled(
        model.contamination.mudRetention,
        asymmetricScale(bias, 1.08, 0.82),
        0.10, 2.0);
}

void applySandBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& terrain = model.deformableTerrainSurface;
    terrain.flotationCoupling = clampScaled(
        terrain.flotationCoupling,
        asymmetricScale(bias, 0.72, 1.32),
        0.0, 2.0);
    terrain.soilShearCoupling = clampScaled(
        terrain.soilShearCoupling,
        asymmetricScale(bias, 0.90, 1.10),
        0.0, 2.0);
    terrain.openVoidRatio = clampScaled(
        terrain.openVoidRatio,
        asymmetricScale(bias, 0.90, 1.10),
        0.0, 0.95);

    // Construction bias can modestly alter vertical compliance/footprint,
    // while actual inflation pressure remains a vehicle-fitment input.
    model.contactGeometry.verticalStiffnessNPerM = clampScaled(
        model.contactGeometry.verticalStiffnessNPerM,
        asymmetricScale(bias, 1.05, 0.94),
        50000.0, 2000000.0);
}

void applyGravelBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& granular = model.shallowGranularSurface;
    granular.treadAggressiveness = clampScaled(
        granular.treadAggressiveness,
        asymmetricScale(bias, 0.82, 1.18),
        0.0, 2.0);
    granular.treadEdgeDensity = clampScaled(
        granular.treadEdgeDensity,
        asymmetricScale(bias, 0.82, 1.18),
        0.0, 2.0);
    granular.openVoidRatio = clampScaled(
        granular.openVoidRatio,
        asymmetricScale(bias, 0.84, 1.16),
        0.0, 0.95);
    granular.granularShearCoupling = clampScaled(
        granular.granularShearCoupling,
        asymmetricScale(bias, 0.84, 1.16),
        0.0, 2.0);
    granular.bulldozingCoupling = clampScaled(
        granular.bulldozingCoupling,
        asymmetricScale(bias, 0.90, 1.10),
        0.0, 2.0);
}

void applyWearEnduranceBias(
    heritage::vehicles::TireModelDescription& model,
    VehicleScalar bias)
{
    auto& wear = model.wear;
    auto& thermal = model.thermal;

    // Positive endurance means slower abrasion/shedding and wider hot margin.
    wear.wearDepthPerJoule = clampScaled(
        wear.wearDepthPerJoule,
        asymmetricScale(bias, 1.75, 0.58),
        1.0e-13, 1.0e-6);
    wear.wearTemperatureSensitivityPerC = clampScaled(
        wear.wearTemperatureSensitivityPerC,
        asymmetricScale(bias, 1.25, 0.82),
        0.0, 0.20);
    wear.rubberSheddingPropensity = clampScaled(
        wear.rubberSheddingPropensity,
        asymmetricScale(bias, 1.28, 0.76),
        0.02, 5.0);
    thermal.hotTemperatureSpanC = clampScaled(
        thermal.hotTemperatureSpanC,
        asymmetricScale(bias, 0.88, 1.16),
        5.0, 250.0);
    thermal.maximumHotFrictionLoss = clampScaled(
        thermal.maximumHotFrictionLoss,
        asymmetricScale(bias, 1.12, 0.90),
        0.02, 0.80);

    // Small compound trade-off: longer-life material sacrifices a little dry
    // peak while a short-life bias may gain a little. The wear model still
    // determines life dynamically from energy and temperature.
    model.peakFriction = clampScaled(
        model.peakFriction,
        asymmetricScale(bias, 1.035, 0.965),
        0.35, 2.20);
}

} // namespace

bool tirePerformanceBiasIsNeutral(const TirePerformanceBias& bias)
{
    return bias.dry == VehicleScalar{0.0}
        && bias.wet == VehicleScalar{0.0}
        && bias.snowIce == VehicleScalar{0.0}
        && bias.mud == VehicleScalar{0.0}
        && bias.sand == VehicleScalar{0.0}
        && bias.gravel == VehicleScalar{0.0}
        && bias.wearEndurance == VehicleScalar{0.0};
}

TirePerformanceBiasApplicationResult applyTirePerformanceBias(
    const heritage::vehicles::TireModelDescription& baseline,
    const TirePerformanceBias& bias)
{
    TirePerformanceBiasApplicationResult result;
    result.model = baseline;

    if (!validTirePerformanceBias(bias))
    {
        result.errorMessage = "Tire performance bias contains a non-finite or out-of-range value.";
        return result;
    }

    if (baseline.importedPropertyFile)
    {
        result.valid = heritage::vehicles::validTireModelDescription(result.model);
        result.authoritativeDataPreserved = true;
        if (!result.valid)
        {
            result.errorMessage = "Authoritative imported tire model is invalid.";
        }
        return result;
    }

    if (tirePerformanceBiasIsNeutral(bias))
    {
        result.valid = heritage::vehicles::validTireModelDescription(result.model);
        if (!result.valid)
        {
            result.errorMessage = "Neutral tire bias received an invalid baseline model.";
        }
        return result;
    }

    applyDryBias(result.model, bias.dry);
    applyWetBias(result.model, bias.wet);
    applySnowIceBias(result.model, bias.snowIce);
    applyMudBias(result.model, bias.mud);
    applySandBias(result.model, bias.sand);
    applyGravelBias(result.model, bias.gravel);
    applyWearEnduranceBias(result.model, bias.wearEndurance);

    if (!heritage::vehicles::validTireModelDescription(result.model))
    {
        result.errorMessage = "Creator tire bias mapping produced an invalid tire model.";
        return result;
    }

    result.model.parameterSource = "Heritage tire-family baseline + creator bias";
    result.model.parameterProvenance = "heritage_estimated_family_baseline+bias_v1";
    result.applied = true;
    result.valid = true;
    return result;
}

} // namespace heritage::vehicles::tires
