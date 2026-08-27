#include "PhysicsRegressionCommon.hpp"

#include "../Audio/Vehicles/VehicleAudioSynthesis.hpp"
#include "../Audio/Vehicles/Models/AerodynamicAudioModel.hpp"
#include "../Audio/Vehicles/Models/EngineAudioModel.hpp"
#include "../Audio/Vehicles/Models/EngineSampleBankModel.hpp"
#include "../Audio/Vehicles/Models/TireAudioModel.hpp"
#include "../Audio/Vehicles/Models/VehicleAudioEventModel.hpp"
#include "../Audio/Vehicles/Models/VehicleAudioFleetBudgetModel.hpp"
#include "../Audio/Acoustics/AcousticPathTracer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace heritage::tests {

bool vehicleAudioSynthesisAndMixAreBounded()
{
    using namespace heritage::audio::vehicles;
    VehicleAudioDefinition definition;
    definition.id = "regression_inline_four";
    definition.cylinderCount = 4;
    definition.cycleRevolutions = 2;
    definition.referenceRpm = 1200.0f;

    constexpr std::array layers{
        SynthesizedVehicleLayer::Exhaust,
        SynthesizedVehicleLayer::Intake,
        SynthesizedVehicleLayer::Mechanical,
        SynthesizedVehicleLayer::Transmission,
        SynthesizedVehicleLayer::Tire,
        SynthesizedVehicleLayer::Wind,
        SynthesizedVehicleLayer::Chassis
    };
    for (const SynthesizedVehicleLayer layer : layers)
    {
        const auto signal = synthesizeVehicleLayer(definition, layer);
        if (signal.sampleRate != 48000
            || signal.samples.size() != 48000
            || !std::isfinite(signalPeak(signal))
            || signalPeak(signal) > 1.001f
            || signalRms(signal) < 0.015f)
            return false;
    }
    constexpr std::array transients{
        SynthesizedVehicleTransient::RevLimiterCut,
        SynthesizedVehicleTransient::OverrunPop
    };
    for (const SynthesizedVehicleTransient transient : transients)
    {
        const auto signal = synthesizeVehicleTransient(definition, transient);
        if (signal.sampleRate != 48000
            || signal.samples.size() < 4000
            || !std::isfinite(signalPeak(signal))
            || signalPeak(signal) > 1.001f
            || signalRms(signal) < 0.015f)
            return false;
    }

    const auto inlineFour = synthesizeVehicleLayer(
        definition, SynthesizedVehicleLayer::Exhaust);
    VehicleAudioDefinition alternateEngine = definition;
    alternateEngine.cylinderCount = 2;
    alternateEngine.cycleRevolutions = 2;
    alternateEngine.engineAcoustics.firingOrder = { 1, 2 };
    alternateEngine.engineAcoustics.displacementLiters = 1.0f;
    alternateEngine.engineAcoustics.compressionRatio = 9.0f;
    alternateEngine.engineAcoustics.exhaustPulseSharpness = 0.30f;
    const auto parallelTwin = synthesizeVehicleLayer(
        alternateEngine, SynthesizedVehicleLayer::Exhaust);
    double engineDifference = 0.0;
    for (std::size_t index = 0; index < inlineFour.samples.size(); ++index)
        engineDifference += std::abs(
            static_cast<double>(inlineFour.samples[index])
            - static_cast<double>(parallelTwin.samples[index]));
    engineDifference /= static_cast<double>(inlineFour.samples.size());

    const auto idle = evaluateEngineAudio(
        definition, 900.0f, 0.05f, false, VehicleAudioDetailLevel::Full);
    const auto loaded = evaluateEngineAudio(
        definition, 6000.0f, 0.95f, false, VehicleAudioDetailLevel::Full);
    const auto interior = evaluateEngineAudio(
        definition, 6000.0f, 0.95f, true, VehicleAudioDetailLevel::Full);
    const auto reduced = evaluateEngineAudio(
        definition, 6000.0f, 0.95f, false, VehicleAudioDetailLevel::Reduced);
    VehicleAudioDefinition variableIntakeDefinition = definition;
    variableIntakeDefinition.engineAcoustics.variableIntakeTransitionRpm = 5000.0f;
    variableIntakeDefinition.engineAcoustics.variableIntakeTransitionWidthRpm = 800.0f;
    variableIntakeDefinition.engineAcoustics.variableIntakeGain = 0.35f;
    const auto intakeBelowTransition = evaluateEngineAudio(
        variableIntakeDefinition, 4200.0f, 0.90f, false,
        VehicleAudioDetailLevel::Full);
    const auto intakeAboveTransition = evaluateEngineAudio(
        variableIntakeDefinition, 6500.0f, 0.90f, false,
        VehicleAudioDetailLevel::Full);
    const auto rolling = evaluateTireAudio(
        definition, 30.0f, 0.01f, 0.0f, false, VehicleAudioDetailLevel::Full);
    const auto sliding = evaluateTireAudio(
        definition, 30.0f, 0.30f, 0.8f, false, VehicleAudioDetailLevel::Full);
    const auto calm = evaluateAerodynamicAudio(
        definition, 5.0f, false, VehicleAudioDetailLevel::Full);
    const auto fast = evaluateAerodynamicAudio(
        definition, 55.0f, false, VehicleAudioDetailLevel::Full);

    definition.samples.gain = 0.40f;
    definition.samples.loops = {
        { {}, 1000.0f, 1.0f },
        { {}, 3000.0f, 1.0f },
        { {}, 6000.0f, 1.0f }
    };
    const auto fullSamples = evaluateEngineSampleBank(
        definition, 2000.0f, 0.75f, VehicleAudioDetailLevel::Full);
    const auto reducedSamples = evaluateEngineSampleBank(
        definition, 2000.0f, 0.75f, VehicleAudioDetailLevel::Reduced);
    const auto crowdSamples = evaluateEngineSampleBank(
        definition, 2000.0f, 0.75f, VehicleAudioDetailLevel::Crowd);
    const auto activeSamples = [](const std::vector<EngineSampleVoiceMix>& values)
    {
        return std::count_if(values.begin(), values.end(),
            [](const EngineSampleVoiceMix& value) { return value.gain > 0.001f; });
    };
    float fullSamplePower = 0.0f;
    for (const EngineSampleVoiceMix& voice : fullSamples)
        fullSamplePower += voice.gain * voice.gain;

    VehicleAudioEventState eventState;
    const auto initialEvents = evaluateVehicleAudioEvents(
        { 1, 0.0f, 1.0f / 120.0f }, eventState);
    const auto shiftEvents = evaluateVehicleAudioEvents(
        { 2, 0.0f, 1.0f / 120.0f }, eventState);
    const auto lightImpact = evaluateVehicleAudioEvents(
        { 2, 0.55f, 1.0f / 120.0f }, eventState);
    const auto heldImpact = evaluateVehicleAudioEvents(
        { 2, 0.70f, 1.0f / 120.0f }, eventState);
    evaluateVehicleAudioEvents({ 2, 0.0f, 0.25f }, eventState);
    const auto heavyImpact = evaluateVehicleAudioEvents(
        { 2, 1.25f, 1.0f / 120.0f }, eventState);
    VehicleAudioEventState limiterState;
    evaluateVehicleAudioEvents(
        { 3, 0.0f, 1.0f / 120.0f, 6500.0f, 100.0f, 7000.0f },
        limiterState);
    const auto limiterEvent = evaluateVehicleAudioEvents(
        { 3, 0.0f, 1.0f / 120.0f, 7000.0f, 0.0f, 7000.0f },
        limiterState);
    const auto heldLimiter = evaluateVehicleAudioEvents(
        { 3, 0.0f, 1.0f / 120.0f, 7000.0f, 0.0f, 7000.0f },
        limiterState);
    VehicleAudioEventState overrunState;
    evaluateVehicleAudioEvents(
        { 3, 0.0f, 1.0f / 120.0f, 5000.0f, 120.0f, 7000.0f },
        overrunState);
    const auto overrunEvent = evaluateVehicleAudioEvents(
        { 3, 0.0f, 1.0f / 120.0f, 4800.0f, -25.0f, 7000.0f },
        overrunState);

    std::vector<VehicleAudioFleetCandidate> fleet;
    for (VehicleSoundHandle handle = 1; handle <= 150; ++handle)
    {
        const float distanceMeters = static_cast<float>(handle - 1) * 2.0f;
        fleet.push_back({
            handle,
            distanceMeters,
            distanceBasedVehicleAudioDetail(definition, distanceMeters) });
    }
    const VehicleAudioFleetBudget fleetBudget;
    const auto assignments = allocateVehicleAudioFleet(fleet, fleetBudget);
    int fleetVoiceCount = 0;
    int fullVehicleCount = 0;
    int reducedVehicleCount = 0;
    int crowdVehicleCount = 0;
    for (const VehicleAudioFleetAssignment& assignment : assignments)
    {
        fleetVoiceCount += assignment.estimatedContinuousVoices;
        fullVehicleCount += assignment.detail == VehicleAudioDetailLevel::Full ? 1 : 0;
        reducedVehicleCount += assignment.detail == VehicleAudioDetailLevel::Reduced ? 1 : 0;
        crowdVehicleCount += assignment.detail == VehicleAudioDetailLevel::Crowd ? 1 : 0;
    }

    heritage::physics::RigidBodySystem acousticBodies;
    heritage::physics::CollisionSystem acousticCollisions;
    heritage::physics::StaticSceneTriangle floor;
    floor.a = { -50.0f, 0.0f, -50.0f };
    floor.b = { 0.0f, 0.0f, 50.0f };
    floor.c = { 50.0f, 0.0f, -50.0f };
    floor.normal = { 0.0f, 1.0f, 0.0f };
    floor.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
    acousticCollisions.setStaticSceneTriangles({ floor });
    const auto openAcoustics =
        heritage::audio::acoustics::AcousticPathTracer::trace(
            { { -5.0f, 1.0f, 0.0f }, { 5.0f, 1.0f, 0.0f } },
            acousticCollisions,
            acousticBodies);

    heritage::physics::StaticSceneTriangle wallA;
    wallA.a = { 0.0f, -5.0f, -5.0f };
    wallA.b = { 0.0f, 5.0f, -5.0f };
    wallA.c = { 0.0f, 5.0f, 5.0f };
    wallA.normal = { 1.0f, 0.0f, 0.0f };
    wallA.surfaceMaterial = heritage::physics::SurfaceMaterial::Kerb;
    heritage::physics::StaticSceneTriangle wallB = wallA;
    wallB.b = { 0.0f, 5.0f, 5.0f };
    wallB.c = { 0.0f, -5.0f, 5.0f };
    acousticCollisions.setStaticSceneTriangles({ floor, wallA, wallB });
    const auto blockedAcoustics =
        heritage::audio::acoustics::AcousticPathTracer::trace(
            { { -5.0f, 1.0f, 0.0f }, { 5.0f, 1.0f, 0.0f } },
            acousticCollisions,
            acousticBodies);

    return loaded.pitch > idle.pitch
        && engineDifference > 0.05
        && loaded.exhaustGain > idle.exhaustGain
        && intakeAboveTransition.intakeGain > intakeBelowTransition.intakeGain
        && interior.exhaustOpenness < loaded.exhaustOpenness
        && reduced.mechanicalGain == 0.0f
        && sliding.gain > rolling.gain
        && fast.gain > calm.gain
        && activeSamples(fullSamples) == 2
        // Equal-power overlap preserves signal power while two neighboring
        // RPM layers cross, avoiding the center dip of a linear fade.
        && std::abs(fullSamplePower - 0.09f) < 0.0001f
        && activeSamples(reducedSamples) == 1
        && activeSamples(crowdSamples) == 0
        && !initialEvents.gearShift
        && shiftEvents.gearShift
        && lightImpact.suspensionLight
        && !lightImpact.suspensionHeavy
        && !heldImpact.suspensionLight
        && !heldImpact.suspensionHeavy
        && heavyImpact.suspensionHeavy
        && limiterEvent.revLimiterCut
        && !heldLimiter.revLimiterCut
        && overrunEvent.overrunPop
        && assignments.size() == 150
        && assignments.front().handle == 1
        && assignments.front().detail == VehicleAudioDetailLevel::Full
        && assignments.back().detail == VehicleAudioDetailLevel::Silent
        && fleetVoiceCount <= fleetBudget.maximumContinuousVoices
        && fullVehicleCount <= fleetBudget.maximumFullVehicles
        && reducedVehicleCount <= fleetBudget.maximumReducedVehicles
        && crowdVehicleCount <= fleetBudget.maximumCrowdVehicles
        && !openAcoustics.directOccluded
        && openAcoustics.directGain == 1.0f
        && openAcoustics.validReflectionPathCount > 0
        && openAcoustics.earlyReflectionDelaySeconds > 0.0f
        && blockedAcoustics.directOccluded
        && blockedAcoustics.directGain < 0.5f
        && blockedAcoustics.directOpenness < 0.5f;
}

} // namespace heritage::tests
