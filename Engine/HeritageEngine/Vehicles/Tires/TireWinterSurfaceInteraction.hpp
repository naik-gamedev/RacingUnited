#pragma once

#include "../VehiclePrecision.hpp"
#include "TireWear.hpp"
#include "../../Physics/CollisionSystem.hpp"

namespace heritage::vehicles::tires {

// TIRE13 clean-room compacted-snow / hard-ice interaction layer. These are
// weakly/non-deformable winter surfaces, so MF6.2 remains the tire force core
// and this provider supplies surface-dependent friction/stiffness/relaxation
// modifiers plus cheap tread snow-packing state. Deep snow is deliberately
// deferred to TIRE15 terramechanics.
struct TireWinterSurfaceDescription
{
    bool enabled = false;

    // Tire traits. These are authoring/fitting inputs, not direct "snow grip"
    // numbers. The provider combines them with surface state to obtain the
    // final scale applied around one MF6.2 evaluation.
    VehicleScalar winterCompoundEffectiveness = 0.10; // 0 summer .. 1 winter
    VehicleScalar sipingDensity = 0.08;                // normalized effective density
    VehicleScalar snowTreadInterlock = 0.28;           // block/void mechanical engagement
    VehicleScalar snowSelfCleaning = 0.30;             // release of packed snow from grooves

    // Optional studs. Count/protrusion are explicit so a future authoring tool
    // can populate them from real tire specifications rather than a vague
    // "studded grip multiplier".
    bool studsEnabled = false;
    int studCount = 0;
    VehicleScalar studProtrusionM = 0.0;
    VehicleScalar studReferenceCount = 120.0;
    VehicleScalar studReferenceProtrusionM = 0.0012;
    VehicleScalar studIceFrictionGain = 0.24;
    VehicleScalar maximumStudIceFrictionGain = 0.32;

    // Hard-ice response. Public testing shows that "ice" is not one constant
    // friction surface; temperature and surface condition matter strongly.
    // Heritage uses a transparent clean-room curve rather than claiming parity
    // with any proprietary winter-tire model.
    VehicleScalar iceColdReferenceTemperatureC = -15.0;
    VehicleScalar iceNearMeltTemperatureC = -0.5;
    VehicleScalar iceColdBaseFrictionScale = 0.15;
    VehicleScalar iceNearMeltBaseFrictionScale = 0.075;
    VehicleScalar iceWinterCompoundGain = 0.045;
    VehicleScalar iceSipingGain = 0.055;
    VehicleScalar iceSlipSpeedLoss = 0.30;
    VehicleScalar iceSlipSpeedReferenceMps = 3.0;
    VehicleScalar iceMeltFilmMaximumDepthM = 0.00012;
    VehicleScalar iceMeltFilmFrictionLoss = 0.42;
    VehicleScalar iceFlashHeatFilmGain = 0.30;

    // Compacted-snow response. Tread blocks/sipes can mechanically interlock
    // with the snow and a modest snow-on-snow contribution is retained in the
    // 48 tread cells. This is not the deep-snow sinkage model.
    VehicleScalar snowBaseFrictionScale = 0.24;
    VehicleScalar snowWinterCompoundGain = 0.060;
    VehicleScalar snowSipingGain = 0.045;
    VehicleScalar snowInterlockGain = 0.16;
    VehicleScalar snowPackedTreadGain = 0.055;
    VehicleScalar snowSlipBuildGain = 0.08;
    VehicleScalar snowHighSlipLoss = 0.12;
    VehicleScalar snowSlipBuildReferenceMps = 0.8;
    VehicleScalar snowHighSlipReferenceMps = 4.0;

    // Structural response around the force core.
    VehicleScalar iceStiffnessScale = 0.28;
    VehicleScalar snowStiffnessScale = 0.42;
    VehicleScalar iceRollingResistanceScale = 1.35;
    VehicleScalar snowRollingResistanceScale = 2.65;
    VehicleScalar iceRelaxationScale = 1.85;
    VehicleScalar snowRelaxationScale = 1.65;
    VehicleScalar minimumFrictionScale = 0.035;
    VehicleScalar maximumFrictionScale = 0.72;

    // 48-cell packed-snow state. Snow enters the current contact cells, while
    // rotation, slip and self-cleaning progressively release it after leaving
    // snow. This is cheap state/history, not extra MF evaluations.
    VehicleScalar packedSnowPickupRateHz = 4.5;
    VehicleScalar packedSnowBaseReleaseRateHz = 0.55;
    VehicleScalar packedSnowSpeedReleasePerM = 0.012;
    VehicleScalar packedSnowSlipReleasePerM = 0.050;
};

struct TireWinterSurfaceInput
{
    bool grounded = false;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;

    bool footprintSurfaceBlendValid = false;
    VehicleScalar footprintSnowFraction = 0.0;
    VehicleScalar footprintIceFraction = 0.0;
    VehicleScalar footprintAverageWetness = 0.0;

    VehicleScalar wheelRotationDegrees = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar referencePressurePa = 220000.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar currentAverageTreadDepthM = 0.0070;
    VehicleScalar initialTreadDepthM = 0.0070;
    VehicleScalar minimumTreadDepthM = 0.0005;

    // Until the future scene SurfaceField supplies local dynamic temperature,
    // static scenes use this explicit compatibility input. The provider itself
    // is already temperature-dependent and does not hard-code one ice value.
    VehicleScalar surfaceTemperatureC = -5.0;
    VehicleScalar bulkTreadTemperatureC = 20.0;
};

struct TireWinterSurfaceOutput
{
    bool valid = false;
    VehicleScalar snowFraction = 0.0;
    VehicleScalar iceFraction = 0.0;
    VehicleScalar winterSurfaceFraction = 0.0;
    VehicleScalar surfaceTemperatureC = -5.0;
    VehicleScalar contactSlipSpeedMps = 0.0;

    VehicleScalar contactPackedSnowFraction = 0.0;
    VehicleScalar averagePackedSnowFraction = 0.0;
    VehicleScalar iceMeltFilmDepthM = 0.0;
    VehicleScalar studFrictionContribution = 0.0;
    VehicleScalar snowInterlockContribution = 0.0;

    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
    VehicleScalar relaxationScale = 1.0;
};

bool validTireWinterSurfaceDescription(
    const TireWinterSurfaceDescription& description);

TireWinterSurfaceOutput evaluateTireWinterSurface(
    const TireWinterSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    const TireWearState& treadState);

TireWinterSurfaceOutput advanceTireWinterSurface(
    const TireWinterSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWinterSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& treadState);

} // namespace heritage::vehicles::tires
