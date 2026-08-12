#pragma once

#include "../../VehiclePrecision.hpp"

#include <cstdint>
#include <string>

namespace heritage::vehicles::tires {

// TIRE17 reusable specialty-family taxonomy. Family names select a neutral
// Heritage baseline only; manufacturer/model strings never imply performance.
enum class TireFamily : std::uint8_t
{
    RoadSummerPerformance = 0,
    RacingSlick,
    RacingWet,
    Winter,
    StuddedIce,
    RallyGravel,
    Motorcycle,
    Kart,
    CommercialTruck,
    LowPressureOffRoad
};

const char* tireFamilyName(TireFamily family);
bool validTireFamily(TireFamily family);

// Creator-facing authoring biases. Zero means Heritage's dimension/construction-
// derived average baseline. Values are deliberately bounded and are inputs to
// coherent parameter generation; they are NOT final-force multipliers.
struct TirePerformanceBias
{
    VehicleScalar dry = 0.0;
    VehicleScalar wet = 0.0;
    VehicleScalar snowIce = 0.0;
    VehicleScalar mud = 0.0;
    VehicleScalar sand = 0.0;
    VehicleScalar gravel = 0.0;
    VehicleScalar wearEndurance = 0.0;
};


struct TirePartEngineeringData
{
    VehicleScalar sectionWidthM = 0.205;
    VehicleScalar aspectRatio = 0.45;
    VehicleScalar rimRadiusM = 0.2159;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar referenceInflationPressurePa = 220000.0;
};

struct TirePartDefinition
{
    std::string id;
    std::string displayName;
    std::string manufacturer;
    std::string model;
    std::string propertyFile;
    std::string propertyProvenance;
    VehicleScalar propertyConfidence = 0.0;
    TirePartEngineeringData engineering;
    TireFamily family = TireFamily::RoadSummerPerformance;
    TirePerformanceBias performanceBias;
};

constexpr VehicleScalar kMinimumTirePerformanceBias = -1.0;
constexpr VehicleScalar kMaximumTirePerformanceBias = 1.0;

bool validTireEngineeringData(const TirePartEngineeringData& engineering);
bool validTirePerformanceBias(const TirePerformanceBias& bias);
bool validTirePartDefinition(const TirePartDefinition& definition);

} // namespace heritage::vehicles::tires
