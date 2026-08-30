#include "EngineSoundLabDsp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>

namespace heritage::audio::lab {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float dbToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

float gainToDb(float gain)
{
    if (!std::isfinite(gain) || gain <= 1.0e-12f)
        return -120.0f;
    return 20.0f * std::log10(gain);
}

float goertzelPower(
    const std::vector<float>& mono,
    std::size_t start,
    std::size_t count,
    float sampleRate,
    float frequency)
{
    if (mono.empty() || count < 8 || sampleRate <= 0.0f)
        return 0.0f;
    frequency = std::clamp(frequency, 10.0f, sampleRate * 0.47f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float coefficient = 2.0f * std::cos(omega);
    float s0 = 0.0f;
    float s1 = 0.0f;
    float s2 = 0.0f;
    const std::size_t end = std::min(start + count, mono.size());
    const std::size_t actual = end > start ? end - start : 0;
    if (actual < 8)
        return 0.0f;
    for (std::size_t i = 0; i < actual; ++i)
    {
        const float phase = actual > 1
            ? static_cast<float>(i) / static_cast<float>(actual - 1)
            : 0.0f;
        const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * phase);
        s0 = mono[start + i] * window + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const float power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::max(0.0f, power / static_cast<float>(actual * actual));
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

struct Biquad
{
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    std::array<float, 2> x1{};
    std::array<float, 2> x2{};
    std::array<float, 2> y1{};
    std::array<float, 2> y2{};

    float process(float x, int channel)
    {
        const float y = b0 * x + b1 * x1[channel] + b2 * x2[channel]
            - a1 * y1[channel] - a2 * y2[channel];
        x2[channel] = x1[channel];
        x1[channel] = x;
        y2[channel] = y1[channel];
        y1[channel] = y;
        return y;
    }
};

Biquad normalizedBiquad(
    float b0, float b1, float b2,
    float a0, float a1, float a2)
{
    Biquad filter;
    const float inverse = std::abs(a0) > 1.0e-8f ? 1.0f / a0 : 1.0f;
    filter.b0 = b0 * inverse;
    filter.b1 = b1 * inverse;
    filter.b2 = b2 * inverse;
    filter.a1 = a1 * inverse;
    filter.a2 = a2 * inverse;
    return filter;
}

float safeFrequency(float value, float sampleRate)
{
    return std::clamp(value, 10.0f, sampleRate * 0.47f);
}

Biquad lowPass(float frequency, float q, float sampleRate)
{
    frequency = safeFrequency(frequency, sampleRate);
    q = std::clamp(q, 0.15f, 8.0f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float alpha = sine / (2.0f * q);
    return normalizedBiquad(
        (1.0f - cosine) * 0.5f,
        1.0f - cosine,
        (1.0f - cosine) * 0.5f,
        1.0f + alpha,
        -2.0f * cosine,
        1.0f - alpha);
}

Biquad highPass(float frequency, float q, float sampleRate)
{
    frequency = safeFrequency(frequency, sampleRate);
    q = std::clamp(q, 0.15f, 8.0f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float alpha = sine / (2.0f * q);
    return normalizedBiquad(
        (1.0f + cosine) * 0.5f,
        -(1.0f + cosine),
        (1.0f + cosine) * 0.5f,
        1.0f + alpha,
        -2.0f * cosine,
        1.0f - alpha);
}

Biquad peaking(float frequency, float q, float gainDb, float sampleRate)
{
    frequency = safeFrequency(frequency, sampleRate);
    q = std::clamp(q, 0.15f, 8.0f);
    gainDb = std::clamp(gainDb, -24.0f, 24.0f);
    const float A = std::pow(10.0f, gainDb / 40.0f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float alpha = sine / (2.0f * q);
    return normalizedBiquad(
        1.0f + alpha * A,
        -2.0f * cosine,
        1.0f - alpha * A,
        1.0f + alpha / A,
        -2.0f * cosine,
        1.0f - alpha / A);
}

Biquad lowShelf(float frequency, float gainDb, float sampleRate)
{
    frequency = safeFrequency(frequency, sampleRate);
    const float A = std::pow(10.0f, std::clamp(gainDb, -24.0f, 24.0f) / 40.0f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float rootA = std::sqrt(A);
    const float alpha = sine * std::sqrt(2.0f) * 0.5f;
    const float twoRootAlpha = 2.0f * rootA * alpha;
    return normalizedBiquad(
        A * ((A + 1.0f) - (A - 1.0f) * cosine + twoRootAlpha),
        2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosine),
        A * ((A + 1.0f) - (A - 1.0f) * cosine - twoRootAlpha),
        (A + 1.0f) + (A - 1.0f) * cosine + twoRootAlpha,
        -2.0f * ((A - 1.0f) + (A + 1.0f) * cosine),
        (A + 1.0f) + (A - 1.0f) * cosine - twoRootAlpha);
}

Biquad highShelf(float frequency, float gainDb, float sampleRate)
{
    frequency = safeFrequency(frequency, sampleRate);
    const float A = std::pow(10.0f, std::clamp(gainDb, -24.0f, 24.0f) / 40.0f);
    const float omega = 2.0f * kPi * frequency / sampleRate;
    const float cosine = std::cos(omega);
    const float sine = std::sin(omega);
    const float rootA = std::sqrt(A);
    const float alpha = sine * std::sqrt(2.0f) * 0.5f;
    const float twoRootAlpha = 2.0f * rootA * alpha;
    return normalizedBiquad(
        A * ((A + 1.0f) + (A - 1.0f) * cosine + twoRootAlpha),
        -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosine),
        A * ((A + 1.0f) + (A - 1.0f) * cosine - twoRootAlpha),
        (A + 1.0f) - (A - 1.0f) * cosine + twoRootAlpha,
        2.0f * ((A - 1.0f) - (A + 1.0f) * cosine),
        (A + 1.0f) - (A - 1.0f) * cosine - twoRootAlpha);
}

void applyBiquad(std::vector<float>& samples, Biquad filter)
{
    for (std::size_t index = 0; index + 1 < samples.size(); index += 2)
    {
        samples[index] = filter.process(samples[index], 0);
        samples[index + 1] = filter.process(samples[index + 1], 1);
    }
}

void applySoftening(std::vector<float>& samples, float amount, float sampleRate)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount <= 0.001f)
        return;
    const float cutoff = 18000.0f * std::pow(0.22f, amount);
    auto softened = samples;
    applyBiquad(softened, lowPass(cutoff, 0.707f, sampleRate));
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = samples[i] * (1.0f - amount * 0.72f)
            + softened[i] * (amount * 0.72f);
}

void applySaturation(std::vector<float>& samples, float amount)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount <= 0.001f)
        return;
    const float drive = 1.0f + amount * 4.0f;
    const float normalizer = 1.0f / std::tanh(drive);
    for (float& sample : samples)
    {
        const float wet = std::tanh(sample * drive) * normalizer;
        sample = sample * (1.0f - amount) + wet * amount;
    }
}

void applySimpleReflections(std::vector<float>& samples, float amount, std::uint32_t sampleRate)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount <= 0.001f || sampleRate == 0)
        return;

    const std::array<float, 4> delaysSeconds{ 0.021f, 0.037f, 0.061f, 0.093f };
    const std::array<float, 4> gains{ 0.34f, 0.25f, 0.18f, 0.12f };
    const auto dry = samples;
    for (std::size_t tap = 0; tap < delaysSeconds.size(); ++tap)
    {
        const std::size_t delayFrames = static_cast<std::size_t>(
            delaysSeconds[tap] * static_cast<float>(sampleRate));
        if (delayFrames == 0)
            continue;
        const std::size_t delaySamples = delayFrames * 2;
        for (std::size_t i = delaySamples; i < samples.size(); ++i)
        {
            // Cross-feed the reflections slightly to make the audition useful
            // in headphones without pretending this is a full room solver.
            const std::size_t source = i - delaySamples;
            const std::size_t sourceChannel = (tap & 1U) ? (source ^ 1U) : source;
            samples[i] += dry[sourceChannel] * gains[tap] * amount;
        }
    }
}

