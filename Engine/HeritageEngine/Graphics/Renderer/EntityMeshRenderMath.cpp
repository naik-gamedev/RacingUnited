#include "EntityMeshRendererInternal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::graphics::entity_mesh_internal {
namespace {
constexpr float kPi = 3.14159265358979323846f;
}

heritage::math::Mat4 multiply(
    const heritage::math::Mat4& left,
    const heritage::math::Mat4& right)
{
    heritage::math::Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[column * 4 + row] =
                left.m[0 * 4 + row] * right.m[column * 4 + 0]
                + left.m[1 * 4 + row] * right.m[column * 4 + 1]
                + left.m[2 * 4 + row] * right.m[column * 4 + 2]
                + left.m[3 * 4 + row] * right.m[column * 4 + 3];
        }
    }
    return result;
}

float linearDeterminant3x3(const heritage::math::Mat4& matrix)
{
    const float a00 = matrix.m[0];
    const float a01 = matrix.m[4];
    const float a02 = matrix.m[8];
    const float a10 = matrix.m[1];
    const float a11 = matrix.m[5];
    const float a12 = matrix.m[9];
    const float a20 = matrix.m[2];
    const float a21 = matrix.m[6];
    const float a22 = matrix.m[10];

    return
        a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);
}



FrustumPlane normalizedPlane(float a, float b, float c, float d)
{
    const float lengthSquared = a * a + b * b + c * c;
    if (lengthSquared <= 1.0e-20f)
        return {};
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        { a * inverseLength, b * inverseLength, c * inverseLength },
        d * inverseLength
    };
}

ViewFrustum extractViewFrustum(
    const heritage::math::Mat4& projection,
    const heritage::math::Mat4& view)
{
    // OpenGL clip inequalities remain -w..+w even with reversed-Z; reversed-Z
    // only swaps which depth end represents near/far. Extract all six planes
    // from projection*view so this also works for asymmetric/triple displays.
    const heritage::math::Mat4 clip = multiply(projection, view);
    ViewFrustum result;
    result.planes[0] = normalizedPlane(
        clip.m[3] + clip.m[0], clip.m[7] + clip.m[4],
        clip.m[11] + clip.m[8], clip.m[15] + clip.m[12]); // left
    result.planes[1] = normalizedPlane(
        clip.m[3] - clip.m[0], clip.m[7] - clip.m[4],
        clip.m[11] - clip.m[8], clip.m[15] - clip.m[12]); // right
    result.planes[2] = normalizedPlane(
        clip.m[3] + clip.m[1], clip.m[7] + clip.m[5],
        clip.m[11] + clip.m[9], clip.m[15] + clip.m[13]); // bottom
    result.planes[3] = normalizedPlane(
        clip.m[3] - clip.m[1], clip.m[7] - clip.m[5],
        clip.m[11] - clip.m[9], clip.m[15] - clip.m[13]); // top
    result.planes[4] = normalizedPlane(
        clip.m[3] + clip.m[2], clip.m[7] + clip.m[6],
        clip.m[11] + clip.m[10], clip.m[15] + clip.m[14]); // z + w
    result.planes[5] = normalizedPlane(
        clip.m[3] - clip.m[2], clip.m[7] - clip.m[6],
        clip.m[11] - clip.m[10], clip.m[15] - clip.m[14]); // w - z
    return result;
}

heritage::math::Vec3 transformPoint(
    const heritage::math::Mat4& matrix,
    const std::array<float, 3>& point)
{
    return {
        matrix.m[0] * point[0] + matrix.m[4] * point[1]
            + matrix.m[8] * point[2] + matrix.m[12],
        matrix.m[1] * point[0] + matrix.m[5] * point[1]
            + matrix.m[9] * point[2] + matrix.m[13],
        matrix.m[2] * point[0] + matrix.m[6] * point[1]
            + matrix.m[10] * point[2] + matrix.m[14]
    };
}

float maximumLinearScale(const heritage::math::Mat4& matrix)
{
    const auto columnLength = [&](int base) {
        return std::sqrt(
            matrix.m[base] * matrix.m[base]
            + matrix.m[base + 1] * matrix.m[base + 1]
            + matrix.m[base + 2] * matrix.m[base + 2]);
    };
    return std::max({ columnLength(0), columnLength(4), columnLength(8) });
}

bool sphereOutsideFrustum(
    const ViewFrustum& frustum,
    const heritage::math::Vec3& center,
    float radius)
{
    radius = std::max(radius, 0.0f);
    for (const FrustumPlane& plane : frustum.planes)
    {
        const float signedDistance =
            plane.normal.x * center.x
            + plane.normal.y * center.y
            + plane.normal.z * center.z
            + plane.distance;
        if (signedDistance < -radius)
            return true;
    }
    return false;
}

