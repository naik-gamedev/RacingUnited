#include "MagicFormula62.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-9;
constexpr VehicleScalar kPi = 3.14159265358979323846;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar signNonZero(VehicleScalar value)
{
    if (value > 0.0)
        return 1.0;
    if (value < 0.0)
        return -1.0;
    return 0.0;
}

VehicleScalar safeDenominator(VehicleScalar value)
{
    if (std::abs(value) >= kEpsilon)
        return value;
    return value < 0.0 ? -kEpsilon : kEpsilon;
}

VehicleScalar magicFormulaCore(
    VehicleScalar x,
    VehicleScalar B,
    VehicleScalar C,
    VehicleScalar D,
    VehicleScalar E)
{
    const VehicleScalar Bx = B * x;
    return D * std::sin(C * std::atan(
        Bx - E * (Bx - std::atan(Bx))));
}

VehicleScalar normalizedWeight(
    VehicleScalar x,
    VehicleScalar shift,
    VehicleScalar B,
    VehicleScalar C,
    VehicleScalar E)
{
    const auto cosineMagic = [B, C, E](VehicleScalar value) {
        const VehicleScalar Bx = B * value;
        return std::cos(C * std::atan(
            Bx - E * (Bx - std::atan(Bx))));
    };

    const VehicleScalar numerator = cosineMagic(x + shift);
    const VehicleScalar denominator = safeDenominator(cosineMagic(shift));
    return numerator / denominator;
}

struct CommonState
{
    VehicleScalar actualLoadN = 0.0;
    VehicleScalar evaluatedLoadN = 0.0;
    VehicleScalar loadScale = 1.0;
    VehicleScalar nominalLoadN = 0.0;
    VehicleScalar pressurePa = 0.0;
    VehicleScalar dfz = 0.0;
    VehicleScalar dpi = 0.0;
    VehicleScalar gamma = 0.0;
    VehicleScalar kappa = 0.0;
    VehicleScalar alpha = 0.0;
    VehicleScalar turnSlipPerM = 0.0;
    VehicleScalar normalizedTurnSlip = 0.0;
};

CommonState makeCommonState(
    const MagicFormula62Parameters& p,
    const MagicFormula62Input& input)
{
    CommonState state;
    state.actualLoadN = std::max(input.normalLoadN, 0.0);
    state.evaluatedLoadN = std::clamp(
        state.actualLoadN,
        p.minimumLoadN,
        p.maximumLoadN);
    state.loadScale = state.evaluatedLoadN > kEpsilon
        ? state.actualLoadN / state.evaluatedLoadN
        : 1.0;
    state.nominalLoadN = std::max(p.nominalLoadN * p.lFz0, kEpsilon);
    state.pressurePa = std::clamp(
        input.inflationPressurePa >= 0.0
            ? input.inflationPressurePa
            : p.nominalPressurePa,
        p.minimumPressurePa,
        p.maximumPressurePa);
    state.dfz = (state.evaluatedLoadN - state.nominalLoadN)
        / state.nominalLoadN;
    state.dpi = (state.pressurePa - p.nominalPressurePa)
        / std::max(p.nominalPressurePa, kEpsilon);
    state.gamma = std::clamp(
        input.camberAngleRadians,
        -p.maximumAbsCamberRadians,
        p.maximumAbsCamberRadians);
    state.kappa = std::clamp(
        input.longitudinalSlip,
        -p.maximumAbsLongitudinalSlip,
        p.maximumAbsLongitudinalSlip);

    // Heritage uses +slip-angle for lateral contact velocity to the right.
    // MF/ISO positive lateral force convention is opposite the restoring force
    // expected by the vehicle solver, so convert once at this boundary.
    state.alpha = std::clamp(
        -input.slipAngleRadians,
        -p.maximumAbsSlipAngleRadians,
        p.maximumAbsSlipAngleRadians);
    state.turnSlipPerM = finiteValue(input.turnSlipPerM)
        ? std::clamp(input.turnSlipPerM, VehicleScalar{-100.0}, VehicleScalar{100.0})
        : 0.0;
    state.normalizedTurnSlip = p.unloadedRadiusM * state.turnSlipPerM;
    return state;
}