void applyGain(std::vector<float>& samples, float db)
{
    const float gain = dbToGain(std::clamp(db, -36.0f, 18.0f));
    for (float& sample : samples)
        sample *= gain;
}

void clampSafety(std::vector<float>& samples)
{
    for (float& sample : samples)
    {
        if (!std::isfinite(sample))
            sample = 0.0f;
        sample = std::clamp(sample, -1.0f, 1.0f);
    }
}

void applySourceCharacter(
    std::vector<float>& samples,
    float sampleRate,
    const EngineSoundAcousticProfile& profile)
{
    applyGain(samples, profile.inputGainDb);
    applyBiquad(samples, highPass(profile.highPassHz, 0.707f, sampleRate));
    applyBiquad(samples, lowPass(profile.lowPassHz, 0.707f, sampleRate));
    applyBiquad(samples, peaking(
        profile.bodyFrequencyHz, profile.bodyQ, profile.bodyGainDb, sampleRate));

    // Combustion/block character is intentionally separate from the corrective
    // Engine-Sim harshness notch. This lets a small four, V8 or motorcycle keep
    // a distinct pressure-pulse / metallic personality after cleanup.
    applyBiquad(samples, peaking(
        760.0f, 0.78f, std::clamp(profile.combustionPunch, 0.0f, 1.0f) * 4.5f, sampleRate));
    applyBiquad(samples, peaking(
        profile.metallicFrequencyHz, 1.35f,
        std::clamp(profile.metallicCharacter, 0.0f, 1.0f) * 5.0f, sampleRate));

    applyBiquad(samples, peaking(
        profile.presenceFrequencyHz, profile.presenceQ,
        -std::abs(profile.presenceCutDb), sampleRate));
    applyBiquad(samples, highShelf(5200.0f, profile.highShelfDb, sampleRate));
    applySoftening(samples, profile.pulseSoftening, sampleRate);
    applySaturation(samples, profile.saturation);
}

} // namespace

