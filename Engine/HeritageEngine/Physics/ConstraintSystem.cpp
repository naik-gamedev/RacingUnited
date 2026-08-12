#include "ConstraintSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::physics {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumLength = 0.0f;
constexpr float kMaximumLength = 1000000.0f;
constexpr float kMaximumStiffness = 1000000000.0f;
constexpr float kMaximumDamping = 1000000000.0f;
constexpr float kMaximumForceLimit = 1000000000000.0f;
constexpr float kLengthEpsilon = 0.000001f;
constexpr float kWakeForceThreshold = 0.01f;

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

heritage::math::Vec3 scale(
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

float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

float length(const heritage::math::Vec3& value)
{
    return std::sqrt(lengthSquared(value));
}

float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

bool validSpringValues(
    float restLength,
    float stiffness,
    float damping,
    float maximumForce)
{
    return finiteFloat(restLength)
        && restLength >= kMinimumLength
        && restLength <= kMaximumLength
        && finiteFloat(stiffness)
        && stiffness >= 0.0f
        && stiffness <= kMaximumStiffness
        && finiteFloat(damping)
        && damping >= 0.0f
        && damping <= kMaximumDamping
        && finiteFloat(maximumForce)
        && maximumForce >= 0.0f
        && maximumForce <= kMaximumForceLimit;
}

} // namespace

heritage::math::Vec3 ConstraintSystem::pointVelocity(
    const RigidBodySystem::Record& body,
    const heritage::math::Vec3& worldPoint)
{
    const heritage::math::Vec3 angularRadians{
        radians(body.angularVelocityDegrees.x),
        radians(body.angularVelocityDegrees.y),
        radians(body.angularVelocityDegrees.z)
    };
    return add(
        body.linearVelocity,
        cross(
            angularRadians,
            subtract(
                worldPoint,
                RigidBodySystem::worldCenterOfMass(body))));
}

void ConstraintSystem::clear()
{
    m_slots.clear();
    m_freeIndices.clear();
    m_aliveCount = 0;
    m_activeCount = 0;
    m_lastError.clear();
}

ConstraintHandle ConstraintSystem::createSpring(
    const SpringConstraintDescription& description,
    const RigidBodySystem& bodies)
{
    if (!bodies.resolve(description.bodyA))
    {
        setError("Physics.CreateSpringConstraint requires a valid body A.");
        return InvalidConstraint;
    }
    if (description.bodyB != InvalidBody
        && !bodies.resolve(description.bodyB))
    {
        setError("Physics.CreateSpringConstraint body B must be valid or zero for a world anchor.");
        return InvalidConstraint;
    }
    if (description.bodyA == description.bodyB)
    {
        setError("Physics.CreateSpringConstraint cannot connect a body to itself.");
        return InvalidConstraint;
    }
    if (!finiteVec3(description.localAnchorA)
        || !finiteVec3(description.anchorB))
    {
        setError("Physics.CreateSpringConstraint requires finite anchor values.");
        return InvalidConstraint;
    }
    if (!validSpringValues(
            description.restLength,
            description.stiffness,
            description.damping,
            description.maximumForce))
    {
        setError("Spring rest length, stiffness, damping or maximum force is outside the supported range.");
        return InvalidConstraint;
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
            setError("Constraint storage exhausted its handle index space.");
            return InvalidConstraint;
        }
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }

    Slot& slot = m_slots[index];
    slot.alive = true;
    slot.record = {};
    slot.record.bodyA = description.bodyA;
    slot.record.bodyB = description.bodyB;
    slot.record.localAnchorA = description.localAnchorA;
    slot.record.anchorB = description.anchorB;
    slot.record.restLength = description.restLength;
    slot.record.stiffness = description.stiffness;
    slot.record.damping = description.damping;
    slot.record.maximumForce = description.maximumForce;
    slot.record.enabled = description.enabled;

    ++m_aliveCount;
    clearError();
    return makeHandle(index, slot.generation);
}

bool ConstraintSystem::destroy(ConstraintHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        setError("Physics.DestroyConstraint received an invalid or stale constraint handle.");
        return false;
    }

    Slot& slot = m_slots[index];
    if (!slot.alive || slot.generation != generation)
    {
        setError("Physics.DestroyConstraint received an invalid or stale constraint handle.");
        return false;
    }
    return destroyResolved(index, slot);
}

bool ConstraintSystem::exists(ConstraintHandle handle) const
{
    const bool result = resolve(handle) != nullptr;
    if (result)
        clearError();
    return result;
}

