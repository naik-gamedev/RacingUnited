#pragma once

#include "../VehicleAudioTypes.hpp"

namespace heritage::audio::vehicles {

struct DrivetrainAudioMix
{
    float gain = 0.0f;
    float pitch = 1.0f;
    float openness = 1.0f;
};

DrivetrainAudioMix evaluateDrivetrainAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float engineLoad,
    float clutchSlipRpm,
    int gear,
    bool interior,
    VehicleAudioDetailLevel detail);

} // namespace heritage::audio::vehicles
