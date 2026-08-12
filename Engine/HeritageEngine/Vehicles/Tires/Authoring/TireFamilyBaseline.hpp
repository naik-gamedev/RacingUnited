#pragma once

#include "TirePartDefinition.hpp"
#include "TirePerformanceBiasMapping.hpp"
#include "../../TireModel.hpp"

#include <string>

namespace heritage::vehicles::tires {

// Physical inputs used when no measured/fitted .tir dataset exists. The
// resulting model is an explicit low-confidence Heritage estimate and never
// overrides identified data.
struct TireFamilyBaselineInput
{
    VehicleScalar sectionWidthM = 0.205;
    VehicleScalar aspectRatio = 0.45;
    VehicleScalar rimRadiusM = 0.2159;
    VehicleScalar nominalLoadN = 3500.0;
    VehicleScalar inflationPressurePa = 220000.0;
};

struct TireFamilyBaselineResult
{
    bool valid = false;
    heritage::vehicles::TireModelDescription model;
    std::string errorMessage;
};

bool validTireFamilyBaselineInput(const TireFamilyBaselineInput& input);

TireFamilyBaselineResult buildTireFamilyBaseline(
    TireFamily family,
    const TireFamilyBaselineInput& input,
    const heritage::vehicles::TireModelDescription& seed = {});

TireFamilyBaselineResult buildTireFamilyBaseline(
    TireFamily family,
    const TireFamilyBaselineInput& input,
    const TirePerformanceBias& performanceBias,
    const heritage::vehicles::TireModelDescription& seed = {});

} // namespace heritage::vehicles::tires
