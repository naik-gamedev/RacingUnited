#include "VehicleAudioRuntime.hpp"

#include "VehicleAudioSynthesis.hpp"
#include "../Acoustics/AcousticPathTracer.hpp"
#include "Models/AerodynamicAudioModel.hpp"
#include "Models/ChassisAudioModel.hpp"
#include "Models/DrivetrainAudioModel.hpp"
#include "Models/EngineAudioModel.hpp"
#include "Models/EngineSampleBankModel.hpp"
#include "Models/VehicleAudioFleetBudgetModel.hpp"
#include "Models/TireAudioModel.hpp"
#include "Models/VehicleAudioEventModel.hpp"
#include "../../Physics/PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace heritage::audio::vehicles {
namespace {

float distance(const AudioVector3& left, const AudioVector3& right)
{
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x * x + y * y + z * z);
}

AudioVector3 toAudio(const heritage::math::Vec3& value)
{
    return { value.x, value.y, value.z };
}

AudioVector3 transformPoint(
    const heritage::math::Vec3& origin,
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward,
    const AudioVector3& local)
{
    return {
        origin.x + right.x * local.x + up.x * local.y + forward.x * local.z,
        origin.y + right.y * local.x + up.y * local.y + forward.y * local.z,
        origin.z + right.z * local.x + up.z * local.y + forward.z * local.z
    };
}

float smoothingFactor(float responsePerSecond, float deltaSeconds)
{
    return 1.0f - std::exp(-std::max(responsePerSecond, 0.0f)
        * std::max(deltaSeconds, 0.0f));
}

AcousticPropagationState smoothedAcoustics(
    const AcousticPropagationState& current,
    const AcousticPropagationState& target,
    float alpha)
{
    const auto blend = [alpha](float from, float to)
    {
        return from + (to - from) * alpha;
    };
    return {
        blend(current.directGain, target.directGain),
        blend(current.directOpenness, target.directOpenness),
        blend(current.earlyReflectionGain, target.earlyReflectionGain),
        blend(current.earlyReflectionDelaySeconds,
            target.earlyReflectionDelaySeconds),
        blend(current.lateReverbGain, target.lateReverbGain)
    };
}

} // namespace

struct VehicleAudioRuntime::Instance
{
    struct SampleVoice
    {
        VehicleEngineSample sample;
        AudioHandle handle = kInvalidAudioHandle;
        bool available = true;
    };

    heritage::vehicles::VehicleHandle vehicle = heritage::vehicles::InvalidVehicle;
    VehicleAudioDefinition definition;
    bool enabled = true;
    AudioHandle exhaust = kInvalidAudioHandle;
    AudioHandle intake = kInvalidAudioHandle;
    AudioHandle mechanical = kInvalidAudioHandle;
    AudioHandle transmission = kInvalidAudioHandle;
    AudioHandle tires = kInvalidAudioHandle;
    AudioHandle wind = kInvalidAudioHandle;
    AudioHandle chassis = kInvalidAudioHandle;
    AudioHandle startup = kInvalidAudioHandle;
    std::vector<SampleVoice> engineSamples;
    std::vector<AudioHandle> transients;
    VehicleAudioEventState eventState;
    std::size_t eventSequence = 0;
    VehicleAudioDetailLevel assignedDetail = VehicleAudioDetailLevel::Silent;
    bool acousticPathTracing = false;
    float acousticTraceTimerSeconds = 0.0f;
    AcousticPropagationState acousticCurrent;
    AcousticPropagationState acousticTarget;
    acoustics::AcousticPathTraceResult lastAcousticTrace;
    VehicleAudioTelemetry telemetry;
};

const char* vehicleAudioDetailName(VehicleAudioDetailLevel value)
{
    switch (value)
    {
    case VehicleAudioDetailLevel::Full: return "Full";
    case VehicleAudioDetailLevel::Reduced: return "Reduced";
    case VehicleAudioDetailLevel::Crowd: return "Crowd";
    case VehicleAudioDetailLevel::Silent: return "Silent";
    }
    return "Silent";
}

