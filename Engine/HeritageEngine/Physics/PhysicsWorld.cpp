#include "PhysicsWorld.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::physics {
namespace {

// A rendered frame may contribute at most 100 ms of wall-clock time. This
// keeps debugger breaks, window dragging and operating-system stalls from
// becoming a huge burst of historical physics work.
constexpr double kMaximumAcceptedFrameDelta = 0.10;
constexpr float kMinimumTickRate = 30.0f;
constexpr float kMaximumTickRate = 500.0f;
constexpr float kMinimumTimeScale = 0.0f;
constexpr float kMaximumTimeScale = 4.0f;
constexpr int kAbsoluteMaximumWorldStepsPerFrame = 256;
constexpr double kStepComparisonEpsilon = 1.0e-12;
constexpr float kMinimumFloatingOriginThreshold = 32.0f;
constexpr float kMaximumFloatingOriginThreshold = 1000000.0f;

bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

} // namespace

PhysicsWorld::PhysicsWorld()
{
    reset();
}

void PhysicsWorld::reset()
{
    m_vehicles.clear();
    m_surfaces.clear();
    m_surfaces.setGlobalOrigin({ 0.0, 0.0, 0.0 });
    m_constraints.clear();
    m_collisions.clear();
    m_rigidBodies.clear();
    m_gravity = { 0.0f, -9.80665f, 0.0f };
    m_globalOrigin = { 0.0, 0.0, 0.0 };
    m_pendingEntityRebase = { 0.0f, 0.0f, 0.0f };
    m_floatingOriginAnchor = InvalidBody;
    m_floatingOriginThreshold = 4096.0f;
    m_originRebaseCount = 0;
    m_fixedDeltaTime = 1.0 / 120.0;
    m_timeScale = 1.0f;
    m_paused = false;
    m_singleStepRequested = false;
    m_lastError.clear();
    resetClock();
}

void PhysicsWorld::resetClock()
{
    m_rigidBodies.snapInterpolation();
    m_vehicles.resetClock();
    m_accumulator = 0.0;
    m_simulationTime = 0.0;
    m_droppedSimulationTime = 0.0;
    m_clampedSimulationTime = 0.0;
    m_peakBacklogTime = 0.0;
    m_lastFrameDeltaTime = 0.0;
    m_interpolationAlpha = 0.0f;
    m_stepCount = 0;
    m_overloadFrameCount = 0;
    m_lastSubstepCount = 0;
    m_overloadedLastFrame = false;
    m_singleStepRequested = false;
}

void PhysicsWorld::advance(
    double frameDeltaTime,
    bool engineAllowsSimulation,
    const StepCallback& callback)
{
    m_lastSubstepCount = 0;
    m_overloadedLastFrame = false;

    if (!std::isfinite(frameDeltaTime) || frameDeltaTime < 0.0)
        frameDeltaTime = 0.0;

    m_lastFrameDeltaTime = (std::min)(frameDeltaTime, kMaximumAcceptedFrameDelta);

    if (engineAllowsSimulation && m_singleStepRequested)
    {
        m_singleStepRequested = false;
        m_accumulator = 0.0;
        performStep(callback);
        m_rigidBodies.snapInterpolation();
        m_interpolationAlpha = 0.0f;
        return;
    }

    if (!engineAllowsSimulation || m_paused || m_timeScale <= 0.0f)
    {
        // Do not retain whole historical steps while paused. A fractional
        // remainder is harmless and keeps interpolation stable.
        discardWholeStepBacklog();
        updateInterpolationAlpha();
        return;
    }

    if (frameDeltaTime > kMaximumAcceptedFrameDelta)
    {
        m_clampedSimulationTime +=
            (frameDeltaTime - kMaximumAcceptedFrameDelta)
            * static_cast<double>(m_timeScale);
        m_overloadedLastFrame = true;
    }

    m_accumulator +=
        m_lastFrameDeltaTime * static_cast<double>(m_timeScale);
    m_peakBacklogTime = (std::max)(m_peakBacklogTime, m_accumulator);

    const int stepBudget = maximumWorldStepsPerFrame();
    while (m_accumulator + kStepComparisonEpsilon >= m_fixedDeltaTime
        && m_lastSubstepCount < stepBudget)
    {
        m_accumulator -= m_fixedDeltaTime;
        if (m_accumulator < 0.0)
            m_accumulator = 0.0;
        performStep(callback);
    }

    // If at least one complete world step is still queued, the render frame
    // exhausted its time-based catch-up allowance. Keep a bounded recovery
    // queue so a brief hitch can be recovered over following frames, but never
    // permit an unbounded spiral of death.
    if (m_accumulator + kStepComparisonEpsilon >= m_fixedDeltaTime)
    {
        m_overloadedLastFrame = true;
        trimBacklogToLimit();
    }

    if (m_overloadedLastFrame)
        ++m_overloadFrameCount;

    updateInterpolationAlpha();
}


