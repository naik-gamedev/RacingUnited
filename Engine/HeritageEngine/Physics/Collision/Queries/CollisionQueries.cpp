// Collision queries: raycasts, sphere casts and overlap queries.

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

bool CollisionSystem::queryAllows(
    const Record& collider,
    const CollisionQueryFilter& filter) const
{
    if (filter.ignoredBody != InvalidBody
        && collider.body == filter.ignoredBody)
    {
        return false;
    }
    if (!filter.includeTriggers && collider.trigger)
        return false;
    return (collider.layer & filter.layerMask) != 0u;
}
bool CollisionSystem::raySphere(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord,
    RaycastHit& hit) const
{
    const heritage::math::Vec3 center = worldCenter(collider, bodyRecord);
    const heritage::math::Vec3 offset = subtract(origin, center);
    const float b = dot(offset, normalizedDirection);
    const float c = dot(offset, offset) - collider.radius * collider.radius;

    float distance = 0.0f;
    if (c > 0.0f)
    {
        if (b > 0.0f)
            return false;
        const float discriminant = b * b - c;
        if (discriminant < 0.0f)
            return false;
        distance = -b - std::sqrt((std::max)(0.0f, discriminant));
        if (distance < 0.0f)
            distance = 0.0f;
    }

    if (distance > maximumDistance)
        return false;

    hit.point = add(origin, scaleVector(normalizedDirection, distance));
    hit.normal = normalized(
        subtract(hit.point, center),
        scaleVector(normalizedDirection, -1.0f));
    hit.distance = distance;
    hit.fraction = maximumDistance > kContactEpsilon
        ? distance / maximumDistance
        : 0.0f;
    return true;
}
bool CollisionSystem::rayBox(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord,
    RaycastHit& hit) const
{
    const heritage::math::Vec3 center = worldCenter(collider, bodyRecord);
    const RigidBodySystem::Quaternion inverseRotation =
        conjugateRotation(bodyRecord.rotation);
    const heritage::math::Vec3 localOrigin = rotateVector(
        inverseRotation,
        subtract(origin, center));
    const heritage::math::Vec3 localDirection = rotateVector(
        inverseRotation,
        normalizedDirection);

    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    int hitAxis = -1;
    float hitSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float originComponent = component(localOrigin, axis);
        const float directionComponent = component(localDirection, axis);
        const float extent = component(collider.halfExtents, axis);

        if (std::abs(directionComponent) <= kContactEpsilon)
        {
            if (originComponent < -extent || originComponent > extent)
                return false;
            continue;
        }

        float first = (-extent - originComponent) / directionComponent;
        float second = (extent - originComponent) / directionComponent;
        float nearSign = directionComponent > 0.0f ? -1.0f : 1.0f;
        if (first > second)
            std::swap(first, second);

        if (first > nearDistance)
        {
            nearDistance = first;
            hitAxis = axis;
            hitSign = nearSign;
        }
        farDistance = (std::min)(farDistance, second);
        if (nearDistance > farDistance)
            return false;
    }

    if (farDistance < 0.0f || nearDistance > maximumDistance)
        return false;

    const bool startedInside = hitAxis < 0 || nearDistance <= 0.0f;
    const float distance = startedInside ? 0.0f : nearDistance;
    hit.point = add(origin, scaleVector(normalizedDirection, distance));

    if (startedInside)
    {
        hit.normal = scaleVector(normalizedDirection, -1.0f);
    }
    else
    {
        heritage::math::Vec3 localNormal{ 0.0f, 0.0f, 0.0f };
        if (hitAxis == 0) localNormal.x = hitSign;
        else if (hitAxis == 1) localNormal.y = hitSign;
        else localNormal.z = hitSign;
        hit.normal = normalized(rotateVector(bodyRecord.rotation, localNormal));
    }

    hit.distance = distance;
    hit.fraction = maximumDistance > kContactEpsilon
        ? distance / maximumDistance
        : 0.0f;
    return true;
}
bool CollisionSystem::rayStaticSceneTriangle(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    const StaticSceneTriangle& triangle,
    RaycastHit& hit) const
{
    const heritage::math::Vec3 edge1 = subtract(triangle.b, triangle.a);
    const heritage::math::Vec3 edge2 = subtract(triangle.c, triangle.a);
    const heritage::math::Vec3 p = cross(normalizedDirection, edge2);
    const float determinant = dot(edge1, p);
    if (std::abs(determinant) <= kContactEpsilon)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const heritage::math::Vec3 t = subtract(origin, triangle.a);
    const float u = dot(t, p) * inverseDeterminant;
    if (u < -kContactEpsilon || u > 1.0f + kContactEpsilon)
        return false;

    const heritage::math::Vec3 q = cross(t, edge1);
    const float v = dot(normalizedDirection, q) * inverseDeterminant;
    if (v < -kContactEpsilon || u + v > 1.0f + kContactEpsilon)
        return false;

    const float distance = dot(edge2, q) * inverseDeterminant;
    if (distance < -kContactEpsilon || distance > maximumDistance + kContactEpsilon)
        return false;

    hit.distance = (std::max)(0.0f, distance);
    hit.fraction = maximumDistance > kContactEpsilon
        ? hit.distance / maximumDistance
        : 0.0f;
    hit.point = add(origin, scaleVector(normalizedDirection, hit.distance));
    hit.normal = normalized(triangle.normal, { 0.0f, 1.0f, 0.0f });
    // Query triangles are two-sided. Always return a normal opposing the ray so
    // a suspension ray cast downward receives an upward road normal even when
    // creator mesh winding is inconsistent.
    if (dot(hit.normal, normalizedDirection) > 0.0f)
        hit.normal = scaleVector(hit.normal, -1.0f);
    hit.collider = InvalidCollider;
    hit.body = InvalidBody;
    hit.surfaceMaterial = triangle.surfaceMaterial;
    hit.surfaceWetness = triangle.surfaceWetness;
    hit.surfaceProperties = triangle.surfaceProperties;
    hit.trigger = false;
    return true;
}
void CollisionSystem::queryStaticSceneTriangles(
    const Aabb& bounds,
    std::vector<std::uint32_t>& triangleIndices,
    std::size_t& nodeTestCount) const
{
    m_staticTriangleBvh.queryAabb(
        bounds.minimum,
        bounds.maximum,
        triangleIndices,
        nodeTestCount);
}
bool CollisionSystem::raycastStaticSceneOnly(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    RaycastHit& hit,
    std::uint32_t& triangleIndex,
    std::size_t& nodeTestCount) const
{
    hit = {};
    triangleIndex = (std::numeric_limits<std::uint32_t>::max)();
    nodeTestCount = 0;
    if (m_staticTriangleBvh.empty()
        || maximumDistance <= kContactEpsilon)
    {
        return false;
    }

    const heritage::math::Vec3 end = add(
        origin,
        scaleVector(normalizedDirection, maximumDistance));
    const Aabb rayBounds{
        {
            (std::min)(origin.x, end.x),
            (std::min)(origin.y, end.y),
            (std::min)(origin.z, end.z)
        },
        {
            (std::max)(origin.x, end.x),
            (std::max)(origin.y, end.y),
            (std::max)(origin.z, end.z)
        }
    };

    std::vector<std::uint32_t> candidates;
    queryStaticSceneTriangles(rayBounds, candidates, nodeTestCount);
    bool found = false;
    RaycastHit closest;
    closest.distance = maximumDistance;
    for (const std::uint32_t candidateIndex : candidates)
    {
        RaycastHit candidate;
        if (!rayStaticSceneTriangle(
                origin,
                normalizedDirection,
                maximumDistance,
                m_staticSceneTriangles[candidateIndex],
                candidate))
        {
            continue;
        }

        if (!found
            || candidate.distance < closest.distance - kContactEpsilon
            || (std::abs(candidate.distance - closest.distance)
                    <= kContactEpsilon
                && candidateIndex < triangleIndex))
        {
            found = true;
            closest = candidate;
            triangleIndex = candidateIndex;
        }
    }

    if (found)
        hit = closest;
    return found;
}
bool CollisionSystem::raycast(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& direction,
    float maximumDistance,
    const CollisionQueryFilter& filter,
    const RigidBodySystem& bodies,
    RaycastHit& hit) const
{
    m_lastQueryCandidateCount = 0;
    m_lastQueryExactTestCount = 0;
    m_lastRaycastDiagnostics = {};
    hit = {};

    if (!finiteVec3(origin) || !finiteVec3(direction)
        || !finiteFloat(maximumDistance)
        || maximumDistance < 0.0f
        || maximumDistance > kMaximumShapeExtent)
    {
        setError("Physics.Raycast requires finite values and a distance between 0 and 1,000,000 metres.");
        return false;
    }

    const float directionLength = length(direction);
    if (directionLength <= kContactEpsilon)
    {
        setError("Physics.Raycast direction must not be zero.");
        return false;
    }

    const heritage::math::Vec3 rayDirection = scaleVector(
        direction,
        1.0f / directionLength);
    const heritage::math::Vec3 end = add(
        origin,
        scaleVector(rayDirection, maximumDistance));
    const Aabb rayBounds{
        {
            (std::min)(origin.x, end.x),
            (std::min)(origin.y, end.y),
            (std::min)(origin.z, end.z)
        },
        {
            (std::max)(origin.x, end.x),
            (std::max)(origin.y, end.y),
            (std::max)(origin.z, end.z)
        }
    };
    m_lastRaycastDiagnostics.staticSceneLoaded =
        m_staticSceneBoundsValid;
    if (m_staticSceneBoundsValid)
    {
        m_lastRaycastDiagnostics.originInsideStaticSceneHorizontalBounds =
            origin.x >= m_staticSceneBounds.minimum.x - kContactEpsilon
            && origin.x <= m_staticSceneBounds.maximum.x + kContactEpsilon
            && origin.z >= m_staticSceneBounds.minimum.z - kContactEpsilon
            && origin.z <= m_staticSceneBounds.maximum.z + kContactEpsilon;
        m_lastRaycastDiagnostics.rayBoundsOverlapStaticScene =
            aabbOverlap(rayBounds, m_staticSceneBounds);
    }

    bool found = false;
    RaycastHit closest;
    closest.distance = maximumDistance;

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive || !queryAllows(slot.record, filter))
            continue;

        const RigidBodySystem::Slot* bodySlot = bodies.resolve(slot.record.body);
        if (!bodySlot)
            continue;
        if (!aabbOverlap(rayBounds, worldAabb(slot.record, bodySlot->record)))
            continue;

        ++m_lastQueryCandidateCount;
        ++m_lastQueryExactTestCount;
        ++m_lastRaycastDiagnostics.colliderCandidateCount;
        ++m_lastRaycastDiagnostics.exactTestCount;
        RaycastHit candidate;
        const bool candidateHit = slot.record.shapeType == ColliderShapeType::Sphere
            ? raySphere(
                origin, rayDirection, maximumDistance,
                slot.record, bodySlot->record, candidate)
            : rayBox(
                origin, rayDirection, maximumDistance,
                slot.record, bodySlot->record, candidate);
        if (!candidateHit)
            continue;

        candidate.collider = makeHandle(index, slot.generation);
        candidate.body = slot.record.body;
        candidate.surfaceMaterial = slot.record.surfaceMaterial;
        candidate.surfaceWetness = slot.record.surfaceWetness;
        candidate.surfaceProperties = defaultSurfaceMaterialProperties(slot.record.surfaceMaterial);
        candidate.trigger = slot.record.trigger;

        if (!found
            || candidate.distance < closest.distance - kContactEpsilon
            || (std::abs(candidate.distance - closest.distance) <= kContactEpsilon
                && candidate.collider < closest.collider))
        {
            closest = candidate;
            found = true;
        }
    }

    // Creator/world static triangles participate in this read-only query path.
    // CollisionSystem::simulate() separately consumes the same BVH-backed world
    // for dynamic primitive-vs-static-triangle rigid-body contacts.
    if ((filter.layerMask & 1u) != 0u && !m_staticSceneTriangles.empty())
    {
        std::vector<std::uint32_t> triangleIndices;
        std::size_t nodeTestCount = 0;
        queryStaticSceneTriangles(
            rayBounds,
            triangleIndices,
            nodeTestCount);
        m_lastRaycastDiagnostics.staticBvhNodeTestCount = nodeTestCount;
        std::uint32_t closestTriangleIndex =
            (std::numeric_limits<std::uint32_t>::max)();
        for (const std::uint32_t triangleIndex : triangleIndices)
        {
            ++m_lastQueryCandidateCount;
            ++m_lastQueryExactTestCount;
            ++m_lastRaycastDiagnostics.staticTriangleCandidateCount;
            ++m_lastRaycastDiagnostics.exactTestCount;
            RaycastHit candidate;
            if (!rayStaticSceneTriangle(
                    origin,
                    rayDirection,
                    maximumDistance,
                    m_staticSceneTriangles[triangleIndex],
                    candidate))
            {
                continue;
            }
            m_lastRaycastDiagnostics.staticTriangleHit = true;

            if (!found
                || candidate.distance < closest.distance - kContactEpsilon
                || (std::abs(candidate.distance - closest.distance)
                        <= kContactEpsilon
                    && closest.collider == InvalidCollider
                    && closest.body == InvalidBody
                    && triangleIndex < closestTriangleIndex))
            {
                closest = candidate;
                closestTriangleIndex = triangleIndex;
                found = true;
            }
        }
    }

    if (found)
    {
        hit = closest;
        m_lastRaycastDiagnostics.selectedHitWasStaticTriangle =
            closest.collider == InvalidCollider
            && closest.body == InvalidBody;
    }
    clearError();
    return found;
}
bool CollisionSystem::raycastAny(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& direction,
    float maximumDistance,
    const CollisionQueryFilter& filter,
    const RigidBodySystem& bodies) const
{
    RaycastHit hit;
    return raycast(origin, direction, maximumDistance, filter, bodies, hit);
}
bool CollisionSystem::sphereCastSphere(
    const heritage::math::Vec3& origin,
    float castRadius,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord,
    SphereCastHit& hit) const
{
    const heritage::math::Vec3 targetCenter = worldCenter(collider, bodyRecord);
    const float combinedRadius = castRadius + collider.radius;
    const heritage::math::Vec3 offset = subtract(origin, targetCenter);
    const float b = dot(offset, normalizedDirection);
    const float c = dot(offset, offset) - combinedRadius * combinedRadius;

    float distance = 0.0f;
    if (c > 0.0f)
    {
        if (b > 0.0f)
            return false;
        const float discriminant = b * b - c;
        if (discriminant < 0.0f)
            return false;
        distance = -b - std::sqrt((std::max)(0.0f, discriminant));
        if (distance < 0.0f)
            distance = 0.0f;
    }

    if (distance > maximumDistance)
        return false;

    const heritage::math::Vec3 sphereCenterAtHit = add(
        origin,
        scaleVector(normalizedDirection, distance));
    hit.normal = normalized(
        subtract(sphereCenterAtHit, targetCenter),
        scaleVector(normalizedDirection, -1.0f));
    hit.point = subtract(
        sphereCenterAtHit,
        scaleVector(hit.normal, castRadius));
    hit.distance = distance;
    hit.fraction = maximumDistance > kContactEpsilon
        ? distance / maximumDistance
        : 0.0f;
    return true;
}
bool CollisionSystem::sphereCastBox(
    const heritage::math::Vec3& origin,
    float castRadius,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord,
    SphereCastHit& hit) const
{
    const heritage::math::Vec3 center = worldCenter(collider, bodyRecord);
    const RigidBodySystem::Quaternion inverseRotation =
        conjugateRotation(bodyRecord.rotation);
    const heritage::math::Vec3 localOrigin = rotateVector(
        inverseRotation,
        subtract(origin, center));
    const heritage::math::Vec3 localDirection = rotateVector(
        inverseRotation,
        normalizedDirection);
    const heritage::math::Vec3 expandedExtents = add(
        collider.halfExtents,
        { castRadius, castRadius, castRadius });

    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    int hitAxis = -1;
    float hitSign = 0.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float originComponent = component(localOrigin, axis);
        const float directionComponent = component(localDirection, axis);
        const float extent = component(expandedExtents, axis);

        if (std::abs(directionComponent) <= kContactEpsilon)
        {
            if (originComponent < -extent || originComponent > extent)
                return false;
            continue;
        }

        float first = (-extent - originComponent) / directionComponent;
        float second = (extent - originComponent) / directionComponent;
        float nearSign = directionComponent > 0.0f ? -1.0f : 1.0f;
        if (first > second)
            std::swap(first, second);

        if (first > nearDistance)
        {
            nearDistance = first;
            hitAxis = axis;
            hitSign = nearSign;
        }
        farDistance = (std::min)(farDistance, second);
        if (nearDistance > farDistance)
            return false;
    }

    if (farDistance < 0.0f || nearDistance > maximumDistance)
        return false;

    const bool startedInside = hitAxis < 0 || nearDistance <= 0.0f;
    const float distance = startedInside ? 0.0f : nearDistance;
    const heritage::math::Vec3 sphereCenterAtHit = add(
        origin,
        scaleVector(normalizedDirection, distance));

    if (startedInside)
    {
        hit.normal = scaleVector(normalizedDirection, -1.0f);
    }
    else
    {
        heritage::math::Vec3 localNormal{ 0.0f, 0.0f, 0.0f };
        if (hitAxis == 0) localNormal.x = hitSign;
        else if (hitAxis == 1) localNormal.y = hitSign;
        else localNormal.z = hitSign;
        hit.normal = normalized(rotateVector(bodyRecord.rotation, localNormal));
    }

    hit.point = subtract(
        sphereCenterAtHit,
        scaleVector(hit.normal, castRadius));
    hit.distance = distance;
    hit.fraction = maximumDistance > kContactEpsilon
        ? distance / maximumDistance
        : 0.0f;
    return true;
}
bool CollisionSystem::sphereCastCollider(
    const heritage::math::Vec3& origin,
    float castRadius,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    ColliderHandle colliderHandle,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord,
    SphereCastHit& hit) const
{
    SphereCastHit candidate;
    const bool found = collider.shapeType == ColliderShapeType::Sphere
        ? sphereCastSphere(
            origin,
            castRadius,
            normalizedDirection,
            maximumDistance,
            collider,
            bodyRecord,
            candidate)
        : sphereCastBox(
            origin,
            castRadius,
            normalizedDirection,
            maximumDistance,
            collider,
            bodyRecord,
            candidate);
    if (!found)
        return false;

    candidate.collider = colliderHandle;
    candidate.body = collider.body;
    candidate.surfaceMaterial = collider.surfaceMaterial;
    candidate.surfaceWetness = collider.surfaceWetness;
    candidate.surfaceProperties = defaultSurfaceMaterialProperties(collider.surfaceMaterial);
    candidate.trigger = collider.trigger;
    hit = candidate;
    return true;
}
bool CollisionSystem::sphereCastStaticSceneTriangle(
    const heritage::math::Vec3& origin,
    float castRadius,
    const heritage::math::Vec3& normalizedDirection,
    float maximumDistance,
    std::uint32_t triangleIndex,
    SphereCastHit& hit) const
{
    if (triangleIndex >= m_staticSceneTriangles.size())
        return false;

    const StaticSceneTriangle& triangle = m_staticSceneTriangles[triangleIndex];

    // Distance to a fixed triangle is a 1-Lipschitz function of the moving
    // sphere centre. Advancing by the current surface separation therefore
    // cannot step past the first contact. This gives us a robust, inexpensive
    // swept-sphere test without tessellating the camera path or inflating the
    // authored triangle mesh.
    constexpr int kMaximumAdvanceIterations = 64;
    constexpr float kSweepTolerance = 1.0e-4f;
    float travel = 0.0f;

    for (int iteration = 0; iteration < kMaximumAdvanceIterations; ++iteration)
    {
        const heritage::math::Vec3 center = add(
            origin,
            scaleVector(normalizedDirection, travel));
        const heritage::math::Vec3 trianglePoint = closestPointOnTriangle(
            center,
            triangle.a,
            triangle.b,
            triangle.c);
        const heritage::math::Vec3 fromSurface = subtract(center, trianglePoint);
        const float centreDistance = std::sqrt((std::max)(
            0.0f,
            lengthSquared(fromSurface)));
        const float separation = centreDistance - castRadius;

        if (separation <= kSweepTolerance)
        {
            heritage::math::Vec3 normal;
            if (centreDistance > kContactEpsilon)
            {
                normal = scaleVector(fromSurface, 1.0f / centreDistance);
            }
            else
            {
                normal = normalized(
                    triangle.normal,
                    { 0.0f, 1.0f, 0.0f });
                if (dot(normal, normalizedDirection) > 0.0f)
                    normal = scaleVector(normal, -1.0f);
            }

            hit.collider = staticTriangleColliderHandle(triangleIndex);
            hit.body = InvalidBody;
            hit.distance = travel;
            hit.fraction = maximumDistance > kContactEpsilon
                ? travel / maximumDistance
                : 0.0f;
            hit.normal = normal;
            // SphereCastHit::point is defined on the swept sphere surface.
            hit.point = subtract(center, scaleVector(normal, castRadius));
            hit.surfaceMaterial = triangle.surfaceMaterial;
            hit.surfaceWetness = triangle.surfaceWetness;
            hit.surfaceProperties = triangle.surfaceProperties;
            hit.trigger = false;
            return true;
        }

        // The exact separation is a conservative safe step. Keep a tiny floor
        // only after the tolerance test so numerical noise cannot stall forever.
        const float advance = (std::max)(separation, kSweepTolerance);
        travel += advance;
        if (travel > maximumDistance + kSweepTolerance)
            return false;
    }

    return false;
}
bool CollisionSystem::sphereCast(
    const heritage::math::Vec3& origin,
    float radius,
    const heritage::math::Vec3& direction,
    float maximumDistance,
    const CollisionQueryFilter& filter,
    const RigidBodySystem& bodies,
    SphereCastHit& hit) const
{
    m_lastQueryCandidateCount = 0;
    m_lastQueryExactTestCount = 0;
    hit = {};

    if (!finiteVec3(origin)
        || !finiteFloat(radius)
        || radius < 0.0f
        || radius > kMaximumShapeExtent
        || !finiteVec3(direction)
        || !finiteFloat(maximumDistance)
        || maximumDistance < 0.0f
        || maximumDistance > kMaximumShapeExtent)
    {
        setError("Physics.SphereCast requires finite values, radius 0..1,000,000 metres, and distance 0..1,000,000 metres.");
        return false;
    }

    const float directionLength = length(direction);
    if (directionLength <= kContactEpsilon)
    {
        setError("Physics.SphereCast direction must not be zero.");
        return false;
    }

    const heritage::math::Vec3 castDirection = scaleVector(
        direction,
        1.0f / directionLength);
    const heritage::math::Vec3 end = add(
        origin,
        scaleVector(castDirection, maximumDistance));
    const heritage::math::Vec3 radiusExtents{ radius, radius, radius };
    const Aabb castBounds{
        subtract(
            {
                (std::min)(origin.x, end.x),
                (std::min)(origin.y, end.y),
                (std::min)(origin.z, end.z)
            },
            radiusExtents),
        add(
            {
                (std::max)(origin.x, end.x),
                (std::max)(origin.y, end.y),
                (std::max)(origin.z, end.z)
            },
            radiusExtents)
    };

    bool found = false;
    SphereCastHit closest;
    closest.distance = maximumDistance;

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive || !queryAllows(slot.record, filter))
            continue;

        const RigidBodySystem::Slot* bodySlot = bodies.resolve(slot.record.body);
        if (!bodySlot)
            continue;
        if (!aabbOverlap(castBounds, worldAabb(slot.record, bodySlot->record)))
            continue;

        ++m_lastQueryCandidateCount;
        ++m_lastQueryExactTestCount;
        SphereCastHit candidate;
        if (!sphereCastCollider(
                origin,
                radius,
                castDirection,
                maximumDistance,
                makeHandle(index, slot.generation),
                slot.record,
                bodySlot->record,
                candidate))
        {
            continue;
        }

        if (!found
            || candidate.distance < closest.distance - kContactEpsilon
            || (std::abs(candidate.distance - closest.distance) <= kContactEpsilon
                && candidate.collider < closest.collider))
        {
            closest = candidate;
            found = true;
        }
    }

    // Static creator-authored triangle scenes participate in swept-sphere
    // queries too. This is important for camera collision: the visible/driveable
    // terrain is usually a Scene_*.glb triangle scene rather than primitive boxes.
    if ((filter.layerMask & 1u) != 0u && !m_staticSceneTriangles.empty())
    {
        std::vector<std::uint32_t> triangleIndices;
        std::size_t nodeTestCount = 0;
        queryStaticSceneTriangles(castBounds, triangleIndices, nodeTestCount);
        (void)nodeTestCount;

        for (const std::uint32_t triangleIndex : triangleIndices)
        {
            ++m_lastQueryCandidateCount;
            ++m_lastQueryExactTestCount;
            SphereCastHit candidate;
            if (!sphereCastStaticSceneTriangle(
                    origin,
                    radius,
                    castDirection,
                    maximumDistance,
                    triangleIndex,
                    candidate))
            {
                continue;
            }

            if (!found
                || candidate.distance < closest.distance - kContactEpsilon
                || (std::abs(candidate.distance - closest.distance)
                        <= kContactEpsilon
                    && candidate.collider < closest.collider))
            {
                closest = candidate;
                found = true;
            }
        }
    }

    if (found)
        hit = closest;
    clearError();
    return found;
}
bool CollisionSystem::sphereCastAny(
    const heritage::math::Vec3& origin,
    float radius,
    const heritage::math::Vec3& direction,
    float maximumDistance,
    const CollisionQueryFilter& filter,
    const RigidBodySystem& bodies) const
{
    SphereCastHit hit;
    return sphereCast(
        origin,
        radius,
        direction,
        maximumDistance,
        filter,
        bodies,
        hit);
}
bool CollisionSystem::sphereOverlapsCollider(
    const heritage::math::Vec3& center,
    float radius,
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord) const
{
    const heritage::math::Vec3 colliderCenter = worldCenter(collider, bodyRecord);
    if (collider.shapeType == ColliderShapeType::Sphere)
    {
        const float combinedRadius = radius + collider.radius;
        return lengthSquared(subtract(center, colliderCenter))
            <= combinedRadius * combinedRadius;
    }

    const heritage::math::Vec3 localCenter = rotateVector(
        conjugateRotation(bodyRecord.rotation),
        subtract(center, colliderCenter));
    const heritage::math::Vec3 closest = clampVector(
        localCenter,
        scaleVector(collider.halfExtents, -1.0f),
        collider.halfExtents);
    return lengthSquared(subtract(localCenter, closest)) <= radius * radius;
}
std::size_t CollisionSystem::overlapSphereCount(
    const heritage::math::Vec3& center,
    float radius,
    const CollisionQueryFilter& filter,
    const RigidBodySystem& bodies) const
{
    m_lastQueryCandidateCount = 0;
    m_lastQueryExactTestCount = 0;

    if (!finiteVec3(center) || !finiteFloat(radius)
        || radius < 0.0f || radius > kMaximumShapeExtent)
    {
        setError("Physics.OverlapSphereCount requires a finite radius between 0 and 1,000,000 metres.");
        return 0;
    }

    const heritage::math::Vec3 extents{ radius, radius, radius };
    const Aabb queryBounds{ subtract(center, extents), add(center, extents) };
    std::size_t result = 0;

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive || !queryAllows(slot.record, filter))
            continue;

        const RigidBodySystem::Slot* bodySlot = bodies.resolve(slot.record.body);
        if (!bodySlot)
            continue;
        if (!aabbOverlap(queryBounds, worldAabb(slot.record, bodySlot->record)))
            continue;

        ++m_lastQueryCandidateCount;
        ++m_lastQueryExactTestCount;
        if (sphereOverlapsCollider(center, radius, slot.record, bodySlot->record))
            ++result;
    }

    clearError();
    return result;
}

} // namespace heritage::physics
