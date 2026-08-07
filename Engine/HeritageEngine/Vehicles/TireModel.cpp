#include "TireModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kEpsilon = 1.0e-6f;

bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

float generalizedTireCurve(
    float slip,
    float stiffness,
    float peakForce,
    float shapeFactor,
    float curvatureFactor)
{
    if (peakForce <= kEpsilon || stiffness <= kEpsilon)
        return 0.0f;

    // B is derived from the requested small-slip stiffness so changing C/E
    // changes curve shape without silently changing the linear response.
    const float B = stiffness
        / std::max(shapeFactor * peakForce, kEpsilon);
    const float Bx = B * slip;
    const float inner = Bx - curvatureFactor * (Bx - std::atan(Bx));
    return peakForce * std::sin(shapeFactor * std::atan(inner));
}

float lpNorm(float x, float y, float exponent)
{
    const float ax = std::abs(x);
    const float ay = std::abs(y);
    return std::pow(
        std::pow(ax, exponent) + std::pow(ay, exponent),
        1.0f / exponent);
}

} // namespace

bool validTireModelDescription(const TireModelDescription& value)
{
    return finiteFloat(value.nominalLoad)
        && value.nominalLoad >= 100.0f
        && value.nominalLoad <= 100000.0f
        && finiteFloat(value.peakFriction)
        && value.peakFriction >= 0.02f
        && value.peakFriction <= 5.0f
        && finiteFloat(value.longitudinalStiffness)
        && value.longitudinalStiffness >= 100.0f
        && value.longitudinalStiffness <= 2000000.0f
        && finiteFloat(value.corneringStiffness)
        && value.corneringStiffness >= 100.0f
        && value.corneringStiffness <= 2000000.0f
        && finiteFloat(value.loadSensitivity)
        && value.loadSensitivity >= 0.0f
        && value.loadSensitivity <= 0.75f
        && finiteFloat(value.longitudinalRelaxationLength)
        && value.longitudinalRelaxationLength >= 0.01f
        && value.longitudinalRelaxationLength <= 10.0f
        && finiteFloat(value.lateralRelaxationLength)
        && value.lateralRelaxationLength >= 0.01f
        && value.lateralRelaxationLength <= 10.0f
        && finiteFloat(value.wheelInertia)
        && value.wheelInertia >= 0.01f
        && value.wheelInertia <= 100.0f
        && finiteFloat(value.pneumaticTrail)
        && value.pneumaticTrail >= 0.0f
        && value.pneumaticTrail <= 1.0f
        && finiteFloat(value.stiffnessLoadExponent)
        && value.stiffnessLoadExponent >= 0.20f
        && value.stiffnessLoadExponent <= 1.50f
        && finiteFloat(value.longitudinalShapeFactor)
        && value.longitudinalShapeFactor >= 0.80f
        && value.longitudinalShapeFactor <= 2.00f
        && finiteFloat(value.lateralShapeFactor)
        && value.lateralShapeFactor >= 0.80f
        && value.lateralShapeFactor <= 2.00f
        && finiteFloat(value.longitudinalCurvatureFactor)
        && value.longitudinalCurvatureFactor >= -2.0f
        && value.longitudinalCurvatureFactor <= 0.99f
        && finiteFloat(value.lateralCurvatureFactor)
        && value.lateralCurvatureFactor >= -2.0f
        && value.lateralCurvatureFactor <= 0.99f
        && finiteFloat(value.combinedSlipExponent)
        && value.combinedSlipExponent >= 1.10f
        && value.combinedSlipExponent <= 6.0f
        && finiteFloat(value.pneumaticTrailFalloff)
        && value.pneumaticTrailFalloff >= 0.0f
        && value.pneumaticTrailFalloff <= 10.0f;
}

TireForceResult evaluateAdvancedRoadTire(
    const TireModelDescription& description,
    const TireContactInput& input)
{
    TireForceResult result;
    if (input.normalLoad <= kEpsilon || !validTireModelDescription(description))
        return result;

    const float loadRatio = std::max(
        input.normalLoad / description.nominalLoad,
        0.05f);
    result.effectiveFriction = std::clamp(
        description.peakFriction
            * std::max(input.frictionMultiplier, 0.0f)
            * std::pow(loadRatio, -description.loadSensitivity),
        0.02f,
        5.0f);

    const float peakForce = result.effectiveFriction * input.normalLoad;
    if (peakForce <= kEpsilon)
        return result;

    // Tire stiffness does not grow linearly forever with vertical load.
    // The exponent exposes that sub-linear behavior as tuneable tire data.
    const float stiffnessLoadScale = std::pow(
        loadRatio,
        description.stiffnessLoadExponent);
    const float stiffnessMultiplier = std::max(
        input.stiffnessMultiplier,
        0.0f);
    const float longitudinalStiffness = description.longitudinalStiffness
        * stiffnessMultiplier * stiffnessLoadScale;
    const float corneringStiffness = description.corneringStiffness
        * stiffnessMultiplier * stiffnessLoadScale;

    result.pureLongitudinalForce = generalizedTireCurve(
        input.longitudinalSlip,
        longitudinalStiffness,
        peakForce,
        description.longitudinalShapeFactor,
        description.longitudinalCurvatureFactor);
    result.pureLateralForce = -generalizedTireCurve(
        input.slipAngleRadians,
        corneringStiffness,
        peakForce,
        description.lateralShapeFactor,
        description.lateralCurvatureFactor);

    const float normalizedLongitudinal = result.pureLongitudinalForce
        / peakForce;
    const float normalizedLateral = result.pureLateralForce / peakForce;
    const float requestedUtilization = lpNorm(
        normalizedLongitudinal,
        normalizedLateral,
        description.combinedSlipExponent);

    result.combinedSlipScale = requestedUtilization > 1.0f
        ? 1.0f / requestedUtilization
        : 1.0f;
    result.longitudinalForce = result.pureLongitudinalForce
        * result.combinedSlipScale;
    result.lateralForce = result.pureLateralForce
        * result.combinedSlipScale;
    result.gripUtilization = std::clamp(
        lpNorm(
            result.longitudinalForce / peakForce,
            result.lateralForce / peakForce,
            description.combinedSlipExponent),
        0.0f,
        1.0f);

    // Pneumatic trail decays as lateral slip saturates and is also shortened
    // by strong longitudinal demand. This gives aligning torque a natural
    // rise-and-fall instead of tying it only to total grip utilization.
    const float characteristicAngle = std::max(
        peakForce / std::max(corneringStiffness, kEpsilon),
        0.005f);
    const float lateralSaturation = std::abs(input.slipAngleRadians)
        / characteristicAngle;
    const float longitudinalUse = std::clamp(
        std::abs(result.longitudinalForce) / peakForce,
        0.0f,
        1.0f);
    result.pneumaticTrail = description.pneumaticTrail
        * std::exp(-description.pneumaticTrailFalloff * lateralSaturation)
        * (1.0f - 0.35f * longitudinalUse);
    result.aligningTorque = -result.lateralForce * result.pneumaticTrail;
    return result;
}

} // namespace heritage::vehicles
