#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setInputs(
    VehicleHandle handle,
    float throttle,
    float brake,
    float steering,
    float handbrake)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetInputs received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(throttle)
        || !finiteFloat(brake)
        || !finiteFloat(steering)
        || !finiteFloat(handbrake))
    {
        setError("Vehicle inputs must be finite numbers.");
        return false;
    }
    slot->record.throttle = std::clamp(throttle, 0.0f, 1.0f);
    slot->record.brake = std::clamp(brake, 0.0f, 1.0f);
    slot->record.steering = std::clamp(steering, -1.0f, 1.0f);
    slot->record.handbrake = std::clamp(handbrake, 0.0f, 1.0f);
    clearError();
    return true;
}

bool VehicleSystem::setTuning(
    VehicleHandle handle,
    float maximumDriveForce,
    float maximumBrakeForce,
    float maximumSteerAngleDegrees,
    float tireFriction,
    float lateralStiffness,
    float rollingResistance)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetTuning received an invalid or stale vehicle handle.");
        return false;
    }
    VehicleDescription value = slot->record.description;
    value.maximumDriveForce = maximumDriveForce;
    value.maximumBrakeForce = maximumBrakeForce;
    value.maximumSteerAngleDegrees = maximumSteerAngleDegrees;
    value.tireFriction = tireFriction;
    value.lateralStiffness = lateralStiffness;
    value.rollingResistance = rollingResistance;
    if (!validVehicleDescription(value))
    {
        setError("Vehicle.SetTuning received values outside the supported range.");
        return false;
    }
    slot->record.description = value;
    clearError();
    return true;
}

bool VehicleSystem::setHighRateHertz(VehicleHandle handle, float hertz)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetHighRateHertz received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(hertz)
        || hertz < kMinimumHighRateHertz
        || hertz > kMaximumHighRateHertz)
    {
        setError("Vehicle high-rate solver must be between 120 and 2000 Hz.");
        return false;
    }
    slot->record.description.highRateHertz = hertz;
    slot->record.highRateAccumulator = 0.0;
    clearError();
    return true;
}

} // namespace heritage::vehicles
