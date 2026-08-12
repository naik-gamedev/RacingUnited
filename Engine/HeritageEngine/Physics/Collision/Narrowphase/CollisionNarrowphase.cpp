// Narrowphase: primitive and static-triangle contact generation.

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

bool CollisionSystem::generateContact(
    ColliderHandle handleA,
    const Record& colliderA,
    const RigidBodySystem::Record& bodyA,
    ColliderHandle handleB,
    const Record& colliderB,
    const RigidBodySystem::Record& bodyB,
    CollisionContact& contact) const
{
    if (colliderA.shapeType == ColliderShapeType::Sphere
        && colliderB.shapeType == ColliderShapeType::Sphere)
    {
        return sphereSphereContact(
            handleA, colliderA, bodyA,
            handleB, colliderB, bodyB,
            contact);
    }

    if (colliderA.shapeType == ColliderShapeType::Sphere
        && colliderB.shapeType == ColliderShapeType::Box)
    {
        return sphereBoxContact(
            handleA, colliderA, bodyA,
            handleB, colliderB, bodyB,
            contact);
    }

    if (colliderA.shapeType == ColliderShapeType::Box
        && colliderB.shapeType == ColliderShapeType::Sphere)
    {
        CollisionContact swapped;
        if (!sphereBoxContact(
                handleB, colliderB, bodyB,
                handleA, colliderA, bodyA,
                swapped))
        {
            return false;
        }

        contact = swapped;
        std::swap(contact.colliderA, contact.colliderB);
        std::swap(contact.bodyA, contact.bodyB);
        contact.normal = scaleVector(contact.normal, -1.0f);
        return true;
    }

    return boxBoxContact(
        handleA, colliderA, bodyA,
        handleB, colliderB, bodyB,
        contact);
}
ColliderHandle CollisionSystem::staticTriangleColliderHandle(
    std::uint32_t triangleIndex)
{
    constexpr ColliderHandle marker = 0x8000000000000000ull;
    return marker | static_cast<ColliderHandle>(triangleIndex + 1u);
}
bool CollisionSystem::staticTriangleIndexFromColliderHandle(
    ColliderHandle handle,
    std::uint32_t& triangleIndex)
{
    constexpr ColliderHandle marker = 0x8000000000000000ull;
    if ((handle & marker) == 0u)
        return false;
    const ColliderHandle encoded = handle & ~marker;
    if (encoded == 0u
        || encoded > (std::numeric_limits<std::uint32_t>::max)())
    {
        return false;
    }
    triangleIndex = static_cast<std::uint32_t>(encoded - 1u);
    return true;
}
bool CollisionSystem::sphereStaticTriangleContact(
    ColliderHandle sphereHandle,
    const Record& sphere,
    const RigidBodySystem::Record& sphereBody,
    std::uint32_t triangleIndex,
    CollisionContact& contact) const
{
    if (triangleIndex >= m_staticSceneTriangles.size())
        return false;
    const StaticSceneTriangle& triangle =
        m_staticSceneTriangles[triangleIndex];
    const heritage::math::Vec3 center = worldCenter(sphere, sphereBody);
    const heritage::math::Vec3 trianglePoint = closestPointOnTriangle(
        center, triangle.a, triangle.b, triangle.c);
    const heritage::math::Vec3 towardTriangle = subtract(
        trianglePoint, center);
    const float distanceSquared = lengthSquared(towardTriangle);
    const float contactRadius = sphere.radius + kStaticTriangleContactSkin;
    if (distanceSquared > contactRadius * contactRadius)
        return false;

    const float distance = std::sqrt((std::max)(0.0f, distanceSquared));
    heritage::math::Vec3 normal;
    if (distance > kContactEpsilon)
    {
        normal = scaleVector(towardTriangle, 1.0f / distance);
    }
    else
    {
        heritage::math::Vec3 surfaceNormal = normalized(
            cross(
                subtract(triangle.b, triangle.a),
                subtract(triangle.c, triangle.a)),
            normalized(triangle.normal));
        if (dot(surfaceNormal, subtract(center, triangle.a)) < 0.0f)
            surfaceNormal = scaleVector(surfaceNormal, -1.0f);
        normal = scaleVector(surfaceNormal, -1.0f);
    }

    contact.colliderA = sphereHandle;
    contact.colliderB = staticTriangleColliderHandle(triangleIndex);
    contact.bodyA = sphere.body;
    contact.bodyB = InvalidBody;
    contact.point = trianglePoint;
    contact.normal = normal;
    contact.penetration = (std::max)(0.0f, sphere.radius - distance);
    contact.trigger = false;
    return true;
}
bool CollisionSystem::boxStaticTriangleContact(
    ColliderHandle boxHandle,
    const Record& box,
    const RigidBodySystem::Record& boxBody,
    std::uint32_t triangleIndex,
    CollisionContact& contact) const
{
    if (triangleIndex >= m_staticSceneTriangles.size())
        return false;
    const StaticSceneTriangle& triangle =
        m_staticSceneTriangles[triangleIndex];
    const heritage::math::Vec3 center = worldCenter(box, boxBody);
    const RigidBodySystem::Quaternion inverseRotation =
        conjugateRotation(boxBody.rotation);
    const heritage::math::Vec3 localTriangle[3]{
        rotateVector(inverseRotation, subtract(triangle.a, center)),
        rotateVector(inverseRotation, subtract(triangle.b, center)),
        rotateVector(inverseRotation, subtract(triangle.c, center))
    };
    const heritage::math::Vec3 edges[3]{
        subtract(localTriangle[1], localTriangle[0]),
        subtract(localTriangle[2], localTriangle[1]),
        subtract(localTriangle[0], localTriangle[2])
    };

    const auto overlapsOnAxis = [&](const heritage::math::Vec3& axis) {
        const float axisLength = length(axis);
        if (axisLength <= kSatEpsilon)
            return true;
        const float radius =
            box.halfExtents.x * std::abs(axis.x)
            + box.halfExtents.y * std::abs(axis.y)
            + box.halfExtents.z * std::abs(axis.z)
            + kStaticTriangleContactSkin * axisLength;
        const float projection0 = dot(localTriangle[0], axis);
        const float projection1 = dot(localTriangle[1], axis);
        const float projection2 = dot(localTriangle[2], axis);
        const float minimum = (std::min)({
            projection0, projection1, projection2 });
        const float maximum = (std::max)({
            projection0, projection1, projection2 });
        return minimum <= radius && maximum >= -radius;
    };

    if (!overlapsOnAxis({ 1.0f, 0.0f, 0.0f })
        || !overlapsOnAxis({ 0.0f, 1.0f, 0.0f })
        || !overlapsOnAxis({ 0.0f, 0.0f, 1.0f }))
    {
        return false;
    }

    const heritage::math::Vec3 localTriangleNormal = cross(
        edges[0], subtract(localTriangle[2], localTriangle[0]));
    if (lengthSquared(localTriangleNormal)
            <= kSatEpsilon * kSatEpsilon
        || !overlapsOnAxis(localTriangleNormal))
    {
        return false;
    }

    const heritage::math::Vec3 boxAxes[3]{
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    };
    for (const heritage::math::Vec3& edge : edges)
    {
        for (const heritage::math::Vec3& axis : boxAxes)
        {
            if (!overlapsOnAxis(cross(edge, axis)))
                return false;
        }
    }

    heritage::math::Vec3 surfaceNormal = normalized(
        cross(
            subtract(triangle.b, triangle.a),
            subtract(triangle.c, triangle.a)),
        normalized(triangle.normal));
    float centerPlaneDistance = dot(
        surfaceNormal,
        subtract(center, triangle.a));
    if (centerPlaneDistance < 0.0f)
    {
        surfaceNormal = scaleVector(surfaceNormal, -1.0f);
        centerPlaneDistance = -centerPlaneDistance;
    }

    const heritage::math::Vec3 worldAxes[3]{
        rotateVector(boxBody.rotation, { 1.0f, 0.0f, 0.0f }),
        rotateVector(boxBody.rotation, { 0.0f, 1.0f, 0.0f }),
        rotateVector(boxBody.rotation, { 0.0f, 0.0f, 1.0f })
    };
    const float projectedRadius =
        box.halfExtents.x * std::abs(dot(worldAxes[0], surfaceNormal))
        + box.halfExtents.y * std::abs(dot(worldAxes[1], surfaceNormal))
        + box.halfExtents.z * std::abs(dot(worldAxes[2], surfaceNormal));
    if (centerPlaneDistance
        > projectedRadius + kStaticTriangleContactSkin)
    {
        return false;
    }

    const heritage::math::Vec3 support = supportPoint(
        center,
        worldAxes,
        box.halfExtents,
        scaleVector(surfaceNormal, -1.0f));
    const heritage::math::Vec3 trianglePoint = closestPointOnTriangle(
        support, triangle.a, triangle.b, triangle.c);

    contact.colliderA = boxHandle;
    contact.colliderB = staticTriangleColliderHandle(triangleIndex);
    contact.bodyA = box.body;
    contact.bodyB = InvalidBody;
    contact.point = trianglePoint;
    contact.normal = scaleVector(surfaceNormal, -1.0f);
    contact.penetration = (std::max)(
        0.0f,
        projectedRadius - centerPlaneDistance);
    contact.trigger = false;
    return true;
}
bool CollisionSystem::generateStaticTriangleContact(
    ColliderHandle colliderHandle,
    const Record& collider,
    const RigidBodySystem::Record& body,
    std::uint32_t triangleIndex,
    CollisionContact& contact) const
{
    return collider.shapeType == ColliderShapeType::Sphere
        ? sphereStaticTriangleContact(
            colliderHandle, collider, body, triangleIndex, contact)
        : boxStaticTriangleContact(
            colliderHandle, collider, body, triangleIndex, contact);
}
bool CollisionSystem::sphereSphereContact(
    ColliderHandle handleA,
    const Record& colliderA,
    const RigidBodySystem::Record& bodyA,
    ColliderHandle handleB,
    const Record& colliderB,
    const RigidBodySystem::Record& bodyB,
    CollisionContact& contact) const
{
    const heritage::math::Vec3 centerA = worldCenter(colliderA, bodyA);
    const heritage::math::Vec3 centerB = worldCenter(colliderB, bodyB);
    const heritage::math::Vec3 delta = subtract(centerB, centerA);
    const float distanceSquared = lengthSquared(delta);
    const float combinedRadius = colliderA.radius + colliderB.radius;
    if (distanceSquared > combinedRadius * combinedRadius)
        return false;

    const float distance = std::sqrt((std::max)(0.0f, distanceSquared));
    const heritage::math::Vec3 normal = distance > kContactEpsilon
        ? scaleVector(delta, 1.0f / distance)
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const float penetration = combinedRadius - distance;

    contact.colliderA = handleA;
    contact.colliderB = handleB;
    contact.bodyA = colliderA.body;
    contact.bodyB = colliderB.body;
    contact.normal = normal;
    contact.penetration = (std::max)(0.0f, penetration);
    contact.point = add(
        centerA,
        scaleVector(normal, colliderA.radius - penetration * 0.5f));
    return true;
}
bool CollisionSystem::sphereBoxContact(
    ColliderHandle sphereHandle,
    const Record& sphere,
    const RigidBodySystem::Record& sphereBody,
    ColliderHandle boxHandle,
    const Record& box,
    const RigidBodySystem::Record& boxBody,
    CollisionContact& contact) const
{
    const heritage::math::Vec3 sphereCenter = worldCenter(sphere, sphereBody);
    const heritage::math::Vec3 boxCenter = worldCenter(box, boxBody);
    const RigidBodySystem::Quaternion inverseRotation = conjugateRotation(boxBody.rotation);
    const heritage::math::Vec3 sphereLocal = rotateVector(
        inverseRotation,
        subtract(sphereCenter, boxCenter));
    const heritage::math::Vec3 minimum = scaleVector(box.halfExtents, -1.0f);
    const heritage::math::Vec3 closestLocal = clampVector(
        sphereLocal,
        minimum,
        box.halfExtents);
    const heritage::math::Vec3 sphereToClosest = subtract(
        closestLocal,
        sphereLocal);
    const float distanceSquared = lengthSquared(sphereToClosest);
    if (distanceSquared > sphere.radius * sphere.radius)
        return false;

    heritage::math::Vec3 normalLocal{};
    heritage::math::Vec3 pointLocal = closestLocal;
    float penetration = 0.0f;

    if (distanceSquared > kContactEpsilon * kContactEpsilon)
    {
        const float distance = std::sqrt(distanceSquared);
        normalLocal = scaleVector(sphereToClosest, 1.0f / distance);
        penetration = sphere.radius - distance;
    }
    else
    {
        // Sphere centre lies inside or exactly on the box. Find the nearest
        // face and choose an inward A->B normal so positional correction moves
        // the sphere outward through that face.
        const float distanceToPositiveX = box.halfExtents.x - sphereLocal.x;
        const float distanceToNegativeX = box.halfExtents.x + sphereLocal.x;
        const float distanceToPositiveY = box.halfExtents.y - sphereLocal.y;
        const float distanceToNegativeY = box.halfExtents.y + sphereLocal.y;
        const float distanceToPositiveZ = box.halfExtents.z - sphereLocal.z;
        const float distanceToNegativeZ = box.halfExtents.z + sphereLocal.z;

        float nearest = distanceToPositiveX;
        normalLocal = { -1.0f, 0.0f, 0.0f };
        pointLocal = { box.halfExtents.x, sphereLocal.y, sphereLocal.z };

        if (distanceToNegativeX < nearest)
        {
            nearest = distanceToNegativeX;
            normalLocal = { 1.0f, 0.0f, 0.0f };
            pointLocal = { -box.halfExtents.x, sphereLocal.y, sphereLocal.z };
        }
        if (distanceToPositiveY < nearest)
        {
            nearest = distanceToPositiveY;
            normalLocal = { 0.0f, -1.0f, 0.0f };
            pointLocal = { sphereLocal.x, box.halfExtents.y, sphereLocal.z };
        }
        if (distanceToNegativeY < nearest)
        {
            nearest = distanceToNegativeY;
            normalLocal = { 0.0f, 1.0f, 0.0f };
            pointLocal = { sphereLocal.x, -box.halfExtents.y, sphereLocal.z };
        }
        if (distanceToPositiveZ < nearest)
        {
            nearest = distanceToPositiveZ;
            normalLocal = { 0.0f, 0.0f, -1.0f };
            pointLocal = { sphereLocal.x, sphereLocal.y, box.halfExtents.z };
        }
        if (distanceToNegativeZ < nearest)
        {
            nearest = distanceToNegativeZ;
            normalLocal = { 0.0f, 0.0f, 1.0f };
            pointLocal = { sphereLocal.x, sphereLocal.y, -box.halfExtents.z };
        }

        penetration = sphere.radius + (std::max)(0.0f, nearest);
    }

    contact.colliderA = sphereHandle;
    contact.colliderB = boxHandle;
    contact.bodyA = sphere.body;
    contact.bodyB = box.body;
    contact.normal = normalized(rotateVector(boxBody.rotation, normalLocal));
    contact.point = add(boxCenter, rotateVector(boxBody.rotation, pointLocal));
    contact.penetration = (std::max)(0.0f, penetration);
    return true;
}
bool CollisionSystem::boxBoxContact(
    ColliderHandle handleA,
    const Record& colliderA,
    const RigidBodySystem::Record& bodyA,
    ColliderHandle handleB,
    const Record& colliderB,
    const RigidBodySystem::Record& bodyB,
    CollisionContact& contact) const
{
    const heritage::math::Vec3 centerA = worldCenter(colliderA, bodyA);
    const heritage::math::Vec3 centerB = worldCenter(colliderB, bodyB);
    const heritage::math::Vec3 centerDelta = subtract(centerB, centerA);

    const heritage::math::Vec3 axesA[3] = {
        normalized(rotateVector(bodyA.rotation, { 1.0f, 0.0f, 0.0f })),
        normalized(rotateVector(bodyA.rotation, { 0.0f, 1.0f, 0.0f })),
        normalized(rotateVector(bodyA.rotation, { 0.0f, 0.0f, 1.0f }))
    };
    const heritage::math::Vec3 axesB[3] = {
        normalized(rotateVector(bodyB.rotation, { 1.0f, 0.0f, 0.0f })),
        normalized(rotateVector(bodyB.rotation, { 0.0f, 1.0f, 0.0f })),
        normalized(rotateVector(bodyB.rotation, { 0.0f, 0.0f, 1.0f }))
    };

    float rotation[3][3]{};
    float absoluteRotation[3][3]{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            rotation[i][j] = dot(axesA[i], axesB[j]);
            absoluteRotation[i][j] = std::abs(rotation[i][j]) + kSatEpsilon;
        }
    }

    const float translationA[3] = {
        dot(centerDelta, axesA[0]),
        dot(centerDelta, axesA[1]),
        dot(centerDelta, axesA[2])
    };

    float minimumPenetration = (std::numeric_limits<float>::max)();
    heritage::math::Vec3 minimumAxis{ 1.0f, 0.0f, 0.0f };

    const auto testAxis = [&](
        const heritage::math::Vec3& axis,
        float distance,
        float radiusA,
        float radiusB,
        float axisLength = 1.0f) -> bool
    {
        const float overlap = radiusA + radiusB - std::abs(distance);
        if (overlap < 0.0f)
            return false;
        if (axisLength <= kContactEpsilon)
            return true;

        const float penetration = overlap / axisLength;
        if (penetration < minimumPenetration)
        {
            heritage::math::Vec3 candidate = scaleVector(axis, 1.0f / axisLength);
            if (dot(centerDelta, candidate) < 0.0f)
                candidate = scaleVector(candidate, -1.0f);
            minimumPenetration = penetration;
            minimumAxis = candidate;
        }
        return true;
    };

    // The three face normals from box A.
    for (int i = 0; i < 3; ++i)
    {
        const float radiusA = component(colliderA.halfExtents, i);
        const float radiusB =
            colliderB.halfExtents.x * absoluteRotation[i][0]
            + colliderB.halfExtents.y * absoluteRotation[i][1]
            + colliderB.halfExtents.z * absoluteRotation[i][2];
        if (!testAxis(axesA[i], translationA[i], radiusA, radiusB))
            return false;
    }

    // The three face normals from box B.
    for (int j = 0; j < 3; ++j)
    {
        const float radiusA =
            colliderA.halfExtents.x * absoluteRotation[0][j]
            + colliderA.halfExtents.y * absoluteRotation[1][j]
            + colliderA.halfExtents.z * absoluteRotation[2][j];
        const float radiusB = component(colliderB.halfExtents, j);
        const float distance =
            translationA[0] * rotation[0][j]
            + translationA[1] * rotation[1][j]
            + translationA[2] * rotation[2][j];
        if (!testAxis(axesB[j], distance, radiusA, radiusB))
            return false;
    }

    // Nine edge-cross-edge axes. Near-parallel edges produce an axis too small
    // to be numerically useful and are already covered by face normals.
    for (int i = 0; i < 3; ++i)
    {
        const int i1 = (i + 1) % 3;
        const int i2 = (i + 2) % 3;
        for (int j = 0; j < 3; ++j)
        {
            const int j1 = (j + 1) % 3;
            const int j2 = (j + 2) % 3;
            const heritage::math::Vec3 axis = cross(axesA[i], axesB[j]);
            const float axisLength = length(axis);
            if (axisLength <= kSatEpsilon)
                continue;

            const float radiusA =
                component(colliderA.halfExtents, i1) * absoluteRotation[i2][j]
                + component(colliderA.halfExtents, i2) * absoluteRotation[i1][j];
            const float radiusB =
                component(colliderB.halfExtents, j1) * absoluteRotation[i][j2]
                + component(colliderB.halfExtents, j2) * absoluteRotation[i][j1];
            const float distance = std::abs(
                translationA[i2] * rotation[i1][j]
                - translationA[i1] * rotation[i2][j]);
            if (!testAxis(axis, distance, radiusA, radiusB, axisLength))
                return false;
        }
    }

    if (!finiteFloat(minimumPenetration)
        || minimumPenetration == (std::numeric_limits<float>::max)())
    {
        return false;
    }

    const heritage::math::Vec3 pointA = supportPoint(
        centerA,
        axesA,
        colliderA.halfExtents,
        minimumAxis);
    const heritage::math::Vec3 pointB = supportPoint(
        centerB,
        axesB,
        colliderB.halfExtents,
        scaleVector(minimumAxis, -1.0f));

    contact.colliderA = handleA;
    contact.colliderB = handleB;
    contact.bodyA = colliderA.body;
    contact.bodyB = colliderB.body;
    contact.normal = minimumAxis;
    contact.penetration = (std::max)(0.0f, minimumPenetration);
    contact.point = scaleVector(add(pointA, pointB), 0.5f);
    return true;
}

} // namespace heritage::physics
