#include "TireWinterSurfaceInteraction.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar clamp01(VehicleScalar value)
{
    return std::clamp(value, VehicleScalar{0.0}, VehicleScalar{1.0});
}

VehicleScalar smoothStep(VehicleScalar edge0, VehicleScalar edge1, VehicleScalar value)
{
    if (edge1 <= edge0)
        return value >= edge1 ? VehicleScalar{1.0} : VehicleScalar{0.0};
    const VehicleScalar t = clamp01((value - edge0) / (edge1 - edge0));
    return t * t * (VehicleScalar{3.0} - VehicleScalar{2.0} * t);
}

VehicleScalar contactSlipSpeed(const TireWinterSurfaceInput& input)
{
    return std::sqrt(
        input.longitudinalSlipVelocityMps * input.longitudinalSlipVelocityMps
        + input.lateralSlipVelocityMps * input.lateralSlipVelocityMps);
}

void winterFractions(
    const TireWinterSurfaceInput& input,
    VehicleScalar& snow,
    VehicleScalar& ice)
{
    if (input.footprintSurfaceBlendValid)
    {
        snow = clamp01(input.footprintSnowFraction);
        ice = clamp01(input.footprintIceFraction);
        const VehicleScalar total = snow + ice;
        if (total > 1.0)
        {
            snow /= total;
            ice /= total;
        }
        return;
    }

    using heritage::physics::SurfaceMaterial;
    snow = (input.surfaceMaterial == SurfaceMaterial::Snow
        || input.surfaceMaterial == SurfaceMaterial::DeepSnow)
        ? VehicleScalar{1.0} : VehicleScalar{0.0};
    ice = input.surfaceMaterial == SurfaceMaterial::Ice ? VehicleScalar{1.0} : VehicleScalar{0.0};
}

VehicleScalar contactPackedSnow(
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    const TireWearState& state)
{
    if (!state.initialized)
        return 0.0;

    TireWearInput wearInput;
    wearInput.wheelRotationDegrees = input.wheelRotationDegrees;
    wearInput.normalLoadN = input.normalLoadN;
    wearInput.nominalLoadN = std::max(input.nominalLoadN, VehicleScalar{1.0});
    wearInput.inflationPressurePa = input.inflationPressurePa;
    wearInput.referencePressurePa = input.referencePressurePa;
    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInput);

    VehicleScalar sum = 0.0;
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandWeight = weights.bands[band];
        const auto p = tireTreadCellIndex(weights.primarySector, band);
        const auto s = tireTreadCellIndex(weights.secondarySector, band);
        sum += bandWeight * (
            weights.primaryWeight * state.cells[p].packedSnowFraction
            + weights.secondaryWeight * state.cells[s].packedSnowFraction);
    }
    return clamp01(sum);
}

VehicleScalar averagePackedSnow(const TireWearState& state)
{
    if (!state.initialized)
        return 0.0;
    VehicleScalar sum = 0.0;
    for (const auto& cell : state.cells)
        sum += clamp01(cell.packedSnowFraction);
    return sum / static_cast<VehicleScalar>(kTireTreadCellCount);
}