const char* engineSoundPerspectiveName(EngineSoundPerspective perspective)
{
    switch (perspective)
    {
    case EngineSoundPerspective::Raw: return "RAW";
    case EngineSoundPerspective::EngineBay: return "ENGINE BAY";
    case EngineSoundPerspective::RearExhaust: return "REAR / EXHAUST";
    case EngineSoundPerspective::DriverCabin: return "DRIVER CABIN";
    }
    return "RAW";
}

EngineSoundPerspective parseEngineSoundPerspective(const char* name)
{
    if (!name)
        return EngineSoundPerspective::Raw;
    std::string value(name);
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "engine" || value == "enginebay" || value == "engine_bay")
        return EngineSoundPerspective::EngineBay;
    if (value == "rear" || value == "exhaust" || value == "rear_exhaust")
        return EngineSoundPerspective::RearExhaust;
    if (value == "cabin" || value == "driver" || value == "driver_cabin")
        return EngineSoundPerspective::DriverCabin;
    return EngineSoundPerspective::Raw;
}


const char* engineSoundPresetName(EngineSoundPreset preset)
{
    switch (preset)
    {
    case EngineSoundPreset::Neutral: return "NEUTRAL";
    case EngineSoundPreset::EngineSimCleanup: return "ENGINE-SIM CLEANUP";
    case EngineSoundPreset::Peugeot206RCStock: return "PEUGEOT 206 RC STOCK";
    case EngineSoundPreset::WarmRoadCar: return "WARM ROAD CAR";
    case EngineSoundPreset::SportExhaust: return "SPORT EXHAUST";
    }
    return "NEUTRAL";
}

