#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// TIRE05/TIRE06 clean-room rigid-ring structural layer.
//
// MF-Swift publicly describes the tire belt as a rigid ring connected to the
// rim/axle through compliant and damped modes. Heritage keeps that structural
// state separate from the MF6.2 force equations. TIRE05 activated translation;
// TIRE06 adds belt yaw and circumferential wind-up using imported public
// structural frequency/stiffness/inertia vocabulary where available.
struct TireRigidRingDescription
{
    bool enabled = false;

    VehicleScalar longitudinalStiffnessNPerM = 0.0;
    VehicleScalar lateralStiffnessNPerM = 0.0;
    VehicleScalar yawStiffnessNmPerRad = 0.0;

    VehicleScalar longitudinalFrequencyHz = 0.0;
    VehicleScalar lateralFrequencyHz = 0.0;
    VehicleScalar radialFrequencyHz = 0.0;
    VehicleScalar yawFrequencyHz = 0.0;
    VehicleScalar windupFrequencyHz = 0.0;

    VehicleScalar longitudinalDampingRatio = 0.0;
    VehicleScalar lateralDampingRatio = 0.0;
    VehicleScalar radialDampingRatio = 0.0;
    VehicleScalar yawDampingRatio = 0.0;
    VehicleScalar windupDampingRatio = 0.0;
    VehicleScalar residualDampingRatio = 0.0;
    VehicleScalar lowSpeedAdditionalDampingRatio = 0.0;
    VehicleScalar lowSpeedDampingScale = 1.0;
    VehicleScalar lowSpeedThresholdMps = 1.0;

    VehicleScalar beltMassKg = 0.0;
    // A tire belt is approximately axisymmetric around its wheel axis. Public
    // BELT_IXX is used for the out-of-plane/yaw inertia and BELT_IYY for the
    // polar wheel-axis wind-up inertia in Heritage's clean-room mapping.
    VehicleScalar beltDiametralInertiaKgM2 = 0.0;
    VehicleScalar beltPolarInertiaKgM2 = 0.0;

    VehicleScalar maximumLongitudinalOffsetM = 0.04;
    VehicleScalar maximumLateralOffsetM = 0.04;
    VehicleScalar maximumRadialOffsetM = 0.08;
    VehicleScalar maximumYawAngleRadians = 0.12;
    VehicleScalar maximumWindupAngleRadians = 0.10;
};

struct TireRigidRingState
{
    bool initialized = false;
    VehicleScalar longitudinalOffsetM = 0.0;
    VehicleScalar longitudinalVelocityMps = 0.0;
    VehicleScalar lateralOffsetM = 0.0;
    VehicleScalar lateralVelocityMps = 0.0;
    VehicleScalar radialOffsetM = 0.0;
    VehicleScalar radialVelocityMps = 0.0;
    VehicleScalar yawAngleRadians = 0.0;
    VehicleScalar yawAngularVelocityRadPerS = 0.0;
    VehicleScalar windupAngleRadians = 0.0;
    VehicleScalar windupAngularVelocityRadPerS = 0.0;
};

struct TireRigidRingInput
{
    VehicleScalar deltaTimeSeconds = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;

    VehicleScalar roadRadialOffsetM = 0.0;

    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;

    // TIRE32: live pneumatic/thermal state scales the identified structural
    // compliance around its reference condition.
    VehicleScalar inflationPressurePa = 0.0;
    VehicleScalar referencePressurePa = 0.0;
    VehicleScalar thermalStiffnessScale = 1.0;

    // TIRE06 rotational excitations. Aligning moment twists the belt in yaw;
    // longitudinal force acting through the effective radius excites belt
    // wind-up around the wheel spin axis.
    VehicleScalar aligningMomentNm = 0.0;
    VehicleScalar longitudinalReactionMomentNm = 0.0;
};

struct TireRigidRingOutput
{
    bool valid = false;
    VehicleScalar longitudinalOffsetM = 0.0;
    VehicleScalar longitudinalVelocityMps = 0.0;
    VehicleScalar lateralOffsetM = 0.0;
    VehicleScalar lateralVelocityMps = 0.0;
    VehicleScalar radialOffsetM = 0.0;
    VehicleScalar radialVelocityMps = 0.0;
    VehicleScalar yawAngleRadians = 0.0;
    VehicleScalar yawAngularVelocityRadPerS = 0.0;
    VehicleScalar windupAngleRadians = 0.0;
    VehicleScalar windupAngularVelocityRadPerS = 0.0;
};

bool validTireRigidRingDescription(const TireRigidRingDescription& description);

TireRigidRingOutput advanceTireRigidRing(
    const TireRigidRingDescription& description,
    const TireRigidRingInput& input,
    TireRigidRingState& state);

void relaxTireRigidRingAirborne(
    const TireRigidRingDescription& description,
    VehicleScalar deltaTimeSeconds,
    TireRigidRingState& state);

} // namespace heritage::vehicles::tires
