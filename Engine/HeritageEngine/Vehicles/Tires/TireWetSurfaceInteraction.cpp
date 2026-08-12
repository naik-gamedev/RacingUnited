#include "TireWetSurfaceInteraction.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kHornePressureCoefficientSI = 0.04845397405472104;

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

bool hardSurface(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Default:
    case SurfaceMaterial::Asphalt:
    case SurfaceMaterial::Kerb:
    case SurfaceMaterial::PaintedLine:
        return true;
    default:
        return false;
    }
}

VehicleScalar contactSlipSpeed(const TireWetSurfaceInput& input)
{
    return std::sqrt(
        input.longitudinalSlipVelocityMps * input.longitudinalSlipVelocityMps
        + input.lateralSlipVelocityMps * input.lateralSlipVelocityMps);
}

VehicleScalar relativeWaterSpeed(const TireWetSurfaceInput& input)
{
    // Sliding raises local tire/water relative velocity; keep it bounded so a
    // pathological slip spike cannot create an unbounded hydrodynamic load.
    const VehicleScalar rolling = std::abs(input.forwardSpeedMps);
    const VehicleScalar slip = std::min(contactSlipSpeed(input), VehicleScalar{35.0});
    return std::min(rolling + VehicleScalar{0.35} * slip, VehicleScalar{120.0});
}

VehicleScalar hardFraction(const TireWetSurfaceInput& input)
{
    if (input.footprintSurfaceBlendValid)
        return clamp01(input.footprintCleanHardFraction);
    return hardSurface(input.surfaceMaterial) ? VehicleScalar{1.0} : VehicleScalar{0.0};
}

VehicleScalar mappedWaterDepth(
    const TireWetSurfaceDescription& description,
    const TireWetSurfaceInput& input)
{
    const VehicleScalar wetness = input.footprintSurfaceBlendValid
        ? clamp01(input.footprintAverageWetness)
        : clamp01(input.surfaceWetness);
    return wetness * description.wetnessOneWaterDepthM;
}

VehicleScalar averageRetainedWater(const TireWearState& state)
{
    if (!state.initialized)
        return 0.0;
    VehicleScalar sum = 0.0;
    for (const auto& cell : state.cells)
        sum += std::max(cell.retainedWaterFilmM, VehicleScalar{0.0});
    return sum / static_cast<VehicleScalar>(kTireTreadCellCount);
}

VehicleScalar contactRetainedWater(
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    const TireWearState& state)
{
    if (!state.initialized)
        return 0.0;

    TireWearInput wearInput;
    wearInput.wheelRotationDegrees = input.wheelRotationDegrees;
    wearInput.normalLoadN = input.normalLoadN;
    wearInput.nominalLoadN = std::max(input.normalLoadN, VehicleScalar{1.0});
    wearInput.inflationPressurePa = input.inflationPressurePa;
    wearInput.referencePressurePa = input.referencePressurePa;
    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInput);

    VehicleScalar sum = 0.0;
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandWeight = weights.bands[band];
        const auto primaryIndex = tireTreadCellIndex(weights.primarySector, band);
        const auto secondaryIndex = tireTreadCellIndex(weights.secondarySector, band);
        sum += bandWeight * (
            weights.primaryWeight * state.cells[primaryIndex].retainedWaterFilmM
            + weights.secondaryWeight * state.cells[secondaryIndex].retainedWaterFilmM);
    }
    return std::max(sum, VehicleScalar{0.0});
}

