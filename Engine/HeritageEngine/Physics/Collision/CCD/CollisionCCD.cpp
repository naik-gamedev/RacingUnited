// Continuous collision detection for fast dynamic bodies.

#include "../../CollisionSystem.hpp"
#include "../CollisionInternal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace heritage::physics {
using namespace collision_detail;

float CollisionSystem::centredSphereRadiusForContinuousCollision(
    BodyHandle bodyHandle,
    const RigidBodySystem& bodies,
    const Record*& sourceCollider) const
{
    sourceCollider = nullptr;
    float radius = 0.0f;
    bool foundSolidCollider = false;

    for (const Slot& slot : m_slots)
    {
        if (!slot.alive
            || slot.record.body != bodyHandle
            || slot.record.trigger)
        {
            continue;
        }

        foundSolidCollider = true;
        if (slot.record.shapeType != ColliderShapeType::Sphere
            || lengthSquared(slot.record.localPosition)
                > kContinuousCollisionLocalOffsetEpsilon
                    * kContinuousCollisionLocalOffsetEpsilon)
        {
            return 0.0f;
        }

        if (slot.record.radius > radius)
        {
            radius = slot.record.radius;
            sourceCollider = &slot.record;
        }
    }

    if (!foundSolidCollider || !bodies.exists(bodyHandle))
        return 0.0f;
    return radius;
}
void CollisionSystem::applyContinuousCollisionDetection(
    RigidBodySystem& bodies,
    float fixedDeltaTime)
{
    (void)fixedDeltaTime;

    for (std::uint32_t bodyIndex = 0;
         bodyIndex < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++bodyIndex)
    {
        RigidBodySystem::Slot& bodySlot = bodies.m_slots[bodyIndex];
        if (!bodySlot.alive)
            continue;

        RigidBodySystem::Record& bodyRecord = bodySlot.record;
        if (bodyRecord.motionType != BodyMotionType::Dynamic
            || bodyRecord.sleeping
            || !bodyRecord.continuousCollision)
        {
            continue;
        }

        ++m_continuousCollisionBodyCount;
        const BodyHandle bodyHandle = RigidBodySystem::makeHandle(
            bodyIndex,
            bodySlot.generation);

        const Record* sourceCollider = nullptr;
        const float castRadius = centredSphereRadiusForContinuousCollision(
            bodyHandle,
            bodies,
            sourceCollider);

        const Record* firstSolidCollider = nullptr;
        float characteristicExtent =
            (std::numeric_limits<float>::max)();
        bool staticSweepAllowed = false;
        for (const Slot& slot : m_slots)
        {
            if (!slot.alive
                || slot.record.body != bodyHandle
                || slot.record.trigger)
            {
                continue;
            }

            if (!firstSolidCollider)
                firstSolidCollider = &slot.record;
            staticSweepAllowed = staticSweepAllowed
                || (slot.record.mask & 1u) != 0u;
            const float extent = slot.record.shapeType
                    == ColliderShapeType::Sphere
                ? slot.record.radius
                : (std::min)({
                    slot.record.halfExtents.x,
                    slot.record.halfExtents.y,
                    slot.record.halfExtents.z });
            characteristicExtent = (std::min)(
                characteristicExtent,
                extent);
        }

        const bool staticSweepSupported = firstSolidCollider
            && staticSweepAllowed
            && !m_staticTriangleBvh.empty();
        const bool primitiveSweepSupported = castRadius > 0.0f
            && sourceCollider;
        if (!primitiveSweepSupported && !staticSweepSupported)
        {
            ++m_continuousCollisionUnsupportedBodyCount;
            continue;
        }

        const heritage::math::Vec3 motion = subtract(
            bodyRecord.position,
            bodyRecord.previousPosition);
        const float travelDistance = length(motion);
        const float minimumTravel = (std::max)(
            kContinuousCollisionMinimumTravel,
            characteristicExtent * kContinuousCollisionTravelFraction);
        if (travelDistance <= minimumTravel)
            continue;

        ++m_continuousCollisionSweepCount;
        const heritage::math::Vec3 travelDirection = scaleVector(
            motion,
            1.0f / travelDistance);
        const heritage::math::Vec3 radiusExtents{
            castRadius,
            castRadius,
            castRadius
        };
        const Aabb sweptBounds{
            subtract(
                {
                    (std::min)(bodyRecord.previousPosition.x, bodyRecord.position.x),
                    (std::min)(bodyRecord.previousPosition.y, bodyRecord.position.y),
                    (std::min)(bodyRecord.previousPosition.z, bodyRecord.position.z)
                },
                radiusExtents),
            add(
                {
                    (std::max)(bodyRecord.previousPosition.x, bodyRecord.position.x),
                    (std::max)(bodyRecord.previousPosition.y, bodyRecord.position.y),
                    (std::max)(bodyRecord.previousPosition.z, bodyRecord.position.z)
                },
                radiusExtents)
        };

        bool found = false;
        bool closestIsStaticTriangle = false;
        SphereCastHit closest;
        closest.distance = travelDistance;
        const Record* selectedSourceCollider = nullptr;
        const Record* closestCollider = nullptr;
        heritage::math::Vec3 closestStaticPathEnd{};

        if (primitiveSweepSupported)
        {
            for (std::uint32_t colliderIndex = 0;
                 colliderIndex < static_cast<std::uint32_t>(m_slots.size());
                 ++colliderIndex)
            {
                const Slot& targetSlot = m_slots[colliderIndex];
                if (!targetSlot.alive
                    || targetSlot.record.body == bodyHandle
                    || targetSlot.record.trigger)
                {
                    continue;
                }

                if ((sourceCollider->mask & targetSlot.record.layer) == 0u
                    || (targetSlot.record.mask & sourceCollider->layer) == 0u)
                {
                    continue;
                }

                const RigidBodySystem::Slot* targetBodySlot =
                    bodies.resolve(targetSlot.record.body);
                if (!targetBodySlot
                    || targetBodySlot->record.motionType
                        == BodyMotionType::Dynamic)
                {
                    // Dynamic-vs-dynamic time of impact requires relative
                    // swept shapes and island-level substepping. It remains a
                    // separate later feature.
                    continue;
                }

                if (!aabbOverlap(
                        sweptBounds,
                        worldAabb(targetSlot.record, targetBodySlot->record)))
                {
                    continue;
                }

                SphereCastHit candidate;
                if (!sphereCastCollider(
                        bodyRecord.previousPosition,
                        castRadius,
                        travelDirection,
                        travelDistance,
                        makeHandle(colliderIndex, targetSlot.generation),
                        targetSlot.record,
                        targetBodySlot->record,
                        candidate))
                {
                    continue;
                }

                if (!found
                    || candidate.distance
                        < closest.distance - kContactEpsilon
                    || (std::abs(candidate.distance - closest.distance)
                            <= kContactEpsilon
                        && candidate.collider < closest.collider))
                {
                    found = true;
                    closestIsStaticTriangle = false;
                    closest = candidate;
                    selectedSourceCollider = sourceCollider;
                    closestCollider = &targetSlot.record;
                }
            }
        }

        if (staticSweepSupported)
        {
            const auto traceStaticPoint =
                [&](const heritage::math::Vec3& previousPoint,
                    const heritage::math::Vec3& currentPoint,
                    const Record& currentSource) {
                    const heritage::math::Vec3 pointMotion = subtract(
                        currentPoint,
                        previousPoint);
                    const float pointDistance = length(pointMotion);
                    if (pointDistance <= kContactEpsilon)
                        return;
                    const heritage::math::Vec3 pointDirection = scaleVector(
                        pointMotion,
                        1.0f / pointDistance);

                    RaycastHit candidate;
                    std::uint32_t triangleIndex = 0;
                    std::size_t nodeTestCount = 0;
                    if (!raycastStaticSceneOnly(
                            previousPoint,
                            pointDirection,
                            pointDistance,
                            candidate,
                            triangleIndex,
                            nodeTestCount))
                    {
                        m_staticBroadphaseNodeTestCount += nodeTestCount;
                        return;
                    }
                    m_staticBroadphaseNodeTestCount += nodeTestCount;

                    const float equivalentBodyDistance =
                        candidate.fraction * travelDistance;
                    const ColliderHandle triangleHandle =
                        staticTriangleColliderHandle(triangleIndex);
                    if (found
                        && equivalentBodyDistance
                            > closest.distance + kContactEpsilon)
                    {
                        return;
                    }
                    if (found
                        && std::abs(
                            equivalentBodyDistance - closest.distance)
                                <= kContactEpsilon
                        && triangleHandle >= closest.collider)
                    {
                        return;
                    }

                    found = true;
                    closestIsStaticTriangle = true;
                    closest.collider = triangleHandle;
                    closest.body = InvalidBody;
                    closest.point = candidate.point;
                    closest.normal = candidate.normal;
                    closest.distance = equivalentBodyDistance;
                    closest.fraction = candidate.fraction;
                    closest.surfaceMaterial = candidate.surfaceMaterial;
                    closest.surfaceWetness = candidate.surfaceWetness;
                    closest.surfaceProperties = candidate.surfaceProperties;
                    closest.trigger = false;
                    closestStaticPathEnd = currentPoint;
                    selectedSourceCollider = &currentSource;
                    closestCollider = nullptr;
                };

            for (std::uint32_t colliderIndex = 0;
                 colliderIndex < static_cast<std::uint32_t>(m_slots.size());
                 ++colliderIndex)
            {
                const Slot& sourceSlot = m_slots[colliderIndex];
                if (!sourceSlot.alive
                    || sourceSlot.record.body != bodyHandle
                    || sourceSlot.record.trigger
                    || (sourceSlot.record.mask & 1u) == 0u)
                {
                    continue;
                }

                const Record& source = sourceSlot.record;
                const auto worldSample =
                    [&](const RigidBodySystem::Quaternion& rotation,
                        const heritage::math::Vec3& bodyPosition,
                        const heritage::math::Vec3& localSample) {
                        return add(
                            bodyPosition,
                            rotateVector(rotation, localSample));
                    };

                if (source.shapeType == ColliderShapeType::Sphere)
                {
                    const heritage::math::Vec3 previousCenter = worldSample(
                        bodyRecord.previousRotation,
                        bodyRecord.previousPosition,
                        source.localPosition);
                    const heritage::math::Vec3 currentCenter = worldSample(
                        bodyRecord.rotation,
                        bodyRecord.position,
                        source.localPosition);
                    const heritage::math::Vec3 centerMotion = subtract(
                        currentCenter,
                        previousCenter);
                    const float centerDistance = length(centerMotion);
                    if (centerDistance <= kContactEpsilon)
                        continue;
                    const heritage::math::Vec3 centerDirection = scaleVector(
                        centerMotion,
                        1.0f / centerDistance);
                    const heritage::math::Vec3 leadingOffset = scaleVector(
                        centerDirection,
                        source.radius);
                    traceStaticPoint(
                        add(previousCenter, leadingOffset),
                        add(currentCenter, leadingOffset),
                        source);
                    continue;
                }

                for (int x = -1; x <= 1; x += 2)
                {
                    for (int y = -1; y <= 1; y += 2)
                    {
                        for (int z = -1; z <= 1; z += 2)
                        {
                            const heritage::math::Vec3 localSample = add(
                                source.localPosition,
                                {
                                    source.halfExtents.x
                                        * static_cast<float>(x),
                                    source.halfExtents.y
                                        * static_cast<float>(y),
                                    source.halfExtents.z
                                        * static_cast<float>(z)
                                });
                            traceStaticPoint(
                                worldSample(
                                    bodyRecord.previousRotation,
                                    bodyRecord.previousPosition,
                                    localSample),
                                worldSample(
                                    bodyRecord.rotation,
                                    bodyRecord.position,
                                    localSample),
                                source);
                        }
                    }
                }

                for (int axis = 0; axis < 3; ++axis)
                {
                    for (int sign = -1; sign <= 1; sign += 2)
                    {
                        heritage::math::Vec3 localSample =
                            source.localPosition;
                        if (axis == 0)
                        {
                            localSample.x += source.halfExtents.x
                                * static_cast<float>(sign);
                        }
                        else if (axis == 1)
                        {
                            localSample.y += source.halfExtents.y
                                * static_cast<float>(sign);
                        }
                        else
                        {
                            localSample.z += source.halfExtents.z
                                * static_cast<float>(sign);
                        }
                        traceStaticPoint(
                            worldSample(
                                bodyRecord.previousRotation,
                                bodyRecord.previousPosition,
                                localSample),
                            worldSample(
                                bodyRecord.rotation,
                                bodyRecord.position,
                                localSample),
                            source);
                    }
                }

                traceStaticPoint(
                    worldSample(
                        bodyRecord.previousRotation,
                        bodyRecord.previousPosition,
                        source.localPosition),
                    worldSample(
                        bodyRecord.rotation,
                        bodyRecord.position,
                        source.localPosition),
                    source);
            }
        }

        if (!found || !selectedSourceCollider)
            continue;

        ++m_continuousCollisionHitCount;
        if (closestIsStaticTriangle)
        {
            // Correct only the motion into the triangle plane. Keeping the
            // tangential component avoids the classic CCD failure where a
            // fast body becomes glued to a road or steep slope.
            const heritage::math::Vec3 remainingPath = subtract(
                closestStaticPathEnd,
                closest.point);
            const float inwardDistance = (std::max)(
                0.0f,
                -dot(remainingPath, closest.normal));
            bodyRecord.position = add(
                bodyRecord.position,
                scaleVector(
                    closest.normal,
                    inwardDistance + kContinuousCollisionSkin));
        }
        else
        {
            const float safeDistance = (std::max)(
                0.0f,
                closest.distance - kContinuousCollisionSkin);
            bodyRecord.position = add(
                bodyRecord.previousPosition,
                scaleVector(travelDirection, safeDistance));
        }

        const float velocityAlongNormal = dot(
            bodyRecord.linearVelocity,
            closest.normal);
        if (velocityAlongNormal < 0.0f)
        {
            const float restitution =
                !closestIsStaticTriangle
                    && closestCollider
                    && velocityAlongNormal
                        < -kRestitutionVelocityThreshold
                ? (std::min)(
                    selectedSourceCollider->restitution,
                    closestCollider->restitution)
                : 0.0f;
            bodyRecord.linearVelocity = subtract(
                bodyRecord.linearVelocity,
                scaleVector(
                    closest.normal,
                    (1.0f + restitution) * velocityAlongNormal));
        }

        RigidBodySystem::wakeRecord(bodyRecord);
        ++m_continuousCollisionClampedBodyCount;
    }
}

} // namespace heritage::physics