heritage::math::Mat4 translation(const heritage::math::Vec3& value)
{
    heritage::math::Mat4 result = heritage::math::identity();
    result.m[12] = value.x;
    result.m[13] = value.y;
    result.m[14] = value.z;
    return result;
}

heritage::math::Mat4 scaleMatrix(const heritage::math::Vec3& value)
{
    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    return result;
}

heritage::math::Mat4 rotationX(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

heritage::math::Mat4 rotationY(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

heritage::math::Mat4 rotationZ(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

heritage::math::Mat4 eulerRotationDegrees(
    const heritage::math::Vec3& rotationDegrees)
{
    const float toRadians = kPi / 180.0f;
    return multiply(
        rotationZ(rotationDegrees.z * toRadians),
        multiply(
            rotationY(rotationDegrees.y * toRadians),
            rotationX(rotationDegrees.x * toRadians)));
}

heritage::math::Mat4 worldPoseMatrix(
    const heritage::math::Vec3& position,
    const heritage::math::Vec3& rotationDegrees)
{
    return multiply(translation(position), eulerRotationDegrees(rotationDegrees));
}

heritage::math::Mat4 modelMatrix(
    const heritage::entities::MeshInstance& instance)
{
    const float toRadians = kPi / 180.0f;
    const heritage::math::Mat4 rotation = multiply(
        rotationZ(instance.rotationDegrees.z * toRadians),
        multiply(
            rotationY(instance.rotationDegrees.y * toRadians),
            rotationX(instance.rotationDegrees.x * toRadians)));
    return multiply(
        translation(instance.position),
        multiply(rotation, scaleMatrix(instance.scale)));
}

heritage::math::Mat4 arrayMatrix(const std::array<float, 16>& value)
{
    heritage::math::Mat4 result{};
    for (std::size_t i = 0; i < 16; ++i)
        result.m[i] = value[i];
    return result;
}

heritage::math::Mat4 quaternionMatrix(
    float x,
    float y,
    float z,
    float w)
{
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = 1.0f - 2.0f * (yy + zz);
    result.m[1] = 2.0f * (xy + wz);
    result.m[2] = 2.0f * (xz - wy);
    result.m[4] = 2.0f * (xy - wz);
    result.m[5] = 1.0f - 2.0f * (xx + zz);
    result.m[6] = 2.0f * (yz + wx);
    result.m[8] = 2.0f * (xz + wy);
    result.m[9] = 2.0f * (yz - wx);
    result.m[10] = 1.0f - 2.0f * (xx + yy);
    return result;
}

std::array<float, 4> normalizedQuaternion(const std::array<float, 4>& q)
{
    const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length <= 1.0e-8f)
        return { 0.0f, 0.0f, 0.0f, 1.0f };
    return { q[0] / length, q[1] / length, q[2] / length, q[3] / length };
}

std::array<float, 4> lerpQuaternion(
    const std::array<float, 4>& a,
    const std::array<float, 4>& b,
    float alpha)
{
    std::array<float, 4> end = b;
    const float dotProduct = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dotProduct < 0.0f)
    {
        end[0] = -end[0];
        end[1] = -end[1];
        end[2] = -end[2];
        end[3] = -end[3];
    }
    return normalizedQuaternion({
        a[0] + (end[0] - a[0]) * alpha,
        a[1] + (end[1] - a[1]) * alpha,
        a[2] + (end[2] - a[2]) * alpha,
        a[3] + (end[3] - a[3]) * alpha });
}

heritage::math::Mat4 trsMatrix(
    const std::array<float, 3>& translationValue,
    const std::array<float, 4>& rotationValue,
    const std::array<float, 3>& scaleValue)
{
    return multiply(
        translation({ translationValue[0], translationValue[1], translationValue[2] }),
        multiply(
            quaternionMatrix(
                rotationValue[0],
                rotationValue[1],
                rotationValue[2],
                rotationValue[3]),
            scaleMatrix({ scaleValue[0], scaleValue[1], scaleValue[2] })));
}

heritage::math::Mat4 inverseMatrix(const heritage::math::Mat4& m)
{
    heritage::math::Mat4 inv{};
    inv.m[0] = m.m[5]  * m.m[10] * m.m[15] - 
             m.m[5]  * m.m[11] * m.m[14] - 
             m.m[9]  * m.m[6]  * m.m[15] + 
             m.m[9]  * m.m[7]  * m.m[14] +
             m.m[13] * m.m[6]  * m.m[11] - 
             m.m[13] * m.m[7]  * m.m[10];

    inv.m[4] = -m.m[4]  * m.m[10] * m.m[15] + 
              m.m[4]  * m.m[11] * m.m[14] + 
              m.m[8]  * m.m[6]  * m.m[15] - 
              m.m[8]  * m.m[7]  * m.m[14] - 
              m.m[12] * m.m[6]  * m.m[11] + 
              m.m[12] * m.m[7]  * m.m[10];

    inv.m[8] = m.m[4]  * m.m[9] * m.m[15] - 
             m.m[4]  * m.m[11] * m.m[13] - 
             m.m[8]  * m.m[5] * m.m[15] + 
             m.m[8]  * m.m[7] * m.m[13] + 
             m.m[12] * m.m[5] * m.m[11] - 
             m.m[12] * m.m[7] * m.m[9];

    inv.m[12] = -m.m[4]  * m.m[9] * m.m[14] + 
               m.m[4]  * m.m[10] * m.m[13] +
               m.m[8]  * m.m[5] * m.m[14] - 
               m.m[8]  * m.m[6] * m.m[13] - 
               m.m[12] * m.m[5] * m.m[10] + 
               m.m[12] * m.m[6] * m.m[9];

    inv.m[1] = -m.m[1]  * m.m[10] * m.m[15] + 
              m.m[1]  * m.m[11] * m.m[14] + 
              m.m[9]  * m.m[2] * m.m[15] - 
              m.m[9]  * m.m[3] * m.m[14] - 
              m.m[13] * m.m[2] * m.m[11] + 
              m.m[13] * m.m[3] * m.m[10];

    inv.m[5] = m.m[0]  * m.m[10] * m.m[15] - 
             m.m[0]  * m.m[11] * m.m[14] - 
             m.m[8]  * m.m[2] * m.m[15] + 
             m.m[8]  * m.m[3] * m.m[14] + 
             m.m[12] * m.m[2] * m.m[11] - 
             m.m[12] * m.m[3] * m.m[10];

    inv.m[9] = -m.m[0]  * m.m[9] * m.m[15] + 
              m.m[0]  * m.m[11] * m.m[13] + 
              m.m[8]  * m.m[1] * m.m[15] - 
              m.m[8]  * m.m[3] * m.m[13] - 
              m.m[12] * m.m[1] * m.m[11] + 
              m.m[12] * m.m[3] * m.m[9];

    inv.m[13] = m.m[0]  * m.m[9] * m.m[14] - 
              m.m[0]  * m.m[10] * m.m[13] - 
              m.m[8]  * m.m[1] * m.m[14] + 
              m.m[8]  * m.m[2] * m.m[13] + 
              m.m[12] * m.m[1] * m.m[10] - 
              m.m[12] * m.m[2] * m.m[9];

    inv.m[2] = m.m[1]  * m.m[6] * m.m[15] - 
             m.m[1]  * m.m[7] * m.m[14] - 
             m.m[5]  * m.m[2] * m.m[15] + 
             m.m[5]  * m.m[3] * m.m[14] + 
             m.m[13] * m.m[2] * m.m[7] - 
             m.m[13] * m.m[3] * m.m[6];

    inv.m[6] = -m.m[0]  * m.m[6] * m.m[15] + 
              m.m[0]  * m.m[7] * m.m[14] + 
              m.m[4]  * m.m[2] * m.m[15] - 
              m.m[4]  * m.m[3] * m.m[14] - 
              m.m[12] * m.m[2] * m.m[7] + 
              m.m[12] * m.m[3] * m.m[6];

    inv.m[10] = m.m[0]  * m.m[5] * m.m[15] - 
              m.m[0]  * m.m[7] * m.m[13] - 
              m.m[4]  * m.m[1] * m.m[15] + 
              m.m[4]  * m.m[3] * m.m[13] + 
              m.m[12] * m.m[1] * m.m[7] - 
              m.m[12] * m.m[3] * m.m[5];

    inv.m[14] = -m.m[0]  * m.m[5] * m.m[14] + 
               m.m[0]  * m.m[6] * m.m[13] + 
               m.m[4]  * m.m[1] * m.m[14] - 
               m.m[4]  * m.m[2] * m.m[13] - 
               m.m[12] * m.m[1] * m.m[6] + 
               m.m[12] * m.m[2] * m.m[5];

    inv.m[3] = -m.m[1] * m.m[6] * m.m[11] + 
              m.m[1] * m.m[7] * m.m[10] + 
              m.m[5] * m.m[2] * m.m[11] - 
              m.m[5] * m.m[3] * m.m[10] - 
              m.m[9] * m.m[2] * m.m[7] + 
              m.m[9] * m.m[3] * m.m[6];

    inv.m[7] = m.m[0] * m.m[6] * m.m[11] - 
             m.m[0] * m.m[7] * m.m[10] - 
             m.m[4] * m.m[2] * m.m[11] + 
             m.m[4] * m.m[3] * m.m[10] + 
             m.m[8] * m.m[2] * m.m[7] - 
             m.m[8] * m.m[3] * m.m[6];

    inv.m[11] = -m.m[0] * m.m[5] * m.m[11] + 
               m.m[0] * m.m[7] * m.m[9] + 
               m.m[4] * m.m[1] * m.m[11] - 
               m.m[4] * m.m[3] * m.m[9] - 
               m.m[8] * m.m[1] * m.m[7] + 
               m.m[8] * m.m[3] * m.m[5];

    inv.m[15] = m.m[0] * m.m[5] * m.m[10] - 
              m.m[0] * m.m[6] * m.m[9] - 
              m.m[4] * m.m[1] * m.m[10] + 
              m.m[4] * m.m[2] * m.m[9] + 
              m.m[8] * m.m[1] * m.m[6] - 
              m.m[8] * m.m[2] * m.m[5];

    float determinant = m.m[0] * inv.m[0] + m.m[1] * inv.m[4] + m.m[2] * inv.m[8] + m.m[3] * inv.m[12];
    if (std::abs(determinant) <= 1.0e-8f)
        return heritage::math::identity();

    determinant = 1.0f / determinant;
    for (float& value : inv.m)
        value *= determinant;
    return inv;
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
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

heritage::math::Vec3 normalize(const heritage::math::Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.000001f)
        return { 0.0f, 0.0f, 0.0f };
    return { value.x / length, value.y / length, value.z / length };
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 multiplyVector(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(dot(value, value));
}

heritage::math::Vec3 transformPoint(
    const heritage::math::Mat4& matrix,
    const heritage::math::Vec3& point)
{
    const float x = matrix.m[0] * point.x + matrix.m[4] * point.y
        + matrix.m[8] * point.z + matrix.m[12];
    const float y = matrix.m[1] * point.x + matrix.m[5] * point.y
        + matrix.m[9] * point.z + matrix.m[13];
    const float z = matrix.m[2] * point.x + matrix.m[6] * point.y
        + matrix.m[10] * point.z + matrix.m[14];
    const float w = matrix.m[3] * point.x + matrix.m[7] * point.y
        + matrix.m[11] * point.z + matrix.m[15];
    if (std::abs(w) > 1.0e-7f && std::abs(w - 1.0f) > 1.0e-7f)
        return { x / w, y / w, z / w };
    return { x, y, z };
}

heritage::math::Vec3 unprojectNdc(
    const heritage::math::Mat4& inverseClip,
    float x,
    float y,
    float z)
{
    const float px = inverseClip.m[0] * x + inverseClip.m[4] * y
        + inverseClip.m[8] * z + inverseClip.m[12];
    const float py = inverseClip.m[1] * x + inverseClip.m[5] * y
        + inverseClip.m[9] * z + inverseClip.m[13];
    const float pz = inverseClip.m[2] * x + inverseClip.m[6] * y
        + inverseClip.m[10] * z + inverseClip.m[14];
    const float pw = inverseClip.m[3] * x + inverseClip.m[7] * y
        + inverseClip.m[11] * z + inverseClip.m[15];
    if (std::abs(pw) <= 1.0e-7f)
        return {};
    return { px / pw, py / pw, pz / pw };
}

heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up)
{
    const heritage::math::Vec3 forward = normalize(subtract(target, eye));
    const heritage::math::Vec3 side = normalize(cross(forward, up));
    const heritage::math::Vec3 correctedUp = cross(side, forward);

    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = side.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = side.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = side.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(side, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

heritage::math::Mat4 orthographic(
    float left,
    float right,
    float bottom,
    float top,
    float zNear,
    float zFar)
{
    heritage::math::Mat4 result{};
    const float width = std::max(right - left, 0.0001f);
    const float height = std::max(top - bottom, 0.0001f);
    const float depth = std::max(zFar - zNear, 0.0001f);
    result.m[0] = 2.0f / width;
    result.m[5] = 2.0f / height;
    result.m[10] = -2.0f / depth;
    result.m[12] = -(right + left) / width;
    result.m[13] = -(top + bottom) / height;
    result.m[14] = -(zFar + zNear) / depth;
    result.m[15] = 1.0f;
    return result;
}

} // namespace heritage::graphics::entity_mesh_internal
