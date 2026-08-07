#include "RigidBodySystem.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace heritage::physics {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumMass = 0.0001f;
constexpr float kMaximumMass = 1000000000.0f;
constexpr float kMaximumDamping = 1000.0f;
constexpr float kQuaternionEpsilon = 0.000001f;

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

heritage::math::Vec3 multiplyComponents(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x * right.x, left.y * right.y, left.z * right.z };
}

heritage::math::Vec3 scaleVector(
    const heritage::math::Vec3& value,
    float scalar)
{
    return { value.x * scalar, value.y * scalar, value.z * scalar };
}

heritage::math::Vec3 lerp(
    const heritage::math::Vec3& previous,
    const heritage::math::Vec3& current,
    float alpha)
{
    return {
        previous.x + (current.x - previous.x) * alpha,
        previous.y + (current.y - previous.y) * alpha,
        previous.z + (current.z - previous.z) * alpha
    };
}

float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

std::string lowercase(const std::string& text)
{
    std::string result = text;
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return result;
}

} // namespace

void RigidBodySystem::clear()
{
    m_slots.clear();
    m_freeIndices.clear();
    m_bodyByEntity.clear();
    m_aliveCount = 0;
    ++m_massPropertiesRevision;
    if (m_massPropertiesRevision == 0)
        m_massPropertiesRevision = 1;
    m_lastError.clear();
}

BodyHandle RigidBodySystem::create(const RigidBodyDescription& description)
{
    if (!finiteVec3(description.position)
        || !finiteVec3(description.rotationDegrees))
    {
        setError("Physics.CreateBody requires finite position and rotation values.");
        return InvalidBody;
    }

    if (!finiteFloat(description.mass)
        || description.mass < kMinimumMass
        || description.mass > kMaximumMass)
    {
        setError("Physics.CreateBody mass must be between 0.0001 and 1,000,000,000 kg.");
        return InvalidBody;
    }

    if (!finiteFloat(description.gravityFactor))
    {
        setError("Physics.CreateBody gravity factor must be finite.");
        return InvalidBody;
    }

    if (!finiteFloat(description.linearDamping)
        || description.linearDamping < 0.0f
        || description.linearDamping > kMaximumDamping
        || !finiteFloat(description.angularDamping)
        || description.angularDamping < 0.0f
        || description.angularDamping > kMaximumDamping)
    {
        setError("Physics.CreateBody damping must be between 0 and 1000.");
        return InvalidBody;
    }

    if (description.entity != heritage::entities::InvalidEntity
        && bodyForEntity(description.entity) != InvalidBody)
    {
        setError("Physics.CreateBody cannot bind two rigid bodies to the same entity.");
        return InvalidBody;
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
            setError("Rigid-body storage exhausted its handle index space.");
            return InvalidBody;
        }

        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }

    Slot& slot = m_slots[index];
    slot.alive = true;
    slot.record = {};
    slot.record.entity = description.entity;
    slot.record.motionType = description.motionType;
    slot.record.previousPosition = description.position;
    slot.record.position = description.position;
    slot.record.previousRotation = quaternionFromEulerDegrees(description.rotationDegrees);
    slot.record.rotation = slot.record.previousRotation;
    slot.record.mass = description.mass;
    slot.record.inverseMass = description.motionType == BodyMotionType::Dynamic
        ? 1.0f / description.mass
        : 0.0f;
    slot.record.inverseInertiaLocal = description.motionType == BodyMotionType::Dynamic
        ? heritage::math::Vec3{
            slot.record.inverseMass,
            slot.record.inverseMass,
            slot.record.inverseMass }
        : heritage::math::Vec3{};
    slot.record.gravityFactor = description.gravityFactor;
    slot.record.linearDamping = description.linearDamping;
    slot.record.angularDamping = description.angularDamping;
    slot.record.continuousCollision = description.continuousCollision;

    const BodyHandle handle = makeHandle(index, slot.generation);
    if (description.entity != heritage::entities::InvalidEntity)
        m_bodyByEntity[description.entity] = handle;

    ++m_aliveCount;
    ++m_massPropertiesRevision;
    if (m_massPropertiesRevision == 0)
        m_massPropertiesRevision = 1;
    clearError();
    return handle;
}