TireWetSurfaceOutput evaluateInternal(
    const TireWetSurfaceDescription& d,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    const TireWearState& treadState)
{
    TireWetSurfaceOutput out;
    if (!d.enabled || !validTireWetSurfaceDescription(d))
        return out;

    out.valid = true;
    out.hardSurfaceFraction = hardFraction(input);
    out.roadWaterDepthM = mappedWaterDepth(d, input) * out.hardSurfaceFraction;
    out.contactRetainedWaterDepthM = contactRetainedWater(
        wearDescription, input, treadState);
    out.averageRetainedWaterDepthM = averageRetainedWater(treadState);

    if (!input.grounded || out.hardSurfaceFraction <= 0.0)
        return out;

    const VehicleScalar effectiveWaterDepth = std::max(
        out.roadWaterDepthM,
        std::min(out.contactRetainedWaterDepthM, d.retainedWaterMaximumDepthM));
    out.wettedFraction = out.hardSurfaceFraction * smoothStep(
        d.minimumActiveWaterDepthM,
        d.fullyWettedWaterDepthM,
        effectiveWaterDepth);

    if (effectiveWaterDepth <= d.minimumActiveWaterDepthM
        || out.wettedFraction <= 0.0)
    {
        return out;
    }

    const VehicleScalar remainingTread = std::clamp(
        input.currentAverageTreadDepthM,
        std::max(d.minimumDrainageTreadDepthM, input.minimumTreadDepthM),
        std::max(input.initialTreadDepthM, d.minimumDrainageTreadDepthM));
    out.grooveDrainageDepthM = remainingTread
        * d.treadVoidRatio * d.drainageEfficiency;

    const VehicleScalar pressureRatio = std::clamp(
        input.inflationPressurePa / std::max(input.referencePressurePa, VehicleScalar{10000.0}),
        VehicleScalar{0.35}, VehicleScalar{2.5});
    const VehicleScalar drainageCapacityM2PerS =
        out.grooveDrainageDepthM * d.drainageReferenceSpeedMps
        * std::sqrt(pressureRatio);
    const VehicleScalar waterSpeed = relativeWaterSpeed(input);
    const VehicleScalar waterDemandM2PerS = effectiveWaterDepth * waterSpeed;
    out.drainageDemandRatio = waterDemandM2PerS
        / std::max(drainageCapacityM2PerS, VehicleScalar{1.0e-7});
    out.waterWedgeFraction = smoothStep(
        d.drainageOnsetRatio,
        d.drainageFullRatio,
        out.drainageDemandRatio);

    // Classical smooth-tire pressure relation retained as transparent
    // diagnostic/sanity telemetry. The actual Heritage lift fraction below is
    // computed from dynamic water pressure, finite footprint area and drainage.
    out.classicalPressureHydroplaningSpeedMps =
        kHornePressureCoefficientSI
        * std::sqrt(std::max(input.inflationPressurePa, VehicleScalar{0.0}));

    const VehicleScalar footprintArea = input.contactPatchAreaM2 > 1.0e-5
        ? input.contactPatchAreaM2
        : std::max(
            input.contactPatchLengthM * input.contactPatchWidthM,
            VehicleScalar{1.0e-4});
    const VehicleScalar dynamicWaterPressurePa = VehicleScalar{0.5}
        * d.waterDensityKgPerM3 * waterSpeed * waterSpeed;
    out.hydrodynamicLiftN = d.hydrodynamicLiftCoefficient
        * dynamicWaterPressurePa
        * footprintArea
        * out.wettedFraction
        * out.waterWedgeFraction;

    out.hydroplaningFraction = std::clamp(
        out.hydrodynamicLiftN / std::max(input.normalLoadN, VehicleScalar{1.0}),
        VehicleScalar{0.0}, d.maximumHydroplaningFraction);
    out.pavementContactFraction = VehicleScalar{1.0} - out.hydroplaningFraction;

    const VehicleScalar projectedWaterArea = std::max(
        input.contactPatchWidthM, VehicleScalar{0.05})
        * std::min(effectiveWaterDepth, VehicleScalar{0.020});
    out.hydrodynamicDragN = d.hydrodynamicDragCoefficient
        * dynamicWaterPressurePa
        * projectedWaterArea
        * out.wettedFraction
        * (VehicleScalar{0.25} + VehicleScalar{0.75} * out.waterWedgeFraction);

    const VehicleScalar speedWetBlend = smoothStep(
        VehicleScalar{0.0},
        d.thinFilmSpeedReferenceMps,
        waterSpeed);
    const VehicleScalar thinFilmScale = std::clamp(
        VehicleScalar{1.0}
            - d.thinFilmMaximumFrictionLoss
                * out.wettedFraction
                * (VehicleScalar{0.35} + VehicleScalar{0.65} * speedWetBlend),
        VehicleScalar{0.45}, VehicleScalar{1.0});
    out.frictionScale = std::clamp(
        thinFilmScale * out.pavementContactFraction
            + d.hydroplaningFrictionFloor * out.hydroplaningFraction,
        d.hydroplaningFrictionFloor, VehicleScalar{1.0});
    out.stiffnessScale = std::clamp(
        VehicleScalar{1.0} * out.pavementContactFraction
            + d.hydroplaningStiffnessFloor * out.hydroplaningFraction,
        d.hydroplaningStiffnessFloor, VehicleScalar{1.0});
    out.rollingResistanceScale = std::clamp(
        VehicleScalar{1.0}
            + d.wetRollingResistanceGain * out.wettedFraction
                * (VehicleScalar{1.0} - VehicleScalar{0.5} * out.hydroplaningFraction),
        VehicleScalar{1.0}, d.maximumRollingResistanceScale);
    out.relaxationScale = std::clamp(
        VehicleScalar{1.0}
            + d.hydroplaningRelaxationGain * out.hydroplaningFraction,
        VehicleScalar{1.0}, d.maximumRelaxationScale);
    out.roadHeatTransferScale = std::clamp(
        VehicleScalar{1.0}
            + d.wetRoadHeatTransferGain * out.wettedFraction
                * out.pavementContactFraction,
        VehicleScalar{1.0}, d.maximumRoadHeatTransferScale);

    return out;
}

