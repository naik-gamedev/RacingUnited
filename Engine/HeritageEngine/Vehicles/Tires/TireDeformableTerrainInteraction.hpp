#pragma once

#include "../VehiclePrecision.hpp"
#include "TireWear.hpp"
#include "../../Physics/CollisionSystem.hpp"
#include "../../Physics/Surfaces/SurfaceField.hpp"

namespace heritage::vehicles::tires {

// TIRE15 clean-room real-time terramechanics provider for surfaces whose
// ground reaction is dominated by terrain deformation: mud, sand, soft soil
// and deep snow. It uses a Bekker-style pressure/sinkage relation and
// Janosi-Hanamoto/Mohr-Coulomb-style shear mobilization around the pneumatic
// tire state. This is deliberately reduced-order and calibration-friendly;
// it does not simulate individual grains or claim parity with any proprietary
// commercial terrain solver.
struct TireDeformableTerrainDescription
{
    bool enabled = false;

    // Tire/tread traits only. Soil parameters belong to SurfaceMaterial /
    // SurfaceField rather than to a tire file.
    VehicleScalar treadAggressiveness = 0.20;
    VehicleScalar treadEdgeDensity = 0.28;
    VehicleScalar openVoidRatio = 0.30;
    VehicleScalar soilShearCoupling = 0.65;
    VehicleScalar bulldozingCoupling = 0.70;
    VehicleScalar plowingCoupling = 0.85;
    VehicleScalar flotationCoupling = 0.35;
    VehicleScalar minimumWornTreadEffectiveness = 0.35;
    VehicleScalar treadDepthEffectExponent = 0.80;

    VehicleScalar maximumSinkageM = 0.45;
    VehicleScalar maximumTerrainForceRatio = 1.40;
    VehicleScalar maximumPlowingForceRatio = 0.85;
    VehicleScalar minimumMfFrictionScale = 0.04;
    VehicleScalar maximumMfFrictionScale = 0.35;
};

struct TireDeformableTerrainInput
{
    bool grounded = false;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;

    bool footprintSurfaceBlendValid = false;
    VehicleScalar footprintMudFraction = 0.0;
    VehicleScalar footprintSandFraction = 0.0;
    VehicleScalar footprintSoftSoilFraction = 0.0;
    VehicleScalar footprintDeepSnowFraction = 0.0;

    heritage::physics::SurfaceFieldSample surfaceField{};
    heritage::physics::SurfaceDeformableProperties surfaceProperties{};
    bool surfacePropertiesValid = false;

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

struct TireDeformableTerrainOutput
{
    bool valid = false;
    VehicleScalar terrainSurfaceFraction = 0.0;
    VehicleScalar mudFraction = 0.0;
    VehicleScalar sandFraction = 0.0;
    VehicleScalar softSoilFraction = 0.0;
    VehicleScalar deepSnowFraction = 0.0;

    VehicleScalar contactPressurePa = 0.0;
    VehicleScalar elasticSinkageM = 0.0;
    VehicleScalar persistentRutDepthM = 0.0;
    VehicleScalar totalSinkageM = 0.0;
    VehicleScalar compaction = 0.0;
    VehicleScalar moisture = 0.0;
    VehicleScalar looseDepthM = 0.0;

    VehicleScalar effectiveCohesionPa = 0.0;
    VehicleScalar effectiveFrictionAngleDegrees = 0.0;
    VehicleScalar shearCapacityN = 0.0;
    VehicleScalar longitudinalShearMobilization = 0.0;
    VehicleScalar lateralShearMobilization = 0.0;
    VehicleScalar treadEffectiveness = 0.0;

    VehicleScalar longitudinalTerrainForceN = 0.0;
    VehicleScalar lateralTerrainForceN = 0.0;
    VehicleScalar lateralBulldozingForceN = 0.0;
    VehicleScalar plowingDragN = 0.0;
    VehicleScalar compactionPowerW = 0.0;
    VehicleScalar additionalContactCapacityN = 0.0;

    // MF remains useful for the pneumatic/contact transient state, but on
    // fully deformable terrain its direct hard-surface force contribution is
    // intentionally reduced and the terrain reaction becomes primary.
    VehicleScalar mfFrictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
    VehicleScalar relaxationScale = 1.0;

    // Persistent SurfaceField update rates/targets for the current contact.
    VehicleScalar rutDepthTargetM = 0.0;
    VehicleScalar compactionRatePerSecond = 0.0;
    VehicleScalar looseDepthLossRateMps = 0.0;
    VehicleScalar displacedVolumeRateM3ps = 0.0;
};

bool validTireDeformableTerrainDescription(
    const TireDeformableTerrainDescription& description);

heritage::physics::SurfaceFieldInitialState deformableTerrainInitialSurfaceState(
    heritage::physics::SurfaceMaterial material,
    VehicleScalar surfaceWetness = 0.0);

heritage::physics::SurfaceFieldInitialState deformableTerrainInitialSurfaceState(
    const heritage::physics::SurfaceDeformableProperties& properties,
    VehicleScalar surfaceWetness = 0.0);

TireDeformableTerrainOutput evaluateTireDeformableTerrain(
    const TireDeformableTerrainDescription& description,
    const TireDeformableTerrainInput& input);

heritage::physics::SurfaceFieldUpdate tireDeformableTerrainFieldUpdate(
    const TireDeformableTerrainDescription& description,
    const TireDeformableTerrainInput& input,
    const TireDeformableTerrainOutput& output,
    VehicleScalar deltaTimeSeconds);

} // namespace heritage::vehicles::tires
