#pragma once

namespace heritage::audio::weather {

struct WeatherAudioModelInput
{
    bool enabled = false;
    float rainMmPerHour = 0.0f;
    float windMetersPerSecond = 0.0f;
    float deltaSeconds = 0.0f;
};

struct WeatherAudioMix
{
    float lightRain = 0.0f;
    float mediumRain = 0.0f;
    float heavyRain = 0.0f;
    float stormRain = 0.0f;
    float wind = 0.0f;
    float windPitch = 1.0f;
};

struct WeatherAudioModelState
{
    WeatherAudioMix mix;
};

// Maps physical weather authority into bounded, smoothly changing ambience
// gains. Playback and asset choice remain separate module/runtime concerns.
WeatherAudioMix evaluateWeatherAudio(
    const WeatherAudioModelInput& input,
    WeatherAudioModelState& state);

} // namespace heritage::audio::weather
