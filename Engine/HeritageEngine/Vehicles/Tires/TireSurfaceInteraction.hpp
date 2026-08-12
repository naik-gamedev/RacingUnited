#pragma once

#include "../VehiclePrecision.hpp"
#include "TireWear.hpp"
#include "../../Physics/CollisionSystem.hpp"

#include <cstddef>

namespace heritage::vehicles::tires {

// TIRE11 clean-room tread contamination/pickup layer. It deliberately owns
// material transfer and cleaning, not the base road force law. Each of the
// 16x3 tread cells retains local pickup history while MF6.2 is still evaluated
// once per tire from the blended active-contact state.
struct TireContaminationDescription
{
    bool enabled = false;

    // First-order pickup rates [1/s] toward the material availability exposed
    // by the contacted surface. Wetness changes source composition below.
    VehicleScalar grassOrganicPickupRateHz = 2.2;
    VehicleScalar dirtMineralPickupRateHz = 1.8;
    VehicleScalar gravelFinesPickupRateHz = 1.4;
    VehicleScalar rubberPickupRateHz = 1.2;
    VehicleScalar mudFilmPickupRateHz = 2.0;

    // Clean hard surfaces progressively shed material. Speed and contact slip
    // add mechanical scrubbing; hot tread accelerates drying/release slightly.
    VehicleScalar baseHardSurfaceCleaningRateHz = 0.45;
    VehicleScalar speedCleaningRatePerM = 0.020;
    VehicleScalar slipCleaningRatePerM = 0.100;
    VehicleScalar hotTreadCleaningRatePerC = 0.0040;
    VehicleScalar hotTreadCleaningThresholdC = 55.0;

    // Retention multipliers by material. Larger values clean more slowly.
    VehicleScalar organicRetention = 0.85;
    VehicleScalar mineralRetention = 0.60;
    VehicleScalar gravelFinesRetention = 0.50;
    VehicleScalar rubberRetention = 0.75;
    VehicleScalar mudRetention = 0.95;

    // Local contact penalties. These are intentionally bounded clean-room
    // authoring coefficients; future measured datasets can replace them.
    VehicleScalar organicMaximumFrictionLoss = 0.22;
    VehicleScalar mineralMaximumFrictionLoss = 0.10;
    VehicleScalar gravelFinesMaximumFrictionLoss = 0.14;
    VehicleScalar rubberPickupMaximumFrictionLoss = 0.12;
    VehicleScalar mudMaximumFrictionLoss = 0.34;
    VehicleScalar maximumCombinedFrictionLoss = 0.48;

    // Films change the thermal contact and rolling drag even though they do
    // not trigger additional MF evaluations.
    VehicleScalar organicRoadHeatInsulation = 0.14;
    VehicleScalar mineralRoadHeatInsulation = 0.06;
    VehicleScalar gravelRoadHeatInsulation = 0.04;
    VehicleScalar rubberRoadHeatInsulation = 0.10;
    VehicleScalar mudRoadHeatInsulation = 0.32;
    VehicleScalar minimumRoadHeatTransferScale = 0.48;

    VehicleScalar organicRollingResistanceGain = 0.12;
    VehicleScalar mineralRollingResistanceGain = 0.05;
    VehicleScalar gravelRollingResistanceGain = 0.10;
    VehicleScalar rubberRollingResistanceGain = 0.04;
    VehicleScalar mudRollingResistanceGain = 0.25;
    VehicleScalar maximumRollingResistanceScale = 1.55;
};

struct TireContaminationInput
{
    bool grounded = false;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;

    // TIRE06 can provide an aggregate composition from the adaptive 2D
    // footprint so brushing one edge onto grass/gravel is visible to TIRE11
    // before the authoritative centre ray itself crosses the boundary. The
    // scalar material/wetness above remain the fallback when no footprint
    // blend is available. Fractions are normalized over supported samples.
    bool footprintSurfaceBlendValid = false;
    VehicleScalar footprintGrassFraction = 0.0;
    VehicleScalar footprintDirtFraction = 0.0;
    VehicleScalar footprintGravelFraction = 0.0;
    VehicleScalar footprintMudFraction = 0.0;
    VehicleScalar footprintSandFraction = 0.0;
    VehicleScalar footprintSoftSoilFraction = 0.0;
    VehicleScalar footprintDeepSnowFraction = 0.0;
    VehicleScalar footprintCleanHardFraction = 0.0;
    VehicleScalar footprintAverageWetness = 0.0;

    // Future dynamic-track layers can expose local rubber/marbles without
    // adding a new collision material. Zero preserves today's static scenes.
    VehicleScalar surfaceRubberDebrisFraction = 0.0;

    VehicleScalar wheelRotationDegrees = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar camberAngleRadians = 0.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar referencePressurePa = 220000.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar bulkTreadTemperatureC = 20.0;
};

struct TireContaminationOutput
{
    bool valid = false;
    std::size_t primaryContactSector = 0;

    VehicleScalar contactOrganic = 0.0;
    VehicleScalar contactMineral = 0.0;
    VehicleScalar contactGravelFines = 0.0;
    VehicleScalar contactRubberPickup = 0.0;
    VehicleScalar contactMudFilm = 0.0;
    VehicleScalar contactTotal = 0.0;

    VehicleScalar averageTotal = 0.0;
    VehicleScalar maximumCellTotal = 0.0;
    std::size_t dirtiestSector = 0;
    std::size_t dirtiestBand = 0;

    VehicleScalar contactFrictionScale = 1.0;
    VehicleScalar roadHeatTransferScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
    VehicleScalar pickupRatePerSecond = 0.0;
    VehicleScalar cleaningRatePerSecond = 0.0;
};

bool validTireContaminationDescription(
    const TireContaminationDescription& description);

TireContaminationOutput evaluateTireContamination(
    const TireContaminationDescription& description,
    const TireWearDescription& wearDescription,
    const TireContaminationInput& input,
    const TireWearState& treadState);

TireContaminationOutput advanceTireContamination(
    const TireContaminationDescription& description,
    const TireWearDescription& wearDescription,
    const TireContaminationInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& treadState);

} // namespace heritage::vehicles::tires
