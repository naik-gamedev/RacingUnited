#include "SuspensionModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

float digressiveDamperForce(
    float shaftVelocityMps,
    float lowSpeedDampingNsPerM,
    float highSpeedDampingNsPerM,
    float kneeVelocityMps)
{
    const float speed = std::abs(shaftVelocityMps);
    const float knee = std::max(kneeVelocityMps, 0.0f);
    const float forceMagnitude = speed <= knee
        ? lowSpeedDampingNsPerM * speed
        : lowSpeedDampingNsPerM * knee
            + highSpeedDampingNsPerM * (speed - knee);
    return std::copysign(forceMagnitude, shaftVelocityMps);
}

} // namespace

const char* suspensionProviderId(SuspensionProviderKind provider)
{
    switch (provider)
    {
    case SuspensionProviderKind::LinearRaycastV1:
        return "linear_raycast_v1";
    }
    return "unknown";
}

bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider)
{
    if (id == "linear_raycast_v1")
    {
        provider = SuspensionProviderKind::LinearRaycastV1;
        return true;
    }
    return false;
}

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input)
{
    SuspensionModelOutput output;
    if (description.provider != SuspensionProviderKind::LinearRaycastV1)
        return output;

    const float motionRatio = std::max(description.motionRatio, 0.0f);
    const float forceRatio = motionRatio * motionRatio;
    const float progressiveForceRatio = forceRatio * motionRatio;
    output.springForceN = description.springPreloadN * motionRatio
        + description.springRateNPerM * input.compressionM * forceRatio
        + 0.5f * description.springProgressionNPerM2
            * input.compressionM * std::abs(input.compressionM)
            * progressiveForceRatio;

    const float shaftVelocity = input.compressionVelocityMps * motionRatio;
    const bool bump = shaftVelocity >= 0.0f;
    const float damperForceAtShaft = digressiveDamperForce(
        shaftVelocity,
        bump ? description.bumpDampingNsPerM
            : description.reboundDampingNsPerM,
        bump ? description.bumpHighSpeedDampingNsPerM
            : description.reboundHighSpeedDampingNsPerM,
        bump ? description.bumpDampingKneeVelocityMps
            : description.reboundDampingKneeVelocityMps);
    output.dampingForceN = damperForceAtShaft * motionRatio;
    output.damperDissipationW = std::max(
        damperForceAtShaft * shaftVelocity,
        0.0f);

    const float bumpStopTravel = std::max(
        input.compressionM - description.bumpStopEngagementM,
        0.0f);
    output.bumpStopForceN = description.bumpStopRateNPerM * bumpStopTravel
        + 0.5f * description.bumpStopProgressionNPerM2
            * bumpStopTravel * bumpStopTravel;
    const float droopStopTravel = std::max(
        -input.compressionM - description.droopStopEngagementM,
        0.0f);
    output.droopStopForceN = description.droopStopRateNPerM
        * droopStopTravel;
    output.unclampedForceN = output.springForceN + output.dampingForceN
        + output.bumpStopForceN - output.droopStopForceN;
    output.normalForceN = std::clamp(
        output.unclampedForceN,
        0.0f,
        std::max(description.maximumForceN, 0.0f));
    return output;
}

} // namespace heritage::vehicles
