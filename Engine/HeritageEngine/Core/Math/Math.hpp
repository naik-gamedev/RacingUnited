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

} // namespace heritage::math
