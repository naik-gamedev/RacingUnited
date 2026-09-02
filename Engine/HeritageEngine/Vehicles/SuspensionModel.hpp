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
    TrailingArmTorsionBarV1 = 2,
    DoubleWishboneV1 = 3,
    PushrodDoubleWishboneV1 = 4,
    LiveAxleV1 = 5,
    LeafSpringLiveAxleV1 = 6,
    MotorcycleTelescopicForkV1 = 7,
    MotorcycleSwingarmLinkageV1 = 8,
    KartChassisFlexV1 = 9,
    MultiLinkV1 = 10,
    SemiTrailingArmV1 = 11,
    TwistBeamV1 = 12
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
    // SUSP09 leaf-pack hysteresis and housing wind-up parameters. The base
    // springRate/progression remain the effective leaf bending calibration.
    VehicleScalar leafInterleafFrictionN = 450.0;
    VehicleScalar leafInterleafVelocityScaleMps = 0.025;
    VehicleScalar leafInterleafViscousNsPerM = 250.0;
    VehicleScalar leafAxleWrapStiffnessNmPerRad = 16000.0;
    VehicleScalar leafAxleWrapDampingNmsPerRad = 1200.0;
    VehicleScalar leafAxleWrapInertiaKgM2 = 5.0;
    VehicleScalar leafAxleWrapJackingNPerRad = 1800.0;
    // SUSP10 rear-chain virtual-work coupling. Historical motorcycle data can
    // author/estimate the rear sprocket pitch radius without changing the
    // generic swingarm/linkage geometry solver.
    VehicleScalar motorcycleRearSprocketPitchRadiusM = 0.105;
    // SUSP13 torsion-beam structural coupling between paired semi-trailing arms.
    VehicleScalar twistBeamTorsionalStiffnessNmPerRad = 14000.0;
    VehicleScalar twistBeamTorsionalDampingNmsPerRad = 900.0;
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
    // SUSP07: pushrod/rocker geometry owns actual spring/damper shaft
    // displacement and separate instantaneous lever ratios. This avoids the
    // constant-ratio approximation k*x_wheel*MR^2 when rocker leverage changes
    // through travel.
    VehicleScalar springCompressionM = 0.0;
    VehicleScalar springMotionRatio = 1.0;
    VehicleScalar damperMotionRatio = 1.0;
    VehicleScalar axleWrapAngleRadians = 0.0;
    VehicleScalar axleWrapRateRadiansPerSecond = 0.0;
    // SUSP10 uses the previous 1 kHz longitudinal tire force to avoid a
    // suspension/contact algebraic loop. The geometry-derived chain-distance
    // ratio maps chain tension into generalized rear-suspension jacking.
    VehicleScalar previousLongitudinalTireForceN = 0.0;
    VehicleScalar wheelEffectiveRadiusM = 0.30;
    VehicleScalar motorcycleChainDistanceMotionRatio = 0.0;
    VehicleScalar twistBeamTwistRadians = 0.0;
    VehicleScalar twistBeamTwistRateRadiansPerSecond = 0.0;
    VehicleScalar twistBeamAngularMotionRatioRadPerM = 0.0;
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
    VehicleScalar leafInterleafForceN = 0.0;
    VehicleScalar leafInterleafDissipationW = 0.0;
    VehicleScalar leafAxleWrapJackingForceN = 0.0;
    VehicleScalar motorcycleChainJackingForceN = 0.0;
    VehicleScalar twistBeamCouplingForceN = 0.0;
    VehicleScalar twistBeamDissipationW = 0.0;
};

// Static ride-height calibration is deliberately separate from the transient
// damper model. At rest, dampers generate no force: supported mass, spring or
// torsion-bar preload, motion ratio and pneumatic tire deflection determine the
// settled chassis datum.
struct StaticRideHeightInput
{
    SuspensionProviderKind provider = SuspensionProviderKind::LinearRaycastV1;
    VehicleScalar supportedLoadN = 0.0;
    VehicleScalar targetBodyOffsetM = 0.0;
    VehicleScalar mountHeightFromAuthoredGroundM = 0.0;
    VehicleScalar unloadedTireRadiusM = 0.30;
    VehicleScalar suspensionRestLengthM = 0.50;
    VehicleScalar maximumCompressionM = 0.20;
    VehicleScalar maximumDroopM = 0.15;
    VehicleScalar springRateNPerM = 35000.0;
    VehicleScalar springProgressionNPerM2 = 0.0;
    VehicleScalar motionRatio = 1.0;
    VehicleScalar tireVerticalStiffnessNPerM = 220000.0;
};

struct StaticRideHeightOutput
{
    bool valid = false;
    VehicleScalar requiredSpringPreloadN = 0.0;
    VehicleScalar targetCompressionM = 0.0;
    VehicleScalar targetSuspensionLengthM = 0.0;
    VehicleScalar staticTireDeflectionM = 0.0;
    VehicleScalar reconstructedSupportForceN = 0.0;
    const char* diagnostic = "invalid input";
};

const char* suspensionProviderId(SuspensionProviderKind provider);
bool parseSuspensionProvider(
    std::string_view id,
    SuspensionProviderKind& provider);

SuspensionModelOutput evaluateSuspensionModel(
    const SuspensionModelDescription& description,
    const SuspensionModelInput& input);

StaticRideHeightOutput solveStaticRideHeight(
    const StaticRideHeightInput& input);

} // namespace heritage::vehicles
