#include "../../VehicleSystem.hpp"
#include "../../VehicleSystemInternal.hpp"
#include "../../Tires/TireSlipDynamics.hpp"
#include "../../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setWheelUnsprungMassModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const UnsprungMassDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelUnsprungMassModel received an invalid vehicle handle or wheel index.");
        return false;
    }

    WheelRecord& wheel = slot->record.wheels[wheelIndex];
    WheelDescription updated = wheel.description;
    updated.effectiveUnsprungMass = static_cast<float>(value.effectiveMassKg);
    updated.tireRadialStiffness = static_cast<float>(value.tireRadialStiffnessNPerM);
    updated.tireRadialDamping = static_cast<float>(value.tireRadialDampingNsPerM);
    updated.maximumTireDeflection = static_cast<float>(value.maximumTireDeflectionM);
    updated.maximumTireNormalForce = static_cast<float>(value.maximumNormalForceN);
    if (!validWheelDescription(updated))
    {
        setError("Vehicle.SetWheelUnsprungMassModel received data outside the supported range.");
        return false;
    }

    const bool modeChanged = (wheel.description.effectiveUnsprungMass <= 0.0f)
        != (updated.effectiveUnsprungMass <= 0.0f);
    wheel.description = updated;
    if (modeChanged)
        wheel.unsprungMass = {};
    clearError();
    return true;
}

bool VehicleSystem::wheelUnsprungMassModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    UnsprungMassDescription& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelUnsprungMassModel received an invalid vehicle handle or wheel index.");
        return false;
    }

    const WheelDescription& description =
        slot->record.wheels[wheelIndex].description;
    value.effectiveMassKg = description.effectiveUnsprungMass;
    value.tireRadialStiffnessNPerM = description.tireRadialStiffness;
    value.tireRadialDampingNsPerM = description.tireRadialDamping;
    value.maximumTireDeflectionM = description.maximumTireDeflection;
    value.maximumNormalForceN = description.maximumTireNormalForce;
    clearError();
    return true;
}

} // namespace heritage::vehicles
