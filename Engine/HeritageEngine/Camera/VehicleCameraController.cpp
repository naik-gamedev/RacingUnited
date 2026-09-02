#include "VehicleCameraController.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::camera {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr double kRadiansToDegrees = 180.0 / kPi;
constexpr double kMinimumFlySpeed = 0.05;
constexpr double kMaximumFlySpeed = 100.0;
// CAM08: Shift is deliberately a *travel* gear for the detached camera, not
// merely a small authoring nudge. 8 m/s * 50 = 400 m/s, so the 1.2-3.2 km
// volumetric cloud layer can be reached in seconds. Vehicle-local camera
// authoring keeps its old precision-oriented x4 multiplier.
constexpr double kDetachedFastMultiplier = 50.0;
constexpr double kAuthoredFastMultiplier = 4.0;
constexpr double kMouseDegreesPerPixel = 0.12;
constexpr double kMaximumPitchDegrees = 89.0;
constexpr double kLookDistanceMeters = 10.0;

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
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

bool finite(const heritage::math::DVec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
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

bool fitsFloat(const heritage::math::DVec3& value)
{
    constexpr double kFloatMax =
        static_cast<double>((std::numeric_limits<float>::max)());
    return std::abs(value.x) <= kFloatMax
        && std::abs(value.y) <= kFloatMax
        && std::abs(value.z) <= kFloatMax;
}

} // namespace

void VehicleCameraController::reset()
{
    m_active = false;
    m_flyEnabled = false;
    m_detachedActive = false;
    m_uiInteractionActive = false;
    m_pose = {};
    m_detachedGlobalPosition = { 0.0, 0.0, 0.0 };
    m_detachedPitchDegrees = 0.0;
    m_detachedYawDegrees = 0.0;
    m_detachedRollDegrees = 0.0;
    m_detachedFlySpeedMetersPerSecond = 8.0;
    m_flySpeedMetersPerSecond = 1.5;
    m_presentedFrame = {};
}

