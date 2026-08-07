#pragma once

#include <string_view>

namespace heritage::vehicles {

// Native suspension providers share this bounded force contract. Geometry
// providers will eventually calculate wheel pose and motion ratio from linkage
// points; the first provider uses the existing massless linear raycast path.
enum class SuspensionProviderKind
{
    LinearRaycastV1 = 0
};

struct SuspensionModelDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    float springRateNPerM = 35000.0f;
    float bumpDampingNsPerM = 3200.0f;
    float reboundDampingNsPerM = 4200.0f;
    float motionRatio = 1.0f;
    float maximumForceN = 250000.0f;
};

struct SuspensionModelInput
{
    float compressionM = 0.0f;
    float compressionVelocityMps = 0.0f;
};

struct SuspensionModelOutput
{
    float springForceN = 0.0f;
    float dampingForceN = 0.0f;
    float normalForceN = 0.0f;
};

const char* suspensionProviderId(SuspensionProviderKind provider);
bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider);

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input);

} // namespace heritage::vehicles
