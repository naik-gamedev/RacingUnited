#include "SuspensionModel.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {

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
    const float damping = input.compressionVelocityMps >= 0.0f
        ? description.bumpDampingNsPerM
        : description.reboundDampingNsPerM;
    output.springForceN = description.springRateNPerM
        * input.compressionM * forceRatio;
    output.dampingForceN = damping
        * input.compressionVelocityMps * forceRatio;
    output.normalForceN = std::clamp(
        output.springForceN + output.dampingForceN,
        0.0f,
        std::max(description.maximumForceN, 0.0f));
    return output;
}

} // namespace heritage::vehicles
