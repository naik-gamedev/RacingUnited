#include "CollisionSystem.hpp"
#include "Collision/CollisionInternal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace heritage::physics {
using namespace collision_detail;

// CLEAN04B core collision coordinator.
// Owns collider/static-scene lifetime, fixed-step orchestration, handles/error
// plumbing and common geometry access. Broadphase, queries, narrowphase, solver,
// CCD and island mechanics are compiled from Collision/ subdirectories.

std::size_t CollisionSystem::ContactPairKeyHash::operator()(
    const ContactPairKey& value) const
{
    const std::size_t firstHash = std::hash<ColliderHandle>{}(value.first);
    const std::size_t secondHash = std::hash<ColliderHandle>{}(value.second);
    return firstHash ^ (secondHash + 0x9e3779b97f4a7c15ull
        + (firstHash << 6u) + (firstHash >> 2u));
}
RigidBodySystem::Quaternion CollisionSystem::conjugateRotation(
    const RigidBodySystem::Quaternion& value)
{
    return heritage::math::conjugate(value);
}
heritage::math::Vec3 CollisionSystem::rotateVector(
    const RigidBodySystem::Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    return heritage::math::rotateVectorUnit(rotation, value);
}
bool CollisionSystem::aabbOverlap(
    const Aabb& left,
    const Aabb& right)
{
    return left.minimum.x <= right.maximum.x
        && left.maximum.x >= right.minimum.x
        && left.minimum.y <= right.maximum.y
        && left.maximum.y >= right.minimum.y
        && left.minimum.z <= right.maximum.z
        && left.maximum.z >= right.minimum.z;
}
float CollisionSystem::inverseMassForContact(
    const RigidBodySystem::Record& body)
{
    return body.motionType == BodyMotionType::Dynamic
        ? body.inverseMass
        : 0.0f;
}
heritage::math::Vec3 CollisionSystem::angularVelocityRadians(
    const RigidBodySystem::Record& body)
{
    constexpr float degreesToRadians = kPi / 180.0f;
    return scaleVector(body.angularVelocityDegrees, degreesToRadians);
}
heritage::math::Vec3 CollisionSystem::pointVelocity(
    const RigidBodySystem::Record& body,
    const heritage::math::Vec3& worldPoint)
{
    const heritage::math::Vec3 leverArm = subtract(
        worldPoint,
        RigidBodySystem::worldCenterOfMass(body));
    return add(
        body.linearVelocity,
        cross(angularVelocityRadians(body), leverArm));
}
heritage::math::Vec3 CollisionSystem::supportPoint(
    const heritage::math::Vec3& center,
    const heritage::math::Vec3 axes[3],
    const heritage::math::Vec3& halfExtents,
    const heritage::math::Vec3& direction)
{
    heritage::math::Vec3 result = center;
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex)
    {
        const float extent = component(halfExtents, axisIndex);
        const float projection = dot(axes[axisIndex], direction);
        float sign = 0.0f;
        if (projection > kContactEpsilon)
            sign = 1.0f;
        else if (projection < -kContactEpsilon)
            sign = -1.0f;
        result = add(result, scaleVector(axes[axisIndex], extent * sign));
    }
    return result;
}
void CollisionSystem::clear()
{
    m_slots.clear();
    m_staticSceneTriangles.clear();
    m_staticTriangleBvh.clear();
    m_staticSceneBounds = {};
    m_staticSceneBoundsValid = false;
    m_freeIndices.clear();
    m_contacts.clear();
    m_aliveCount = 0;
    m_broadphaseCandidateCount = 0;
    m_narrowphaseTestCount = 0;
    m_resolvedContactCount = 0;
    m_simulationIslandCount = 0;
    m_activeIslandCount = 0;
    m_sleepingIslandCount = 0;
    m_warmStartedContactCount = 0;
    m_staticBroadphaseNodeTestCount = 0;
    m_staticTriangleCandidateCount = 0;
    m_staticTriangleNarrowphaseTestCount = 0;
    m_staticTriangleContactCount = 0;
    m_continuousCollisionBodyCount = 0;
    m_continuousCollisionSweepCount = 0;
    m_continuousCollisionHitCount = 0;
    m_continuousCollisionClampedBodyCount = 0;
    m_continuousCollisionUnsupportedBodyCount = 0;
    m_lastQueryCandidateCount = 0;
    m_lastQueryExactTestCount = 0;
    m_lastRaycastDiagnostics = {};
    m_simulationSequence = 0;
    m_contactCache.clear();
    ++m_topologyRevision;
    if (m_topologyRevision == 0)
        m_topologyRevision = 1;
    m_cachedTopologyRevision = 0;
    m_cachedBodyMassRevision = 0;
    m_lastError.clear();
}
void CollisionSystem::setStaticSceneTriangles(
    std::vector<StaticSceneTriangle> triangles)
{
    m_staticSceneTriangles = std::move(triangles);
    m_staticSceneBounds = {};
    std::vector<StaticTriangleBvhPrimitive> primitives;
    primitives.reserve(m_staticSceneTriangles.size());
    for (std::uint32_t triangleIndex = 0;
         triangleIndex < static_cast<std::uint32_t>(
             m_staticSceneTriangles.size());
         ++triangleIndex)
    {
        const StaticSceneTriangle& triangle =
            m_staticSceneTriangles[triangleIndex];
        StaticTriangleBvhPrimitive primitive;
        primitive.minimum = {
            (std::min)({ triangle.a.x, triangle.b.x, triangle.c.x }),
            (std::min)({ triangle.a.y, triangle.b.y, triangle.c.y }),
            (std::min)({ triangle.a.z, triangle.b.z, triangle.c.z })
        };
        primitive.maximum = {
            (std::max)({ triangle.a.x, triangle.b.x, triangle.c.x }),
            (std::max)({ triangle.a.y, triangle.b.y, triangle.c.y }),
            (std::max)({ triangle.a.z, triangle.b.z, triangle.c.z })
        };
        primitive.centroid = scaleVector(
            add(add(triangle.a, triangle.b), triangle.c),
            1.0f / 3.0f);
        primitive.triangleIndex = triangleIndex;
        primitives.push_back(primitive);
    }
    m_staticTriangleBvh.build(std::move(primitives));
    m_staticSceneBoundsValid = m_staticTriangleBvh.bounds(
        m_staticSceneBounds.minimum,
        m_staticSceneBounds.maximum);
    clearError();
}
void CollisionSystem::rebaseLocalOrigin(
    const heritage::math::Vec3& shift)
{
    if (m_staticSceneTriangles.empty())
    {
        m_contacts.clear();
        m_contactCache.clear();
        m_lastRaycastDiagnostics = {};
        clearError();
        return;
    }

    std::vector<StaticSceneTriangle> rebased = std::move(m_staticSceneTriangles);
    for (StaticSceneTriangle& triangle : rebased)
    {
        triangle.a.x -= shift.x;
        triangle.a.y -= shift.y;
        triangle.a.z -= shift.z;
        triangle.b.x -= shift.x;
        triangle.b.y -= shift.y;
        triangle.b.z -= shift.z;
        triangle.c.x -= shift.x;
        triangle.c.y -= shift.y;
        triangle.c.z -= shift.z;
    }

    // Cached world-space contact points are invalid in the new frame. The next
    // fixed step reconstructs them deterministically from unchanged geometry.
    m_contacts.clear();
    m_contactCache.clear();
    m_lastRaycastDiagnostics = {};
    setStaticSceneTriangles(std::move(rebased));
}
void CollisionSystem::clearStaticSceneTriangles()
{
    m_staticSceneTriangles.clear();
    m_staticTriangleBvh.clear();
    m_staticSceneBounds = {};
    m_staticSceneBoundsValid = false;
    clearError();
}
ColliderHandle CollisionSystem::create(
    const ColliderDescription& description,
    const RigidBodySystem& bodies)
{
    if (!bodies.resolve(description.body))
    {
        setError("Physics collider creation requires a valid rigid-body handle.");
        return InvalidCollider;
    }

    if (!finiteVec3(description.localPosition))
    {
        setError("Physics collider local position must contain finite values.");
        return InvalidCollider;
    }

    if (description.shapeType == ColliderShapeType::Sphere)
    {
        if (!finiteFloat(description.radius)
            || description.radius < kMinimumRadius
            || description.radius > kMaximumShapeExtent)
        {
            setError("Physics sphere radius must be between 0.0001 and 1,000,000 metres.");
            return InvalidCollider;
        }
    }
    else
    {
        if (!finiteVec3(description.halfExtents)
            || description.halfExtents.x < kMinimumRadius
            || description.halfExtents.y < kMinimumRadius
            || description.halfExtents.z < kMinimumRadius
            || description.halfExtents.x > kMaximumShapeExtent
            || description.halfExtents.y > kMaximumShapeExtent
            || description.halfExtents.z > kMaximumShapeExtent)
        {
            setError("Physics box half extents must be between 0.0001 and 1,000,000 metres.");
            return InvalidCollider;
        }
    }

    if (!finiteFloat(description.friction)
        || description.friction < 0.0f
        || description.friction > kMaximumMaterialValue
        || !finiteFloat(description.restitution)
        || description.restitution < 0.0f
        || description.restitution > 1.0f)
    {
        setError("Physics collider friction must be 0..10 and restitution must be 0..1.");
        return InvalidCollider;
    }

    if (!finiteFloat(description.surfaceWetness)
        || description.surfaceWetness < 0.0f
        || description.surfaceWetness > 1.0f)
    {
        setError("Physics collider surface wetness must be between 0 and 1.");
        return InvalidCollider;
    }

    const int surfaceValue = static_cast<int>(description.surfaceMaterial);
    if (surfaceValue < static_cast<int>(SurfaceMaterial::Default)
        || surfaceValue > static_cast<int>(SurfaceMaterial::DeepSnow))
    {
        setError("Physics collider surface material is unknown.");
        return InvalidCollider;
    }

    if (description.layer == 0u)
    {
        setError("Physics collider layer must contain at least one bit.");
        return InvalidCollider;
    }

    std::uint32_t index = 0;
    if (!m_freeIndices.empty())
    {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    }
    else
    {
        if (m_slots.size() >= static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)() - 1u))
        {
            setError("Collider storage exhausted its handle index space.");
            return InvalidCollider;
        }
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }

    Slot& slot = m_slots[index];
    slot.alive = true;
    slot.record = {};
    slot.record.body = description.body;
    slot.record.shapeType = description.shapeType;
    slot.record.localPosition = description.localPosition;
    slot.record.halfExtents = description.halfExtents;
    slot.record.radius = description.radius;
    slot.record.friction = description.friction;
    slot.record.restitution = description.restitution;
    slot.record.surfaceMaterial = description.surfaceMaterial;
    slot.record.surfaceWetness = description.surfaceWetness;
    slot.record.trigger = description.trigger;
    slot.record.layer = description.layer;
    slot.record.mask = description.mask;

    ++m_aliveCount;
    ++m_topologyRevision;
    if (m_topologyRevision == 0)
        m_topologyRevision = 1;
    clearError();
    return makeHandle(index, slot.generation);
}
ColliderHandle CollisionSystem::createSphere(
    BodyHandle body,
    float radius,
    const heritage::math::Vec3& localPosition,
    float friction,
    float restitution,
    bool trigger,
    const RigidBodySystem& bodies)
{
    ColliderDescription description;
    description.body = body;
    description.shapeType = ColliderShapeType::Sphere;
    description.radius = radius;
    description.localPosition = localPosition;
    description.friction = friction;
    description.restitution = restitution;
    description.trigger = trigger;
    return create(description, bodies);
}
ColliderHandle CollisionSystem::createBox(
    BodyHandle body,
    const heritage::math::Vec3& halfExtents,
    const heritage::math::Vec3& localPosition,
    float friction,
    float restitution,
    bool trigger,
    const RigidBodySystem& bodies)
{
    ColliderDescription description;
    description.body = body;
    description.shapeType = ColliderShapeType::Box;
    description.halfExtents = halfExtents;
    description.localPosition = localPosition;
    description.friction = friction;
    description.restitution = restitution;
    description.trigger = trigger;
    return create(description, bodies);
}
bool CollisionSystem::destroy(ColliderHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        setError("Physics.DestroyCollider received an invalid or stale collider handle.");
        return false;
    }

    Slot& slot = m_slots[index];
    if (!slot.alive || slot.generation != generation)
    {
        setError("Physics.DestroyCollider received an invalid or stale collider handle.");
        return false;
    }

    return destroyResolved(index, slot);
}
bool CollisionSystem::exists(ColliderHandle handle) const
{
    const bool result = resolve(handle) != nullptr;
    if (result)
        clearError();
    return result;
}
std::size_t CollisionSystem::countForBody(BodyHandle bodyHandle) const
{
    std::size_t result = 0;
    for (const Slot& slot : m_slots)
    {
        if (slot.alive && slot.record.body == bodyHandle)
            ++result;
    }
    return result;
}
bool CollisionSystem::staticSceneTriangle(
    std::uint32_t triangleIndex,
    StaticSceneTriangle& triangle) const
{
    if (triangleIndex >= m_staticSceneTriangles.size())
        return false;
    triangle = m_staticSceneTriangles[triangleIndex];
    return true;
}

