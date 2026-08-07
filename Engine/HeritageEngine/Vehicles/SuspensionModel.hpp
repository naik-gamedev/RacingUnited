#pragma once

#include <string_view>

namespace heritage::vehicles {

// Native suspension providers share this bounded force contract. Upright
// kinematics use the separate SuspensionGeometry contract; a provider ID names
// a compatible force/geometry implementation pair.
enum class SuspensionProviderKind
{
    LinearRaycastV1 = 0
};

struct SuspensionModelDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    float springPreloadN = 0.0f;
    float springRateNPerM = 35000.0f;
    float springProgressionNPerM2 = 0.0f;
    float bumpDampingNsPerM = 3200.0f;
    float bumpHighSpeedDampingNsPerM = 3200.0f;
    float bumpDampingKneeVelocityMps = 1.0f;
    float reboundDampingNsPerM = 4200.0f;
    float reboundHighSpeedDampingNsPerM = 4200.0f;
    float reboundDampingKneeVelocityMps = 1.0f;
    float bumpStopEngagementM = 0.18f;
    float bumpStopRateNPerM = 0.0f;
    float bumpStopProgressionNPerM2 = 0.0f;
    float droopStopEngagementM = 0.15f;
    float droopStopRateNPerM = 0.0f;
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
    float bumpStopForceN = 0.0f;
    float droopStopForceN = 0.0f;
    float unclampedForceN = 0.0f;
    float normalForceN = 0.0f;
    float damperDissipationW = 0.0f;
};

const char* suspensionProviderId(SuspensionProviderKind provider);
bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider);

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input);

} // namespace heritage::vehicles
