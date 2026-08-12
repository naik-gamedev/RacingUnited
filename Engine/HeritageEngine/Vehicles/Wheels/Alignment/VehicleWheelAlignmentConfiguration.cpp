#include "../../VehicleSystem.hpp"
#include "../../VehicleSystemInternal.hpp"
#include "../../Tires/TireSlipDynamics.hpp"
#include "../../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setWheelAlignment(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const WheelAlignmentSetup& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelAlignment received an invalid vehicle handle or wheel index.");
        return false;
    }
    if (!validWheelAlignmentSetup(value))
    {
        setError("Vehicle.SetWheelAlignment received alignment data outside the supported range.");
        return false;
    }

    WheelDescription updated = slot->record.wheels[wheelIndex].description;
    updated.staticCamberDegrees = static_cast<float>(value.camberDegrees);
    updated.staticToeDegrees = static_cast<float>(value.toeDegrees);
    updated.casterOverrideEnabled = value.casterOverrideEnabled;
    updated.staticCasterDegrees = static_cast<float>(value.casterDegrees);
    if (!validWheelDescription(updated))
    {
        setError("Vehicle.SetWheelAlignment produced an invalid wheel description.");
        return false;
    }

    slot->record.wheels[wheelIndex].description = updated;
    clearError();
    return true;
}

bool VehicleSystem::wheelAlignment(
    VehicleHandle handle,
    std::size_t wheelIndex,
    WheelAlignmentSetup& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelAlignment received an invalid vehicle handle or wheel index.");
        return false;
    }

    const WheelDescription& description = slot->record.wheels[wheelIndex].description;
    value.camberDegrees = description.staticCamberDegrees;
    value.toeDegrees = description.staticToeDegrees;
    value.casterOverrideEnabled = description.casterOverrideEnabled;
    value.casterDegrees = description.staticCasterDegrees;
    clearError();
    return true;
}

} // namespace heritage::vehicles
