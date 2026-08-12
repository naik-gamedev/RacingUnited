#pragma once

#include <cmath>

namespace heritage::math
{

struct Vec3
{
    float x;
    float y;
    float z;
};

// Double-precision absolute/world coordinate. Rendering and most local-space
// geometry intentionally remain Vec3/FP32; DVec3 is reserved for world-scale
// anchors where kilometres of travel must not quantize millimetre motion.
struct DVec3
{
    double x;
    double y;
    double z;
};

inline DVec3 toDouble(const Vec3& value)
{
    return {
        static_cast<double>(value.x),
        static_cast<double>(value.y),
        static_cast<double>(value.z)
    };
}

inline Vec3 toFloat(const DVec3& value)
{
    return {
        static_cast<float>(value.x),
        static_cast<float>(value.y),
        static_cast<float>(value.z)
    };
}

struct Mat4
{
    float m[16] = {};
};

inline Mat4 identity()
{
    Mat4 result;
    result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
    return result;
}

inline Mat4 perspective(float fovY, float aspect, float zNear, float zFar)
{
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 result;
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (zFar + zNear) / (zNear - zFar);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return result;
}

// Reversed-Z perspective for very large view distances. Near maps to depth 1
// and far maps to depth 0, which pairs with GL_GREATER + a floating-point
// depth buffer. This keeps cockpit-scale near clipping while preserving much
// better depth precision across tens of kilometres.
inline Mat4 perspectiveReversedZ(float fovY, float aspect, float zNear, float zFar)
{
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 result;
    result.m[0] = f / aspect;
    result.m[5] = f;
    result.m[10] = (zFar + zNear) / (zFar - zNear);
    result.m[11] = -1.0f;
    result.m[14] = (2.0f * zFar * zNear) / (zFar - zNear);
    return result;
}

} // namespace heritage::math
