#include "TrailingArmKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;
constexpr float kMinimumGeometryLength = 0.001f;
constexpr float kDerivativeStepM = 0.00025f;

float radians(float degreesValue)
{
    return degreesValue * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
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

float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(lengthSquared(value));
}

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    if (magnitude <= kEpsilon)
        return fallback;
    return scale(value, 1.0f / magnitude);
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
    const heritage::math::Vec3& unitAxis,
    float angleRadians)
{
    return add(
        linePoint,
        rotateAroundAxis(subtract(point, linePoint), unitAxis, angleRadians));
}

float wrapRadians(float angle)
{
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

bool solveNearestZeroTrig(
    float a,
    float b,
    float target,
    float& angleRadians,
    bool& clamped)
{
    const float magnitude = std::sqrt(a * a + b * b);
    if (magnitude <= kEpsilon)
        return false;

    float normalizedTarget = target / magnitude;
    clamped = normalizedTarget < -1.0f || normalizedTarget > 1.0f;
    normalizedTarget = std::clamp(normalizedTarget, -1.0f, 1.0f);
    const float phase = std::atan2(b, a);
    const float offset = std::acos(normalizedTarget);
    const float candidateA = wrapRadians(phase + offset);
    const float candidateB = wrapRadians(phase - offset);
    angleRadians = std::abs(candidateA) <= std::abs(candidateB)
        ? candidateA : candidateB;
    return true;
}

struct ArmPose
{
    bool valid = false;
    bool clamped = false;
    float angleRadians = 0.0f;
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 damperLowerMount{};
};

ArmPose poseAtCompression(
    const TrailingArmHardpoints& hardpoints,
    const heritage::math::Vec3& suspensionDirection,
    float compressionM)
{
    ArmPose pose;
    const heritage::math::Vec3 pivotAxis = normalized(
        subtract(hardpoints.armPivotOuter, hardpoints.armPivotInner),
        { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 referenceVector = subtract(
        hardpoints.wheelCenter, hardpoints.armPivotInner);
    const heritage::math::Vec3 radialVector = subtract(
        referenceVector,
        scale(pivotAxis, dot(referenceVector, pivotAxis)));
    if (length(radialVector) <= kMinimumGeometryLength)
        return pose;

    const heritage::math::Vec3 compressionDirection = scale(
        normalized(suspensionDirection, { 0.0f, -1.0f, 0.0f }),
        -1.0f);
    const float a = dot(compressionDirection, radialVector);
    const float b = dot(
        compressionDirection,
        cross(pivotAxis, radialVector));
    const float target = compressionM + a;
    if (!solveNearestZeroTrig(
            a,
            b,
            target,
            pose.angleRadians,
            pose.clamped))
    {
        return pose;
    }

    pose.wheelCenter = rotatePointAroundLine(
        hardpoints.wheelCenter,
        hardpoints.armPivotInner,
        pivotAxis,
        pose.angleRadians);
    pose.damperLowerMount = rotatePointAroundLine(
        hardpoints.damperLowerMount,
        hardpoints.armPivotInner,
        pivotAxis,
        pose.angleRadians);
    pose.valid = finite(pose.wheelCenter) && finite(pose.damperLowerMount);
    return pose;
}

float damperCompressionAt(
    const TrailingArmHardpoints& hardpoints,
    const heritage::math::Vec3& suspensionDirection,
    float compressionM)
{
    const ArmPose pose = poseAtCompression(
        hardpoints, suspensionDirection, compressionM);
    if (!pose.valid)
        return 0.0f;
    const float referenceLength = length(subtract(
        hardpoints.damperLowerMount, hardpoints.damperUpperMount));
    const float currentLength = length(subtract(
        pose.damperLowerMount, hardpoints.damperUpperMount));
    return referenceLength - currentLength;
}

float armAngleAt(
    const TrailingArmHardpoints& hardpoints,
    const heritage::math::Vec3& suspensionDirection,
    float compressionM)
{
    const ArmPose pose = poseAtCompression(
        hardpoints, suspensionDirection, compressionM);
    return pose.valid ? pose.angleRadians : 0.0f;
}

float derivativeCentral(
    float centerCompressionM,
    const auto& sample)
{
    const float positive = sample(centerCompressionM + kDerivativeStepM);
    const float negative = sample(centerCompressionM - kDerivativeStepM);
    return (positive - negative) / (2.0f * kDerivativeStepM);
}

heritage::math::Vec3 eulerDegreesFromBasis(
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward)
{
    const float y = std::asin(std::clamp(-right.z, -1.0f, 1.0f));
    const float cosineY = std::cos(y);
    float x = 0.0f;
    float z = 0.0f;
    if (std::abs(cosineY) > kEpsilon)
    {
        x = std::atan2(up.z, forward.z);
        z = std::atan2(right.y, right.x);
    }
    else
    {
        x = std::atan2(-forward.y, up.y);
    }
    return { degrees(x), degrees(y), degrees(z) };
}

float horizontalToeDegrees(const heritage::math::Vec3& forward)
{
    return degrees(std::atan2(forward.x, forward.z));
}

float camberDegreesFromRight(const heritage::math::Vec3& right)
{
    return degrees(std::atan2(right.y, right.x));
}

} // namespace

bool validTrailingArmHardpoints(const TrailingArmHardpoints& hardpoints)
{
    if (!hardpoints.authored
        || !finite(hardpoints.armPivotInner)
        || !finite(hardpoints.armPivotOuter)
        || !finite(hardpoints.wheelCenter)
        || !finite(hardpoints.damperUpperMount)
        || !finite(hardpoints.damperLowerMount))
    {
        return false;
    }

    const heritage::math::Vec3 axis = subtract(
        hardpoints.armPivotOuter, hardpoints.armPivotInner);
    const float axisLength = length(axis);
    if (axisLength <= kMinimumGeometryLength)
        return false;

    const heritage::math::Vec3 unitAxis = scale(axis, 1.0f / axisLength);
    const heritage::math::Vec3 wheelRadiusVector = subtract(
        hardpoints.wheelCenter, hardpoints.armPivotInner);
    const float armRadius = length(cross(wheelRadiusVector, unitAxis));
    const float damperReferenceLength = length(subtract(
        hardpoints.damperLowerMount, hardpoints.damperUpperMount));
    const float damperArmRadius = length(cross(
        subtract(hardpoints.damperLowerMount, hardpoints.armPivotInner),
        unitAxis));
    return armRadius > 0.05f
        && armRadius < 3.0f
        && damperReferenceLength > 0.03f
        && damperReferenceLength < 3.0f
        && damperArmRadius > 0.01f
        && damperArmRadius < 3.0f;
}

TrailingArmKinematicsOutput evaluateTrailingArmKinematics(
    const TrailingArmHardpoints& hardpoints,
    const TrailingArmKinematicsInput& input)
{
    TrailingArmKinematicsOutput output;
    if (!validTrailingArmHardpoints(hardpoints)
        || !std::isfinite(input.compressionM)
        || !finite(input.suspensionDirection)
        || !std::isfinite(input.staticCamberDegrees)
        || !std::isfinite(input.staticToeDegrees))
    {
        return output;
    }

    const ArmPose pose = poseAtCompression(
        hardpoints, input.suspensionDirection, input.compressionM);
    if (!pose.valid)
        return output;

    const heritage::math::Vec3 pivotAxis = normalized(
        subtract(hardpoints.armPivotOuter, hardpoints.armPivotInner),
        { 1.0f, 0.0f, 0.0f });
    const float toe = radians(input.staticToeDegrees);
    const float camber = radians(input.staticCamberDegrees);
    heritage::math::Vec3 forward = rotateAroundAxis(
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, toe);
    heritage::math::Vec3 right = rotateAroundAxis(
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, toe);
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    right = rotateAroundAxis(right, forward, camber);
    up = rotateAroundAxis(up, forward, camber);

    forward = rotateAroundAxis(forward, pivotAxis, pose.angleRadians);
    right = rotateAroundAxis(right, pivotAxis, pose.angleRadians);
    up = rotateAroundAxis(up, pivotAxis, pose.angleRadians);
    forward = normalized(forward, { 0.0f, 0.0f, 1.0f });
    right = normalized(right, { 1.0f, 0.0f, 0.0f });
    up = normalized(cross(forward, right), { 0.0f, 1.0f, 0.0f });
    right = normalized(cross(up, forward), { 1.0f, 0.0f, 0.0f });

    const auto angleSample = [&](float compression) {
        return armAngleAt(hardpoints, input.suspensionDirection, compression);
    };
    const float currentAngularRatio = std::abs(derivativeCentral(
        input.compressionM, angleSample));
    const float referenceAngularRatio = std::abs(derivativeCentral(
        0.0f, angleSample));
    const auto damperSample = [&](float compression) {
        return damperCompressionAt(
            hardpoints, input.suspensionDirection, compression);
    };

    output.valid = true;
    output.travelClamped = pose.clamped;
    output.armRotationRadians = pose.angleRadians;
    output.torsionBarTwistRadians = std::copysign(
        std::abs(pose.angleRadians), input.compressionM);
    if (std::abs(input.compressionM) <= kEpsilon)
        output.torsionBarTwistRadians = 0.0f;
    output.torsionBarAngularMotionRatioRadPerM = currentAngularRatio;
    output.referenceTorsionBarAngularMotionRatioRadPerM =
        referenceAngularRatio;
    output.damperCompressionM = damperCompressionAt(
        hardpoints, input.suspensionDirection, input.compressionM);
    output.damperMotionRatio = derivativeCentral(
        input.compressionM, damperSample);
    output.camberDegrees = camberDegreesFromRight(right);
    output.toeDegrees = horizontalToeDegrees(forward);
    output.localWheelCenter = pose.wheelCenter;
    output.localWheelForward = forward;
    output.localWheelRight = right;
    output.localWheelUp = up;
    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        right, up, forward);
    return output;
}

} // namespace heritage::vehicles