void advanceRetainedWaterCells(
    const TireWetSurfaceDescription& d,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& state)
{
    if (!state.initialized || deltaTimeSeconds <= 0.0)
        return;

    TireWearInput wearInput;
    wearInput.wheelRotationDegrees = input.wheelRotationDegrees;
    wearInput.normalLoadN = input.normalLoadN;
    wearInput.nominalLoadN = std::max(input.normalLoadN, VehicleScalar{1.0});
    wearInput.inflationPressurePa = input.inflationPressurePa;
    wearInput.referencePressurePa = input.referencePressurePa;
    const TireTreadContactWeights weights = tireTreadContactWeights(
        wearDescription, wearInput);

    const VehicleScalar roadDepth = mappedWaterDepth(d, input) * hardFraction(input);
    const VehicleScalar targetDepth = input.grounded
        ? std::min(roadDepth, d.retainedWaterMaximumDepthM)
        : VehicleScalar{0.0};
    const VehicleScalar speedRelease = d.retainedWaterSpeedReleasePerM
        * std::abs(input.forwardSpeedMps);
    const VehicleScalar releaseRate = d.retainedWaterReleaseRateHz + speedRelease;

    // All exposed tread gradually sheds retained water. Current contact cells
    // additionally approach the road-water target. This remains deliberately
    // cheap: 48 scalar state updates per tire at the tire substep rate.
    for (auto& cell : state.cells)
    {
        const VehicleScalar decay = std::exp(-releaseRate * deltaTimeSeconds);
        cell.retainedWaterFilmM *= decay;
        if (cell.retainedWaterFilmM < 1.0e-8)
            cell.retainedWaterFilmM = 0.0;
    }

    if (!input.grounded || targetDepth <= 0.0)
        return;

    const VehicleScalar pickupBlend = VehicleScalar{1.0}
        - std::exp(-d.retainedWaterPickupRateHz * deltaTimeSeconds);
    for (std::size_t band = 0; band < kTireTreadBandCount; ++band)
    {
        const VehicleScalar bandWeight = weights.bands[band];
        const auto apply = [&](std::size_t sector, VehicleScalar sectorWeight) {
            auto& cell = state.cells[tireTreadCellIndex(sector, band)];
            const VehicleScalar localTarget = targetDepth
                * std::clamp(bandWeight * VehicleScalar{3.0}, VehicleScalar{0.15}, VehicleScalar{1.0});
            cell.retainedWaterFilmM += (
                localTarget - cell.retainedWaterFilmM)
                * pickupBlend * sectorWeight;
            cell.retainedWaterFilmM = std::clamp(
                cell.retainedWaterFilmM,
                VehicleScalar{0.0}, d.retainedWaterMaximumDepthM);
        };
        apply(weights.primarySector, weights.primaryWeight);
        apply(weights.secondarySector, weights.secondaryWeight);
    }
}

} // namespace

