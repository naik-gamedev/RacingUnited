#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// TIRE07 clean-room lumped thermal/pressure model.
//
// Public Simcenter Tire material confirms that the modern MF-Tyre/MF-Swift
// lineage contains explicit Temperature & Velocity behavior, but the complete
// proprietary T&V equations/identified coefficients are not public. Heritage
// therefore keeps the thermal network independent from the MF6.2 equations:
// three energy states (tread, carcass, contained gas), physical heat flows and
// ideal-gas pressure feedback. A fitted/reference T&V provider can replace the
// empirical grip/stiffness modifiers later without changing the wheel solver.
struct TireThermalDescription
{
    bool enabled = false;

    // Reference state. inflation pressure is gauge pressure; ideal-gas work is
    // performed with absolute pressure internally.
    VehicleScalar referenceTemperatureC = 20.0;
    VehicleScalar initialTreadTemperatureC = 20.0;
    VehicleScalar initialCarcassTemperatureC = 20.0;
    VehicleScalar initialGasTemperatureC = 20.0;
    VehicleScalar ambientTemperatureC = 20.0;
    VehicleScalar roadTemperatureC = 20.0;
    VehicleScalar ambientPressurePa = 101325.0;
    VehicleScalar referenceGaugePressurePa = 220000.0;

    // Lumped heat capacities [J/K]. These are authoring/fitting parameters,
    // not Magic Formula coefficients.
    VehicleScalar treadHeatCapacityJPerK = 4200.0;
    VehicleScalar carcassHeatCapacityJPerK = 9500.0;
    VehicleScalar gasHeatCapacityJPerK = 220.0;

    // Thermal conductances [W/K]. Speed-dependent terms are added to the air
    // paths using abs(vehicle speed) in m/s.
    VehicleScalar treadToCarcassConductanceWPerK = 55.0;
    VehicleScalar treadToRoadConductanceWPerK = 90.0;
    VehicleScalar treadToAirConductanceWPerK = 10.0;
    VehicleScalar carcassToAirConductanceWPerK = 8.0;
    VehicleScalar carcassToGasConductanceWPerK = 10.0;
    VehicleScalar gasToAmbientConductanceWPerK = 2.0;
    VehicleScalar treadAirSpeedConductanceWPerKPerMps = 0.70;
    VehicleScalar carcassAirSpeedConductanceWPerKPerMps = 0.35;

    // Frictional slip power is primarily created in the tread/contact region;
    // the remainder enters the carcass. Radial/rolling losses enter carcass.
    VehicleScalar slipHeatFractionToTread = 0.85;
    VehicleScalar slipHeatEfficiency = 0.92;
    VehicleScalar carcassLossHeatEfficiency = 0.95;

    // Clean-room temperature response around an optimum. The curve is
    // normalized to referenceTemperatureC, so enabling TIRE07 does not create
    // an immediate discontinuity at the authored reference state.
    VehicleScalar optimumTreadTemperatureC = 70.0;
    VehicleScalar coldTemperatureSpanC = 60.0;
    VehicleScalar hotTemperatureSpanC = 70.0;
    VehicleScalar maximumColdFrictionLoss = 0.10;
    VehicleScalar maximumHotFrictionLoss = 0.28;
    VehicleScalar minimumFrictionScale = 0.65;
    VehicleScalar maximumFrictionScale = 1.12;

    // Warmer rubber/carcass generally becomes more compliant. This is a small
    // clean-room modifier around the authored reference state; fitted MF T&V
    // data can supersede it later.
    VehicleScalar stiffnessTemperatureSlopePerC = -0.0012;
    VehicleScalar minimumStiffnessScale = 0.78;
    VehicleScalar maximumStiffnessScale = 1.10;

    VehicleScalar minimumTemperatureC = -50.0;
    VehicleScalar maximumTemperatureC = 220.0;
    // Zero must remain representable for punctures/blowouts. Construction-
    // specific force providers may impose their own identified-data floor,
    // but the contained-air state itself must be able to reach ambient.
    VehicleScalar minimumGaugePressurePa = 0.0;
    VehicleScalar maximumGaugePressurePa = 700000.0;
};

struct TireThermalState
{
    bool initialized = false;
    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar inflationPressurePa = 220000.0;
    // Current contained mass divided by mass at the fitted cold reference.
    // TIRE19 owns leak integration; TIRE07 turns that mass and temperature
    // into live absolute/gauge pressure.
    VehicleScalar containedGasMassRatio = 1.0;
};

struct TireThermalInput
{
    bool grounded = false;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar longitudinalSlipVelocityMps = 0.0;
    VehicleScalar lateralSlipVelocityMps = 0.0;
    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar radialDissipationWatts = 0.0;
    VehicleScalar rollingResistanceDissipationWatts = 0.0;
    VehicleScalar contactPatchAreaM2 = 0.0;

    // TIRE15B live world-surface climate input. Legacy/direct unit tests can
    // leave this false and retain the temperatures authored in the tire file.
    bool environmentTemperatureOverride = false;
    VehicleScalar ambientTemperatureC = 20.0;
    VehicleScalar roadTemperatureC = 20.0;

    // TIRE11 contamination can insulate the tread from the road. One is a
    // clean contact; lower values reduce only the tread-to-road conductance.
    VehicleScalar roadHeatTransferScale = 1.0;
};

struct TireThermalOutput
{
    bool valid = false;
    VehicleScalar treadTemperatureC = 20.0;
    VehicleScalar carcassTemperatureC = 20.0;
    VehicleScalar gasTemperatureC = 20.0;
    VehicleScalar inflationPressurePa = 220000.0;
    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
    VehicleScalar slipDissipationWatts = 0.0;
    VehicleScalar carcassDissipationWatts = 0.0;
    VehicleScalar roadHeatFlowWatts = 0.0;
    VehicleScalar airHeatFlowWatts = 0.0;
};

bool validTireThermalDescription(const TireThermalDescription& value);

// Public helper used by TIRE08's spatial tread layer. It evaluates the same
// normalized clean-room temperature curve used by the lumped TIRE07 state so
// local surface hot/cold spots can be expressed as a ratio to the bulk tread.
VehicleScalar tireThermalFrictionScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar treadTemperatureC);

VehicleScalar tireThermalStiffnessScaleForTemperature(
    const TireThermalDescription& description,
    VehicleScalar carcassTemperatureC);

// Reads current state without advancing time. An uninitialized state is
// reported at the authored initial/reference condition, which makes it safe to
// query pressure/modifiers before the first 1 ms integration step.
TireThermalOutput evaluateTireThermalState(
    const TireThermalDescription& description,
    const TireThermalState& state);

TireThermalOutput advanceTireThermal(
    const TireThermalDescription& description,
    const TireThermalInput& input,
    VehicleScalar deltaTimeSeconds,
    TireThermalState& state);

} // namespace heritage::vehicles::tires
