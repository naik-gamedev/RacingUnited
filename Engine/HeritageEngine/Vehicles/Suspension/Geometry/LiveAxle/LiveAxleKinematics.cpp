#include "LiveAxleKinematics.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.000001f;
constexpr float kMinimumLength = 0.005f;
constexpr float kMotionRatioStepM = 0.0005f;
constexpr float kMaximumAxleRollRadians = 0.60f;

float radians(float degreesValue)
{
    return degreesValue * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

heritage::math::Vec3 add(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 scale(const heritage::math::Vec3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float lengthSquared(const heritage::math::Vec3& v)
{
    return dot(v, v);
}

float length(const heritage::math::Vec3& v)
{
    return std::sqrt(lengthSquared(v));
}

bool finite(const heritage::math::Vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& v,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(v);
    return magnitude > kEpsilon ? scale(v, 1.0f / magnitude) : fallback;
}

heritage::math::Vec3 rotateAroundAxis(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& unitAxis,
    float angleRadians)
{
    const float c = std::cos(angleRadians);
    const float s = std::sin(angleRadians);
    return add(
        add(scale(value, c), scale(cross(unitAxis, value), s)),
        scale(unitAxis, dot(unitAxis, value) * (1.0f - c)));
}

heritage::math::Vec3 eulerDegreesFromBasis(
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward)
{
    const float y = std::asin(std::clamp(-right.z, -1.0f, 1.0f));
    const float cy = std::cos(y);
    float x = 0.0f;
    float z = 0.0f;
    if (std::abs(cy) > kEpsilon)
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

float chooseClosestRoot(float a, float b, float reference)
{
    return std::abs(a - reference) <= std::abs(b - reference) ? a : b;
}

struct AxlePose
{
    bool valid = false;
    bool clamped = false;
    float rollRadians = 0.0f;
    heritage::math::Vec3 center{};
    heritage::math::Vec3 leftWheel{};
    heritage::math::Vec3 rightWheel{};
    heritage::math::Vec3 leftSpringAxle{};
    heritage::math::Vec3 rightSpringAxle{};
    heritage::math::Vec3 leftDamperAxle{};
    heritage::math::Vec3 rightDamperAxle{};
};

heritage::math::Vec3 rolledAxlePoint(
    const LiveAxleHardpoints& h,
    const heritage::math::Vec3& referencePoint,
    const heritage::math::Vec3& center,
    float rollRadians)
{
    return add(
        center,
        rotateAroundAxis(
            subtract(referencePoint, h.axleCenter),
            { 0.0f, 0.0f, 1.0f },
            rollRadians));
}

bool solvePanhardCenterX(
    const LiveAxleHardpoints& h,
    heritage::math::Vec3& center,
    float rollRadians,
    bool& clamped)
{
    const float restLength = length(subtract(
        h.panhardAxleMount, h.panhardChassisMount));
    if (restLength < kMinimumLength)
        return false;

    const heritage::math::Vec3 offset = rotateAroundAxis(
        subtract(h.panhardAxleMount, h.axleCenter),
        { 0.0f, 0.0f, 1.0f },
        rollRadians);
    const float dy = center.y + offset.y - h.panhardChassisMount.y;
    const float dz = center.z + offset.z - h.panhardChassisMount.z;
    float remaining = restLength * restLength - dy * dy - dz * dz;
    if (remaining < 0.0f)
    {
        if (remaining < -0.0004f)
            return false;
        remaining = 0.0f;
        clamped = true;
    }
    const float root = std::sqrt(remaining);
    const float candidateA = h.panhardChassisMount.x - offset.x + root;
    const float candidateB = h.panhardChassisMount.x - offset.x - root;
    center.x = chooseClosestRoot(candidateA, candidateB, h.axleCenter.x);
    return std::isfinite(center.x);
}

bool trailingCenterZCandidate(
    const heritage::math::Vec3& chassisMount,
    const heritage::math::Vec3& axleMountReference,
    const LiveAxleHardpoints& h,
    const heritage::math::Vec3& center,
    float rollRadians,
    float& value,
    bool& clamped)
{
    const float restLength = length(subtract(axleMountReference, chassisMount));
    if (restLength < kMinimumLength)
        return false;
    const heritage::math::Vec3 offset = rotateAroundAxis(
        subtract(axleMountReference, h.axleCenter),
        { 0.0f, 0.0f, 1.0f },
        rollRadians);
    const float dx = center.x + offset.x - chassisMount.x;
    const float dy = center.y + offset.y - chassisMount.y;
    float remaining = restLength * restLength - dx * dx - dy * dy;
    if (remaining < 0.0f)
    {
        if (remaining < -0.0004f)
            return false;
        remaining = 0.0f;
        clamped = true;
    }
    const float root = std::sqrt(remaining);
    const float candidateA = chassisMount.z - offset.z + root;
    const float candidateB = chassisMount.z - offset.z - root;
    value = chooseClosestRoot(candidateA, candidateB, h.axleCenter.z);
    return std::isfinite(value);
}

AxlePose solvePose(
    const LiveAxleHardpoints& h,
    float leftCompressionM,
    float rightCompressionM)
{
    AxlePose pose;
    const heritage::math::Vec3 trackVector = subtract(
        h.rightWheelCenter, h.leftWheelCenter);
    const float lateralTrack = std::max(
        std::sqrt(trackVector.x * trackVector.x + trackVector.y * trackVector.y),
        0.10f);
    const float requestedRoll = std::atan2(
        rightCompressionM - leftCompressionM,
        lateralTrack);
    pose.rollRadians = std::clamp(
        requestedRoll, -kMaximumAxleRollRadians, kMaximumAxleRollRadians);
    pose.clamped = std::abs(pose.rollRadians - requestedRoll) > 0.000001f;
    pose.center = h.axleCenter;
    pose.center.y += 0.5f * (leftCompressionM + rightCompressionM);

    if (!solvePanhardCenterX(h, pose.center, pose.rollRadians, pose.clamped))
        return pose;

    float leftZ = h.axleCenter.z;
    float rightZ = h.axleCenter.z;
    if (!trailingCenterZCandidate(
            h.leftTrailingChassisMount,
            h.leftTrailingAxleMount,
            h,
            pose.center,
            pose.rollRadians,
            leftZ,
            pose.clamped)
        || !trailingCenterZCandidate(
            h.rightTrailingChassisMount,
            h.rightTrailingAxleMount,
            h,
            pose.center,
            pose.rollRadians,
            rightZ,
            pose.clamped))
    {
        return pose;
    }
    pose.center.z = 0.5f * (leftZ + rightZ);

    pose.leftWheel = rolledAxlePoint(
        h, h.leftWheelCenter, pose.center, pose.rollRadians);
    pose.rightWheel = rolledAxlePoint(
        h, h.rightWheelCenter, pose.center, pose.rollRadians);
    pose.leftSpringAxle = rolledAxlePoint(
        h, h.leftSpringAxleMount, pose.center, pose.rollRadians);
    pose.rightSpringAxle = rolledAxlePoint(
        h, h.rightSpringAxleMount, pose.center, pose.rollRadians);
    pose.leftDamperAxle = rolledAxlePoint(
        h, h.leftDamperAxleMount, pose.center, pose.rollRadians);
    pose.rightDamperAxle = rolledAxlePoint(
        h, h.rightDamperAxleMount, pose.center, pose.rollRadians);

    pose.valid = finite(pose.center)
        && finite(pose.leftWheel)
        && finite(pose.rightWheel)
        && finite(pose.leftSpringAxle)
        && finite(pose.rightSpringAxle)
        && finite(pose.leftDamperAxle)
        && finite(pose.rightDamperAxle)
        && std::abs(length(subtract(pose.rightWheel, pose.leftWheel))
            - length(subtract(h.rightWheelCenter, h.leftWheelCenter))) < 0.0005f;
    return pose;
}

float shaftCompression(
    const heritage::math::Vec3& chassisMount,
    const heritage::math::Vec3& referenceAxleMount,
    const heritage::math::Vec3& currentAxleMount)
{
    return length(subtract(chassisMount, referenceAxleMount))
        - length(subtract(chassisMount, currentAxleMount));
}

} // namespace

bool validLiveAxleHardpoints(const LiveAxleHardpoints& h)
{
    if (!h.authored)
        return false;
    const heritage::math::Vec3 points[] = {
        h.axleCenter, h.leftWheelCenter, h.rightWheelCenter,
        h.panhardChassisMount, h.panhardAxleMount,
        h.leftTrailingChassisMount, h.leftTrailingAxleMount,
        h.rightTrailingChassisMount, h.rightTrailingAxleMount,
        h.leftSpringChassisMount, h.leftSpringAxleMount,
        h.rightSpringChassisMount, h.rightSpringAxleMount,
        h.leftDamperChassisMount, h.leftDamperAxleMount,
        h.rightDamperChassisMount, h.rightDamperAxleMount
    };
    for (const auto& point : points)
    {
        if (!finite(point))
            return false;
    }
    const float track = length(subtract(h.rightWheelCenter, h.leftWheelCenter));
    return track > 0.40f
        && length(subtract(h.panhardAxleMount, h.panhardChassisMount)) > 0.10f
        && length(subtract(h.leftTrailingAxleMount, h.leftTrailingChassisMount)) > 0.10f
        && length(subtract(h.rightTrailingAxleMount, h.rightTrailingChassisMount)) > 0.10f
        && length(subtract(h.leftSpringAxleMount, h.leftSpringChassisMount)) > 0.02f
        && length(subtract(h.rightSpringAxleMount, h.rightSpringChassisMount)) > 0.02f
        && length(subtract(h.leftDamperAxleMount, h.leftDamperChassisMount)) > 0.02f
        && length(subtract(h.rightDamperAxleMount, h.rightDamperChassisMount)) > 0.02f;
}

LiveAxleKinematicsOutput evaluateLiveAxleKinematics(
    const LiveAxleHardpoints& h,
    const LiveAxleKinematicsInput& input)
{
    LiveAxleKinematicsOutput output;
    if (!validLiveAxleHardpoints(h))
        return output;

    const AxlePose pose = solvePose(
        h, input.leftCompressionM, input.rightCompressionM);
    if (!pose.valid)
        return output;

    const bool left = input.evaluateLeftWheel;
    const heritage::math::Vec3 currentWheel = left
        ? pose.leftWheel : pose.rightWheel;
    const heritage::math::Vec3 referenceWheel = left
        ? h.leftWheelCenter : h.rightWheelCenter;
    const heritage::math::Vec3 springChassis = left
        ? h.leftSpringChassisMount : h.rightSpringChassisMount;
    const heritage::math::Vec3 springReferenceAxle = left
        ? h.leftSpringAxleMount : h.rightSpringAxleMount;
    const heritage::math::Vec3 springCurrentAxle = left
        ? pose.leftSpringAxle : pose.rightSpringAxle;
    const heritage::math::Vec3 damperChassis = left
        ? h.leftDamperChassisMount : h.rightDamperChassisMount;
    const heritage::math::Vec3 damperReferenceAxle = left
        ? h.leftDamperAxleMount : h.rightDamperAxleMount;
    const heritage::math::Vec3 damperCurrentAxle = left
        ? pose.leftDamperAxle : pose.rightDamperAxle;

    output.springCompressionM = shaftCompression(
        springChassis, springReferenceAxle, springCurrentAxle);
    output.damperCompressionM = shaftCompression(
        damperChassis, damperReferenceAxle, damperCurrentAxle);

    LiveAxleKinematicsInput derivativeInput = input;
    if (left)
        derivativeInput.leftCompressionM += kMotionRatioStepM;
    else
        derivativeInput.rightCompressionM += kMotionRatioStepM;
    const AxlePose derivativePose = solvePose(
        h, derivativeInput.leftCompressionM, derivativeInput.rightCompressionM);
    if (!derivativePose.valid)
        return output;
    const heritage::math::Vec3 derivativeSpringAxle = left
        ? derivativePose.leftSpringAxle : derivativePose.rightSpringAxle;
    const heritage::math::Vec3 derivativeDamperAxle = left
        ? derivativePose.leftDamperAxle : derivativePose.rightDamperAxle;
    output.springMotionRatio = (
        shaftCompression(springChassis, springReferenceAxle, derivativeSpringAxle)
        - output.springCompressionM) / kMotionRatioStepM;
    output.damperMotionRatio = (
        shaftCompression(damperChassis, damperReferenceAxle, derivativeDamperAxle)
        - output.damperCompressionM) / kMotionRatioStepM;

    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 right{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    forward = rotateAroundAxis(forward, { 0.0f, 0.0f, 1.0f }, pose.rollRadians);
    right = rotateAroundAxis(right, { 0.0f, 0.0f, 1.0f }, pose.rollRadians);
    up = rotateAroundAxis(up, { 0.0f, 0.0f, 1.0f }, pose.rollRadians);

    const float toeRadians = radians(input.staticToeDegrees + input.steeringDegrees);
    forward = rotateAroundAxis(forward, normalized(up, { 0.0f, 1.0f, 0.0f }), toeRadians);
    right = rotateAroundAxis(right, normalized(up, { 0.0f, 1.0f, 0.0f }), toeRadians);
    const float camberRadians = radians(input.staticCamberDegrees);
    right = rotateAroundAxis(right, normalized(forward, { 0.0f, 0.0f, 1.0f }), camberRadians);
    up = rotateAroundAxis(up, normalized(forward, { 0.0f, 0.0f, 1.0f }), camberRadians);

    output.valid = true;
    output.travelClamped = pose.clamped;
    output.axleRollRadians = pose.rollRadians;
    output.lateralShiftM = pose.center.x - h.axleCenter.x;
    output.longitudinalShiftM = pose.center.z - h.axleCenter.z;
    output.localAxleCenter = pose.center;
    output.localWheelCenter = currentWheel;
    output.localWheelForward = normalized(forward, { 0.0f, 0.0f, 1.0f });
    output.localWheelRight = normalized(right, { 1.0f, 0.0f, 0.0f });
    output.localWheelUp = normalized(up, { 0.0f, 1.0f, 0.0f });
    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        output.localWheelRight, output.localWheelUp, output.localWheelForward);
    output.camberDegrees = degrees(std::atan2(
        output.localWheelRight.y, output.localWheelRight.x));
    output.toeDegrees = degrees(std::atan2(
        output.localWheelForward.x, output.localWheelForward.z));

    if (!finite(output.localWheelCenter)
        || !std::isfinite(output.springMotionRatio)
        || !std::isfinite(output.damperMotionRatio))
    {
        return {};
    }
    return output;
}

} // namespace heritage::vehicles
