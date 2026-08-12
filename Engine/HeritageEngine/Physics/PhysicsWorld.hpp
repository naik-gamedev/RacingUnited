#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "../Core/Math/Math.hpp"
#include "CollisionSystem.hpp"
#include "ConstraintSystem.hpp"
#include "RigidBodySystem.hpp"
#include "Surfaces/SurfaceWorld.hpp"
#include "../Vehicles/VehicleSystem.hpp"

namespace heritage::physics {

// Native fixed-step simulation world used by every future physics body,
// collider, vehicle and force-feedback system. Step 28E adds sleeping,
// simulation islands and persistent warm-started contacts. Step 28G adds
// swept-sphere queries and opt-in fast-motion protection without coupling the
// world clock to rendering. Step 28H adds generation-checked spring/damper
// constraints that apply equal and opposite forces at body anchors. Vehicle
// simulation remains on an independent deterministic high-rate clock; Step
// 29F adds generic collider surface identity/wetness that query hits pass to
// each tire independently.
class PhysicsWorld
{
public:
    using StepCallback = std::function<void(float fixedDeltaTime)>;

    PhysicsWorld();

    void reset();

    // Advances the accumulator by one rendered frame and invokes callback once
    // per fixed world step. engineAllowsSimulation is false while the engine
    // pause menu is open, preventing historical wall-clock time from being
    // accumulated and replayed after unpausing.
    void advance(
        double frameDeltaTime,
        bool engineAllowsSimulation,
        const StepCallback& callback);

    bool isAvailable() const { return true; }

    RigidBodySystem& rigidBodies() { return m_rigidBodies; }
    const RigidBodySystem& rigidBodies() const { return m_rigidBodies; }
    CollisionSystem& collisions() { return m_collisions; }
    const CollisionSystem& collisions() const { return m_collisions; }
    ConstraintSystem& constraints() { return m_constraints; }
    const ConstraintSystem& constraints() const { return m_constraints; }
    heritage::vehicles::VehicleSystem& vehicles() { return m_vehicles; }
    const heritage::vehicles::VehicleSystem& vehicles() const { return m_vehicles; }
    SurfaceWorld& surfaces() { return m_surfaces; }
    const SurfaceWorld& surfaces() const { return m_surfaces; }

    // Destroys attached colliders before invalidating the body handle.
    bool destroyBody(BodyHandle handle);

    // Writes interpolated native body poses to their bound entities once per
    // rendered frame. Bodies are simulated only inside fixed world steps.
    void synchronizeEntityTransforms(
        heritage::entities::EntityRegistry& entities);

    heritage::math::Vec3 gravity() const { return m_gravity; }
    void setGravity(const heritage::math::Vec3& gravity);

    // P64-02 large-world coordinate policy:
    // - m_globalOrigin is FP64 absolute truth.
    // - rigid bodies/collision/entities remain in a compact FP32 local frame.
    // - an anchor body may periodically move that local frame so important
    //   simulation stays numerically close to zero without changing the world.
    bool setFloatingOriginAnchor(
        BodyHandle body,
        float rebaseThresholdMeters = 4096.0f);
    void clearFloatingOriginAnchor();
    BodyHandle floatingOriginAnchor() const { return m_floatingOriginAnchor; }
    float floatingOriginThreshold() const { return m_floatingOriginThreshold; }
    heritage::math::DVec3 globalOrigin() const { return m_globalOrigin; }
    std::uint64_t originRebaseCount() const { return m_originRebaseCount; }
    heritage::math::DVec3 localToGlobal(
        const heritage::math::Vec3& localPosition) const;
    bool globalToLocal(
        const heritage::math::DVec3& globalPosition,
        heritage::math::Vec3& localPosition) const;
    bool bodyGlobalPosition(
        BodyHandle body,
        heritage::math::DVec3& globalPosition) const;
    bool setBodyGlobalPosition(
        BodyHandle body,
        const heritage::math::DVec3& globalPosition);
    bool resetWorldOrigin();

