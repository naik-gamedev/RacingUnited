#include "VehicleAudioEventModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

VehicleAudioEventFrame evaluateVehicleAudioEvents(
    const VehicleAudioEventInput& input,
    VehicleAudioEventState& state)
{
    VehicleAudioEventFrame frame;
    state.suspensionCooldownSeconds = std::max(
        0.0f,
        state.suspensionCooldownSeconds
            - std::clamp(input.deltaSeconds, 0.0f, 1.0f));
    const float elapsed = std::clamp(input.deltaSeconds, 0.0f, 1.0f);
    state.limiterCooldownSeconds = std::max(
        0.0f, state.limiterCooldownSeconds - elapsed);
    state.overrunCooldownSeconds = std::max(
        0.0f, state.overrunCooldownSeconds - elapsed);

    const float activity = std::isfinite(input.suspensionActivity)
        ? std::max(input.suspensionActivity, 0.0f) : 0.0f;
    if (state.initialized)
    {
        frame.gearShift = input.gear != state.previousGear;
        const bool suspensionImpact = activity > 0.38f
            && state.previousSuspensionActivity <= 0.28f
            && state.suspensionCooldownSeconds <= 0.0f;
        if (suspensionImpact)
        {
            frame.suspensionHeavy = activity >= 1.0f;
            frame.suspensionLight = !frame.suspensionHeavy;
            state.suspensionCooldownSeconds = frame.suspensionHeavy
                ? 0.28f : 0.18f;
        }

        const float redline = std::max(input.redlineRpm, 500.0f);
        if (input.engineRpm >= redline - 25.0f
            && state.limiterCooldownSeconds <= 0.0f)
        {
            frame.revLimiterCut = true;
            state.limiterCooldownSeconds = 0.105f;
        }
        if (state.previousEngineTorqueNm > 8.0f
            && input.engineTorqueNm < -4.0f
            && input.engineRpm > 2600.0f
            && state.overrunCooldownSeconds <= 0.0f)
        {
            frame.overrunPop = true;
            state.overrunCooldownSeconds = 0.22f;
        }
    }

    state.initialized = true;
    state.previousGear = input.gear;
    state.previousSuspensionActivity = activity;
    state.previousEngineTorqueNm = std::isfinite(input.engineTorqueNm)
        ? input.engineTorqueNm : 0.0f;
    return frame;
}

} // namespace heritage::audio::vehicles
