#include "../../VehicleSystem.hpp"
#include "../../VehicleSystemInternal.hpp"
#include "../../Tires/TireSlipDynamics.hpp"
#include "../../Tires/TireContactPatch.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles {
using namespace vehicle_system_detail;


bool VehicleSystem::setAntiRollBar(
    VehicleHandle handle,
    std::size_t antiRollBarIndex,
    const SuspensionAntiRollBarDescription& value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Vehicle.SetAntiRollBar received an invalid vehicle handle.");
        return false;
    }
    if (!validSuspensionAntiRollBarDescription(value)
        || value.leftWheelIndex >= slot->record.wheels.size()
        || value.rightWheelIndex >= slot->record.wheels.size()
        || value.leftWheelIndex == value.rightWheelIndex)
    {
        setError("Vehicle.SetAntiRollBar received invalid wheel indices or anti-roll-bar parameters.");
        return false;
    }
    if (antiRollBarIndex > slot->record.antiRollBars.size())
    {
        setError("Vehicle.SetAntiRollBar indices must be contiguous.");
        return false;
    }

    AntiRollBarRecord record;
    record.description = value;
    if (antiRollBarIndex == slot->record.antiRollBars.size())
        slot->record.antiRollBars.push_back(record);
    else
        slot->record.antiRollBars[antiRollBarIndex] = record;
    clearError();
    return true;
}

bool VehicleSystem::antiRollBar(
    VehicleHandle handle,
    std::size_t antiRollBarIndex,
    SuspensionAntiRollBarDescription& description,
    SuspensionAntiRollBarOutput& state) const
{
    const Slot* slot = resolve(handle);
    if (!slot || antiRollBarIndex >= slot->record.antiRollBars.size())
    {
        setError("Vehicle.GetAntiRollBar received an invalid vehicle handle or anti-roll-bar index.");
        return false;
    }
    description = slot->record.antiRollBars[antiRollBarIndex].description;
    state = slot->record.antiRollBars[antiRollBarIndex].state;
    clearError();
    return true;
}

std::size_t VehicleSystem::antiRollBarCount(VehicleHandle handle) const
{
    const Slot* slot = resolve(handle);
    return slot ? slot->record.antiRollBars.size() : 0;
}

} // namespace heritage::vehicles
