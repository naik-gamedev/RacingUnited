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

// CAM03 chase camera policy:
// - heading follows the ACTUAL interpolated chassis forward vector, never an
//   Euler yaw component or steering-input sign;
// - heading inertia is spring-damped and hard-bounded so the eye always stays
//   inside a narrow cone behind the vehicle, even through roundabouts/spins;
// - jump/landing heave keeps the light under-damped response from CAM01;
// - collision is resolved from a chassis-height anchor toward the desired eye.
//   The camera snaps inward immediately, but springs outward after the obstacle
//   clears so terrain/walls do not cause ugly popping.
class ChaseCamera
{
public:
    struct Tuning
    {
        double distanceMeters = 6.6;
        double eyeHeightMeters = 2.70;
        double targetHeightMeters = 0.85;
        double lookAheadMeters = 2.0;

        // NFSU-like yaw inertia. The hard lag cone is the safety guarantee that
        // prevents the camera from becoming a free 360-degree orbit camera.
        double maximumHeadingLagDegrees = 7.0;
        double headingSpringFrequencyHz = 1.15;
        double headingDampingRatio = 0.95;

        // Jump/landing response.
        double verticalInertia = 0.68;
        double maximumVerticalLagMeters = 1.10;
        double verticalSpringFrequencyHz = 1.85;
        double verticalDampingRatio = 0.62;
        double targetVerticalResponse = 0.18;

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
        float deltaSeconds);

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
    double verticalLagMeters() const { return m_verticalLagMeters; }
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
    double m_verticalLagMeters = 0.0;
    double m_verticalLagVelocity = 0.0;
    double m_collisionRayDistanceMeters = 0.0;
    double m_collisionRayDistanceVelocity = 0.0;
};

} // namespace heritage::camera
