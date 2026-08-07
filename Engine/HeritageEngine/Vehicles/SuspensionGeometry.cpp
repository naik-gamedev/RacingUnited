#include "SuspensionGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kVectorEpsilon = 0.000001f;

float radians(float degreesValue)
{
    return degreesValue * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float factor)
{
    return { value.x * factor, value.y * factor, value.z * factor };
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

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitudeSquared = dot(value, value);
    if (magnitudeSquared <= kVectorEpsilon * kVectorEpsilon)
        return fallback;
    return scale(value, 1.0f / std::sqrt(magnitudeSquared));
}

heritage::math::Vec3 rotateAroundAxis(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& unitAxis,
    float angleDegrees)
{
    const float angle = radians(angleDegrees);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return add(
        add(
            scale(value, cosine),
            scale(cross(unitAxis, value), sine)),
        scale(unitAxis, dot(unitAxis, value) * (1.0f - cosine)));
}

float travelCurve(
    float staticDegrees,
    float gainDegreesPerM,
    float progressionDegreesPerM2,
    float compressionM)
{
    return staticDegrees + gainDegreesPerM * compressionM
        + 0.5f * progressionDegreesPerM2
            * compressionM * std::abs(compressionM);
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
    if (std::abs(cosineY) > kVectorEpsilon)
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

} // namespace

SuspensionGeometryOutput evaluateSuspensionGeometry(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input)
{
    SuspensionGeometryOutput output;
    if (description.provider != SuspensionProviderKind::LinearRaycastV1)
        return output;

    output.camberDegrees = travelCurve(
        description.staticCamberDegrees,
        description.camberGainDegreesPerM,
        description.camberProgressionDegreesPerM2,
        input.compressionM);
    output.toeDegrees = travelCurve(
        description.staticToeDegrees,
        description.toeGainDegreesPerM,
        description.toeProgressionDegreesPerM2,
        input.compressionM);
    output.localSteeringAxis = normalized(
        description.localSteeringAxis,
        { 0.0f, 1.0f, 0.0f });

    heritage::math::Vec3 forward = rotateAroundAxis(
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },
        output.toeDegrees);
    heritage::math::Vec3 right = rotateAroundAxis(
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        output.toeDegrees);
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };

    right = rotateAroundAxis(right, forward, output.camberDegrees);
    up = rotateAroundAxis(up, forward, output.camberDegrees);

    forward = rotateAroundAxis(
        forward,
        output.localSteeringAxis,
        input.steeringDegrees);
    right = rotateAroundAxis(
        right,
        output.localSteeringAxis,
        input.steeringDegrees);
    up = rotateAroundAxis(
        up,
        output.localSteeringAxis,
        input.steeringDegrees);

    // Re-orthogonalize after the composed rotations so downstream tire and
    // presentation code receives a stable right-handed upright basis.
    output.localWheelForward = normalized(
        forward,
        { 0.0f, 0.0f, 1.0f });
    output.localWheelRight = normalized(
        right,
        { 1.0f, 0.0f, 0.0f });
    output.localWheelUp = normalized(
        cross(output.localWheelForward, output.localWheelRight),
        { 0.0f, 1.0f, 0.0f });
    output.localWheelRight = normalized(
        cross(output.localWheelUp, output.localWheelForward),
        { 1.0f, 0.0f, 0.0f });
    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        output.localWheelRight,
        output.localWheelUp,
        output.localWheelForward);
    return output;
}

} // namespace heritage::vehicles
