#include "PhysicsRegressionCommon.hpp"

#include <cmath>

#include "../Camera/ChaseCamera.hpp"
#include "../Camera/VehicleCameraController.hpp"

namespace heritage::tests {

bool chaseCameraOrbitPersistsAndReturnsOnForwardTravel()
{
    heritage::camera::ChaseCamera camera;
    const heritage::math::DVec3 position{ 0.0, 0.0, 0.0 };
    const heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    constexpr float dt = 1.0f / 120.0f;

    camera.update(position, forward, dt);

    heritage::camera::ChaseCameraInput drag{};
    drag.orbitDragActive = true;
    drag.pointerDeltaX = 300.0;
    drag.pointerDeltaY = -80.0;
    camera.update(position, forward, dt, drag);

    const double draggedYaw = camera.orbitYawDegrees();
    const double draggedPitch = camera.orbitPitchDegrees();
    if (std::abs(draggedYaw - 60.0) > 0.001
        || std::abs(draggedPitch - 16.0) > 0.001)
    {
        return false;
    }

    heritage::camera::ChaseCameraInput parked{};
    for (int frame = 0; frame < 240; ++frame)
        camera.update(position, forward, dt, parked);
    if (std::abs(camera.orbitYawDegrees() - draggedYaw) > 0.001
        || std::abs(camera.orbitPitchDegrees() - draggedPitch) > 0.001
        || camera.orbitReturning())
    {
        return false;
    }

    heritage::camera::ChaseCameraInput reversing{};
    reversing.forwardSpeedMetersPerSecond = -12.0;
    for (int frame = 0; frame < 120; ++frame)
        camera.update(position, forward, dt, reversing);
    if (std::abs(camera.orbitYawDegrees() - draggedYaw) > 0.001
        || std::abs(camera.orbitPitchDegrees() - draggedPitch) > 0.001
        || camera.orbitReturning())
    {
        return false;
    }

    heritage::camera::ChaseCameraInput drivingForward{};
    drivingForward.forwardSpeedMetersPerSecond = 12.0;
    camera.update(position, forward, dt, drivingForward);
    const double firstReturnYaw = camera.orbitYawDegrees();
    if (!camera.orbitReturning()
        || firstReturnYaw <= 0.0
        || firstReturnYaw >= draggedYaw)
    {
        return false;
    }

    for (int frame = 0; frame < 720; ++frame)
        camera.update(position, forward, dt, drivingForward);

    heritage::camera::CameraFrame localFrame{};
    return std::abs(camera.orbitYawDegrees()) < 0.01
        && std::abs(camera.orbitPitchDegrees()) < 0.01
        && !camera.orbitReturning()
        && camera.buildLocalFrame({ 0.0, 0.0, 0.0 }, localFrame)
        && localFrame.valid;
}


bool vehicleCameraAuthoringPoseAndFlyAreVehicleLocal()
{
    heritage::camera::VehicleCameraController camera;
    camera.setActive(true);
    heritage::camera::VehicleCameraPose pose{};
    pose.positionMeters = { 1.0f, 2.0f, 3.0f };
    pose.pitchDegrees = 0.0;
    pose.yawDegrees = 0.0;
    pose.rollDegrees = 0.0;
    camera.setPose(pose);

    heritage::camera::CameraFrame frame{};
    if (!camera.buildLocalFrame(
            { 100.0, 50.0, -20.0 },
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            { 90.0, 40.0, -30.0 },
            frame))
    {
        return false;
    }

    if (std::abs(frame.eyeLocal.x - 11.0f) > 0.0001f
        || std::abs(frame.eyeLocal.y - 12.0f) > 0.0001f
        || std::abs(frame.eyeLocal.z - 13.0f) > 0.0001f
        || std::abs(frame.targetLocal.z - 23.0f) > 0.0001f)
    {
        return false;
    }

    camera.setFlySpeedMetersPerSecond(2.0);
    camera.setFlyEnabled(true);
    heritage::camera::VehicleCameraFlyInput fly{};
    fly.moveForward = true;
    camera.updateFly(fly, 0.5f);
    if (std::abs(camera.pose().positionMeters.z - 4.0f) > 0.0001f)
        return false;

    fly = {};
    fly.pointerDeltaX = 100.0;
    fly.pointerDeltaY = -50.0;
    camera.updateFly(fly, 1.0f / 60.0f);
    return camera.pose().yawDegrees > 11.9
        && camera.pose().yawDegrees < 12.1
        && camera.pose().pitchDegrees > 5.9
        && camera.pose().pitchDegrees < 6.1;
}

} // namespace heritage::tests