bool RigidBodySystem::destroy(BodyHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        setError("Physics.DestroyBody received an invalid or stale body handle.");
        return false;
    }

    Slot& slot = m_slots[index];
    if (!slot.alive || slot.generation != generation)
    {
        setError("Physics.DestroyBody received an invalid or stale body handle.");
        return false;
    }

    return destroyResolved(index, slot);
}

bool RigidBodySystem::exists(BodyHandle handle) const
{
    const bool result = resolve(handle) != nullptr;
    if (result)
        clearError();
    return result;
}

BodyHandle RigidBodySystem::bodyForEntity(
    heritage::entities::EntityHandle entity) const
{
    if (entity == heritage::entities::InvalidEntity)
        return InvalidBody;

    const auto iterator = m_bodyByEntity.find(entity);
    if (iterator == m_bodyByEntity.end())
        return InvalidBody;

    return resolve(iterator->second) ? iterator->second : InvalidBody;
}

heritage::entities::EntityHandle RigidBodySystem::entityForBody(
    BodyHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyEntity received an invalid or stale body handle.");
        return heritage::entities::InvalidEntity;
    }

    clearError();
    return slot->record.entity;
}

bool RigidBodySystem::motionType(
    BodyHandle handle,
    BodyMotionType& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyMotionType received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.motionType;
    clearError();
    return true;
}

bool RigidBodySystem::setMotionType(
    BodyHandle handle,
    BodyMotionType value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyMotionType received an invalid or stale body handle.");
        return false;
    }

    Record& body = slot->record;
    body.motionType = value;
    body.inverseMass = value == BodyMotionType::Dynamic
        ? 1.0f / body.mass
        : 0.0f;
    if (value == BodyMotionType::Dynamic
        && body.inverseInertiaLocal.x <= 0.0f
        && body.inverseInertiaLocal.y <= 0.0f
        && body.inverseInertiaLocal.z <= 0.0f)
    {
        body.inverseInertiaLocal = {
            body.inverseMass,
            body.inverseMass,
            body.inverseMass
        };
    }
    else if (value != BodyMotionType::Dynamic)
    {
        body.inverseInertiaLocal = {};
    }
    body.accumulatedForce = {};
    body.sleepTimer = 0.0f;
    body.sleeping = false;

    if (value == BodyMotionType::Static)
    {
        body.linearVelocity = {};
        body.angularVelocityDegrees = {};
    }

    body.previousPosition = body.position;
    body.previousRotation = body.rotation;
    ++m_massPropertiesRevision;
    if (m_massPropertiesRevision == 0)
        m_massPropertiesRevision = 1;
    clearError();
    return true;
}

bool RigidBodySystem::mass(BodyHandle handle, float& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyMass received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.mass;
    clearError();
    return true;
}

bool RigidBodySystem::setMass(BodyHandle handle, float value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyMass received an invalid or stale body handle.");
        return false;
    }
    if (!finiteFloat(value) || value < kMinimumMass || value > kMaximumMass)
    {
        setError("Physics.SetBodyMass requires 0.0001 to 1,000,000,000 kg.");
        return false;
    }

    const float previousMass = slot->record.mass;
    slot->record.mass = value;
    slot->record.inverseMass = slot->record.motionType == BodyMotionType::Dynamic
        ? 1.0f / value
        : 0.0f;
    if (slot->record.motionType == BodyMotionType::Dynamic
        && previousMass > 0.0f)
    {
        // Preserve a physically sensible tensor until CollisionSystem rebuilds
        // exact collider-derived mass properties on the next fixed step.
        slot->record.inverseInertiaLocal = scaleVector(
            slot->record.inverseInertiaLocal,
            previousMass / value);
    }
    else
    {
        slot->record.inverseInertiaLocal = {};
    }
    wakeRecord(slot->record);
    ++m_massPropertiesRevision;
    if (m_massPropertiesRevision == 0)
        m_massPropertiesRevision = 1;
    clearError();
    return true;
}