VehicleAudioRuntime::VehicleAudioRuntime(
    AudioSystem& audio,
    heritage::physics::PhysicsWorld& physics)
    : m_audio(&audio), m_physics(&physics)
{
}

VehicleAudioRuntime::~VehicleAudioRuntime()
{
    clear();
}

VehicleSoundHandle VehicleAudioRuntime::create(
    heritage::vehicles::VehicleHandle vehicle,
    const VehicleAudioDefinition& definition)
{
    if (!m_audio || !m_audio->isAvailable() || !m_physics
        || !m_physics->vehicles().exists(vehicle))
        return kInvalidVehicleSoundHandle;

    Instance instance;
    instance.vehicle = vehicle;
    instance.definition = definition;
    instance.definition.cylinderCount = std::clamp(definition.cylinderCount, 1, 24);
    instance.definition.cycleRevolutions = std::clamp(definition.cycleRevolutions, 1, 2);
    instance.definition.referenceRpm = std::clamp(definition.referenceRpm, 600.0f, 2400.0f);
    auto& acoustics = instance.definition.engineAcoustics;
    acoustics.displacementLiters = std::clamp(acoustics.displacementLiters, 0.05f, 30.0f);
    acoustics.compressionRatio = std::clamp(acoustics.compressionRatio, 5.0f, 30.0f);
    acoustics.exhaustPulseSharpness = std::clamp(
        acoustics.exhaustPulseSharpness, 0.05f, 0.98f);
    acoustics.intakePulseSharpness = std::clamp(
        acoustics.intakePulseSharpness, 0.05f, 0.98f);
    acoustics.exhaustHeaderImbalance = std::clamp(
        acoustics.exhaustHeaderImbalance, 0.0f, 0.35f);
    acoustics.intakeResonanceOrder = std::clamp(
        acoustics.intakeResonanceOrder, 0.5f, 12.0f);
    acoustics.mechanicalOrderGain = std::clamp(
        acoustics.mechanicalOrderGain, 0.0f, 1.0f);
    acoustics.combustionVariation = std::clamp(
        acoustics.combustionVariation, 0.0f, 0.20f);
    acoustics.variableIntakeTransitionRpm = std::clamp(
        acoustics.variableIntakeTransitionRpm, 0.0f, instance.definition.redlineRpm);
    acoustics.variableIntakeTransitionWidthRpm = std::clamp(
        acoustics.variableIntakeTransitionWidthRpm, 50.0f, 3000.0f);
    acoustics.variableIntakeGain = std::clamp(
        acoustics.variableIntakeGain, 0.0f, 1.0f);
    instance.definition.samples.gain = std::clamp(
        instance.definition.samples.gain, 0.0f, 1.0f);
    instance.definition.samples.proceduralGain = std::clamp(
        instance.definition.samples.proceduralGain, 0.0f, 1.0f);

    const auto validFiringOrder = [&]()
    {
        if (acoustics.firingOrder.size()
            != static_cast<std::size_t>(instance.definition.cylinderCount))
            return false;
        std::vector<bool> seen(
            static_cast<std::size_t>(instance.definition.cylinderCount), false);
        for (const int cylinder : acoustics.firingOrder)
        {
            if (cylinder < 1 || cylinder > instance.definition.cylinderCount
                || seen[static_cast<std::size_t>(cylinder - 1)])
                return false;
            seen[static_cast<std::size_t>(cylinder - 1)] = true;
        }
        return true;
    };
    if (!validFiringOrder())
    {
        acoustics.firingOrder.clear();
        for (int cylinder = 1; cylinder <= instance.definition.cylinderCount; ++cylinder)
            acoustics.firingOrder.push_back(cylinder);
    }
    std::sort(
        instance.definition.samples.loops.begin(),
        instance.definition.samples.loops.end(),
        [](const VehicleEngineSample& left, const VehicleEngineSample& right)
        {
            return left.referenceRpm < right.referenceRpm;
        });

    for (const VehicleEngineSample& sample : instance.definition.samples.loops)
        instance.engineSamples.push_back({ sample, kInvalidAudioHandle, !sample.path.empty() });
    if (!instance.definition.samples.startupPath.empty())
    {
        instance.startup = m_audio->playOneShot(
            instance.definition.samples.startupPath,
            AudioBus::Effects,
            std::clamp(instance.definition.samples.gain, 0.0f, 1.0f),
            1.0f);
    }

    const VehicleSoundHandle handle = m_nextHandle++;
    instance.acousticTraceTimerSeconds = 0.0025f
        * static_cast<float>(handle % 20);
    m_instances.emplace(handle, std::move(instance));
    return handle;
}

