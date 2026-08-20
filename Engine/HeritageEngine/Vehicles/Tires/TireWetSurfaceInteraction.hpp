#pragma once

#include "../VehiclePrecision.hpp"
#include "TireWear.hpp"
#include "../../Physics/CollisionSystem.hpp"

namespace heritage::vehicles::tires {

// TIRE12 clean-room wet hard-surface / hydroplaning layer. MF6.2 remains the
// tire force core while this provider estimates water evacuation, hydrodynamic
// lift/contact loss and water drag from the finite footprint. The coefficients
// are Heritage authoring parameters, not proprietary Simcenter 2512 data.
struct TireWetSurfaceDescription
{
    bool enabled = false;

    // Backwards-compatible scene bridge. Existing colliders expose normalized
    // surfaceWetness; until dynamic weather/road state supplies an explicit
    // water depth, wetness=1 maps to this standing-water depth.
    VehicleScalar wetnessOneWaterDepthM = 0.0030;
    VehicleScalar minimumActiveWaterDepthM = 0.00003;
    VehicleScalar fullyWettedWaterDepthM = 0.00025;

    // Tread-groove drainage approximation. groove storage is estimated from
    // remaining tread depth * void ratio * efficiency; flow capacity then grows
    // with this reference evacuation speed and inflation pressure.
    VehicleScalar treadVoidRatio = 0.30;
    VehicleScalar drainageEfficiency = 0.82;
    VehicleScalar drainageReferenceSpeedMps = 12.0;
    VehicleScalar minimumDrainageTreadDepthM = 0.0005;

    // Hydrodynamic water-wedge model. A coefficient near 0.7 is consistent
    // with public classical hydroplaning experiments; Heritage still treats
    // this as a tunable clean-room coefficient rather than claiming equation
    // parity with any commercial wet-road model.
    VehicleScalar waterDensityKgPerM3 = 997.0;
    VehicleScalar hydrodynamicLiftCoefficient = 0.70;
    VehicleScalar hydrodynamicDragCoefficient = 0.85;
    VehicleScalar drainageOnsetRatio = 0.18;
    VehicleScalar drainageFullRatio = 1.10;
    VehicleScalar maximumHydroplaningFraction = 0.985;

    // Thin-film wet friction before meaningful lift develops, then progressive
    // contact-loss floors as hydrodynamic support displaces pavement support.
    VehicleScalar thinFilmMaximumFrictionLoss = 0.20;
    VehicleScalar thinFilmSpeedReferenceMps = 15.0;
    VehicleScalar hydroplaningFrictionFloor = 0.055;
    VehicleScalar hydroplaningStiffnessFloor = 0.10;
    VehicleScalar hydroplaningRelaxationGain = 1.50;
    VehicleScalar maximumRelaxationScale = 2.75;

    // Water plowing / rolling-loss and thermal-contact modifiers.
    VehicleScalar wetRollingResistanceGain = 0.12;
    VehicleScalar maximumRollingResistanceScale = 1.45;
    VehicleScalar wetRoadHeatTransferGain = 0.65;
    VehicleScalar maximumRoadHeatTransferScale = 1.80;

    // 48-cell retained-water state. This is cheap material history, not 48
    // hydrodynamic solvers. It lets the tire remain locally wet briefly after
    // leaving a puddle and becomes useful later for contamination/wet pickup.
    VehicleScalar retainedWaterMaximumDepthM = 0.0008;
    VehicleScalar retainedWaterPickupRateHz = 10.0;
    VehicleScalar retainedWaterReleaseRateHz = 2.5;
    VehicleScalar retainedWaterSpeedReleasePerM = 0.018;
};

struct TireWetSurfaceInput
{
    bool grounded = false;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0;
    VehicleScalar surfaceWeatherWetness = 0.0;
    // Explicit world water film. When valid this is authoritative; normalized
    // wetness remains as the backwards-compatible authored-scene bridge.
    bool surfaceWaterDepthValid = false;
    VehicleScalar surfaceWaterDepthM = 0.0;

    bool footprintSurfaceBlendValid = false;
    VehicleScalar footprintCleanHardFraction = 0.0;
    VehicleScalar footprintAverageWetness = 0.0;
    VehicleScalar footprintAverageWeatherWetness = 0.0;
    bool footprintAverageWaterDepthValid = false;
    VehicleScalar footprintAverageWaterDepthM = 0.0;

    VehicleScalar wheelRotationDegrees = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar referencePressurePa = 220000.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;

    VehicleScalar contactPatchLengthM = 0.0;
    VehicleScalar contactPatchWidthM = 0.0;
    VehicleScalar contactPatchAreaM2 = 0.0;

    VehicleScalar currentAverageTreadDepthM = 0.0070;
    VehicleScalar initialTreadDepthM = 0.0070;
    VehicleScalar minimumTreadDepthM = 0.0005;
    VehicleScalar bulkTreadTemperatureC = 20.0;
};

struct TireWetSurfaceOutput
{
    bool valid = false;

    VehicleScalar hardSurfaceFraction = 0.0;
    VehicleScalar roadWaterDepthM = 0.0;
    VehicleScalar contactRetainedWaterDepthM = 0.0;
    VehicleScalar averageRetainedWaterDepthM = 0.0;
    VehicleScalar wettedFraction = 0.0;

    VehicleScalar grooveDrainageDepthM = 0.0;
    VehicleScalar drainageDemandRatio = 0.0;
    VehicleScalar waterWedgeFraction = 0.0;

    VehicleScalar classicalPressureHydroplaningSpeedMps = 0.0;
    VehicleScalar hydrodynamicLiftN = 0.0;
    VehicleScalar hydroplaningFraction = 0.0;
    VehicleScalar pavementContactFraction = 1.0;
    VehicleScalar hydrodynamicDragN = 0.0;

    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar rollingResistanceScale = 1.0;
    VehicleScalar relaxationScale = 1.0;
    VehicleScalar roadHeatTransferScale = 1.0;
};

bool validTireWetSurfaceDescription(const TireWetSurfaceDescription& description);

TireWetSurfaceOutput evaluateTireWetSurface(
    const TireWetSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    const TireWearState& treadState);

TireWetSurfaceOutput advanceTireWetSurface(
    const TireWetSurfaceDescription& description,
    const TireWearDescription& wearDescription,
    const TireWetSurfaceInput& input,
    VehicleScalar deltaTimeSeconds,
    TireWearState& treadState);

} // namespace heritage::vehicles::tires
