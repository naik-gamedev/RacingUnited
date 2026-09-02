#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// TIRE46 final reduced-order tire thermal network.
//
// Heritage intentionally does not clone proprietary MF-Tyre T&V equations. The
// public steady-state MF force core is coupled to an independently authored
// energy network whose states are physically interpretable and cheap enough for
// large grids. TIRE46 extends the former four-node model into a seven-node
// construction model: tread surface bulk, belt/undertread, carcass, inner and
// outer sidewalls, contained gas, and wheel/rim. The existing 16x3 tread field
// still carries material-fixed surface hot spots on top of the bulk tread node.
struct TireThermalDescription
{
    bool enabled = false;

    VehicleScalar referenceTemperatureC = 20.0;
    VehicleScalar initialTreadTemperatureC = 20.0;
    VehicleScalar initialBeltTemperatureC = 20.0;
    VehicleScalar initialCarcassTemperatureC = 20.0;
    VehicleScalar initialInnerSidewallTemperatureC = 20.0;
    VehicleScalar initialOuterSidewallTemperatureC = 20.0;
    VehicleScalar initialGasTemperatureC = 20.0;
    VehicleScalar initialRimTemperatureC = 20.0;
    VehicleScalar ambientTemperatureC = 20.0;
    VehicleScalar roadTemperatureC = 20.0;
    VehicleScalar ambientPressurePa = 101325.0;
    VehicleScalar referenceGaugePressurePa = 220000.0;

    // Lumped heat capacities [J/K].
    VehicleScalar treadHeatCapacityJPerK = 4200.0;
    VehicleScalar beltHeatCapacityJPerK = 3200.0;
    VehicleScalar carcassHeatCapacityJPerK = 7200.0;
    VehicleScalar innerSidewallHeatCapacityJPerK = 2400.0;
    VehicleScalar outerSidewallHeatCapacityJPerK = 2400.0;
    VehicleScalar gasHeatCapacityJPerK = 220.0;
    VehicleScalar rimHeatCapacityJPerK = 18000.0;

    // Thermal conductances [W/K]. Speed-dependent terms are added to the air
    // paths using resultant vehicle/world-air speed in m/s.
    VehicleScalar treadToBeltConductanceWPerK = 78.0;
    // Retained direct path for construction flexibility and old property files.
    VehicleScalar treadToCarcassConductanceWPerK = 18.0;
    VehicleScalar beltToCarcassConductanceWPerK = 42.0;
    VehicleScalar carcassToInnerSidewallConductanceWPerK = 26.0;
    VehicleScalar carcassToOuterSidewallConductanceWPerK = 26.0;
    VehicleScalar treadToRoadConductanceWPerK = 90.0;
    VehicleScalar treadToAirConductanceWPerK = 10.0;
    VehicleScalar carcassToAirConductanceWPerK = 8.0;
    VehicleScalar innerSidewallToAirConductanceWPerK = 6.0;
    VehicleScalar outerSidewallToAirConductanceWPerK = 6.0;
    VehicleScalar carcassToGasConductanceWPerK = 7.0;
    VehicleScalar innerSidewallToGasConductanceWPerK = 2.0;
    VehicleScalar outerSidewallToGasConductanceWPerK = 2.0;
    VehicleScalar gasToAmbientConductanceWPerK = 2.0;
    VehicleScalar carcassToRimConductanceWPerK = 7.0;
    VehicleScalar innerSidewallToRimConductanceWPerK = 4.0;
    VehicleScalar outerSidewallToRimConductanceWPerK = 4.0;
    VehicleScalar rimToAirConductanceWPerK = 12.0;
    VehicleScalar treadAirSpeedConductanceWPerKPerMps = 0.70;
    VehicleScalar carcassAirSpeedConductanceWPerKPerMps = 0.35;
    VehicleScalar sidewallAirSpeedConductanceWPerKPerMps = 0.42;
    VehicleScalar rimAirSpeedConductanceWPerKPerMps = 0.65;