EngineSoundAnalysis analyzeEngineSoundStereo(
    const std::vector<float>& interleavedStereo,
    std::uint32_t sampleRate)
{
    EngineSoundAnalysis result;
    if (interleavedStereo.size() < 256 || (interleavedStereo.size() & 1U) != 0 || sampleRate < 8000)
        return result;

    const std::size_t frameCount = interleavedStereo.size() / 2;
    std::vector<float> mono(frameCount);
    double sum = 0.0;
    double sumSquares = 0.0;
    float peak = 0.0f;
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        const float left = std::isfinite(interleavedStereo[frame * 2]) ? interleavedStereo[frame * 2] : 0.0f;
        const float right = std::isfinite(interleavedStereo[frame * 2 + 1]) ? interleavedStereo[frame * 2 + 1] : 0.0f;
        const float value = 0.5f * (left + right);
        mono[frame] = value;
        peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
        sum += value;
        sumSquares += static_cast<double>(value) * static_cast<double>(value);
    }

    const float rms = std::sqrt(static_cast<float>(sumSquares / static_cast<double>(frameCount)));
    result.peakDb = gainToDb(peak);
    result.rmsDb = gainToDb(rms);
    result.crestDb = std::max(0.0f, result.peakDb - result.rmsDb);
    result.dcOffset = static_cast<float>(sum / static_cast<double>(frameCount));

    // Envelope for a useful at-a-glance waveform without retaining the raw
    // capture in the UI layer.
    for (std::size_t bin = 0; bin < result.waveform.size(); ++bin)
    {
        const std::size_t begin = (frameCount * bin) / result.waveform.size();
        const std::size_t end = (frameCount * (bin + 1)) / result.waveform.size();
        float envelope = 0.0f;
        for (std::size_t i = begin; i < end; ++i)
            envelope = std::max(envelope, std::abs(mono[i]));
        result.waveform[bin] = envelope;
    }

    // RMS stability across equal windows. A held engine that is wandering in
    // RPM/load tends to produce a visibly larger dB spread here.
    constexpr std::size_t kStabilityWindows = 16;
    std::array<float, kStabilityWindows> windowDb{};
    float windowMean = 0.0f;
    for (std::size_t window = 0; window < kStabilityWindows; ++window)
    {
        const std::size_t begin = (frameCount * window) / kStabilityWindows;
        const std::size_t end = (frameCount * (window + 1)) / kStabilityWindows;
        double energy = 0.0;
        const std::size_t count = std::max<std::size_t>(1, end - begin);
        for (std::size_t i = begin; i < end; ++i)
            energy += static_cast<double>(mono[i]) * static_cast<double>(mono[i]);
        windowDb[window] = gainToDb(std::sqrt(static_cast<float>(energy / static_cast<double>(count))));
        windowMean += windowDb[window];
    }
    windowMean /= static_cast<float>(kStabilityWindows);
    float variance = 0.0f;
    for (const float value : windowDb)
    {
        const float delta = value - windowMean;
        variance += delta * delta;
    }
    result.stabilityDb = std::sqrt(variance / static_cast<float>(kStabilityWindows));

    // A small log-frequency Goertzel bank is plenty for an authoring assistant
    // and keeps Heritage Studio independent from an FFT/audio middleware SDK.
    const std::size_t analysisCount = std::min<std::size_t>(8192, frameCount);
    const std::size_t analysisStart = frameCount > analysisCount
        ? (frameCount - analysisCount) / 2
        : 0;
    constexpr float kMinFrequency = 35.0f;
    const float maxFrequency = std::min(16000.0f, static_cast<float>(sampleRate) * 0.45f);
    std::array<float, EngineSoundAnalysis::kSpectrumBins> powers{};
    float maximumPower = 1.0e-20f;
    float totalPower = 0.0f;
    float low = 0.0f;
    float body = 0.0f;
    float mid = 0.0f;
    float presence = 0.0f;
    float air = 0.0f;
    float strongestPresencePower = -1.0f;
    float strongestPresenceHz = 2800.0f;
    for (std::size_t bin = 0; bin < powers.size(); ++bin)
    {
        const float t = powers.size() > 1
            ? static_cast<float>(bin) / static_cast<float>(powers.size() - 1)
            : 0.0f;
        const float frequency = kMinFrequency * std::pow(maxFrequency / kMinFrequency, t);
        const float power = goertzelPower(mono, analysisStart, analysisCount,
            static_cast<float>(sampleRate), frequency);
        powers[bin] = power;
        maximumPower = std::max(maximumPower, power);
        totalPower += power;
        if (frequency < 180.0f) low += power;
        else if (frequency < 650.0f) body += power;
        else if (frequency < 2500.0f) mid += power;
        else if (frequency < 6000.0f)
        {
            presence += power;
            if (power > strongestPresencePower)
            {
                strongestPresencePower = power;
                strongestPresenceHz = frequency;
            }
        }
        else air += power;
    }
    for (std::size_t bin = 0; bin < powers.size(); ++bin)
    {
        const float normalized = std::sqrt(powers[bin] / maximumPower);
        result.spectrum[bin] = clamp01(normalized);
    }

    const float denominator = std::max(totalPower, 1.0e-20f);
    result.lowRatio = low / denominator;
    result.bodyRatio = body / denominator;
    result.midRatio = mid / denominator;
    result.presenceRatio = presence / denominator;
    result.airRatio = air / denominator;
    result.dominantPresenceHz = strongestPresenceHz;
    // Presence/air weight is intentionally nonlinear: Engine Simulator's
    // unpleasant synthetic edge usually appears as excess upper-mid/air energy.
    result.harshness = clamp01(
        result.presenceRatio * 2.0f
        + result.airRatio * 4.5f
        + clamp01((result.crestDb - 9.0f) / 12.0f) * 0.15f);
    result.valid = std::isfinite(result.rmsDb) && rms > 1.0e-6f;
    return result;
}