namespace heritage::tests {

bool detachedFreeCameraCopiesCurrentFrameAndMovesInWorldSpace()
{
    heritage::camera::VehicleCameraController camera;
    heritage::camera::CameraFrame source{};
    source.eyeLocal = { 10.0f, 5.0f, -3.0f };
    source.targetLocal = { 10.0f, 5.0f, 7.0f };
    source.up = { 0.0f, 1.0f, 0.0f };
    source.valid = true;

    const heritage::math::DVec3 origin{ 1000.0, 200.0, -500.0 };
    if (!camera.activateDetachedFromFrame(source, origin)
        || !camera.detachedActive()
        || !camera.flyEnabled())
    {
        return false;
    }

    heritage::camera::CameraFrame frameA{};
    if (!camera.buildLocalFrame(
            { 0.0, 0.0, 0.0 },
            { 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f },
            origin,
            frameA))
    {
        return false;
    }

    if (std::abs(frameA.eyeLocal.x - source.eyeLocal.x) > 0.0001f
        || std::abs(frameA.eyeLocal.y - source.eyeLocal.y) > 0.0001f
        || std::abs(frameA.eyeLocal.z - source.eyeLocal.z) > 0.0001f)
    {
        return false;
    }

    // Changing the chassis pose must not move a detached world camera.
    heritage::camera::CameraFrame frameB{};
    if (!camera.buildLocalFrame(
            { 50000.0, -1000.0, 80000.0 },
            { 0.0f, 0.0f, -1.0f },
            { 0.0f, 1.0f, 0.0f },
            { 1.0f, 0.0f, 0.0f },
            origin,
            frameB))
    {
        return false;
    }
    if (std::abs(frameB.eyeLocal.x - frameA.eyeLocal.x) > 0.0001f
        || std::abs(frameB.eyeLocal.y - frameA.eyeLocal.y) > 0.0001f
        || std::abs(frameB.eyeLocal.z - frameA.eyeLocal.z) > 0.0001f)
    {
        return false;
    }

    heritage::camera::VehicleCameraFlyInput fly{};
    fly.moveForward = true;
    camera.updateFly(fly, 0.5f);
    const heritage::math::DVec3 moved = camera.detachedGlobalPosition();
    // Detached base speed is 8 m/s, so half a second forward from this level
    // +Z view should move almost exactly four metres in global Z.
    if (std::abs(moved.x - 1010.0) > 0.001
        || std::abs(moved.y - 205.0) > 0.001
        || std::abs(moved.z - (-499.0)) > 0.001)
    {
        return false;
    }

    // CAM08 cloud-travel gear: Shift/Fast is intentionally enormous only for
    // detached world flight. 8 m/s * 50 * 0.25 s = 100 m.
    fly.fast = true;
    camera.updateFly(fly, 0.25f);
    const heritage::math::DVec3 fastMoved = camera.detachedGlobalPosition();
    if (std::abs(fastMoved.z - (-399.0)) > 0.001)
        return false;

    // CAM09 detached navigation must agree with the renderer's screen-right
    // convention: from a level +Z view, Camera Right moves toward -X and a
    // positive mouse X delta turns the view toward -X.
    fly = {};
    fly.moveRight = true;
    camera.updateFly(fly, 0.5f);
    const heritage::math::DVec3 rightMoved = camera.detachedGlobalPosition();
    if (std::abs(rightMoved.x - 1006.0) > 0.001)
        return false;

    fly = {};
    fly.pointerDeltaX = 100.0;
    camera.updateFly(fly, 1.0f / 60.0f);
    if (!(camera.detachedYawDegrees() < -11.9
        && camera.detachedYawDegrees() > -12.1))
    {
        return false;
    }

    camera.deactivateDetached();
    return !camera.detachedActive() && !camera.flyEnabled();
}

bool detachedWorldCameraPoseIsFloatingOriginSafe()
{
    heritage::camera::VehicleCameraController camera;
    const heritage::math::DVec3 globalEye{ 100012.0, 205.0, -49990.0 };
    if (!camera.setDetachedWorldPose(globalEye, -12.0, 45.0, 3.0)
        || !camera.detachedActive()
        || camera.flyEnabled()
        || std::abs(camera.detachedGlobalPosition().x - globalEye.x) > 1.0e-9
        || std::abs(camera.detachedGlobalPosition().y - globalEye.y) > 1.0e-9
        || std::abs(camera.detachedGlobalPosition().z - globalEye.z) > 1.0e-9
        || std::abs(camera.detachedPitchDegrees() - (-12.0)) > 1.0e-9
        || std::abs(camera.detachedYawDegrees() - 45.0) > 1.0e-9
        || std::abs(camera.detachedRollDegrees() - 3.0) > 1.0e-9)
    {
        return false;
    }

    const heritage::math::DVec3 origin{ 100000.0, 200.0, -50000.0 };
    heritage::camera::CameraFrame frame{};
    if (!camera.buildLocalFrame(
            { -900000.0, 5000.0, 800000.0 },
            { 0.0f, 0.0f, -1.0f },
            { 0.0f, 1.0f, 0.0f },
            { 1.0f, 0.0f, 0.0f },
            origin,
            frame))
    {
        return false;
    }

    if (!frame.valid
        || std::abs(frame.eyeLocal.x - 12.0f) > 0.0001f
        || std::abs(frame.eyeLocal.y - 5.0f) > 0.0001f
        || std::abs(frame.eyeLocal.z - 10.0f) > 0.0001f)
    {
        return false;
    }

    camera.setDetachedWorldActive(false);
    return !camera.detachedActive() && !camera.flyEnabled();
}

bool chaseCameraDynamicOffsetsAreDampedAndBounded()
{
    heritage::camera::ChaseCamera camera;
    const heritage::math::DVec3 position{ 0.0, 0.0, 0.0 };
    const heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    constexpr float dt = 1.0f / 120.0f;

    heritage::camera::ChaseCameraInput input{};
    camera.update(position, forward, dt, input);

    // Establish a zero-speed sample, then accelerate. The camera must drift
    // rearward gradually rather than snapping directly to the target offset.
    camera.update(position, forward, dt, input);
    input.forwardSpeedMetersPerSecond = 30.0;
    camera.update(position, forward, dt, input);
    const double firstLongitudinal = camera.longitudinalDynamicOffsetMeters();
    if (!(firstLongitudinal > 0.0 && firstLongitudinal < 0.10))
        return false;

    for (int frame = 0; frame < 240; ++frame)
        camera.update(position, forward, dt, input);
    const double settledLongitudinal = camera.longitudinalDynamicOffsetMeters();
    if (!(settledLongitudinal > firstLongitudinal
        && settledLongitudinal <= 0.68))
    {
        return false;
    }

    // A sharp rightward lateral-speed change produces an inertial sway to the
    // opposite side, but the configured hard bound must never be exceeded.
    input.lateralSpeedMetersPerSecond = 8.0;
    camera.update(position, forward, dt, input);
    for (int frame = 0; frame < 30; ++frame)
        camera.update(position, forward, dt, input);
    const double lateral = camera.lateralDynamicOffsetMeters();
    if (lateral >= 0.0 || std::abs(lateral) > 0.34 + 1.0e-6)
        return false;

    // CAM09: a one-metre sideways chassis translation must NOT be inherited
    // one-for-one by the chase rig. It creates a bounded lag which then
    // returns smoothly to zero after the chassis stops moving sideways.
    heritage::math::DVec3 shiftedPosition{ 1.0, 0.0, 0.0 };
    camera.update(shiftedPosition, forward, dt, input);
    const double firstFollowLag = camera.lateralFollowLagMeters();
    if (!(firstFollowLag < -0.10
        && std::abs(firstFollowLag) <= 0.65 + 1.0e-6))
    {
        return false;
    }
    for (int frame = 0; frame < 240; ++frame)
        camera.update(shiftedPosition, forward, dt, input);
    const double settledFollowLag = camera.lateralFollowLagMeters();
    if (!(std::abs(settledFollowLag) < std::abs(firstFollowLag)
        && std::abs(settledFollowLag) < 0.02))
    {
        return false;
    }

    // Pause/suspension of dynamic response should settle rather than treating
    // the frozen velocity as a braking acceleration impulse.
    input.dynamicMotionResponseActive = false;
    const double beforePause = camera.longitudinalDynamicOffsetMeters();
    for (int frame = 0; frame < 240; ++frame)
        camera.update(shiftedPosition, forward, dt, input);
    return std::abs(camera.longitudinalDynamicOffsetMeters())
            < std::abs(beforePause)
        && std::abs(camera.lateralDynamicOffsetMeters()) < std::abs(lateral)
        && std::abs(camera.lateralFollowLagMeters()) < 0.005;
}

} // namespace heritage::tests