void CollisionSystem::nearbyStaticSceneTriangles(
    const heritage::math::Vec3& center,
    float halfExtent,
    std::size_t maximumTriangles,
    std::vector<StaticSceneTriangle>& triangles) const
{
    triangles.clear();
    if (maximumTriangles == 0 || halfExtent <= 0.0f || m_staticTriangleBvh.empty())
        return;

    const Aabb bounds{
        { center.x - halfExtent, center.y - halfExtent, center.z - halfExtent },
        { center.x + halfExtent, center.y + halfExtent, center.z + halfExtent }
    };
    std::vector<std::uint32_t> indices;
    std::size_t nodeTests = 0;
    queryStaticSceneTriangles(bounds, indices, nodeTests);
    (void)nodeTests;

    // TIRE22/VIS14: rank by the true closest point on each finite triangle.
    // Vertex/centroid ranking can discard a huge road/kerb/sidewalk triangle
    // that passes directly through the tire simply because all of its vertices
    // are far away. The visual deformation cache must reflect geometric
    // proximity to the triangle surface, not proximity to its authored corners.
    const auto distanceScore = [&center](const StaticSceneTriangle& tri)
    {
        const auto sub = [](const heritage::math::Vec3& a, const heritage::math::Vec3& b)
        {
            return heritage::math::Vec3{ a.x - b.x, a.y - b.y, a.z - b.z };
        };
        const auto addScaled = [](
            const heritage::math::Vec3& a,
            const heritage::math::Vec3& b,
            float scale)
        {
            return heritage::math::Vec3{
                a.x + b.x * scale,
                a.y + b.y * scale,
                a.z + b.z * scale };
        };
        const auto dot3 = [](const heritage::math::Vec3& a, const heritage::math::Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        };
        const heritage::math::Vec3 ab = sub(tri.b, tri.a);
        const heritage::math::Vec3 ac = sub(tri.c, tri.a);
        const heritage::math::Vec3 ap = sub(center, tri.a);
        const float d1 = dot3(ab, ap);
        const float d2 = dot3(ac, ap);
        heritage::math::Vec3 closest = tri.a;
        if (!(d1 <= 0.0f && d2 <= 0.0f))
        {
            const heritage::math::Vec3 bp = sub(center, tri.b);
            const float d3 = dot3(ab, bp);
            const float d4 = dot3(ac, bp);
            if (d3 >= 0.0f && d4 <= d3)
            {
                closest = tri.b;
            }
            else
            {
                const float vc = d1 * d4 - d3 * d2;
                if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
                {
                    const float v = d1 / (d1 - d3);
                    closest = addScaled(tri.a, ab, v);
                }
                else
                {
                    const heritage::math::Vec3 cp = sub(center, tri.c);
                    const float d5 = dot3(ab, cp);
                    const float d6 = dot3(ac, cp);
                    if (d6 >= 0.0f && d5 <= d6)
                    {
                        closest = tri.c;
                    }
                    else
                    {
                        const float vb = d5 * d2 - d1 * d6;
                        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
                        {
                            const float w = d2 / (d2 - d6);
                            closest = addScaled(tri.a, ac, w);
                        }
                        else
                        {
                            const float va = d3 * d6 - d5 * d4;
                            if (va <= 0.0f && (d4 - d3) >= 0.0f
                                && (d5 - d6) >= 0.0f)
                            {
                                const heritage::math::Vec3 bc = sub(tri.c, tri.b);
                                const float w = (d4 - d3)
                                    / ((d4 - d3) + (d5 - d6));
                                closest = addScaled(tri.b, bc, w);
                            }
                            else
                            {
                                const float denominator = va + vb + vc;
                                if (std::abs(denominator) > 1.0e-12f)
                                {
                                    const float inverse = 1.0f / denominator;
                                    const float v = vb * inverse;
                                    const float w = vc * inverse;
                                    closest = addScaled(
                                        addScaled(tri.a, ab, v), ac, w);
                                }
                            }
                        }
                    }
                }
            }
        }
        const heritage::math::Vec3 delta = sub(closest, center);
        return dot3(delta, delta);
    };

    std::stable_sort(indices.begin(), indices.end(),
        [this, &distanceScore](std::uint32_t lhs, std::uint32_t rhs)
        {
            return distanceScore(m_staticSceneTriangles[lhs])
                < distanceScore(m_staticSceneTriangles[rhs]);
        });
    const std::size_t count = (std::min)(maximumTriangles, indices.size());
    triangles.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        triangles.push_back(m_staticSceneTriangles[indices[i]]);
}

