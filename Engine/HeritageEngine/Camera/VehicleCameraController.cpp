#include "VehicleCameraController.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::camera {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kMinimumFlySpeed = 0.05;
constexpr double kMaximumFlySpeed = 100.0;
constexpr double kMouseDegreesPerPixel = 0.12;
constexpr double kMaximumPitchDegrees = 89.0;

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& v,
    float value)
{
    return { v.x * value, v.y * value, v.z * value };
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 normalizeSafe(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const double lengthSquared = static_cast<double>(dot(value, value));
    if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12)
        return fallback;
    const float inverseLength = static_cast<float>(1.0 / std::sqrt(lengthSquared));
    return scale(value, inverseLength);
}

bool finite(const VehicleCameraPose& pose)
{
    return std::isfinite(pose.positionMeters.x)
        && std::isfinite(pose.positionMeters.y)
        && std::isfinite(pose.positionMeters.z)
        && std::isfinite(pose.pitchDegrees)
        && std::isfinite(pose.yawDegrees)
        && std::isfinite(pose.rollDegrees);
}

heritage::math::Vec3 transformVehicleLocal(
    const heritage::math::Vec3& local,
    const heritage::math::Vec3& vehicleRight,
    const heritage::math::Vec3& vehicleUp,
    const heritage::math::Vec3& vehicleForward)
{
    return {
        vehicleRight.x * local.x + vehicleUp.x * local.y + vehicleForward.x * local.z,
        vehicleRight.y * local.x + vehicleUp.y * local.y + vehicleForward.y * local.z,
        vehicleRight.z * local.x + vehicleUp.z * local.y + vehicleForward.z * local.z
    };
}

} // namespace

void VehicleCameraController::reset()
{
    m_active = false;
    m_flyEnabled = false;
    m_pose = {};
    m_flySpeedMetersPerSecond = 1.5;
}

void VehicleCameraController::setPose(const VehicleCameraPose& pose)
{
    if (!finite(pose))
        return;

    m_pose = pose;
    m_pose.pitchDegrees = std::clamp(
        m_pose.pitchDegrees,
        -kMaximumPitchDegrees,
        kMaximumPitchDegrees);
    m_pose.yawDegrees = wrapDegrees(m_pose.yawDegrees);
    m_pose.rollDegrees = wrapDegrees(m_pose.rollDegrees);
}

void VehicleCameraController::setFlySpeedMetersPerSecond(double speed)
{
    if (!std::isfinite(speed))
        return;
    m_flySpeedMetersPerSecond = std::clamp(
        speed, kMinimumFlySpeed, kMaximumFlySpeed);
}

void VehicleCameraController::updateFly(
    const VehicleCameraFlyInput& input,
    float deltaSeconds)
{
    if (!m_active || !m_flyEnabled
        || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        return;
    }

    m_pose.yawDegrees = wrapDegrees(
        m_pose.yawDegrees + input.pointerDeltaX * kMouseDegreesPerPixel);
    m_pose.pitchDegrees = std::clamp(
        m_pose.pitchDegrees - input.pointerDeltaY * kMouseDegreesPerPixel,
        -kMaximumPitchDegrees,
        kMaximumPitchDegrees);

    heritage::math::Vec3 cameraRight{};
    heritage::math::Vec3 cameraUp{};
    heritage::math::Vec3 cameraForward{};
    localCameraBasis(m_pose, cameraRight, cameraUp, cameraForward);

    heritage::math::Vec3 movement{};
    if (input.moveForward) movement = add(movement, cameraForward);
    if (input.moveBackward) movement = add(movement, scale(cameraForward, -1.0f));
    if (input.moveRight) movement = add(movement, cameraRight);
    if (input.moveLeft) movement = add(movement, scale(cameraRight, -1.0f));
    if (input.moveUp) movement = add(movement, cameraUp);
    if (input.moveDown) movement = add(movement, scale(cameraUp, -1.0f));

    movement = normalizeSafe(movement, { 0.0f, 0.0f, 0.0f });
    double speed = m_flySpeedMetersPerSecond;
    if (input.fast)
        speed *= 4.0;
    if (input.slow)
        speed *= 0.25;

    const float distance = static_cast<float>(
        speed * static_cast<double>(deltaSeconds));
    m_pose.positionMeters = add(
        m_pose.positionMeters,
        scale(movement, distance));
}