void PhysicsWorld::synchronizeEntityTransforms(
    heritage::entities::EntityRegistry& entities)
{
    if (m_pendingEntityRebase.x != 0.0f
        || m_pendingEntityRebase.y != 0.0f
        || m_pendingEntityRebase.z != 0.0f)
    {
        entities.rebaseRootPositions(m_pendingEntityRebase);
        m_pendingEntityRebase = { 0.0f, 0.0f, 0.0f };
    }

    m_rigidBodies.synchronizeEntities(entities, m_interpolationAlpha);
    m_vehicles.removeInvalidBodies(m_rigidBodies);
    m_constraints.removeInvalidBodies(m_rigidBodies);
    m_collisions.removeInvalidBodies(m_rigidBodies);
}

bool PhysicsWorld::destroyBody(BodyHandle handle)
{
    if (!m_rigidBodies.exists(handle))
        return false;
    m_vehicles.destroyForBody(handle);
    m_constraints.destroyForBody(handle);
    m_collisions.destroyForBody(handle);
    return m_rigidBodies.destroy(handle);
}

const std::string& PhysicsWorld::lastError() const
{
    if (!m_lastError.empty())
        return m_lastError;
    if (!m_vehicles.lastError().empty())
        return m_vehicles.lastError();
    if (!m_constraints.lastError().empty())
        return m_constraints.lastError();
    if (!m_collisions.lastError().empty())
        return m_collisions.lastError();
    return m_rigidBodies.lastError();
}

void PhysicsWorld::setGravity(const heritage::math::Vec3& gravity)
{
    if (!finiteFloat(gravity.x)
        || !finiteFloat(gravity.y)
        || !finiteFloat(gravity.z))
    {
        m_lastError = "Physics gravity values must be finite numbers.";
        return;
    }

    m_gravity = gravity;
    m_rigidBodies.wakeAll();
    m_lastError.clear();
}

bool PhysicsWorld::setFloatingOriginAnchor(
    BodyHandle body,
    float rebaseThresholdMeters)
{
    if (!m_rigidBodies.exists(body))
    {
        m_lastError = "Floating-origin anchor requires a valid rigid body.";
        return false;
    }
    if (!finiteFloat(rebaseThresholdMeters)
        || rebaseThresholdMeters < kMinimumFloatingOriginThreshold
        || rebaseThresholdMeters > kMaximumFloatingOriginThreshold)
    {
        m_lastError = "Floating-origin threshold must be between 32 and 1,000,000 metres.";
        return false;
    }

    m_floatingOriginAnchor = body;
    m_floatingOriginThreshold = rebaseThresholdMeters;
    m_lastError.clear();
    return true;
}

void PhysicsWorld::clearFloatingOriginAnchor()
{
    m_floatingOriginAnchor = InvalidBody;
    m_lastError.clear();
}

heritage::math::DVec3 PhysicsWorld::localToGlobal(
    const heritage::math::Vec3& localPosition) const
{
    return {
        m_globalOrigin.x + static_cast<double>(localPosition.x),
        m_globalOrigin.y + static_cast<double>(localPosition.y),
        m_globalOrigin.z + static_cast<double>(localPosition.z)
    };
}

bool PhysicsWorld::globalToLocal(
    const heritage::math::DVec3& globalPosition,
    heritage::math::Vec3& localPosition) const
{
    const double x = globalPosition.x - m_globalOrigin.x;
    const double y = globalPosition.y - m_globalOrigin.y;
    const double z = globalPosition.z - m_globalOrigin.z;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || std::abs(x) > static_cast<double>((std::numeric_limits<float>::max)())
        || std::abs(y) > static_cast<double>((std::numeric_limits<float>::max)())
        || std::abs(z) > static_cast<double>((std::numeric_limits<float>::max)()))
    {
        return false;
    }

    localPosition = {
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(z)
    };
    return true;
}

bool PhysicsWorld::bodyGlobalPosition(
    BodyHandle body,
    heritage::math::DVec3& globalPosition) const
{
    RigidBodyPose pose;
    if (!m_rigidBodies.pose(body, pose))
        return false;
    globalPosition = localToGlobal(pose.position);
    return true;
}

