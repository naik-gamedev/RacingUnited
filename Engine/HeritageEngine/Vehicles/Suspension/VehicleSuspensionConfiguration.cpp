#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setWheelSuspensionModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const SuspensionModelDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelSuspensionModel received an invalid vehicle handle or wheel index.");
        return false;
    }

    WheelDescription updated = slot->record.wheels[wheelIndex].description;
    updated.suspensionProvider = value.provider;
    updated.springPreload = static_cast<float>(value.springPreloadN);
    updated.springRate = static_cast<float>(value.springRateNPerM);
    updated.springProgression = static_cast<float>(value.springProgressionNPerM2);
    updated.bumpDamping = static_cast<float>(value.bumpDampingNsPerM);
    updated.bumpHighSpeedDamping = static_cast<float>(value.bumpHighSpeedDampingNsPerM);
    updated.bumpDampingKneeVelocity = static_cast<float>(value.bumpDampingKneeVelocityMps);
    updated.reboundDamping = static_cast<float>(value.reboundDampingNsPerM);
    updated.reboundHighSpeedDamping = static_cast<float>(value.reboundHighSpeedDampingNsPerM);
    updated.reboundDampingKneeVelocity = static_cast<float>(
        value.reboundDampingKneeVelocityMps);
    updated.bumpStopEngagement = static_cast<float>(value.bumpStopEngagementM);
    updated.bumpStopRate = static_cast<float>(value.bumpStopRateNPerM);
    updated.bumpStopProgression = static_cast<float>(value.bumpStopProgressionNPerM2);
    updated.droopStopEngagement = static_cast<float>(value.droopStopEngagementM);
    updated.droopStopRate = static_cast<float>(value.droopStopRateNPerM);
    updated.suspensionMotionRatio = static_cast<float>(value.motionRatio);
    updated.maximumSuspensionForce = static_cast<float>(value.maximumForceN);
    if (!validWheelDescription(updated))
    {
        setError("Vehicle.SetWheelSuspensionModel received suspension data outside the supported range.");
        return false;
    }

    slot->record.wheels[wheelIndex].description = updated;
    clearError();
    return true;
}

bool VehicleSystem::wheelSuspensionModel(
    VehicleHandle handle,
    std::size_t wheelIndex,
    SuspensionModelDescription& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelSuspensionModel received an invalid vehicle handle or wheel index.");
        return false;
    }

    const WheelDescription& description =
        slot->record.wheels[wheelIndex].description;
    value.provider = description.suspensionProvider;
    value.springPreloadN = description.springPreload;
    value.springRateNPerM = description.springRate;
    value.springProgressionNPerM2 = description.springProgression;
    value.bumpDampingNsPerM = description.bumpDamping;
    value.bumpHighSpeedDampingNsPerM = description.bumpHighSpeedDamping;
    value.bumpDampingKneeVelocityMps =
        description.bumpDampingKneeVelocity;
    value.reboundDampingNsPerM = description.reboundDamping;
    value.reboundHighSpeedDampingNsPerM =
        description.reboundHighSpeedDamping;
    value.reboundDampingKneeVelocityMps =
        description.reboundDampingKneeVelocity;
    value.bumpStopEngagementM = description.bumpStopEngagement;
    value.bumpStopRateNPerM = description.bumpStopRate;
    value.bumpStopProgressionNPerM2 = description.bumpStopProgression;
    value.droopStopEngagementM = description.droopStopEngagement;
    value.droopStopRateNPerM = description.droopStopRate;
    value.motionRatio = description.suspensionMotionRatio;
    value.maximumForceN = description.maximumSuspensionForce;
    clearError();
    return true;
}

bool VehicleSystem::setWheelSuspensionGeometry(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const SuspensionGeometryDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.SetWheelSuspensionGeometry received an invalid vehicle handle or wheel index.");
        return false;
    }

    WheelDescription updated = slot->record.wheels[wheelIndex].description;
    updated.suspensionProvider = value.provider;
    updated.localSteeringAxis = value.localSteeringAxis;
    updated.staticCamberDegrees = value.staticCamberDegrees;
    updated.camberGainDegreesPerM = value.camberGainDegreesPerM;
    updated.camberProgressionDegreesPerM2 =
        value.camberProgressionDegreesPerM2;
    updated.staticToeDegrees = value.staticToeDegrees;
    updated.toeGainDegreesPerM = value.toeGainDegreesPerM;
    updated.toeProgressionDegreesPerM2 =
        value.toeProgressionDegreesPerM2;
    updated.macPhersonHardpoints = value.macPherson;
    updated.trailingArmHardpoints = value.trailingArm;
    if (!validWheelDescription(updated))
    {
        setError("Vehicle.SetWheelSuspensionGeometry received geometry data outside the supported range.");
        return false;
    }

    updated.localSteeringAxis = normalized(
        updated.localSteeringAxis,
        { 0.0f, 1.0f, 0.0f });
    slot->record.wheels[wheelIndex].description = updated;
    clearError();
    return true;
}

bool VehicleSystem::wheelSuspensionGeometry(
    VehicleHandle handle,
    std::size_t wheelIndex,
    SuspensionGeometryDescription& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.GetWheelSuspensionGeometry received an invalid vehicle handle or wheel index.");
        return false;
    }

    value = suspensionGeometryDescription(
        slot->record.wheels[wheelIndex].description);
    clearError();
    return true;
}

} // namespace heritage::vehicles
