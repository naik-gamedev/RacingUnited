#pragma once

// Heritage Engine shared quaternion primitives.
//
// CLEAN04A centralizes the rotation conventions previously duplicated by
// entities, rigid bodies, collision queries and vehicle simulation.  Keep this
// header representation-neutral: it owns quaternion algebra and conversion,
// while higher-level systems continue to own transform policy, interpolation
// timing and coordinate-space semantics.

#include "Math.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::math
{

inline constexpr float kQuaternionEpsilon = 0.000001f;
inline constexpr float kPiFloat = 3.14159265358979323846f;

struct Quaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float radiansFromDegrees(float degrees)
{
    return degrees * (kPiFloat / 180.0f);
}

inline float degreesFromRadians(float radians)
{
    return radians * (180.0f / kPiFloat);
}

// Intrinsic X-Y-Z rotation represented as qZ * qY * qX.  This intentionally
// returns the direct trigonometric result; callers that historically normalize
// the result should continue to do so explicitly so CLEAN04A remains behavior-
// preserving for every subsystem.
inline Quaternion makeQuaternionFromEulerDegrees(const Vec3& value)
{
    const float halfX = radiansFromDegrees(value.x) * 0.5f;
    const float halfY = radiansFromDegrees(value.y) * 0.5f;
    const float halfZ = radiansFromDegrees(value.z) * 0.5f;

    const float cx = std::cos(halfX);
    const float sx = std::sin(halfX);
    const float cy = std::cos(halfY);
    const float sy = std::sin(halfY);
    const float cz = std::cos(halfZ);
    const float sz = std::sin(halfZ);

    return {
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    };
}

inline Quaternion multiply(
    const Quaternion& left,
    const Quaternion& right)
{
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w
    };
}

inline float quaternionLengthSquared(const Quaternion& value)
{
    return value.w * value.w
        + value.x * value.x
        + value.y * value.y
        + value.z * value.z;
}

// Threshold is intentionally expressed as squared length because both entity
// hierarchy and rigid-body code historically used that exact comparison.
inline Quaternion normalized(
    const Quaternion& value,
    float minimumLengthSquared = kQuaternionEpsilon)
{
    const float lengthSquared = quaternionLengthSquared(value);
    if (lengthSquared <= minimumLengthSquared)
        return {};

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value.w * inverseLength,
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength
    };
}

inline Quaternion conjugate(const Quaternion& value)
{
    return { value.w, -value.x, -value.y, -value.z };
}

inline Quaternion inverse(
    const Quaternion& value,
    float minimumLengthSquared = kQuaternionEpsilon)
{
    const float lengthSquared = quaternionLengthSquared(value);
    if (lengthSquared <= minimumLengthSquared)
        return {};

    return {
        value.w / lengthSquared,
        -value.x / lengthSquared,
        -value.y / lengthSquared,
        -value.z / lengthSquared
    };
}

// Conversion for a quaternion already treated as unit length by its owner.
// Entity hierarchy historically follows this path without an extra normalize.
inline Vec3 eulerDegreesFromUnitQuaternion(const Quaternion& value)
{
    const float sinXCosY = 2.0f * (value.w * value.x + value.y * value.z);
    const float cosXCosY = 1.0f - 2.0f * (value.x * value.x + value.y * value.y);
    const float angleX = std::atan2(sinXCosY, cosXCosY);

    const float sinY = std::clamp(
        2.0f * (value.w * value.y - value.z * value.x),
        -1.0f,
        1.0f);
    const float angleY = std::asin(sinY);

    const float sinZCosY = 2.0f * (value.w * value.z + value.x * value.y);
    const float cosZCosY = 1.0f - 2.0f * (value.y * value.y + value.z * value.z);
    const float angleZ = std::atan2(sinZCosY, cosZCosY);

    return {
        degreesFromRadians(angleX),
        degreesFromRadians(angleY),
        degreesFromRadians(angleZ)
    };
}

// Optimized q*v*q^-1 for a unit quaternion.  Rigid-body, collision and vehicle
// code historically use this exact cross-product form.
inline Vec3 rotateVectorUnit(
    const Quaternion& rotation,
    const Vec3& value)
{
    const Vec3 q{ rotation.x, rotation.y, rotation.z };
    const Vec3 twiceCross{
        2.0f * (q.y * value.z - q.z * value.y),
        2.0f * (q.z * value.x - q.x * value.z),
        2.0f * (q.x * value.y - q.y * value.x)
    };
    const Vec3 qCrossTwice{
        q.y * twiceCross.z - q.z * twiceCross.y,
        q.z * twiceCross.x - q.x * twiceCross.z,
        q.x * twiceCross.y - q.y * twiceCross.x
    };
    return {
        value.x + (twiceCross.x * rotation.w + qCrossTwice.x),
        value.y + (twiceCross.y * rotation.w + qCrossTwice.y),
        value.z + (twiceCross.z * rotation.w + qCrossTwice.z)
    };
}

// General q*v*q^-1 rotation retained for callers whose historical behavior did
// not assume exact unit length (notably EntityRegistry hierarchy operations).
inline Vec3 rotateVectorGeneral(
    const Quaternion& rotation,
    const Vec3& value)
{
    const Quaternion vectorQuaternion{ 0.0f, value.x, value.y, value.z };
    const Quaternion rotated = multiply(
        multiply(rotation, vectorQuaternion),
        inverse(rotation));
    return { rotated.x, rotated.y, rotated.z };
}

} // namespace heritage::math