struct TurnSlipState
{
    VehicleScalar normalizedSpin = 0.0;
    VehicleScalar zetaLongitudinalPeak = 1.0;
    VehicleScalar zetaLateralPeak = 1.0;
    VehicleScalar zetaCorneringStiffness = 1.0;
    VehicleScalar zetaCamberStiffness = 1.0;
    VehicleScalar zetaTrail = 1.0;
    VehicleScalar zetaResidual = 1.0;
    VehicleScalar rollingSpinMomentNm = 0.0;
};

TurnSlipState evaluateTurnSlipState(
    const MagicFormula62Parameters& p,
    const CommonState& s)
{
    TurnSlipState out;
    out.normalizedSpin = s.normalizedTurnSlip;
    if (std::abs(out.normalizedSpin) <= kEpsilon)
        return out;

    // The zeta reductions follow the public MF6.x coefficient semantics:
    // PDXP* reduces the Fx peak, PDYP* the Fy peak, PKYP1 cornering
    // stiffness, PECP* camber stiffness, QDTP1 trail and QBRP1 residual
    // spin torque. These expressions are kept isolated so a future equation-
    // parity validation against a licensed/reference implementation can
    // replace individual terms without touching the vehicle solver.
    const VehicleScalar Bxp = (p.pDxP1 + p.pDxP2 * s.dfz)
        * std::cos(std::atan(p.pDxP3 * s.kappa));
    out.zetaLongitudinalPeak = std::clamp(
        std::cos(std::atan(Bxp * out.normalizedSpin)),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    const VehicleScalar Byp = (p.pDyP1 + p.pDyP2 * s.dfz)
        * std::cos(std::atan(p.pDyP3 * std::tan(s.alpha)));
    const VehicleScalar signedRootSpin = signNonZero(out.normalizedSpin)
        * std::sqrt(std::abs(out.normalizedSpin));
    const VehicleScalar lateralSpinArgument = out.normalizedSpin
        + p.pDyP4 * signedRootSpin;
    out.zetaLateralPeak = std::clamp(
        std::cos(std::atan(Byp * lateralSpinArgument)),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    const VehicleScalar corneringCos = std::cos(
        std::atan(p.pKyP1 * out.normalizedSpin));
    out.zetaCorneringStiffness = std::clamp(
        corneringCos * corneringCos,
        VehicleScalar{0.0}, VehicleScalar{1.0});

    const VehicleScalar camberReduction = std::clamp(
        p.pEcP1 * (1.0 + p.pEcP2 * s.dfz),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    out.zetaCamberStiffness = std::clamp(
        1.0 - camberReduction * (1.0 - out.zetaCorneringStiffness),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    out.zetaTrail = std::clamp(
        std::cos(std::atan(p.qDtP1 * out.normalizedSpin)),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    out.zetaResidual = std::clamp(
        std::cos(std::atan(p.qBrP1 * out.normalizedSpin)),
        VehicleScalar{0.0}, VehicleScalar{1.0});

    // QDRP1 defines the MF6.2 rolling turn-slip moment peak. QCRP1 is
    // intentionally *not* used here: its documented role is constant turning
    // at zero forward speed, which TIRE03 handles in TireContactPatch.
    const VehicleScalar spinMagnitude = std::abs(out.normalizedSpin);
    const VehicleScalar build = (2.0 / kPi)
        * std::atan(spinMagnitude);
    const VehicleScalar sideSlipReduction = std::clamp(
        std::cos(std::atan(p.qBrP1 * std::tan(s.alpha))),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    const VehicleScalar alpha90Gain = 1.0 + p.qCrP2
        * std::clamp(std::abs(s.alpha) / (0.5 * kPi),
            VehicleScalar{0.0}, VehicleScalar{1.0});
    out.rollingSpinMomentNm = -signNonZero(out.normalizedSpin)
        * p.qDrP1 * s.evaluatedLoadN * p.unloadedRadiusM
        * build * sideSlipReduction * alpha90Gain;
    return out;
}

struct LongitudinalState
{
    VehicleScalar pureForceN = 0.0;
    VehicleScalar combinedForceN = 0.0;
    VehicleScalar mu = 0.0;
    VehicleScalar stiffnessN = 0.0;
    VehicleScalar weight = 1.0;
};

LongitudinalState evaluateLongitudinal(
    const MagicFormula62Parameters& p,
    const CommonState& s,
    const MagicFormula62Input& input,
    const TurnSlipState& turn)
{
    LongitudinalState out;
    const VehicleScalar gammaSquared = s.gamma * s.gamma;
    const VehicleScalar dpiSquared = s.dpi * s.dpi;

    const VehicleScalar Cx = p.pCx1 * p.lCx;
    out.mu = (p.pDx1 + p.pDx2 * s.dfz)
        * (1.0 - p.pDx3 * gammaSquared)
        * (1.0 + p.ppX3 * s.dpi + p.ppX4 * dpiSquared)
        * p.lMux
        * std::max(input.frictionScale, 0.0);
    out.mu = std::max(out.mu, 0.0);

    const VehicleScalar Dx = out.mu * s.evaluatedLoadN
        * turn.zetaLongitudinalPeak;
    const VehicleScalar SHx = (p.pHx1 + p.pHx2 * s.dfz) * p.lHx;
    const VehicleScalar kappaX = s.kappa + SHx;
    const VehicleScalar Ex = std::clamp(
        (p.pEx1 + p.pEx2 * s.dfz + p.pEx3 * s.dfz * s.dfz)
            * (1.0 - p.pEx4 * signNonZero(kappaX)) * p.lEx,
        -10.0,
        1.0);

    out.stiffnessN = (p.pKx1 + p.pKx2 * s.dfz)
        * std::exp(p.pKx3 * s.dfz)
        * (1.0 + p.ppX1 * s.dpi + p.ppX2 * dpiSquared)
        * s.evaluatedLoadN
        * p.lKxk
        * std::max(input.stiffnessScale, 0.0);
    const VehicleScalar Bx = out.stiffnessN
        / safeDenominator(Cx * Dx);
    const VehicleScalar SVx = (p.pVx1 + p.pVx2 * s.dfz)
        * s.evaluatedLoadN * p.lVx * p.lMux
        * std::max(input.frictionScale, 0.0);

    out.pureForceN = magicFormulaCore(kappaX, Bx, Cx, Dx, Ex) + SVx;

    const VehicleScalar BxAlpha = (p.rBx1 + p.rBx3 * gammaSquared)
        * std::cos(std::atan(p.rBx2 * s.kappa)) * p.lXalpha;
    const VehicleScalar CxAlpha = p.rCx1;
    const VehicleScalar ExAlpha = std::clamp(
        p.rEx1 + p.rEx2 * s.dfz,
        -10.0,
        1.0);
    const VehicleScalar SHxAlpha = p.rHx1;
    out.weight = normalizedWeight(
        s.alpha,
        SHxAlpha,
        BxAlpha,
        CxAlpha,
        ExAlpha);
    out.combinedForceN = out.pureForceN * out.weight;

    return out;
}

struct LateralState
{
    VehicleScalar pureForceN = 0.0;
    VehicleScalar combinedForceN = 0.0;
    VehicleScalar mu = 0.0;
    VehicleScalar corneringStiffnessNPerRad = 0.0;
    VehicleScalar camberStiffnessNPerRad = 0.0;
    VehicleScalar weight = 1.0;
    VehicleScalar By = 0.0;
    VehicleScalar Cy = 0.0;
    VehicleScalar SHy = 0.0;
    VehicleScalar SVy = 0.0;
    VehicleScalar SVyKappa = 0.0;
};

LateralState evaluateLateral(
    const MagicFormula62Parameters& p,
    const CommonState& s,
    const MagicFormula62Input& input,
    const TurnSlipState& turn)
{
    LateralState out;
    const VehicleScalar gammaSquared = s.gamma * s.gamma;
    const VehicleScalar dpiSquared = s.dpi * s.dpi;
    const VehicleScalar nominalLoad = s.nominalLoadN;

    out.Cy = p.pCy1 * p.lCy;
    out.mu = (p.pDy1 + p.pDy2 * s.dfz)
        * (1.0 - p.pDy3 * gammaSquared)
        * (1.0 + p.ppY3 * s.dpi + p.ppY4 * dpiSquared)
        * p.lMuy
        * std::max(input.frictionScale, 0.0);
    out.mu = std::max(out.mu, 0.0);
    const VehicleScalar Dy = out.mu * s.evaluatedLoadN
        * turn.zetaLateralPeak;

    const VehicleScalar loadDenominator =
        (p.pKy2 + p.pKy5 * gammaSquared)
        * (1.0 + p.ppY2 * s.dpi)
        * nominalLoad;
    out.corneringStiffnessNPerRad = p.pKy1 * nominalLoad
        * (1.0 + p.ppY1 * s.dpi)
        * std::sin(p.pKy4 * std::atan(
            s.evaluatedLoadN / safeDenominator(loadDenominator)))
        * (1.0 - p.pKy3 * std::abs(s.gamma))
        * p.lKya
        * turn.zetaCorneringStiffness
        * std::max(input.stiffnessScale, 0.0);
    out.camberStiffnessNPerRad = (p.pKy6 + p.pKy7 * s.dfz)
        * (1.0 + p.ppY5 * s.dpi)
        * s.evaluatedLoadN
        * p.lKygamma
        * turn.zetaCamberStiffness
        * std::max(input.stiffnessScale, 0.0);

    const VehicleScalar SVyGamma = s.evaluatedLoadN
        * (p.pVy3 + p.pVy4 * s.dfz)
        * s.gamma * p.lKygamma * p.lMuy
        * std::max(input.frictionScale, 0.0);
    const VehicleScalar SHyGamma =
        (out.camberStiffnessNPerRad * s.gamma - SVyGamma)
        / safeDenominator(out.corneringStiffnessNPerRad);
    const VehicleScalar SHy0 = (p.pHy1 + p.pHy2 * s.dfz) * p.lHy;
    out.SHy = SHy0 + SHyGamma;

    const VehicleScalar SVy0 = s.evaluatedLoadN
        * (p.pVy1 + p.pVy2 * s.dfz) * p.lVy * p.lMuy
        * std::max(input.frictionScale, 0.0);
    out.SVy = SVy0 + SVyGamma;

    const VehicleScalar alphaY = s.alpha + out.SHy;
    const VehicleScalar Ey = std::clamp(
        (p.pEy1 + p.pEy2 * s.dfz)
        * (1.0 + p.pEy5 * gammaSquared
            - (p.pEy3 + p.pEy4 * s.gamma) * signNonZero(alphaY))
        * p.lEy,
        -10.0,
        1.0);
    out.By = out.corneringStiffnessNPerRad
        / safeDenominator(out.Cy * Dy);
    out.pureForceN = magicFormulaCore(
        alphaY,
        out.By,
        out.Cy,
        Dy,
        Ey) + out.SVy;

    const VehicleScalar DvyKappa = out.mu * s.evaluatedLoadN
        * (p.rVy1 + p.rVy2 * s.dfz + p.rVy3 * s.gamma)
        * std::cos(std::atan(p.rVy4 * s.alpha));
    out.SVyKappa = DvyKappa
        * std::sin(p.rVy5 * std::atan(p.rVy6 * s.kappa))
        * p.lVykappa;

    const VehicleScalar ByKappa = (p.rBy1 + p.rBy4 * gammaSquared)
        * std::cos(std::atan(p.rBy2 * (s.alpha - p.rBy3)))
        * p.lYkappa;
    const VehicleScalar CyKappa = p.rCy1;
    const VehicleScalar EyKappa = std::clamp(
        p.rEy1 + p.rEy2 * s.dfz,
        -10.0,
        1.0);
    const VehicleScalar SHyKappa = p.rHy1 + p.rHy2 * s.dfz;
    out.weight = normalizedWeight(
        s.kappa,
        SHyKappa,
        ByKappa,
        CyKappa,
        EyKappa);
    out.combinedForceN = out.weight * out.pureForceN + out.SVyKappa;

    return out;
}

VehicleScalar evaluateOverturningMoment(
    const MagicFormula62Parameters& p,
    const CommonState& s,
    const MagicFormula62Input& input,
    VehicleScalar lateralForceN)
{
    const VehicleScalar loadRatio = s.evaluatedLoadN
        / std::max(s.nominalLoadN, kEpsilon);
    const VehicleScalar normalizedFy = lateralForceN
        / std::max(s.nominalLoadN, kEpsilon);
    const VehicleScalar inner = p.qSx1 * p.lVMx
        - p.qSx2 * s.gamma * (1.0 + p.ppMx1 * s.dpi)
        - p.qSx12 * s.gamma * std::abs(s.gamma)
        + p.qSx3 * normalizedFy
        + p.qSx4
            * std::cos(p.qSx5 * std::atan(
                std::pow(p.qSx6 * loadRatio, 2.0)))
            * std::sin(p.qSx7 * s.gamma
                + p.qSx8 * std::atan(p.qSx9 * normalizedFy))
        + p.qSx10 * std::atan(p.qSx11 * loadRatio) * s.gamma;
    const VehicleScalar moment = p.unloadedRadiusM
        * s.evaluatedLoadN * p.lMx * inner
        + p.unloadedRadiusM * lateralForceN * p.lMx
            * (p.qSx13 + p.qSx14 * std::abs(s.gamma));
    return moment * std::max(input.frictionScale, 0.0);
}

VehicleScalar evaluateRollingResistanceMoment(
    const MagicFormula62Parameters& p,
    const CommonState& s,
    VehicleScalar longitudinalForceN,
    VehicleScalar forwardSpeedMps)
{
    const VehicleScalar loadRatio = s.evaluatedLoadN
        / std::max(s.nominalLoadN, kEpsilon);
    const VehicleScalar pressureRatio = s.pressurePa
        / std::max(p.nominalPressurePa, kEpsilon);
    const VehicleScalar speedRatio = std::abs(forwardSpeedMps)
        / std::max(p.referenceSpeedMps, 0.1);
    const VehicleScalar normalizedFx = longitudinalForceN
        / std::max(s.nominalLoadN, kEpsilon);

    VehicleScalar bracket = p.qSy1
        + p.qSy2 * normalizedFx
        + p.qSy3 * speedRatio
        + p.qSy4 * std::pow(speedRatio, 4.0)
        + p.qSy5 * s.gamma * s.gamma
        + p.qSy6 * loadRatio * s.gamma * s.gamma;
    bracket = std::max(bracket, 0.0);

    VehicleScalar direction = signNonZero(forwardSpeedMps);
    if (direction == 0.0)
        direction = signNonZero(longitudinalForceN);
    return -direction * p.unloadedRadiusM * s.nominalLoadN * p.lMy
        * bracket
        * std::pow(std::max(loadRatio, 0.01), p.qSy7)
        * std::pow(std::max(pressureRatio, 0.01), p.qSy8);
}

struct AligningState
{
    VehicleScalar momentNm = 0.0;
    VehicleScalar trailM = 0.0;
    VehicleScalar residualMomentNm = 0.0;
};

AligningState evaluateAligningMoment(
    const MagicFormula62Parameters& p,
    const CommonState& s,
    const MagicFormula62Input& input,
    const LongitudinalState& longitudinal,
    const LateralState& lateral,
    const TurnSlipState& turn)
{
    AligningState out;

    // Equation 75 uses the combined-slip side force evaluated at zero camber.
    CommonState zeroCamberState = s;
    zeroCamberState.gamma = 0.0;
    const LateralState zeroCamber = evaluateLateral(
        p,
        zeroCamberState,
        input,
        turn);

    const VehicleScalar SHt = p.qHz1 + p.qHz2 * s.dfz
        + (p.qHz3 + p.qHz4 * s.dfz) * s.gamma;
    const VehicleScalar alphaT = s.alpha + SHt;
    const VehicleScalar alphaR = s.alpha + lateral.SHy
        + lateral.SVy / safeDenominator(lateral.corneringStiffnessNPerRad);

    const VehicleScalar stiffnessRatio = longitudinal.stiffnessN
        / safeDenominator(lateral.corneringStiffnessNPerRad);
    const auto equivalentAngle = [stiffnessRatio, &s](VehicleScalar alpha) {
        const VehicleScalar tangent = std::tan(alpha);
        return std::atan(std::sqrt(
            tangent * tangent
            + stiffnessRatio * stiffnessRatio * s.kappa * s.kappa))
            * signNonZero(alpha);
    };
    const VehicleScalar alphaTeq = equivalentAngle(alphaT);
    const VehicleScalar alphaReq = equivalentAngle(alphaR);

    const VehicleScalar Bt = (p.qBz1 + p.qBz2 * s.dfz
        + p.qBz3 * s.dfz * s.dfz)
        * (1.0 + p.qBz4 * s.gamma + p.qBz5 * std::abs(s.gamma))
        * p.lKya / safeDenominator(p.lMuy);
    const VehicleScalar Ct = p.qCz1;
    const VehicleScalar Dt = (p.qDz1 + p.qDz2 * s.dfz)
        * (1.0 - p.ppZ1 * s.dpi)
        * (1.0 + p.qDz3 * s.gamma + p.qDz4 * s.gamma * s.gamma)
        * s.evaluatedLoadN * p.unloadedRadiusM
        / std::max(s.nominalLoadN, kEpsilon)
        * p.lT;
    const VehicleScalar Et = std::clamp(
        (p.qEz1 + p.qEz2 * s.dfz + p.qEz3 * s.dfz * s.dfz)
        * (1.0 + (p.qEz4 + p.qEz5 * s.gamma)
            * (2.0 / kPi) * std::atan(Bt * Ct * alphaT)),
        -10.0,
        1.0);

    out.trailM = Dt * std::cos(Ct * std::atan(
        Bt * alphaTeq
        - Et * (Bt * alphaTeq - std::atan(Bt * alphaTeq))))
        * std::cos(s.alpha) * turn.zetaTrail;

    const VehicleScalar Br = p.qBz9 * p.lKya / safeDenominator(p.lMuy)
        + p.qBz10 * lateral.By * lateral.Cy;
    const VehicleScalar Dr = (
        (p.qDz6 + p.qDz7 * s.dfz) * p.lMzr
        + (p.qDz8 + p.qDz9 * s.dfz)
            * (1.0 - p.ppZ2 * s.dpi) * s.gamma * p.lKzgamma
        + (p.qDz10 + p.qDz11 * s.dfz)
            * s.gamma * std::abs(s.gamma) * p.lKzgamma)
        * s.evaluatedLoadN * p.unloadedRadiusM * p.lMuy
        * std::max(input.frictionScale, 0.0);
    out.residualMomentNm = Dr * std::cos(std::atan(Br * alphaReq))
        * std::cos(s.alpha) * turn.zetaResidual;

    const VehicleScalar lever = (
        p.sSz1
        + p.sSz2 * lateral.combinedForceN
            / std::max(s.nominalLoadN, kEpsilon)
        + (p.sSz3 + p.sSz4 * s.dfz) * s.gamma)
        * p.unloadedRadiusM * p.lS;

    out.momentNm = -out.trailM * zeroCamber.combinedForceN
        + out.residualMomentNm
        + lever * longitudinal.combinedForceN;

    return out;
}

} // namespace

bool validMagicFormula62Parameters(const MagicFormula62Parameters& p)
{
    const VehicleScalar values[] = {
        p.unloadedRadiusM, p.nominalLoadN, p.nominalPressurePa,
        p.referenceSpeedMps, p.minimumLoadN, p.maximumLoadN,
        p.minimumPressurePa, p.maximumPressurePa,
        p.maximumAbsLongitudinalSlip, p.maximumAbsSlipAngleRadians,
        p.maximumAbsCamberRadians, p.lMp, p.pCx1, p.pCy1, p.pDx1, p.pDy1,
        p.pKx1, p.pKy1, p.pKy2, p.pKy4, p.qCz1,
        p.pDxP1, p.pDxP2, p.pDxP3, p.pKyP1,
        p.pDyP1, p.pDyP2, p.pDyP3, p.pDyP4,
        p.pHyP1, p.pHyP2, p.pHyP3, p.pHyP4,
        p.pEcP1, p.pEcP2, p.qDtP1, p.qCrP1, p.qCrP2,
        p.qBrP1, p.qDrP1
    };
    for (VehicleScalar value : values)
    {
        if (!finiteValue(value))
            return false;
    }

    return p.unloadedRadiusM > 0.05
        && p.unloadedRadiusM < 2.5
        && p.nominalLoadN >= 100.0
        && p.nominalLoadN <= 200000.0
        && p.nominalPressurePa >= 20000.0
        && p.nominalPressurePa <= 2000000.0
        && p.referenceSpeedMps > 0.1
        && p.minimumLoadN > 0.0
        && p.maximumLoadN >= p.minimumLoadN
        && p.minimumPressurePa > 0.0
        && p.maximumPressurePa >= p.minimumPressurePa
        && p.maximumAbsLongitudinalSlip > 0.01
        && p.maximumAbsSlipAngleRadians > 0.01
        && p.maximumAbsCamberRadians > 0.01
        && p.pCx1 > 0.1
        && p.pCy1 > 0.1
        && p.pKy2 > 0.01;
}

MagicFormula62Result evaluateMagicFormula62(
    const MagicFormula62Parameters& p,
    const MagicFormula62Input& input)
{
    MagicFormula62Result result;
    if (!validMagicFormula62Parameters(p)
        || !finiteValue(input.normalLoadN)
        || input.normalLoadN <= kEpsilon)
    {
        return result;
    }

    const CommonState state = makeCommonState(p, input);
    const TurnSlipState turn = evaluateTurnSlipState(p, state);
    const LongitudinalState longitudinal = evaluateLongitudinal(
        p, state, input, turn);
    const LateralState lateral = evaluateLateral(p, state, input, turn);
    const AligningState aligning = evaluateAligningMoment(
        p, state, input, longitudinal, lateral, turn);

    const VehicleScalar loadScale = state.loadScale;
    result.pureLongitudinalForceN = longitudinal.pureForceN * loadScale;
    result.pureLateralForceN = lateral.pureForceN * loadScale;
    result.longitudinalForceN = longitudinal.combinedForceN * loadScale;
    result.lateralForceN = lateral.combinedForceN * loadScale;
    result.overturningMomentNm = evaluateOverturningMoment(
        p, state, input, lateral.combinedForceN) * loadScale;
    result.rollingResistanceMomentNm = evaluateRollingResistanceMoment(
        p, state, longitudinal.combinedForceN, input.forwardSpeedMps) * loadScale;
    result.turnSlipMomentNm = turn.rollingSpinMomentNm * loadScale;
    result.aligningMomentNm = (aligning.momentNm
        + turn.rollingSpinMomentNm) * loadScale;
    result.pneumaticTrailM = aligning.trailM;
    result.residualAligningMomentNm = aligning.residualMomentNm * loadScale;
    result.longitudinalFrictionCoefficient = longitudinal.mu;
    result.lateralFrictionCoefficient = lateral.mu;
    result.longitudinalSlipStiffnessN = longitudinal.stiffnessN * loadScale;
    result.corneringStiffnessNPerRad = lateral.corneringStiffnessNPerRad * loadScale;
    result.camberStiffnessNPerRad = lateral.camberStiffnessNPerRad * loadScale;
    result.combinedLongitudinalWeight = longitudinal.weight;
    result.combinedLateralWeight = lateral.weight;
    result.normalizedTurnSlip = turn.normalizedSpin;
    result.turnSlipLongitudinalReduction = turn.zetaLongitudinalPeak;
    result.turnSlipLateralReduction = turn.zetaLateralPeak;
    result.turnSlipCorneringReduction = turn.zetaCorneringStiffness;
    result.turnSlipTrailReduction = turn.zetaTrail;
    return result;
}

} // namespace heritage::vehicles::tires