std::size_t ConstraintSystem::enabledCount() const
{
    std::size_t result = 0;
    for (const Slot& slot : m_slots)
    {
        if (slot.alive && slot.record.enabled)
            ++result;
    }
    return result;
}

void ConstraintSystem::destroyForBody(BodyHandle body)
{
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        Slot& slot = m_slots[index];
        if (!slot.alive)
            continue;
        if (slot.record.bodyA == body || slot.record.bodyB == body)
            destroyResolved(index, slot);
    }
    clearError();
}

void ConstraintSystem::removeInvalidBodies(const RigidBodySystem& bodies)
{
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        Slot& slot = m_slots[index];
        if (!slot.alive)
            continue;
        if (!bodies.resolve(slot.record.bodyA)
            || (slot.record.bodyB != InvalidBody
                && !bodies.resolve(slot.record.bodyB)))
        {
            destroyResolved(index, slot);
        }
    }
    clearError();
}

bool ConstraintSystem::setEnabled(
    ConstraintHandle handle,
    bool value,
    RigidBodySystem& bodies)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetConstraintEnabled received an invalid or stale constraint handle.");
        return false;
    }
    slot->record.enabled = value;
    slot->record.state = {};
    if (value)
        wakeBodies(slot->record, bodies);
    clearError();
    return true;
}

bool ConstraintSystem::enabled(ConstraintHandle handle, bool& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetConstraintEnabled received an invalid or stale constraint handle.");
        return false;
    }
    value = slot->record.enabled;
    clearError();
    return true;
}

bool ConstraintSystem::setSpringProperties(
    ConstraintHandle handle,
    float restLength,
    float stiffness,
    float damping,
    float maximumForce,
    RigidBodySystem& bodies)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.SetSpringConstraintProperties received an invalid or stale constraint handle.");
        return false;
    }
    if (!validSpringValues(restLength, stiffness, damping, maximumForce))
    {
        setError("Spring rest length, stiffness, damping or maximum force is outside the supported range.");
        return false;
    }

    slot->record.restLength = restLength;
    slot->record.stiffness = stiffness;
    slot->record.damping = damping;
    slot->record.maximumForce = maximumForce;
    wakeBodies(slot->record, bodies);
    clearError();
    return true;
}

bool ConstraintSystem::state(
    ConstraintHandle handle,
    SpringConstraintState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetSpringConstraintState received an invalid or stale constraint handle.");
        return false;
    }
    value = slot->record.state;
    clearError();
    return true;
}

BodyHandle ConstraintSystem::bodyA(ConstraintHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetConstraintBodyA received an invalid or stale constraint handle.");
        return InvalidBody;
    }
    clearError();
    return slot->record.bodyA;
}

BodyHandle ConstraintSystem::bodyB(ConstraintHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Physics.GetConstraintBodyB received an invalid or stale constraint handle.");
        return InvalidBody;
    }
    clearError();
    return slot->record.bodyB;
}

void ConstraintSystem::rebaseLocalOrigin(
    const heritage::math::Vec3& shift)
{
    for (Slot& slot : m_slots)
    {
        if (!slot.alive || slot.record.bodyB != InvalidBody)
            continue;

        slot.record.anchorB.x -= shift.x;
        slot.record.anchorB.y -= shift.y;
        slot.record.anchorB.z -= shift.z;
    }

    clearError();
}