bool RigidBodySystem::gravityFactor(BodyHandle handle, float& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyGravityFactor received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.gravityFactor;
    clearError();
    return true;
}

bool RigidBodySystem::setGravityFactor(BodyHandle handle, float value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyGravityFactor received an invalid or stale body handle.");
        return false;
    }
    if (!finiteFloat(value))
    {
        setError("Physics.SetBodyGravityFactor requires a finite value.");
        return false;
    }

    slot->record.gravityFactor = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::linearDamping(BodyHandle handle, float& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyLinearDamping received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.linearDamping;
    clearError();
    return true;
}

bool RigidBodySystem::setLinearDamping(BodyHandle handle, float value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyLinearDamping received an invalid or stale body handle.");
        return false;
    }
    if (!finiteFloat(value) || value < 0.0f || value > kMaximumDamping)
    {
        setError("Physics.SetBodyLinearDamping requires a value between 0 and 1000.");
        return false;
    }

    slot->record.linearDamping = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::angularDamping(BodyHandle handle, float& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyAngularDamping received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.angularDamping;
    clearError();
    return true;
}

bool RigidBodySystem::setAngularDamping(BodyHandle handle, float value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyAngularDamping received an invalid or stale body handle.");
        return false;
    }
    if (!finiteFloat(value) || value < 0.0f || value > kMaximumDamping)
    {
        setError("Physics.SetBodyAngularDamping requires a value between 0 and 1000.");
        return false;
    }

    slot->record.angularDamping = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::continuousCollision(
    BodyHandle handle,
    bool& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyContinuousCollision received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.continuousCollision;
    clearError();
    return true;
}

bool RigidBodySystem::setContinuousCollision(
    BodyHandle handle,
    bool enabled)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyContinuousCollision received an invalid or stale body handle.");
        return false;
    }

    slot->record.continuousCollision = enabled;
    if (enabled)
        wakeRecord(slot->record);
    clearError();
    return true;
}


bool RigidBodySystem::pose(BodyHandle handle, RigidBodyPose& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyPose received an invalid or stale body handle.");
        return false;
    }

    value.position = slot->record.position;
    value.rotationDegrees = eulerDegreesFromQuaternion(slot->record.rotation);
    clearError();
    return true;
}

bool RigidBodySystem::interpolatedPose(
    BodyHandle handle,
    float alpha,
    RigidBodyPose& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyInterpolatedPose received an invalid or stale body handle.");
        return false;
    }

    const float safeAlpha = std::clamp(alpha, 0.0f, 1.0f);
    value.position = lerp(
        slot->record.previousPosition,
        slot->record.position,
        safeAlpha);
    value.rotationDegrees = eulerDegreesFromQuaternion(interpolate(
        slot->record.previousRotation,
        slot->record.rotation,
        safeAlpha));
    clearError();
    return true;
}

