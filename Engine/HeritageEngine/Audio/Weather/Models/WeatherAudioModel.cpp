#include "WeatherAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::weather {
namespace {

float smoothstep(float lower, float upper, float value)
{
    if (upper <= lower)
        return value >= upper ? 1.0f : 0.0f;
    const float x = std::clamp((value - lower) / (upper - lower), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void approach(float& current, float target, float deltaSeconds)
{
    const float timeConstant = target > current ? 1.25f : 3.50f;
    const float alpha = 1.0f - std::exp(
        -std::clamp(deltaSeconds, 0.0f, 0.25f) / timeConstant);
    current += (target - current) * alpha;
    current = std::clamp(current, 0.0f, 1.0f);
}

} // namespace

WeatherAudioMix evaluateWeatherAudio(
    const WeatherAudioModelInput& input,
    WeatherAudioModelState& state)
{
    const float rain = input.enabled
        ? std::clamp(input.rainMmPerHour, 0.0f, 250.0f)
        : 0.0f;
    const float windSpeed = input.enabled
        ? std::clamp(input.windMetersPerSecond, 0.0f, 80.0f)
        : 0.0f;

    // Logarithmic rain coordinate preserves useful resolution for drizzle while
    // still covering a 150 mm/h deluge. Adjacent recordings use equal-power
    // crossfades, preventing level holes or four simultaneous full-gain loops.
    const float rainPresence = smoothstep(0.05f, 1.5f, rain);
    const float coordinate = rain > 0.0f
        ? 3.0f * std::log1p(rain) / std::log1p(150.0f)
        : 0.0f;
    const int lower = std::clamp(static_cast<int>(std::floor(coordinate)), 0, 3);
    const int upper = std::min(lower + 1, 3);
    const float fraction = std::clamp(coordinate - static_cast<float>(lower), 0.0f, 1.0f);
    float targetRain[4]{};
    targetRain[lower] = std::sqrt(1.0f - fraction) * rainPresence;
    targetRain[upper] = (upper == lower ? 0.0f : std::sqrt(fraction) * rainPresence);

    approach(state.mix.lightRain, targetRain[0], input.deltaSeconds);
    approach(state.mix.mediumRain, targetRain[1], input.deltaSeconds);
    approach(state.mix.heavyRain, targetRain[2], input.deltaSeconds);
    approach(state.mix.stormRain, targetRain[3], input.deltaSeconds);

    const float windTarget = smoothstep(1.5f, 24.0f, windSpeed);
    approach(state.mix.wind, windTarget, input.deltaSeconds);
    state.mix.windPitch = 0.90f + 0.20f * smoothstep(0.0f, 35.0f, windSpeed);
    return state.mix;
}

} // namespace heritage::audio::weather
