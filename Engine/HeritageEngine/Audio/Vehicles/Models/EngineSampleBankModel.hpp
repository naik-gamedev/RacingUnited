#pragma once

#include <vector>

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct EngineSampleVoiceMix
{
    float gain = 0.0f;
    float pitch = 1.0f;
};

// Returns one mix instruction per authored loop. Full detail crossfades two
// neighboring RPM bands; reduced detail keeps only the nearest band; crowd
// and silent detail spend no recorded voices.
std::vector<EngineSampleVoiceMix> evaluateEngineSampleBank(
    const VehicleAudioDefinition& definition,
    float engineRpm,
    float proceduralExhaustGain,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
