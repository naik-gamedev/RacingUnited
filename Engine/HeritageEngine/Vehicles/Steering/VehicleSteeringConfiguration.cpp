#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setSteeringGeometry(
    VehicleHandle handle,
    float ackermannPercent,
    float steeringRateDegreesPerSecond,
    float steeringReturnRateDegreesPerSecond,
    float highSpeedSteeringRateFactor,
    float highSpeedReferenceMps)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetSteeringGeometry received an invalid or stale vehicle handle.");
        return false;
    }

    VehicleDescription value = slot->record.description;
    value.ackermannPercent = ackermannPercent;
    value.steeringRateDegreesPerSecond = steeringRateDegreesPerSecond;
    value.steeringReturnRateDegreesPerSecond = steeringReturnRateDegreesPerSecond;
    value.highSpeedSteeringRateFactor = highSpeedSteeringRateFactor;
    value.highSpeedReferenceMps = highSpeedReferenceMps;
    if (!validVehicleDescription(value))
    {
        setError("Vehicle.SetSteeringGeometry received values outside the supported range.");
        return false;
    }

    slot->record.description = value;
    clearError();
    return true;
}

bool VehicleSystem::steeringState(
    VehicleHandle handle,
    SteeringState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetSteeringState received an invalid or stale vehicle handle.");
        return false;
    }

    value.input = slot->record.steering;
    value.targetCenterAngleDegrees = slot->record.targetSteerCenterDegrees;
    value.currentCenterAngleDegrees = slot->record.currentSteerCenterDegrees;
    value.innerWheelAngleDegrees = slot->record.innerSteerAngleDegrees;
    value.outerWheelAngleDegrees = slot->record.outerSteerAngleDegrees;
    value.detectedWheelbase = slot->record.detectedWheelbase;
    value.detectedSteerTrack = slot->record.detectedSteerTrack;
    value.currentRateFactor = slot->record.currentSteeringRateFactor;
    clearError();
    return true;
}

} // namespace heritage::vehicles
