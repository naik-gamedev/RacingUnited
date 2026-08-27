#include "ChassisAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

ChassisAudioMix evaluateChassisAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float suspensionActivity,
    bool interior,
    VehicleAudioDetailLevel detail)
{
    ChassisAudioMix mix;
    if (detail != VehicleAudioDetailLevel::Full)
        return mix;
    const float speed = std::clamp(std::abs(speedMetersPerSecond) / 30.0f, 0.0f, 1.0f);
    const float activity = std::clamp(suspensionActivity / 1.5f, 0.0f, 1.0f);
    mix.gain = definition.gains.chassis * (0.12f * speed + 0.88f * activity);
    mix.pitch = 0.72f + 0.46f * speed;
    mix.openness = interior ? 0.52f : 0.82f;
    if (interior)
        mix.gain *= 1.35f;
    return mix;
}

} // namespace heritage::audio::vehicles