bool PhysicsWorld::setBodyGlobalPosition(
    BodyHandle body,
    const heritage::math::DVec3& globalPosition)
{
    heritage::math::Vec3 local{};
    if (!globalToLocal(globalPosition, local))
    {
        m_lastError = "Global body position is outside the current local FP32 frame.";
        return false;
    }
    if (!m_rigidBodies.setPosition(body, local))
        return false;
    m_lastError.clear();
    return true;
}

void PhysicsWorld::applyLocalOriginShift(
    const heritage::math::Vec3& shift)
{
    if (shift.x == 0.0f && shift.y == 0.0f && shift.z == 0.0f)
        return;

    m_rigidBodies.rebaseLocalOrigin(shift);
    m_constraints.rebaseLocalOrigin(shift);
    m_collisions.rebaseLocalOrigin(shift);

    // EntityRegistry is owned by the module runtime, not PhysicsWorld. Queue
    // the same shift and apply it once before the next render synchronization.
    m_pendingEntityRebase.x += shift.x;
    m_pendingEntityRebase.y += shift.y;
    m_pendingEntityRebase.z += shift.z;

    m_globalOrigin.x += static_cast<double>(shift.x);
    m_globalOrigin.y += static_cast<double>(shift.y);
    m_globalOrigin.z += static_cast<double>(shift.z);
    m_surfaces.setGlobalOrigin(m_globalOrigin);
    ++m_originRebaseCount;
}

void PhysicsWorld::updateFloatingOrigin()
{
    if (m_floatingOriginAnchor == InvalidBody)
        return;
    if (!m_rigidBodies.exists(m_floatingOriginAnchor))
    {
        m_floatingOriginAnchor = InvalidBody;
        return;
    }

    RigidBodyPose pose;
    if (!m_rigidBodies.pose(m_floatingOriginAnchor, pose))
        return;

    heritage::math::Vec3 shift{};
    if (std::abs(pose.position.x) >= m_floatingOriginThreshold)
        shift.x = pose.position.x;
    if (std::abs(pose.position.y) >= m_floatingOriginThreshold)
        shift.y = pose.position.y;
    if (std::abs(pose.position.z) >= m_floatingOriginThreshold)
        shift.z = pose.position.z;

    applyLocalOriginShift(shift);
}

bool PhysicsWorld::resetWorldOrigin()
{
    if (m_globalOrigin.x == 0.0
        && m_globalOrigin.y == 0.0
        && m_globalOrigin.z == 0.0)
    {
        return true;
    }

    const heritage::math::DVec3 desiredShift{
        -m_globalOrigin.x,
        -m_globalOrigin.y,
        -m_globalOrigin.z
    };
    if (std::abs(desiredShift.x)
            > static_cast<double>((std::numeric_limits<float>::max)())
        || std::abs(desiredShift.y)
            > static_cast<double>((std::numeric_limits<float>::max)())
        || std::abs(desiredShift.z)
            > static_cast<double>((std::numeric_limits<float>::max)()))
    {
        m_lastError = "Current FP64 world origin cannot be represented as one local rebase shift.";
        return false;
    }

    applyLocalOriginShift({
        static_cast<float>(desiredShift.x),
        static_cast<float>(desiredShift.y),
        static_cast<float>(desiredShift.z)
    });
    // The accumulated origin is authoritative; scene reset wants exact authored
    // zero even if the final float shift rounded by a few ULPs.
    m_globalOrigin = { 0.0, 0.0, 0.0 };
    m_surfaces.setGlobalOrigin(m_globalOrigin);
    m_lastError.clear();
    return true;
}

float PhysicsWorld::tickRate() const
{
    return m_fixedDeltaTime > 0.0
        ? static_cast<float>(1.0 / m_fixedDeltaTime)
        : 0.0f;
}

bool PhysicsWorld::setTickRate(float hertz)
{
    if (!finiteFloat(hertz)
        || hertz < kMinimumTickRate
        || hertz > kMaximumTickRate)
    {
        m_lastError = "Physics world tick rate must be between 30 and 500 Hz.";
        return false;
    }

    m_fixedDeltaTime = 1.0 / static_cast<double>(hertz);
    m_rigidBodies.snapInterpolation();
    m_vehicles.resetClock();
    m_accumulator = 0.0;
    m_interpolationAlpha = 0.0f;
    m_lastError.clear();
    return true;
}

bool PhysicsWorld::setTimeScale(float scale)
{
    if (!finiteFloat(scale)
        || scale < kMinimumTimeScale
        || scale > kMaximumTimeScale)
    {
        m_lastError = "Physics time scale must be between 0 and 4.";
        return false;
    }

    m_timeScale = scale;
    m_lastError.clear();
    return true;
}