void CollisionSystem::destroyForBody(BodyHandle bodyHandle)
{
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        Slot& slot = m_slots[index];
        if (slot.alive && slot.record.body == bodyHandle)
            destroyResolved(index, slot);
    }

    m_contacts.erase(
        std::remove_if(
            m_contacts.begin(),
            m_contacts.end(),
            [bodyHandle](const CollisionContact& contact) {
                return contact.bodyA == bodyHandle || contact.bodyB == bodyHandle;
            }),
        m_contacts.end());
    clearError();
}
void CollisionSystem::removeInvalidBodies(const RigidBodySystem& bodies)
{
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        Slot& slot = m_slots[index];
        if (slot.alive && !bodies.resolve(slot.record.body))
            destroyResolved(index, slot);
    }
}
bool CollisionSystem::shapeType(
    ColliderHandle handle,
    ColliderShapeType& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetColliderShape received an invalid or stale collider handle.");
        return false;
    }
    value = slot->record.shapeType;
    clearError();
    return true;
}
BodyHandle CollisionSystem::body(ColliderHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetColliderBody received an invalid or stale collider handle.");
        return InvalidBody;
    }
    clearError();
    return slot->record.body;
}
bool CollisionSystem::setMaterial(
    ColliderHandle handle,
    float friction,
    float restitution)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetColliderMaterial received an invalid or stale collider handle.");
        return false;
    }
    if (!finiteFloat(friction)
        || friction < 0.0f
        || friction > kMaximumMaterialValue
        || !finiteFloat(restitution)
        || restitution < 0.0f
        || restitution > 1.0f)
    {
        setError("Physics.SetColliderMaterial requires friction 0..10 and restitution 0..1.");
        return false;
    }

    slot->record.friction = friction;
    slot->record.restitution = restitution;
    clearError();
    return true;
}
bool CollisionSystem::setSurface(
    ColliderHandle handle,
    SurfaceMaterial material,
    float wetness)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetColliderSurface received an invalid or stale collider handle.");
        return false;
    }

    const int materialValue = static_cast<int>(material);
    if (materialValue < static_cast<int>(SurfaceMaterial::Default)
        || materialValue > static_cast<int>(SurfaceMaterial::DeepSnow)
        || !finiteFloat(wetness)
        || wetness < 0.0f
        || wetness > 1.0f)
    {
        setError("Physics.SetColliderSurface requires a known material and wetness from 0 to 1.");
        return false;
    }

    slot->record.surfaceMaterial = material;
    slot->record.surfaceWetness = wetness;
    clearError();
    return true;
}
bool CollisionSystem::surface(
    ColliderHandle handle,
    SurfaceMaterial& material,
    float& wetness) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetColliderSurface received an invalid or stale collider handle.");
        return false;
    }

    material = slot->record.surfaceMaterial;
    wetness = slot->record.surfaceWetness;
    clearError();
    return true;
}
bool CollisionSystem::setTrigger(ColliderHandle handle, bool trigger)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetColliderTrigger received an invalid or stale collider handle.");
        return false;
    }
    slot->record.trigger = trigger;
    clearError();
    return true;
}
bool CollisionSystem::setFilter(
    ColliderHandle handle,
    std::uint32_t layer,
    std::uint32_t mask)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetColliderFilter received an invalid or stale collider handle.");
        return false;
    }
    if (layer == 0u)
    {
        setError("Physics.SetColliderFilter layer must contain at least one bit.");
        return false;
    }
    slot->record.layer = layer;
    slot->record.mask = mask;
    clearError();
    return true;
}
void CollisionSystem::simulate(
    RigidBodySystem& bodies,
    float fixedDeltaTime)
{
    removeInvalidBodies(bodies);
    rebuildMassProperties(bodies);
    ++m_simulationSequence;
    if (m_simulationSequence == 0)
        m_simulationSequence = 1;

    m_contacts.clear();
    m_broadphaseCandidateCount = 0;
    m_narrowphaseTestCount = 0;
    m_resolvedContactCount = 0;
    m_simulationIslandCount = 0;
    m_activeIslandCount = 0;
    m_sleepingIslandCount = 0;
    m_warmStartedContactCount = 0;
    m_staticBroadphaseNodeTestCount = 0;
    m_staticTriangleCandidateCount = 0;
    m_staticTriangleNarrowphaseTestCount = 0;
    m_staticTriangleContactCount = 0;
    m_continuousCollisionBodyCount = 0;
    m_continuousCollisionSweepCount = 0;
    m_continuousCollisionHitCount = 0;
    m_continuousCollisionClampedBodyCount = 0;
    m_continuousCollisionUnsupportedBodyCount = 0;

    applyContinuousCollisionDetection(bodies, fixedDeltaTime);

    collectBroadphaseContacts(bodies);

    // Contacts define connected dynamic-body islands. Before solving, an awake
    // body or moving kinematic body wakes every dynamic body in the same island.
    updateSimulationIslandsAndSleeping(bodies, fixedDeltaTime, false);

    for (CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t staticTriangleIndex = 0;
        if (staticTriangleIndexFromColliderHandle(
                contact.colliderB,
                staticTriangleIndex))
        {
            RigidBodySystem::Slot* bodySlot = bodies.resolve(contact.bodyA);
            if (!bodySlot
                || bodySlot->record.motionType != BodyMotionType::Dynamic
                || bodySlot->record.sleeping)
            {
                continue;
            }
            resolveStaticPosition(bodySlot->record, contact);
            ++m_resolvedContactCount;
            continue;
        }

        RigidBodySystem::Slot* bodySlotA = bodies.resolve(contact.bodyA);
        RigidBodySystem::Slot* bodySlotB = bodies.resolve(contact.bodyB);
        if (!bodySlotA || !bodySlotB)
            continue;

        const bool activeA = bodySlotA->record.motionType == BodyMotionType::Dynamic
            && !bodySlotA->record.sleeping;
        const bool activeB = bodySlotB->record.motionType == BodyMotionType::Dynamic
            && !bodySlotB->record.sleeping;
        if (!activeA && !activeB)
            continue;

        resolvePosition(bodySlotA->record, bodySlotB->record, contact);
        ++m_resolvedContactCount;
    }

    // Reuse the previous fixed step's coherent impulses. This gives resting
    // contacts a useful initial solution instead of starting from zero.
    for (CollisionContact& contact : m_contacts)
    {
        if (contact.trigger
            || (contact.accumulatedNormalImpulse <= 0.0f
                && std::abs(contact.accumulatedTangentImpulse) <= kContactEpsilon))
        {
            continue;
        }

        std::uint32_t staticTriangleIndex = 0;
        if (staticTriangleIndexFromColliderHandle(
                contact.colliderB,
                staticTriangleIndex))
        {
            RigidBodySystem::Slot* bodySlot = bodies.resolve(contact.bodyA);
            if (!bodySlot
                || bodySlot->record.motionType != BodyMotionType::Dynamic
                || bodySlot->record.sleeping)
            {
                continue;
            }
            warmStartStaticContact(bodySlot->record, contact);
            continue;
        }

        RigidBodySystem::Slot* bodySlotA = bodies.resolve(contact.bodyA);
        RigidBodySystem::Slot* bodySlotB = bodies.resolve(contact.bodyB);
        if (!bodySlotA || !bodySlotB)
            continue;

        const bool activeA = bodySlotA->record.motionType == BodyMotionType::Dynamic
            && !bodySlotA->record.sleeping;
        const bool activeB = bodySlotB->record.motionType == BodyMotionType::Dynamic
            && !bodySlotB->record.sleeping;
        if (!activeA && !activeB)
            continue;

        warmStartContact(bodySlotA->record, bodySlotB->record, contact);
    }

    for (int iteration = 0; iteration < m_velocitySolverIterations; ++iteration)
    {
        for (CollisionContact& contact : m_contacts)
        {
            if (contact.trigger)
                continue;

            std::uint32_t staticTriangleIndex = 0;
            if (staticTriangleIndexFromColliderHandle(
                    contact.colliderB,
                    staticTriangleIndex))
            {
                const Slot* colliderSlot = resolve(contact.colliderA);
                RigidBodySystem::Slot* bodySlot = bodies.resolve(
                    contact.bodyA);
                if (!colliderSlot
                    || !bodySlot
                    || staticTriangleIndex >= m_staticSceneTriangles.size()
                    || bodySlot->record.motionType
                        != BodyMotionType::Dynamic
                    || bodySlot->record.sleeping)
                {
                    continue;
                }
                resolveStaticVelocity(
                    colliderSlot->record,
                    m_staticSceneTriangles[staticTriangleIndex],
                    bodySlot->record,
                    contact);
                continue;
            }

            const Slot* colliderSlotA = resolve(contact.colliderA);
            const Slot* colliderSlotB = resolve(contact.colliderB);
            RigidBodySystem::Slot* bodySlotA = bodies.resolve(contact.bodyA);
            RigidBodySystem::Slot* bodySlotB = bodies.resolve(contact.bodyB);
            if (!colliderSlotA || !colliderSlotB || !bodySlotA || !bodySlotB)
                continue;

            const bool activeA = bodySlotA->record.motionType == BodyMotionType::Dynamic
                && !bodySlotA->record.sleeping;
            const bool activeB = bodySlotB->record.motionType == BodyMotionType::Dynamic
                && !bodySlotB->record.sleeping;
            if (!activeA && !activeB)
                continue;

            resolveVelocity(
                colliderSlotA->record,
                bodySlotA->record,
                colliderSlotB->record,
                bodySlotB->record,
                contact);
        }
    }

    updateSimulationIslandsAndSleeping(bodies, fixedDeltaTime, true);
    persistContactCache();
    clearError();
}
std::size_t CollisionSystem::contactCountForBody(BodyHandle bodyHandle) const
{
    std::size_t result = 0;
    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.bodyA == bodyHandle || contact.bodyB == bodyHandle)
            ++result;
    }
    return result;
}
bool CollisionSystem::bodyTouching(BodyHandle bodyHandle) const
{
    return contactCountForBody(bodyHandle) > 0;
}
ColliderHandle CollisionSystem::makeHandle(
    std::uint32_t index,
    std::uint32_t generation)
{
    return (static_cast<ColliderHandle>(generation) << 32u)
        | static_cast<ColliderHandle>(index + 1u);
}
bool CollisionSystem::decodeHandle(
    ColliderHandle handle,
    std::uint32_t& index,
    std::uint32_t& generation)
{
    if (handle == InvalidCollider)
        return false;

    const std::uint32_t encodedIndex = static_cast<std::uint32_t>(
        handle & 0xffffffffull);
    generation = static_cast<std::uint32_t>(handle >> 32u);
    if (encodedIndex == 0u || generation == 0u)
        return false;

    index = encodedIndex - 1u;
    return true;
}
CollisionSystem::Slot* CollisionSystem::resolve(ColliderHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        return nullptr;
    }

    Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}