bool RigidBodySystem::setPose(
    BodyHandle handle,
    const RigidBodyPose& value)
{
    if (!finiteVec3(value.position) || !finiteVec3(value.rotationDegrees))
    {
        setError("Physics.SetBodyPose requires finite position and rotation values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyPose received an invalid or stale body handle.");
        return false;
    }

    slot->record.position = value.position;
    slot->record.previousPosition = value.position;
    slot->record.rotation = quaternionFromEulerDegrees(value.rotationDegrees);
    slot->record.previousRotation = slot->record.rotation;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::setPosition(
    BodyHandle handle,
    const heritage::math::Vec3& value)
{
    if (!finiteVec3(value))
    {
        setError("Physics.SetBodyPosition requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyPosition received an invalid or stale body handle.");
        return false;
    }

    slot->record.position = value;
    slot->record.previousPosition = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::setRotationDegrees(
    BodyHandle handle,
    const heritage::math::Vec3& value)
{
    if (!finiteVec3(value))
    {
        setError("Physics.SetBodyRotation requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyRotation received an invalid or stale body handle.");
        return false;
    }

    slot->record.rotation = quaternionFromEulerDegrees(value);
    slot->record.previousRotation = slot->record.rotation;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::linearVelocity(
    BodyHandle handle,
    heritage::math::Vec3& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyLinearVelocity received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.linearVelocity;
    clearError();
    return true;
}

bool RigidBodySystem::setLinearVelocity(
    BodyHandle handle,
    const heritage::math::Vec3& value)
{
    if (!finiteVec3(value))
    {
        setError("Physics.SetBodyLinearVelocity requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyLinearVelocity received an invalid or stale body handle.");
        return false;
    }

    slot->record.linearVelocity = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::angularVelocityDegrees(
    BodyHandle handle,
    heritage::math::Vec3& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyAngularVelocity received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.angularVelocityDegrees;
    clearError();
    return true;
}

bool RigidBodySystem::setAngularVelocityDegrees(
    BodyHandle handle,
    const heritage::math::Vec3& value)
{
    if (!finiteVec3(value))
    {
        setError("Physics.SetBodyAngularVelocity requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyAngularVelocity received an invalid or stale body handle.");
        return false;
    }

    slot->record.angularVelocityDegrees = value;
    wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::applyForce(
    BodyHandle handle,
    const heritage::math::Vec3& force)
{
    if (!finiteVec3(force))
    {
        setError("Physics.ApplyBodyForce requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.ApplyBodyForce received an invalid or stale body handle.");
        return false;
    }
    if (slot->record.motionType != BodyMotionType::Dynamic)
    {
        setError("Physics.ApplyBodyForce requires a dynamic rigid body.");
        return false;
    }

    wakeRecord(slot->record);
    slot->record.accumulatedForce = add(slot->record.accumulatedForce, force);
    clearError();
    return true;
}

bool RigidBodySystem::applyLinearImpulse(
    BodyHandle handle,
    const heritage::math::Vec3& impulse)
{
    if (!finiteVec3(impulse))
    {
        setError("Physics.ApplyBodyImpulse requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.ApplyBodyImpulse received an invalid or stale body handle.");
        return false;
    }
    if (slot->record.motionType != BodyMotionType::Dynamic)
    {
        setError("Physics.ApplyBodyImpulse requires a dynamic rigid body.");
        return false;
    }

    wakeRecord(slot->record);
    slot->record.linearVelocity = add(
        slot->record.linearVelocity,
        scaleVector(impulse, slot->record.inverseMass));
    clearError();
    return true;
}

bool RigidBodySystem::applyImpulseAtPoint(
    BodyHandle handle,
    const heritage::math::Vec3& impulse,
    const heritage::math::Vec3& worldPoint)
{
    if (!finiteVec3(impulse) || !finiteVec3(worldPoint))
    {
        setError("Physics.ApplyBodyImpulseAtPoint requires finite impulse and point values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.ApplyBodyImpulseAtPoint received an invalid or stale body handle.");
        return false;
    }
    if (slot->record.motionType != BodyMotionType::Dynamic)
    {
        setError("Physics.ApplyBodyImpulseAtPoint requires a dynamic rigid body.");
        return false;
    }

    wakeRecord(slot->record);
    applyImpulseToRecord(slot->record, impulse, worldPoint);
    clearError();
    return true;
}

bool RigidBodySystem::applyAngularImpulse(
    BodyHandle handle,
    const heritage::math::Vec3& angularImpulse)
{
    if (!finiteVec3(angularImpulse))
    {
        setError("Physics.ApplyBodyAngularImpulse requires finite values.");
        return false;
    }

    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.ApplyBodyAngularImpulse received an invalid or stale body handle.");
        return false;
    }
    if (slot->record.motionType != BodyMotionType::Dynamic)
    {
        setError("Physics.ApplyBodyAngularImpulse requires a dynamic rigid body.");
        return false;
    }

    wakeRecord(slot->record);
    const heritage::math::Vec3 deltaRadiansPerSecond =
        applyWorldInverseInertia(slot->record, angularImpulse);
    slot->record.angularVelocityDegrees = add(
        slot->record.angularVelocityDegrees,
        {
            degrees(deltaRadiansPerSecond.x),
            degrees(deltaRadiansPerSecond.y),
            degrees(deltaRadiansPerSecond.z)
        });
    clearError();
    return true;
}

bool RigidBodySystem::clearForces(BodyHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.ClearBodyForces received an invalid or stale body handle.");
        return false;
    }

    slot->record.accumulatedForce = {};
    clearError();
    return true;
}

bool RigidBodySystem::sleeping(BodyHandle handle, bool& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.IsBodySleeping received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.sleeping;
    clearError();
    return true;
}

bool RigidBodySystem::setSleeping(BodyHandle handle, bool value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodySleeping received an invalid or stale body handle.");
        return false;
    }
    if (slot->record.motionType != BodyMotionType::Dynamic)
    {
        setError("Only dynamic rigid bodies can enter sleep.");
        return false;
    }
    if (value && !slot->record.allowSleep)
    {
        setError("Physics.SetBodySleeping cannot sleep a body while allow-sleep is disabled.");
        return false;
    }

    if (value)
        sleepRecord(slot->record);
    else
        wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::allowSleep(BodyHandle handle, bool& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetBodyAllowSleep received an invalid or stale body handle.");
        return false;
    }

    value = slot->record.allowSleep;
    clearError();
    return true;
}

bool RigidBodySystem::setAllowSleep(BodyHandle handle, bool value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetBodyAllowSleep received an invalid or stale body handle.");
        return false;
    }

    slot->record.allowSleep = value;
    if (!value)
        wakeRecord(slot->record);
    clearError();
    return true;
}

bool RigidBodySystem::wake(BodyHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.WakeBody received an invalid or stale body handle.");
        return false;
    }

    wakeRecord(slot->record);
    clearError();
    return true;
}