void VehicleCameraController::setActive(bool active)
{
    if (active)
    {
        // A named vehicle view and a detached world camera are mutually
        // exclusive authorities. Re-applying the already-active authored pose
        // must not cancel its fly-edit session, but switching FROM detached
        // camera authority deliberately ends detached capture.
        const bool wasDetached = m_detachedActive;
        m_detachedActive = false;
        if (wasDetached)
            m_flyEnabled = false;
        m_active = true;
        return;
    }

    m_active = false;
    if (!m_detachedActive)
        m_flyEnabled = false;
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

void VehicleCameraController::setFlyEnabled(bool enabled)
{
    if (m_detachedActive)
    {
        // Detached mode is itself a fly-navigation mode. Disabling fly exits
        // it; enabling keeps it captured.
        if (!enabled)
            deactivateDetached();
        else
            m_flyEnabled = true;
        return;
    }

    m_flyEnabled = enabled && m_active;
}

bool VehicleCameraController::activateDetachedFromFrame(
    const CameraFrame& sourceFrame,
    const heritage::math::DVec3& globalOrigin)
{
    if (!sourceFrame.valid || !finite(globalOrigin))
        return false;

    const heritage::math::Vec3 forwardLocal = subtract(
        sourceFrame.targetLocal,
        sourceFrame.eyeLocal);
    const heritage::math::Vec3 forward = normalizeSafe(
        forwardLocal, { 0.0f, 0.0f, 1.0f });

    const double clampedY = std::clamp(
        static_cast<double>(forward.y), -1.0, 1.0);
    const double pitch = std::asin(clampedY) * kRadiansToDegrees;
    const double yaw = std::atan2(
        static_cast<double>(forward.x),
        static_cast<double>(forward.z)) * kRadiansToDegrees;
    if (!std::isfinite(pitch) || !std::isfinite(yaw))
        return false;

    m_detachedGlobalPosition = {
        globalOrigin.x + static_cast<double>(sourceFrame.eyeLocal.x),
        globalOrigin.y + static_cast<double>(sourceFrame.eyeLocal.y),
        globalOrigin.z + static_cast<double>(sourceFrame.eyeLocal.z)
    };
    if (!finite(m_detachedGlobalPosition))
        return false;

    m_detachedPitchDegrees = std::clamp(
        pitch, -kMaximumPitchDegrees, kMaximumPitchDegrees);
    m_detachedYawDegrees = wrapDegrees(yaw);
    // Free-flight intentionally starts level in roll. Mouse-look has no roll
    // control, so preserving a banked chase/up vector would strand the user at
    // an arbitrary tilt.
    m_detachedRollDegrees = 0.0;
    m_active = false;
    m_detachedActive = true;
    m_flyEnabled = true;
    return true;
}

bool VehicleCameraController::setDetachedWorldPose(
    const heritage::math::DVec3& globalPosition,
    double pitchDegrees,
    double yawDegrees,
    double rollDegrees)
{
    if (!finite(globalPosition)
        || !std::isfinite(pitchDegrees)
        || !std::isfinite(yawDegrees)
        || !std::isfinite(rollDegrees))
    {
        return false;
    }

    m_detachedGlobalPosition = globalPosition;
    m_detachedPitchDegrees = std::clamp(
        pitchDegrees, -kMaximumPitchDegrees, kMaximumPitchDegrees);
    m_detachedYawDegrees = wrapDegrees(yawDegrees);
    m_detachedRollDegrees = wrapDegrees(rollDegrees);
    m_active = false;
    m_detachedActive = true;
    m_flyEnabled = false;
    return true;
}

void VehicleCameraController::setDetachedWorldActive(bool active)
{
    if (!active)
    {
        deactivateDetached();
        return;
    }

    m_active = false;
    m_detachedActive = true;
    m_flyEnabled = false;
}

void VehicleCameraController::deactivateDetached()
{
    m_detachedActive = false;
    m_flyEnabled = false;
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
    if (!m_flyEnabled
        || (!m_active && !m_detachedActive)
        || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
    {
        return;
    }

    double* yawDegrees = m_detachedActive
        ? &m_detachedYawDegrees
        : &m_pose.yawDegrees;
    double* pitchDegrees = m_detachedActive
        ? &m_detachedPitchDegrees
        : &m_pose.pitchDegrees;

    // CAM09: Heritage's render-facing lookAt() defines screen-right with
    // cross(forward, up). The detached camera originally inherited the
    // opposite yaw sign from vehicle-local authoring, so mouse-left/right
    // felt reversed even though pitch behaved normally. Detached free flight
    // now follows the rendered screen convention; authored vehicle-camera
    // editing deliberately keeps its established controls unchanged.
    const double yawInputSign = m_detachedActive ? -1.0 : 1.0;
    *yawDegrees = wrapDegrees(
        *yawDegrees
        + input.pointerDeltaX * kMouseDegreesPerPixel * yawInputSign);
    *pitchDegrees = std::clamp(
        *pitchDegrees - input.pointerDeltaY * kMouseDegreesPerPixel,
        -kMaximumPitchDegrees,
        kMaximumPitchDegrees);

    heritage::math::Vec3 cameraRight{};
    heritage::math::Vec3 cameraUp{};
    heritage::math::Vec3 cameraForward{};
    if (m_detachedActive)
        detachedCameraBasis(cameraRight, cameraUp, cameraForward);
    else
        localCameraBasis(m_pose, cameraRight, cameraUp, cameraForward);

    heritage::math::Vec3 movement{};
    if (input.moveForward) movement = add(movement, cameraForward);
    if (input.moveBackward) movement = add(movement, scale(cameraForward, -1.0f));
    if (input.moveRight) movement = add(movement, cameraRight);
    if (input.moveLeft) movement = add(movement, scale(cameraRight, -1.0f));
    if (input.moveUp) movement = add(movement, cameraUp);
    if (input.moveDown) movement = add(movement, scale(cameraUp, -1.0f));

    movement = normalizeSafe(movement, { 0.0f, 0.0f, 0.0f });
    double speed = m_detachedActive
        ? m_detachedFlySpeedMetersPerSecond
        : m_flySpeedMetersPerSecond;
    if (input.fast)
        speed *= m_detachedActive
            ? kDetachedFastMultiplier
            : kAuthoredFastMultiplier;
    if (input.slow)
        speed *= 0.25;

    const double distance = speed * static_cast<double>(deltaSeconds);
    if (m_detachedActive)
    {
        m_detachedGlobalPosition.x += static_cast<double>(movement.x) * distance;
        m_detachedGlobalPosition.y += static_cast<double>(movement.y) * distance;
        m_detachedGlobalPosition.z += static_cast<double>(movement.z) * distance;
    }
    else
    {
        m_pose.positionMeters = add(
            m_pose.positionMeters,
            scale(movement, static_cast<float>(distance)));
    }
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

    if (m_detachedActive)
    {
        if (!finite(m_detachedGlobalPosition) || !finite(globalOrigin))
            return false;

        heritage::math::Vec3 cameraRight{};
        heritage::math::Vec3 cameraUp{};
        heritage::math::Vec3 cameraForward{};
        detachedCameraBasis(cameraRight, cameraUp, cameraForward);

        const heritage::math::DVec3 localEye{
            m_detachedGlobalPosition.x - globalOrigin.x,
            m_detachedGlobalPosition.y - globalOrigin.y,
            m_detachedGlobalPosition.z - globalOrigin.z
        };
        const heritage::math::DVec3 localTarget{
            localEye.x + static_cast<double>(cameraForward.x) * kLookDistanceMeters,
            localEye.y + static_cast<double>(cameraForward.y) * kLookDistanceMeters,
            localEye.z + static_cast<double>(cameraForward.z) * kLookDistanceMeters
        };
        if (!fitsFloat(localEye) || !fitsFloat(localTarget))
            return false;

        frame.eyeLocal = heritage::math::toFloat(localEye);
        frame.targetLocal = heritage::math::toFloat(localTarget);
        frame.up = cameraUp;
        frame.valid = true;
        return true;
    }

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
        eyeGlobal.x + static_cast<double>(cameraForwardWorld.x) * kLookDistanceMeters,
        eyeGlobal.y + static_cast<double>(cameraForwardWorld.y) * kLookDistanceMeters,
        eyeGlobal.z + static_cast<double>(cameraForwardWorld.z) * kLookDistanceMeters
    };

    const heritage::math::DVec3 localEye{
        eyeGlobal.x - globalOrigin.x,
        eyeGlobal.y - globalOrigin.y,
        eyeGlobal.z - globalOrigin.z
    };
    const heritage::math::DVec3 localTarget{
        targetGlobal.x - globalOrigin.x,
        targetGlobal.y - globalOrigin.y,
        targetGlobal.z - globalOrigin.z
    };
    if (!fitsFloat(localEye) || !fitsFloat(localTarget))
        return false;

    frame.eyeLocal = heritage::math::toFloat(localEye);
    frame.targetLocal = heritage::math::toFloat(localTarget);
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

void VehicleCameraController::detachedCameraBasis(
    heritage::math::Vec3& right,
    heritage::math::Vec3& up,
    heritage::math::Vec3& forward) const
{
    VehicleCameraPose pose{};
    pose.pitchDegrees = m_detachedPitchDegrees;
    pose.yawDegrees = m_detachedYawDegrees;
    pose.rollDegrees = m_detachedRollDegrees;
    localCameraBasis(pose, right, up, forward);

    // CAM09: Entity/scene lookAt() uses cross(forward, up) as the rendered
    // screen-right vector. localCameraBasis() predates detached navigation and
    // uses the vehicle-local +X convention (cross(up, forward)), which is the
    // exact opposite direction. Only detached translation needs the
    // render-facing basis, so flip its right vector here without changing any
    // existing authored vehicle-camera pose/roll semantics.
    right = scale(right, -1.0f);
}

} // namespace heritage::camera