void ConstraintSystem::simulate(
    RigidBodySystem& bodies,
    float fixedDeltaTime)
{
    removeInvalidBodies(bodies);
    m_activeCount = 0;
    if (!finiteFloat(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
        return;

    for (Slot& slot : m_slots)
    {
        if (!slot.alive)
            continue;

        Record& constraint = slot.record;
        constraint.state = {};
        if (!constraint.enabled)
            continue;

        RigidBodySystem::Slot* bodySlotA = bodies.resolve(constraint.bodyA);
        RigidBodySystem::Slot* bodySlotB = constraint.bodyB != InvalidBody
            ? bodies.resolve(constraint.bodyB)
            : nullptr;
        if (!bodySlotA || (constraint.bodyB != InvalidBody && !bodySlotB))
            continue;

        RigidBodySystem::Record& bodyA = bodySlotA->record;
        RigidBodySystem::Record* bodyB = bodySlotB ? &bodySlotB->record : nullptr;

        const heritage::math::Vec3 worldAnchorA = add(
            bodyA.position,
            RigidBodySystem::rotateVector(bodyA.rotation, constraint.localAnchorA));
        const heritage::math::Vec3 worldAnchorB = bodyB
            ? add(
                bodyB->position,
                RigidBodySystem::rotateVector(bodyB->rotation, constraint.anchorB))
            : constraint.anchorB;

        const heritage::math::Vec3 delta = subtract(worldAnchorB, worldAnchorA);
        const float currentLength = length(delta);
        constraint.state.currentLength = currentLength;
        constraint.state.extension = currentLength - constraint.restLength;
        if (currentLength <= kLengthEpsilon)
            continue;

        const heritage::math::Vec3 direction = scale(delta, 1.0f / currentLength);
        const heritage::math::Vec3 velocityA = pointVelocity(bodyA, worldAnchorA);
        const heritage::math::Vec3 velocityB = bodyB
            ? pointVelocity(*bodyB, worldAnchorB)
            : heritage::math::Vec3{};
        const float relativeSpeed = dot(subtract(velocityB, velocityA), direction);
        constraint.state.relativeSpeed = relativeSpeed;

        float force = constraint.stiffness * constraint.state.extension
            + constraint.damping * relativeSpeed;
        force = std::clamp(force, -constraint.maximumForce, constraint.maximumForce);
        constraint.state.appliedForce = force;

        const bool dynamicA = bodyA.motionType == BodyMotionType::Dynamic;
        const bool dynamicB = bodyB
            && bodyB->motionType == BodyMotionType::Dynamic;
        if (!dynamicA && !dynamicB)
            continue;

        // A fully sleeping connected pair stays frozen until an authored change
        // or external force wakes it. This allows parked/suspended assemblies to
        // stop consuming CPU after settling.
        if ((!dynamicA || bodyA.sleeping)
            && (!dynamicB || bodyB->sleeping))
        {
            continue;
        }

        if (std::abs(force) > kWakeForceThreshold)
        {
            if (dynamicA && bodyA.sleeping)
                RigidBodySystem::wakeRecord(bodyA);
            if (dynamicB && bodyB->sleeping)
                RigidBodySystem::wakeRecord(*bodyB);
        }

        const heritage::math::Vec3 impulse = scale(
            direction,
            force * fixedDeltaTime);
        if (dynamicA && !bodyA.sleeping)
            RigidBodySystem::applyImpulseToRecord(bodyA, impulse, worldAnchorA);
        if (dynamicB && !bodyB->sleeping)
        {
            RigidBodySystem::applyImpulseToRecord(
                *bodyB,
                scale(impulse, -1.0f),
                worldAnchorB);
        }
        ++m_activeCount;
    }

    clearError();
}

ConstraintHandle ConstraintSystem::makeHandle(
    std::uint32_t index,
    std::uint32_t generation)
{
    return (static_cast<ConstraintHandle>(generation) << 32u)
        | static_cast<ConstraintHandle>(index + 1u);
}

bool ConstraintSystem::decodeHandle(
    ConstraintHandle handle,
    std::uint32_t& index,
    std::uint32_t& generation)
{
    if (handle == InvalidConstraint)
        return false;
    const std::uint32_t encodedIndex = static_cast<std::uint32_t>(
        handle & 0xffffffffull);
    generation = static_cast<std::uint32_t>(handle >> 32u);
    if (encodedIndex == 0 || generation == 0)
        return false;
    index = encodedIndex - 1u;
    return true;
}

ConstraintSystem::Slot* ConstraintSystem::resolve(ConstraintHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
        return nullptr;
    Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

const ConstraintSystem::Slot* ConstraintSystem::resolve(
    ConstraintHandle handle) const
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
        return nullptr;
    const Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

bool ConstraintSystem::destroyResolved(std::uint32_t index, Slot& slot)
{
    slot.alive = false;
    slot.record = {};
    ++slot.generation;
    if (slot.generation == 0)
        slot.generation = 1;
    m_freeIndices.push_back(index);
    if (m_aliveCount > 0)
        --m_aliveCount;
    clearError();
    return true;
}

void ConstraintSystem::wakeBodies(
    Record& constraint,
    RigidBodySystem& bodies)
{
    if (RigidBodySystem::Slot* body = bodies.resolve(constraint.bodyA))
        RigidBodySystem::wakeRecord(body->record);
    if (constraint.bodyB != InvalidBody)
    {
        if (RigidBodySystem::Slot* body = bodies.resolve(constraint.bodyB))
            RigidBodySystem::wakeRecord(body->record);
    }
}

void ConstraintSystem::setError(const std::string& message) const
{
    m_lastError = message;
}

void ConstraintSystem::clearError() const
{
    m_lastError.clear();
}

} // namespace heritage::physics
