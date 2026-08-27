#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../AudioSystem.hpp"

namespace heritage::audio::vehicles {

using VehicleSoundHandle = std::uint64_t;
inline constexpr VehicleSoundHandle kInvalidVehicleSoundHandle = 0;

enum class VehicleAudioDetailLevel
{
    Full,
    Reduced,
    Crowd,
    Silent
};

struct VehicleAudioLayerGains
{
    float exhaust = 0.90f;
    float intake = 0.48f;
    float mechanical = 0.26f;
    float transmission = 0.18f;
    float tires = 0.32f;
    float wind = 0.28f;
    float chassis = 0.22f;
};

struct VehicleEngineSample
{
    std::filesystem::path path;
    float referenceRpm = 1000.0f;
    float gain = 1.0f;
};

// An optional recorded layer which complements the deterministic procedural
// model. Each loop is authored at a reference RPM; the runtime crossfades
// neighboring bands and only makes a small bounded pitch correction.
struct VehicleEngineSampleBank
{
    std::filesystem::path startupPath;
    std::vector<VehicleEngineSample> loops;
    float gain = 0.45f;
    // Retain a quiet deterministic layer beneath recorded banks so missing
    // operating states never become silent. A value of one preserves the
    // fully procedural engine; zero makes available recordings authoritative.
    float proceduralGain = 1.0f;
};

struct VehicleAudioEventBank
{
    std::vector<std::filesystem::path> gearShift;
    std::vector<std::filesystem::path> suspensionLight;
    std::vector<std::filesystem::path> suspensionHeavy;
    float gain = 0.55f;
    int maximumVoices = 6;
};

// Reduced-order acoustic description of a reciprocating engine. These values
// shape the deterministic source loops; they never alter authoritative vehicle
// physics. Engine orders are expressed relative to crankshaft speed so the
// same source remains coherent when the runtime pitch follows live RPM.
struct VehicleEngineAcousticDefinition
{
    float displacementLiters = 2.0f;
    float compressionRatio = 10.0f;
    std::vector<int> firingOrder{ 1, 3, 4, 2 };
    float exhaustPulseSharpness = 0.62f;
    float intakePulseSharpness = 0.48f;
    float exhaustHeaderImbalance = 0.04f;
    float intakeResonanceOrder = 3.0f;
    float mechanicalOrderGain = 0.22f;
    float combustionVariation = 0.025f;
    float variableIntakeTransitionRpm = 0.0f;
    float variableIntakeTransitionWidthRpm = 500.0f;
    float variableIntakeGain = 0.0f;
};

struct VehicleAudioDefinition
{
    std::string id = "generic_inline_four";
    std::string category = "car";
    int cylinderCount = 4;
    int cycleRevolutions = 2; // two for four-stroke, one for two-stroke
    float idleRpm = 900.0f;
    float redlineRpm = 7000.0f;
    float referenceRpm = 1200.0f;
    float maximumTorqueNm = 250.0f;
    VehicleEngineAcousticDefinition engineAcoustics;
    VehicleAudioLayerGains gains;
    VehicleEngineSampleBank samples;
    VehicleAudioEventBank events;

    AudioVector3 engineEmitter{ 0.0f, 0.72f, 0.85f };
    AudioVector3 intakeEmitter{ 0.35f, 0.78f, 0.95f };
    AudioVector3 exhaustEmitter{ 0.0f, 0.32f, -1.65f };
    AudioVector3 transmissionEmitter{ -0.25f, 0.48f, 0.42f };
    AudioVector3 chassisEmitter{ 0.0f, 0.48f, 0.0f };

    float cabinRadiusMeters = 1.35f;
    float fullDetailDistanceMeters = 50.0f;
    float reducedDetailDistanceMeters = 150.0f;
    float maximumDistanceMeters = 400.0f;
};

struct VehicleAudioTelemetry
{
    bool valid = false;
    bool interior = false;
    VehicleAudioDetailLevel detail = VehicleAudioDetailLevel::Silent;
    float distanceMeters = 0.0f;
    float engineRpm = 0.0f;
    float engineLoad = 0.0f;
    float speedMetersPerSecond = 0.0f;
    float averageTireSlip = 0.0f;
    float suspensionActivity = 0.0f;
    int gear = 0;
    int activeLayerCount = 0;
    int activeSampleVoices = 0;
    int activeTransientVoices = 0;
    bool acousticPathTraced = false;
    bool directPathOccluded = false;
    int acousticRayCount = 0;
    int reflectionPathCount = 0;
    float directPathGain = 1.0f;
    float reflectionGain = 0.0f;
    float reflectionDelayMilliseconds = 0.0f;
};

const char* vehicleAudioDetailName(VehicleAudioDetailLevel value);

} // namespace heritage::audio::vehicles
