#include "PushrodDoubleWishboneKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-6f;
constexpr float kMinimumGeometryLength = 0.005f;
constexpr float kMotionRatioDerivativeStepM = 0.0005f;

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(const heritage::math::Vec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float factor)
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(std::max(dot(value, value), 0.0f));
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    return magnitude > kEpsilon ? scale(value, 1.0f / magnitude) : fallback;
}

heritage::math::Vec3 rotateAroundAxis(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& unitAxis,
    float angleRadians)
{
    const float cosine = std::cos(angleRadians);
    const float sine = std::sin(angleRadians);
    return add(
        add(
            scale(value, cosine),
            scale(cross(unitAxis, value), sine)),
        scale(unitAxis, dot(unitAxis, value) * (1.0f - cosine)));
}

heritage::math::Vec3 rotatePointAroundLine(
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& linePoint,
    const heritage::math::Vec3& lineAxis,
    float angleRadians)
{
    return add(
        linePoint,
        rotateAroundAxis(
            subtract(point, linePoint),
            normalized(lineAxis, { 0.0f, 0.0f, 1.0f }),
            angleRadians));
}

float wrapPi(float angle)
{
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

struct ActuationPose
{
    bool valid = false;
    bool travelClamped = false;
    DoubleWishboneKinematicsOutput linkage{};
    float rockerAngleRadians = 0.0f;
    float springCompressionM = 0.0f;
    float damperCompressionM = 0.0f;
    float pushrodLengthErrorM = 0.0f;
    heritage::math::Vec3 pushrodLowerMount{};
    heritage::math::Vec3 rockerPushrodMount{};
    heritage::math::Vec3 springRockerMount{};
    heritage::math::Vec3 damperRockerMount{};
};

bool solveRockerAngle(
    const PushrodDoubleWishboneHardpoints& hardpoints,
    const heritage::math::Vec3& movingPushrodOuter,
    float& angleRadians)
{
    const heritage::math::Vec3 pivotAxis = normalized(
        subtract(hardpoints.rockerPivotRear, hardpoints.rockerPivotFront),
        { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 center = hardpoints.rockerPivotFront;
    const heritage::math::Vec3 inputOffset = subtract(
        hardpoints.rockerPushrodMount, center);
    const float axial = dot(inputOffset, pivotAxis);
    const heritage::math::Vec3 axialOffset = scale(pivotAxis, axial);
    const heritage::math::Vec3 radial = subtract(inputOffset, axialOffset);
    const float radialLength = length(radial);
    if (radialLength <= kMinimumGeometryLength)
        return false;

    const float referencePushrodLength = length(subtract(
        hardpoints.pushrodLowerArmMount,
        hardpoints.rockerPushrodMount));
    if (referencePushrodLength <= kMinimumGeometryLength)
        return false;

    // The rocker input travels on a circle. Resolve the pushrod-length
    // constraint analytically as A*cos(theta)+B*sin(theta)=target, then choose
    // the solution on the reference branch nearest theta=0.
    const heritage::math::Vec3 q = subtract(
        subtract(movingPushrodOuter, center), axialOffset);
    const heritage::math::Vec3 e1 = scale(radial, 1.0f / radialLength);
    const heritage::math::Vec3 e2 = normalized(
        cross(pivotAxis, e1), { 0.0f, 1.0f, 0.0f });
    const float a = dot(e1, q);
    const float b = dot(e2, q);
    const float amplitude = std::sqrt(a * a + b * b);
    if (amplitude <= kEpsilon)
        return false;

    const float target = (radialLength * radialLength + dot(q, q)
        - referencePushrodLength * referencePushrodLength)
        / (2.0f * radialLength);
    const float normalizedTarget = target / amplitude;
    if (normalizedTarget < -1.0005f || normalizedTarget > 1.0005f)
        return false;

    const float phase = std::atan2(b, a);
    const float delta = std::acos(std::clamp(normalizedTarget, -1.0f, 1.0f));
    const float first = wrapPi(phase + delta);
    const float second = wrapPi(phase - delta);
    angleRadians = std::abs(first) <= std::abs(second) ? first : second;
    return finite(angleRadians) && std::abs(angleRadians) <= 2.60f;
}

ActuationPose solveActuationPose(
    const PushrodDoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input,
    float compressionM)
{
    ActuationPose output;
    DoubleWishboneKinematicsInput linkageInput = input;
    linkageInput.compressionM = compressionM;
    output.linkage = evaluateDoubleWishboneLinkageKinematics(
        hardpoints.wishbone, linkageInput);
    if (!output.linkage.valid)
        return output;

    const heritage::math::Vec3 lowerAxis = normalized(
        subtract(
            hardpoints.wishbone.lowerArmInnerRear,
            hardpoints.wishbone.lowerArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    output.pushrodLowerMount = rotatePointAroundLine(
        hardpoints.pushrodLowerArmMount,
        hardpoints.wishbone.lowerArmInnerFront,
        lowerAxis,
        output.linkage.lowerArmRotationRadians);

    if (!solveRockerAngle(
            hardpoints,
            output.pushrodLowerMount,
            output.rockerAngleRadians))
    {
        return output;
    }

    const heritage::math::Vec3 rockerAxis = normalized(
        subtract(hardpoints.rockerPivotRear, hardpoints.rockerPivotFront),
        { 0.0f, 0.0f, 1.0f });
    output.rockerPushrodMount = rotatePointAroundLine(
        hardpoints.rockerPushrodMount,
        hardpoints.rockerPivotFront,
        rockerAxis,
        output.rockerAngleRadians);
    output.springRockerMount = rotatePointAroundLine(
        hardpoints.springRockerMount,
        hardpoints.rockerPivotFront,
        rockerAxis,
        output.rockerAngleRadians);
    output.damperRockerMount = rotatePointAroundLine(
        hardpoints.damperRockerMount,
        hardpoints.rockerPivotFront,
        rockerAxis,
        output.rockerAngleRadians);

    const float referenceSpringLength = length(subtract(
        hardpoints.springRockerMount, hardpoints.springChassisMount));
    const float currentSpringLength = length(subtract(
        output.springRockerMount, hardpoints.springChassisMount));
    const float referenceDamperLength = length(subtract(
        hardpoints.damperRockerMount, hardpoints.damperChassisMount));
    const float currentDamperLength = length(subtract(
        output.damperRockerMount, hardpoints.damperChassisMount));
    const float currentPushrodLength = length(subtract(
        output.pushrodLowerMount, output.rockerPushrodMount));
    const float referencePushrodLength = length(subtract(
        hardpoints.pushrodLowerArmMount,
        hardpoints.rockerPushrodMount));

    output.springCompressionM = referenceSpringLength - currentSpringLength;
    output.damperCompressionM = referenceDamperLength - currentDamperLength;
    output.pushrodLengthErrorM = currentPushrodLength - referencePushrodLength;
    output.travelClamped = output.linkage.travelClamped;
    output.valid = finite(output.springCompressionM)
        && finite(output.damperCompressionM)
        && finite(output.pushrodLengthErrorM)
        && std::abs(output.pushrodLengthErrorM) <= 0.0005f;
    return output;
}

} // namespace

bool validPushrodDoubleWishboneHardpoints(
    const PushrodDoubleWishboneHardpoints& hardpoints)
{
    if (!hardpoints.authored
        || !validDoubleWishboneLinkageHardpoints(hardpoints.wishbone)
        || !finite(hardpoints.pushrodLowerArmMount)
        || !finite(hardpoints.rockerPivotFront)
        || !finite(hardpoints.rockerPivotRear)
        || !finite(hardpoints.rockerPushrodMount)
        || !finite(hardpoints.springChassisMount)
        || !finite(hardpoints.springRockerMount)
        || !finite(hardpoints.damperChassisMount)
        || !finite(hardpoints.damperRockerMount))
    {
        return false;
    }

    const heritage::math::Vec3 rockerAxis = normalized(
        subtract(hardpoints.rockerPivotRear, hardpoints.rockerPivotFront),
        { 0.0f, 0.0f, 1.0f });
    const float rockerInputRadius = length(cross(
        subtract(hardpoints.rockerPushrodMount, hardpoints.rockerPivotFront),
        rockerAxis));
    const float springRockerRadius = length(cross(
        subtract(hardpoints.springRockerMount, hardpoints.rockerPivotFront),
        rockerAxis));
    const float damperRockerRadius = length(cross(
        subtract(hardpoints.damperRockerMount, hardpoints.rockerPivotFront),
        rockerAxis));

    return length(subtract(
            hardpoints.rockerPivotRear,
            hardpoints.rockerPivotFront)) > kMinimumGeometryLength
        && rockerInputRadius > 0.01f
        && springRockerRadius > 0.01f
        && damperRockerRadius > 0.01f
        && length(subtract(
            hardpoints.pushrodLowerArmMount,
            hardpoints.rockerPushrodMount)) > 0.03f
        && length(subtract(
            hardpoints.springRockerMount,
            hardpoints.springChassisMount)) > 0.03f
        && length(subtract(
            hardpoints.damperRockerMount,
            hardpoints.damperChassisMount)) > 0.03f;
}

PushrodDoubleWishboneKinematicsOutput evaluatePushrodDoubleWishboneKinematics(
    const PushrodDoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input)
{
    PushrodDoubleWishboneKinematicsOutput output;
    if (!validPushrodDoubleWishboneHardpoints(hardpoints)
        || !finite(input.compressionM)
        || !finite(input.steeringDegrees)
        || !finite(input.staticCamberDegrees)
        || !finite(input.staticToeDegrees))
    {
        return output;
    }

    const ActuationPose current = solveActuationPose(
        hardpoints, input, input.compressionM);
    const ActuationPose plus = solveActuationPose(
        hardpoints, input, input.compressionM + kMotionRatioDerivativeStepM);
    const ActuationPose minus = solveActuationPose(
        hardpoints, input, input.compressionM - kMotionRatioDerivativeStepM);
    if (!current.valid || !plus.valid || !minus.valid)
        return output;

    const float springRatio = (plus.springCompressionM - minus.springCompressionM)
        / (2.0f * kMotionRatioDerivativeStepM);
    const float damperRatio = (plus.damperCompressionM - minus.damperCompressionM)
        / (2.0f * kMotionRatioDerivativeStepM);
    // A conventional pushrod suspension must compress its spring/damper under
    // positive wheel bump on the reference branch. Reject crossed/dead-center
    // layouts instead of taking abs() and silently reversing virtual work.
    if (!finite(springRatio) || !finite(damperRatio)
        || springRatio <= 0.02f || damperRatio <= 0.02f
        || springRatio > 8.0f || damperRatio > 8.0f)
    {
        return output;
    }

    output.linkage = current.linkage;
    output.rockerAngleRadians = current.rockerAngleRadians;
    output.springCompressionM = current.springCompressionM;
    output.springMotionRatio = springRatio;
    output.damperCompressionM = current.damperCompressionM;
    output.damperMotionRatio = damperRatio;
    output.pushrodLengthErrorM = current.pushrodLengthErrorM;
    output.localPushrodLowerMount = current.pushrodLowerMount;
    output.localRockerPushrodMount = current.rockerPushrodMount;
    output.localSpringRockerMount = current.springRockerMount;
    output.localDamperRockerMount = current.damperRockerMount;
    output.travelClamped = current.travelClamped;
    output.valid = true;
    return output;
}

} // namespace heritage::vehicles