EngineSoundAcousticProfile makeEngineSoundPreset(EngineSoundPreset preset)
{
    EngineSoundAcousticProfile profile;
    switch (preset)
    {
    case EngineSoundPreset::Neutral:
        profile.inputGainDb = 0.0f;
        profile.highPassHz = 20.0f;
        profile.lowPassHz = 20000.0f;
        profile.bodyGainDb = 0.0f;
        profile.presenceCutDb = 0.0f;
        profile.highShelfDb = 0.0f;
        profile.pulseSoftening = 0.0f;
        profile.saturation = 0.0f;
        profile.combustionPunch = 0.18f;
        profile.metallicCharacter = 0.10f;
        profile.mechanicalPresence = 0.10f;
        profile.intakePresence = 0.10f;
        profile.intakeThroat = 0.12f;
        profile.airboxDamping = 0.20f;
        profile.exhaustMuffling = 0.15f;
        profile.exhaustBodyGainDb = 1.0f;
        profile.exhaustRasp = 0.10f;
        profile.exhaustDrone = 0.08f;
        profile.tailpipeBrightness = 0.20f;
        profile.cabinDamping = 0.45f;
        profile.firewallDamping = 0.45f;
        profile.glassLeak = 0.20f;
        profile.cabinBoom = 0.14f;
        profile.cabinLowFrequencyLeak = 0.65f;
        profile.cabinResonance = 0.15f;
        profile.reverbPreview = 0.0f;
        profile.occlusionPreview = 0.0f;
        profile.outputGainDb = -1.0f;
        break;
    case EngineSoundPreset::EngineSimCleanup:
        profile.presenceCutDb = 6.0f;
        profile.presenceFrequencyHz = 3000.0f;
        profile.highShelfDb = -4.0f;
        profile.pulseSoftening = 0.24f;
        profile.saturation = 0.05f;
        profile.bodyGainDb = 2.5f;
        profile.bodyFrequencyHz = 135.0f;
        profile.lowPassHz = 15000.0f;
        profile.combustionPunch = 0.24f;
        profile.metallicCharacter = 0.11f;
        profile.mechanicalPresence = 0.18f;
        profile.intakePresence = 0.16f;
        profile.intakeThroat = 0.18f;
        profile.airboxDamping = 0.48f;
        profile.exhaustMuffling = 0.46f;
        profile.exhaustRasp = 0.12f;
        profile.exhaustDrone = 0.12f;
        profile.tailpipeBrightness = 0.18f;
        break;
    case EngineSoundPreset::Peugeot206RCStock:
        profile.inputGainDb = -1.0f;
        profile.highPassHz = 30.0f;
        profile.lowPassHz = 15000.0f;
        profile.bodyGainDb = 2.8f;
        profile.bodyFrequencyHz = 128.0f;
        profile.bodyQ = 0.80f;
        profile.presenceCutDb = 5.5f;
        profile.presenceFrequencyHz = 2850.0f;
        profile.presenceQ = 0.90f;
        profile.highShelfDb = -3.8f;
        profile.pulseSoftening = 0.22f;
        profile.saturation = 0.07f;
        profile.combustionPunch = 0.30f;
        profile.metallicCharacter = 0.16f;
        profile.metallicFrequencyHz = 3150.0f;
        profile.mechanicalPresence = 0.23f;
        profile.intakePresence = 0.24f;
        profile.intakeFrequencyHz = 1200.0f;
        profile.intakeThroat = 0.27f;
        profile.airboxDamping = 0.62f;
        profile.exhaustMuffling = 0.52f;
        profile.exhaustBodyGainDb = 3.5f;
        profile.exhaustBodyFrequencyHz = 108.0f;
        profile.exhaustBodyQ = 0.78f;
        profile.exhaustRasp = 0.16f;
        profile.exhaustDrone = 0.18f;
        profile.tailpipeBrightness = 0.18f;
        profile.cabinDamping = 0.60f;
        profile.firewallDamping = 0.58f;
        profile.glassLeak = 0.17f;
        profile.cabinBoom = 0.24f;
        profile.cabinLowFrequencyLeak = 0.70f;
        profile.cabinResonance = 0.23f;
        profile.cabinResonanceHz = 112.0f;
        profile.reverbPreview = 0.06f;
        profile.outputGainDb = -1.5f;
        break;
    case EngineSoundPreset::WarmRoadCar:
        profile = makeEngineSoundPreset(EngineSoundPreset::Peugeot206RCStock);
        profile.bodyGainDb = 4.0f;
        profile.lowPassHz = 12500.0f;
        profile.highShelfDb = -5.5f;
        profile.pulseSoftening = 0.32f;
        profile.combustionPunch = 0.36f;
        profile.metallicCharacter = 0.10f;
        profile.mechanicalPresence = 0.14f;
        profile.intakePresence = 0.17f;
        profile.intakeThroat = 0.24f;
        profile.airboxDamping = 0.72f;
        profile.exhaustMuffling = 0.62f;
        profile.exhaustRasp = 0.10f;
        profile.exhaustDrone = 0.28f;
        profile.tailpipeBrightness = 0.10f;
        profile.cabinDamping = 0.67f;
        profile.firewallDamping = 0.68f;
        profile.cabinBoom = 0.32f;
        break;
    case EngineSoundPreset::SportExhaust:
        profile = makeEngineSoundPreset(EngineSoundPreset::Peugeot206RCStock);
        profile.lowPassHz = 17500.0f;
        profile.highShelfDb = -2.0f;
        profile.pulseSoftening = 0.14f;
        profile.combustionPunch = 0.34f;
        profile.metallicCharacter = 0.24f;
        profile.intakePresence = 0.34f;
        profile.intakeThroat = 0.42f;
        profile.airboxDamping = 0.28f;
        profile.exhaustMuffling = 0.24f;
        profile.exhaustBodyGainDb = 5.0f;
        profile.exhaustBodyFrequencyHz = 115.0f;
        profile.exhaustRasp = 0.36f;
        profile.exhaustDrone = 0.30f;
        profile.tailpipeBrightness = 0.48f;
        profile.cabinDamping = 0.52f;
        profile.firewallDamping = 0.50f;
        profile.glassLeak = 0.22f;
        break;
    }
    return profile;
}

