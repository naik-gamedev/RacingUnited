#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/Entities/EntityRegistry.hpp"
#include "../Core/Math/Math.hpp"

namespace heritage::physics {

class CollisionSystem;
class ConstraintSystem;

using BodyHandle = std::uint64_t;
inline constexpr BodyHandle InvalidBody = 0;

enum class BodyMotionType
{
    Static,
    Kinematic,
    Dynamic
};

struct RigidBodyDescription
{
    heritage::entities::EntityHandle entity = heritage::entities::InvalidEntity;
    BodyMotionType motionType = BodyMotionType::Dynamic;
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    float mass = 1.0f;
    float gravityFactor = 1.0f;
    float linearDamping = 0.02f;
    float angularDamping = 0.05f;
    bool continuousCollision = false;
};

struct RigidBodyPose
{
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
};

// Generation-checked, contiguous rigid-body storage. Primitive collider and
// contact ownership lives in CollisionSystem. Step 28E adds explicit sleep/wake
// state so quiet bodies can be skipped until forces, impulses, authored motion,
// or an active simulation island wakes them.
class RigidBodySystem
{
public:
    void clear();

    BodyHandle create(const RigidBodyDescription& description);
    bool destroy(BodyHandle handle);
    bool exists(BodyHandle handle) const;
    std::size_t count() const { return m_aliveCount; }

    BodyHandle bodyForEntity(heritage::entities::EntityHandle entity) const;
    heritage::entities::EntityHandle entityForBody(BodyHandle handle) const;

    bool motionType(BodyHandle handle, BodyMotionType& value) const;
    bool setMotionType(BodyHandle handle, BodyMotionType value);

    bool mass(BodyHandle handle, float& value) const;
    bool setMass(BodyHandle handle, float value);

    bool gravityFactor(BodyHandle handle, float& value) const;
    bool setGravityFactor(BodyHandle handle, float value);

    bool linearDamping(BodyHandle handle, float& value) const;
    bool setLinearDamping(BodyHandle handle, float value);
    bool angularDamping(BodyHandle handle, float& value) const;
    bool setAngularDamping(BodyHandle handle, float value);

    bool continuousCollision(BodyHandle handle, bool& value) const;
    bool setContinuousCollision(BodyHandle handle, bool enabled);

    bool pose(BodyHandle handle, RigidBodyPose& value) const;
    bool interpolatedPose(BodyHandle handle, float alpha, RigidBodyPose& value) const;
    bool setPose(BodyHandle handle, const RigidBodyPose& value);
    bool setPosition(BodyHandle handle, const heritage::math::Vec3& value);
    bool setRotationDegrees(BodyHandle handle, const heritage::math::Vec3& value);

    bool linearVelocity(BodyHandle handle, heritage::math::Vec3& value) const;
    bool setLinearVelocity(BodyHandle handle, const heritage::math::Vec3& value);
    bool angularVelocityDegrees(BodyHandle handle, heritage::math::Vec3& value) const;
    bool setAngularVelocityDegrees(BodyHandle handle, const heritage::math::Vec3& value);

    bool applyForce(BodyHandle handle, const heritage::math::Vec3& force);
    bool applyLinearImpulse(BodyHandle handle, const heritage::math::Vec3& impulse);
    bool applyImpulseAtPoint(
        BodyHandle handle,
        const heritage::math::Vec3& impulse,
        const heritage::math::Vec3& worldPoint);
    bool applyAngularImpulse(
        BodyHandle handle,
        const heritage::math::Vec3& angularImpulse);
    bool clearForces(BodyHandle handle);

    bool sleeping(BodyHandle handle, bool& value) const;
    bool setSleeping(BodyHandle handle, bool sleeping);
    bool allowSleep(BodyHandle handle, bool& value) const;
    bool setAllowSleep(BodyHandle handle, bool allowSleep);
    bool wake(BodyHandle handle);
    void wakeAll();
    std::size_t sleepingCount() const;
    std::size_t activeDynamicCount() const;