int PhysicsWorld::maximumWorldStepsPerFrame() const
{
    if (m_fixedDeltaTime <= 0.0)
        return 1;

    // Derive the step budget from simulation time, not from a hard-coded step
    // count. Changing 120 Hz to 240 Hz therefore doubles the step allowance
    // while preserving the same 100 ms catch-up window.
    const double rawBudget =
        std::ceil((m_maximumCatchUpTime / m_fixedDeltaTime) - 1.0e-9);
    return (std::max)(
        1,
        (std::min)(
            kAbsoluteMaximumWorldStepsPerFrame,
            static_cast<int>(rawBudget)));
}

int PhysicsWorld::pendingWorldStepCount() const
{
    if (m_fixedDeltaTime <= 0.0 || m_accumulator <= 0.0)
        return 0;

    const double pending =
        std::floor((m_accumulator + kStepComparisonEpsilon)
            / m_fixedDeltaTime);
    if (pending <= 0.0)
        return 0;
    if (pending >= static_cast<double>((std::numeric_limits<int>::max)()))
        return (std::numeric_limits<int>::max)();
    return static_cast<int>(pending);
}

void PhysicsWorld::performStep(const StepCallback& callback)
{
    const float fixedDeltaTime = static_cast<float>(m_fixedDeltaTime);

    // Module code runs at the start of a fixed step so forces and impulses
    // submitted from OnFixedUpdate affect this same native integration step.
    if (callback)
        callback(fixedDeltaTime);

    m_vehicles.simulate(
        m_rigidBodies,
        m_collisions,
        m_surfaces,
        fixedDeltaTime,
        m_gravity);
    m_constraints.simulate(m_rigidBodies, fixedDeltaTime);
    m_rigidBodies.integrate(fixedDeltaTime, m_gravity);
    m_collisions.simulate(m_rigidBodies, fixedDeltaTime);

    // Rebase only after the complete fixed-step solve so every body, contact,
    // constraint and tire sees one coherent coordinate frame during the step.
    updateFloatingOrigin();

    ++m_stepCount;
    ++m_lastSubstepCount;
    m_simulationTime += m_fixedDeltaTime;
}

void PhysicsWorld::updateInterpolationAlpha()
{
    if (m_fixedDeltaTime <= 0.0 || m_accumulator <= 0.0)
    {
        m_interpolationAlpha = 0.0f;
        return;
    }

    // The accumulator can intentionally contain complete queued steps during
    // overload recovery. Rendering only needs the fractional part between the
    // two most recent completed world states.
    double fractionalTime = std::fmod(m_accumulator, m_fixedDeltaTime);
    if (fractionalTime < 0.0)
        fractionalTime = 0.0;

    m_interpolationAlpha = static_cast<float>(
        (std::max)(0.0,
            (std::min)(1.0, fractionalTime / m_fixedDeltaTime)));
}

void PhysicsWorld::discardWholeStepBacklog()
{
    if (m_fixedDeltaTime <= 0.0 || m_accumulator < m_fixedDeltaTime)
        return;

    const double remainder = std::fmod(m_accumulator, m_fixedDeltaTime);
    const double discarded = m_accumulator - remainder;
    if (discarded > 0.0)
        m_droppedSimulationTime += discarded;
    m_accumulator = (std::max)(0.0, remainder);
}

void PhysicsWorld::trimBacklogToLimit()
{
    if (m_fixedDeltaTime <= 0.0
        || m_accumulator <= m_maximumBacklogTime)
    {
        return;
    }

    // Drop only complete fixed steps. Retaining the fractional phase keeps the
    // interpolation sequence deterministic and avoids a visual discontinuity.
    const double fractionalTime = std::fmod(m_accumulator, m_fixedDeltaTime);
    const double safeFraction = (std::max)(0.0, fractionalTime);
    const double wholeTime = m_accumulator - safeFraction;
    const double allowedWholeTime = (std::max)(
        0.0,
        m_maximumBacklogTime - safeFraction);
    const double allowedWholeSteps =
        std::floor((allowedWholeTime / m_fixedDeltaTime) + 1.0e-9);
    const double allowedTime =
        allowedWholeSteps * m_fixedDeltaTime;
    const double discarded = wholeTime - allowedTime;

    if (discarded > 0.0)
    {
        m_droppedSimulationTime += discarded;
        m_accumulator -= discarded;
        if (m_accumulator < 0.0)
            m_accumulator = 0.0;
    }
}

} // namespace heritage::physics