    float fixedDeltaTime() const { return static_cast<float>(m_fixedDeltaTime); }
    float tickRate() const;
    bool setTickRate(float hertz);

    float timeScale() const { return m_timeScale; }
    bool setTimeScale(float scale);

    bool paused() const { return m_paused; }
    void setPaused(bool paused) { m_paused = paused; }
    void requestSingleStep() { m_singleStepRequested = true; }

    // World-clock statistics. The older step/substep names remain as aliases
    // so existing Lua modules continue to work, but future vehicle substeps
    // will have their own independent counters and rates.
    std::uint64_t worldStepCount() const { return m_stepCount; }
    std::uint64_t stepCount() const { return worldStepCount(); }
    double simulationTime() const { return m_simulationTime; }
    float interpolationAlpha() const { return m_interpolationAlpha; }
    int lastWorldStepCount() const { return m_lastSubstepCount; }
    int lastSubstepCount() const { return lastWorldStepCount(); }

    int maximumWorldStepsPerFrame() const;
    int maximumSubsteps() const { return maximumWorldStepsPerFrame(); }
    int pendingWorldStepCount() const;
    double backlogTime() const { return m_accumulator; }
    double peakBacklogTime() const { return m_peakBacklogTime; }

    bool overloadedLastFrame() const { return m_overloadedLastFrame; }
    std::uint64_t overloadFrameCount() const { return m_overloadFrameCount; }

    // droppedSimulationTime is backlog deliberately discarded after the
    // bounded recovery queue fills. clampedSimulationTime is wall-clock time
    // ignored because a rendered frame exceeded the giant-stall limit.
    double droppedSimulationTime() const { return m_droppedSimulationTime; }
    double clampedSimulationTime() const { return m_clampedSimulationTime; }

    double lastFrameDeltaTime() const { return m_lastFrameDeltaTime; }
    double maximumCatchUpTime() const { return m_maximumCatchUpTime; }
    double maximumBacklogTime() const { return m_maximumBacklogTime; }

    // Resets accumulated simulation time/statistics while preserving gravity,
    // tick rate, time scale and pause state.
    void resetClock();

    const std::string& lastError() const;

private:
    void performStep(const StepCallback& callback);
    void updateFloatingOrigin();
    void applyLocalOriginShift(const heritage::math::Vec3& shift);
    void updateInterpolationAlpha();
    void discardWholeStepBacklog();
    void trimBacklogToLimit();

    RigidBodySystem m_rigidBodies;
    CollisionSystem m_collisions;
    ConstraintSystem m_constraints;
    SurfaceWorld m_surfaces;
    heritage::vehicles::VehicleSystem m_vehicles;
    heritage::math::Vec3 m_gravity{ 0.0f, -9.80665f, 0.0f };
    heritage::math::DVec3 m_globalOrigin{ 0.0, 0.0, 0.0 };
    heritage::math::Vec3 m_pendingEntityRebase{ 0.0f, 0.0f, 0.0f };
    BodyHandle m_floatingOriginAnchor = InvalidBody;
    float m_floatingOriginThreshold = 4096.0f;
    std::uint64_t m_originRebaseCount = 0;
    double m_fixedDeltaTime = 1.0 / 120.0;
    double m_accumulator = 0.0;
    double m_simulationTime = 0.0;
    double m_droppedSimulationTime = 0.0;
    double m_clampedSimulationTime = 0.0;
    double m_peakBacklogTime = 0.0;
    double m_lastFrameDeltaTime = 0.0;
    double m_maximumCatchUpTime = 0.10;
    double m_maximumBacklogTime = 0.20;
    float m_timeScale = 1.0f;
    float m_interpolationAlpha = 0.0f;
    std::uint64_t m_stepCount = 0;
    std::uint64_t m_overloadFrameCount = 0;
    int m_lastSubstepCount = 0;
    bool m_overloadedLastFrame = false;
    bool m_paused = false;
    bool m_singleStepRequested = false;
    std::string m_lastError;
};

} // namespace heritage::physics
