// Solver: contact cache, warm starting, mass properties and impulse response.

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

CollisionSystem::ContactPairKey CollisionSystem::contactPairKey(
    const CollisionContact& contact)
{
    return { contact.colliderA, contact.colliderB };
}
void CollisionSystem::canonicalizeContact(CollisionContact& contact)
{
    if (contact.colliderA <= contact.colliderB)
        return;

    std::swap(contact.colliderA, contact.colliderB);
    std::swap(contact.bodyA, contact.bodyB);
    contact.normal = scaleVector(contact.normal, -1.0f);
    contact.tangent = scaleVector(contact.tangent, -1.0f);
}
void CollisionSystem::restoreCachedImpulse(CollisionContact& contact)
{
    const auto iterator = m_contactCache.find(contactPairKey(contact));
    if (iterator == m_contactCache.end())
        return;

    const CachedContact& cached = iterator->second;
    if (cached.lastSeenStep + 1u != m_simulationSequence)
        return;

    const heritage::math::Vec3 pointDelta = subtract(contact.point, cached.point);
    if (lengthSquared(pointDelta)
        > kWarmStartPointDistance * kWarmStartPointDistance)
    {
        return;
    }
    if (dot(contact.normal, cached.normal) < kWarmStartNormalDot)
        return;

    contact.tangent = cached.tangent;
    contact.accumulatedNormalImpulse = (std::max)(0.0f, cached.normalImpulse);
    contact.accumulatedTangentImpulse = cached.tangentImpulse;
    contact.warmStarted = contact.accumulatedNormalImpulse > kContactEpsilon
        || std::abs(contact.accumulatedTangentImpulse) > kContactEpsilon;
}
void CollisionSystem::warmStartContact(
    RigidBodySystem::Record& bodyA,
    RigidBodySystem::Record& bodyB,
    CollisionContact& contact)
{
    const heritage::math::Vec3 impulse = add(
        scaleVector(contact.normal, contact.accumulatedNormalImpulse),
        scaleVector(contact.tangent, contact.accumulatedTangentImpulse));
    if (lengthSquared(impulse) <= kContactEpsilon * kContactEpsilon)
        return;

    RigidBodySystem::applyImpulseToRecord(
        bodyA,
        scaleVector(impulse, -1.0f),
        contact.point);
    RigidBodySystem::applyImpulseToRecord(
        bodyB,
        impulse,
        contact.point);
    contact.warmStarted = true;
    ++m_warmStartedContactCount;
}
void CollisionSystem::persistContactCache()
{
    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        CachedContact& cached = m_contactCache[contactPairKey(contact)];
        cached.point = contact.point;
        cached.normal = contact.normal;
        cached.tangent = contact.tangent;
        cached.normalImpulse = contact.accumulatedNormalImpulse;
        cached.tangentImpulse = contact.accumulatedTangentImpulse;
        cached.lastSeenStep = m_simulationSequence;
    }

    for (auto iterator = m_contactCache.begin();
         iterator != m_contactCache.end();)
    {
        if (iterator->second.lastSeenStep + 2u < m_simulationSequence)
            iterator = m_contactCache.erase(iterator);
        else
            ++iterator;
    }
}
void CollisionSystem::rebuildMassProperties(RigidBodySystem& bodies)
{
    if (m_cachedTopologyRevision == m_topologyRevision
        && m_cachedBodyMassRevision == bodies.m_massPropertiesRevision)
    {
        return;
    }

    struct Accumulator
    {
        float totalVolume = 0.0f;
        heritage::math::Vec3 inertiaPerUnitMass{};
    };

    std::vector<Accumulator> accumulators(bodies.m_slots.size());
    for (std::uint32_t colliderIndex = 0;
         colliderIndex < static_cast<std::uint32_t>(m_slots.size());
         ++colliderIndex)
    {
        const Slot& colliderSlot = m_slots[colliderIndex];
        if (!colliderSlot.alive)
            continue;

        std::uint32_t bodyIndex = 0;
        std::uint32_t bodyGeneration = 0;
        if (!RigidBodySystem::decodeHandle(
                colliderSlot.record.body,
                bodyIndex,
                bodyGeneration)
            || bodyIndex >= bodies.m_slots.size())
        {
            continue;
        }

        const RigidBodySystem::Slot& bodySlot = bodies.m_slots[bodyIndex];
        if (!bodySlot.alive || bodySlot.generation != bodyGeneration)
            continue;

        const Record& collider = colliderSlot.record;
        float volume = 0.0f;
        heritage::math::Vec3 shapeInertiaPerUnitMass{};
        if (collider.shapeType == ColliderShapeType::Sphere)
        {
            volume = (4.0f / 3.0f) * kPi
                * collider.radius * collider.radius * collider.radius;
            const float inertia = 0.4f * collider.radius * collider.radius;
            shapeInertiaPerUnitMass = { inertia, inertia, inertia };
        }
        else
        {
            volume = 8.0f
                * collider.halfExtents.x
                * collider.halfExtents.y
                * collider.halfExtents.z;
            shapeInertiaPerUnitMass = {
                (collider.halfExtents.y * collider.halfExtents.y
                    + collider.halfExtents.z * collider.halfExtents.z) / 3.0f,
                (collider.halfExtents.x * collider.halfExtents.x
                    + collider.halfExtents.z * collider.halfExtents.z) / 3.0f,
                (collider.halfExtents.x * collider.halfExtents.x
                    + collider.halfExtents.y * collider.halfExtents.y) / 3.0f
            };
        }

        // Parallel-axis contribution is measured from the physical centre of
        // mass, not from the authored body/entity reference origin.
        const heritage::math::Vec3 offset = subtract(
            collider.localPosition,
            bodySlot.record.centerOfMassLocal);
        shapeInertiaPerUnitMass.x += offset.y * offset.y + offset.z * offset.z;
        shapeInertiaPerUnitMass.y += offset.x * offset.x + offset.z * offset.z;
        shapeInertiaPerUnitMass.z += offset.x * offset.x + offset.y * offset.y;

        Accumulator& accumulator = accumulators[bodyIndex];
        accumulator.totalVolume += volume;
        accumulator.inertiaPerUnitMass = add(
            accumulator.inertiaPerUnitMass,
            scaleVector(shapeInertiaPerUnitMass, volume));
    }

    for (std::uint32_t bodyIndex = 0;
         bodyIndex < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++bodyIndex)
    {
        RigidBodySystem::Slot& bodySlot = bodies.m_slots[bodyIndex];
        if (!bodySlot.alive)
            continue;

        RigidBodySystem::Record& body = bodySlot.record;
        if (body.motionType != BodyMotionType::Dynamic)
        {
            body.inverseInertiaLocal = {};
            continue;
        }

        // Explicit mass properties are authoritative. Collision geometry is
        // still used for contacts but must not silently rewrite a vehicle's
        // authored/estimated rotational inertia when wheels/bodywork change.
        if (body.hasInertiaLocalOverride)
        {
            const heritage::math::Vec3 inertia = body.inertiaLocalOverrideKgM2;
            body.inverseInertiaLocal = {
                inertia.x > kContactEpsilon ? 1.0f / inertia.x : 0.0f,
                inertia.y > kContactEpsilon ? 1.0f / inertia.y : 0.0f,
                inertia.z > kContactEpsilon ? 1.0f / inertia.z : 0.0f
            };
            continue;
        }

        const Accumulator& accumulator = accumulators[bodyIndex];
        if (accumulator.totalVolume <= kContactEpsilon)
        {
            body.inverseInertiaLocal = {
                body.inverseMass,
                body.inverseMass,
                body.inverseMass
            };
            continue;
        }

        const heritage::math::Vec3 inertiaPerUnitMass = scaleVector(
            accumulator.inertiaPerUnitMass,
            1.0f / accumulator.totalVolume);
        const heritage::math::Vec3 inertia = scaleVector(
            inertiaPerUnitMass,
            body.mass);
        body.inverseInertiaLocal = {
            inertia.x > kContactEpsilon ? 1.0f / inertia.x : 0.0f,
            inertia.y > kContactEpsilon ? 1.0f / inertia.y : 0.0f,
            inertia.z > kContactEpsilon ? 1.0f / inertia.z : 0.0f
        };
    }

    m_cachedTopologyRevision = m_topologyRevision;
    m_cachedBodyMassRevision = bodies.m_massPropertiesRevision;
}
void CollisionSystem::resolvePosition(
    RigidBodySystem::Record& bodyA,
    RigidBodySystem::Record& bodyB,
    const CollisionContact& contact)
{
    const float inverseMassA = inverseMassForContact(bodyA);
    const float inverseMassB = inverseMassForContact(bodyB);
    const float inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassSum <= 0.0f)
        return;

    const float correctionMagnitude =
        (std::max)(0.0f, contact.penetration - kPositionSlop)
        * kPositionCorrectionPercent
        / inverseMassSum;
    const heritage::math::Vec3 correction = scaleVector(
        contact.normal,
        correctionMagnitude);
    if (inverseMassA > 0.0f)
    {
        bodyA.position = subtract(
            bodyA.position,
            scaleVector(correction, inverseMassA));
    }
    if (inverseMassB > 0.0f)
    {
        bodyB.position = add(
            bodyB.position,
            scaleVector(correction, inverseMassB));
    }
}
void CollisionSystem::resolveStaticPosition(
    RigidBodySystem::Record& body,
    const CollisionContact& contact)
{
    if (body.motionType != BodyMotionType::Dynamic)
        return;
    const float correctionMagnitude = (std::max)(
        0.0f,
        contact.penetration - kPositionSlop)
        * kPositionCorrectionPercent;
    body.position = subtract(
        body.position,
        scaleVector(contact.normal, correctionMagnitude));
}
float CollisionSystem::staticEffectiveMassDenominator(
    const RigidBodySystem::Record& body,
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& direction) const
{
    float denominator = inverseMassForContact(body);
    const heritage::math::Vec3 lever = subtract(
        point,
        RigidBodySystem::worldCenterOfMass(body));
    const heritage::math::Vec3 angular = cross(
        RigidBodySystem::applyWorldInverseInertia(
            body,
            cross(lever, direction)),
        lever);
    denominator += dot(angular, direction);
    return denominator;
}
void CollisionSystem::warmStartStaticContact(
    RigidBodySystem::Record& body,
    CollisionContact& contact)
{
    const heritage::math::Vec3 impulse = add(
        scaleVector(contact.normal, contact.accumulatedNormalImpulse),
        scaleVector(contact.tangent, contact.accumulatedTangentImpulse));
    if (lengthSquared(impulse) <= kContactEpsilon * kContactEpsilon)
        return;
    RigidBodySystem::applyImpulseToRecord(
        body,
        scaleVector(impulse, -1.0f),
        contact.point);
    contact.warmStarted = true;
    ++m_warmStartedContactCount;
}
float CollisionSystem::effectiveMassDenominator(
    const RigidBodySystem::Record& bodyA,
    const RigidBodySystem::Record& bodyB,
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& direction) const
{
    float denominator = inverseMassForContact(bodyA)
        + inverseMassForContact(bodyB);

    const heritage::math::Vec3 leverA = subtract(
        point,
        RigidBodySystem::worldCenterOfMass(bodyA));
    const heritage::math::Vec3 leverB = subtract(
        point,
        RigidBodySystem::worldCenterOfMass(bodyB));
    const heritage::math::Vec3 angularA = cross(
        RigidBodySystem::applyWorldInverseInertia(
            bodyA,
            cross(leverA, direction)),
        leverA);
    const heritage::math::Vec3 angularB = cross(
        RigidBodySystem::applyWorldInverseInertia(
            bodyB,
            cross(leverB, direction)),
        leverB);
    denominator += dot(add(angularA, angularB), direction);
    return denominator;
}
void CollisionSystem::resolveVelocity(
    const Record& colliderA,
    RigidBodySystem::Record& bodyA,
    const Record& colliderB,
    RigidBodySystem::Record& bodyB,
    CollisionContact& contact)
{
    heritage::math::Vec3 velocityA = pointVelocity(bodyA, contact.point);
    heritage::math::Vec3 velocityB = pointVelocity(bodyB, contact.point);
    heritage::math::Vec3 relativeVelocity = subtract(velocityB, velocityA);
    const float velocityAlongNormal = dot(relativeVelocity, contact.normal);

    const float normalDenominator = effectiveMassDenominator(
        bodyA,
        bodyB,
        contact.point,
        contact.normal);
    if (normalDenominator > kContactEpsilon)
    {
        const float restitution =
            velocityAlongNormal < -kRestitutionVelocityThreshold
            ? (std::min)(colliderA.restitution, colliderB.restitution)
            : 0.0f;
        const float incrementalNormalImpulse =
            -(1.0f + restitution) * velocityAlongNormal
            / normalDenominator;
        const float previousNormalImpulse =
            contact.accumulatedNormalImpulse;
        contact.accumulatedNormalImpulse = (std::max)(
            0.0f,
            previousNormalImpulse + incrementalNormalImpulse);
        const float appliedNormalImpulse =
            contact.accumulatedNormalImpulse - previousNormalImpulse;

        if (std::abs(appliedNormalImpulse) > kContactEpsilon)
        {
            const heritage::math::Vec3 impulse = scaleVector(
                contact.normal,
                appliedNormalImpulse);
            RigidBodySystem::applyImpulseToRecord(
                bodyA,
                scaleVector(impulse, -1.0f),
                contact.point);
            RigidBodySystem::applyImpulseToRecord(
                bodyB,
                impulse,
                contact.point);
        }
    }

    velocityA = pointVelocity(bodyA, contact.point);
    velocityB = pointVelocity(bodyB, contact.point);
    relativeVelocity = subtract(velocityB, velocityA);

    heritage::math::Vec3 tangent = subtract(
        relativeVelocity,
        scaleVector(contact.normal, dot(relativeVelocity, contact.normal)));
    const float tangentLength = length(tangent);
    if (tangentLength > kContactEpsilon)
    {
        tangent = scaleVector(tangent, 1.0f / tangentLength);
        if (dot(tangent, contact.tangent) < 0.0f)
        {
            contact.tangent = scaleVector(tangent, -1.0f);
            contact.accumulatedTangentImpulse =
                -contact.accumulatedTangentImpulse;
        }
        else
        {
            contact.tangent = tangent;
        }
    }

    const float tangentDenominator = effectiveMassDenominator(
        bodyA,
        bodyB,
        contact.point,
        contact.tangent);
    if (tangentDenominator <= kContactEpsilon)
        return;

    const float incrementalTangentImpulse =
        -dot(relativeVelocity, contact.tangent)
        / tangentDenominator;
    const float friction = std::sqrt(
        colliderA.friction * colliderB.friction);
    const float maximumFrictionImpulse =
        contact.accumulatedNormalImpulse * friction;
    const float previousTangentImpulse =
        contact.accumulatedTangentImpulse;
    contact.accumulatedTangentImpulse = std::clamp(
        previousTangentImpulse + incrementalTangentImpulse,
        -maximumFrictionImpulse,
        maximumFrictionImpulse);
    const float appliedTangentImpulse =
        contact.accumulatedTangentImpulse - previousTangentImpulse;
    if (std::abs(appliedTangentImpulse) <= kContactEpsilon)
        return;

    const heritage::math::Vec3 frictionImpulse = scaleVector(
        contact.tangent,
        appliedTangentImpulse);
    RigidBodySystem::applyImpulseToRecord(
        bodyA,
        scaleVector(frictionImpulse, -1.0f),
        contact.point);
    RigidBodySystem::applyImpulseToRecord(
        bodyB,
        frictionImpulse,
        contact.point);
}
void CollisionSystem::resolveStaticVelocity(
    const Record& collider,
    const StaticSceneTriangle& triangle,
    RigidBodySystem::Record& body,
    CollisionContact& contact)
{
    heritage::math::Vec3 velocity = pointVelocity(body, contact.point);
    heritage::math::Vec3 relativeVelocity = scaleVector(velocity, -1.0f);
    const float velocityAlongNormal = dot(
        relativeVelocity,
        contact.normal);
    const float normalDenominator = staticEffectiveMassDenominator(
        body,
        contact.point,
        contact.normal);
    if (normalDenominator > kContactEpsilon)
    {
        const float incrementalNormalImpulse =
            -velocityAlongNormal / normalDenominator;
        const float previousNormalImpulse =
            contact.accumulatedNormalImpulse;
        contact.accumulatedNormalImpulse = (std::max)(
            0.0f,
            previousNormalImpulse + incrementalNormalImpulse);
        const float appliedNormalImpulse =
            contact.accumulatedNormalImpulse - previousNormalImpulse;
        if (std::abs(appliedNormalImpulse) > kContactEpsilon)
        {
            const heritage::math::Vec3 impulse = scaleVector(
                contact.normal,
                appliedNormalImpulse);
            RigidBodySystem::applyImpulseToRecord(
                body,
                scaleVector(impulse, -1.0f),
                contact.point);
        }
    }

    velocity = pointVelocity(body, contact.point);
    relativeVelocity = scaleVector(velocity, -1.0f);
    heritage::math::Vec3 tangent = subtract(
        relativeVelocity,
        scaleVector(contact.normal, dot(relativeVelocity, contact.normal)));
    const float tangentLength = length(tangent);
    if (tangentLength > kContactEpsilon)
    {
        tangent = scaleVector(tangent, 1.0f / tangentLength);
        if (dot(tangent, contact.tangent) < 0.0f)
        {
            contact.tangent = scaleVector(tangent, -1.0f);
            contact.accumulatedTangentImpulse =
                -contact.accumulatedTangentImpulse;
        }
        else
        {
            contact.tangent = tangent;
        }
    }

    const float tangentDenominator = staticEffectiveMassDenominator(
        body,
        contact.point,
        contact.tangent);
    if (tangentDenominator <= kContactEpsilon)
        return;

    const float incrementalTangentImpulse =
        -dot(relativeVelocity, contact.tangent)
        / tangentDenominator;
    const float wetness = std::clamp(triangle.surfaceWetness, 0.0f, 1.0f);
    const float terrainFriction = kStaticTriangleCollisionFriction
        * (1.0f - 0.35f * wetness);
    const float friction = std::sqrt(
        collider.friction * terrainFriction);
    const float maximumFrictionImpulse =
        contact.accumulatedNormalImpulse * friction;
    const float previousTangentImpulse =
        contact.accumulatedTangentImpulse;
    contact.accumulatedTangentImpulse = std::clamp(
        previousTangentImpulse + incrementalTangentImpulse,
        -maximumFrictionImpulse,
        maximumFrictionImpulse);
    const float appliedTangentImpulse =
        contact.accumulatedTangentImpulse - previousTangentImpulse;
    if (std::abs(appliedTangentImpulse) <= kContactEpsilon)
        return;

    RigidBodySystem::applyImpulseToRecord(
        body,
        scaleVector(
            contact.tangent,
            -appliedTangentImpulse),
        contact.point);
}

} // namespace heritage::physics
