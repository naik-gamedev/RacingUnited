#pragma once

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct TireAudioMix
{
    float gain = 0.0f;
    float pitch = 1.0f;
    float openness = 1.0f;
};

TireAudioMix evaluateTireAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float averageSlip,
    float wetness,
    bool interior,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