bool VehicleAudioRuntime::destroy(VehicleSoundHandle handle)
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    stopLayers(found->second);
    m_instances.erase(found);
    return true;
}

void VehicleAudioRuntime::clear()
{
    for (auto& [handle, instance] : m_instances)
    {
        (void)handle;
        stopLayers(instance);
    }
    m_instances.clear();
    m_nextHandle = 1;
}

void VehicleAudioRuntime::update(float deltaSeconds)
{
    if (!m_audio || !m_physics)
        return;

    int activeTransients = 0;
    std::vector<VehicleAudioFleetCandidate> candidates;
    candidates.reserve(m_instances.size());
    auto& vehicles = m_physics->vehicles();
    auto& bodies = m_physics->rigidBodies();
    const float alpha = m_physics->interpolationAlpha();
    for (auto& [handle, instance] : m_instances)
    {
        std::erase_if(instance.transients, [&](AudioHandle voice)
        {
            return !m_audio->isPlaying(voice);
        });
        activeTransients += static_cast<int>(instance.transients.size());

        float listenerDistance = (std::numeric_limits<float>::max)();
        if (instance.enabled && vehicles.exists(instance.vehicle))
        {
            heritage::physics::RigidBodyPose pose;
            if (bodies.interpolatedPose(
                    vehicles.chassisBody(instance.vehicle), alpha, pose))
            {
                listenerDistance = distance(
                    toAudio(pose.position), m_audio->listener().position);
            }
        }
        candidates.push_back({
            handle,
            listenerDistance,
            distanceBasedVehicleAudioDetail(instance.definition, listenerDistance) });
    }

    const VehicleAudioFleetBudget budget;
    m_transientVoiceSlots = std::max(
        budget.maximumTransientVoices - activeTransients, 0);
    const auto assignments = allocateVehicleAudioFleet(std::move(candidates), budget);
    // Geometry paths are reserved for the 20 closest audible vehicles. This
    // includes the player car. Remaining vehicles still use the inexpensive
    // distance, panning and Doppler route selected by the fleet voice budget.
    int acousticPathSlots = 20;
    for (const VehicleAudioFleetAssignment& assignment : assignments)
    {
        const auto found = m_instances.find(assignment.handle);
        if (found == m_instances.end())
            continue;
        found->second.assignedDetail = assignment.detail;
        found->second.acousticPathTracing = acousticPathSlots > 0
            && assignment.detail != VehicleAudioDetailLevel::Silent;
        if (found->second.acousticPathTracing)
            --acousticPathSlots;
        updateInstance(found->second, deltaSeconds);
    }
}

bool VehicleAudioRuntime::setEnabled(VehicleSoundHandle handle, bool enabled)
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    if (enabled && !found->second.enabled)
        found->second.eventState = {};
    found->second.enabled = enabled;
    if (!enabled)
        stopLayers(found->second);
    return true;
}

bool VehicleAudioRuntime::telemetry(
    VehicleSoundHandle handle,
    VehicleAudioTelemetry& value) const
{
    const auto found = m_instances.find(handle);
    if (found == m_instances.end())
        return false;
    value = found->second.telemetry;
    return true;
}