void RigidBodySystem::wakeAll()
{
    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;
        wakeRecord(slot.record);
    }
    clearError();
}

std::size_t RigidBodySystem::sleepingCount() const
{
    std::size_t result = 0;
    for (const Slot& slot : m_slots)
    {
        if (slot.alive
            && slot.record.motionType == BodyMotionType::Dynamic
            && slot.record.sleeping)
        {
            ++result;
        }
    }
    return result;
}

std::size_t RigidBodySystem::activeDynamicCount() const
{
    std::size_t result = 0;
    for (const Slot& slot : m_slots)
    {
        if (slot.alive
            && slot.record.motionType == BodyMotionType::Dynamic
            && !slot.record.sleeping)
        {
            ++result;
        }
    }
    return result;
}

void RigidBodySystem::integrate(
    float fixedDeltaTime,
    const heritage::math::Vec3& gravity)
{
    if (!finiteFloat(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
        return;

    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;

        Record& body = slot.record;
        body.previousPosition = body.position;
        body.previousRotation = body.rotation;

        if (body.motionType == BodyMotionType::Static)
        {
            body.sleeping = false;
            body.sleepTimer = 0.0f;
            body.accumulatedForce = {};
            continue;
        }

        if (body.motionType == BodyMotionType::Kinematic)
        {
            // Kinematic motion is authored directly by game code. It is not
            // affected by gravity, forces, mass or damping.
            body.position = add(
                body.position,
                scaleVector(body.linearVelocity, fixedDeltaTime));
            body.rotation = integrateAngularVelocity(
                body.rotation,
                body.angularVelocityDegrees,
                fixedDeltaTime);
            body.sleeping = false;
            body.sleepTimer = 0.0f;
            body.accumulatedForce = {};
            continue;
        }

        if (body.sleeping)
        {
            body.linearVelocity = {};
            body.angularVelocityDegrees = {};
            body.accumulatedForce = {};
            continue;
        }

        const heritage::math::Vec3 acceleration = add(
            scaleVector(gravity, body.gravityFactor),
            scaleVector(body.accumulatedForce, body.inverseMass));
        body.linearVelocity = add(
            body.linearVelocity,
            scaleVector(acceleration, fixedDeltaTime));

        const float linearDecay = std::exp(-body.linearDamping * fixedDeltaTime);
        const float angularDecay = std::exp(-body.angularDamping * fixedDeltaTime);
        body.linearVelocity = scaleVector(body.linearVelocity, linearDecay);
        body.angularVelocityDegrees = scaleVector(
            body.angularVelocityDegrees,
            angularDecay);

        body.position = add(
            body.position,
            scaleVector(body.linearVelocity, fixedDeltaTime));
        body.rotation = integrateAngularVelocity(
            body.rotation,
            body.angularVelocityDegrees,
            fixedDeltaTime);
        body.accumulatedForce = {};
    }
}

void RigidBodySystem::snapInterpolation()
{
    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;
        slot.record.previousPosition = slot.record.position;
        slot.record.previousRotation = slot.record.rotation;
    }
}

