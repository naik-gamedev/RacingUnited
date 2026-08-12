#include "TireContactPatch.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar signNonZero(VehicleScalar value)
{
    if (value > 0.0) return 1.0;
    if (value < 0.0) return -1.0;
    return 0.0;
}

} // namespace

bool validTireContactPatchDescription(
    const TireContactPatchDescription& d)
{
    return finiteValue(d.torsionalRelaxationLengthM)
        && d.torsionalRelaxationLengthM >= 0.01
        && d.torsionalRelaxationLengthM <= 5.0
        && finiteValue(d.maximumElasticTwistRadians)
        && d.maximumElasticTwistRadians >= 0.005
        && d.maximumElasticTwistRadians <= 1.0
        && finiteValue(d.turnSlipRegularizationSpeedMps)
        && d.turnSlipRegularizationSpeedMps >= 0.05
        && d.turnSlipRegularizationSpeedMps <= 10.0
        && finiteValue(d.parkingMomentTransitionSpeedMps)
        && d.parkingMomentTransitionSpeedMps >= 0.05
        && d.parkingMomentTransitionSpeedMps <= 20.0;
}

TireContactPatchOutput integrateTireContactPatch(
    const TireContactPatchDescription& d,
    const TireContactPatchInput& input,
    VehicleScalar deltaTimeSeconds,
    TireContactPatchState& state)
{
    TireContactPatchOutput out;
    if (!validTireContactPatchDescription(d)
        || !finiteValue(input.wheelYawRateRadiansPerSecond)
        || !finiteValue(input.forwardSpeedMps)
        || !finiteValue(input.normalLoadN)
        || !finiteValue(input.effectiveFriction)
        || !finiteValue(input.unloadedRadiusM)
        || !finiteValue(input.zeroSpeedTurnMomentCoefficient)
        || !finiteValue(input.parkingMomentScale)
        || !finiteValue(deltaTimeSeconds)
        || deltaTimeSeconds <= 0.0)
    {
        out.torsionalTwistRadians = state.torsionalTwistRadians;
        return out;
    }

    const VehicleScalar speed = std::abs(input.forwardSpeedMps);
    const VehicleScalar releaseRate = speed
        / std::max(d.torsionalRelaxationLengthM, kEpsilon);

    // Exact integration of d(theta)/dt = yawRate - releaseRate*theta for a
    // constant input over the substep. This keeps the parking state nearly
    // invariant when the vehicle high-rate loop is changed between 1000 Hz and
    // lower regression rates.
    if (releaseRate > kEpsilon)
    {
        const VehicleScalar decay = std::exp(-releaseRate * deltaTimeSeconds);
        state.torsionalTwistRadians = state.torsionalTwistRadians * decay
            + input.wheelYawRateRadiansPerSecond / releaseRate
                * (1.0 - decay);
    }
    else
    {
        state.torsionalTwistRadians +=
            input.wheelYawRateRadiansPerSecond * deltaTimeSeconds;
    }

    // Keep numerical state bounded even during pathological steering input.
    // The actual elastic response is smoothly saturated below this hard cap.
    const VehicleScalar stateLimit = 4.0 * d.maximumElasticTwistRadians;
    state.torsionalTwistRadians = std::clamp(
        state.torsionalTwistRadians, -stateLimit, stateLimit);

    const VehicleScalar effectiveTwist = d.maximumElasticTwistRadians
        * std::tanh(state.torsionalTwistRadians
            / d.maximumElasticTwistRadians);

    const VehicleScalar regularizedSpeed = std::sqrt(
        input.forwardSpeedMps * input.forwardSpeedMps
        + d.turnSlipRegularizationSpeedMps
            * d.turnSlipRegularizationSpeedMps);
    out.turnSlipPerM = input.wheelYawRateRadiansPerSecond
        / std::max(regularizedSpeed, kEpsilon);

    const VehicleScalar transitionRatio = speed
        / d.parkingMomentTransitionSpeedMps;
    out.parkingMomentBlend = 1.0
        / (1.0 + transitionRatio * transitionRatio);

    const VehicleScalar momentCapacity = std::max(
        input.zeroSpeedTurnMomentCoefficient, VehicleScalar{0.0})
        * std::max(input.effectiveFriction, VehicleScalar{0.0})
        * std::max(input.normalLoadN, VehicleScalar{0.0})
        * std::max(input.unloadedRadiusM, VehicleScalar{0.0})
        * std::max(input.parkingMomentScale, VehicleScalar{0.0});
    const VehicleScalar normalizedTwist = std::abs(effectiveTwist)
        / d.maximumElasticTwistRadians;
    out.parkingTurnMomentNm = -signNonZero(effectiveTwist)
        * momentCapacity * normalizedTwist * out.parkingMomentBlend;
    out.torsionalTwistRadians = effectiveTwist;
    return out;
}

} // namespace heritage::vehicles::tires
