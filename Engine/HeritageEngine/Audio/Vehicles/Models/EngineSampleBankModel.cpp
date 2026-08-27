#include "EngineSampleBankModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {

std::vector<EngineSampleVoiceMix> evaluateEngineSampleBank(
    const VehicleAudioDefinition& definition,
    float engineRpm,
    float proceduralExhaustGain,
    VehicleAudioDetailLevel detail)
{
    std::vector<EngineSampleVoiceMix> mix(definition.samples.loops.size());
    if (mix.empty())
        return mix;

    const float rpm = std::max(engineRpm, definition.idleRpm);
    std::vector<float> weights(mix.size(), 0.0f);
    if (detail == VehicleAudioDetailLevel::Full)
    {
        if (rpm <= definition.samples.loops.front().referenceRpm)
        {
            weights.front() = 1.0f;
        }
        else if (rpm >= definition.samples.loops.back().referenceRpm)
        {
            weights.back() = 1.0f;
        }
        else
        {
            for (std::size_t upper = 1; upper < mix.size(); ++upper)
            {
                const float upperRpm = definition.samples.loops[upper].referenceRpm;
                if (rpm > upperRpm)
                    continue;
                const float lowerRpm = definition.samples.loops[upper - 1].referenceRpm;
                float blend = std::clamp(
                    (rpm - lowerRpm) / std::max(upperRpm - lowerRpm, 1.0f),
                    0.0f,
                    1.0f);
                blend = blend * blend * (3.0f - 2.0f * blend);
                // Equal-power interpolation keeps perceived loudness stable
                // through a band crossing. Linear amplitudes produced a dip
                // followed by a rise and made unrelated contact-microphone
                // phases sound like an electronic tremolo.
                weights[upper - 1] = std::sqrt(1.0f - blend);
                weights[upper] = std::sqrt(blend);
                break;
            }
        }
    }
    else if (detail == VehicleAudioDetailLevel::Reduced)
    {
        std::size_t nearest = 0;
        float nearestDistance = std::abs(
            rpm - definition.samples.loops.front().referenceRpm);
        for (std::size_t index = 1; index < mix.size(); ++index)
        {
            const float candidate = std::abs(
                rpm - definition.samples.loops[index].referenceRpm);
            if (candidate < nearestDistance)
            {
                nearest = index;
                nearestDistance = candidate;
            }
        }
        weights[nearest] = 1.0f;
    }

    const float bankGain = std::clamp(definition.samples.gain, 0.0f, 1.0f);
    for (std::size_t index = 0; index < mix.size(); ++index)
    {
        const VehicleEngineSample& sample = definition.samples.loops[index];
        // A recorded engine must remain audible at idle even though the
        // procedural exhaust model deliberately becomes quiet off load.
        const float recordedAudibility = std::max(
            std::clamp(proceduralExhaustGain, 0.0f, 1.0f), 0.30f);
        mix[index].gain = recordedAudibility
            * bankGain * std::clamp(sample.gain, 0.0f, 2.0f) * weights[index];
        mix[index].pitch = std::clamp(
            engineRpm / std::max(sample.referenceRpm, 1.0f),
            0.75f,
            1.35f);
    }
    return mix;
}

} // namespace heritage::audio::vehicles
