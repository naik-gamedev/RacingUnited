#pragma once

#include "TireFlexibleRingField.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace heritage::vehicles::tires {

inline constexpr std::size_t TireCarcassDevelopmentGroupCount = 16;

struct TireCarcassDevelopmentParameterInfo
{
    std::string key;
    std::string label;
    std::string group;
    std::size_t groupIndex = 0;
    VehicleScalar minimum = 0.0;
    VehicleScalar maximum = 1.0;
    VehicleScalar defaultValue = 1.0;
    bool integer = false;
};

std::size_t tireCarcassDevelopmentParameterCount();
bool tireCarcassDevelopmentParameterInfo(
    std::size_t index,
    TireCarcassDevelopmentParameterInfo& value);
VehicleScalar tireCarcassDevelopmentParameterValue(
    const TireFlexibleRingDevelopmentTuning& tuning,
    std::size_t index);
bool setTireCarcassDevelopmentParameterValue(
    TireFlexibleRingDevelopmentTuning& tuning,
    std::size_t index,
    VehicleScalar value);
void resetTireCarcassDevelopmentTuning(
    TireFlexibleRingDevelopmentTuning& tuning,
    bool enabled = true);
const char* tireCarcassDevelopmentGroupName(std::size_t groupIndex);

enum class TireCarcassSyntheticScenario : std::uint8_t
{
    StaticFlat = 0,
    HardAcceleration = 1,
    HardBraking = 2,
    HardCornering = 3,
    CombinedBrakingCornering = 4,
    LowPressure = 5,
    ZeroPressure = 6,
    PartialRoadEdge = 7,
    BankedRoad = 8,
    FlatSpot = 9,
    AirborneRelaxation = 10,
    HighPressure = 11
};

const char* tireCarcassSyntheticScenarioName(TireCarcassSyntheticScenario value);
bool tireCarcassSyntheticScenarioFromName(
    const std::string& name,
    TireCarcassSyntheticScenario& value);

struct TireCarcassSyntheticInput
{
    TireFlexibleRingFieldDescription description;
    VehicleScalar referencePressurePa = 220000.0;
    VehicleScalar normalLoadN = 3200.0;
    VehicleScalar roadOverlapM = 0.018;
    std::size_t integrationSteps = 48;
};

struct TireCarcassSyntheticResult
{
    bool valid = false;
    std::string scenario;
    std::size_t integrationSteps = 0;
    VehicleScalar pathologyScore = 0.0;
    VehicleScalar roadPenetrationMm = 0.0;
    VehicleScalar rimPenetrationMm = 0.0;
    VehicleScalar lowerHookMm = 0.0;
    VehicleScalar staticAsymmetryMm = 0.0;
    VehicleScalar footprintHeightRangeMm = 0.0;
    VehicleScalar maximumDisplacementMm = 0.0;
    VehicleScalar rmsVelocityMps = 0.0;
    VehicleScalar centerBottomHeightMm = 0.0;
    VehicleScalar frontBottomHeightMm = 0.0;
    VehicleScalar rearBottomHeightMm = 0.0;
    VehicleScalar centerForwardDisplacementMm = 0.0;
    VehicleScalar centerDownDisplacementMm = 0.0;
    VehicleScalar centerLateralDisplacementMm = 0.0;
    VehicleScalar centerRadialDisplacementMm = 0.0;
    VehicleScalar centerTangentialDisplacementMm = 0.0;
    std::array<VehicleScalar, TireFlexibleRingFieldStations>
        radialProfileMm{};
    std::array<VehicleScalar, TireFlexibleRingFieldBands>
        bottomCrossSectionMm{};
};

TireCarcassSyntheticResult runTireCarcassSyntheticScenario(
    const TireCarcassSyntheticInput& input,
    const TireFlexibleRingDevelopmentTuning& tuning,
    TireCarcassSyntheticScenario scenario);

struct TireCarcassSearchBatchResult
{
    bool valid = false;
    std::uint64_t bestTrialIndex = 0;
    VehicleScalar bestScore = 0.0;
    std::size_t evaluatedCount = 0;
    VehicleScalar elapsedSeconds = 0.0;
    TireCarcassSyntheticResult bestScenario;
};

TireFlexibleRingDevelopmentTuning tireCarcassDevelopmentTrialTuning(
    const TireFlexibleRingDevelopmentTuning& base,
    std::uint64_t seed,
    std::uint64_t trialIndex,
    VehicleScalar spread,
    std::uint32_t groupMask);

TireCarcassSearchBatchResult runTireCarcassSearchBatch(
    const TireCarcassSyntheticInput& input,
    const TireFlexibleRingDevelopmentTuning& base,
    TireCarcassSyntheticScenario scenario,
    std::uint64_t seed,
    std::uint64_t firstTrialIndex,
    std::size_t trialCount,
    VehicleScalar spread,
    std::uint32_t groupMask);

} // namespace heritage::vehicles::tires
