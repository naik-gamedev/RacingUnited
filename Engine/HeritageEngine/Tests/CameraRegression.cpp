#include "PhysicsRegressionCommon.hpp"

#include <cmath>

#include "../Camera/ChaseCamera.hpp"

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

} // namespace heritage::tests
