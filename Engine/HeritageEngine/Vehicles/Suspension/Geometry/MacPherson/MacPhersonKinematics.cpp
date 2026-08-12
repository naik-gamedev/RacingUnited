#include "MacPhersonKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;
constexpr float kMinimumGeometryLength = 0.001f;

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

heritage::math::Vec3 steeringAxisWithCaster(
    const heritage::math::Vec3& value,
    float casterDegrees)
{
    const heritage::math::Vec3 axis = normalized(
        value, { 0.0f, 1.0f, 0.0f });
    const float yzMagnitude = std::sqrt(
        axis.y * axis.y + axis.z * axis.z);
    if (yzMagnitude <= kEpsilon)
        return axis;
    const float caster = radians(casterDegrees);
    return normalized(
        { axis.x,
          yzMagnitude * std::cos(caster),
          -yzMagnitude * std::sin(caster) },
        axis);
}

float wrapRadians(float angle)
{
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

// Solves A*cos(theta) + B*sin(theta) = target and chooses the solution nearest
// zero. At reference ride height the correct lower-arm solution is therefore
// theta=0 instead of the mathematically valid opposite branch.
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

bool lowerBallJointAtCompression(
    const MacPhersonHardpoints& hardpoints,
    const heritage::math::Vec3& suspensionDirection,
    float compressionM,
    heritage::math::Vec3& lowerBallJoint,
    bool& travelClamped)
{
    const heritage::math::Vec3 hingeAxis = normalized(
        subtract(
            hardpoints.lowerArmInnerRear,
            hardpoints.lowerArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 referenceVector = subtract(
        hardpoints.lowerBallJoint,
        hardpoints.lowerArmInnerFront);
    const heritage::math::Vec3 radialVector = subtract(
        referenceVector,
        scale(hingeAxis, dot(referenceVector, hingeAxis)));
    if (length(radialVector) <= kMinimumGeometryLength)
        return false;

    const heritage::math::Vec3 compressionDirection = scale(
        normalized(suspensionDirection, { 0.0f, -1.0f, 0.0f }),
        -1.0f);
    const float a = dot(compressionDirection, radialVector);
    const float b = dot(
        compressionDirection,
        cross(hingeAxis, radialVector));
    // Projection of R(theta)r-r onto compressionDirection equals compression.
    const float target = compressionM + a;
    float lowerArmAngle = 0.0f;
    if (!solveNearestZeroTrig(
            a,
            b,
            target,
            lowerArmAngle,
            travelClamped))
    {
        return false;
    }

    lowerBallJoint = rotatePointAroundLine(
        hardpoints.lowerBallJoint,
        hardpoints.lowerArmInnerFront,
        hingeAxis,
        lowerArmAngle);
    return finite(lowerBallJoint);
}

heritage::math::Vec3 alignVectorBetweenAxes(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& referenceAxis,
    const heritage::math::Vec3& currentAxis)
{
    const heritage::math::Vec3 from = normalized(
        referenceAxis, { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 to = normalized(
        currentAxis, { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 rotationAxis = cross(from, to);
    const float axisLength = length(rotationAxis);
    const float cosine = std::clamp(dot(from, to), -1.0f, 1.0f);
    if (axisLength <= kEpsilon)
    {
        if (cosine > 0.0f)
            return value;
        heritage::math::Vec3 fallbackAxis = cross(
            from, { 1.0f, 0.0f, 0.0f });
        if (length(fallbackAxis) <= kEpsilon)
            fallbackAxis = cross(from, { 0.0f, 0.0f, 1.0f });
        fallbackAxis = normalized(fallbackAxis, { 0.0f, 0.0f, 1.0f });
        return rotateAroundAxis(value, fallbackAxis, kPi);
    }
    return rotateAroundAxis(
        value,
        scale(rotationAxis, 1.0f / axisLength),
        std::atan2(axisLength, cosine));
}

struct TravelPose
{
    bool valid = false;
    bool clamped = false;
    heritage::math::Vec3 lowerBallJoint{};
    heritage::math::Vec3 steeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 strutUprightMount{};
    heritage::math::Vec3 tieRodOuter{};
    heritage::math::Vec3 wheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 wheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 wheelUp{ 0.0f, 1.0f, 0.0f };
};

TravelPose baseTravelPose(
    const MacPhersonHardpoints& hardpoints,
    const MacPhersonKinematicsInput& input,
    float compressionM)
{
    TravelPose pose;
    if (!lowerBallJointAtCompression(
            hardpoints,
            input.suspensionDirection,
            compressionM,
            pose.lowerBallJoint,
            pose.clamped))
    {
        return pose;
    }

    const heritage::math::Vec3 referenceSteeringAxis = normalized(
        subtract(hardpoints.strutTopMount, hardpoints.lowerBallJoint),
        { 0.0f, 1.0f, 0.0f });
    pose.steeringAxis = normalized(
        subtract(hardpoints.strutTopMount, pose.lowerBallJoint),
        referenceSteeringAxis);

    const auto transformUprightPoint = [&](const heritage::math::Vec3& point) {
        const heritage::math::Vec3 referenceOffset = subtract(
            point, hardpoints.lowerBallJoint);
        return add(
            pose.lowerBallJoint,
            alignVectorBetweenAxes(
                referenceOffset,
                referenceSteeringAxis,
                pose.steeringAxis));
    };

    pose.wheelCenter = transformUprightPoint(hardpoints.wheelCenter);
    pose.strutUprightMount = transformUprightPoint(
        hardpoints.strutUprightMount);
    pose.tieRodOuter = transformUprightPoint(hardpoints.tieRodOuter);

    const float toe = radians(input.staticToeDegrees);
    const float camber = radians(input.staticCamberDegrees);
    heritage::math::Vec3 forward = rotateAroundAxis(
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, toe);
    heritage::math::Vec3 right = rotateAroundAxis(
        { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, toe);
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    right = rotateAroundAxis(right, forward, camber);
    up = rotateAroundAxis(up, forward, camber);

    pose.wheelForward = alignVectorBetweenAxes(
        forward, referenceSteeringAxis, pose.steeringAxis);
    pose.wheelRight = alignVectorBetweenAxes(
        right, referenceSteeringAxis, pose.steeringAxis);
    pose.wheelUp = alignVectorBetweenAxes(
        up, referenceSteeringAxis, pose.steeringAxis);
    pose.valid = finite(pose.wheelCenter)
        && finite(pose.strutUprightMount)
        && finite(pose.tieRodOuter);
    return pose;
}

float solveBumpSteerRadians(
    const MacPhersonHardpoints& hardpoints,
    const TravelPose& pose)
{
    const float referenceTieRodLength = length(subtract(
        hardpoints.tieRodOuter, hardpoints.tieRodInner));
    if (referenceTieRodLength <= kMinimumGeometryLength)
        return 0.0f;

    const heritage::math::Vec3 q = subtract(
        pose.tieRodOuter, pose.lowerBallJoint);
    const heritage::math::Vec3 c = subtract(
        hardpoints.tieRodInner, pose.lowerBallJoint);
    const heritage::math::Vec3 qParallel = scale(
        pose.steeringAxis, dot(q, pose.steeringAxis));
    const heritage::math::Vec3 qPerpendicular = subtract(q, qParallel);
    const float constantTerm = dot(c, qParallel);
    const float a = dot(c, qPerpendicular);
    const float b = dot(c, cross(pose.steeringAxis, qPerpendicular));
    const float target = 0.5f * (
        lengthSquared(q) + lengthSquared(c)
        - referenceTieRodLength * referenceTieRodLength)
        - constantTerm;

    float angle = 0.0f;
    bool clamped = false;
    if (!solveNearestZeroTrig(a, b, target, angle, clamped))
        return 0.0f;
    // A real linkage can become singular near lock. Keep the passive correction
    // bounded; impossible geometry is reported through travelClamped elsewhere
    // rather than injecting a violent wheel-angle discontinuity.
    return std::clamp(angle, radians(-30.0f), radians(30.0f));
}

float strutCompressionAt(
    const MacPhersonHardpoints& hardpoints,
    const MacPhersonKinematicsInput& input,
    float compressionM)
{
    const TravelPose pose = baseTravelPose(hardpoints, input, compressionM);
    if (!pose.valid)
        return 0.0f;
    const float referenceLength = length(subtract(
        hardpoints.strutUprightMount,
        hardpoints.strutTopMount));
    const float currentLength = length(subtract(
        pose.strutUprightMount,
        hardpoints.strutTopMount));
    return referenceLength - currentLength;
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

bool validMacPhersonHardpoints(const MacPhersonHardpoints& hardpoints)
{
    if (!hardpoints.authored
        || !finite(hardpoints.strutTopMount)
        || !finite(hardpoints.strutUprightMount)
        || !finite(hardpoints.lowerArmInnerFront)
        || !finite(hardpoints.lowerArmInnerRear)
        || !finite(hardpoints.lowerBallJoint)
        || !finite(hardpoints.tieRodInner)
        || !finite(hardpoints.tieRodOuter)
        || !finite(hardpoints.wheelCenter))
    {
        return false;
    }

    const float hingeLength = length(subtract(
        hardpoints.lowerArmInnerRear,
        hardpoints.lowerArmInnerFront));
    const float controlArmRadius = length(cross(
        subtract(hardpoints.lowerBallJoint, hardpoints.lowerArmInnerFront),
        normalized(
            subtract(
                hardpoints.lowerArmInnerRear,
                hardpoints.lowerArmInnerFront),
            { 0.0f, 0.0f, 1.0f })));
    const float steeringAxisLength = length(subtract(
        hardpoints.strutTopMount,
        hardpoints.lowerBallJoint));
    const float strutLength = length(subtract(
        hardpoints.strutUprightMount,
        hardpoints.strutTopMount));
    const float tieRodLength = length(subtract(
        hardpoints.tieRodOuter,
        hardpoints.tieRodInner));
    const float uprightRadius = length(subtract(
        hardpoints.wheelCenter,
        hardpoints.lowerBallJoint));
    return hingeLength > kMinimumGeometryLength
        && controlArmRadius > kMinimumGeometryLength
        && steeringAxisLength > kMinimumGeometryLength
        && strutLength > kMinimumGeometryLength
        && tieRodLength > kMinimumGeometryLength
        && uprightRadius > kMinimumGeometryLength;
}

MacPhersonKinematicsOutput evaluateMacPhersonKinematics(
    const MacPhersonHardpoints& hardpoints,
    const MacPhersonKinematicsInput& input)
{
    MacPhersonKinematicsOutput output;
    if (!validMacPhersonHardpoints(hardpoints)
        || !std::isfinite(input.compressionM)
        || !std::isfinite(input.steeringDegrees)
        || !std::isfinite(input.staticCasterDegrees)
        || std::abs(input.staticCasterDegrees) > 30.0f)
    {
        return output;
    }

    TravelPose pose = baseTravelPose(
        hardpoints, input, input.compressionM);
    if (!pose.valid)
        return output;

    const float bumpSteerRadians = solveBumpSteerRadians(hardpoints, pose);
    const heritage::math::Vec3 baseWheelForward = pose.wheelForward;
    const heritage::math::Vec3 baseWheelRight = pose.wheelRight;
    const float commandedSteerRadians = radians(input.steeringDegrees);

    if (!input.casterOverrideEnabled)
    {
        // Preserve the pre-FITMENT01 path exactly when setup caster is not
        // enabled. Existing vehicle definitions therefore retain identical
        // steering/upright behavior.
        const float totalSteerRadians =
            bumpSteerRadians + commandedSteerRadians;
        const auto steerPoint = [&](const heritage::math::Vec3& point) {
            return rotatePointAroundLine(
                point,
                pose.lowerBallJoint,
                pose.steeringAxis,
                totalSteerRadians);
        };
        output.localWheelCenter = steerPoint(pose.wheelCenter);
        pose.wheelForward = rotateAroundAxis(
            pose.wheelForward, pose.steeringAxis, totalSteerRadians);
        pose.wheelRight = rotateAroundAxis(
            pose.wheelRight, pose.steeringAxis, totalSteerRadians);
        pose.wheelUp = rotateAroundAxis(
            pose.wheelUp, pose.steeringAxis, totalSteerRadians);
        output.localSteeringAxis = pose.steeringAxis;
    }
    else
    {
        // A setup caster value is an adjustable steering-axis target, not a
        // rewrite of the authored hardpoints. Passive bump steer remains tied
        // to the physical linkage; commanded steer uses the configured caster.
        const heritage::math::Vec3 configuredSteeringAxis =
            steeringAxisWithCaster(
                pose.steeringAxis,
                input.staticCasterDegrees);
        output.localWheelCenter = rotatePointAroundLine(
            rotatePointAroundLine(
                pose.wheelCenter,
                pose.lowerBallJoint,
                pose.steeringAxis,
                bumpSteerRadians),
            pose.lowerBallJoint,
            configuredSteeringAxis,
            commandedSteerRadians);
        pose.wheelForward = rotateAroundAxis(
            pose.wheelForward, pose.steeringAxis, bumpSteerRadians);
        pose.wheelRight = rotateAroundAxis(
            pose.wheelRight, pose.steeringAxis, bumpSteerRadians);
        pose.wheelUp = rotateAroundAxis(
            pose.wheelUp, pose.steeringAxis, bumpSteerRadians);
        pose.wheelForward = rotateAroundAxis(
            pose.wheelForward, configuredSteeringAxis, commandedSteerRadians);
        pose.wheelRight = rotateAroundAxis(
            pose.wheelRight, configuredSteeringAxis, commandedSteerRadians);
        pose.wheelUp = rotateAroundAxis(
            pose.wheelUp, configuredSteeringAxis, commandedSteerRadians);
        output.localSteeringAxis = configuredSteeringAxis;
    }

    // The lower ball joint is a point on the MacPherson steering axis. Expose
    // it so downstream fitment diagnostics can intersect the *actual current*
    // steering axis with the road plane without inventing another datum.
    output.localSteeringAxisPoint = pose.lowerBallJoint;

    output.localWheelForward = normalized(
        pose.wheelForward, { 0.0f, 0.0f, 1.0f });
    output.localWheelRight = normalized(
        pose.wheelRight, { 1.0f, 0.0f, 0.0f });
    output.localWheelUp = normalized(
        cross(output.localWheelForward, output.localWheelRight),
        { 0.0f, 1.0f, 0.0f });
    output.localWheelRight = normalized(
        cross(output.localWheelUp, output.localWheelForward),
        { 1.0f, 0.0f, 0.0f });
    output.bumpSteerDegrees = degrees(bumpSteerRadians);

    // Report passive alignment independently of the commanded steering angle.
    heritage::math::Vec3 passiveForward = rotateAroundAxis(
        baseWheelForward, pose.steeringAxis, bumpSteerRadians);
    heritage::math::Vec3 passiveRight = rotateAroundAxis(
        baseWheelRight, pose.steeringAxis, bumpSteerRadians);
    output.toeDegrees = horizontalToeDegrees(passiveForward);
    output.camberDegrees = camberDegreesFromRight(passiveRight);

    output.strutCompressionM = strutCompressionAt(
        hardpoints, input, input.compressionM);
    constexpr float kDerivativeStepM = 0.0005f;
    const float plus = strutCompressionAt(
        hardpoints, input, input.compressionM + kDerivativeStepM);
    const float minus = strutCompressionAt(
        hardpoints, input, input.compressionM - kDerivativeStepM);
    output.springMotionRatio = std::clamp(
        std::abs((plus - minus) / (2.0f * kDerivativeStepM)),
        0.05f,
        5.0f);
    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        output.localWheelRight,
        output.localWheelUp,
        output.localWheelForward);
    output.travelClamped = pose.clamped;
    output.valid = true;
    return output;
}

} // namespace heritage::vehicles
