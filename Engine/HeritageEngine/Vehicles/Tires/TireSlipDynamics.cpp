#include "TireSlipDynamics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

VehicleScalar magicFormulaLongitudinalRelaxationLengthM(
    const TireSlipDynamicsCoefficients& c,
    VehicleScalar normalLoadN,
    VehicleScalar nominalLoadN,
    VehicleScalar unloadedRadiusM,
    VehicleScalar fallbackLengthM)
{
    if (!finiteValue(normalLoadN) || !finiteValue(nominalLoadN)
        || !finiteValue(unloadedRadiusM) || !finiteValue(fallbackLengthM)
        || !finiteValue(c.pTx1) || !finiteValue(c.pTx2)
        || !finiteValue(c.pTx3) || !finiteValue(c.lSgKappa)
        || c.pTx1 <= 0.0 || nominalLoadN <= 1.0 || unloadedRadiusM <= 0.01)
    {
        return fallbackLengthM;
    }

    const VehicleScalar load = std::max(normalLoadN, VehicleScalar{1.0});
    const VehicleScalar dfz = (load - nominalLoadN) / nominalLoadN;
    const VehicleScalar length = load * (c.pTx1 + c.pTx2 * dfz)
        * std::exp(-c.pTx3 * dfz)
        * (unloadedRadiusM / nominalLoadN)
        * c.lSgKappa;
    return finiteValue(length) && length >= 0.01 && length <= 20.0
        ? length : fallbackLengthM;
}

VehicleScalar magicFormulaLateralRelaxationLengthM(
    const TireSlipDynamicsCoefficients& c,
    VehicleScalar normalLoadN,
    VehicleScalar nominalLoadN,
    VehicleScalar unloadedRadiusM,
    VehicleScalar camberAngleRadians,
    VehicleScalar lateralCamberSensitivity,
    VehicleScalar fallbackLengthM)
{
    if (!finiteValue(normalLoadN) || !finiteValue(nominalLoadN)
        || !finiteValue(unloadedRadiusM) || !finiteValue(camberAngleRadians)
        || !finiteValue(lateralCamberSensitivity) || !finiteValue(fallbackLengthM)
        || !finiteValue(c.pTy1) || !finiteValue(c.pTy2)
        || !finiteValue(c.lSgAlpha)
        || c.pTy1 <= 0.0 || c.pTy2 <= 0.01
        || nominalLoadN <= 1.0 || unloadedRadiusM <= 0.01)
    {
        return fallbackLengthM;
    }

    const VehicleScalar loadRatio = std::max(normalLoadN, VehicleScalar{1.0})
        / nominalLoadN;
    const VehicleScalar loadShape = std::sin(2.0 * std::atan(
        loadRatio / c.pTy2));
    const VehicleScalar camberScale = std::max(
        VehicleScalar{0.05},
        1.0 - lateralCamberSensitivity * std::abs(camberAngleRadians));
    const VehicleScalar length = c.pTy1 * loadShape * camberScale
        * unloadedRadiusM * c.lSgAlpha;
    return finiteValue(length) && length >= 0.01 && length <= 20.0
        ? length : fallbackLengthM;
}

bool validTireSlipDynamicsDescription(
    const TireSlipDynamicsDescription& d)
{
    return finiteValue(d.longitudinalRelaxationLengthM)
        && d.longitudinalRelaxationLengthM >= 0.01
        && d.longitudinalRelaxationLengthM <= 20.0
        && finiteValue(d.lateralRelaxationLengthM)
        && d.lateralRelaxationLengthM >= 0.01
        && d.lateralRelaxationLengthM <= 20.0
        && finiteValue(d.minimumTransportSpeedMps)
        && d.minimumTransportSpeedMps >= 0.0
        && d.minimumTransportSpeedMps <= 20.0;
}

VehicleScalar tireRelaxationBlend(
    VehicleScalar transportSpeedMps,
    VehicleScalar relaxationLengthM,
    VehicleScalar deltaTimeSeconds,
    VehicleScalar minimumTransportSpeedMps)
{
    if (!finiteValue(transportSpeedMps)
        || !finiteValue(relaxationLengthM)
        || !finiteValue(deltaTimeSeconds)
        || !finiteValue(minimumTransportSpeedMps)
        || relaxationLengthM <= 0.0
        || deltaTimeSeconds <= 0.0)
    {
        return 0.0;
    }

    const VehicleScalar transportSpeed = std::max(
        std::abs(transportSpeedMps),
        std::max(minimumTransportSpeedMps, VehicleScalar{0.0}));
    return std::clamp(
        1.0 - std::exp(-(transportSpeed / relaxationLengthM)
            * deltaTimeSeconds),
        VehicleScalar{0.0},
        VehicleScalar{1.0});
}

void integrateTireSlipDynamics(
    const TireSlipDynamicsDescription& d,
    VehicleScalar targetLongitudinalSlip,
    VehicleScalar targetSlipAngleRadians,
    VehicleScalar transportSpeedMps,
    VehicleScalar deltaTimeSeconds,
    TireSlipDynamicsState& state)
{
    if (!validTireSlipDynamicsDescription(d)
        || !finiteValue(targetLongitudinalSlip)
        || !finiteValue(targetSlipAngleRadians))
    {
        return;
    }

    const VehicleScalar longitudinalBlend = tireRelaxationBlend(
        transportSpeedMps,
        d.longitudinalRelaxationLengthM,
        deltaTimeSeconds,
        d.minimumTransportSpeedMps);
    const VehicleScalar lateralBlend = tireRelaxationBlend(
        transportSpeedMps,
        d.lateralRelaxationLengthM,
        deltaTimeSeconds,
        d.minimumTransportSpeedMps);

    state.longitudinalSlip += (targetLongitudinalSlip
        - state.longitudinalSlip) * longitudinalBlend;
    state.slipAngleRadians += (targetSlipAngleRadians
        - state.slipAngleRadians) * lateralBlend;
}

} // namespace heritage::vehicles::tires
