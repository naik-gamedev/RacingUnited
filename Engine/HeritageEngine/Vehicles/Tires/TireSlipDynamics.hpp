#pragma once

#include "../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

struct TireSlipDynamicsDescription
{
    VehicleScalar longitudinalRelaxationLengthM = 0.35;
    VehicleScalar lateralRelaxationLengthM = 0.45;
    VehicleScalar minimumTransportSpeedMps = 0.50;
};

struct TireSlipDynamicsState
{
    VehicleScalar longitudinalSlip = 0.0;
    VehicleScalar slipAngleRadians = 0.0;
};

// MF-Tyre transient-property coefficients retained from .tir files. When the
// required pair is present, TIRE03 derives the relaxation length from load,
// tire radius and camber rather than using a single Heritage constant.
struct TireSlipDynamicsCoefficients
{
    VehicleScalar pTx1 = 0.0;
    VehicleScalar pTx2 = 0.0;
    VehicleScalar pTx3 = 0.0;
    VehicleScalar pTy1 = 0.0;
    VehicleScalar pTy2 = 0.0;
    VehicleScalar lSgKappa = 1.0;
    VehicleScalar lSgAlpha = 1.0;
};

VehicleScalar magicFormulaLongitudinalRelaxationLengthM(
    const TireSlipDynamicsCoefficients& coefficients,
    VehicleScalar normalLoadN,
    VehicleScalar nominalLoadN,
    VehicleScalar unloadedRadiusM,
    VehicleScalar fallbackLengthM);

VehicleScalar magicFormulaLateralRelaxationLengthM(
    const TireSlipDynamicsCoefficients& coefficients,
    VehicleScalar normalLoadN,
    VehicleScalar nominalLoadN,
    VehicleScalar unloadedRadiusM,
    VehicleScalar camberAngleRadians,
    VehicleScalar lateralCamberSensitivity,
    VehicleScalar fallbackLengthM);

bool validTireSlipDynamicsDescription(
    const TireSlipDynamicsDescription& description);

VehicleScalar tireRelaxationBlend(
    VehicleScalar transportSpeedMps,
    VehicleScalar relaxationLengthM,
    VehicleScalar deltaTimeSeconds,
    VehicleScalar minimumTransportSpeedMps = 0.50);

void integrateTireSlipDynamics(
    const TireSlipDynamicsDescription& description,
    VehicleScalar targetLongitudinalSlip,
    VehicleScalar targetSlipAngleRadians,
    VehicleScalar transportSpeedMps,
    VehicleScalar deltaTimeSeconds,
    TireSlipDynamicsState& state);

} // namespace heritage::vehicles::tires
