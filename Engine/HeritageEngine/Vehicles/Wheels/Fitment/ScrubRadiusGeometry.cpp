#include "ScrubRadiusGeometry.hpp"

#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-10;

VehicleScalar dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return static_cast<VehicleScalar>(left.x) * right.x
        + static_cast<VehicleScalar>(left.y) * right.y
        + static_cast<VehicleScalar>(left.z) * right.z;
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    VehicleScalar scalar)
{
    return {
        static_cast<float>(static_cast<VehicleScalar>(value.x) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.y) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.z) * scalar) };
}

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

SteeringGroundGeometry evaluateSteeringGroundGeometry(
    const heritage::math::Vec3& steeringAxisPointWorld,
    const heritage::math::Vec3& steeringAxisDirectionWorld,
    const heritage::math::Vec3& contactPointWorld,
    const heritage::math::Vec3& contactNormalWorld,
    const heritage::math::Vec3& wheelForwardWorld,
    const heritage::math::Vec3& wheelRightWorld)
{
    SteeringGroundGeometry result;
    if (!finite(steeringAxisPointWorld)
        || !finite(steeringAxisDirectionWorld)
        || !finite(contactPointWorld)
        || !finite(contactNormalWorld)
        || !finite(wheelForwardWorld)
        || !finite(wheelRightWorld))
    {
        return result;
    }

    const VehicleScalar denominator = dot(
        steeringAxisDirectionWorld,
        contactNormalWorld);
    if (!std::isfinite(denominator) || std::abs(denominator) <= kEpsilon)
        return result;

    const VehicleScalar lineParameter = dot(
        subtract(contactPointWorld, steeringAxisPointWorld),
        contactNormalWorld) / denominator;
    if (!std::isfinite(lineParameter)
        || std::abs(lineParameter) > 1000.0)
    {
        return result;
    }

    result.steeringAxisGroundPointWorld = add(
        steeringAxisPointWorld,
        scale(steeringAxisDirectionWorld, lineParameter));

    const heritage::math::Vec3 axisToContact = subtract(
        contactPointWorld,
        result.steeringAxisGroundPointWorld);
    result.signedScrubRadiusM = dot(axisToContact, wheelRightWorld);
    result.scrubRadiusMagnitudeM = std::abs(result.signedScrubRadiusM);
    // Positive when the steering-axis ground intercept lies ahead of the
    // contact patch in the wheel's current forward direction.
    result.mechanicalTrailM = -dot(axisToContact, wheelForwardWorld);
    result.valid = finite(result.steeringAxisGroundPointWorld)
        && std::isfinite(result.signedScrubRadiusM)
        && std::isfinite(result.mechanicalTrailM);
    return result;
}

} // namespace heritage::vehicles
