#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setDriverAids(
    VehicleHandle handle,
    bool antiLockBrakesEnabled,
    bool tractionControlEnabled,
    float antiLockTargetSlip,
    float tractionControlTargetSlip,
    float minimumActivationSpeed,
    float modulationRate,
    float maximumHandbrakeTorque)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetDriverAids received an invalid or stale vehicle handle.");
        return false;
    }

    DriverAidDescription value;
    value.antiLockBrakesEnabled = antiLockBrakesEnabled;
    value.tractionControlEnabled = tractionControlEnabled;
    value.antiLockTargetSlip = antiLockTargetSlip;
    value.tractionControlTargetSlip = tractionControlTargetSlip;
    value.minimumActivationSpeed = minimumActivationSpeed;
    value.modulationRate = modulationRate;
    value.maximumHandbrakeTorque = maximumHandbrakeTorque;
    if (!validDriverAidDescription(value))
    {
        setError("Vehicle.SetDriverAids received values outside the supported range.");
        return false;
    }

    slot->record.driverAids = value;
    if (!antiLockBrakesEnabled)
    {
        for (WheelRecord& wheel : slot->record.wheels)
        {
            wheel.state.antiLockModulation = 1.0f;
            wheel.state.antiLockActive = false;
        }
    }
    if (!tractionControlEnabled)
    {
        for (WheelRecord& wheel : slot->record.wheels)
        {
            wheel.state.tractionControlModulation = 1.0f;
            wheel.state.tractionControlActive = false;
        }
    }
    clearError();
    return true;
}

bool VehicleSystem::driverAidState(
    VehicleHandle handle,
    DriverAidState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetDriverAidState received an invalid or stale vehicle handle.");
        return false;
    }

    value.antiLockBrakesEnabled =
        slot->record.driverAids.antiLockBrakesEnabled;
    value.tractionControlEnabled =
        slot->record.driverAids.tractionControlEnabled;
    value.antiLockActiveWheelCount =
        slot->record.antiLockActiveWheelCount;
    value.tractionControlActiveWheelCount =
        slot->record.tractionControlActiveWheelCount;
    value.antiLockTargetSlip =
        slot->record.driverAids.antiLockTargetSlip;
    value.tractionControlTargetSlip =
        slot->record.driverAids.tractionControlTargetSlip;
    value.minimumActivationSpeed =
        slot->record.driverAids.minimumActivationSpeed;
    value.handbrakeInput = slot->record.handbrake;
    clearError();
    return true;
}

} // namespace heritage::vehicles