TireWinterSurfaceOutput evaluateInternal(
    const TireWinterSurfaceDescription& d,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    const TireWearState& treadState)
{
    TireWinterSurfaceOutput out;
    if (!d.enabled || !validTireWinterSurfaceDescription(d))
        return out;

    out.valid = true;
    winterFractions(input, out.snowFraction, out.iceFraction);
    out.winterSurfaceFraction = clamp01(out.snowFraction + out.iceFraction);
    out.surfaceTemperatureC = input.surfaceTemperatureC;
    out.contactSlipSpeedMps = contactSlipSpeed(input);
    out.contactPackedSnowFraction = contactPackedSnow(
        wearDescription, input, treadState);
    out.averagePackedSnowFraction = averagePackedSnow(treadState);

    if (!input.grounded || out.winterSurfaceFraction <= 0.0)
        return out;

    const VehicleScalar treadDepthRange = std::max(
        input.initialTreadDepthM - input.minimumTreadDepthM,
        VehicleScalar{1.0e-5});
    const VehicleScalar treadDepthRatio = clamp01(
        (input.currentAverageTreadDepthM - input.minimumTreadDepthM)
        / treadDepthRange);

    const VehicleScalar winterCompound = clamp01(d.winterCompoundEffectiveness);
    const VehicleScalar siping = clamp01(d.sipingDensity);
    const VehicleScalar interlock = clamp01(d.snowTreadInterlock);

    // ICE: interpolate between colder dry ice and near-melting ice, then add
    // tire-specific compound/siping/stud contributions. Relative sliding and a
    // thin interface film further reduce non-studded rubber contact.
    const VehicleScalar warmIce = smoothStep(
        d.iceColdReferenceTemperatureC,
        d.iceNearMeltTemperatureC,
        input.surfaceTemperatureC);
    VehicleScalar iceScale = d.iceColdBaseFrictionScale
        + (d.iceNearMeltBaseFrictionScale - d.iceColdBaseFrictionScale) * warmIce;
    iceScale += d.iceWinterCompoundGain * winterCompound;
    iceScale += d.iceSipingGain * siping * (VehicleScalar{0.55} + VehicleScalar{0.45} * treadDepthRatio);

    const VehicleScalar slipFilm = smoothStep(
        VehicleScalar{0.20}, d.iceSlipSpeedReferenceMps,
        out.contactSlipSpeedMps);
    const VehicleScalar wetness = input.footprintSurfaceBlendValid
        ? clamp01(input.footprintAverageWetness)
        : clamp01(input.surfaceWetness);
    const VehicleScalar meltFilmFraction = clamp01(
        VehicleScalar{0.55} * warmIce
        + VehicleScalar{0.30} * wetness
        + d.iceFlashHeatFilmGain * warmIce * slipFilm);
    out.iceMeltFilmDepthM = d.iceMeltFilmMaximumDepthM * meltFilmFraction;
    iceScale *= VehicleScalar{1.0} - d.iceMeltFilmFrictionLoss * meltFilmFraction;

    const VehicleScalar iceSlipLoss = d.iceSlipSpeedLoss * smoothStep(
        VehicleScalar{0.0}, d.iceSlipSpeedReferenceMps,
        out.contactSlipSpeedMps);
    iceScale *= VehicleScalar{1.0} - iceSlipLoss;

    if (d.studsEnabled && d.studCount > 0 && d.studProtrusionM > 0.0)
    {
        const VehicleScalar countFactor = std::clamp(
            static_cast<VehicleScalar>(d.studCount)
                / std::max(d.studReferenceCount, VehicleScalar{1.0}),
            VehicleScalar{0.0}, VehicleScalar{1.5});
        const VehicleScalar protrusionFactor = std::clamp(
            d.studProtrusionM
                / std::max(d.studReferenceProtrusionM, VehicleScalar{1.0e-5}),
            VehicleScalar{0.0}, VehicleScalar{1.5});
        // Water film weakens rubber adhesion but does not erase mechanical stud
        // penetration. Keep the stud contribution additive and bounded.
        out.studFrictionContribution = std::min(
            d.studIceFrictionGain * std::sqrt(countFactor * protrusionFactor)
                * (VehicleScalar{0.85} + VehicleScalar{0.15} * (VehicleScalar{1.0} - warmIce)),
            d.maximumStudIceFrictionGain);
        iceScale += out.studFrictionContribution;
    }

    // COMPACTED SNOW: mechanical block/sipe interlock and retained snow in the
    // grooves raise the contact scale. A little slip can build shear/interlock,
    // while excessive slip progressively tears through the weak surface.
    const VehicleScalar snowBuild = smoothStep(
        VehicleScalar{0.0}, d.snowSlipBuildReferenceMps,
        out.contactSlipSpeedMps);
    const VehicleScalar snowHighSlip = smoothStep(
        d.snowSlipBuildReferenceMps,
        d.snowHighSlipReferenceMps,
        out.contactSlipSpeedMps);
    out.snowInterlockContribution = d.snowInterlockGain
        * interlock * std::sqrt(treadDepthRatio)
        * (VehicleScalar{0.72} + VehicleScalar{0.28} * snowBuild);
    VehicleScalar snowScale = d.snowBaseFrictionScale
        + d.snowWinterCompoundGain * winterCompound
        + d.snowSipingGain * siping * std::sqrt(treadDepthRatio)
        + out.snowInterlockContribution
        + d.snowPackedTreadGain * out.contactPackedSnowFraction;
    snowScale *= VehicleScalar{1.0}
        + d.snowSlipBuildGain * snowBuild
        - d.snowHighSlipLoss * snowHighSlip;

    iceScale = std::clamp(
        iceScale, d.minimumFrictionScale, d.maximumFrictionScale);
    snowScale = std::clamp(
        snowScale, d.minimumFrictionScale, d.maximumFrictionScale);

    const VehicleScalar otherFraction = VehicleScalar{1.0} - out.winterSurfaceFraction;
    out.frictionScale = std::clamp(
        otherFraction
            + out.snowFraction * snowScale
            + out.iceFraction * iceScale,
        d.minimumFrictionScale, VehicleScalar{1.0});
    out.stiffnessScale = std::clamp(
        otherFraction
            + out.snowFraction * d.snowStiffnessScale
            + out.iceFraction * d.iceStiffnessScale,
        VehicleScalar{0.05}, VehicleScalar{1.0});
    out.rollingResistanceScale = std::clamp(
        otherFraction
            + out.snowFraction * d.snowRollingResistanceScale
            + out.iceFraction * d.iceRollingResistanceScale,
        VehicleScalar{1.0}, VehicleScalar{5.0});
    out.relaxationScale = std::clamp(
        otherFraction
            + out.snowFraction * d.snowRelaxationScale
            + out.iceFraction * d.iceRelaxationScale,
        VehicleScalar{1.0}, VehicleScalar{4.0});

    return out;
}