void VehicleAudioRuntime::updateInstance(Instance& instance, float deltaSeconds)
{
    instance.telemetry = {};
    if (!m_audio)
        return;
    if (!m_physics || !instance.enabled
        || !m_physics->vehicles().exists(instance.vehicle))
    {
        const std::array<AudioHandle, 7> handles{
            instance.exhaust, instance.intake, instance.mechanical,
            instance.transmission, instance.tires, instance.wind, instance.chassis };
        for (const AudioHandle handle : handles)
            m_audio->setHandleVolume(handle, 0.0f);
        for (const Instance::SampleVoice& voice : instance.engineSamples)
            m_audio->setHandleVolume(voice.handle, 0.0f);
        return;
    }

    auto& vehicles = m_physics->vehicles();
    auto& bodies = m_physics->rigidBodies();
    heritage::vehicles::DrivetrainState drivetrain;
    if (!vehicles.drivetrainState(instance.vehicle, drivetrain))
        return;
    const heritage::physics::BodyHandle body = vehicles.chassisBody(instance.vehicle);
    heritage::physics::RigidBodyPose pose;
    heritage::math::Vec3 right{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };
    heritage::math::Vec3 forward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 velocity{};
    const float alpha = m_physics->interpolationAlpha();
    if (!bodies.interpolatedPose(body, alpha, pose))
        return;
    bodies.interpolatedBasis(body, alpha, right, up, forward);
    bodies.linearVelocity(body, velocity);

    const AudioVector3 vehiclePosition = toAudio(pose.position);
    const float listenerDistance = distance(
        vehiclePosition, m_audio->listener().position);
    const bool interior = listenerDistance <= instance.definition.cabinRadiusMeters;
    const VehicleAudioDetailLevel detail = instance.assignedDetail;
    const float speed = vehicles.speed(instance.vehicle);
    const float load = std::clamp(
        std::abs(drivetrain.engineTorque)
            / (std::max)(instance.definition.maximumTorqueNm, 1.0f),
        0.0f,
        1.0f);

    float slip = 0.0f;
    float wetness = 0.0f;
    float suspensionActivity = 0.0f;
    const std::size_t wheelCount = vehicles.wheelCount(instance.vehicle);
    std::size_t sampledWheels = 0;
    for (std::size_t wheelIndex = 0; wheelIndex < wheelCount; ++wheelIndex)
    {
        heritage::vehicles::WheelState wheel;
        if (!vehicles.wheelState(instance.vehicle, wheelIndex, wheel))
            continue;
        slip += std::abs(static_cast<float>(wheel.slipRatio))
            + std::abs(static_cast<float>(wheel.slipAngleDegrees)) / 18.0f;
        wetness += static_cast<float>(wheel.surfaceWetness);
        suspensionActivity += std::abs(static_cast<float>(wheel.compressionVelocity));
        ++sampledWheels;
    }
    if (sampledWheels > 0)
    {
        const float inverse = 1.0f / static_cast<float>(sampledWheels);
        slip *= inverse;
        wetness *= inverse;
        suspensionActivity *= inverse;
    }

    const EngineAudioMix engine = evaluateEngineAudio(
        instance.definition, drivetrain.engineRpm, load, interior, detail);
    const DrivetrainAudioMix transmission = evaluateDrivetrainAudio(
        instance.definition, speed, load, drivetrain.clutchSlipRpm,
        drivetrain.currentGear, interior, detail);
    const TireAudioMix tires = evaluateTireAudio(
        instance.definition, speed, slip, wetness, interior, detail);
    const AerodynamicAudioMix wind = evaluateAerodynamicAudio(
        instance.definition, speed, interior, detail);
    const ChassisAudioMix chassis = evaluateChassisAudio(
        instance.definition, speed, suspensionActivity, interior, detail);

    const std::string prefix = "vehicle:" + instance.definition.id + ":";
    const auto realizeGenerated =
        [&](AudioHandle& handle,
            SynthesizedVehicleLayer layer,
            const char* name,
            bool required)
    {
        if (!required)
        {
            if (handle != kInvalidAudioHandle)
                m_audio->stop(handle);
            handle = kInvalidAudioHandle;
            return;
        }
        if (handle == kInvalidAudioHandle)
        {
            handle = m_audio->playGeneratedLoopCached(
                prefix + name,
                [&instance, layer]()
                {
                    return synthesizeVehicleLayer(instance.definition, layer);
                },
                AudioBus::Effects,
                0.0f,
                1.0f);
        }
    };
    const bool full = detail == VehicleAudioDetailLevel::Full;
    const bool reduced = detail == VehicleAudioDetailLevel::Reduced;
    const bool audible = detail != VehicleAudioDetailLevel::Silent;
    realizeGenerated(instance.exhaust, SynthesizedVehicleLayer::Exhaust,
        "exhaust", audible);
    realizeGenerated(instance.intake, SynthesizedVehicleLayer::Intake,
        "intake", full);
    realizeGenerated(instance.mechanical, SynthesizedVehicleLayer::Mechanical,
        "mechanical", full);
    realizeGenerated(instance.transmission, SynthesizedVehicleLayer::Transmission,
        "transmission", full || reduced);
    realizeGenerated(instance.tires, SynthesizedVehicleLayer::Tire,
        "tires", full || reduced);
    realizeGenerated(instance.wind, SynthesizedVehicleLayer::Wind,
        "wind", full);
    realizeGenerated(instance.chassis, SynthesizedVehicleLayer::Chassis,
        "chassis", full);

    const std::vector<EngineSampleVoiceMix> sampleMix = evaluateEngineSampleBank(
        instance.definition, drivetrain.engineRpm, engine.exhaustGain, detail);
    for (std::size_t index = 0; index < instance.engineSamples.size(); ++index)
    {
        Instance::SampleVoice& voice = instance.engineSamples[index];
        const bool required = voice.available
            && index < sampleMix.size()
            && sampleMix[index].gain > 0.001f;
        if (!required)
        {
            if (voice.handle != kInvalidAudioHandle)
                m_audio->stop(voice.handle);
            voice.handle = kInvalidAudioHandle;
            continue;
        }
        if (voice.handle == kInvalidAudioHandle)
        {
            voice.handle = m_audio->playLoop(
                voice.sample.path, AudioBus::Effects, 0.0f, 1.0f);
            if (voice.handle == kInvalidAudioHandle)
                voice.available = false;
        }
    }

    const AudioVector3 worldEngine = transformPoint(
        pose.position, right, up, forward, instance.definition.engineEmitter);
    const AudioVector3 worldIntake = transformPoint(
        pose.position, right, up, forward, instance.definition.intakeEmitter);
    const AudioVector3 worldExhaust = transformPoint(
        pose.position, right, up, forward, instance.definition.exhaustEmitter);
    const AudioVector3 worldTransmission = transformPoint(
        pose.position, right, up, forward, instance.definition.transmissionEmitter);
    const AudioVector3 worldChassis = transformPoint(
        pose.position, right, up, forward, instance.definition.chassisEmitter);
    const AudioVector3 emitterVelocity = toAudio(velocity);
    const auto spatial = [&](AudioHandle handle, const AudioVector3& position)
    {
        AudioEmitterState emitter;
        emitter.position = position;
        emitter.velocity = emitterVelocity;
        emitter.minimumDistanceMeters = 1.2f;
        emitter.maximumDistanceMeters = instance.definition.maximumDistanceMeters;
        m_audio->setHandleSpatial(handle, emitter);
    };

    spatial(instance.exhaust, worldExhaust);
    spatial(instance.intake, worldIntake);
    spatial(instance.mechanical, worldEngine);
    spatial(instance.transmission, worldTransmission);
    spatial(instance.tires, worldChassis);
    spatial(instance.wind, worldChassis);
    spatial(instance.chassis, worldChassis);
    if (instance.startup != kInvalidAudioHandle)
        spatial(instance.startup, worldEngine);
    for (const Instance::SampleVoice& voice : instance.engineSamples)
    {
        if (voice.handle != kInvalidAudioHandle)
            spatial(voice.handle, worldEngine);
    }

    instance.acousticTraceTimerSeconds -= std::max(deltaSeconds, 0.0f);
    if (instance.acousticPathTracing
        && instance.acousticTraceTimerSeconds <= 0.0f)
    {
        // One path solution is shared by the tightly grouped emitters on a
        // vehicle. Its authoritative collision body is ignored at the source,
        // while cabin filtering remains a separate listener-side operation.
        const AudioVector3 acousticAnchor = interior ? worldEngine : worldExhaust;
        const acoustics::AcousticPathTraceInput traceInput{
            acousticAnchor,
            m_audio->listener().position,
            body,
            instance.definition.maximumDistanceMeters
        };
        instance.lastAcousticTrace = acoustics::AcousticPathTracer::trace(
            traceInput, m_physics->collisions(), bodies);
        instance.acousticTarget = {
            instance.lastAcousticTrace.directGain,
            instance.lastAcousticTrace.directOpenness,
            instance.lastAcousticTrace.earlyReflectionGain,
            instance.lastAcousticTrace.earlyReflectionDelaySeconds,
            instance.lastAcousticTrace.lateReverbGain
        };
        // Twenty vehicles at 20 Hz keeps geometry work bounded. Staggering at
        // creation prevents every source from issuing its rays in one frame.
        instance.acousticTraceTimerSeconds += 0.050f;
        if (instance.acousticTraceTimerSeconds <= 0.0f)
            instance.acousticTraceTimerSeconds = 0.050f;
    }
    else if (!instance.acousticPathTracing)
    {
        instance.acousticTarget = {};
        instance.lastAcousticTrace = {};
        instance.acousticTraceTimerSeconds = std::min(
            instance.acousticTraceTimerSeconds, 0.050f);
    }
    instance.acousticCurrent = smoothedAcoustics(
        instance.acousticCurrent,
        instance.acousticTarget,
        smoothingFactor(14.0f, deltaSeconds));

    const auto playTransient =
        [&](const std::vector<std::filesystem::path>& choices,
            const AudioVector3& position,
            float relativeGain)
    {
        const std::size_t voiceLimit = static_cast<std::size_t>(std::clamp(
            instance.definition.events.maximumVoices, 1, 16));
        if (choices.empty() || instance.transients.size() >= voiceLimit
            || detail == VehicleAudioDetailLevel::Silent
            || m_transientVoiceSlots <= 0)
            return;
        const std::filesystem::path& path = choices[
            instance.eventSequence % choices.size()];
        const float variation = 0.97f
            + 0.02f * static_cast<float>(instance.eventSequence % 4);
        ++instance.eventSequence;
        const AudioHandle handle = m_audio->playOneShot(
            path,
            AudioBus::Effects,
            std::clamp(instance.definition.events.gain * relativeGain, 0.0f, 1.0f),
            variation);
        if (handle == kInvalidAudioHandle)
            return;
        AudioEmitterState emitter;
        emitter.position = position;
        emitter.velocity = emitterVelocity;
        emitter.minimumDistanceMeters = 1.0f;
        emitter.maximumDistanceMeters = std::min(
            instance.definition.maximumDistanceMeters, 120.0f);
        m_audio->setHandleSpatial(handle, emitter);
        m_audio->setHandleLowPass(handle, interior ? 0.38f : 1.0f);
        m_audio->setHandleAcoustics(handle, instance.acousticCurrent);
        instance.transients.push_back(handle);
        --m_transientVoiceSlots;
    };

    const VehicleAudioEventFrame events = evaluateVehicleAudioEvents(
        {
            drivetrain.currentGear,
            suspensionActivity,
            deltaSeconds,
            drivetrain.engineRpm,
            drivetrain.engineTorque,
            instance.definition.redlineRpm
        },
        instance.eventState);
    if (events.gearShift)
    {
        playTransient(
            instance.definition.events.gearShift,
            worldTransmission,
            0.45f + 0.45f * load);
    }
    if (events.suspensionLight || events.suspensionHeavy)
    {
        playTransient(
            events.suspensionHeavy ? instance.definition.events.suspensionHeavy
                : instance.definition.events.suspensionLight,
            worldChassis,
            events.suspensionHeavy
                ? 1.0f : std::clamp(suspensionActivity, 0.35f, 0.75f));
    }

    const auto playGeneratedTransient = [&]
    (
        SynthesizedVehicleTransient transient,
        const char* name,
        float relativeGain)
    {
        const std::size_t voiceLimit = static_cast<std::size_t>(std::clamp(
            instance.definition.events.maximumVoices, 1, 16));
        if (instance.transients.size() >= voiceLimit
            || detail == VehicleAudioDetailLevel::Silent
            || m_transientVoiceSlots <= 0)
            return;
        const AudioHandle handle = m_audio->playGeneratedOneShotCached(
            prefix + name,
            [&instance, transient]()
            {
                return synthesizeVehicleTransient(instance.definition, transient);
            },
            AudioBus::Effects,
            std::clamp(relativeGain, 0.0f, 1.0f),
            engine.pitch);
        if (handle == kInvalidAudioHandle)
            return;
        AudioEmitterState emitter;
        emitter.position = worldExhaust;
        emitter.velocity = emitterVelocity;
        emitter.minimumDistanceMeters = 1.0f;
        emitter.maximumDistanceMeters = std::min(
            instance.definition.maximumDistanceMeters, 140.0f);
        m_audio->setHandleSpatial(handle, emitter);
        m_audio->setHandleLowPass(handle, interior ? 0.18f : 0.92f);
        m_audio->setHandleAcoustics(handle, instance.acousticCurrent);
        instance.transients.push_back(handle);
        --m_transientVoiceSlots;
    };
    if (events.revLimiterCut)
    {
        playGeneratedTransient(
            SynthesizedVehicleTransient::RevLimiterCut,
            "rev_limiter_cut",
            0.42f * instance.definition.gains.exhaust);
    }
    if (events.overrunPop)
    {
        playGeneratedTransient(
            SynthesizedVehicleTransient::OverrunPop,
            "overrun_pop",
            0.30f * instance.definition.gains.exhaust);
    }

    const auto apply = [&](AudioHandle handle, float gain, float pitch, float openness)
    {
        m_audio->setHandleVolume(handle, std::clamp(gain, 0.0f, 1.0f));
        m_audio->setHandlePitch(handle, pitch);
        m_audio->setHandleLowPass(handle, openness);
        m_audio->setHandleAcoustics(handle, instance.acousticCurrent);
    };
    const bool recordedBankAvailable = std::any_of(
        instance.engineSamples.begin(), instance.engineSamples.end(),
        [](const Instance::SampleVoice& voice) { return voice.available; });
    const float proceduralGain = recordedBankAvailable
        ? instance.definition.samples.proceduralGain : 1.0f;
    apply(instance.exhaust, engine.exhaustGain * proceduralGain,
        engine.pitch, engine.exhaustOpenness);
    apply(instance.intake, engine.intakeGain * proceduralGain,
        engine.pitch, engine.intakeOpenness);
    apply(instance.mechanical, engine.mechanicalGain * proceduralGain,
        engine.pitch, engine.mechanicalOpenness);
    apply(instance.transmission, transmission.gain, transmission.pitch, transmission.openness);
    apply(instance.tires, tires.gain, tires.pitch, tires.openness);
    apply(instance.wind, wind.gain, wind.pitch, wind.openness);
    apply(instance.chassis, chassis.gain, chassis.pitch, chassis.openness);

    int activeSampleVoices = 0;
    for (std::size_t index = 0; index < instance.engineSamples.size(); ++index)
    {
        const Instance::SampleVoice& voice = instance.engineSamples[index];
        const EngineSampleVoiceMix voiceMix = index < sampleMix.size()
            ? sampleMix[index] : EngineSampleVoiceMix{};
        if (voice.handle != kInvalidAudioHandle)
        {
            // Contact-microphone recordings already contain their physical
            // spectral envelope. Reusing the procedural exhaust's very closed
            // idle/cockpit filter removed nearly all useful upper harmonics.
            const float recordedOpenness = interior
                ? std::max(engine.exhaustOpenness, 0.52f)
                : std::max(engine.exhaustOpenness, 0.94f);
            apply(voice.handle, voiceMix.gain, voiceMix.pitch, recordedOpenness);
            activeSampleVoices += voiceMix.gain > 0.001f ? 1 : 0;
        }
    }

    instance.telemetry.valid = true;
    instance.telemetry.interior = interior;
    instance.telemetry.detail = detail;
    instance.telemetry.distanceMeters = listenerDistance;
    instance.telemetry.engineRpm = drivetrain.engineRpm;
    instance.telemetry.engineLoad = load;
    instance.telemetry.speedMetersPerSecond = speed;
    instance.telemetry.averageTireSlip = slip;
    instance.telemetry.suspensionActivity = suspensionActivity;
    instance.telemetry.gear = drivetrain.currentGear;
    const std::array<AudioHandle, 7> continuousHandles{
        instance.exhaust, instance.intake, instance.mechanical,
        instance.transmission, instance.tires, instance.wind, instance.chassis };
    instance.telemetry.activeLayerCount = static_cast<int>(std::count_if(
        continuousHandles.begin(), continuousHandles.end(),
        [](AudioHandle handle) { return handle != kInvalidAudioHandle; }));
    instance.telemetry.activeSampleVoices = activeSampleVoices;
    instance.telemetry.activeTransientVoices = static_cast<int>(instance.transients.size());
    instance.telemetry.acousticPathTraced = instance.acousticPathTracing;
    instance.telemetry.directPathOccluded =
        instance.lastAcousticTrace.directOccluded;
    instance.telemetry.acousticRayCount =
        instance.lastAcousticTrace.tracedRayCount;
    instance.telemetry.reflectionPathCount =
        instance.lastAcousticTrace.validReflectionPathCount;
    instance.telemetry.directPathGain = instance.acousticCurrent.directGain;
    instance.telemetry.reflectionGain =
        instance.acousticCurrent.earlyReflectionGain;
    instance.telemetry.reflectionDelayMilliseconds = 1000.0f
        * instance.acousticCurrent.earlyReflectionDelaySeconds;
}

