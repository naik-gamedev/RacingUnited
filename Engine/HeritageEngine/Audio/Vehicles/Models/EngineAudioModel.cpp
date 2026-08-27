#include "EngineAudioModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::audio::vehicles {
namespace {

float smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0)
        return value >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

EngineAudioMix evaluateEngineAudio(
    const VehicleAudioDefinition& definition,
    float rpm,
    float normalizedLoad,
    bool interior,
    VehicleAudioDetailLevel detail)
{
    EngineAudioMix mix;
    const float safeReference = (std::max)(definition.referenceRpm, 300.0f);
    const float safeIdle = (std::max)(definition.idleRpm, 300.0f);
    const float safeRedline = (std::max)(definition.redlineRpm, safeIdle + 100.0f);
    const float clampedRpm = std::clamp(rpm, safeIdle * 0.72f, safeRedline * 1.05f);
    const float speed = std::clamp(
        (clampedRpm - safeIdle) / (safeRedline - safeIdle), 0.0f, 1.0f);
    const float load = std::clamp(normalizedLoad, 0.0f, 1.0f);
    const auto& acoustics = definition.engineAcoustics;
    const float intakeTransition = acoustics.variableIntakeTransitionRpm > 0.0f
        ? smoothStep(
            acoustics.variableIntakeTransitionRpm
                - 0.5f * acoustics.variableIntakeTransitionWidthRpm,
            acoustics.variableIntakeTransitionRpm
                + 0.5f * acoustics.variableIntakeTransitionWidthRpm,
            clampedRpm)
        : 0.0f;
    const float variableIntake = 1.0f
        + std::clamp(acoustics.variableIntakeGain, 0.0f, 1.0f)
            * intakeTransition * (0.35f + 0.65f * load);

    mix.pitch = clampedRpm / safeReference;
    mix.exhaustGain = definition.gains.exhaust
        * (0.18f + 0.62f * load + 0.20f * speed);
    mix.intakeGain = definition.gains.intake
        * (0.08f + 0.78f * std::sqrt(load) + 0.14f * speed)
        * variableIntake;
    mix.mechanicalGain = definition.gains.mechanical
        * (0.30f + 0.70f * speed);

    if (interior)
    {
        mix.exhaustGain *= 0.56f;
        mix.intakeGain *= 0.78f;
        mix.mechanicalGain *= 1.12f;
        mix.exhaustOpenness = 0.18f;
        mix.intakeOpenness = std::clamp(
            0.40f + 0.12f * intakeTransition, 0.0f, 1.0f);
        mix.mechanicalOpenness = 0.62f;
    }

    if (detail == VehicleAudioDetailLevel::Reduced)
    {
        mix.intakeGain *= 0.45f;
        mix.mechanicalGain = 0.0f;
    }
    else if (detail == VehicleAudioDetailLevel::Crowd)
    {
        mix.exhaustGain *= 0.65f;
        mix.intakeGain = 0.0f;
        mix.mechanicalGain = 0.0f;
    }
    else if (detail == VehicleAudioDetailLevel::Silent)
    {
        mix.exhaustGain = mix.intakeGain = mix.mechanicalGain = 0.0f;
    }
    return mix;
}

} // namespace heritage::audio::vehicles
