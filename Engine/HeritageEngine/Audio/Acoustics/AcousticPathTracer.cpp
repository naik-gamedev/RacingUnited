#include "AcousticPathTracer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace heritage::audio::acoustics {
namespace {

using heritage::math::Vec3;
using heritage::physics::ColliderHandle;
using heritage::physics::CollisionQueryFilter;
using heritage::physics::RaycastHit;
using heritage::physics::SurfaceMaterial;

Vec3 toPhysics(const AudioVector3& value)
{
    return { value.x, value.y, value.z };
}

Vec3 add(const Vec3& left, const Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

Vec3 subtract(const Vec3& left, const Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

Vec3 scale(const Vec3& value, float factor)
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

float dot(const Vec3& left, const Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

float length(const Vec3& value)
{
    return std::sqrt(std::max(dot(value, value), 0.0f));
}

Vec3 normalized(const Vec3& value)
{
    const float magnitude = length(value);
    return magnitude > 1.0e-5f
        ? scale(value, 1.0f / magnitude)
        : Vec3{ 0.0f, 1.0f, 0.0f };
}

float reflectionCoefficient(SurfaceMaterial material)
{
    switch (material)
    {
    case SurfaceMaterial::Asphalt: return 0.64f;
    case SurfaceMaterial::Kerb: return 0.72f;
    case SurfaceMaterial::PaintedLine: return 0.66f;
    case SurfaceMaterial::Gravel: return 0.38f;
    case SurfaceMaterial::Dirt: return 0.30f;
    case SurfaceMaterial::Grass: return 0.24f;
    case SurfaceMaterial::Snow:
    case SurfaceMaterial::DeepSnow: return 0.10f;
    case SurfaceMaterial::Mud:
    case SurfaceMaterial::Sand:
    case SurfaceMaterial::SoftSoil: return 0.18f;
    case SurfaceMaterial::Ice: return 0.70f;
    case SurfaceMaterial::Default:
    default: return 0.58f;
    }
}

float obstructionTransmission(SurfaceMaterial material)
{
    switch (material)
    {
    case SurfaceMaterial::Grass:
    case SurfaceMaterial::Dirt:
    case SurfaceMaterial::Gravel: return 0.30f;
    case SurfaceMaterial::Snow:
    case SurfaceMaterial::DeepSnow:
    case SurfaceMaterial::SoftSoil: return 0.24f;
    default: return 0.14f;
    }
}

bool visibleEndpoint(
    const Vec3& origin,
    const Vec3& endpoint,
    ColliderHandle expectedCollider,
    const CollisionQueryFilter& filter,
    const heritage::physics::CollisionSystem& collisions,
    const heritage::physics::RigidBodySystem& bodies,
    int& rayCount)
{
    const Vec3 delta = subtract(endpoint, origin);
    const float distance = length(delta);
    if (distance <= 0.05f)
        return false;

    RaycastHit hit;
    ++rayCount;
    if (!collisions.raycast(
            origin, normalized(delta), distance + 0.08f,
            filter, bodies, hit))
    {
        return false;
    }
    return hit.collider == expectedCollider
        && std::abs(hit.distance - distance) <= 0.22f;
}

} // namespace

AcousticPathTraceResult AcousticPathTracer::trace(
    const AcousticPathTraceInput& input,
    const heritage::physics::CollisionSystem& collisions,
    const heritage::physics::RigidBodySystem& bodies)
{
    AcousticPathTraceResult result;
    const Vec3 source = toPhysics(input.source);
    const Vec3 listener = toPhysics(input.listener);
    const Vec3 sourceToListener = subtract(listener, source);
    const float directDistance = length(sourceToListener);
    if (!std::isfinite(directDistance)
        || directDistance <= 0.05f
        || directDistance > std::max(input.maximumDistanceMeters, 0.1f))
    {
        return result;
    }

    CollisionQueryFilter filter;
    filter.includeTriggers = false;
    filter.ignoredBody = input.ignoredEmitterBody;

    RaycastHit directHit;
    ++result.tracedRayCount;
    if (collisions.raycast(
            source, normalized(sourceToListener),
            std::max(directDistance - 0.10f, 0.05f),
            filter, bodies, directHit))
    {
        // A hit immediately around the listener is normally its own vehicle
        // cabin. Cabin filtering is handled separately; it must not turn every
        // opponent into a sound hidden behind a concrete wall.
        const float remaining = directDistance - directHit.distance;
        if (remaining > 1.75f)
        {
            result.directOccluded = true;
            result.directGain = obstructionTransmission(
                directHit.surfaceMaterial);
            result.directOpenness = 0.16f;
        }
    }

    const Vec3 midpoint = scale(add(source, listener), 0.5f);
    constexpr float diagonal = 0.70710678f;
    const std::array<Vec3, 10> probeDirections{
        Vec3{ 0.0f, -1.0f, 0.0f }, Vec3{ 0.0f, 1.0f, 0.0f },
        Vec3{ 1.0f, 0.0f, 0.0f }, Vec3{ -1.0f, 0.0f, 0.0f },
        Vec3{ 0.0f, 0.0f, 1.0f }, Vec3{ 0.0f, 0.0f, -1.0f },
        Vec3{ diagonal, 0.0f, diagonal },
        Vec3{ -diagonal, 0.0f, diagonal },
        Vec3{ diagonal, 0.0f, -diagonal },
        Vec3{ -diagonal, 0.0f, -diagonal }
    };

    std::unordered_set<ColliderHandle> visited;
    float weightedDelay = 0.0f;
    float totalReflection = 0.0f;
    int nearbySurfaces = 0;
    const float probeDistance = std::clamp(
        12.0f + directDistance * 0.45f, 18.0f, 65.0f);

    for (const Vec3& probeDirection : probeDirections)
    {
        RaycastHit planeHit;
        ++result.tracedRayCount;
        if (!collisions.raycast(
                midpoint, probeDirection, probeDistance,
                filter, bodies, planeHit))
        {
            continue;
        }
        ++nearbySurfaces;
        if (!visited.insert(planeHit.collider).second)
            continue;

        const Vec3 normal = normalized(planeHit.normal);
        const float sourcePlaneDistance = dot(
            subtract(source, planeHit.point), normal);
        const Vec3 mirroredSource = subtract(
            source, scale(normal, 2.0f * sourcePlaneDistance));
        const Vec3 listenerToImage = subtract(mirroredSource, listener);
        const float denominator = dot(listenerToImage, normal);
        if (std::abs(denominator) <= 1.0e-5f)
            continue;

        const float interpolation = dot(
            subtract(planeHit.point, listener), normal) / denominator;
        if (interpolation <= 0.01f || interpolation >= 0.99f)
            continue;
        const Vec3 bounce = add(listener, scale(listenerToImage, interpolation));

        if (!visibleEndpoint(
                source, bounce, planeHit.collider, filter,
                collisions, bodies, result.tracedRayCount)
            || !visibleEndpoint(
                listener, bounce, planeHit.collider, filter,
                collisions, bodies, result.tracedRayCount))
        {
            continue;
        }

        const float pathLength = length(subtract(bounce, source))
            + length(subtract(listener, bounce));
        const float excessLength = std::max(pathLength - directDistance, 0.0f);
        const float spreading = directDistance / std::max(pathLength, directDistance);
        const float reflection = reflectionCoefficient(planeHit.surfaceMaterial)
            * spreading * spreading * 0.34f;
        totalReflection += reflection;
        weightedDelay += reflection * (excessLength / 343.0f);
        ++result.validReflectionPathCount;
    }

    result.earlyReflectionGain = std::clamp(totalReflection, 0.0f, 0.68f);
    result.earlyReflectionDelaySeconds = totalReflection > 1.0e-5f
        ? std::clamp(weightedDelay / totalReflection, 0.004f, 0.120f)
        : 0.0f;
    const float enclosure = static_cast<float>(nearbySurfaces)
        / static_cast<float>(probeDirections.size());
    result.lateReverbGain = std::clamp(
        result.earlyReflectionGain * 0.38f
            + enclosure * 0.12f
            + (result.directOccluded ? 0.10f : 0.0f),
        0.0f,
        0.42f);
    return result;
}

} // namespace heritage::audio::acoustics
