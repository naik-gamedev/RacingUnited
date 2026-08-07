#include "CollisionSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace heritage::physics {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumRadius = 0.0001f;
constexpr float kMaximumShapeExtent = 1000000.0f;
constexpr float kMaximumMaterialValue = 10.0f;
constexpr float kContactEpsilon = 0.000001f;
constexpr float kSatEpsilon = 0.00001f;
constexpr float kPositionSlop = 0.0005f;
constexpr float kPositionCorrectionPercent = 0.78f;
constexpr float kRestitutionVelocityThreshold = 1.0f;
constexpr float kWarmStartPointDistance = 0.35f;
constexpr float kWarmStartNormalDot = 0.80f;
constexpr float kSleepFreeLinearSpeed = 0.15f;
constexpr float kSleepContactTangentialSpeed = 0.05f;
constexpr float kSleepContactNormalSpeed = 0.15f;
constexpr float kSleepAngularSpeedDegrees = 8.0f;
constexpr float kSleepDelaySeconds = 1.0f;
constexpr float kKinematicWakeSpeed = 0.01f;
constexpr float kPenetrationWakeThreshold = 0.02f;
constexpr float kContinuousCollisionLocalOffsetEpsilon = 0.0001f;
constexpr float kContinuousCollisionTravelFraction = 0.25f;
constexpr float kContinuousCollisionMinimumTravel = 0.01f;
constexpr float kContinuousCollisionSkin = 0.001f;

bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

bool finiteVec3(const heritage::math::Vec3& value)
{
    return finiteFloat(value.x)
        && finiteFloat(value.y)
        && finiteFloat(value.z);
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

heritage::math::Vec3 cross(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

float component(const heritage::math::Vec3& value, int index)
{
    switch (index)
    {
    case 0: return value.x;
    case 1: return value.y;
    default: return value.z;
    }
}

heritage::math::Vec3 scaleVector(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

float dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt((std::max)(0.0f, lengthSquared(value)));
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback = { 0.0f, 1.0f, 0.0f })
{
    const float magnitude = length(value);
    if (magnitude <= kContactEpsilon)
        return fallback;
    return scaleVector(value, 1.0f / magnitude);
}

heritage::math::Vec3 clampVector(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& minimum,
    const heritage::math::Vec3& maximum)
{
    return {
        std::clamp(value.x, minimum.x, maximum.x),
        std::clamp(value.y, minimum.y, maximum.y),
        std::clamp(value.z, minimum.z, maximum.z)
    };
}


} // namespace

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
    return { value.w, -value.x, -value.y, -value.z };
}

heritage::math::Vec3 CollisionSystem::rotateVector(
    const RigidBodySystem::Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    const heritage::math::Vec3 q{ rotation.x, rotation.y, rotation.z };
    const heritage::math::Vec3 twiceCross{
        2.0f * (q.y * value.z - q.z * value.y),
        2.0f * (q.z * value.x - q.x * value.z),
        2.0f * (q.x * value.y - q.y * value.x)
    };
    const heritage::math::Vec3 qCrossTwice{
        q.y * twiceCross.z - q.z * twiceCross.y,
        q.z * twiceCross.x - q.x * twiceCross.z,
        q.x * twiceCross.y - q.y * twiceCross.x
    };
    return add(value, add(
        scaleVector(twiceCross, rotation.w),
        qCrossTwice));
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
    const heritage::math::Vec3 leverArm = subtract(worldPoint, body.position);
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
    m_continuousCollisionBodyCount = 0;
    m_continuousCollisionSweepCount = 0;
    m_continuousCollisionHitCount = 0;
    m_continuousCollisionClampedBodyCount = 0;
    m_continuousCollisionUnsupportedBodyCount = 0;
    m_lastQueryCandidateCount = 0;
    m_lastQueryExactTestCount = 0;
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
    clearError();
}

