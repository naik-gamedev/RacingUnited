#pragma once

#include "../../VehiclePrecision.hpp"

namespace heritage::vehicles::tires {

// Heritage clean-room implementation of the public MF-Tyre 6.x steady-state
// force/moment equations. The coefficient names intentionally follow the
// published TNO notation so fitted FITTYP=61/62-style datasets can be mapped
// without inventing a Heritage-only vocabulary.
//
// Scope of this type:
// - pure/combined longitudinal force
// - pure/combined lateral force with explicit camber stiffness
// - overturning moment
// - rolling-resistance moment with load/pressure/speed dependence
// - pneumatic trail + residual self-aligning moment
//
// MF-Tyre/MF-Swift 6.2 also contains turn-slip, low-speed/contact and rigid-ring
// dynamics. Those are separate state/contact mechanisms rather than hidden in
// this steady-state evaluator.
struct MagicFormula62Parameters
{
    // General/reference quantities.
    VehicleScalar unloadedRadiusM = 0.300;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar nominalPressurePa = 220000.0;
    VehicleScalar referenceSpeedMps = 16.6666666667;

    // Longitudinal force, pure slip.
    VehicleScalar pCx1 = 1.65;
    VehicleScalar pDx1 = 1.15;
    VehicleScalar pDx2 = -0.138;
    VehicleScalar pDx3 = 0.0;
    VehicleScalar pEx1 = 0.20;
    VehicleScalar pEx2 = 0.0;
    VehicleScalar pEx3 = 0.0;
    VehicleScalar pEx4 = 0.0;
    VehicleScalar pKx1 = 25.7142857143;
    VehicleScalar pKx2 = -3.8571428571;
    VehicleScalar pKx3 = 0.0;
    VehicleScalar pHx1 = 0.0;
    VehicleScalar pHx2 = 0.0;
    VehicleScalar pVx1 = 0.0;
    VehicleScalar pVx2 = 0.0;
    VehicleScalar ppX1 = 0.0;
    VehicleScalar ppX2 = 0.0;
    VehicleScalar ppX3 = 0.0;
    VehicleScalar ppX4 = 0.0;

    // Longitudinal force, combined slip.
    VehicleScalar rBx1 = 12.0;
    VehicleScalar rBx2 = 7.0;
    VehicleScalar rBx3 = 0.0;
    VehicleScalar rCx1 = 1.0;
    VehicleScalar rEx1 = 0.0;
    VehicleScalar rEx2 = 0.0;
    VehicleScalar rHx1 = 0.0;

    // Lateral force, pure slip. Positive Heritage slip angle is converted to
    // the ISO/MF force sign convention at the evaluator boundary.
    VehicleScalar pCy1 = 1.30;
    VehicleScalar pDy1 = 1.15;
    VehicleScalar pDy2 = -0.138;
    VehicleScalar pDy3 = 0.05;
    VehicleScalar pEy1 = 0.15;
    VehicleScalar pEy2 = 0.0;
    VehicleScalar pEy3 = 0.0;
    VehicleScalar pEy4 = 0.0;
    VehicleScalar pEy5 = 0.0;
    VehicleScalar pKy1 = 27.65;
    VehicleScalar pKy2 = 1.90;
    VehicleScalar pKy3 = 0.0;
    VehicleScalar pKy4 = 2.0;
    VehicleScalar pKy5 = 0.0;
    VehicleScalar pKy6 = 2.5;
    VehicleScalar pKy7 = 0.0;
    VehicleScalar pHy1 = 0.0;
    VehicleScalar pHy2 = 0.0;
    VehicleScalar pVy1 = 0.0;
    VehicleScalar pVy2 = 0.0;
    VehicleScalar pVy3 = 0.0;
    VehicleScalar pVy4 = 0.0;
    VehicleScalar ppY1 = 0.0;
    VehicleScalar ppY2 = 0.0;
    VehicleScalar ppY3 = 0.0;
    VehicleScalar ppY4 = 0.0;
    VehicleScalar ppY5 = 0.0;

    // Lateral force, combined slip.
    VehicleScalar rBy1 = 7.0;
    VehicleScalar rBy2 = 9.0;
    VehicleScalar rBy3 = 0.0;
    VehicleScalar rBy4 = 0.0;
    VehicleScalar rCy1 = 1.0;
    VehicleScalar rEy1 = 0.0;
    VehicleScalar rEy2 = 0.0;
    VehicleScalar rHy1 = 0.0;
    VehicleScalar rHy2 = 0.0;
    VehicleScalar rVy1 = 0.0;
    VehicleScalar rVy2 = 0.0;
    VehicleScalar rVy3 = 0.0;
    VehicleScalar rVy4 = 0.0;
    VehicleScalar rVy5 = 1.0;
    VehicleScalar rVy6 = 1.0;

