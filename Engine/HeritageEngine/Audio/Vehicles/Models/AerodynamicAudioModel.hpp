#pragma once

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct AerodynamicAudioMix
{
    float gain = 0.0f;
    float pitch = 1.0f;
    float openness = 1.0f;
};

AerodynamicAudioMix evaluateAerodynamicAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    bool interior,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