bool validTireWetSurfaceDescription(const TireWetSurfaceDescription& d)
{
    return finiteValue(d.wetnessOneWaterDepthM)
        && d.wetnessOneWaterDepthM >= 0.0 && d.wetnessOneWaterDepthM <= 0.05
        && finiteValue(d.minimumActiveWaterDepthM)
        && d.minimumActiveWaterDepthM >= 0.0
        && finiteValue(d.fullyWettedWaterDepthM)
        && d.fullyWettedWaterDepthM > d.minimumActiveWaterDepthM
        && d.fullyWettedWaterDepthM <= 0.02
        && finiteValue(d.treadVoidRatio) && d.treadVoidRatio >= 0.0 && d.treadVoidRatio <= 0.9
        && finiteValue(d.drainageEfficiency) && d.drainageEfficiency >= 0.0 && d.drainageEfficiency <= 2.0
        && finiteValue(d.drainageReferenceSpeedMps) && d.drainageReferenceSpeedMps > 0.1 && d.drainageReferenceSpeedMps <= 100.0
        && finiteValue(d.minimumDrainageTreadDepthM) && d.minimumDrainageTreadDepthM >= 0.0 && d.minimumDrainageTreadDepthM <= 0.02
        && finiteValue(d.waterDensityKgPerM3) && d.waterDensityKgPerM3 >= 500.0 && d.waterDensityKgPerM3 <= 1500.0
        && finiteValue(d.hydrodynamicLiftCoefficient) && d.hydrodynamicLiftCoefficient >= 0.0 && d.hydrodynamicLiftCoefficient <= 2.0
        && finiteValue(d.hydrodynamicDragCoefficient) && d.hydrodynamicDragCoefficient >= 0.0 && d.hydrodynamicDragCoefficient <= 3.0
        && finiteValue(d.drainageOnsetRatio) && d.drainageOnsetRatio >= 0.0
        && finiteValue(d.drainageFullRatio) && d.drainageFullRatio > d.drainageOnsetRatio
        && finiteValue(d.maximumHydroplaningFraction) && d.maximumHydroplaningFraction >= 0.0 && d.maximumHydroplaningFraction < 1.0
        && finiteValue(d.thinFilmMaximumFrictionLoss) && d.thinFilmMaximumFrictionLoss >= 0.0 && d.thinFilmMaximumFrictionLoss <= 0.8
        && finiteValue(d.thinFilmSpeedReferenceMps) && d.thinFilmSpeedReferenceMps > 0.1 && d.thinFilmSpeedReferenceMps <= 100.0
        && finiteValue(d.hydroplaningFrictionFloor) && d.hydroplaningFrictionFloor >= 0.0 && d.hydroplaningFrictionFloor <= 1.0
        && finiteValue(d.hydroplaningStiffnessFloor) && d.hydroplaningStiffnessFloor >= 0.0 && d.hydroplaningStiffnessFloor <= 1.0
        && finiteValue(d.hydroplaningRelaxationGain) && d.hydroplaningRelaxationGain >= 0.0 && d.hydroplaningRelaxationGain <= 10.0
        && finiteValue(d.maximumRelaxationScale) && d.maximumRelaxationScale >= 1.0 && d.maximumRelaxationScale <= 10.0
        && finiteValue(d.wetRollingResistanceGain) && d.wetRollingResistanceGain >= 0.0 && d.wetRollingResistanceGain <= 2.0
        && finiteValue(d.maximumRollingResistanceScale) && d.maximumRollingResistanceScale >= 1.0 && d.maximumRollingResistanceScale <= 5.0
        && finiteValue(d.wetRoadHeatTransferGain) && d.wetRoadHeatTransferGain >= 0.0 && d.wetRoadHeatTransferGain <= 5.0
        && finiteValue(d.maximumRoadHeatTransferScale) && d.maximumRoadHeatTransferScale >= 1.0 && d.maximumRoadHeatTransferScale <= 5.0
        && finiteValue(d.retainedWaterMaximumDepthM) && d.retainedWaterMaximumDepthM >= 0.0 && d.retainedWaterMaximumDepthM <= 0.01
        && finiteValue(d.retainedWaterPickupRateHz) && d.retainedWaterPickupRateHz >= 0.0 && d.retainedWaterPickupRateHz <= 1000.0
        && finiteValue(d.retainedWaterReleaseRateHz) && d.retainedWaterReleaseRateHz >= 0.0 && d.retainedWaterReleaseRateHz <= 1000.0
        && finiteValue(d.retainedWaterSpeedReleasePerM) && d.retainedWaterSpeedReleasePerM >= 0.0 && d.retainedWaterSpeedReleasePerM <= 10.0;
}

TireWetSurfaceOutput evaluateTireWetSurface(
    const TireWetSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    const TireWearState& treadState)
{
    return evaluateInternal(description, wearDescription, input, treadState);
}

TireWetSurfaceOutput advanceTireWetSurface(
    const TireWetSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& treadState)
{
    if (!description.enabled || !validTireWetSurfaceDescription(description)
        || !finiteValue(deltaTimeSeconds) || deltaTimeSeconds < 0.0)
    {
        return {};
    }

    advanceRetainedWaterCells(
        description, wearDescription, input, deltaTimeSeconds, treadState);
    return evaluateInternal(description, wearDescription, input, treadState);
}

} // namespace heritage::vehicles::tires
