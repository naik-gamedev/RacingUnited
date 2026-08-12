#include "TorsionBar.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {

TorsionBarEquivalentOutput evaluateEquivalentTorsionBar(
    const TorsionBarEquivalentDescription& description,
    const TorsionBarEquivalentInput& input)
{
    TorsionBarEquivalentOutput output;
    const double referenceRatio = std::abs(
        input.referenceAngularMotionRatioRadPerM);
    const double currentRatio = std::abs(
        input.angularMotionRatioRadPerM);
    if (!std::isfinite(description.referenceWheelPreloadN)
        || !std::isfinite(description.referenceWheelRateNPerM)
        || !std::isfinite(description.referenceWheelProgressionNPerM2)
        || !std::isfinite(description.maximumWheelForceN)
        || !std::isfinite(input.twistRadians)
        || !std::isfinite(referenceRatio)
        || !std::isfinite(currentRatio)
        || description.referenceWheelPreloadN < 0.0
        || description.referenceWheelRateNPerM <= 0.0
        || description.referenceWheelProgressionNPerM2 < 0.0
        || description.maximumWheelForceN <= 0.0
        || referenceRatio <= 0.000001
        || currentRatio <= 0.000001)
    {
        return output;
    }

    output.preloadTorqueNm =
        description.referenceWheelPreloadN / referenceRatio;
    output.stiffnessNmPerRad =
        description.referenceWheelRateNPerM
        / (referenceRatio * referenceRatio);
    output.progressionNmPerRad2 =
        description.referenceWheelProgressionNPerM2
        / (referenceRatio * referenceRatio * referenceRatio);
    output.springTorqueNm = output.preloadTorqueNm
        + output.stiffnessNmPerRad * input.twistRadians
        + 0.5 * output.progressionNmPerRad2
            * input.twistRadians * std::abs(input.twistRadians);
    output.wheelForceN = std::clamp(
        output.springTorqueNm * currentRatio,
        -description.maximumWheelForceN,
        description.maximumWheelForceN);
    output.valid = true;
    return output;
}

} // namespace heritage::vehicles
