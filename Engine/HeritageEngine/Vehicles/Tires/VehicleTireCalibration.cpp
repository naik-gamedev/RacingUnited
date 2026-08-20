#include "../VehicleSystem.hpp"

#include <algorithm>

namespace heritage::vehicles {

bool VehicleSystem::wheelTireCalibrationSweep(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const std::string& sweepName,
    TireCalibrationSweepResult& value) const
{
    value = {};
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.RunTireCalibrationSweep received an invalid vehicle handle or wheel index.");
        return false;
    }
    if (sweepName.empty())
    {
        setError("Vehicle.RunTireCalibrationSweep requires a named canonical sweep.");
        return false;
    }

    const WheelRecord& wheel = slot->record.wheels[wheelIndex];
    const auto descriptions = standardTireCalibrationSweeps(
        wheel.tireModel,
        static_cast<VehicleScalar>(wheel.description.radius));
    const auto found = std::find_if(
        descriptions.begin(),
        descriptions.end(),
        [&sweepName](const TireCalibrationSweepDescription& description) {
            return description.name == sweepName;
        });
    if (found == descriptions.end())
    {
        setError("Vehicle.RunTireCalibrationSweep received an unknown sweep or an invalid fitted tire.");
        return false;
    }

    value = runTireCalibrationSweep(wheel.tireModel, *found);
    if (!value.valid)
    {
        setError("Vehicle.RunTireCalibrationSweep: " + value.error);
        return false;
    }

    clearError();
    return true;
}

bool VehicleSystem::wheelTireScenario(
    VehicleHandle handle,
    std::size_t wheelIndex,
    const std::string& scenarioName,
    TireScenarioResult& value) const
{
    value = {};
    const Slot* slot = resolve(handle);
    if (!slot || wheelIndex >= slot->record.wheels.size())
    {
        setError("Vehicle.RunTireScenario received an invalid vehicle handle or wheel index.");
        return false;
    }
    const WheelRecord& wheel = slot->record.wheels[wheelIndex];
    value = runStandardTireScenario(
        wheel.tireModel,
        static_cast<VehicleScalar>(wheel.description.radius),
        scenarioName);
    if (!value.valid)
    {
        setError("Vehicle.RunTireScenario: " + value.error);
        return false;
    }
    clearError();
    return true;
}

} // namespace heritage::vehicles
