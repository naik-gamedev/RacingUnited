#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "../../Core/Math/Math.hpp"

namespace heritage::physics::collision_detail {

// CLEAN04B internal collision implementation vocabulary.
// These low-level numerical helpers/constants are shared by the collision
// responsibility translation units. Public policy stays in CollisionSystem.

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumRadius = 0.0001f;
constexpr float kMaximumShapeExtent = 1000000.0f;
constexpr float kMaximumMaterialValue = 10.0f;
constexpr float kContactEpsilon = 0.000001f;
constexpr float kSatEpsilon = 0.00001f;
constexpr float kPositionSlop = 0.0005f;
constexpr float kPositionCorrectionPercent = 0.78f;
constexpr float kRestitutionVelocityThreshold = 1.0f;
constexpr float kWarmStartPointDistance = 0.35f;
constexpr float kWarmStartNormalDot = 0.80f;
constexpr float kSleepFreeLinearSpeed = 0.15f;
constexpr float kSleepContactTangentialSpeed = 0.05f;
constexpr float kSleepContactNormalSpeed = 0.15f;
constexpr float kSleepAngularSpeedDegrees = 8.0f;
constexpr float kSleepDelaySeconds = 1.0f;
constexpr float kKinematicWakeSpeed = 0.01f;
constexpr float kPenetrationWakeThreshold = 0.02f;
constexpr float kContinuousCollisionLocalOffsetEpsilon = 0.0001f;
constexpr float kContinuousCollisionTravelFraction = 0.25f;
constexpr float kContinuousCollisionMinimumTravel = 0.01f;
constexpr float kContinuousCollisionSkin = 0.001f;
// Static mesh contact offset is deliberately only 2 mm: large enough to keep
// a 120 Hz body from numerically alternating across a zero-thickness triangle,
// but far below visible suspension or terrain scales.
constexpr float kStaticTriangleContactSkin = 0.002f;
constexpr float kStaticTriangleManifoldPointSeparation = 0.04f;
constexpr std::size_t kMaximumStaticContactsPerCollider = 4;
constexpr float kStaticTriangleCollisionFriction = 0.90f;

inline bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

inline bool finiteVec3(const heritage::math::Vec3& value)
{
    return finiteFloat(value.x)
        && finiteFloat(value.y)
        && finiteFloat(value.z);
}

inline heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

inline heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

inline heritage::math::Vec3 cross(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

inline float component(const heritage::math::Vec3& value, int index)
{
    switch (index)
    {
    case 0: return value.x;
    case 1: return value.y;
    default: return value.z;
    }
}

inline heritage::math::Vec3 scaleVector(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

inline float dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

inline float length(const heritage::math::Vec3& value)
{
    return std::sqrt((std::max)(0.0f, lengthSquared(value)));
}

inline heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback = { 0.0f, 1.0f, 0.0f })
{
    const float magnitude = length(value);
    if (magnitude <= kContactEpsilon)
        return fallback;
    return scaleVector(value, 1.0f / magnitude);
}

inline heritage::math::Vec3 clampVector(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& minimum,
    const heritage::math::Vec3& maximum)
{
    return {
        std::clamp(value.x, minimum.x, maximum.x),
        std::clamp(value.y, minimum.y, maximum.y),
        std::clamp(value.z, minimum.z, maximum.z)
    };
}

inline heritage::math::Vec3 closestPointOnTriangle(
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    const heritage::math::Vec3& c)
{
    const heritage::math::Vec3 ab = subtract(b, a);
    const heritage::math::Vec3 ac = subtract(c, a);
    const heritage::math::Vec3 ap = subtract(point, a);
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f)
        return a;

    const heritage::math::Vec3 bp = subtract(point, b);
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3)
        return b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        const float v = d1 / (d1 - d3);
        return add(a, scaleVector(ab, v));
    }

    const heritage::math::Vec3 cp = subtract(point, c);
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6)
        return c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        const float w = d2 / (d2 - d6);
        return add(a, scaleVector(ac, w));
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        const heritage::math::Vec3 bc = subtract(c, b);
        const float w = (d4 - d3)
            / ((d4 - d3) + (d5 - d6));
        return add(b, scaleVector(bc, w));
    }

    const float denominator = 1.0f / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    return add(a, add(scaleVector(ab, v), scaleVector(ac, w)));
}

} // namespace heritage::physics::collision_detail