    void integrate(float fixedDeltaTime, const heritage::math::Vec3& gravity);
    void snapInterpolation();

    // Removes bodies whose bound entities no longer exist, then writes an
    // interpolated body pose to every surviving bound entity. This keeps the
    // render rate independent from the fixed simulation rate.
    void synchronizeEntities(
        heritage::entities::EntityRegistry& entities,
        float interpolationAlpha);

    const std::string& lastError() const { return m_lastError; }

private:
    friend class CollisionSystem;
    friend class ConstraintSystem;
class ConstraintSystem;

    struct Quaternion
    {
        float w = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Record
    {
        heritage::entities::EntityHandle entity = heritage::entities::InvalidEntity;
        BodyMotionType motionType = BodyMotionType::Dynamic;
        heritage::math::Vec3 previousPosition{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
        Quaternion previousRotation;
        Quaternion rotation;
        heritage::math::Vec3 linearVelocity{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 angularVelocityDegrees{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 accumulatedForce{ 0.0f, 0.0f, 0.0f };
        // Diagonal inverse inertia in body-local space. CollisionSystem rebuilds
        // this from all attached primitive colliders whenever topology or mass
        // changes. World-space impulses rotate through this tensor.
        heritage::math::Vec3 inverseInertiaLocal{ 1.0f, 1.0f, 1.0f };
        float mass = 1.0f;
        float inverseMass = 1.0f;
        float gravityFactor = 1.0f;
        float linearDamping = 0.02f;
        float angularDamping = 0.05f;
        float sleepTimer = 0.0f;
        bool allowSleep = true;
        bool sleeping = false;
        bool continuousCollision = false;
    };

    struct Slot
    {
        std::uint32_t generation = 1;
        bool alive = false;
        Record record;
    };

    static BodyHandle makeHandle(std::uint32_t index, std::uint32_t generation);
    static bool decodeHandle(
        BodyHandle handle,
        std::uint32_t& index,
        std::uint32_t& generation);

    Slot* resolve(BodyHandle handle);
    const Slot* resolve(BodyHandle handle) const;

    bool destroyResolved(std::uint32_t index, Slot& slot);

    static Quaternion quaternionFromEulerDegrees(const heritage::math::Vec3& value);
    static heritage::math::Vec3 eulerDegreesFromQuaternion(const Quaternion& value);
    static Quaternion multiply(const Quaternion& left, const Quaternion& right);
    static Quaternion normalized(const Quaternion& value);
    static Quaternion interpolate(
        const Quaternion& previous,
        const Quaternion& current,
        float alpha);
    static Quaternion integrateAngularVelocity(
        const Quaternion& rotation,
        const heritage::math::Vec3& angularVelocityDegrees,
        float fixedDeltaTime);
    static Quaternion conjugate(const Quaternion& value);
    static heritage::math::Vec3 rotateVector(
        const Quaternion& rotation,
        const heritage::math::Vec3& value);
    static heritage::math::Vec3 applyWorldInverseInertia(
        const Record& body,
        const heritage::math::Vec3& worldVector);
    static void applyImpulseToRecord(
        Record& body,
        const heritage::math::Vec3& impulse,
        const heritage::math::Vec3& worldPoint);
    static void wakeRecord(Record& body);
    static void sleepRecord(Record& body);

    void setError(const std::string& message) const;
    void clearError() const;

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeIndices;
    std::unordered_map<heritage::entities::EntityHandle, BodyHandle> m_bodyByEntity;
    std::size_t m_aliveCount = 0;
    // Incremented only when mass-relevant body state changes. CollisionSystem
    // uses it to rebuild inertia once, not once per fixed step.
    std::uint64_t m_massPropertiesRevision = 1;
    mutable std::string m_lastError;
};

const char* bodyMotionTypeName(BodyMotionType value);
bool parseBodyMotionType(const std::string& text, BodyMotionType& value);

} // namespace heritage::physics
