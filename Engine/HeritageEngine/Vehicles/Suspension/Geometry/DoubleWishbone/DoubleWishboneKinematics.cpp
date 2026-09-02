#include "DoubleWishboneKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;
constexpr float kMinimumGeometryLength = 0.001f;
constexpr float kDerivativeStepRadians = 0.0002f;
constexpr float kMotionRatioDerivativeStepM = 0.0005f;

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
        rotateAroundAxis(
            subtract(point, linePoint),
            unitAxis,
            angleRadians));
}

heritage::math::Vec3 alignVectorBetweenAxes(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& referenceAxis,
    const heritage::math::Vec3& currentAxis)
{
    const heritage::math::Vec3 from = normalized(
        referenceAxis,
        { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 to = normalized(
        currentAxis,
        { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 rotationAxis = cross(from, to);
    const float axisLength = length(rotationAxis);
    const float cosine = std::clamp(dot(from, to), -1.0f, 1.0f);

    if (axisLength <= kEpsilon)
    {
        if (cosine > 0.0f)
            return value;

        heritage::math::Vec3 fallbackAxis = cross(
            from,
            { 1.0f, 0.0f, 0.0f });
        if (length(fallbackAxis) <= kEpsilon)
        {
            fallbackAxis = cross(
                from,
                { 0.0f, 0.0f, 1.0f });
        }
        return rotateAroundAxis(
            value,
            normalized(fallbackAxis, { 0.0f, 0.0f, 1.0f }),
            kPi);
    }

    return rotateAroundAxis(
        value,
        scale(rotationAxis, 1.0f / axisLength),
        std::atan2(axisLength, cosine));
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

float wrapRadians(float angle)
{
    while (angle > kPi)
        angle -= 2.0f * kPi;
    while (angle < -kPi)
        angle += 2.0f * kPi;
    return angle;
}

struct SolverGeometry
{
    heritage::math::Vec3 upperHingeAxis{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 lowerHingeAxis{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 compressionDirection{ 0.0f, 1.0f, 0.0f };
    float uprightLength = 0.0f;
};

struct TravelPose
{
    bool valid = false;
    bool clamped = false;
    float upperArmAngleRadians = 0.0f;
    float lowerArmAngleRadians = 0.0f;
    heritage::math::Vec3 upperBallJoint{};
    heritage::math::Vec3 lowerBallJoint{};
    heritage::math::Vec3 steeringAxis{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 wheelCenter{};
    heritage::math::Vec3 tieRodOuter{};
    heritage::math::Vec3 damperLowerMount{};
    heritage::math::Vec3 wheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 wheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 wheelUp{ 0.0f, 1.0f, 0.0f };
};

TravelPose poseAtAngles(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input,
    const SolverGeometry& geometry,
    float upperArmAngleRadians,
    float lowerArmAngleRadians)
{
    TravelPose pose;
    pose.upperArmAngleRadians = upperArmAngleRadians;
    pose.lowerArmAngleRadians = lowerArmAngleRadians;
    pose.upperBallJoint = rotatePointAroundLine(
        hardpoints.upperBallJoint,
        hardpoints.upperArmInnerFront,
        geometry.upperHingeAxis,
        upperArmAngleRadians);
    pose.lowerBallJoint = rotatePointAroundLine(
        hardpoints.lowerBallJoint,
        hardpoints.lowerArmInnerFront,
        geometry.lowerHingeAxis,
        lowerArmAngleRadians);

    const heritage::math::Vec3 referenceSteeringAxis = normalized(
        subtract(hardpoints.upperBallJoint, hardpoints.lowerBallJoint),
        { 0.0f, 1.0f, 0.0f });
    pose.steeringAxis = normalized(
        subtract(pose.upperBallJoint, pose.lowerBallJoint),
        referenceSteeringAxis);

    const auto transformUprightPoint = [&](const heritage::math::Vec3& point) {
        return add(
            pose.lowerBallJoint,
            alignVectorBetweenAxes(
                subtract(point, hardpoints.lowerBallJoint),
                referenceSteeringAxis,
                pose.steeringAxis));
    };

    pose.wheelCenter = transformUprightPoint(hardpoints.wheelCenter);
    pose.tieRodOuter = transformUprightPoint(hardpoints.tieRodOuter);
    pose.damperLowerMount = rotatePointAroundLine(
        hardpoints.damperLowerMount,
        hardpoints.lowerArmInnerFront,
        geometry.lowerHingeAxis,
        lowerArmAngleRadians);

    const float toeRadians = radians(input.staticToeDegrees);
    const float camberRadians = radians(input.staticCamberDegrees);
    heritage::math::Vec3 forward = rotateAroundAxis(
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },
        toeRadians);
    heritage::math::Vec3 right = rotateAroundAxis(
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        toeRadians);
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    right = rotateAroundAxis(right, forward, camberRadians);
    up = rotateAroundAxis(up, forward, camberRadians);

    pose.wheelForward = alignVectorBetweenAxes(
        forward,
        referenceSteeringAxis,
        pose.steeringAxis);
    pose.wheelRight = alignVectorBetweenAxes(
        right,
        referenceSteeringAxis,
        pose.steeringAxis);
    pose.wheelUp = alignVectorBetweenAxes(
        up,
        referenceSteeringAxis,
        pose.steeringAxis);
    pose.valid = finite(pose.upperBallJoint)
        && finite(pose.lowerBallJoint)
        && finite(pose.wheelCenter)
        && finite(pose.tieRodOuter)
        && finite(pose.damperLowerMount);
    return pose;
}

void travelResiduals(
    const DoubleWishboneHardpoints& hardpoints,
    const SolverGeometry& geometry,
    const TravelPose& pose,
    float requestedCompressionM,
    float& uprightResidualM,
    float& compressionResidualM)
{
    uprightResidualM = length(subtract(
        pose.upperBallJoint,
        pose.lowerBallJoint)) - geometry.uprightLength;
    compressionResidualM = dot(
        subtract(pose.wheelCenter, hardpoints.wheelCenter),
        geometry.compressionDirection) - requestedCompressionM;
}

TravelPose solveTravelPose(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input,
    float requestedCompressionM)
{
    SolverGeometry geometry;
    geometry.upperHingeAxis = normalized(
        subtract(
            hardpoints.upperArmInnerRear,
            hardpoints.upperArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    geometry.lowerHingeAxis = normalized(
        subtract(
            hardpoints.lowerArmInnerRear,
            hardpoints.lowerArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    geometry.compressionDirection = scale(
        normalized(
            input.suspensionDirection,
            { 0.0f, -1.0f, 0.0f }),
        -1.0f);
    geometry.uprightLength = length(subtract(
        hardpoints.upperBallJoint,
        hardpoints.lowerBallJoint));

    const float lowerArmRadius = std::max(
        length(subtract(
            hardpoints.lowerBallJoint,
            hardpoints.lowerArmInnerFront)),
        0.05f);
    float lowerAngle = std::clamp(
        requestedCompressionM / lowerArmRadius,
        -0.35f,
        0.35f);
    float upperAngle = lowerAngle;

    TravelPose best = poseAtAngles(
        hardpoints,
        input,
        geometry,
        upperAngle,
        lowerAngle);
    float bestError = 1.0e9f;

    // Two generalized arm angles are solved against two physical constraints:
    // rigid upright length and requested wheel-centre travel along the authored
    // suspension axis. Finite-difference Newton keeps this mechanism generic for
    // unequal arm lengths and non-parallel inner pivot axes.
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        const TravelPose current = poseAtAngles(
            hardpoints,
            input,
            geometry,
            upperAngle,
            lowerAngle);
        if (!current.valid)
            break;

        float uprightResidual = 0.0f;
        float compressionResidual = 0.0f;
        travelResiduals(
            hardpoints,
            geometry,
            current,
            requestedCompressionM,
            uprightResidual,
            compressionResidual);
        const float error = std::abs(uprightResidual)
            + std::abs(compressionResidual);
        if (error < bestError)
        {
            best = current;
            bestError = error;
        }
        if (std::abs(uprightResidual) < 0.000005f
            && std::abs(compressionResidual) < 0.000005f)
        {
            TravelPose converged = current;
            converged.clamped = false;
            return converged;
        }

        const TravelPose upperProbe = poseAtAngles(
            hardpoints,
            input,
            geometry,
            upperAngle + kDerivativeStepRadians,
            lowerAngle);
        const TravelPose lowerProbe = poseAtAngles(
            hardpoints,
            input,
            geometry,
            upperAngle,
            lowerAngle + kDerivativeStepRadians);
        if (!upperProbe.valid || !lowerProbe.valid)
            break;

        float upperUprightResidual = 0.0f;
        float upperCompressionResidual = 0.0f;
        float lowerUprightResidual = 0.0f;
        float lowerCompressionResidual = 0.0f;
        travelResiduals(
            hardpoints,
            geometry,
            upperProbe,
            requestedCompressionM,
            upperUprightResidual,
            upperCompressionResidual);
        travelResiduals(
            hardpoints,
            geometry,
            lowerProbe,
            requestedCompressionM,
            lowerUprightResidual,
            lowerCompressionResidual);

        const float a = (upperUprightResidual - uprightResidual)
            / kDerivativeStepRadians;
        const float b = (lowerUprightResidual - uprightResidual)
            / kDerivativeStepRadians;
        const float c = (upperCompressionResidual - compressionResidual)
            / kDerivativeStepRadians;
        const float d = (lowerCompressionResidual - compressionResidual)
            / kDerivativeStepRadians;
        const float determinant = a * d - b * c;
        if (std::abs(determinant) < 1.0e-8f)
            break;

        float upperDelta = (-uprightResidual * d
            + b * compressionResidual) / determinant;
        float lowerDelta = (c * uprightResidual
            - a * compressionResidual) / determinant;
        upperDelta = std::clamp(upperDelta, -0.12f, 0.12f);
        lowerDelta = std::clamp(lowerDelta, -0.12f, 0.12f);
        upperAngle = std::clamp(
            upperAngle + upperDelta,
            -1.20f,
            1.20f);
        lowerAngle = std::clamp(
            lowerAngle + lowerDelta,
            -1.20f,
            1.20f);
    }

    // A near-boundary solve may miss Newton's tight convergence tolerance while
    // still lying within a few millimetres of the closest physically reachable
    // pose. Report it as clamped rather than injecting a discontinuity.
    best.clamped = true;
    best.valid = best.valid && bestError < 0.0025f;
    return best;
}

float solveBumpSteerRadians(
    const DoubleWishboneHardpoints& hardpoints,
    const TravelPose& pose)
{
    const float referenceTieRodLength = length(subtract(
        hardpoints.tieRodOuter,
        hardpoints.tieRodInner));
    if (referenceTieRodLength <= kMinimumGeometryLength)
        return 0.0f;

    const heritage::math::Vec3 q = subtract(
        pose.tieRodOuter,
        pose.lowerBallJoint);
    const heritage::math::Vec3 c = subtract(
        hardpoints.tieRodInner,
        pose.lowerBallJoint);
    const heritage::math::Vec3 qParallel = scale(
        pose.steeringAxis,
        dot(q, pose.steeringAxis));
    const heritage::math::Vec3 qPerpendicular = subtract(q, qParallel);
    const float constantTerm = dot(c, qParallel);
    const float a = dot(c, qPerpendicular);
    const float b = dot(
        c,
        cross(pose.steeringAxis, qPerpendicular));
    const float target = 0.5f * (
        lengthSquared(q) + lengthSquared(c)
        - referenceTieRodLength * referenceTieRodLength)
        - constantTerm;
    const float magnitude = std::sqrt(a * a + b * b);
    if (magnitude <= kEpsilon)
        return 0.0f;

    const float normalizedTarget = std::clamp(
        target / magnitude,
        -1.0f,
        1.0f);
    const float phase = std::atan2(b, a);
    const float offset = std::acos(normalizedTarget);
    const float candidateA = wrapRadians(phase + offset);
    const float candidateB = wrapRadians(phase - offset);
    const float chosen = std::abs(candidateA) <= std::abs(candidateB)
        ? candidateA
        : candidateB;
    return std::clamp(
        chosen,
        radians(-30.0f),
        radians(30.0f));
}

float damperCompressionAt(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input,
    float compressionM)
{
    const TravelPose pose = solveTravelPose(
        hardpoints,
        input,
        compressionM);
    if (!pose.valid)
        return 0.0f;

    const float referenceLength = length(subtract(
        hardpoints.damperLowerMount,
        hardpoints.damperUpperMount));
    const float currentLength = length(subtract(
        pose.damperLowerMount,
        hardpoints.damperUpperMount));
    return referenceLength - currentLength;
}

} // namespace

bool validDoubleWishboneLinkageHardpoints(
    const DoubleWishboneHardpoints& hardpoints)
{
    if (!hardpoints.authored
        || !finite(hardpoints.upperArmInnerFront)
        || !finite(hardpoints.upperArmInnerRear)
        || !finite(hardpoints.upperBallJoint)
        || !finite(hardpoints.lowerArmInnerFront)
        || !finite(hardpoints.lowerArmInnerRear)
        || !finite(hardpoints.lowerBallJoint)
        || !finite(hardpoints.tieRodInner)
        || !finite(hardpoints.tieRodOuter)
        || !finite(hardpoints.wheelCenter))
    {
        return false;
    }

    const heritage::math::Vec3 upperHingeAxis = normalized(
        subtract(
            hardpoints.upperArmInnerRear,
            hardpoints.upperArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    const heritage::math::Vec3 lowerHingeAxis = normalized(
        subtract(
            hardpoints.lowerArmInnerRear,
            hardpoints.lowerArmInnerFront),
        { 0.0f, 0.0f, 1.0f });
    const float upperArmRadius = length(cross(
        subtract(
            hardpoints.upperBallJoint,
            hardpoints.upperArmInnerFront),
        upperHingeAxis));
    const float lowerArmRadius = length(cross(
        subtract(
            hardpoints.lowerBallJoint,
            hardpoints.lowerArmInnerFront),
        lowerHingeAxis));

    return length(subtract(
            hardpoints.upperArmInnerRear,
            hardpoints.upperArmInnerFront)) > kMinimumGeometryLength
        && length(subtract(
            hardpoints.lowerArmInnerRear,
            hardpoints.lowerArmInnerFront)) > kMinimumGeometryLength
        && upperArmRadius > 0.02f
        && lowerArmRadius > 0.02f
        && length(subtract(
            hardpoints.upperBallJoint,
            hardpoints.lowerBallJoint)) > 0.05f
        && length(subtract(
            hardpoints.tieRodOuter,
            hardpoints.tieRodInner)) > 0.02f
        && length(subtract(
            hardpoints.wheelCenter,
            hardpoints.lowerBallJoint)) > 0.01f;
}

bool validDoubleWishboneHardpoints(
    const DoubleWishboneHardpoints& hardpoints)
{
    return validDoubleWishboneLinkageHardpoints(hardpoints)
        && finite(hardpoints.damperUpperMount)
        && finite(hardpoints.damperLowerMount)
        && length(subtract(
            hardpoints.damperLowerMount,
            hardpoints.damperUpperMount)) > 0.02f;
}

DoubleWishboneKinematicsOutput evaluateDoubleWishboneLinkageKinematics(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input)
{
    DoubleWishboneKinematicsOutput output;
    if (!validDoubleWishboneLinkageHardpoints(hardpoints)
        || !std::isfinite(input.compressionM)
        || !std::isfinite(input.steeringDegrees)
        || !std::isfinite(input.staticCamberDegrees)
        || !std::isfinite(input.staticToeDegrees))
    {
        return output;
    }

    TravelPose pose = solveTravelPose(
        hardpoints,
        input,
        input.compressionM);
    if (!pose.valid)
        return output;

    const float bumpSteerRadians = solveBumpSteerRadians(
        hardpoints,
        pose);
    const float totalSteerRadians = bumpSteerRadians
        + radians(input.steeringDegrees);
    const heritage::math::Vec3 baseWheelForward = pose.wheelForward;
    const heritage::math::Vec3 baseWheelRight = pose.wheelRight;

    output.localWheelCenter = rotatePointAroundLine(
        pose.wheelCenter,
        pose.lowerBallJoint,
        pose.steeringAxis,
        totalSteerRadians);
    pose.wheelForward = rotateAroundAxis(
        pose.wheelForward,
        pose.steeringAxis,
        totalSteerRadians);
    pose.wheelRight = rotateAroundAxis(
        pose.wheelRight,
        pose.steeringAxis,
        totalSteerRadians);
    pose.wheelUp = rotateAroundAxis(
        pose.wheelUp,
        pose.steeringAxis,
        totalSteerRadians);

    output.localWheelForward = normalized(
        pose.wheelForward,
        { 0.0f, 0.0f, 1.0f });
    output.localWheelRight = normalized(
        pose.wheelRight,
        { 1.0f, 0.0f, 0.0f });
    output.localWheelUp = normalized(
        cross(output.localWheelForward, output.localWheelRight),
        { 0.0f, 1.0f, 0.0f });
    output.localWheelRight = normalized(
        cross(output.localWheelUp, output.localWheelForward),
        { 1.0f, 0.0f, 0.0f });

    const heritage::math::Vec3 passiveForward = rotateAroundAxis(
        baseWheelForward,
        pose.steeringAxis,
        bumpSteerRadians);
    const heritage::math::Vec3 passiveRight = rotateAroundAxis(
        baseWheelRight,
        pose.steeringAxis,
        bumpSteerRadians);
    output.bumpSteerDegrees = degrees(bumpSteerRadians);
    output.toeDegrees = horizontalToeDegrees(passiveForward);
    output.camberDegrees = camberDegreesFromRight(passiveRight);

    output.localSteeringAxis = pose.steeringAxis;
    output.localSteeringAxisPoint = pose.lowerBallJoint;
    output.casterDegrees = degrees(std::atan2(
        -pose.steeringAxis.z,
        std::max(std::abs(pose.steeringAxis.y), kEpsilon)));
    output.kingpinInclinationDegrees = degrees(std::atan2(
        pose.steeringAxis.x,
        std::max(std::abs(pose.steeringAxis.y), kEpsilon)));
    output.upperArmRotationRadians = pose.upperArmAngleRadians;
    output.lowerArmRotationRadians = pose.lowerArmAngleRadians;

    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        output.localWheelRight,
        output.localWheelUp,
        output.localWheelForward);
    output.travelClamped = pose.clamped;
    output.valid = true;
    return output;
}

DoubleWishboneKinematicsOutput evaluateDoubleWishboneKinematics(
    const DoubleWishboneHardpoints& hardpoints,
    const DoubleWishboneKinematicsInput& input)
{
    if (!validDoubleWishboneHardpoints(hardpoints))
        return {};

    DoubleWishboneKinematicsOutput output =
        evaluateDoubleWishboneLinkageKinematics(hardpoints, input);
    if (!output.valid)
        return output;

    output.damperCompressionM = damperCompressionAt(
        hardpoints,
        input,
        input.compressionM);
    const float plus = damperCompressionAt(
        hardpoints,
        input,
        input.compressionM + kMotionRatioDerivativeStepM);
    const float minus = damperCompressionAt(
        hardpoints,
        input,
        input.compressionM - kMotionRatioDerivativeStepM);
    output.damperMotionRatio = std::clamp(
        std::abs((plus - minus)
            / (2.0f * kMotionRatioDerivativeStepM)),
        0.05f,
        5.0f);
    output.springMotionRatio = output.damperMotionRatio;
    return output;
}

} // namespace heritage::vehicles
