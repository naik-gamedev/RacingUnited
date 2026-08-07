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
    m_constraints.clear();
    m_collisions.clear();
    m_rigidBodies.clear();
    m_gravity = { 0.0f, -9.80665f, 0.0f };
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
        fixedDeltaTime,
        m_gravity);
    m_constraints.simulate(m_rigidBodies, fixedDeltaTime);
    m_rigidBodies.integrate(fixedDeltaTime, m_gravity);
    m_collisions.simulate(m_rigidBodies, fixedDeltaTime);

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
