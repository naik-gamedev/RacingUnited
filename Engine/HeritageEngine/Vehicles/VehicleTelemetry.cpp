#include "VehicleSystem.hpp"
#include "VehicleSystemInternal.hpp"
#include "Tires/TireSlipDynamics.hpp"
#include "Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;

bool VehicleSystem::startDynamicsLabCapture(
    VehicleHandle handle,
    float maximumDurationSeconds,
    float captureHertz)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab start received an invalid or stale vehicle handle.");
        return false;
    }
    if (captureHertz > slot->record.description.highRateHertz + 0.001f)
    {
        setError("Dynamics lab capture rate cannot exceed the vehicle high-rate solver.");
        return false;
    }
    if (!slot->record.dynamicsLab.start(
            maximumDurationSeconds,
            captureHertz,
            slot->record.wheels.size()))
    {
        setError(slot->record.dynamicsLab.lastError());
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::stopDynamicsLabCapture(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab stop received an invalid or stale vehicle handle.");
        return false;
    }
    slot->record.dynamicsLab.stop();
    clearError();
    return true;
}

bool VehicleSystem::clearDynamicsLabCapture(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab clear received an invalid or stale vehicle handle.");
        return false;
    }
    slot->record.dynamicsLab.clear();
    clearError();
    return true;
}

bool VehicleSystem::dynamicsLabSummary(
    VehicleHandle handle,
    DynamicsLabSummary& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab summary received an invalid or stale vehicle handle.");
        return false;
    }
    value = slot->record.dynamicsLab.summary();
    clearError();
    return true;
}

bool VehicleSystem::dynamicsLabMetricSeries(
    VehicleHandle handle,
    DynamicsLabMetric metric,
    std::size_t wheelIndex,
    std::size_t maximumPoints,
    std::vector<float>& values) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab series received an invalid or stale vehicle handle.");
        return false;
    }
    if (!slot->record.dynamicsLab.metricSeries(
            metric,
            wheelIndex,
            maximumPoints,
            values))
    {
        setError("Dynamics lab series received an invalid wheel or point count.");
        return false;
    }
    clearError();
    return true;
}

bool VehicleSystem::exportDynamicsLabCsv(
    VehicleHandle handle,
    const std::filesystem::path& path)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Dynamics lab export received an invalid or stale vehicle handle.");
        return false;
    }
    if (!slot->record.dynamicsLab.exportCsv(path))
    {
        setError(slot->record.dynamicsLab.lastError());
        return false;
    }
    clearError();
    return true;
}


void VehicleSystem::captureDynamicsLabFrame(
    Record& vehicle,
    const heritage::physics::RigidBodySystem& bodies,
    float sourceDeltaTime)
{
    if (!vehicle.dynamicsLab.recording())
        return;

    heritage::physics::RigidBodyPose pose;
    heritage::math::Vec3 linearVelocity{};
    heritage::math::Vec3 angularVelocityDegrees{};
    if (!bodies.pose(vehicle.description.chassisBody, pose)
        || !bodies.linearVelocity(
            vehicle.description.chassisBody,
            linearVelocity)
        || !bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            angularVelocityDegrees))
    {
        return;
    }

    const Quaternion rotation = quaternionFromEulerDegrees(
        pose.rotationDegrees);
    const heritage::math::Vec3 localVelocity = inverseRotateVector(
        rotation,
        linearVelocity);
    const heritage::math::Vec3 localAngularVelocity = inverseRotateVector(
        rotation,
        angularVelocityDegrees);

    DynamicsLabFrame frame;
    frame.position = pose.position;
    frame.localVelocity = localVelocity;
    // Local X is right (pitch), Y is up (yaw), and Z is forward (roll).
    frame.rollRateDegreesPerSecond = localAngularVelocity.z;
    frame.pitchRateDegreesPerSecond = localAngularVelocity.x;
    frame.yawRateDegreesPerSecond = localAngularVelocity.y;
    frame.speedMps = length(linearVelocity);
    frame.steeringInput = vehicle.steering;
    frame.steeringAngleDegrees = vehicle.currentSteerCenterDegrees;
    frame.throttleInput = vehicle.throttle;
    frame.brakeInput = vehicle.brake;
    frame.handbrakeInput = vehicle.handbrake;
    frame.engineRpm = vehicle.engineRpm;
    frame.wheels.reserve(vehicle.wheels.size());
    for (const WheelRecord& source : vehicle.wheels)
    {
        const WheelState& state = source.state;
        DynamicsLabWheelSample wheel;
        wheel.grounded = state.grounded;
        wheel.compression = static_cast<float>(state.compression);
        wheel.suspensionVelocity = static_cast<float>(state.compressionVelocity);
        wheel.suspensionSpringForce = static_cast<float>(state.suspensionSpringForce);
        wheel.suspensionDampingForce = static_cast<float>(state.suspensionDampingForce);
        wheel.suspensionBumpStopForce = static_cast<float>(state.suspensionBumpStopForce);
        wheel.suspensionDroopStopForce = static_cast<float>(state.suspensionDroopStopForce);
        wheel.damperDissipationWatts = static_cast<float>(state.damperDissipationWatts);
        wheel.unsprungVelocity = static_cast<float>(state.unsprungVelocity);
        wheel.tireDeflection = static_cast<float>(state.tireDeflection);
        wheel.tireDeflectionVelocity = static_cast<float>(state.tireDeflectionVelocity);
        wheel.tireRadialDissipationWatts = static_cast<float>(
            state.tireRadialDissipationWatts);
        wheel.camberDegrees = static_cast<float>(state.camberAngleDegrees);
        wheel.toeDegrees = static_cast<float>(state.toeAngleDegrees);
        wheel.normalForce = static_cast<float>(state.normalForce);
        wheel.longitudinalForce = static_cast<float>(state.longitudinalForce);
        wheel.lateralForce = static_cast<float>(state.lateralForce);
        wheel.steerAngleDegrees = static_cast<float>(state.steerAngleDegrees);
        wheel.wheelAngularVelocity = static_cast<float>(state.wheelAngularVelocity);
        wheel.slipRatio = static_cast<float>(state.relaxedSlipRatio);
        wheel.slipAngleDegrees = static_cast<float>(state.relaxedSlipAngleDegrees);
        wheel.gripUtilization = static_cast<float>(state.gripUtilization);
        wheel.aligningTorque = static_cast<float>(state.aligningTorque);
        frame.wheels.push_back(wheel);
    }
    vehicle.dynamicsLab.capture(sourceDeltaTime, frame);
}


} // namespace heritage::vehicles
