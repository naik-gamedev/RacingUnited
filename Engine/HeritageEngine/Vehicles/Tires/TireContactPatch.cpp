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

VehicleScalar smoothStep(VehicleScalar edge0, VehicleScalar edge1, VehicleScalar value)
{
    const VehicleScalar t = std::clamp(
        (value - edge0) / std::max(edge1 - edge0, kEpsilon),
        VehicleScalar{0.0}, VehicleScalar{1.0});
    return t * t * (VehicleScalar{3.0} - VehicleScalar{2.0} * t);
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
        && finiteValue(d.stationaryNeutralRecoveryTimeSeconds)
        && d.stationaryNeutralRecoveryTimeSeconds >= 0.01
        && d.stationaryNeutralRecoveryTimeSeconds <= 10.0
        && finiteValue(d.neutralSteerThresholdRadians)
        && d.neutralSteerThresholdRadians >= 0.001
        && d.neutralSteerThresholdRadians <= 0.35
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
        || !finiteValue(input.wheelSteerAngleRadians)
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
    const VehicleScalar rollingReleaseRate = speed
        / std::max(d.torsionalRelaxationLengthM, kEpsilon);
    const VehicleScalar absoluteSteerAngle = std::abs(
        input.wheelSteerAngleRadians);
    const VehicleScalar neutralRecoveryBlend = VehicleScalar{1.0}
        - smoothStep(
            d.neutralSteerThresholdRadians,
            d.neutralSteerThresholdRadians * VehicleScalar{2.0},
            absoluteSteerAngle);
    const VehicleScalar stationaryNeutralReleaseRate = neutralRecoveryBlend
        / std::max(d.stationaryNeutralRecoveryTimeSeconds, kEpsilon);
    const VehicleScalar releaseRate = rollingReleaseRate
        + stationaryNeutralReleaseRate;

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

    // The observable elastic response is smoothly saturated, while the
    // internal accumulator must cover an ordinary lock-to-lock steering sweep.
    // A too-small hidden cap clips the outward sweep and leaves a false residual
    // twist when the same steering motion returns to centre.
    const VehicleScalar stateLimit = std::max(
        VehicleScalar{3.14159265358979323846},
        VehicleScalar{8.0} * d.maximumElasticTwistRadians);
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
