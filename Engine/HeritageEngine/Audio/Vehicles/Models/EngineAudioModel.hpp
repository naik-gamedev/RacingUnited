#pragma once

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct EngineAudioMix
{
    float pitch = 1.0f;
    float exhaustGain = 0.0f;
    float intakeGain = 0.0f;
    float mechanicalGain = 0.0f;
    float exhaustOpenness = 1.0f;
    float intakeOpenness = 1.0f;
    float mechanicalOpenness = 1.0f;
};

EngineAudioMix evaluateEngineAudio(
    const VehicleAudioDefinition& definition,
    float rpm,
    float normalizedLoad,
    bool interior,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
