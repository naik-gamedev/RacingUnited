#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// TIRE03 low-speed/standstill torsional contact-patch state. MF turn slip is a
// steady rolling quantity (yaw rate / forward speed) and therefore becomes
// singular as speed approaches zero. Real tread rubber instead stores shear
// deformation while the wheel is steered at standstill, then releases it as
// the contact patch rolls through. This small state model bridges that region
// without smuggling a velocity damper into the Magic Formula equations.
struct TireContactPatchDescription
{
    VehicleScalar torsionalRelaxationLengthM = 0.12;
    VehicleScalar maximumElasticTwistRadians = 0.14; // ~8 degrees
    VehicleScalar turnSlipRegularizationSpeedMps = 0.50;
    VehicleScalar parkingMomentTransitionSpeedMps = 1.50;
};

struct TireContactPatchState
{
    VehicleScalar torsionalTwistRadians = 0.0;
};

struct TireContactPatchInput
{
    VehicleScalar wheelYawRateRadiansPerSecond = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar effectiveFriction = 1.0;
    VehicleScalar unloadedRadiusM = 0.30;

    // MF6.2 QCRP1: dimensionless turning-moment scale for constant turning at
    // zero longitudinal speed. Zero disables the parking moment while the
    // state itself may still be observed in diagnostics.
    VehicleScalar zeroSpeedTurnMomentCoefficient = 0.0;
    VehicleScalar parkingMomentScale = 1.0; // MF6.2 LMP
};

struct TireContactPatchOutput
{
    VehicleScalar torsionalTwistRadians = 0.0;
    VehicleScalar turnSlipPerM = 0.0;
    VehicleScalar parkingTurnMomentNm = 0.0;
    VehicleScalar parkingMomentBlend = 0.0;
};

bool validTireContactPatchDescription(
    const TireContactPatchDescription& description);

TireContactPatchOutput integrateTireContactPatch(
    const TireContactPatchDescription& description,
    const TireContactPatchInput& input,
    VehicleScalar deltaTimeSeconds,
    TireContactPatchState& state);

} // namespace heritage::vehicles::tires
