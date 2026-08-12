#include "../../VehicleSystem.hpp"
#include "../../VehicleSystemInternal.hpp"
#include "../../Tires/TireSlipDynamics.hpp"
#include "../../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setWheelFitment(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const WheelFitmentDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelFitment received an invalid vehicle handle or wheel index.");
        return false;
    }

    if (!validWheelFitmentDescription(value))
    {
        setError("Vehicle.SetWheelFitment received fitment data outside the supported range.");
        return false;
    }

    WheelDescription updated = slot->record.wheels[wheelIndex].description;
    updated.fitment = value;
    const WheelFitmentResolved resolvedFitment = resolveWheelFitment(value);
    if (value.enabled)
        updated.radius = static_cast<float>(resolvedFitment.nominalTireRadiusM);
    if (!validWheelDescription(updated))
    {
        setError("Vehicle.SetWheelFitment produced an invalid wheel description.");
        return false;
    }

    slot->record.wheels[wheelIndex].description = updated;
    clearError();
    return true;
}

bool VehicleSystem::wheelFitment(
    VehicleHandle handle,
    std::size_t wheelIndex,
    WheelFitmentDescription& value,
    WheelFitmentResolved& resolved) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelFitment received an invalid vehicle handle or wheel index.");
        return false;
    }

    value = slot->record.wheels[wheelIndex].description.fitment;
    resolved = resolveWheelFitment(value);
    clearError();
    return true;
}

bool VehicleSystem::wheelFitmentGeometry(
    VehicleHandle handle,
    std::size_t wheelIndex,
    WheelFitmentGeometryState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelFitmentGeometry received an invalid vehicle handle or wheel index.");
        return false;
    }

    const WheelRecord& wheel = slot->record.wheels[wheelIndex];
    const WheelHubReferenceGeometry hub = resolveWheelHubReferenceGeometry(
        wheel.description.localMount,
        wheel.description.fitment);

    value = {};
    value.hubReferenceValid = hub.valid;
    value.referenceWheelCenterLocal = hub.referenceWheelCenterLocal;
    value.referenceHubFaceCenterLocal = hub.referenceHubFaceCenterLocal;
    value.installedMountFaceCenterLocal = hub.installedMountFaceCenterLocal;
    value.installedWheelCenterLocal = hub.installedWheelCenterLocal;
    value.installedInnerTirePlaneLocal = hub.installedInnerTirePlaneLocal;
    value.installedOuterTirePlaneLocal = hub.installedOuterTirePlaneLocal;
    value.inboardTireExtensionFromReferenceHubM =
        hub.inboardTireExtensionFromReferenceHubM;
    value.outboardTireExtensionFromReferenceHubM =
        hub.outboardTireExtensionFromReferenceHubM;

    value.steeringGroundGeometryValid =
        wheel.state.steeringGroundGeometryValid;
    value.worldSteeringAxisPoint = wheel.state.worldSteeringAxisPoint;
    value.steeringAxisGroundPointWorld =
        wheel.state.steeringAxisGroundPointWorld;
    value.signedScrubRadiusM = wheel.state.signedScrubRadiusM;
    value.scrubRadiusMagnitudeM = wheel.state.scrubRadiusMagnitudeM;
    value.mechanicalTrailM = wheel.state.mechanicalTrailM;
    clearError();
    return true;
}

} // namespace heritage::vehicles
