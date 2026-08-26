#include "ChaseCamera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::camera {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kMinimumFrequencyHz = 0.01;
constexpr double kMaximumFrameStepSeconds = 1.0 / 120.0;
constexpr int kMaximumSpringSubsteps = 16;
constexpr double kVectorEpsilon = 1.0e-9;

bool finite(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

double distanceSquared(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    const double x = a.x - b.x;
    const double y = a.y - b.y;
    const double z = a.z - b.z;
    return x * x + y * y + z * z;
}

heritage::math::DVec3 subtract(
    const heritage::math::DVec3& a,
    const heritage::math::DVec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::DVec3 addScaled(
    const heritage::math::DVec3& origin,
    const heritage::math::DVec3& direction,
    double distance)
{
    return {
        origin.x + direction.x * distance,
        origin.y + direction.y * distance,
        origin.z + direction.z * distance
    };
}

double length(const heritage::math::DVec3& value)
{
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

double wrapDegrees(double value)
{
    value = std::fmod(value + 180.0, 360.0);
    if (value < 0.0)
        value += 360.0;
    return value - 180.0;
}

} // namespace

void ChaseCamera::reset()
{
    m_initialized = false;
    m_previousChassisGlobalPosition = { 0.0, 0.0, 0.0 };
    m_eyeGlobal = { 0.0, 0.0, 0.0 };
    m_desiredEyeGlobal = { 0.0, 0.0, 0.0 };
    m_targetGlobal = { 0.0, 1.0, 0.0 };
    m_collisionAnchorGlobal = { 0.0, 1.0, 0.0 };
    m_followYawDegrees = 0.0;
    m_followYawVelocity = 0.0;
    m_headingLagDegrees = 0.0;
    m_orbitYawDegrees = 0.0;
    m_orbitYawVelocity = 0.0;
    m_orbitPitchDegrees = 0.0;
    m_orbitPitchVelocity = 0.0;
    m_orbitReturnActive = false;
    m_verticalLagMeters = 0.0;
    m_verticalLagVelocity = 0.0;
    m_longitudinalDynamicOffsetMeters = 0.0;
    m_longitudinalDynamicOffsetVelocity = 0.0;
    m_lateralDynamicOffsetMeters = 0.0;
    m_lateralDynamicOffsetVelocity = 0.0;
    m_lateralFollowLagMeters = 0.0;
    m_lateralFollowLagVelocity = 0.0;
    m_previousForwardSpeedMetersPerSecond = 0.0;
    m_previousLateralSpeedMetersPerSecond = 0.0;
    m_haveDynamicMotionSample = false;
    m_collisionRayDistanceMeters = 0.0;
    m_collisionRayDistanceVelocity = 0.0;
}

void ChaseCamera::stepSpring(
    double& value,
    double& velocity,
    double target,
    double frequencyHz,
    double dampingRatio,
    double deltaSeconds)
{
    if (!std::isfinite(value)
        || !std::isfinite(velocity)
        || !std::isfinite(target))
    {
        value = target;
        velocity = 0.0;
        return;
    }

    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
        return;

    frequencyHz = std::max(frequencyHz, kMinimumFrequencyHz);
    dampingRatio = std::max(dampingRatio, 0.0);

    int substeps = static_cast<int>(std::ceil(
        deltaSeconds / kMaximumFrameStepSeconds));
    substeps = std::clamp(substeps, 1, kMaximumSpringSubsteps);
    const double step = deltaSeconds / static_cast<double>(substeps);
    const double omega = 2.0 * kPi * frequencyHz;
    const double stiffness = omega * omega;
    const double damping = 2.0 * dampingRatio * omega;

    for (int index = 0; index < substeps; ++index)
    {
        const double acceleration =
            stiffness * (target - value) - damping * velocity;
        velocity += acceleration * step;
        value += velocity * step;
    }
}

bool ChaseCamera::horizontalHeadingDegrees(
    const heritage::math::Vec3& forward,
    double& headingDegrees,
    heritage::math::Vec3& horizontalForward)
{
    if (!finite(forward))
        return false;

    const double x = static_cast<double>(forward.x);
    const double z = static_cast<double>(forward.z);
    const double horizontalLength = std::sqrt(x * x + z * z);
    if (horizontalLength <= kVectorEpsilon)
        return false;

    horizontalForward = {
        static_cast<float>(x / horizontalLength),
        0.0f,
        static_cast<float>(z / horizontalLength)
    };
    headingDegrees = std::atan2(x, z) * (180.0 / kPi);
    return std::isfinite(headingDegrees);
}

void ChaseCamera::snap(
    const heritage::math::DVec3& chassisGlobalPosition,
    const heritage::math::Vec3& horizontalForward,
    double chassisHeadingDegrees)
{
    m_previousChassisGlobalPosition = chassisGlobalPosition;
    m_followYawDegrees = chassisHeadingDegrees;
    m_followYawVelocity = 0.0;
    m_headingLagDegrees = 0.0;
    m_verticalLagMeters = 0.0;
    m_verticalLagVelocity = 0.0;
    m_longitudinalDynamicOffsetMeters = 0.0;
    m_longitudinalDynamicOffsetVelocity = 0.0;
    m_lateralDynamicOffsetMeters = 0.0;
    m_lateralDynamicOffsetVelocity = 0.0;
    m_lateralFollowLagMeters = 0.0;
    m_lateralFollowLagVelocity = 0.0;
    m_previousForwardSpeedMetersPerSecond = 0.0;
    m_previousLateralSpeedMetersPerSecond = 0.0;
    m_haveDynamicMotionSample = false;
    m_collisionRayDistanceMeters = 0.0;
    m_collisionRayDistanceVelocity = 0.0;
    m_initialized = true;
    rebuildDesiredPose(chassisGlobalPosition, horizontalForward);

    const double desiredDistance = length(subtract(
        m_desiredEyeGlobal,
        m_collisionAnchorGlobal));
    m_collisionRayDistanceMeters = desiredDistance;
    rebuildCollisionResolvedEye();
}

void ChaseCamera::rebuildDesiredPose(
    const heritage::math::DVec3& chassisGlobalPosition,
    const heritage::math::Vec3& horizontalForward)
{
    const double cameraYawRadians =
        (m_followYawDegrees + m_orbitYawDegrees) * (kPi / 180.0);
    const double cameraForwardX = std::sin(cameraYawRadians);
    const double cameraForwardZ = std::cos(cameraYawRadians);

    // Rotate the existing default eye/target separation on a sphere. At zero
    // orbit pitch this reproduces the original chase pose exactly.
    const double baseVerticalSeparation =
        m_tuning.eyeHeightMeters - m_tuning.targetHeightMeters;
    const double orbitRadius = std::hypot(
        m_tuning.distanceMeters,
        baseVerticalSeparation);
    const double baseElevationRadians = std::atan2(
        baseVerticalSeparation,
        m_tuning.distanceMeters);
    const double elevationRadians = baseElevationRadians
        + m_orbitPitchDegrees * (kPi / 180.0);
    const double horizontalDistance = std::max(
        0.25,
        std::cos(elevationRadians) * orbitRadius
            + m_longitudinalDynamicOffsetMeters);
    const double eyeHeight = m_tuning.targetHeightMeters
        + std::sin(elevationRadians) * orbitRadius;
    const double cameraRightX = std::cos(cameraYawRadians);
    const double cameraRightZ = -std::sin(cameraYawRadians);
    const double chassisRightX = static_cast<double>(horizontalForward.z);
    const double chassisRightZ = -static_cast<double>(horizontalForward.x);

    // CAM09 keeps the whole chase rig from inheriting lateral chassis
    // translation instantaneously. The follow-lag offset is applied equally
    // to eye and target (below), while the smaller acceleration sway remains
    // eye-only. The result is a real side-to-side positional damper rather
    // than merely a softened camera rotation.
    const double rigLateralOffsetX =
        chassisRightX * m_lateralFollowLagMeters;
    const double rigLateralOffsetZ =
        chassisRightZ * m_lateralFollowLagMeters;

    m_desiredEyeGlobal = {
        chassisGlobalPosition.x
            + rigLateralOffsetX
            - cameraForwardX * horizontalDistance
            + cameraRightX * m_lateralDynamicOffsetMeters,
        chassisGlobalPosition.y
            + eyeHeight
            + m_verticalLagMeters,
        chassisGlobalPosition.z
            + rigLateralOffsetZ
            - cameraForwardZ * horizontalDistance
            + cameraRightZ * m_lateralDynamicOffsetMeters
    };

    // A manually orbited view should look at the car rather than a point two
    // metres beyond it. Blend the normal chase look-ahead back in as the orbit
    // offset returns to centre.
    const double orbitInfluence = std::clamp(
        std::max(
            std::abs(m_orbitYawDegrees) / 25.0,
            std::abs(m_orbitPitchDegrees) / 15.0),
        0.0,
        1.0);
    const double lookAhead =
        m_tuning.lookAheadMeters * (1.0 - orbitInfluence);

    m_targetGlobal = {
        chassisGlobalPosition.x
            + rigLateralOffsetX
            + static_cast<double>(horizontalForward.x)
                * lookAhead,
        chassisGlobalPosition.y
            + m_tuning.targetHeightMeters
            + m_verticalLagMeters * m_tuning.targetVerticalResponse,
        chassisGlobalPosition.z
            + rigLateralOffsetZ
            + static_cast<double>(horizontalForward.z)
                * lookAhead
    };

    m_collisionAnchorGlobal = {
        chassisGlobalPosition.x,
        chassisGlobalPosition.y
            + m_tuning.collisionAnchorHeightMeters
            + m_verticalLagMeters * 0.15,
        chassisGlobalPosition.z
    };

    const double desiredDistance = length(subtract(
        m_desiredEyeGlobal,
        m_collisionAnchorGlobal));
    if (m_collisionRayDistanceMeters <= 0.0
        || !std::isfinite(m_collisionRayDistanceMeters))
    {
        m_collisionRayDistanceMeters = desiredDistance;
    }
    else
    {
        // Never let a stale collision distance extend beyond the newly desired
        // eye after a large heave/yaw change.
        m_collisionRayDistanceMeters = std::min(
            m_collisionRayDistanceMeters,
            desiredDistance);
    }

    rebuildCollisionResolvedEye();
}

void ChaseCamera::updateOrbit(
    const ChaseCameraInput& input,
    double deltaSeconds)
{
    const bool finitePointer = std::isfinite(input.pointerDeltaX)
        && std::isfinite(input.pointerDeltaY);
    if (input.orbitDragActive && finitePointer)
    {
        m_orbitYawDegrees = wrapDegrees(
            m_orbitYawDegrees
            + input.pointerDeltaX * m_tuning.orbitDegreesPerPixel);
        m_orbitPitchDegrees = std::clamp(
            m_orbitPitchDegrees
                - input.pointerDeltaY * m_tuning.orbitDegreesPerPixel,
            m_tuning.minimumOrbitPitchDegrees,
            m_tuning.maximumOrbitPitchDegrees);
        m_orbitYawVelocity = 0.0;
        m_orbitPitchVelocity = 0.0;
        m_orbitReturnActive = false;
        return;
    }

    if (std::isfinite(input.forwardSpeedMetersPerSecond)
        && input.forwardSpeedMetersPerSecond
            >= m_tuning.orbitReturnForwardSpeedMetersPerSecond
        && (std::abs(m_orbitYawDegrees) > 0.001
            || std::abs(m_orbitPitchDegrees) > 0.001))
    {
        // Latch the return so braking below the threshold cannot strand the
        // camera halfway between the inspected view and chase view.
        m_orbitReturnActive = true;
    }

    if (!m_orbitReturnActive)
        return;

    stepSpring(
        m_orbitYawDegrees,
        m_orbitYawVelocity,
        0.0,
        m_tuning.orbitReturnSpringFrequencyHz,
        m_tuning.orbitReturnDampingRatio,
        deltaSeconds);
    stepSpring(
        m_orbitPitchDegrees,
        m_orbitPitchVelocity,
        0.0,
        m_tuning.orbitReturnSpringFrequencyHz,
        m_tuning.orbitReturnDampingRatio,
        deltaSeconds);

    if (std::abs(m_orbitYawDegrees) < 0.01
        && std::abs(m_orbitPitchDegrees) < 0.01
        && std::abs(m_orbitYawVelocity) < 0.05
        && std::abs(m_orbitPitchVelocity) < 0.05)
    {
        m_orbitYawDegrees = 0.0;
        m_orbitYawVelocity = 0.0;
        m_orbitPitchDegrees = 0.0;
        m_orbitPitchVelocity = 0.0;
        m_orbitReturnActive = false;
    }
}

void ChaseCamera::updateDynamicMotion(
    const ChaseCameraInput& input,
    double deltaSeconds)
{
    double targetLongitudinalOffset = 0.0;
    double targetLateralOffset = 0.0;

    const bool validMotion = input.dynamicMotionResponseActive
        && std::isfinite(input.forwardSpeedMetersPerSecond)
        && std::isfinite(input.lateralSpeedMetersPerSecond)
        && std::isfinite(deltaSeconds)
        && deltaSeconds > 0.0;

    if (validMotion)
    {
        const double forwardSpeed = input.forwardSpeedMetersPerSecond;
        const double lateralSpeed = input.lateralSpeedMetersPerSecond;
        const double speedPullback = std::clamp(
            std::abs(forwardSpeed)
                * m_tuning.speedPullbackMetersPerMeterPerSecond,
            0.0,
            m_tuning.maximumSpeedPullbackMeters);
        targetLongitudinalOffset = speedPullback;

        if (m_haveDynamicMotionSample)
        {
            const double accelerationLimit = std::max(
                0.0,
                m_tuning.maximumSampleAccelerationMetersPerSecondSquared);
            const double forwardAcceleration = std::clamp(
                (forwardSpeed - m_previousForwardSpeedMetersPerSecond)
                    / deltaSeconds,
                -accelerationLimit,
                accelerationLimit);
            const double lateralAcceleration = std::clamp(
                (lateralSpeed - m_previousLateralSpeedMetersPerSecond)
                    / deltaSeconds,
                -accelerationLimit,
                accelerationLimit);

            const double accelerationOffset = std::clamp(
                forwardAcceleration * m_tuning.longitudinalAccelerationGain,
                -m_tuning.maximumBrakingPushForwardMeters,
                m_tuning.maximumAccelerationPullbackMeters);
            targetLongitudinalOffset += accelerationOffset;
            targetLateralOffset = std::clamp(
                -lateralAcceleration * m_tuning.lateralAccelerationGain,
                -m_tuning.maximumLateralSwayMeters,
                m_tuning.maximumLateralSwayMeters);
        }

        m_previousForwardSpeedMetersPerSecond = forwardSpeed;
        m_previousLateralSpeedMetersPerSecond = lateralSpeed;
        m_haveDynamicMotionSample = true;
    }
    else
    {
        // Do not manufacture a huge acceleration when simulation resumes from
        // pause or after camera authority changes. The existing offsets simply
        // settle back to neutral through the same camera damper.
        m_haveDynamicMotionSample = false;
    }

    stepSpring(
        m_longitudinalDynamicOffsetMeters,
        m_longitudinalDynamicOffsetVelocity,
        targetLongitudinalOffset,
        m_tuning.motionSpringFrequencyHz,
        m_tuning.motionDampingRatio,
        deltaSeconds);
    stepSpring(
        m_lateralDynamicOffsetMeters,
        m_lateralDynamicOffsetVelocity,
        targetLateralOffset,
        m_tuning.motionSpringFrequencyHz,
        m_tuning.motionDampingRatio,
        deltaSeconds);

    m_longitudinalDynamicOffsetMeters = std::clamp(
        m_longitudinalDynamicOffsetMeters,
        -m_tuning.maximumBrakingPushForwardMeters,
        m_tuning.maximumSpeedPullbackMeters
            + m_tuning.maximumAccelerationPullbackMeters);
    m_lateralDynamicOffsetMeters = std::clamp(
        m_lateralDynamicOffsetMeters,
        -m_tuning.maximumLateralSwayMeters,
        m_tuning.maximumLateralSwayMeters);
}

void ChaseCamera::rebuildCollisionResolvedEye()
{
    const heritage::math::DVec3 ray = subtract(
        m_desiredEyeGlobal,
        m_collisionAnchorGlobal);
    const double desiredDistance = length(ray);
    if (desiredDistance <= kVectorEpsilon)
    {
        m_eyeGlobal = m_collisionAnchorGlobal;
        return;
    }

    const double distance = std::clamp(
        m_collisionRayDistanceMeters,
        0.0,
        desiredDistance);
    const heritage::math::DVec3 direction{
        ray.x / desiredDistance,
        ray.y / desiredDistance,
        ray.z / desiredDistance
    };
    m_eyeGlobal = addScaled(m_collisionAnchorGlobal, direction, distance);
}

void ChaseCamera::update(
    const heritage::math::DVec3& chassisGlobalPosition,
    const heritage::math::Vec3& chassisForwardWorld,
    float deltaSeconds,
    const ChaseCameraInput& input)
{
    if (!finite(chassisGlobalPosition))
    {
        reset();
        return;
    }

    double rawChassisHeadingDegrees = 0.0;
    heritage::math::Vec3 horizontalForward{};
    if (!horizontalHeadingDegrees(
            chassisForwardWorld,
            rawChassisHeadingDegrees,
            horizontalForward))
    {
        // A vertical/invalid forward vector should never occur for a car, but
        // do not invent a random heading if a malformed pose reaches us.
        reset();
        return;
    }

    const double dt = std::clamp(
        static_cast<double>(deltaSeconds),
        0.0,
        0.13333333333333333);
    updateOrbit(input, dt);
    updateDynamicMotion(input, dt);

    if (!m_initialized
        || dt <= 0.0
        || distanceSquared(
            chassisGlobalPosition,
            m_previousChassisGlobalPosition)
            > m_tuning.teleportDistanceMeters * m_tuning.teleportDistanceMeters)
    {
        snap(
            chassisGlobalPosition,
            horizontalForward,
            rawChassisHeadingDegrees);
        return;
    }

    // CAM09: horizontal forward travel still follows directly, but lateral
    // chassis translation feeds a bounded positional lag. This is the missing
    // side-to-side damper: when the car changes lane/turns, the camera rig
    // eases into its new horizontal position instead of moving with the car
    // one-for-one and then stopping abruptly.
    const double chassisDeltaX =
        chassisGlobalPosition.x - m_previousChassisGlobalPosition.x;
    const double chassisDeltaZ =
        chassisGlobalPosition.z - m_previousChassisGlobalPosition.z;
    const double chassisRightX = static_cast<double>(horizontalForward.z);
    const double chassisRightZ = -static_cast<double>(horizontalForward.x);
    const double lateralChassisDelta =
        chassisDeltaX * chassisRightX + chassisDeltaZ * chassisRightZ;
    if (std::isfinite(lateralChassisDelta))
    {
        m_lateralFollowLagMeters -=
            lateralChassisDelta * m_tuning.lateralFollowInertia;
        m_lateralFollowLagMeters = std::clamp(
            m_lateralFollowLagMeters,
            -m_tuning.maximumLateralFollowLagMeters,
            m_tuning.maximumLateralFollowLagMeters);
    }
    stepSpring(
        m_lateralFollowLagMeters,
        m_lateralFollowLagVelocity,
        0.0,
        m_tuning.lateralFollowSpringFrequencyHz,
        m_tuning.lateralFollowDampingRatio,
        dt);
    m_lateralFollowLagMeters = std::clamp(
        m_lateralFollowLagMeters,
        -m_tuning.maximumLateralFollowLagMeters,
        m_tuning.maximumLateralFollowLagMeters);

    // Camera heave is world-up inertia only. Forward horizontal placement is
    // still rebuilt directly from the chassis, so racing speed never leaves
    // the camera metres behind the vehicle.
    const double chassisDeltaY =
        chassisGlobalPosition.y - m_previousChassisGlobalPosition.y;
    if (std::isfinite(chassisDeltaY))
    {
        m_verticalLagMeters -= chassisDeltaY * m_tuning.verticalInertia;
        m_verticalLagMeters = std::clamp(
            m_verticalLagMeters,
            -m_tuning.maximumVerticalLagMeters,
            m_tuning.maximumVerticalLagMeters);
    }

    // Unwrap the real chassis heading around the current camera heading before
    // springing. Unlike Euler-Y tracking, this value came from the interpolated
    // quaternion's actual +Z forward vector, so roundabouts, pitch and roll do
    // not change which component happens to be called "yaw".
    const double chassisHeadingUnwrapped = m_followYawDegrees
        + wrapDegrees(rawChassisHeadingDegrees - m_followYawDegrees);
    stepSpring(
        m_followYawDegrees,
        m_followYawVelocity,
        chassisHeadingUnwrapped,
        m_tuning.headingSpringFrequencyHz,
        m_tuning.headingDampingRatio,
        dt);

    const double rawLag = wrapDegrees(
        m_followYawDegrees - chassisHeadingUnwrapped);
    m_headingLagDegrees = std::clamp(
        rawLag,
        -m_tuning.maximumHeadingLagDegrees,
        m_tuning.maximumHeadingLagDegrees);
    if (m_headingLagDegrees != rawLag)
    {
        m_followYawDegrees = chassisHeadingUnwrapped + m_headingLagDegrees;
        // If the spring was pushing farther outside the safe chase cone, kill
        // that outward component. It may immediately accelerate inward again.
        const double directionToTarget = wrapDegrees(
            chassisHeadingUnwrapped - m_followYawDegrees);
        if (m_followYawVelocity * directionToTarget < 0.0)
            m_followYawVelocity = 0.0;
    }

    stepSpring(
        m_verticalLagMeters,
        m_verticalLagVelocity,
        0.0,
        m_tuning.verticalSpringFrequencyHz,
        m_tuning.verticalDampingRatio,
        dt);
    m_verticalLagMeters = std::clamp(
        m_verticalLagMeters,
        -m_tuning.maximumVerticalLagMeters,
        m_tuning.maximumVerticalLagMeters);

    m_previousChassisGlobalPosition = chassisGlobalPosition;
    rebuildDesiredPose(chassisGlobalPosition, horizontalForward);
}

void ChaseCamera::resolveCollisionDistance(
    double maximumAnchorToEyeDistanceMeters,
    float deltaSeconds)
{
    if (!m_initialized || !std::isfinite(maximumAnchorToEyeDistanceMeters))
        return;

    const double desiredDistance = length(subtract(
        m_desiredEyeGlobal,
        m_collisionAnchorGlobal));
    if (desiredDistance <= kVectorEpsilon)
        return;

    const double targetDistance = std::clamp(
        maximumAnchorToEyeDistanceMeters,
        std::min(m_tuning.minimumCollisionDistanceMeters, desiredDistance),
        desiredDistance);

    const double dt = std::clamp(
        static_cast<double>(deltaSeconds),
        0.0,
        0.13333333333333333);

    if (targetDistance < m_collisionRayDistanceMeters)
    {
        // Collision must never be allowed to clip for the sake of smoothing.
        m_collisionRayDistanceMeters = targetDistance;
        m_collisionRayDistanceVelocity = 0.0;
    }
    else if (dt > 0.0)
    {
        stepSpring(
            m_collisionRayDistanceMeters,
            m_collisionRayDistanceVelocity,
            targetDistance,
            m_tuning.collisionRecoveryFrequencyHz,
            m_tuning.collisionRecoveryDampingRatio,
            dt);
    }
    else
    {
        m_collisionRayDistanceMeters = targetDistance;
        m_collisionRayDistanceVelocity = 0.0;
    }

    m_collisionRayDistanceMeters = std::clamp(
        m_collisionRayDistanceMeters,
        std::min(m_tuning.minimumCollisionDistanceMeters, desiredDistance),
        desiredDistance);
    rebuildCollisionResolvedEye();
}

bool ChaseCamera::buildLocalFrame(
    const heritage::math::DVec3& globalOrigin,
    CameraFrame& frame) const
{
    if (!m_initialized || !finite(globalOrigin))
        return false;

    const heritage::math::DVec3 localEye = subtract(m_eyeGlobal, globalOrigin);
    const heritage::math::DVec3 localTarget = subtract(m_targetGlobal, globalOrigin);
    if (!finite(localEye) || !finite(localTarget))
        return false;

    constexpr double kFloatMax =
        static_cast<double>((std::numeric_limits<float>::max)());
    if (std::abs(localEye.x) > kFloatMax
        || std::abs(localEye.y) > kFloatMax
        || std::abs(localEye.z) > kFloatMax
        || std::abs(localTarget.x) > kFloatMax
        || std::abs(localTarget.y) > kFloatMax
        || std::abs(localTarget.z) > kFloatMax)
    {
        return false;
    }

    frame.eyeLocal = heritage::math::toFloat(localEye);
    frame.targetLocal = heritage::math::toFloat(localTarget);
    frame.up = { 0.0f, 1.0f, 0.0f };
    frame.valid = true;
    return true;
}

} // namespace heritage::camera