const CollisionSystem::Slot* CollisionSystem::resolve(ColliderHandle handle) const
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        return nullptr;
    }

    const Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}
bool CollisionSystem::destroyResolved(std::uint32_t index, Slot& slot)
{
    slot.alive = false;
    slot.record = {};
    ++slot.generation;
    if (slot.generation == 0u)
        slot.generation = 1u;
    m_freeIndices.push_back(index);
    if (m_aliveCount > 0)
        --m_aliveCount;
    ++m_topologyRevision;
    if (m_topologyRevision == 0)
        m_topologyRevision = 1;
    clearError();
    return true;
}
CollisionSystem::Aabb CollisionSystem::worldAabb(
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord) const
{
    const heritage::math::Vec3 center = worldCenter(collider, bodyRecord);
    heritage::math::Vec3 extent{};

    if (collider.shapeType == ColliderShapeType::Sphere)
    {
        extent = { collider.radius, collider.radius, collider.radius };
    }
    else
    {
        const RigidBodySystem::Quaternion& q = bodyRecord.rotation;
        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        const float m00 = 1.0f - 2.0f * (yy + zz);
        const float m01 = 2.0f * (xy - wz);
        const float m02 = 2.0f * (xz + wy);
        const float m10 = 2.0f * (xy + wz);
        const float m11 = 1.0f - 2.0f * (xx + zz);
        const float m12 = 2.0f * (yz - wx);
        const float m20 = 2.0f * (xz - wy);
        const float m21 = 2.0f * (yz + wx);
        const float m22 = 1.0f - 2.0f * (xx + yy);

        extent = {
            std::abs(m00) * collider.halfExtents.x
                + std::abs(m01) * collider.halfExtents.y
                + std::abs(m02) * collider.halfExtents.z,
            std::abs(m10) * collider.halfExtents.x
                + std::abs(m11) * collider.halfExtents.y
                + std::abs(m12) * collider.halfExtents.z,
            std::abs(m20) * collider.halfExtents.x
                + std::abs(m21) * collider.halfExtents.y
                + std::abs(m22) * collider.halfExtents.z
        };
    }

    return {
        subtract(center, extent),
        add(center, extent)
    };
}
heritage::math::Vec3 CollisionSystem::worldCenter(
    const Record& collider,
    const RigidBodySystem::Record& bodyRecord) const
{
    return add(
        bodyRecord.position,
        rotateVector(bodyRecord.rotation, collider.localPosition));
}
void CollisionSystem::setError(const std::string& message) const
{
    m_lastError = message;
}
void CollisionSystem::clearError() const
{
    m_lastError.clear();
}
const char* colliderShapeTypeName(ColliderShapeType value)
{
    switch (value)
    {
    case ColliderShapeType::Sphere:
        return "sphere";
    case ColliderShapeType::Box:
        return "box";
    default:
        return "unknown";
    }
}