    // Overturning moment Mx.
    VehicleScalar qSx1 = 0.0;
    VehicleScalar qSx2 = 0.0;
    VehicleScalar qSx3 = 0.0;
    VehicleScalar qSx4 = 0.0;
    VehicleScalar qSx5 = 1.0;
    VehicleScalar qSx6 = 1.0;
    VehicleScalar qSx7 = 0.0;
    VehicleScalar qSx8 = 0.0;
    VehicleScalar qSx9 = 1.0;
    VehicleScalar qSx10 = 0.0;
    VehicleScalar qSx11 = 1.0;
    VehicleScalar qSx12 = 0.0;
    VehicleScalar qSx13 = 0.0;
    VehicleScalar qSx14 = 0.0;
    VehicleScalar ppMx1 = 0.0;

    // Rolling-resistance moment My.
    VehicleScalar qSy1 = 0.012;
    VehicleScalar qSy2 = 0.0;
    VehicleScalar qSy3 = 0.0015;
    VehicleScalar qSy4 = 0.0;
    VehicleScalar qSy5 = 0.0;
    VehicleScalar qSy6 = 0.0;
    VehicleScalar qSy7 = 0.85;
    VehicleScalar qSy8 = -0.40;

    // Self-aligning moment Mz / pneumatic trail.
    VehicleScalar qBz1 = 8.0;
    VehicleScalar qBz2 = 0.0;
    VehicleScalar qBz3 = 0.0;
    VehicleScalar qBz4 = 0.0;
    VehicleScalar qBz5 = 0.0;
    VehicleScalar qBz9 = 1.0;
    VehicleScalar qBz10 = 0.0;
    VehicleScalar qCz1 = 1.10;
    VehicleScalar qDz1 = 0.25;
    VehicleScalar qDz2 = 0.0;
    VehicleScalar qDz3 = 0.0;
    VehicleScalar qDz4 = 0.0;
    VehicleScalar qDz6 = 0.0;
    VehicleScalar qDz7 = 0.0;
    VehicleScalar qDz8 = 0.0;
    VehicleScalar qDz9 = 0.0;
    VehicleScalar qDz10 = 0.0;
    VehicleScalar qDz11 = 0.0;
    VehicleScalar qEz1 = 0.0;
    VehicleScalar qEz2 = 0.0;
    VehicleScalar qEz3 = 0.0;
    VehicleScalar qEz4 = 0.0;
    VehicleScalar qEz5 = 0.0;
    VehicleScalar qHz1 = 0.0;
    VehicleScalar qHz2 = 0.0;
    VehicleScalar qHz3 = 0.0;
    VehicleScalar qHz4 = 0.0;
    VehicleScalar sSz1 = 0.0;
    VehicleScalar sSz2 = 0.0;
    VehicleScalar sSz3 = 0.0;
    VehicleScalar sSz4 = 0.0;
    VehicleScalar ppZ1 = 0.0;
    VehicleScalar ppZ2 = 0.0;

    // MF6.2 turn-slip / spin coefficients. Zero means the fitted dataset did
    // not identify that mechanism, so TIRE01/TIRE02 datasets remain exactly
    // backward compatible until coefficients are supplied by a .tir file.
    // Names correspond directly to the published TIR property vocabulary.
    VehicleScalar pDxP1 = 0.0;
    VehicleScalar pDxP2 = 0.0;
    VehicleScalar pDxP3 = 0.0;
    VehicleScalar pKyP1 = 0.0;
    VehicleScalar pDyP1 = 0.0;
    VehicleScalar pDyP2 = 0.0;
    VehicleScalar pDyP3 = 0.0;
    VehicleScalar pDyP4 = 0.0;
    VehicleScalar pHyP1 = 0.0;
    VehicleScalar pHyP2 = 0.0;
    VehicleScalar pHyP3 = 0.0;
    VehicleScalar pHyP4 = 0.0;
    VehicleScalar pEcP1 = 0.0;
    VehicleScalar pEcP2 = 0.0;
    VehicleScalar qDtP1 = 0.0;
    VehicleScalar qCrP1 = 0.0;
    VehicleScalar qCrP2 = 0.0;
    VehicleScalar qBrP1 = 0.0;
    VehicleScalar qDrP1 = 0.0;

