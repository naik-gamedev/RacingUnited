#pragma once

#include "TirePartDefinition.hpp"
#include "../../TireModel.hpp"

#include <string>

namespace heritage::vehicles::tires {

// TIRE17B engine-owned creator-bias mapping contract. Increment this when the
// semantic mapping changes so authored parts can retain explicit provenance.
inline constexpr int kTirePerformanceBiasMappingVersion = 1;

struct TirePerformanceBiasApplicationResult
{
    bool valid = false;
    bool applied = false;
    bool authoritativeDataPreserved = false;
    heritage::vehicles::TireModelDescription model;
    std::string errorMessage;
};

bool tirePerformanceBiasIsNeutral(const TirePerformanceBias& bias);

// Applies bounded creator-facing residual calibration to an estimated tire
// model. Imported/fitted .tir data is preserved by default: brand/model names
// never select behavior and this function never post-multiplies final forces.
TirePerformanceBiasApplicationResult applyTirePerformanceBias(
    const heritage::vehicles::TireModelDescription& baseline,
    const TirePerformanceBias& bias);

} // namespace heritage::vehicles::tires
