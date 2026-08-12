#pragma once

#include "TireFamilyBaseline.hpp"
#include "../../TireModel.hpp"

#include <filesystem>
#include <string>

namespace heritage::vehicles::tires {

inline constexpr int kTirePartResolverVersion = 1;

// Runtime fitment belongs to the vehicle/wheel, not to the reusable tire part.
// Zero cold pressure means use the tire part/reference dataset pressure.
struct TirePartFitment
{
    VehicleScalar coldInflationPressurePa = 0.0;
};

enum class TirePartResolutionSource : std::uint8_t
{
    None = 0,
    EstimatedFamilyBaseline,
    AuthoritativePropertyFile
};

struct TirePartResolutionResult
{
    bool valid = false;
    TirePartResolutionSource source = TirePartResolutionSource::None;
    bool fitmentPressureApplied = false;
    heritage::vehicles::TireModelDescription model;
    std::filesystem::path resolvedPropertyFile;
    std::string errorMessage;
};

struct TirePartAssignmentInfo
{
    bool assigned = false;
    std::string partId;
    std::string displayName;
    TireFamily family = TireFamily::RoadSummerPerformance;
    TirePartResolutionSource source = TirePartResolutionSource::None;
    VehicleScalar coldInflationPressurePa = 0.0;
};

bool validTirePartFitment(const TirePartFitment& fitment);

// Resolve one reusable tire-part definition into a runtime TireModelDescription.
// Explicit property-file data is authoritative. Otherwise Heritage generates a
// low-confidence family baseline from engineering dimensions and creator biases.
TirePartResolutionResult resolveTirePart(
    const TirePartDefinition& definition,
    const std::filesystem::path& propertyRoot = {},
    const TirePartFitment& fitment = {});

} // namespace heritage::vehicles::tires