void RigidBodySystem::synchronizeEntities(
    heritage::entities::EntityRegistry& entities,
    float interpolationAlpha)
{
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        Slot& slot = m_slots[index];
        if (!slot.alive)
            continue;

        const heritage::entities::EntityHandle entity = slot.record.entity;
        if (entity == heritage::entities::InvalidEntity)
            continue;

        if (!entities.exists(entity))
        {
            destroyResolved(index, slot);
            continue;
        }

        const float alpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
        const heritage::math::Vec3 position = lerp(
            slot.record.previousPosition,
            slot.record.position,
            alpha);
        const heritage::math::Vec3 rotationDegrees = eulerDegreesFromQuaternion(
            interpolate(slot.record.previousRotation, slot.record.rotation, alpha));

        entities.setWorldPosition(entity, position);
        entities.setWorldRotationDegrees(entity, rotationDegrees);
    }

    clearError();
}

BodyHandle RigidBodySystem::makeHandle(
    std::uint32_t index,
    std::uint32_t generation)
{
    return (static_cast<BodyHandle>(generation) << 32u)
        | static_cast<BodyHandle>(index + 1u);
}

bool RigidBodySystem::decodeHandle(
    BodyHandle handle,
    std::uint32_t& index,
    std::uint32_t& generation)
{
    if (handle == InvalidBody)
        return false;

    const std::uint32_t encodedIndex = static_cast<std::uint32_t>(
        handle & 0xffffffffull);
    generation = static_cast<std::uint32_t>(handle >> 32u);
    if (encodedIndex == 0 || generation == 0)
        return false;

    index = encodedIndex - 1u;
    return true;
}

RigidBodySystem::Slot* RigidBodySystem::resolve(BodyHandle handle)
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

const RigidBodySystem::Slot* RigidBodySystem::resolve(BodyHandle handle) const
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

bool RigidBodySystem::destroyResolved(std::uint32_t index, Slot& slot)
{
    if (slot.record.entity != heritage::entities::InvalidEntity)
    {
        const auto iterator = m_bodyByEntity.find(slot.record.entity);
        if (iterator != m_bodyByEntity.end())
            m_bodyByEntity.erase(iterator);
    }

    slot.alive = false;
    slot.record = {};
    ++slot.generation;
    if (slot.generation == 0)
        slot.generation = 1;
    m_freeIndices.push_back(index);
    if (m_aliveCount > 0)
        --m_aliveCount;
    ++m_massPropertiesRevision;
    if (m_massPropertiesRevision == 0)
        m_massPropertiesRevision = 1;
    clearError();
    return true;
}

