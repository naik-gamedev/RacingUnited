#include "DrivetrainAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

DrivetrainAudioMix evaluateDrivetrainAudio(
    const VehicleAudioDefinition& definition,
    float speedMetersPerSecond,
    float engineLoad,
    float clutchSlipRpm,
    int gear,
    bool interior,
    VehicleAudioDetailLevel detail)
{
    DrivetrainAudioMix mix;
    if (detail != VehicleAudioDetailLevel::Full)
        return mix;
    const float speed = std::abs(speedMetersPerSecond);
    const float load = std::clamp(engineLoad, 0.0f, 1.0f);
    const float slip = std::clamp(std::abs(clutchSlipRpm) / 1800.0f, 0.0f, 1.0f);
    mix.gain = definition.gains.transmission
        * std::clamp(speed / 35.0f, 0.0f, 1.0f)
        * (0.28f + 0.52f * load + 0.20f * slip);
    mix.pitch = std::clamp(0.45f + speed / 18.0f + std::abs(gear) * 0.06f, 0.45f, 4.5f);
    mix.openness = interior ? 0.46f : 0.82f;
    if (interior)
        mix.gain *= 1.22f;
    return mix;
}

} // namespace heritage::audio::vehicles
