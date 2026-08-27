#pragma once

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct ChassisAudioMix
{
    float gain = 0.0f;
    float pitch = 1.0f;
    float openness = 1.0f;
};

ChassisAudioMix evaluateChassisAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float suspensionActivity,
    bool interior,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
