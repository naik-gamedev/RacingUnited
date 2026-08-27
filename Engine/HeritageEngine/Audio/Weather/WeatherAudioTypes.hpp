#pragma once

#include <cstdint>
#include <filesystem>

namespace heritage::audio::weather {

using WeatherSoundHandle = std::uint64_t;
inline constexpr WeatherSoundHandle kInvalidWeatherSoundHandle = 0;

struct WeatherAudioDefinition
{
    std::filesystem::path lightRainPath;
    std::filesystem::path mediumRainPath;
    std::filesystem::path heavyRainPath;
    std::filesystem::path stormRainPath;
    std::filesystem::path windPath;
    float rainGain = 0.70f;
    float windGain = 0.45f;
};

struct WeatherAudioTelemetry
{
    bool enabled = false;
    float rainMmPerHour = 0.0f;
    float windMetersPerSecond = 0.0f;
    float lightRainGain = 0.0f;
    float mediumRainGain = 0.0f;
    float heavyRainGain = 0.0f;
    float stormRainGain = 0.0f;
    float windGain = 0.0f;
    int activeVoiceCount = 0;
};

} // namespace heritage::audio::weather