const char* surfaceMaterialName(SurfaceMaterial value)
{
    switch (value)
    {
    case SurfaceMaterial::Default:
        return "default";
    case SurfaceMaterial::Asphalt:
        return "asphalt";
    case SurfaceMaterial::Gravel:
        return "gravel";
    case SurfaceMaterial::Dirt:
        return "dirt";
    case SurfaceMaterial::Grass:
        return "grass";
    case SurfaceMaterial::Snow:
        return "snow";
    case SurfaceMaterial::Ice:
        return "ice";
    case SurfaceMaterial::Mud:
        return "mud";
    case SurfaceMaterial::Sand:
        return "sand";
    case SurfaceMaterial::SoftSoil:
        return "soft_soil";
    case SurfaceMaterial::DeepSnow:
        return "deep_snow";
    case SurfaceMaterial::Kerb:
        return "kerb";
    case SurfaceMaterial::PaintedLine:
        return "painted_line";
    default:
        return "unknown";
    }
}

bool parseSurfaceMaterial(const std::string& text, SurfaceMaterial& value)
{
    std::string normalizedText;
    normalizedText.reserve(text.size());
    for (char character : text)
    {
        if (character == '-' || character == ' ')
            normalizedText.push_back('_');
        else
            normalizedText.push_back(static_cast<char>(
                std::tolower(static_cast<unsigned char>(character))));
    }

    if (normalizedText == "default")
        value = SurfaceMaterial::Default;
    else if (normalizedText == "asphalt" || normalizedText == "tarmac")
        value = SurfaceMaterial::Asphalt;
    else if (normalizedText == "gravel")
        value = SurfaceMaterial::Gravel;
    else if (normalizedText == "dirt" || normalizedText == "soil")
        value = SurfaceMaterial::Dirt;
    else if (normalizedText == "grass")
        value = SurfaceMaterial::Grass;
    else if (normalizedText == "snow")
        value = SurfaceMaterial::Snow;
    else if (normalizedText == "ice")
        value = SurfaceMaterial::Ice;
    else if (normalizedText == "mud")
        value = SurfaceMaterial::Mud;
    else if (normalizedText == "sand")
        value = SurfaceMaterial::Sand;
    else if (normalizedText == "soft_soil" || normalizedText == "softsoil")
        value = SurfaceMaterial::SoftSoil;
    else if (normalizedText == "deep_snow" || normalizedText == "deepsnow"
        || normalizedText == "powder_snow")
        value = SurfaceMaterial::DeepSnow;
    else if (normalizedText == "kerb" || normalizedText == "curb")
        value = SurfaceMaterial::Kerb;
    else if (normalizedText == "painted_line"
        || normalizedText == "paint"
        || normalizedText == "road_paint")
    {
        value = SurfaceMaterial::PaintedLine;
    }
    else
    {
        return false;
    }

    return true;
}

} // namespace heritage::physics