void VehicleAudioRuntime::stopLayers(Instance& instance)
{
    if (!m_audio)
        return;
    const std::array<AudioHandle, 7> handles{
        instance.exhaust, instance.intake, instance.mechanical,
        instance.transmission, instance.tires, instance.wind, instance.chassis };
    for (const AudioHandle handle : handles)
    {
        if (handle != kInvalidAudioHandle)
            m_audio->stop(handle);
    }
    instance.exhaust = kInvalidAudioHandle;
    instance.intake = kInvalidAudioHandle;
    instance.mechanical = kInvalidAudioHandle;
    instance.transmission = kInvalidAudioHandle;
    instance.tires = kInvalidAudioHandle;
    instance.wind = kInvalidAudioHandle;
    instance.chassis = kInvalidAudioHandle;
    if (instance.startup != kInvalidAudioHandle)
        m_audio->stop(instance.startup);
    instance.startup = kInvalidAudioHandle;
    for (const Instance::SampleVoice& voice : instance.engineSamples)
    {
        if (voice.handle != kInvalidAudioHandle)
            m_audio->stop(voice.handle);
    }
    for (Instance::SampleVoice& voice : instance.engineSamples)
        voice.handle = kInvalidAudioHandle;
    for (const AudioHandle transient : instance.transients)
    {
        if (transient != kInvalidAudioHandle)
            m_audio->stop(transient);
    }
    instance.transients.clear();
}

} // namespace heritage::audio::vehicles
