#pragma once

#include "../Geometry/TrailingArm/TrailingArmKinematics.hpp"

#include <string>

namespace heritage::vehicles {

// Low-confidence assisted authoring for an independent trailing arm with a
// transverse torsion-bar spring and separate damper. The scale input belongs
// to the chassis reference package, not the currently installed wheel/tire.
struct TrailingArmHardpointEstimateInput
{
    heritage::math::Vec3 wheelCenter{};
    float referencePackageScaleM = 0.30f;
};

struct TrailingArmHardpointEstimateResult
{
    bool valid = false;
    TrailingArmHardpoints hardpoints;
    std::string profileId;
    float confidence = 0.0f;
};

TrailingArmHardpointEstimateResult estimateTrailingArmHardpointsV1(
    const TrailingArmHardpointEstimateInput& input);

} // namespace heritage::vehicles
