#pragma once

#include "../Geometry/MacPherson/MacPhersonKinematics.hpp"

#include <string>

namespace heritage::vehicles {

// SUS03A assisted-authoring input. This deliberately describes only values
// that a small creator team can usually obtain with reasonable confidence:
// wheel centre, an immutable chassis reference package scale and approximate
// alignment. The estimator produces
// a coherent linkage starting point; it never claims factory-CAD accuracy.
struct MacPhersonHardpointEstimateInput
{
    heritage::math::Vec3 wheelCenter{};
    float referencePackageScaleM = 0.30f;
    float casterDegrees = 3.0f;
    float steeringAxisInclinationDegrees = 10.0f;
};

struct MacPhersonHardpointEstimateResult
{
    bool valid = false;
    MacPhersonHardpoints hardpoints;
    std::string profileId;
    float confidence = 0.0f;
};

// Builds a dimension-scaled compact-road-car MacPherson package. The scale is
// part of the chassis suspension authoring reference and must not be rebuilt
// from whatever wheel/tire happens to be installed later. The profile
// is intentionally conservative and deterministic so creators can replace any
// inferred point later with measured/asset-authored data without changing the
// provider contract.
MacPhersonHardpointEstimateResult estimateMacPhersonHardpointsV1(
    const MacPhersonHardpointEstimateInput& input);

} // namespace heritage::vehicles