bool VehicleCameraController::buildLocalFrame(
    const heritage::math::DVec3& chassisGlobalPosition,
    const heritage::math::Vec3& chassisRightWorld,
    const heritage::math::Vec3& chassisUpWorld,
    const heritage::math::Vec3& chassisForwardWorld,
    const heritage::math::DVec3& globalOrigin,
    CameraFrame& frame) const
{
    frame = {};
    if (!m_active || !finite(m_pose))
        return false;

    const heritage::math::Vec3 vehicleRight = normalizeSafe(
        chassisRightWorld, { 1.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 vehicleUp = normalizeSafe(
        chassisUpWorld, { 0.0f, 1.0f, 0.0f });
    const heritage::math::Vec3 vehicleForward = normalizeSafe(
        chassisForwardWorld, { 0.0f, 0.0f, 1.0f });

    heritage::math::Vec3 localRight{};
    heritage::math::Vec3 localUp{};
    heritage::math::Vec3 localForward{};
    localCameraBasis(m_pose, localRight, localUp, localForward);

    const heritage::math::Vec3 positionWorldOffset = transformVehicleLocal(
        m_pose.positionMeters, vehicleRight, vehicleUp, vehicleForward);
    const heritage::math::Vec3 cameraForwardWorld = normalizeSafe(
        transformVehicleLocal(localForward, vehicleRight, vehicleUp, vehicleForward),
        vehicleForward);
    const heritage::math::Vec3 cameraUpWorld = normalizeSafe(
        transformVehicleLocal(localUp, vehicleRight, vehicleUp, vehicleForward),
        vehicleUp);

    const heritage::math::DVec3 eyeGlobal{
        chassisGlobalPosition.x + static_cast<double>(positionWorldOffset.x),
        chassisGlobalPosition.y + static_cast<double>(positionWorldOffset.y),
        chassisGlobalPosition.z + static_cast<double>(positionWorldOffset.z)
    };
    const heritage::math::DVec3 targetGlobal{
        eyeGlobal.x + static_cast<double>(cameraForwardWorld.x) * 10.0,
        eyeGlobal.y + static_cast<double>(cameraForwardWorld.y) * 10.0,
        eyeGlobal.z + static_cast<double>(cameraForwardWorld.z) * 10.0
    };

    frame.eyeLocal = {
        static_cast<float>(eyeGlobal.x - globalOrigin.x),
        static_cast<float>(eyeGlobal.y - globalOrigin.y),
        static_cast<float>(eyeGlobal.z - globalOrigin.z)
    };
    frame.targetLocal = {
        static_cast<float>(targetGlobal.x - globalOrigin.x),
        static_cast<float>(targetGlobal.y - globalOrigin.y),
        static_cast<float>(targetGlobal.z - globalOrigin.z)
    };
    frame.up = cameraUpWorld;
    frame.valid = true;
    return true;
}

double VehicleCameraController::wrapDegrees(double value)
{
    if (!std::isfinite(value))
        return 0.0;
    value = std::fmod(value + 180.0, 360.0);
    if (value < 0.0)
        value += 360.0;
    return value - 180.0;
}

void VehicleCameraController::localCameraBasis(
    const VehicleCameraPose& pose,
    heritage::math::Vec3& right,
    heritage::math::Vec3& up,
    heritage::math::Vec3& forward)
{
    const double yaw = pose.yawDegrees * kDegreesToRadians;
    const double pitch = pose.pitchDegrees * kDegreesToRadians;
    const double roll = pose.rollDegrees * kDegreesToRadians;

    const double cosPitch = std::cos(pitch);
    forward = normalizeSafe({
        static_cast<float>(std::sin(yaw) * cosPitch),
        static_cast<float>(std::sin(pitch)),
        static_cast<float>(std::cos(yaw) * cosPitch)
    }, { 0.0f, 0.0f, 1.0f });

    const heritage::math::Vec3 referenceUp{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 unrolledRight = normalizeSafe(
        cross(referenceUp, forward), { 1.0f, 0.0f, 0.0f });
    heritage::math::Vec3 unrolledUp = normalizeSafe(
        cross(forward, unrolledRight), referenceUp);

    const float cosRoll = static_cast<float>(std::cos(roll));
    const float sinRoll = static_cast<float>(std::sin(roll));
    right = normalizeSafe(
        add(scale(unrolledRight, cosRoll), scale(unrolledUp, sinRoll)),
        unrolledRight);
    up = normalizeSafe(
        add(scale(unrolledUp, cosRoll), scale(unrolledRight, -sinRoll)),
        unrolledUp);
}

} // namespace heritage::camera
