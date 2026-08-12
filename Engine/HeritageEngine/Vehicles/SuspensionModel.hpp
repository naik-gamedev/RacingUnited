#pragma once

#include "VehiclePrecision.hpp"

#include <string_view>

namespace heritage::vehicles {

// Native suspension providers share this bounded force contract. Upright
// kinematics use the separate SuspensionGeometry contract; a provider ID names
// a compatible force/geometry implementation pair.
enum class SuspensionProviderKind
{
    LinearRaycastV1 = 0,
    MacPhersonStrutV1 = 1,
    TrailingArmTorsionBarV1 = 2
};

struct SuspensionModelDescription
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    VehicleScalar springPreloadN = 0.0;
    VehicleScalar springRateNPerM = 35000.0;
    VehicleScalar springProgressionNPerM2 = 0.0;
    VehicleScalar bumpDampingNsPerM = 3200.0;
    VehicleScalar bumpHighSpeedDampingNsPerM = 3200.0;
    VehicleScalar bumpDampingKneeVelocityMps = 1.0;
    VehicleScalar reboundDampingNsPerM = 4200.0;
    VehicleScalar reboundHighSpeedDampingNsPerM = 4200.0;
    VehicleScalar reboundDampingKneeVelocityMps = 1.0;
    VehicleScalar bumpStopEngagementM = 0.18;
    VehicleScalar bumpStopRateNPerM = 0.0;
    VehicleScalar bumpStopProgressionNPerM2 = 0.0;
    VehicleScalar droopStopEngagementM = 0.15;
    VehicleScalar droopStopRateNPerM = 0.0;
    VehicleScalar motionRatio = 1.0;
    VehicleScalar maximumForceN = 250000.0;
};

struct SuspensionModelInput
{
    VehicleScalar compressionM = 0.0;
    VehicleScalar compressionVelocityMps = 0.0;
    // Mechanism-specific generalized spring coordinates. They remain zero for
    // linear and MacPherson providers. A trailing-arm torsion-bar provider
    // supplies the actual arm/torsion rotation and its instantaneous leverage.
    VehicleScalar springTwistRadians = 0.0;
    VehicleScalar springAngularMotionRatioRadPerM = 0.0;
    VehicleScalar referenceSpringAngularMotionRatioRadPerM = 0.0;
};

struct SuspensionModelOutput
{
    VehicleScalar springForceN = 0.0;
    VehicleScalar dampingForceN = 0.0;
    VehicleScalar bumpStopForceN = 0.0;
    VehicleScalar droopStopForceN = 0.0;
    VehicleScalar unclampedForceN = 0.0;
    VehicleScalar normalForceN = 0.0;
    VehicleScalar damperDissipationW = 0.0;
};

const char* suspensionProviderId(SuspensionProviderKind provider);
bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider);

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input);

} // namespace heritage::vehicles
