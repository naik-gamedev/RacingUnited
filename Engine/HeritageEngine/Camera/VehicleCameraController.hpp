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

// CAMLAB01: fixed vehicle cameras and Blender-like authoring navigation live in
// a dedicated controller rather than overloading the spring-damped ChaseCamera.
// Lua owns named presets/persistence; this native controller only owns the live
// render pose and fly-navigation mechanics.
class VehicleCameraController
{
public:
    void reset();

    void setActive(bool active) { m_active = active; }
    bool active() const { return m_active; }

    void setPose(const VehicleCameraPose& pose);
    const VehicleCameraPose& pose() const { return m_pose; }

    void setFlyEnabled(bool enabled) { m_flyEnabled = enabled && m_active; }
    bool flyEnabled() const { return m_flyEnabled; }

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

    bool m_active = false;
    bool m_flyEnabled = false;
    VehicleCameraPose m_pose{};
    double m_flySpeedMetersPerSecond = 1.5;
};

} // namespace heritage::camera