void CollisionSystem::clearStaticSceneTriangles()
{
    m_staticSceneTriangles.clear();
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
        || surfaceValue > static_cast<int>(SurfaceMaterial::PaintedLine))
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
        || materialValue > static_cast<int>(SurfaceMaterial::PaintedLine)
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
    hit.trigger = false;
    return true;
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

    // Creator/world static triangle scene participates in read-only queries.
    // It is intentionally not yet fed into the rigid-body contact solver.
    if ((filter.layerMask & 1u) != 0u && !m_staticSceneTriangles.empty())
    {
        for (const StaticSceneTriangle& triangle : m_staticSceneTriangles)
        {
            const Aabb triangleBounds{
                {
                    (std::min)({ triangle.a.x, triangle.b.x, triangle.c.x }),
                    (std::min)({ triangle.a.y, triangle.b.y, triangle.c.y }),
                    (std::min)({ triangle.a.z, triangle.b.z, triangle.c.z })
                },
                {
                    (std::max)({ triangle.a.x, triangle.b.x, triangle.c.x }),
                    (std::max)({ triangle.a.y, triangle.b.y, triangle.c.y }),
                    (std::max)({ triangle.a.z, triangle.b.z, triangle.c.z })
                }
            };
            if (!aabbOverlap(rayBounds, triangleBounds))
                continue;

            ++m_lastQueryCandidateCount;
            ++m_lastQueryExactTestCount;
            RaycastHit candidate;
            if (!rayStaticSceneTriangle(
                    origin,
                    rayDirection,
                    maximumDistance,
                    triangle,
                    candidate))
            {
                continue;
            }

            if (!found || candidate.distance < closest.distance - kContactEpsilon)
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
    candidate.trigger = collider.trigger;
    hit = candidate;
    return true;
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
        if (castRadius <= 0.0f || !sourceCollider)
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
            castRadius * kContinuousCollisionTravelFraction);
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
        SphereCastHit closest;
        closest.distance = travelDistance;
        const Record* closestCollider = nullptr;

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
                || targetBodySlot->record.motionType == BodyMotionType::Dynamic)
            {
                // Dynamic-vs-dynamic time of impact requires relative swept
                // shapes and island-level substepping. It is intentionally
                // deferred; Step 28G protects fast bodies against static and
                // kinematic world geometry.
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
                || candidate.distance < closest.distance - kContactEpsilon
                || (std::abs(candidate.distance - closest.distance)
                        <= kContactEpsilon
                    && candidate.collider < closest.collider))
            {
                found = true;
                closest = candidate;
                closestCollider = &targetSlot.record;
            }
        }

        if (!found || !closestCollider)
            continue;

        ++m_continuousCollisionHitCount;
        const float safeDistance = (std::max)(
            0.0f,
            closest.distance - kContinuousCollisionSkin);
        bodyRecord.position = add(
            bodyRecord.previousPosition,
            scaleVector(travelDirection, safeDistance));

        const float velocityAlongNormal = dot(
            bodyRecord.linearVelocity,
            closest.normal);
        if (velocityAlongNormal < 0.0f)
        {
            const float restitution =
                velocityAlongNormal < -kRestitutionVelocityThreshold
                ? (std::min)(
                    sourceCollider->restitution,
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
    m_continuousCollisionBodyCount = 0;
    m_continuousCollisionSweepCount = 0;
    m_continuousCollisionHitCount = 0;
    m_continuousCollisionClampedBodyCount = 0;
    m_continuousCollisionUnsupportedBodyCount = 0;

    applyContinuousCollisionDetection(bodies, fixedDeltaTime);

    std::vector<BroadphaseProxy> proxies;
    proxies.reserve(m_aliveCount);
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive)
            continue;

        const RigidBodySystem::Slot* bodySlot = bodies.resolve(slot.record.body);
        if (!bodySlot)
            continue;

        BroadphaseProxy proxy;
        proxy.slotIndex = index;
        proxy.handle = makeHandle(index, slot.generation);
        proxy.bounds = worldAabb(slot.record, bodySlot->record);
        proxies.push_back(proxy);
    }

    std::stable_sort(
        proxies.begin(),
        proxies.end(),
        [](const BroadphaseProxy& left, const BroadphaseProxy& right) {
            if (left.bounds.minimum.x != right.bounds.minimum.x)
                return left.bounds.minimum.x < right.bounds.minimum.x;
            return left.handle < right.handle;
        });

    for (std::size_t firstIndex = 0; firstIndex < proxies.size(); ++firstIndex)
    {
        const BroadphaseProxy& proxyA = proxies[firstIndex];
        Slot& slotA = m_slots[proxyA.slotIndex];
        RigidBodySystem::Slot* bodySlotA = bodies.resolve(slotA.record.body);
        if (!bodySlotA)
            continue;

        for (std::size_t secondIndex = firstIndex + 1;
             secondIndex < proxies.size();
             ++secondIndex)
        {
            const BroadphaseProxy& proxyB = proxies[secondIndex];
            if (proxyB.bounds.minimum.x > proxyA.bounds.maximum.x)
                break;

            Slot& slotB = m_slots[proxyB.slotIndex];
            if (slotA.record.body == slotB.record.body)
                continue;
            if ((slotA.record.mask & slotB.record.layer) == 0u
                || (slotB.record.mask & slotA.record.layer) == 0u)
            {
                continue;
            }
            if (!aabbOverlap(proxyA.bounds, proxyB.bounds))
                continue;

            ++m_broadphaseCandidateCount;
            RigidBodySystem::Slot* bodySlotB = bodies.resolve(slotB.record.body);
            if (!bodySlotB)
                continue;

            ++m_narrowphaseTestCount;
            CollisionContact contact;
            if (!generateContact(
                    proxyA.handle,
                    slotA.record,
                    bodySlotA->record,
                    proxyB.handle,
                    slotB.record,
                    bodySlotB->record,
                    contact))
            {
                continue;
            }

            contact.trigger = slotA.record.trigger || slotB.record.trigger;
            canonicalizeContact(contact);
            if (!contact.trigger)
                restoreCachedImpulse(contact);
            m_contacts.push_back(contact);
        }
    }

    // Contacts define connected dynamic-body islands. Before solving, an awake
    // body or moving kinematic body wakes every dynamic body in the same island.
    updateSimulationIslandsAndSleeping(bodies, fixedDeltaTime, false);

    for (CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

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

void CollisionSystem::updateSimulationIslandsAndSleeping(
    RigidBodySystem& bodies,
    float fixedDeltaTime,
    bool finalizeSleep)
{
    const std::uint32_t invalidIndex =
        (std::numeric_limits<std::uint32_t>::max)();
    std::vector<std::uint32_t> parent(bodies.m_slots.size(), invalidIndex);
    std::vector<std::uint8_t> rank(bodies.m_slots.size(), 0u);

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++index)
    {
        const RigidBodySystem::Slot& slot = bodies.m_slots[index];
        if (slot.alive
            && slot.record.motionType == BodyMotionType::Dynamic)
        {
            parent[index] = index;
        }
    }

    const auto findRoot = [&parent, invalidIndex](std::uint32_t index) {
        if (index >= parent.size() || parent[index] == invalidIndex)
            return invalidIndex;

        std::uint32_t root = index;
        while (parent[root] != root)
            root = parent[root];

        while (parent[index] != index)
        {
            const std::uint32_t next = parent[index];
            parent[index] = root;
            index = next;
        }
        return root;
    };

    const auto unionBodies = [&parent, &rank, &findRoot, invalidIndex](
        std::uint32_t first,
        std::uint32_t second) {
        std::uint32_t rootA = findRoot(first);
        std::uint32_t rootB = findRoot(second);
        if (rootA == invalidIndex || rootB == invalidIndex || rootA == rootB)
            return;

        if (rank[rootA] < rank[rootB])
            std::swap(rootA, rootB);
        parent[rootB] = rootA;
        if (rank[rootA] == rank[rootB])
            ++rank[rootA];
    };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        const bool validA = RigidBodySystem::decodeHandle(
            contact.bodyA, bodyIndexA, bodyGenerationA);
        const bool validB = RigidBodySystem::decodeHandle(
            contact.bodyB, bodyIndexB, bodyGenerationB);
        if (!validA || !validB
            || bodyIndexA >= bodies.m_slots.size()
            || bodyIndexB >= bodies.m_slots.size())
        {
            continue;
        }

        const RigidBodySystem::Slot& slotA = bodies.m_slots[bodyIndexA];
        const RigidBodySystem::Slot& slotB = bodies.m_slots[bodyIndexB];
        if (!slotA.alive || slotA.generation != bodyGenerationA
            || !slotB.alive || slotB.generation != bodyGenerationB)
        {
            continue;
        }

        if (slotA.record.motionType == BodyMotionType::Dynamic
            && slotB.record.motionType == BodyMotionType::Dynamic)
        {
            unionBodies(bodyIndexA, bodyIndexB);
        }
    }

    struct Island
    {
        std::vector<std::uint32_t> bodyIndices;
        bool wakeRequested = false;
        bool touchedMovingKinematic = false;
    };

    std::vector<Island> islands;
    std::unordered_map<std::uint32_t, std::size_t> islandByRoot;
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++index)
    {
        if (parent[index] == invalidIndex)
            continue;

        const std::uint32_t root = findRoot(index);
        auto [iterator, inserted] = islandByRoot.emplace(root, islands.size());
        if (inserted)
            islands.emplace_back();

        Island& island = islands[iterator->second];
        island.bodyIndices.push_back(index);
        if (!bodies.m_slots[index].record.sleeping)
            island.wakeRequested = true;
    }

    const auto movingKinematic = [](const RigidBodySystem::Record& body) {
        if (body.motionType != BodyMotionType::Kinematic)
            return false;
        return lengthSquared(body.linearVelocity)
                > kKinematicWakeSpeed * kKinematicWakeSpeed
            || lengthSquared(body.angularVelocityDegrees)
                > kKinematicWakeSpeed * kKinematicWakeSpeed;
    };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        if (!RigidBodySystem::decodeHandle(
                contact.bodyA, bodyIndexA, bodyGenerationA)
            || !RigidBodySystem::decodeHandle(
                contact.bodyB, bodyIndexB, bodyGenerationB)
            || bodyIndexA >= bodies.m_slots.size()
            || bodyIndexB >= bodies.m_slots.size())
        {
            continue;
        }

        const RigidBodySystem::Record& bodyA =
            bodies.m_slots[bodyIndexA].record;
        const RigidBodySystem::Record& bodyB =
            bodies.m_slots[bodyIndexB].record;

        if (bodyA.motionType == BodyMotionType::Dynamic
            && movingKinematic(bodyB))
        {
            const std::uint32_t root = findRoot(bodyIndexA);
            const auto iterator = islandByRoot.find(root);
            if (iterator != islandByRoot.end())
            {
                islands[iterator->second].wakeRequested = true;
                islands[iterator->second].touchedMovingKinematic = true;
            }
        }
        if (bodyB.motionType == BodyMotionType::Dynamic
            && movingKinematic(bodyA))
        {
            const std::uint32_t root = findRoot(bodyIndexB);
            const auto iterator = islandByRoot.find(root);
            if (iterator != islandByRoot.end())
            {
                islands[iterator->second].wakeRequested = true;
                islands[iterator->second].touchedMovingKinematic = true;
            }
        }

        if (contact.penetration > kPenetrationWakeThreshold)
        {
            if (bodyA.motionType == BodyMotionType::Dynamic)
            {
                const auto iterator = islandByRoot.find(findRoot(bodyIndexA));
                if (iterator != islandByRoot.end())
                    islands[iterator->second].wakeRequested = true;
            }
            if (bodyB.motionType == BodyMotionType::Dynamic)
            {
                const auto iterator = islandByRoot.find(findRoot(bodyIndexB));
                if (iterator != islandByRoot.end())
                    islands[iterator->second].wakeRequested = true;
            }
        }
    }

    if (!finalizeSleep)
    {
        for (Island& island : islands)
        {
            if (!island.wakeRequested)
                continue;
            for (const std::uint32_t bodyIndex : island.bodyIndices)
            {
                RigidBodySystem::Record& body =
                    bodies.m_slots[bodyIndex].record;
                if (body.sleeping)
                    RigidBodySystem::wakeRecord(body);
            }
        }
        return;
    }

    m_simulationIslandCount = islands.size();
    m_activeIslandCount = 0;
    m_sleepingIslandCount = 0;

    const float safeDeltaTime =
        finiteFloat(fixedDeltaTime) && fixedDeltaTime > 0.0f
        ? fixedDeltaTime
        : 0.0f;
    const float freeLinearThresholdSquared =
        kSleepFreeLinearSpeed * kSleepFreeLinearSpeed;
    const float contactTangentialThresholdSquared =
        kSleepContactTangentialSpeed * kSleepContactTangentialSpeed;
    const float angularThresholdSquared =
        kSleepAngularSpeedDegrees * kSleepAngularSpeedDegrees;

    std::vector<bool> hasContact(bodies.m_slots.size(), false);
    std::vector<float> maximumTangentialSpeedSquared(
        bodies.m_slots.size(), 0.0f);
    std::vector<float> maximumNormalSpeed(
        bodies.m_slots.size(), 0.0f);

    const auto recordContactVelocity =
        [&bodies,
         &hasContact,
         &maximumTangentialSpeedSquared,
         &maximumNormalSpeed](
            std::uint32_t bodyIndex,
            const heritage::math::Vec3& normal) {
            if (bodyIndex >= bodies.m_slots.size())
                return;
            const RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            if (body.motionType != BodyMotionType::Dynamic)
                return;

            const float normalSpeed = dot(body.linearVelocity, normal);
            const heritage::math::Vec3 tangentVelocity = subtract(
                body.linearVelocity,
                scaleVector(normal, normalSpeed));
            hasContact[bodyIndex] = true;
            maximumTangentialSpeedSquared[bodyIndex] = (std::max)(
                maximumTangentialSpeedSquared[bodyIndex],
                lengthSquared(tangentVelocity));
            maximumNormalSpeed[bodyIndex] = (std::max)(
                maximumNormalSpeed[bodyIndex],
                std::abs(normalSpeed));
        };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        if (!RigidBodySystem::decodeHandle(
                contact.bodyA, bodyIndexA, bodyGenerationA)
            || !RigidBodySystem::decodeHandle(
                contact.bodyB, bodyIndexB, bodyGenerationB))
        {
            continue;
        }

        recordContactVelocity(bodyIndexA, contact.normal);
        recordContactVelocity(bodyIndexB, contact.normal);
    }

    for (Island& island : islands)
    {
        bool allSleeping = true;
        bool allQuiet = !island.touchedMovingKinematic;
        bool allAllowSleep = true;

        for (const std::uint32_t bodyIndex : island.bodyIndices)
        {
            const RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            allSleeping = allSleeping && body.sleeping;
            allAllowSleep = allAllowSleep && body.allowSleep;

            const bool linearQuiet =
                lengthSquared(body.linearVelocity)
                    <= freeLinearThresholdSquared
                || (hasContact[bodyIndex]
                    && maximumTangentialSpeedSquared[bodyIndex]
                        <= contactTangentialThresholdSquared
                    && maximumNormalSpeed[bodyIndex]
                        <= kSleepContactNormalSpeed);
            if (!linearQuiet
                || lengthSquared(body.angularVelocityDegrees)
                    > angularThresholdSquared)
            {
                allQuiet = false;
            }
        }

        if (allSleeping)
        {
            ++m_sleepingIslandCount;
            continue;
        }

        if (!allQuiet || !allAllowSleep)
        {
            for (const std::uint32_t bodyIndex : island.bodyIndices)
                RigidBodySystem::wakeRecord(bodies.m_slots[bodyIndex].record);
            ++m_activeIslandCount;
            continue;
        }

        bool readyToSleep = true;
        for (const std::uint32_t bodyIndex : island.bodyIndices)
        {
            RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            body.sleepTimer += safeDeltaTime;
            readyToSleep = readyToSleep
                && body.sleepTimer >= kSleepDelaySeconds;
        }

        if (readyToSleep)
        {
            for (const std::uint32_t bodyIndex : island.bodyIndices)
                RigidBodySystem::sleepRecord(bodies.m_slots[bodyIndex].record);
            ++m_sleepingIslandCount;
        }
        else
        {
            ++m_activeIslandCount;
        }
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

        // Parallel-axis contribution for collider offsets from the body origin.
        const heritage::math::Vec3& offset = collider.localPosition;
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

float CollisionSystem::effectiveMassDenominator(
    const RigidBodySystem::Record& bodyA,
    const RigidBodySystem::Record& bodyB,
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& direction) const
{
    float denominator = inverseMassForContact(bodyA)
        + inverseMassForContact(bodyB);

    const heritage::math::Vec3 leverA = subtract(point, bodyA.position);
    const heritage::math::Vec3 leverB = subtract(point, bodyB.position);
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