RigidBodySystem::Quaternion RigidBodySystem::quaternionFromEulerDegrees(
    const heritage::math::Vec3& value)
{
    const float halfX = radians(value.x) * 0.5f;
    const float halfY = radians(value.y) * 0.5f;
    const float halfZ = radians(value.z) * 0.5f;

    const float cx = std::cos(halfX);
    const float sx = std::sin(halfX);
    const float cy = std::cos(halfY);
    const float sy = std::sin(halfY);
    const float cz = std::cos(halfZ);
    const float sz = std::sin(halfZ);

    return normalized({
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    });
}

heritage::math::Vec3 RigidBodySystem::eulerDegreesFromQuaternion(
    const Quaternion& value)
{
    const Quaternion q = normalized(value);
    const float sinXCosY = 2.0f * (q.w * q.x + q.y * q.z);
    const float cosXCosY = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    const float angleX = std::atan2(sinXCosY, cosXCosY);

    const float sinY = std::clamp(
        2.0f * (q.w * q.y - q.z * q.x),
        -1.0f,
        1.0f);
    const float angleY = std::asin(sinY);

    const float sinZCosY = 2.0f * (q.w * q.z + q.x * q.y);
    const float cosZCosY = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    const float angleZ = std::atan2(sinZCosY, cosZCosY);

    return { degrees(angleX), degrees(angleY), degrees(angleZ) };
}

RigidBodySystem::Quaternion RigidBodySystem::multiply(
    const Quaternion& left,
    const Quaternion& right)
{
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w
    };
}

RigidBodySystem::Quaternion RigidBodySystem::normalized(
    const Quaternion& value)
{
    const float lengthSquared = value.w * value.w
        + value.x * value.x
        + value.y * value.y
        + value.z * value.z;
    if (lengthSquared <= kQuaternionEpsilon)
        return {};

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    return {
        value.w * inverseLength,
        value.x * inverseLength,
        value.y * inverseLength,
        value.z * inverseLength
    };
}

RigidBodySystem::Quaternion RigidBodySystem::interpolate(
    const Quaternion& previous,
    const Quaternion& current,
    float alpha)
{
    Quaternion target = current;
    const float dot = previous.w * current.w
        + previous.x * current.x
        + previous.y * current.y
        + previous.z * current.z;
    if (dot < 0.0f)
    {
        target.w = -target.w;
        target.x = -target.x;
        target.y = -target.y;
        target.z = -target.z;
    }

    return normalized({
        previous.w + (target.w - previous.w) * alpha,
        previous.x + (target.x - previous.x) * alpha,
        previous.y + (target.y - previous.y) * alpha,
        previous.z + (target.z - previous.z) * alpha
    });
}

RigidBodySystem::Quaternion RigidBodySystem::integrateAngularVelocity(
    const Quaternion& rotation,
    const heritage::math::Vec3& angularVelocityDegrees,
    float fixedDeltaTime)
{
    const heritage::math::Vec3 radiansPerSecond{
        radians(angularVelocityDegrees.x),
        radians(angularVelocityDegrees.y),
        radians(angularVelocityDegrees.z)
    };
    const float speed = std::sqrt(
        radiansPerSecond.x * radiansPerSecond.x
        + radiansPerSecond.y * radiansPerSecond.y
        + radiansPerSecond.z * radiansPerSecond.z);
    if (speed <= 0.000001f)
        return rotation;

    const float angle = speed * fixedDeltaTime;
    const float halfAngle = angle * 0.5f;
    const float axisScale = std::sin(halfAngle) / speed;
    const Quaternion delta{
        std::cos(halfAngle),
        radiansPerSecond.x * axisScale,
        radiansPerSecond.y * axisScale,
        radiansPerSecond.z * axisScale
    };

    // Angular velocity is expressed in world space.
    return normalized(multiply(delta, rotation));
}