    // Frictional slip power is primarily created in the tread/contact region.
    // Radial/rolling losses heat belt/carcass/sidewalls according to construction.
    VehicleScalar slipHeatFractionToTread = 0.85;
    VehicleScalar slipHeatFractionToBelt = 0.08;
    VehicleScalar slipHeatEfficiency = 0.92;
    VehicleScalar carcassLossHeatEfficiency = 0.95;
    // Partition of carcass/rolling structural loss. Belt + sidewalls + carcass
    // must sum to one; validation rejects energy-creating authoring.
    VehicleScalar carcassLossHeatFractionToBelt = 0.18;
    VehicleScalar sidewallFlexHeatFraction = 0.32;
    VehicleScalar brakeHeatFractionToRim = 0.32;

    // Clean-room temperature response around an optimum.
    VehicleScalar optimumTreadTemperatureC = 70.0;
    VehicleScalar coldTemperatureSpanC = 60.0;
    VehicleScalar hotTemperatureSpanC = 70.0;
    VehicleScalar maximumColdFrictionLoss = 0.10;
    VehicleScalar maximumHotFrictionLoss = 0.28;
    VehicleScalar minimumFrictionScale = 0.65;
    VehicleScalar maximumFrictionScale = 1.12;

    // Structural stiffness follows a weighted construction temperature rather
    // than a single carcass node after TIRE46.
    VehicleScalar stiffnessTemperatureSlopePerC = -0.0012;
    VehicleScalar minimumStiffnessScale = 0.78;
    VehicleScalar maximumStiffnessScale = 1.10;

    VehicleScalar minimumTemperatureC = -50.0;
    VehicleScalar maximumTemperatureC = 220.0;
    VehicleScalar minimumGaugePressurePa = 0.0;
    VehicleScalar maximumGaugePressurePa = 700000.0;
};

struct TireThermalState
{
    bool initialized = false;
    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar beltTemperatureC = 20.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar innerSidewallTemperatureC = 20.0;
    VehicleScalar outerSidewallTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar rimTemperatureC = 20.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar containedGasMassRatio = 1.0;
};

struct TireThermalInput
{
    bool grounded = false;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar ambientAirSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar radialDissipationWatts = 0.0;
    VehicleScalar rollingResistanceDissipationWatts = 0.0;
    VehicleScalar brakeDissipationWatts = 0.0;
    VehicleScalar contactPatchAreaM2 = 0.0;
    VehicleScalar camberAngleRadians = 0.0;

    bool environmentTemperatureOverride = false;
    VehicleScalar ambientTemperatureC = 20.0;
    VehicleScalar roadTemperatureC = 20.0;
    VehicleScalar roadHeatTransferScale = 1.0;
};

struct TireThermalOutput
{
    bool valid = false;
    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar beltTemperatureC = 20.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar innerSidewallTemperatureC = 20.0;
    VehicleScalar outerSidewallTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar rimTemperatureC = 20.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar slipDissipationWatts = 0.0;
    VehicleScalar carcassDissipationWatts = 0.0;
    VehicleScalar sidewallDissipationWatts = 0.0;
    VehicleScalar roadHeatFlowWatts = 0.0;
    VehicleScalar airHeatFlowWatts = 0.0;
    VehicleScalar brakeHeatInputWatts = 0.0;
    VehicleScalar rimToCarcassHeatFlowWatts = 0.0;
};

bool validTireThermalDescription(const TireThermalDescription& value);

VehicleScalar tireThermalFrictionScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar treadTemperatureC);

VehicleScalar tireThermalStiffnessScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar structuralTemperatureC);

TireThermalOutput evaluateTireThermalState(
    const TireThermalDescription& description,
    const TireThermalState& state);

TireThermalOutput advanceTireThermal(
    const TireThermalDescription& description,
    const TireThermalInput& input,
    VehicleScalar deltaTimeSeconds,
    TireThermalState& state);

} // namespace heritage::vehicles::tires
