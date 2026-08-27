#include "VehicleAudioSynthesis.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace heritage::audio::vehicles {
namespace {

constexpr float pi = 3.14159265358979323846f;

float softClip(float value)
{
    return std::tanh(value);
}

std::uint32_t xorshift(std::uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

float noise(std::uint32_t& state)
{
    return static_cast<float>(xorshift(state) & 0x00ffffffU)
        / static_cast<float>(0x007fffffU) - 1.0f;
}

float wrapUnit(float value)
{
    value -= std::floor(value);
    return value < 0.0f ? value + 1.0f : value;
}

float engineOrder(float revolutionsPerSecond, float order, float time, float phase = 0.0f)
{
    return std::sin(2.0f * pi * revolutionsPerSecond * order * time + phase);
}

float pulseTrain(
    const VehicleAudioDefinition& definition,
    float cyclePhase,
    float sharpness,
    float pathImbalance)
{
    const int cylinderCount = (std::max)(definition.cylinderCount, 1);
    const auto& firingOrder = definition.engineAcoustics.firingOrder;
    const float decay = 4.0f + 15.0f * std::clamp(sharpness, 0.0f, 1.0f);
    const float variation = std::clamp(
        definition.engineAcoustics.combustionVariation, 0.0f, 0.20f);
    float result = 0.0f;
    for (int event = 0; event < cylinderCount; ++event)
    {
        int cylinder = event;
        if (firingOrder.size() == static_cast<std::size_t>(cylinderCount))
            cylinder = std::clamp(firingOrder[static_cast<std::size_t>(event)] - 1,
                0, cylinderCount - 1);
        const float cylinderPosition = cylinderCount > 1
            ? (2.0f * static_cast<float>(cylinder)
                / static_cast<float>(cylinderCount - 1) - 1.0f)
            : 0.0f;
        const float phaseSkew = pathImbalance * cylinderPosition
            / static_cast<float>(cylinderCount);
        const float eventPhase = wrapUnit(
            static_cast<float>(event) / static_cast<float>(cylinderCount)
            + phaseSkew);
        const float ageInEventIntervals = wrapUnit(cyclePhase - eventPhase)
            * static_cast<float>(cylinderCount);
        const float cylinderColour = 1.0f + variation
            * std::sin(1.61803398875f * static_cast<float>(cylinder + 1));
        result += cylinderColour * std::exp(-ageInEventIntervals * decay);
    }
    return result;
}

void closeLoop(GeneratedMonoAudio& audio)
{
    const std::size_t blend = (std::min<std::size_t>)(1024, audio.samples.size() / 8);
    if (blend == 0)
        return;
    for (std::size_t index = 0; index < blend; ++index)
    {
        const float amount = static_cast<float>(index) / static_cast<float>(blend);
        const std::size_t tail = audio.samples.size() - blend + index;
        const float shared = audio.samples[index] * amount
            + audio.samples[tail] * (1.0f - amount);
        audio.samples[index] = shared;
        audio.samples[tail] = shared;
    }
}

void conditionLoop(GeneratedMonoAudio& audio, float targetPeak)
{
    closeLoop(audio);
    if (audio.samples.empty())
        return;

    double sum = 0.0;
    for (const float sample : audio.samples)
        sum += sample;
    const float mean = static_cast<float>(sum / static_cast<double>(audio.samples.size()));
    float peak = 0.0f;
    for (float& sample : audio.samples)
    {
        sample -= mean;
        peak = (std::max)(peak, std::abs(sample));
    }
    if (peak <= 1.0e-6f)
        return;
    const float gain = targetPeak / peak;
    for (float& sample : audio.samples)
        sample = softClip(sample * gain);
}

void conditionOneShot(GeneratedMonoAudio& audio, float targetPeak)
{
    if (audio.samples.empty())
        return;
    float low = 0.0f;
    float peak = 0.0f;
    const std::size_t fadeSamples = (std::min<std::size_t>)(
        audio.samples.size() / 3,
        static_cast<std::size_t>(audio.sampleRate / 50));
    for (std::size_t index = 0; index < audio.samples.size(); ++index)
    {
        low += 0.012f * (audio.samples[index] - low);
        float value = audio.samples[index] - low;
        const float attack = std::clamp(
            static_cast<float>(index) / static_cast<float>(audio.sampleRate / 500),
            0.0f,
            1.0f);
        const std::size_t remaining = audio.samples.size() - 1 - index;
        const float release = fadeSamples > 0
            ? std::clamp(static_cast<float>(remaining)
                / static_cast<float>(fadeSamples), 0.0f, 1.0f)
            : 1.0f;
        audio.samples[index] = value * attack * release;
        peak = (std::max)(peak, std::abs(audio.samples[index]));
    }
    if (peak <= 1.0e-6f)
        return;
    const float gain = targetPeak / peak;
    for (float& sample : audio.samples)
        sample = softClip(sample * gain);
}

} // namespace

GeneratedMonoAudio synthesizeVehicleLayer(
    const VehicleAudioDefinition& definition,
    SynthesizedVehicleLayer layer)
{
    GeneratedMonoAudio audio;
    audio.sampleRate = 48000;
    audio.samples.resize(audio.sampleRate); // one seamless second

    const float rpm = std::clamp(definition.referenceRpm, 600.0f, 2400.0f);
    const float revolutionsPerSecond = rpm / 60.0f;
    const int cycleRevolutions = (std::max)(definition.cycleRevolutions, 1);
    const float firingOrder = static_cast<float>((std::max)(definition.cylinderCount, 1))
        / static_cast<float>(cycleRevolutions);
    const auto& acoustics = definition.engineAcoustics;
    const float compressionCharacter = std::clamp(
        (acoustics.compressionRatio - 7.0f) / 8.0f, 0.0f, 1.0f);
    const float displacementCharacter = std::clamp(
        std::sqrt(acoustics.displacementLiters / 2.0f), 0.35f, 2.2f);
    std::uint32_t randomState = 0x9e3779b9U
        ^ static_cast<std::uint32_t>(definition.cylinderCount * 7919)
        ^ static_cast<std::uint32_t>(layer) * 104729U;
    float lowNoise = 0.0f;
    float previousNoise = 0.0f;

    for (std::size_t index = 0; index < audio.samples.size(); ++index)
    {
        const float time = static_cast<float>(index) / static_cast<float>(audio.sampleRate);
        const float cyclePhase = wrapUnit(
            time * revolutionsPerSecond / static_cast<float>(cycleRevolutions));
        const float exhaustPulse = pulseTrain(
            definition,
            cyclePhase,
            acoustics.exhaustPulseSharpness + 0.12f * compressionCharacter,
            acoustics.exhaustHeaderImbalance);
        const float intakePulse = pulseTrain(
            definition,
            cyclePhase,
            acoustics.intakePulseSharpness,
            0.35f * acoustics.exhaustHeaderImbalance);
        const float crank = engineOrder(revolutionsPerSecond, 1.0f, time);
        const float combustion = engineOrder(revolutionsPerSecond, firingOrder, time);
        const float rawNoise = noise(randomState);
        lowNoise += 0.035f * (rawNoise - lowNoise);
        const float highNoise = rawNoise - previousNoise;
        previousNoise = rawNoise;

        float value = 0.0f;
        switch (layer)
        {
        case SynthesizedVehicleLayer::Exhaust:
            value = (0.78f + 0.18f * compressionCharacter) * exhaustPulse
                + 0.34f * combustion
                + 0.18f * displacementCharacter * crank
                + 0.15f * engineOrder(revolutionsPerSecond, firingOrder * 2.0f, time, 0.35f)
                + 0.07f * engineOrder(revolutionsPerSecond, firingOrder * 3.0f, time, 0.62f)
                + 0.055f * lowNoise;
            break;
        case SynthesizedVehicleLayer::Intake:
            value = 0.48f * intakePulse
                + 0.30f * combustion
                + 0.24f * engineOrder(
                    revolutionsPerSecond,
                    acoustics.intakeResonanceOrder,
                    time,
                    0.48f)
                + 0.12f * engineOrder(revolutionsPerSecond, firingOrder * 2.0f, time)
                + 0.16f * lowNoise;
            break;
        case SynthesizedVehicleLayer::Mechanical:
            value = 0.30f * crank
                + 0.28f * combustion
                + acoustics.mechanicalOrderGain
                    * (0.72f * engineOrder(revolutionsPerSecond, 4.0f, time, 0.25f)
                        + 0.38f * engineOrder(revolutionsPerSecond, 8.0f, time, 0.65f))
                + 0.075f * highNoise;
            break;
        case SynthesizedVehicleLayer::Transmission:
            value = 0.62f * std::sin(2.0f * pi * 180.0f * time)
                + 0.25f * std::sin(2.0f * pi * 360.0f * time)
                + 0.08f * highNoise;
            break;
        case SynthesizedVehicleLayer::Tire:
            value = 0.70f * lowNoise + 0.22f * highNoise;
            break;
        case SynthesizedVehicleLayer::Wind:
            value = 0.86f * lowNoise + 0.08f * highNoise;
            break;
        case SynthesizedVehicleLayer::Chassis:
            value = 0.64f * lowNoise
                + 0.16f * std::sin(2.0f * pi * 42.0f * time)
                + 0.10f * highNoise;
            break;
        }
        audio.samples[index] = value;
    }

    const bool engineLayer = layer == SynthesizedVehicleLayer::Exhaust
        || layer == SynthesizedVehicleLayer::Intake
        || layer == SynthesizedVehicleLayer::Mechanical;
    conditionLoop(audio, engineLayer ? 0.90f : 0.82f);
    return audio;
}

GeneratedMonoAudio synthesizeVehicleTransient(
    const VehicleAudioDefinition& definition,
    SynthesizedVehicleTransient transient)
{
    GeneratedMonoAudio audio;
    audio.sampleRate = 48000;
    const float durationSeconds = transient == SynthesizedVehicleTransient::RevLimiterCut
        ? 0.105f : 0.185f;
    audio.samples.resize(static_cast<std::size_t>(
        static_cast<float>(audio.sampleRate) * durationSeconds));

    const float rpm = std::clamp(definition.referenceRpm, 600.0f, 2400.0f);
    const float crankHz = rpm / 60.0f;
    const float firingOrder = static_cast<float>((std::max)(definition.cylinderCount, 1))
        / static_cast<float>((std::max)(definition.cycleRevolutions, 1));
    const float fireHz = crankHz * firingOrder;
    const float compressionCharacter = std::clamp(
        (definition.engineAcoustics.compressionRatio - 7.0f) / 8.0f,
        0.0f,
        1.0f);
    std::uint32_t randomState = 0x73a91e2dU
        ^ static_cast<std::uint32_t>(definition.cylinderCount * 1543)
        ^ static_cast<std::uint32_t>(transient) * 65537U;
    float lowNoise = 0.0f;
    for (std::size_t index = 0; index < audio.samples.size(); ++index)
    {
        const float time = static_cast<float>(index)
            / static_cast<float>(audio.sampleRate);
        const float random = noise(randomState);
        lowNoise += 0.055f * (random - lowNoise);
        float value = 0.0f;
        if (transient == SynthesizedVehicleTransient::RevLimiterCut)
        {
            const float envelope = std::exp(-time * 28.0f);
            value = envelope * (
                (0.64f + 0.20f * compressionCharacter)
                    * std::sin(2.0f * pi * fireHz * time)
                + 0.34f * std::sin(2.0f * pi * fireHz * 2.0f * time + 0.35f)
                + 0.24f * lowNoise);
        }
        else
        {
            const float envelope = std::exp(-time * 17.0f);
            const float body = std::sin(2.0f * pi * crankHz * 0.72f * time)
                + 0.42f * std::sin(2.0f * pi * fireHz * time + 0.8f);
            value = envelope * (0.72f * body + 0.52f * lowNoise);
        }
        audio.samples[index] = value;
    }
    conditionOneShot(audio, 0.94f);
    return audio;
}

float signalPeak(const GeneratedMonoAudio& audio)
{
    float peak = 0.0f;
    for (const float sample : audio.samples)
        peak = (std::max)(peak, std::abs(sample));
    return peak;
}

float signalRms(const GeneratedMonoAudio& audio)
{
    if (audio.samples.empty())
        return 0.0f;
    double energy = 0.0;
    for (const float sample : audio.samples)
        energy += static_cast<double>(sample) * static_cast<double>(sample);
    return static_cast<float>(std::sqrt(energy / static_cast<double>(audio.samples.size())));
}

} // namespace heritage::audio::vehicles