RigidBodySystem::Quaternion RigidBodySystem::conjugate(
    const Quaternion& value)
{
    return { value.w, -value.x, -value.y, -value.z };
}

heritage::math::Vec3 RigidBodySystem::rotateVector(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    const heritage::math::Vec3 q{ rotation.x, rotation.y, rotation.z };
    const heritage::math::Vec3 twiceCross = scaleVector(cross(q, value), 2.0f);
    return add(
        value,
        add(
            scaleVector(twiceCross, rotation.w),
            cross(q, twiceCross)));
}

heritage::math::Vec3 RigidBodySystem::applyWorldInverseInertia(
    const Record& body,
    const heritage::math::Vec3& worldVector)
{
    if (body.motionType != BodyMotionType::Dynamic)
        return {};

    const heritage::math::Vec3 localVector = rotateVector(
        conjugate(body.rotation),
        worldVector);
    const heritage::math::Vec3 localResult = multiplyComponents(
        localVector,
        body.inverseInertiaLocal);
    return rotateVector(body.rotation, localResult);
}

void RigidBodySystem::applyImpulseToRecord(
    Record& body,
    const heritage::math::Vec3& impulse,
    const heritage::math::Vec3& worldPoint)
{
    if (body.motionType != BodyMotionType::Dynamic)
        return;

    body.linearVelocity = add(
        body.linearVelocity,
        scaleVector(impulse, body.inverseMass));

    const heritage::math::Vec3 leverArm = subtract(worldPoint, body.position);
    const heritage::math::Vec3 angularImpulse = cross(leverArm, impulse);
    const heritage::math::Vec3 deltaRadiansPerSecond =
        applyWorldInverseInertia(body, angularImpulse);
    body.angularVelocityDegrees = add(
        body.angularVelocityDegrees,
        {
            degrees(deltaRadiansPerSecond.x),
            degrees(deltaRadiansPerSecond.y),
            degrees(deltaRadiansPerSecond.z)
        });
}

void RigidBodySystem::wakeRecord(Record& body)
{
    if (body.motionType != BodyMotionType::Dynamic)
    {
        body.sleeping = false;
        body.sleepTimer = 0.0f;
        return;
    }

    body.sleeping = false;
    body.sleepTimer = 0.0f;
}

void RigidBodySystem::sleepRecord(Record& body)
{
    if (body.motionType != BodyMotionType::Dynamic || !body.allowSleep)
        return;

    body.sleeping = true;
    body.sleepTimer = 0.0f;
    body.linearVelocity = {};
    body.angularVelocityDegrees = {};
    body.accumulatedForce = {};
    body.previousPosition = body.position;
    body.previousRotation = body.rotation;
}

void RigidBodySystem::setError(const std::string& message) const
{
    m_lastError = message;
}

void RigidBodySystem::clearError() const
{
    m_lastError.clear();
}

const char* bodyMotionTypeName(BodyMotionType value)
{
    switch (value)
    {
    case BodyMotionType::Static:
        return "static";
    case BodyMotionType::Kinematic:
        return "kinematic";
    case BodyMotionType::Dynamic:
        return "dynamic";
    default:
        return "unknown";
    }
}

bool parseBodyMotionType(const std::string& text, BodyMotionType& value)
{
    const std::string normalized = lowercase(text);
    if (normalized == "static")
    {
        value = BodyMotionType::Static;
        return true;
    }
    if (normalized == "kinematic")
    {
        value = BodyMotionType::Kinematic;
        return true;
    }
    if (normalized == "dynamic")
    {
        value = BodyMotionType::Dynamic;
        return true;
    }
    return false;
}

} // namespace heritage::physics
