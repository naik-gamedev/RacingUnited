#include "TrailingArmHardpointEstimator.hpp"

#include <cmath>

namespace heritage::vehicles {
namespace {

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

} // namespace

TrailingArmHardpointEstimateResult estimateTrailingArmHardpointsV1(
    const TrailingArmHardpointEstimateInput& input)
{
    TrailingArmHardpointEstimateResult result;
    result.profileId = "estimated_trailing_arm_torsion_bar_road_v1";
    result.confidence = 0.30f;

    const float scaleM = input.referencePackageScaleM;
    if (!finite(input.wheelCenter)
        || !std::isfinite(scaleM)
        || scaleM < 0.20f
        || scaleM > 0.60f)
    {
        return result;
    }

    const float sideSign = input.wheelCenter.x < 0.0f ? -1.0f : 1.0f;
    const float inboardSign = -sideSign;

    // This is deliberately a conservative packaging estimate rather than a
    // claim about Peugeot factory coordinates. The pivot is placed forward of
    // the wheel, almost transversely across the car; the lower damper eye moves
    // with the arm and the upper eye remains chassis-fixed. Every point carries
    // low confidence and can be replaced independently by GLB/measured data.
    const heritage::math::Vec3 pivotCenter = add(
        input.wheelCenter,
        { inboardSign * scaleM * 0.08f,
          scaleM * 0.10f,
          scaleM * 1.85f });

    TrailingArmHardpoints hardpoints;
    hardpoints.authored = true;
    hardpoints.armPivotInner = add(
        pivotCenter,
        { inboardSign * scaleM * 0.42f, 0.0f, 0.0f });
    hardpoints.armPivotOuter = add(
        pivotCenter,
        { sideSign * scaleM * 0.20f, 0.0f, 0.0f });
    hardpoints.wheelCenter = input.wheelCenter;
    hardpoints.damperLowerMount = add(
        input.wheelCenter,
        { inboardSign * scaleM * 0.32f,
          scaleM * 0.12f,
          scaleM * 0.52f });
    hardpoints.damperUpperMount = add(
        input.wheelCenter,
        { inboardSign * scaleM * 0.56f,
          scaleM * 1.18f,
          scaleM * 0.86f });

    if (!validTrailingArmHardpoints(hardpoints))
        return result;

    result.hardpoints = hardpoints;
    result.valid = true;
    return result;
}

} // namespace heritage::vehicles