    // Scaling coefficients. They remain one during ordinary parameter fitting
    // and are useful for setup/surface modifiers without mutating fitted data.
    VehicleScalar lFz0 = 1.0;
    VehicleScalar lCx = 1.0;
    VehicleScalar lMux = 1.0;
    VehicleScalar lEx = 1.0;
    VehicleScalar lKxk = 1.0;
    VehicleScalar lHx = 1.0;
    VehicleScalar lVx = 1.0;
    VehicleScalar lXalpha = 1.0;

    VehicleScalar lCy = 1.0;
    VehicleScalar lMuy = 1.0;
    VehicleScalar lEy = 1.0;
    VehicleScalar lKya = 1.0;
    VehicleScalar lKygamma = 1.0;
    VehicleScalar lHy = 1.0;
    VehicleScalar lVy = 1.0;
    VehicleScalar lYkappa = 1.0;
    VehicleScalar lVykappa = 1.0;

    VehicleScalar lMx = 1.0;
    VehicleScalar lVMx = 1.0;
    VehicleScalar lMy = 1.0;
    VehicleScalar lMp = 1.0;
    VehicleScalar lT = 1.0;
    VehicleScalar lMzr = 1.0;
    VehicleScalar lKzgamma = 1.0;
    VehicleScalar lS = 1.0;

    // Explicit validity envelope. MF models should be fitted/evaluated within
    // the measurement region rather than trusted for arbitrary extrapolation.
    VehicleScalar minimumLoadN = 100.0;
    VehicleScalar maximumLoadN = 20000.0;
    VehicleScalar minimumPressurePa = 80000.0;
    VehicleScalar maximumPressurePa = 500000.0;
    VehicleScalar maximumAbsLongitudinalSlip = 1.50;
    VehicleScalar maximumAbsSlipAngleRadians = 1.20;
    VehicleScalar maximumAbsCamberRadians = 1.20;
};

struct MagicFormula62Input
{
    VehicleScalar normalLoadN = 0.0;
    VehicleScalar longitudinalSlip = 0.0;
    VehicleScalar slipAngleRadians = 0.0;
    VehicleScalar camberAngleRadians = 0.0;
    VehicleScalar inflationPressurePa = 0.0;
    VehicleScalar forwardSpeedMps = 0.0;

    // Wheel spin about the contact normal divided by forward speed [1/m].
    // The vehicle/contact layer regularizes the zero-speed singularity; this
    // steady-state evaluator only consumes the resulting turn-slip quantity.
    VehicleScalar turnSlipPerM = 0.0;

    VehicleScalar frictionScale = 1.0;
    VehicleScalar stiffnessScale = 1.0;
};

struct MagicFormula62Result
{
    VehicleScalar pureLongitudinalForceN = 0.0;
    VehicleScalar pureLateralForceN = 0.0;
    VehicleScalar longitudinalForceN = 0.0;
    VehicleScalar lateralForceN = 0.0;
    VehicleScalar overturningMomentNm = 0.0;
    VehicleScalar rollingResistanceMomentNm = 0.0;
    VehicleScalar aligningMomentNm = 0.0;
    VehicleScalar pneumaticTrailM = 0.0;
    VehicleScalar residualAligningMomentNm = 0.0;

    VehicleScalar longitudinalFrictionCoefficient = 0.0;
    VehicleScalar lateralFrictionCoefficient = 0.0;
    VehicleScalar longitudinalSlipStiffnessN = 0.0;
    VehicleScalar corneringStiffnessNPerRad = 0.0;
    VehicleScalar camberStiffnessNPerRad = 0.0;
    VehicleScalar combinedLongitudinalWeight = 1.0;
    VehicleScalar combinedLateralWeight = 1.0;

    // TIRE03 turn-slip diagnostics. zeta* are the spin reduction factors used
    // by the public MF6.x lineage; turnSlipMomentNm is the steady rolling-spin
    // contribution before any separate standstill contact-patch torsion.
    VehicleScalar normalizedTurnSlip = 0.0;
    VehicleScalar turnSlipLongitudinalReduction = 1.0;
    VehicleScalar turnSlipLateralReduction = 1.0;
    VehicleScalar turnSlipCorneringReduction = 1.0;
    VehicleScalar turnSlipTrailReduction = 1.0;
    VehicleScalar turnSlipMomentNm = 0.0;
};

bool validMagicFormula62Parameters(const MagicFormula62Parameters& value);

MagicFormula62Result evaluateMagicFormula62(
    const MagicFormula62Parameters& parameters,
    const MagicFormula62Input& input);

} // namespace heritage::vehicles::tires