EngineSoundAcousticProfile autoTuneEngineSoundProfile(
    const EngineSoundAnalysis& analysis,
    EngineSoundPreset seedPreset)
{
    EngineSoundAcousticProfile profile = makeEngineSoundPreset(seedPreset);
    if (!analysis.valid)
        return profile;

    // Keep useful headroom for subsequent perspective EQ and reflections.
    profile.inputGainDb = std::clamp(-5.0f - analysis.peakDb, -12.0f, 6.0f);
    profile.presenceFrequencyHz = std::clamp(analysis.dominantPresenceHz, 1200.0f, 6000.0f);
    profile.presenceCutDb = std::clamp(
        profile.presenceCutDb + analysis.harshness * 6.5f,
        0.0f, 14.0f);
    profile.highShelfDb = std::clamp(
        profile.highShelfDb - analysis.harshness * 3.5f,
        -12.0f, 2.0f);
    profile.pulseSoftening = std::clamp(
        profile.pulseSoftening + analysis.harshness * 0.24f,
        0.0f, 0.65f);
    profile.lowPassHz = std::clamp(
        profile.lowPassHz - analysis.harshness * 2500.0f,
        9000.0f, 19000.0f);

    const float lowBody = analysis.lowRatio + analysis.bodyRatio;
    if (lowBody < 0.34f)
        profile.bodyGainDb = std::clamp(profile.bodyGainDb + (0.34f - lowBody) * 7.0f, -2.0f, 6.5f);
    else if (lowBody > 0.62f)
        profile.bodyGainDb = std::clamp(profile.bodyGainDb - (lowBody - 0.62f) * 5.0f, -2.0f, 6.5f);

    // Very peaky captures benefit from a little more soft saturation; dense
    // already-compressed captures should not be flattened further.
    const float transientNeed = clamp01((analysis.crestDb - 10.0f) / 10.0f);
    profile.saturation = std::clamp(profile.saturation + transientNeed * 0.08f, 0.0f, 0.22f);
    return profile;
}

