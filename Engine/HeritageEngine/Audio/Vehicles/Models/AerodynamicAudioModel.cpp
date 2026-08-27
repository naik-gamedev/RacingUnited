#include "AerodynamicAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

AerodynamicAudioMix evaluateAerodynamicAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    bool interior,
    VehicleAudioDetailLevel detail)
{
    AerodynamicAudioMix mix;
    if (detail != VehicleAudioDetailLevel::Full)
        return mix;
    const float speed = std::abs(speedMetersPerSecond);
    const float dynamicPressure = std::clamp(speed * speed / (55.0f * 55.0f), 0.0f, 1.0f);
    mix.gain = definition.gains.wind * dynamicPressure;
    mix.pitch = std::clamp(0.60f + speed / 65.0f, 0.55f, 1.65f);
    mix.openness = interior ? 0.25f : 0.90f;
    if (interior)
        mix.gain *= 0.58f;
    return mix;
}

} // namespace heritage::audio::vehicles
