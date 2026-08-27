#include "TireAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

TireAudioMix evaluateTireAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float averageSlip,
    float wetness,
    bool interior,
    VehicleAudioDetailLevel detail)
{
    TireAudioMix mix;
    if (detail == VehicleAudioDetailLevel::Crowd
        || detail == VehicleAudioDetailLevel::Silent)
        return mix;
    const float speed = std::abs(speedMetersPerSecond);
    const float rolling = std::clamp(speed / 32.0f, 0.0f, 1.0f);
    const float scrub = std::clamp(std::abs(averageSlip) / 0.28f, 0.0f, 1.0f);
    const float water = std::clamp(wetness, 0.0f, 1.0f);
    mix.gain = definition.gains.tires * rolling
        * (0.18f + 0.72f * scrub + 0.20f * water);
    mix.pitch = std::clamp(0.55f + speed / 42.0f + scrub * 0.30f, 0.45f, 2.25f);
    mix.openness = interior ? 0.34f : 0.78f;
    if (interior)
        mix.gain *= 0.72f;
    return mix;
}

} // namespace heritage::audio::vehicles