void advancePackedSnowCells(
    const TireWinterSurfaceDescription& d,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& state)
{
    if (!state.initialized || deltaTimeSeconds <= 0.0)
        return;

    VehicleScalar snowFraction = 0.0;
    VehicleScalar iceFraction = 0.0;
    winterFractions(input, snowFraction, iceFraction);
    (void)iceFraction;

    const VehicleScalar slip = contactSlipSpeed(input);
    const VehicleScalar releaseRate = d.packedSnowBaseReleaseRateHz
        + d.packedSnowSpeedReleasePerM * std::abs(input.forwardSpeedMps)
        + d.packedSnowSlipReleasePerM * slip
        + d.snowSelfCleaning * VehicleScalar{0.25};
    const VehicleScalar release = std::exp(-std::max(releaseRate, VehicleScalar{0.0})
        * deltaTimeSeconds);
    for (auto& cell : state.cells)
    {
        cell.packedSnowFraction *= release;
        if (cell.packedSnowFraction < 1.0e-7)
            cell.packedSnowFraction = 0.0;
    }

    if (!input.grounded || snowFraction <= 0.0)
        return;

    TireWearInput wearInput;
    wearInput.wheelRotationDegrees = input.wheelRotationDegrees;
    wearInput.normalLoadN = input.normalLoadN;
    wearInput.nominalLoadN = std::max(input.nominalLoadN, VehicleScalar{1.0});
    wearInput.inflationPressurePa = input.inflationPressurePa;
    wearInput.referencePressurePa = input.referencePressurePa;
    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInput);

    const VehicleScalar treadRange = std::max(
        input.initialTreadDepthM - input.minimumTreadDepthM,
        VehicleScalar{1.0e-5});
    const VehicleScalar treadRatio = clamp01(
        (input.currentAverageTreadDepthM - input.minimumTreadDepthM) / treadRange);
    const VehicleScalar target = clamp01(
        snowFraction * (VehicleScalar{0.25}
            + VehicleScalar{0.75} * std::sqrt(treadRatio)
                * std::clamp(d.snowTreadInterlock, VehicleScalar{0.0}, VehicleScalar{1.0})));
    const VehicleScalar pickup = VehicleScalar{1.0}
        - std::exp(-d.packedSnowPickupRateHz * deltaTimeSeconds);

    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandWeight = weights.bands[band];
        const auto apply = [&](std::size_t sector, VehicleScalar sectorWeight) {
            auto& cell = state.cells[tireTreadCellIndex(sector, band)];
            const VehicleScalar localTarget = target
                * std::clamp(bandWeight * VehicleScalar{3.0}, VehicleScalar{0.15}, VehicleScalar{1.0});
            cell.packedSnowFraction += (localTarget - cell.packedSnowFraction)
                * pickup * sectorWeight;
            cell.packedSnowFraction = clamp01(cell.packedSnowFraction);
        };
        apply(weights.primarySector, weights.primaryWeight);
        apply(weights.secondarySector, weights.secondaryWeight);
    }
}

} // namespace

