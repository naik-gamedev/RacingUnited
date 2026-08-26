#pragma once

#include "../Core/Math/Math.hpp"

namespace heritage::camera {

// Render-facing camera frame expressed in the current compact FP32 local world.
// Absolute camera state stays FP64; only the final render frame is converted to
// the floating-origin local frame.
struct CameraFrame
{
    heritage::math::Vec3 eyeLocal{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 targetLocal{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    bool valid = false;
};

// Per-frame player intent for the chase camera. Pointer deltas are consumed
// only while orbitDragActive; forward speed is physical chassis speed along
// its own forward axis, so reversing does not cancel a deliberately parked
// inspection view.
struct ChaseCameraInput
{
    bool orbitDragActive = false;
    double pointerDeltaX = 0.0;
    double pointerDeltaY = 0.0;
    double forwardSpeedMetersPerSecond = 0.0;
    double lateralSpeedMetersPerSecond = 0.0;

    // Paused gameplay keeps the chase/orbit camera interactive but must not
    // interpret a frozen rigid-body velocity as a new acceleration impulse.
    bool dynamicMotionResponseActive = true;
};

// CAM03 chase camera policy:
// - heading follows the ACTUAL interpolated chassis forward vector, never an
//   Euler yaw component or steering-input sign;
// - automatic heading inertia is spring-damped and hard-bounded, while an
//   explicit mouse orbit remains a separate persistent player-authored offset;
// - beginning forward travel releases that offset smoothly back to chase view;
// - jump/landing heave keeps the light under-damped response from CAM01;
// - collision is resolved from a chassis-height anchor toward the desired eye.
//   The camera snaps inward immediately, but springs outward after the obstacle
//   clears so terrain/walls do not cause ugly popping.
class ChaseCamera
{
public:
    struct Tuning
    {
        // CAM07 lowers and flattens the normal chase composition toward the
        // classic street-racing view: more horizon, less roof/ground seen from
        // above, while keeping enough distance for collision recovery.
        double distanceMeters = 6.80;
        double eyeHeightMeters = 2.20;
        double targetHeightMeters = 0.95;
        double lookAheadMeters = 2.75;

        // NFSU-like yaw inertia. The hard lag cone is the safety guarantee that
        // prevents the camera from becoming a free 360-degree orbit camera.
        double maximumHeadingLagDegrees = 7.0;
        double headingSpringFrequencyHz = 1.15;
        double headingDampingRatio = 1.02;

        // Hold the primary pointer button and drag to orbit. The chosen view
        // persists at rest; once the chassis travels forward, it returns to
        // centre through this critically damped spring.
        double orbitDegreesPerPixel = 0.20;
        double minimumOrbitPitchDegrees = -12.0;
        double maximumOrbitPitchDegrees = 45.0;
        double orbitReturnForwardSpeedMetersPerSecond = 0.50;
        double orbitReturnSpringFrequencyHz = 1.15;
        double orbitReturnDampingRatio = 1.0;

        // Jump/landing response.
        double verticalInertia = 0.56;
        double maximumVerticalLagMeters = 0.90;
        double verticalSpringFrequencyHz = 1.65;
        double verticalDampingRatio = 0.88;
        double targetVerticalResponse = 0.16;

        // CAM07 dynamic chase inertia. Constant speed adds only a small
        // rearward extension; acceleration/braking and lateral acceleration
        // add bounded spring-damped offsets. These are deliberately small so
        // the camera feels alive without becoming an arcade wobble camera.
        double speedPullbackMetersPerMeterPerSecond = 0.0080;
        double maximumSpeedPullbackMeters = 0.38;
        double longitudinalAccelerationGain = 0.030;
        double maximumAccelerationPullbackMeters = 0.30;
        double maximumBrakingPushForwardMeters = 0.24;
        double lateralAccelerationGain = 0.022;
        double maximumLateralSwayMeters = 0.34;
        double motionSpringFrequencyHz = 1.35;
        double motionDampingRatio = 1.05;
        double maximumSampleAccelerationMetersPerSecondSquared = 25.0;

        // CAM09 lateral FOLLOW damper. CAM07 damped the small acceleration
        // sway offset, but the whole chase rig still inherited the chassis'
        // horizontal side translation immediately. Accumulate only the
        // chassis displacement perpendicular to its forward axis, then let a
        // critically damped spring catch the rig up. Forward travel remains
        // direct, so high speed does not leave the camera metres behind.
        double lateralFollowInertia = 0.85;
        double maximumLateralFollowLagMeters = 0.65;
        double lateralFollowSpringFrequencyHz = 1.05;
        double lateralFollowDampingRatio = 1.0;

        // CAM04 camera collision volume. A swept sphere protects the camera
        // centre AND its near-plane neighbourhood from terrain/wall corners.
        // Padding is extra safety beyond the sphere radius itself.
        double collisionAnchorHeightMeters = 1.05;
        double collisionRadiusMeters = 0.32;
        double collisionPaddingMeters = 0.08;
        double minimumCollisionDistanceMeters = 0.35;
        double collisionRecoveryFrequencyHz = 3.0;
        double collisionRecoveryDampingRatio = 1.0;

        // Teleports/resets snap rather than launching the springs.
        double teleportDistanceMeters = 12.0;
    };

    void reset();
    bool initialized() const { return m_initialized; }

    void setTuning(const Tuning& tuning) { m_tuning = tuning; }
    const Tuning& tuning() const { return m_tuning; }

    void update(
        const heritage::math::DVec3& chassisGlobalPosition,
        const heritage::math::Vec3& chassisForwardWorld,
        float deltaSeconds,
        const ChaseCameraInput& input = {});

    // The collision query should sweep the configured camera sphere from
    // collisionAnchorGlobal() toward desiredEyeGlobal(). Pass the hit-distance
    // minus padding here, or the full desired probe length when there is no hit.
    void resolveCollisionDistance(
        double maximumAnchorToEyeDistanceMeters,
        float deltaSeconds);

    bool buildLocalFrame(
        const heritage::math::DVec3& globalOrigin,
        CameraFrame& frame) const;

    heritage::math::DVec3 eyeGlobal() const { return m_eyeGlobal; }
    heritage::math::DVec3 desiredEyeGlobal() const { return m_desiredEyeGlobal; }
    heritage::math::DVec3 targetGlobal() const { return m_targetGlobal; }
    heritage::math::DVec3 collisionAnchorGlobal() const
    {
        return m_collisionAnchorGlobal;
    }
    double headingLagDegrees() const { return m_headingLagDegrees; }
    double orbitYawDegrees() const { return m_orbitYawDegrees; }
    double orbitPitchDegrees() const { return m_orbitPitchDegrees; }
    bool orbitReturning() const { return m_orbitReturnActive; }
    double verticalLagMeters() const { return m_verticalLagMeters; }
    double longitudinalDynamicOffsetMeters() const
    {
        return m_longitudinalDynamicOffsetMeters;
    }
    double lateralDynamicOffsetMeters() const
    {
        return m_lateralDynamicOffsetMeters;
    }
    double lateralFollowLagMeters() const
    {
        return m_lateralFollowLagMeters;
    }
    double currentCollisionDistanceMeters() const
    {
        return m_collisionRayDistanceMeters;
    }

private:
    static void stepSpring(
        double& value,
        double& velocity,
        double target,
        double frequencyHz,
        double dampingRatio,
        double deltaSeconds);

    static bool horizontalHeadingDegrees(
        const heritage::math::Vec3& forward,
        double& headingDegrees,
        heritage::math::Vec3& horizontalForward);

    void snap(
        const heritage::math::DVec3& chassisGlobalPosition,
        const heritage::math::Vec3& horizontalForward,
        double chassisHeadingDegrees);
    void rebuildDesiredPose(
        const heritage::math::DVec3& chassisGlobalPosition,
        const heritage::math::Vec3& horizontalForward);
    void rebuildCollisionResolvedEye();
    void updateOrbit(const ChaseCameraInput& input, double deltaSeconds);
    void updateDynamicMotion(
        const ChaseCameraInput& input,
        double deltaSeconds);

    Tuning m_tuning{};
    bool m_initialized = false;
    heritage::math::DVec3 m_previousChassisGlobalPosition{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 m_eyeGlobal{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 m_desiredEyeGlobal{ 0.0, 0.0, 0.0 };
    heritage::math::DVec3 m_targetGlobal{ 0.0, 1.0, 0.0 };
    heritage::math::DVec3 m_collisionAnchorGlobal{ 0.0, 1.0, 0.0 };

    double m_followYawDegrees = 0.0;
    double m_followYawVelocity = 0.0;
    double m_headingLagDegrees = 0.0;
    double m_orbitYawDegrees = 0.0;
    double m_orbitYawVelocity = 0.0;
    double m_orbitPitchDegrees = 0.0;
    double m_orbitPitchVelocity = 0.0;
    bool m_orbitReturnActive = false;
    double m_verticalLagMeters = 0.0;
    double m_verticalLagVelocity = 0.0;
    double m_longitudinalDynamicOffsetMeters = 0.0;
    double m_longitudinalDynamicOffsetVelocity = 0.0;
    double m_lateralDynamicOffsetMeters = 0.0;
    double m_lateralDynamicOffsetVelocity = 0.0;
    double m_lateralFollowLagMeters = 0.0;
    double m_lateralFollowLagVelocity = 0.0;
    double m_previousForwardSpeedMetersPerSecond = 0.0;
    double m_previousLateralSpeedMetersPerSecond = 0.0;
    bool m_haveDynamicMotionSample = false;
    double m_collisionRayDistanceMeters = 0.0;
    double m_collisionRayDistanceVelocity = 0.0;
};

} // namespace heritage::camera
