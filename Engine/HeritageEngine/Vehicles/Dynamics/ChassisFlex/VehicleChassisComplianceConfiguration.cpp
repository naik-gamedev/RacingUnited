#include "../../VehicleSystem.hpp"
#include "../../VehicleSystemInternal.hpp"
#include "../../Tires/TireSlipDynamics.hpp"
#include "../../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setChassisTorsionalCompliance(
    VehicleHandle handle,
    const ChassisTorsionalComplianceDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetChassisTorsionalCompliance received an invalid vehicle handle.");
        return false;
    }
    if (value.enabled && !validChassisTorsionalComplianceDescription(value))
    {
        setError("Vehicle.SetChassisTorsionalCompliance received invalid stiffness, damping, inertia or reference geometry.");
        return false;
    }
    slot->record.chassisFlex = value;
    slot->record.chassisFlexState = {};
    clearError();
    return true;
}

bool VehicleSystem::chassisTorsionalCompliance(
    VehicleHandle handle,
    ChassisTorsionalComplianceDescription& description,
    ChassisTorsionalComplianceState& state) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetChassisFlexState received an invalid vehicle handle.");
        return false;
    }
    description = slot->record.chassisFlex;
    state = slot->record.chassisFlexState;
    clearError();
    return true;
}

} // namespace heritage::vehicles