bool validTireWinterSurfaceDescription(const TireWinterSurfaceDescription& d)
{
    return finiteValue(d.winterCompoundEffectiveness)
        && d.winterCompoundEffectiveness >= 0.0 && d.winterCompoundEffectiveness <= 1.0
        && finiteValue(d.sipingDensity) && d.sipingDensity >= 0.0 && d.sipingDensity <= 1.0
        && finiteValue(d.snowTreadInterlock) && d.snowTreadInterlock >= 0.0 && d.snowTreadInterlock <= 1.0
        && finiteValue(d.snowSelfCleaning) && d.snowSelfCleaning >= 0.0 && d.snowSelfCleaning <= 1.0
        && d.studCount >= 0 && d.studCount <= 1000
        && finiteValue(d.studProtrusionM) && d.studProtrusionM >= 0.0 && d.studProtrusionM <= 0.01
        && finiteValue(d.studReferenceCount) && d.studReferenceCount > 0.0
        && finiteValue(d.studReferenceProtrusionM) && d.studReferenceProtrusionM > 0.0
        && finiteValue(d.studIceFrictionGain) && d.studIceFrictionGain >= 0.0
        && finiteValue(d.maximumStudIceFrictionGain) && d.maximumStudIceFrictionGain >= 0.0
        && finiteValue(d.iceColdReferenceTemperatureC)
        && finiteValue(d.iceNearMeltTemperatureC)
        && d.iceNearMeltTemperatureC > d.iceColdReferenceTemperatureC
        && d.iceNearMeltTemperatureC <= 1.0
        && finiteValue(d.iceColdBaseFrictionScale) && d.iceColdBaseFrictionScale >= 0.0
        && finiteValue(d.iceNearMeltBaseFrictionScale) && d.iceNearMeltBaseFrictionScale >= 0.0
        && finiteValue(d.iceSlipSpeedReferenceMps) && d.iceSlipSpeedReferenceMps > 0.0
        && finiteValue(d.iceMeltFilmMaximumDepthM) && d.iceMeltFilmMaximumDepthM >= 0.0 && d.iceMeltFilmMaximumDepthM <= 0.002
        && finiteValue(d.snowBaseFrictionScale) && d.snowBaseFrictionScale >= 0.0
        && finiteValue(d.snowSlipBuildReferenceMps) && d.snowSlipBuildReferenceMps > 0.0
        && finiteValue(d.snowHighSlipReferenceMps) && d.snowHighSlipReferenceMps > d.snowSlipBuildReferenceMps
        && finiteValue(d.iceStiffnessScale) && d.iceStiffnessScale > 0.0 && d.iceStiffnessScale <= 1.0
        && finiteValue(d.snowStiffnessScale) && d.snowStiffnessScale > 0.0 && d.snowStiffnessScale <= 1.0
        && finiteValue(d.iceRollingResistanceScale) && d.iceRollingResistanceScale >= 1.0
        && finiteValue(d.snowRollingResistanceScale) && d.snowRollingResistanceScale >= 1.0
        && finiteValue(d.iceRelaxationScale) && d.iceRelaxationScale >= 1.0
        && finiteValue(d.snowRelaxationScale) && d.snowRelaxationScale >= 1.0
        && finiteValue(d.minimumFrictionScale) && d.minimumFrictionScale >= 0.0
        && finiteValue(d.maximumFrictionScale) && d.maximumFrictionScale >= d.minimumFrictionScale
        && d.maximumFrictionScale <= 1.5
        && finiteValue(d.packedSnowPickupRateHz) && d.packedSnowPickupRateHz >= 0.0
        && finiteValue(d.packedSnowBaseReleaseRateHz) && d.packedSnowBaseReleaseRateHz >= 0.0
        && finiteValue(d.packedSnowSpeedReleasePerM) && d.packedSnowSpeedReleasePerM >= 0.0
        && finiteValue(d.packedSnowSlipReleasePerM) && d.packedSnowSlipReleasePerM >= 0.0;
}

TireWinterSurfaceOutput evaluateTireWinterSurface(
    const TireWinterSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    const TireWearState& treadState)
{
    return evaluateInternal(description, wearDescription, input, treadState);
}

TireWinterSurfaceOutput advanceTireWinterSurface(
    const TireWinterSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& treadState)
{
    if (!description.enabled || !validTireWinterSurfaceDescription(description)
        || !finiteValue(deltaTimeSeconds) || deltaTimeSeconds < 0.0)
    {
        return {};
    }

    advancePackedSnowCells(
        description, wearDescription, input, deltaTimeSeconds, treadState);
    return evaluateInternal(description, wearDescription, input, treadState);
}

} // namespace heritage::vehicles::tires