std::vector<float> processEngineSoundStereo(
    const std::vector<float>& interleavedStereo,
    std::uint32_t sampleRate,
    EngineSoundPerspective perspective,
    const EngineSoundAcousticProfile& profile)
{
    std::vector<float> output = interleavedStereo;
    if (output.empty() || (output.size() & 1U) != 0 || sampleRate < 8000)
        return {};

    if (perspective == EngineSoundPerspective::Raw)
    {
        clampSafety(output);
        return output;
    }

    const float rate = static_cast<float>(sampleRate);
    applySourceCharacter(output, rate, profile);
    const std::vector<float> sourceCharacter = output;

    if (perspective == EngineSoundPerspective::EngineBay)
    {
        const float presence = std::clamp(profile.mechanicalPresence, 0.0f, 1.0f);
        applyBiquad(output, peaking(1850.0f, 1.0f, presence * 4.0f, rate));
        applyBiquad(output, highShelf(6200.0f, presence * 2.0f, rate));
        const float intake = std::clamp(profile.intakePresence, 0.0f, 1.0f);
        const float throat = std::clamp(profile.intakeThroat, 0.0f, 1.0f);
        const float airbox = std::clamp(profile.airboxDamping, 0.0f, 1.0f);
        applyBiquad(output, peaking(
            profile.intakeFrequencyHz, 0.82f, intake * 6.0f, rate));
        applyBiquad(output, peaking(
            std::max(180.0f, profile.intakeFrequencyHz * 0.58f),
            0.65f, throat * 5.0f, rate));
        const float intakeCutoff = 19000.0f * std::pow(0.30f, airbox);
        applyBiquad(output, lowPass(intakeCutoff, 0.72f, rate));
        applyBiquad(output, highShelf(4300.0f, -airbox * 4.5f, rate));
    }
    else if (perspective == EngineSoundPerspective::RearExhaust)
    {
        const float muffling = std::clamp(profile.exhaustMuffling, 0.0f, 1.0f);
        const float exhaustCutoff = 17500.0f * std::pow(0.27f, muffling);
        applyBiquad(output, lowPass(exhaustCutoff, 0.72f, rate));
        applyBiquad(output, peaking(
            profile.exhaustBodyFrequencyHz,
            profile.exhaustBodyQ,
            profile.exhaustBodyGainDb,
            rate));
        applyBiquad(output, lowShelf(180.0f, 1.5f + muffling * 2.0f, rate));
        const float rasp = std::clamp(profile.exhaustRasp, 0.0f, 1.0f);
        const float drone = std::clamp(profile.exhaustDrone, 0.0f, 1.0f);
        const float bright = std::clamp(profile.tailpipeBrightness, 0.0f, 1.0f);
        applyBiquad(output, peaking(2350.0f, 1.15f, rasp * 6.0f, rate));
        applyBiquad(output, peaking(
            std::clamp(profile.exhaustBodyFrequencyHz * 1.18f, 45.0f, 240.0f),
            2.0f, drone * 5.0f, rate));
        applyBiquad(output, highShelf(4800.0f, bright * 5.5f - muffling * 1.5f, rate));
    }
    else if (perspective == EngineSoundPerspective::DriverCabin)
    {
        const float damping = std::clamp(profile.cabinDamping, 0.0f, 1.0f);
        const float firewall = std::clamp(profile.firewallDamping, 0.0f, 1.0f);
        const float glass = std::clamp(profile.glassLeak, 0.0f, 1.0f);
        const float leak = std::clamp(profile.cabinLowFrequencyLeak, 0.0f, 1.0f);
        const float boom = std::clamp(profile.cabinBoom, 0.0f, 1.0f);
        const float windowOpen = std::clamp(profile.windowOpenPreview, 0.0f, 1.0f);
        const float effectiveDamping = std::clamp(damping + firewall * 0.35f - glass * 0.22f, 0.0f, 1.0f);
        const float cabinCutoff = 15500.0f * std::pow(0.12f, effectiveDamping);
        applyBiquad(output, lowPass(cabinCutoff, 0.70f, rate));
        applyBiquad(output, highShelf(2700.0f, -2.0f - damping * 7.0f - firewall * 6.0f + glass * 3.0f, rate));
        applyBiquad(output, lowShelf(180.0f, leak * 5.0f - 1.5f, rate));
        applyBiquad(output, peaking(
            profile.cabinResonanceHz,
            0.9f,
            std::clamp(profile.cabinResonance, 0.0f, 1.0f) * 5.0f + boom * 3.5f,
            rate));
        applyBiquad(output, peaking(88.0f, 1.25f, boom * 3.0f, rate));
        applyGain(output, -1.0f - damping * 5.0f - firewall * 2.5f + glass * 1.0f);

        // Window-open preview is a true acoustic leak toward the already-shaped
        // exterior source, not merely a volume slider. This is authoring-only;
        // the game will drive the equivalent blend continuously at runtime.
        if (windowOpen > 0.001f)
        {
            const float exteriorMix = windowOpen * 0.72f;
            for (std::size_t i = 0; i < output.size(); ++i)
                output[i] = output[i] * (1.0f - exteriorMix) + sourceCharacter[i] * exteriorMix;
        }
    }

    const float occlusion = std::clamp(profile.occlusionPreview, 0.0f, 1.0f);
    if (occlusion > 0.001f)
    {
        const float cutoff = 16000.0f * std::pow(0.075f, occlusion);
        applyBiquad(output, lowPass(cutoff, 0.70f, rate));
        applyBiquad(output, lowShelf(180.0f, occlusion * 3.0f, rate));
        applyGain(output, -occlusion * 10.0f);
    }
    applySimpleReflections(output, profile.reverbPreview, sampleRate);
    applyGain(output, profile.outputGainDb);
    clampSafety(output);
    return output;
}

} // namespace heritage::audio::lab
