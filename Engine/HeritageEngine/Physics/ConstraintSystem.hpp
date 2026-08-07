#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"
#include "RigidBodySystem.hpp"

namespace heritage::physics {

using ConstraintHandle = std::uint64_t;
inline constexpr ConstraintHandle InvalidConstraint = 0;

// A force-based spring/damper between an anchor on body A and either an anchor
// on body B or a fixed world-space anchor when bodyB is InvalidBody. This is a
// general engine primitive for suspension prototypes, trailer couplings,
// engine mounts and other compliant links. The later Vehicle API will run its
// dedicated tire/suspension solver at a higher rate, but it uses the same
// force-at-point mechanics introduced here.
struct SpringConstraintDescription
{
    BodyHandle bodyA = InvalidBody;
    BodyHandle bodyB = InvalidBody;
    heritage::math::Vec3 localAnchorA{ 0.0f, 0.0f, 0.0f };
    // Interpreted in body-B local space when bodyB is valid, otherwise as a
    // fixed world-space anchor.
    heritage::math::Vec3 anchorB{ 0.0f, 0.0f, 0.0f };
    float restLength = 1.0f;
    float stiffness = 1000.0f;
    float damping = 100.0f;
    float maximumForce = 1000000.0f;
    bool enabled = true;
};

struct SpringConstraintState
{
    float currentLength = 0.0f;
    float extension = 0.0f;
    float relativeSpeed = 0.0f;
    float appliedForce = 0.0f;
};

class ConstraintSystem
{
public:
    void clear();

    ConstraintHandle createSpring(
        const SpringConstraintDescription& description,
        const RigidBodySystem& bodies);
    bool destroy(ConstraintHandle handle);
    bool exists(ConstraintHandle handle) const;
    std::size_t count() const { return m_aliveCount; }
    std::size_t enabledCount() const;
    std::size_t activeCount() const { return m_activeCount; }

    void destroyForBody(BodyHandle body);
    void removeInvalidBodies(const RigidBodySystem& bodies);

    bool setEnabled(ConstraintHandle handle, bool enabled, RigidBodySystem& bodies);
    bool enabled(ConstraintHandle handle, bool& value) const;
    bool setSpringProperties(
        ConstraintHandle handle,
        float restLength,
        float stiffness,
        float damping,
        float maximumForce,
        RigidBodySystem& bodies);
    bool state(ConstraintHandle handle, SpringConstraintState& value) const;
    BodyHandle bodyA(ConstraintHandle handle) const;
    BodyHandle bodyB(ConstraintHandle handle) const;

    // Applies one deterministic impulse equal to force * fixedDeltaTime at both
    // anchor points. Called before rigid-body integration in each fixed step.
    void simulate(RigidBodySystem& bodies, float fixedDeltaTime);

    const std::string& lastError() const { return m_lastError; }

private:
    struct Record
    {
        BodyHandle bodyA = InvalidBody;
        BodyHandle bodyB = InvalidBody;
        heritage::math::Vec3 localAnchorA{};
        heritage::math::Vec3 anchorB{};
        float restLength = 1.0f;
        float stiffness = 1000.0f;
        float damping = 100.0f;
        float maximumForce = 1000000.0f;
        bool enabled = true;
        SpringConstraintState state;
    };

    struct Slot
    {
        std::uint32_t generation = 1;
        bool alive = false;
        Record record;
    };

    static ConstraintHandle makeHandle(std::uint32_t index, std::uint32_t generation);
    static bool decodeHandle(
        ConstraintHandle handle,
        std::uint32_t& index,
        std::uint32_t& generation);
    Slot* resolve(ConstraintHandle handle);
    const Slot* resolve(ConstraintHandle handle) const;
    bool destroyResolved(std::uint32_t index, Slot& slot);

    static heritage::math::Vec3 pointVelocity(
        const RigidBodySystem::Record& body,
        const heritage::math::Vec3& worldPoint);
    void wakeBodies(Record& constraint, RigidBodySystem& bodies);
    void setError(const std::string& message) const;
    void clearError() const;

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeIndices;
    std::size_t m_aliveCount = 0;
    std::size_t m_activeCount = 0;
    mutable std::string m_lastError;
};

} // namespace heritage::physics
