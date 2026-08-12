#include "SuspensionAntiRollBar.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

bool finiteScalar(VehicleScalar value)
{
    return std::isfinite(value);
}

VehicleScalar safeLever(VehicleScalar value)
{
    return std::max<VehicleScalar>(std::abs(value), 1.0e-4);
}

} // namespace

bool validSuspensionAntiRollBarDescription(
    const SuspensionAntiRollBarDescription& description)
{
    return finiteScalar(description.torsionalStiffnessNmPerRad)
        && description.torsionalStiffnessNmPerRad >= 0.0
        && finiteScalar(description.torsionalDampingNmsPerRad)
        && description.torsionalDampingNmsPerRad >= 0.0
        && finiteScalar(description.leftLeverArmM)
        && description.leftLeverArmM > 0.0
        && finiteScalar(description.rightLeverArmM)
        && description.rightLeverArmM > 0.0
        && finiteScalar(description.leftLinkMotionRatio)
        && description.leftLinkMotionRatio > 0.0
        && finiteScalar(description.rightLinkMotionRatio)
        && description.rightLinkMotionRatio > 0.0
        && finiteScalar(description.maximumWheelForceN)
        && description.maximumWheelForceN >= 0.0;
}

SuspensionAntiRollBarOutput evaluateSuspensionAntiRollBar(
    const SuspensionAntiRollBarDescription& description,
    const SuspensionAntiRollBarInput& input)
{
    SuspensionAntiRollBarOutput output;
    if (!description.enabled
        || !validSuspensionAntiRollBarDescription(description))
    {
        return output;
    }

    const VehicleScalar leftLever = safeLever(description.leftLeverArmM);
    const VehicleScalar rightLever = safeLever(description.rightLeverArmM);
    const VehicleScalar leftAngle = input.leftCompressionM
        * description.leftLinkMotionRatio / leftLever;
    const VehicleScalar rightAngle = input.rightCompressionM
        * description.rightLinkMotionRatio / rightLever;
    const VehicleScalar leftAngularVelocity = input.leftCompressionVelocityMps
        * description.leftLinkMotionRatio / leftLever;
    const VehicleScalar rightAngularVelocity = input.rightCompressionVelocityMps
        * description.rightLinkMotionRatio / rightLever;

    output.twistRadians = leftAngle - rightAngle;
    output.twistRateRadiansPerSecond =
        leftAngularVelocity - rightAngularVelocity;
    output.elasticTorqueNm = description.torsionalStiffnessNmPerRad
        * output.twistRadians;
    output.dampingTorqueNm = description.torsionalDampingNmsPerRad
        * output.twistRateRadiansPerSecond;
    output.totalTorqueNm = output.elasticTorqueNm + output.dampingTorqueNm;

    const VehicleScalar maximumForce = description.maximumWheelForceN;
    const VehicleScalar rawLeftForce = output.totalTorqueNm
        * description.leftLinkMotionRatio / leftLever;
    const VehicleScalar rawRightForce = -output.totalTorqueNm
        * description.rightLinkMotionRatio / rightLever;
    output.leftWheelForceN = std::clamp(
        rawLeftForce, -maximumForce, maximumForce);
    output.rightWheelForceN = std::clamp(
        rawRightForce, -maximumForce, maximumForce);
    return output;
}

} // namespace heritage::vehicles
