#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;



bool VehicleSystem::setWheelBrakeFactors(
    VehicleHandle handle,
    std::size_t wheelIndex,
    float serviceBrakeFactor,
    float handbrakeFactor)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelBrakeFactors received an invalid vehicle handle or wheel index.");
        return false;
    }
    if (!finiteFloat(serviceBrakeFactor)
        || serviceBrakeFactor < 0.0f
        || !finiteFloat(handbrakeFactor)
        || handbrakeFactor < 0.0f)
    {
        setError("Vehicle wheel brake factors must be finite non-negative numbers.");
        return false;
    }

    slot->record.wheels[wheelIndex].description.brakeFactor =
        serviceBrakeFactor;
    slot->record.wheels[wheelIndex].description.handbrakeFactor =
        handbrakeFactor;
    clearError();
    return true;
}

} // namespace heritage::vehicles
