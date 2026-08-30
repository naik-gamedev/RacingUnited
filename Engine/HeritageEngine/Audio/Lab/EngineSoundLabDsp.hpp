#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace heritage::audio::lab {

enum class EngineSoundPerspective
{
    Raw = 0,
    EngineBay,
    RearExhaust,
    DriverCabin
};

enum class EngineSoundPreset
{
    Neutral = 0,
    EngineSimCleanup,
    Peugeot206RCStock,
    WarmRoadCar,
    SportExhaust
};

struct EngineSoundAcousticProfile
{
    float inputGainDb = 0.0f;
    float highPassHz = 32.0f;
    float lowPassHz = 16500.0f;

    float bodyGainDb = 2.0f;
    float bodyFrequencyHz = 125.0f;
    float bodyQ = 0.85f;

    float presenceCutDb = 3.0f;
    float presenceFrequencyHz = 2800.0f;
    float presenceQ = 0.85f;
    float highShelfDb = -2.0f;

    float pulseSoftening = 0.12f;
    float saturation = 0.06f;

    // Vehicle-character layer. These are deliberately intelligible physical /
    // acoustic controls rather than only mastering-style EQ knobs. They are
    // persisted in .hacoustic so each vehicle can have a distinct sound DNA.
    float combustionPunch = 0.30f;
    float metallicCharacter = 0.16f;
    float metallicFrequencyHz = 3100.0f;

    float mechanicalPresence = 0.22f;
    float intakePresence = 0.18f;
    float intakeFrequencyHz = 1050.0f;
    float intakeThroat = 0.24f;
    float airboxDamping = 0.52f;

    float exhaustMuffling = 0.42f;
    float exhaustBodyGainDb = 3.0f;
    float exhaustBodyFrequencyHz = 105.0f;
    float exhaustBodyQ = 0.8f;
    float exhaustRasp = 0.16f;
    float exhaustDrone = 0.14f;
    float tailpipeBrightness = 0.20f;

    float cabinDamping = 0.58f;
    float firewallDamping = 0.58f;
    float glassLeak = 0.16f;
    float cabinBoom = 0.20f;
    float windowOpenPreview = 0.0f;
    float cabinLowFrequencyLeak = 0.68f;
    float cabinResonance = 0.22f;
    float cabinResonanceHz = 115.0f;

    float reverbPreview = 0.08f;
    float occlusionPreview = 0.0f;
    float outputGainDb = -1.0f;
};

struct EngineSoundAnalysis
{
    static constexpr std::size_t kWaveformBins = 96;
    static constexpr std::size_t kSpectrumBins = 32;

    bool valid = false;
    float peakDb = -120.0f;
    float rmsDb = -120.0f;
    float crestDb = 0.0f;
    float dcOffset = 0.0f;
    float stabilityDb = 0.0f;

    float lowRatio = 0.0f;       // roughly 35-180 Hz
    float bodyRatio = 0.0f;      // roughly 180-650 Hz
    float midRatio = 0.0f;       // roughly 650-2500 Hz
    float presenceRatio = 0.0f;  // roughly 2.5-6 kHz
    float airRatio = 0.0f;       // roughly 6-16 kHz
    float harshness = 0.0f;      // normalized heuristic, 0..1
    float dominantPresenceHz = 2800.0f;

    std::array<float, kWaveformBins> waveform{};
    std::array<float, kSpectrumBins> spectrum{};
};

const char* engineSoundPerspectiveName(EngineSoundPerspective perspective);
EngineSoundPerspective parseEngineSoundPerspective(const char* name);
const char* engineSoundPresetName(EngineSoundPreset preset);

// Generic broad-band analysis used by Heritage Studio's Audio Assistant. This
// deliberately stays lightweight and dependency-free: it is an authoring aid,
// not a mastering-grade spectrum analyzer.
EngineSoundAnalysis analyzeEngineSoundStereo(
    const std::vector<float>& interleavedStereo,
    std::uint32_t sampleRate);

// Deterministic starting points for users who do not want to hand-tune every
// DSP parameter. Presets remain non-destructive .hacoustic state.
EngineSoundAcousticProfile makeEngineSoundPreset(EngineSoundPreset preset);

// Adapts a seed profile to the captured source: input headroom, presence cut,
// top-end damping and pulse softening are derived from measured raw content.
EngineSoundAcousticProfile autoTuneEngineSoundProfile(
    const EngineSoundAnalysis& analysis,
    EngineSoundPreset seedPreset);

// Interleaved stereo IEEE-float processing. The raw perspective is a true
// bypass except for a final hard safety clamp; all other perspectives first
// pass through the source-character chain and then their listening transform.
std::vector<float> processEngineSoundStereo(
    const std::vector<float>& interleavedStereo,
    std::uint32_t sampleRate,
    EngineSoundPerspective perspective,
    const EngineSoundAcousticProfile& profile);

} // namespace heritage::audio::lab
