#pragma once

#include "ChaseCamera.hpp"
#include "../Core/Math/Math.hpp"

namespace heritage::camera {

// Creator-facing vehicle-mounted camera pose. Coordinates are vehicle-local in
// Heritage's runtime convention: +X right, +Y up, +Z forward. Angles are
// degrees; positive yaw turns right, positive pitch looks up, positive roll
// tilts clockwise when looking forward.
struct VehicleCameraPose
{
    heritage::math::Vec3 positionMeters{ 0.0f, 1.35f, 0.25f };
    double pitchDegrees = 0.0;
    double yawDegrees = 0.0;
    double rollDegrees = 0.0;
};

struct VehicleCameraFlyInput
{
    bool moveForward = false;
    bool moveBackward = false;
    bool moveLeft = false;
    bool moveRight = false;
    bool moveUp = false;
    bool moveDown = false;
    bool fast = false;
    bool slow = false;
    double pointerDeltaX = 0.0;
    double pointerDeltaY = 0.0;
};

// CAMLAB01 vehicle-mounted authoring navigation remains available for named
// camera presets. CAM07 adds a separate detached world-space free camera using
// the same mouse/WASD/QE fly mechanics. The detached camera stores its eye in
// FP64 global coordinates so it remains stable in large free-roam worlds and
// does not inherit any subsequent vehicle translation, pitch, roll or yaw.
class VehicleCameraController
{
public:
    void reset();

    void setActive(bool active);
    bool active() const { return m_active; }

    void setPose(const VehicleCameraPose& pose);
    const VehicleCameraPose& pose() const { return m_pose; }

    void setFlyEnabled(bool enabled);
    bool flyEnabled() const { return m_flyEnabled; }

    // Detached free camera. Activation copies the CURRENT render frame so the
    // transition is visually seamless instead of jumping to an arbitrary pose.
    bool activateDetachedFromFrame(
        const CameraFrame& sourceFrame,
        const heritage::math::DVec3& globalOrigin);
    void deactivateDetached();
    bool detachedActive() const { return m_detachedActive; }
    heritage::math::DVec3 detachedGlobalPosition() const
    {
        return m_detachedGlobalPosition;
    }
    double detachedYawDegrees() const { return m_detachedYawDegrees; }
    double detachedPitchDegrees() const { return m_detachedPitchDegrees; }

    // CAM10: module/engine UI may temporarily borrow the pointer while a fly
    // camera stays logically active. This suppresses navigation/cursor capture
    // without destroying the detached camera pose, so closing the UI resumes
    // flight exactly where the user left it.
    void setUiInteractionActive(bool active) { m_uiInteractionActive = active; }
    bool uiInteractionActive() const { return m_uiInteractionActive; }

    void setFlySpeedMetersPerSecond(double speed);
    double flySpeedMetersPerSecond() const { return m_flySpeedMetersPerSecond; }

    void updateFly(const VehicleCameraFlyInput& input, float deltaSeconds);

    bool buildLocalFrame(
        const heritage::math::DVec3& chassisGlobalPosition,
        const heritage::math::Vec3& chassisRightWorld,
        const heritage::math::Vec3& chassisUpWorld,
        const heritage::math::Vec3& chassisForwardWorld,
        const heritage::math::DVec3& globalOrigin,
        CameraFrame& frame) const;

private:
    static double wrapDegrees(double value);
    static void localCameraBasis(
        const VehicleCameraPose& pose,
        heritage::math::Vec3& right,
        heritage::math::Vec3& up,
        heritage::math::Vec3& forward);
    void detachedCameraBasis(
        heritage::math::Vec3& right,
        heritage::math::Vec3& up,
        heritage::math::Vec3& forward) const;

    bool m_active = false;
    bool m_flyEnabled = false;
    bool m_detachedActive = false;
    bool m_uiInteractionActive = false;
    VehicleCameraPose m_pose{};
    heritage::math::DVec3 m_detachedGlobalPosition{ 0.0, 0.0, 0.0 };
    double m_detachedPitchDegrees = 0.0;
    double m_detachedYawDegrees = 0.0;
    double m_detachedRollDegrees = 0.0;
    double m_detachedFlySpeedMetersPerSecond = 8.0;
    double m_flySpeedMetersPerSecond = 1.5;
};

} // namespace heritage::camera
