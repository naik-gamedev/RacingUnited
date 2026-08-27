#include "PhysicsRegressionCommon.hpp"

#include "../Audio/Weather/Models/WeatherAudioModel.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace heritage::tests {

bool weatherAudioMixIsPhysicalSmoothAndBounded()
{
    using namespace heritage::audio::weather;
    WeatherAudioModelState state;
    WeatherAudioMix previous;
    bool changedSmoothly = true;
    for (int frame = 0; frame < 600; ++frame)
    {
        const auto mix = evaluateWeatherAudio(
            { true, 25.0f, 12.0f, 1.0f / 120.0f }, state);
        const std::array values{
            mix.lightRain, mix.mediumRain, mix.heavyRain,
            mix.stormRain, mix.wind };
        changedSmoothly = changedSmoothly
            && std::all_of(values.begin(), values.end(), [](float value)
            {
                return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
            })
            && std::abs(mix.heavyRain - previous.heavyRain) < 0.03f
            && std::abs(mix.wind - previous.wind) < 0.03f;
        previous = mix;
    }
    const float activeRainEnergy = state.mix.lightRain * state.mix.lightRain
        + state.mix.mediumRain * state.mix.mediumRain
        + state.mix.heavyRain * state.mix.heavyRain
        + state.mix.stormRain * state.mix.stormRain;
    const float wetWind = state.mix.wind;

    // The authored release time is deliberately longer than the attack so a
    // passing shower recedes naturally instead of audibly gating off.
    for (int frame = 0; frame < 2400; ++frame)
        evaluateWeatherAudio({ false, 250.0f, 80.0f, 1.0f / 120.0f }, state);

    return changedSmoothly
        && activeRainEnergy > 0.80f
        && activeRainEnergy < 1.05f
        && wetWind > 0.20f
        && state.mix.lightRain < 0.01f
        && state.mix.mediumRain < 0.01f
        && state.mix.heavyRain < 0.01f
        && state.mix.stormRain < 0.01f
        && state.mix.wind < 0.01f;
}

} // namespace heritage::tests
