#include "../VehicleSystem.hpp"
#include "../VehicleSystemInternal.hpp"
#include "../Tires/TireSlipDynamics.hpp"
#include "../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setPowertrain(
    VehicleHandle handle,
    float idleRpm,
    float redlineRpm,
    float maximumTorque,
    float engineBrakingTorque,
    float finalDriveRatio,
    float drivetrainEfficiency,
    float shiftDurationSeconds,
    float clutchEngagementRate)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetPowertrain received an invalid or stale vehicle handle.");
        return false;
    }

    PowertrainDescription value = slot->record.powertrain;
    value.idleRpm = idleRpm;
    value.redlineRpm = redlineRpm;
    value.maximumTorque = maximumTorque;
    value.engineBrakingTorque = engineBrakingTorque;
    value.finalDriveRatio = finalDriveRatio;
    value.drivetrainEfficiency = drivetrainEfficiency;
    value.shiftDurationSeconds = shiftDurationSeconds;
    value.clutchEngagementRate = clutchEngagementRate;
    if (!validPowertrainDescription(value))
    {
        setError("Vehicle.SetPowertrain received values outside the supported range.");
        return false;
    }

    slot->record.powertrain = value;
    slot->record.engineRpm = std::clamp(
        slot->record.engineRpm,
        value.idleRpm,
        value.redlineRpm + 1000.0f);
    slot->record.selectedGearRatio = selectedGearRatio(
        value,
        slot->record.currentGear);
    clearError();
    return true;
}

bool VehicleSystem::setGearRatios(
    VehicleHandle handle,
    float reverseGearRatio,
    const std::vector<float>& forwardGearRatios)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetGearRatios received an invalid or stale vehicle handle.");
        return false;
    }

    PowertrainDescription value = slot->record.powertrain;
    value.reverseGearRatio = reverseGearRatio;
    value.forwardGearRatios = forwardGearRatios;
    if (!validPowertrainDescription(value))
    {
        setError("Vehicle.SetGearRatios requires one to sixteen positive forward ratios and one negative reverse ratio.");
        return false;
    }

    slot->record.powertrain = value;
    const int maximumGear = static_cast<int>(value.forwardGearRatios.size());
    if (slot->record.currentGear > maximumGear)
        slot->record.currentGear = 0;
    if (slot->record.requestedGear > maximumGear)
        slot->record.requestedGear = slot->record.currentGear;
    slot->record.selectedGearRatio = selectedGearRatio(
        value,
        slot->record.currentGear);
    clearError();
    return true;
}

bool VehicleSystem::setDifferential(
    VehicleHandle handle,
    DifferentialMode mode,
    float biasRatio)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetDifferential received an invalid or stale vehicle handle.");
        return false;
    }
    if (!finiteFloat(biasRatio) || biasRatio < 1.0f || biasRatio > 20.0f)
    {
        setError("Vehicle differential bias ratio must be between 1 and 20.");
        return false;
    }

    slot->record.powertrain.differentialMode = mode;
    slot->record.powertrain.differentialBiasRatio = biasRatio;
    clearError();
    return true;
}

bool VehicleSystem::setGear(VehicleHandle handle, int gear)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetGear received an invalid or stale vehicle handle.");
        return false;
    }

    const int maximumGear = static_cast<int>(
        slot->record.powertrain.forwardGearRatios.size());
    if (gear < -1 || gear > maximumGear)
    {
        setError("Vehicle.SetGear received a gear outside reverse, neutral and the configured forward gears.");
        return false;
    }

    const int activeGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    if (gear == activeGear)
    {
        clearError();
        return true;
    }

    const bool directionChange = gear != 0
        && activeGear != 0
        && ((gear < 0) != (activeGear < 0));
    if (directionChange && slot->record.speed > 1.5f)
    {
        setError("Vehicle.SetGear blocked a forward/reverse direction change above 1.5 m/s.");
        return false;
    }

    slot->record.requestedGear = gear;
    slot->record.outputTorque = 0.0f;
    if (slot->record.powertrain.shiftDurationSeconds <= 0.0001f)
    {
        slot->record.currentGear = gear;
        slot->record.shifting = false;
        slot->record.shiftTimeRemaining = 0.0f;
    }
    else
    {
        slot->record.shifting = true;
        slot->record.shiftTimeRemaining =
            slot->record.powertrain.shiftDurationSeconds;
    }
    clearError();
    return true;
}

bool VehicleSystem::shiftUp(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.ShiftUp received an invalid or stale vehicle handle.");
        return false;
    }
    const int baseGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    const int maximumGear = static_cast<int>(
        slot->record.powertrain.forwardGearRatios.size());
    return setGear(handle, std::min(baseGear + 1, maximumGear));
}

bool VehicleSystem::shiftDown(VehicleHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.ShiftDown received an invalid or stale vehicle handle.");
        return false;
    }
    const int baseGear = slot->record.shifting
        ? slot->record.requestedGear
        : slot->record.currentGear;
    return setGear(handle, std::max(baseGear - 1, -1));
}

bool VehicleSystem::drivetrainState(
    VehicleHandle handle,
    DrivetrainState& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.GetDrivetrainState received an invalid or stale vehicle handle.");
        return false;
    }

    value.currentGear = slot->record.currentGear;
    value.requestedGear = slot->record.requestedGear;
    value.shifting = slot->record.shifting;
    value.shiftTimeRemaining = slot->record.shiftTimeRemaining;
    value.engineRpm = slot->record.engineRpm;
    value.engineTorque = slot->record.engineTorque;
    value.clutchEngagement = slot->record.clutchEngagement;
    value.clutchSlipRpm = slot->record.clutchSlipRpm;
    value.wheelCoupledRpm = slot->record.wheelCoupledRpm;
    value.selectedGearRatio = slot->record.selectedGearRatio;
    value.finalDriveRatio = slot->record.powertrain.finalDriveRatio;
    value.outputTorque = slot->record.outputTorque;
    value.drivenWheelSpeedDifferenceRpm =
        slot->record.drivenWheelSpeedDifferenceRpm;
    value.differentialMode = slot->record.powertrain.differentialMode;
    clearError();
    return true;
}

std::size_t VehicleSystem::forwardGearCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.powertrain.forwardGearRatios.size() : 0;
}

} // namespace heritage::vehicles
