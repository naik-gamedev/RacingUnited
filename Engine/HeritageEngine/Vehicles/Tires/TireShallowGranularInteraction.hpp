#pragma once

#include "../VehiclePrecision.hpp"
#include "TireWear.hpp"
#include "../../Physics/CollisionSystem.hpp"

namespace heritage::vehicles::tires {

// TIRE14 clean-room shallow-granular interaction for rally gravel and hard
// dirt. These surfaces still have a load-bearing base, so MF6.2/SWIFT remains
// the pneumatic-tire force core. This provider adds the loose-layer physics
// around it: shallow sinkage, Mohr-Coulomb/Janosi-style shear mobilization,
// lateral bulldozing and longitudinal plowing/compaction resistance.
//
// TIRE15 will own fully deformable terrain (mud, sand, deep snow, soft soil)
// with persistent SurfaceField rut/compaction state. TIRE14 deliberately does
// not pretend that the static collision mesh can remember terrain deformation.
struct TireShallowGranularDescription
{
    bool enabled = false;

    // Tire/tread engineering traits. These are mechanism inputs rather than
    // direct gravel/dirt grip multipliers and can eventually be authored from
    // measured data or estimated from a sufficiently accurate tread model.
    VehicleScalar treadAggressiveness = 0.18;      // normalized block/lug aggressiveness
    VehicleScalar treadEdgeDensity = 0.25;         // normalized biting-edge density
    VehicleScalar openVoidRatio = 0.28;            // normalized open tread void fraction
    VehicleScalar granularShearCoupling = 0.60;    // tread-to-particle shear engagement
    VehicleScalar bulldozingCoupling = 0.55;       // sidewall/shoulder passive-wedge coupling
    VehicleScalar plowingCoupling = 0.75;          // longitudinal loose-layer displacement
    VehicleScalar minimumWornTreadEffectiveness = 0.42;
    VehicleScalar treadDepthEffectExponent = 0.70;

    // Safety/transition bounds for the reduced-order hybrid model. These keep
    // the provider stable with synthetic or incomplete development datasets.
    VehicleScalar maximumSinkageM = 0.045;
    VehicleScalar maximumGranularForceRatio = 0.65; // additional shear/bulldozing vs Fz
    VehicleScalar maximumPlowingForceRatio = 0.35;  // resistive drag vs Fz
    VehicleScalar minimumBaseFrictionScale = 0.22;
    VehicleScalar maximumBaseFrictionScale = 0.72;
};

struct TireShallowGranularInput
{
    bool grounded = false;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;

    bool footprintSurfaceBlendValid = false;
    VehicleScalar footprintGravelFraction = 0.0;
    VehicleScalar footprintDirtFraction = 0.0;
    VehicleScalar footprintAverageWetness = 0.0;

    VehicleScalar normalLoadN = 0.0;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar slipRatio = 0.0;
    VehicleScalar slipAngleRadians = 0.0;

    VehicleScalar unloadedRadiusM = 0.30;
    VehicleScalar contactPatchLengthM = 0.0;
    VehicleScalar contactPatchWidthM = 0.0;
    VehicleScalar contactPatchAreaM2 = 0.0;

    VehicleScalar currentAverageTreadDepthM = 0.0070;
    VehicleScalar initialTreadDepthM = 0.0070;
    VehicleScalar minimumTreadDepthM = 0.0005;
};

struct TireShallowGranularOutput
{
    bool valid = false;
    VehicleScalar gravelFraction = 0.0;
    VehicleScalar dirtFraction = 0.0;
    VehicleScalar granularSurfaceFraction = 0.0;
    VehicleScalar surfaceWetness = 0.0;

    // Instantaneous shallow-layer geometry/soil state. Sinkage is measured
    // downward from the authored surface top toward the load-bearing base.
    VehicleScalar looseLayerDepthM = 0.0;
    VehicleScalar sinkageM = 0.0;
    VehicleScalar sinkageFraction = 0.0;
    VehicleScalar contactPressurePa = 0.0;
    VehicleScalar effectiveCohesionPa = 0.0;
    VehicleScalar effectiveFrictionAngleDegrees = 0.0;

    // Janosi/Hanamoto-style shear mobilization state.
    VehicleScalar longitudinalShearDisplacementM = 0.0;
    VehicleScalar lateralShearDisplacementM = 0.0;
    VehicleScalar longitudinalShearMobilization = 0.0;
    VehicleScalar lateralShearMobilization = 0.0;
    VehicleScalar soilShearCapacityN = 0.0;
    VehicleScalar treadEffectiveness = 0.0;

    // Additional terrain forces around the single MF6.2 tire evaluation.
    VehicleScalar longitudinalShearForceN = 0.0;
    VehicleScalar lateralShearForceN = 0.0;
    VehicleScalar lateralBulldozingForceN = 0.0;
    VehicleScalar plowingDragN = 0.0;
    VehicleScalar compactionPowerW = 0.0;
    VehicleScalar additionalContactCapacityN = 0.0;

    // MF/base-contact and transient modifiers. Partial split-surface contact
    // is blended back toward 1.0 using granularSurfaceFraction.
    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
    VehicleScalar relaxationScale = 1.0;
};

bool validTireShallowGranularDescription(
    const TireShallowGranularDescription& description);

TireShallowGranularOutput evaluateTireShallowGranular(
    const TireShallowGranularDescription& description,
    const TireShallowGranularInput& input);

} // namespace heritage::vehicles::tires
