#include "TirePartDefinition.hpp"

#include <cmath>

namespace heritage::vehicles::tires {

const char* tireFamilyName(TireFamily family)
{
    switch (family)
    {
    case TireFamily::RoadSummerPerformance: return "Road summer/performance";
    case TireFamily::RacingSlick: return "Racing slick";
    case TireFamily::RacingWet: return "Racing wet";
    case TireFamily::Winter: return "Winter";
    case TireFamily::StuddedIce: return "Studded ice";
    case TireFamily::RallyGravel: return "Rally gravel";
    case TireFamily::Motorcycle: return "Motorcycle";
    case TireFamily::Kart: return "Kart";
    case TireFamily::CommercialTruck: return "Commercial/truck";
    case TireFamily::LowPressureOffRoad: return "Low-pressure off-road";
    }
    return "Unknown";
}

bool validTireFamily(TireFamily family)
{
    switch (family)
    {
    case TireFamily::RoadSummerPerformance:
    case TireFamily::RacingSlick:
    case TireFamily::RacingWet:
    case TireFamily::Winter:
    case TireFamily::StuddedIce:
    case TireFamily::RallyGravel:
    case TireFamily::Motorcycle:
    case TireFamily::Kart:
    case TireFamily::CommercialTruck:
    case TireFamily::LowPressureOffRoad:
        return true;
    }
    return false;
}

namespace {

bool validBiasValue(VehicleScalar value)
{
    return std::isfinite(value)
        && value >= kMinimumTirePerformanceBias
        && value <= kMaximumTirePerformanceBias;
}

} // namespace

bool validTireEngineeringData(const TirePartEngineeringData& engineering)
{
    return std::isfinite(static_cast<double>(engineering.sectionWidthM))
        && engineering.sectionWidthM >= 0.08
        && engineering.sectionWidthM <= 0.80
        && std::isfinite(static_cast<double>(engineering.aspectRatio))
        && engineering.aspectRatio >= 0.15
        && engineering.aspectRatio <= 1.20
        && std::isfinite(static_cast<double>(engineering.rimRadiusM))
        && engineering.rimRadiusM >= 0.08
        && engineering.rimRadiusM <= 0.60
        && std::isfinite(static_cast<double>(engineering.nominalLoadN))
        && engineering.nominalLoadN >= 100.0
        && engineering.nominalLoadN <= 100000.0
        && std::isfinite(static_cast<double>(engineering.referenceInflationPressurePa))
        && engineering.referenceInflationPressurePa >= 20000.0
        && engineering.referenceInflationPressurePa <= 2000000.0;
}

bool validTirePerformanceBias(const TirePerformanceBias& bias)
{
    return validBiasValue(bias.dry)
        && validBiasValue(bias.wet)
        && validBiasValue(bias.snowIce)
        && validBiasValue(bias.mud)
        && validBiasValue(bias.sand)
        && validBiasValue(bias.gravel)
        && validBiasValue(bias.wearEndurance);
}

bool validTirePartDefinition(const TirePartDefinition& definition)
{
    return !definition.id.empty()
        && !definition.displayName.empty()
        && validTireFamily(definition.family)
        && validTireEngineeringData(definition.engineering)
        && std::isfinite(static_cast<double>(definition.propertyConfidence))
        && definition.propertyConfidence >= 0.0
        && definition.propertyConfidence <= 1.0
        && validTirePerformanceBias(definition.performanceBias);
}

} // namespace heritage::vehicles::tires
