#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"
#include "../Physics/CollisionSystem.hpp"
#include "../Physics/RigidBodySystem.hpp"
#include "TireModel.hpp"
#include "Tires/TireContactPatch.hpp"
#include "Tires/Authoring/TirePartResolver.hpp"
#include "SuspensionGeometry.hpp"
#include "SuspensionModel.hpp"
#include "Suspension/Common/SuspensionAntiRollBar.hpp"
#include "Dynamics/ChassisFlex/ChassisTorsionalCompliance.hpp"
#include "Wheels/Fitment/WheelFitment.hpp"
#include "Wheels/Fitment/HubReferenceGeometry.hpp"
#include "Wheels/Fitment/ScrubRadiusGeometry.hpp"
#include "UnsprungMassModel.hpp"
#include "VehicleDynamicsLab.hpp"
#include "VehiclePrecision.hpp"

namespace heritage::physics {
class SurfaceWorld;
}

namespace heritage::vehicles {

using VehicleHandle = std::uint64_t;
inline constexpr VehicleHandle InvalidVehicle = 0;

enum class DifferentialMode
{
    Open = 0,
    LimitedSlip = 1,
    Locked = 2
};

enum class TireSurface
{
    DryAsphalt = 0,
    WetAsphalt = 1,
    Gravel = 2,
    Dirt = 3,
    Snow = 4,
    Ice = 5
};

struct VehicleDescription
{
    heritage::physics::BodyHandle chassisBody = heritage::physics::InvalidBody;
    float highRateHertz = 1000.0f;
    float maximumDriveForce = 7000.0f;
    float maximumBrakeForce = 12000.0f;
    float maximumSteerAngleDegrees = 38.0f;
    float ackermannPercent = 1.0f;
    float steeringRateDegreesPerSecond = 260.0f;
    float steeringReturnRateDegreesPerSecond = 360.0f;
    float highSpeedSteeringRateFactor = 0.35f;
    float highSpeedReferenceMps = 40.0f;
    float tireFriction = 1.15f;
    float lateralStiffness = 11000.0f;
    float rollingResistance = 90.0f;
};



struct DriverAidDescription
{
    bool antiLockBrakesEnabled = true;
    bool tractionControlEnabled = true;
    float antiLockTargetSlip = 0.16f;
    float tractionControlTargetSlip = 0.12f;
    float minimumActivationSpeed = 2.5f;
    float modulationRate = 18.0f;
    float maximumHandbrakeTorque = 3500.0f;
};

struct DriverAidState
{
    bool antiLockBrakesEnabled = true;
    bool tractionControlEnabled = true;
    int antiLockActiveWheelCount = 0;
    int tractionControlActiveWheelCount = 0;
    float antiLockTargetSlip = 0.16f;
    float tractionControlTargetSlip = 0.12f;
    float minimumActivationSpeed = 2.5f;
    float handbrakeInput = 0.0f;
};

struct PowertrainDescription
{
    float idleRpm = 900.0f;
    float redlineRpm = 7000.0f;
    float maximumTorque = 250.0f;
    float engineBrakingTorque = 70.0f;
    float engineResponse = 8.0f;
    float finalDriveRatio = 3.90f;
    float drivetrainEfficiency = 0.88f;
    float shiftDurationSeconds = 0.22f;
    float clutchEngagementRate = 5.0f;
    float reverseGearRatio = -3.20f;
    std::vector<float> forwardGearRatios{
        3.40f, 2.10f, 1.45f, 1.12f, 0.89f, 0.74f
    };
    DifferentialMode differentialMode = DifferentialMode::LimitedSlip;
    float differentialBiasRatio = 2.25f;
};

struct WheelDescription
{
    heritage::math::Vec3 localMount{ 0.0f, 0.8f, 0.0f };
    heritage::math::Vec3 localSuspensionDirection{ 0.0f, -1.0f, 0.0f };
    float radius = 0.35f;
    float restLength = 0.50f;
    float maximumCompression = 0.18f;
    float maximumDroop = 0.15f;
    float springPreload = 0.0f;
    float springRate = 35000.0f;
    float springProgression = 0.0f;
    float bumpDamping = 3200.0f;
    float bumpHighSpeedDamping = 3200.0f;
    float bumpDampingKneeVelocity = 1.0f;
    float reboundDamping = 4200.0f;
    float reboundHighSpeedDamping = 4200.0f;
    float reboundDampingKneeVelocity = 1.0f;
    float bumpStopEngagement = 0.18f;
    float bumpStopRate = 0.0f;
    float bumpStopProgression = 0.0f;
    float droopStopEngagement = 0.15f;
    float droopStopRate = 0.0f;
    SuspensionProviderKind suspensionProvider =
        SuspensionProviderKind::LinearRaycastV1;
    MacPhersonHardpoints macPhersonHardpoints;
    TrailingArmHardpoints trailingArmHardpoints;
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float camberGainDegreesPerM = 0.0f;
    float camberProgressionDegreesPerM2 = 0.0f;
    float staticToeDegrees = 0.0f;
    bool casterOverrideEnabled = false;
    float staticCasterDegrees = 0.0f;
    float toeGainDegreesPerM = 0.0f;
    float toeProgressionDegreesPerM2 = 0.0f;
    float suspensionMotionRatio = 1.0f;
    float maximumSuspensionForce = 250000.0f;
    float effectiveUnsprungMass = 0.0f;
    float tireRadialStiffness = 220000.0f;
    float tireRadialDamping = 1800.0f;
    float maximumTireDeflection = 0.08f;
    float maximumTireNormalForce = 250000.0f;
    float driveFactor = 0.0f;
    float steerFactor = 0.0f;
    float brakeFactor = 1.0f;
    float handbrakeFactor = 0.0f;
    WheelFitmentDescription fitment;
};

struct SteeringState
{
    float input = 0.0f;
    float targetCenterAngleDegrees = 0.0f;
    float currentCenterAngleDegrees = 0.0f;
    float innerWheelAngleDegrees = 0.0f;
    float outerWheelAngleDegrees = 0.0f;
    float detectedWheelbase = 0.0f;
    float detectedSteerTrack = 0.0f;
    float currentRateFactor = 1.0f;
};

struct DrivetrainState
{
    int currentGear = 1;
    int requestedGear = 1;
    bool shifting = false;
    float shiftTimeRemaining = 0.0f;
    float engineRpm = 900.0f;
    float engineTorque = 0.0f;
    float clutchEngagement = 0.0f;
    float clutchSlipRpm = 0.0f;
    float wheelCoupledRpm = 0.0f;
    float selectedGearRatio = 3.40f;
    float finalDriveRatio = 3.90f;
    float outputTorque = 0.0f;
    float drivenWheelSpeedDifferenceRpm = 0.0f;
    DifferentialMode differentialMode = DifferentialMode::LimitedSlip;
};

struct VehicleRestState
{
    bool resting = false;
    bool candidate = false;
    bool requiresBrake = false;
    float quietTimeSeconds = 0.0f;
    float requiredHoldForce = 0.0f;
    float availableBrakeHoldForce = 0.0f;
};

// Authoritative result of the latest wheel support query. These values make a
// lost road contact distinguishable from suspension travel limits, an unloaded
// tire, missing world geometry, scene boundaries and tunnelling behind the ray
// origin without changing the force solver itself.
enum class WheelContactStatus
{
    Supported = 0,
    SuspensionBottomed = 1,
    RoadDetectedNoLoad = 2,
    SurfaceBehindRayOrigin = 3,
    OutsideStaticSceneBounds = 4,
    NoWorldGeometry = 5,
    NoRayCandidates = 6,
    RayCandidatesMissed = 7,
    BeyondSuspensionReach = 8,
    NoSupportHit = 9
};

struct WheelFitmentGeometryState
{
    bool hubReferenceValid = false;
    heritage::math::Vec3 referenceWheelCenterLocal{};
    heritage::math::Vec3 referenceHubFaceCenterLocal{};
    heritage::math::Vec3 installedMountFaceCenterLocal{};
    heritage::math::Vec3 installedWheelCenterLocal{};
    heritage::math::Vec3 installedInnerTirePlaneLocal{};
    heritage::math::Vec3 installedOuterTirePlaneLocal{};
    VehicleScalar inboardTireExtensionFromReferenceHubM = 0.0;
    VehicleScalar outboardTireExtensionFromReferenceHubM = 0.0;

    bool steeringGroundGeometryValid = false;
    heritage::math::Vec3 worldSteeringAxisPoint{};
    heritage::math::Vec3 steeringAxisGroundPointWorld{};
    VehicleScalar signedScrubRadiusM = 0.0;
    VehicleScalar scrubRadiusMagnitudeM = 0.0;
    VehicleScalar mechanicalTrailM = 0.0;
};

struct WheelState
{
    bool grounded = false;
    WheelContactStatus contactStatus = WheelContactStatus::NoSupportHit;
    std::uint64_t contactLossTransitionCount = 0;
    std::size_t rayCandidateCount = 0;
    std::size_t rayExactTestCount = 0;
    std::size_t staticTriangleCandidateCount = 0;
    bool staticSceneLoaded = false;
    bool originInsideStaticSceneBounds = false;
    bool rayBoundsOverlapStaticScene = false;
    bool selectedHitWasStaticTriangle = false;
    VehicleScalar rawSupportDistance = 0.0f;
    bool suspensionBottomed = false;
    VehicleScalar bottomOutPenetration = 0.0f;
    VehicleScalar suspensionLength = 0.0f;
    VehicleScalar compression = 0.0f;
    VehicleScalar compressionVelocity = 0.0f;
    VehicleScalar suspensionSpringForce = 0.0f;
    VehicleScalar suspensionDampingForce = 0.0f;
    VehicleScalar suspensionBumpStopForce = 0.0f;
    VehicleScalar suspensionDroopStopForce = 0.0f;
    VehicleScalar suspensionUnclampedForce = 0.0f;
    VehicleScalar antiRollBarForce = 0.0f;
    VehicleScalar damperDissipationWatts = 0.0f;
    VehicleScalar unsprungVelocity = 0.0f;
    VehicleScalar tireDeflection = 0.0f;
    VehicleScalar tireDeflectionVelocity = 0.0f;
    VehicleScalar tireRadialDissipationWatts = 0.0f;
    // TIRE04 quasi-static tire geometry. Loaded radius follows radial
    // deflection; effective radius is the rolling/slip lever arm. The finite
    // footprint becomes the input boundary for later SWIFT-like enveloping.
    VehicleScalar tireFreeRollingRadius = 0.0f;
    VehicleScalar tireLoadedRadius = 0.0f;
    VehicleScalar tireEffectiveRollingRadius = 0.0f;
    VehicleScalar tireContactPatchLength = 0.0f;
    VehicleScalar tireContactPatchWidth = 0.0f;
    VehicleScalar tireContactPatchArea = 0.0f;
    // TIRE05 SWIFT-like structural/enveloping telemetry. The envelope offset
    // is the road-height correction generated by the tandem-cam filter; ring
    // offsets/velocities are belt motion relative to the rim.
    VehicleScalar tireEnvelopeRoadOffset = 0.0f;
    VehicleScalar tireEnvelopeSlopeDegrees = 0.0f;
    VehicleScalar tireEnvelopeCrossSlopeDegrees = 0.0f;
    VehicleScalar tireEnvelopeValidSamples = 0.0f;
    VehicleScalar tireFootprintTotalSamples = 0.0f;
    VehicleScalar tireFootprintSupportedFraction = 0.0f;
    VehicleScalar tireFootprintRoughnessRange = 0.0f;
    VehicleScalar tireFootprintSurfaceFriction = 1.0f;
    VehicleScalar tireFootprintSurfaceSpread = 0.0f;
    bool tireFootprintRefined = false;
    VehicleScalar tireRingRadialOffset = 0.0f;
    VehicleScalar tireRingRadialVelocity = 0.0f;
    VehicleScalar tireRingLongitudinalOffset = 0.0f;
    VehicleScalar tireRingLongitudinalVelocity = 0.0f;
    VehicleScalar tireRingLateralOffset = 0.0f;
    VehicleScalar tireRingLateralVelocity = 0.0f;
    VehicleScalar tireRingYawDegrees = 0.0f;
    VehicleScalar tireRingYawRateDegreesPerSecond = 0.0f;
    VehicleScalar tireRingWindupDegrees = 0.0f;
    VehicleScalar tireRingWindupRateDegreesPerSecond = 0.0f;
    // TIRE07 lumped thermal/pressure telemetry. Temperatures are stateful at
    // the 1000 Hz tire rate; inflation pressure is gauge pressure.
    VehicleScalar tireTreadTemperatureC = 20.0f;
    VehicleScalar tireCarcassTemperatureC = 20.0f;
    VehicleScalar tireGasTemperatureC = 20.0f;
    VehicleScalar tireInflationPressurePa = 220000.0f;
    tires::TireFailureStage tireFailureStage = tires::TireFailureStage::Healthy;
    std::uint64_t tireFailureEventSerial = 0;
    VehicleScalar tireContainedGasMassRatio = 1.0f;
    VehicleScalar tirePressurizedGasFraction = 1.0f;
    VehicleScalar tirePunctureAreaMm2 = 0.0f;
    VehicleScalar tireEffectiveLeakAreaMm2 = 0.0f;
    VehicleScalar tireLeakMassFlowGramsPerSecond = 0.0f;
    VehicleScalar tireStructuralIntegrity = 1.0f;
    VehicleScalar tireTreadAttachment = 1.0f;
    VehicleScalar tireRimContactFraction = 0.0f;
    VehicleScalar tireFailureEventElapsedSeconds = 0.0f;
    VehicleScalar tireThermalFrictionScale = 1.0f;
    VehicleScalar tireThermalStiffnessScale = 1.0f;
    VehicleScalar tireSlipDissipationWatts = 0.0f;
    VehicleScalar tireThermalLossDissipationWatts = 0.0f;
    VehicleScalar tireRoadHeatFlowWatts = 0.0f;
    VehicleScalar tireAirHeatFlowWatts = 0.0f;
    // TIRE08 spatial tread telemetry. The 48-cell field stays inside the tire
    // provider; WheelState exposes cheap aggregates for diagnostics/UI.
    VehicleScalar tireTreadInsideSurfaceTemperatureC = 20.0f;
    VehicleScalar tireTreadCenterSurfaceTemperatureC = 20.0f;
    VehicleScalar tireTreadOutsideSurfaceTemperatureC = 20.0f;
    VehicleScalar tireTreadHottestSurfaceTemperatureC = 20.0f;
    VehicleScalar tireTreadInsideDepthMm = 7.0f;
    VehicleScalar tireTreadCenterDepthMm = 7.0f;
    VehicleScalar tireTreadOutsideDepthMm = 7.0f;
    VehicleScalar tireTreadMinimumDepthMm = 7.0f;
    VehicleScalar tireTreadWearFraction = 0.0f;
    VehicleScalar tireFlatSpotDepthMm = 0.0f;
    VehicleScalar tireFlatSpotSector = 0.0f;
    // TIRE10 radius coupling. Average loss represents global tread-radius
    // reduction; contact loss follows the current rotating sector/band blend;
    // variation is the signed local departure that produces flat-spot thump.
    VehicleScalar tireAverageTreadRadiusLossMm = 0.0f;
    VehicleScalar tireContactTreadRadiusLossMm = 0.0f;
    VehicleScalar tireContactRadiusVariationMm = 0.0f;
    VehicleScalar tireSpatialFrictionScale = 1.0f;
    VehicleScalar tireTreadContactSector = 0.0f;
    VehicleScalar tireTreadHottestSector = 0.0f;
    // TIRE11 local 16x3 contamination/pickup aggregates. The full channel
    // history remains in TireWearState; WheelState exposes only cheap current
    // contact/readback values.
    VehicleScalar tireContaminationFrictionScale = 1.0f;
    VehicleScalar tireContaminationTotal = 0.0f;
    VehicleScalar tireContaminationAverage = 0.0f;
    VehicleScalar tireOrganicContamination = 0.0f;
    VehicleScalar tireMineralContamination = 0.0f;
    VehicleScalar tireGravelFinesContamination = 0.0f;
    VehicleScalar tireRubberPickupContamination = 0.0f;
    VehicleScalar tireMudFilmContamination = 0.0f;
    VehicleScalar tireContaminationCleaningRate = 0.0f;
    // TIRE12 wet hard-surface / hydroplaning telemetry. Water depth comes
    // from the spatial footprint wetness bridge; retained water remains in
    // the 48 material-fixed tread cells.
    VehicleScalar tireRoadWaterDepthMm = 0.0f;
    VehicleScalar tireRetainedWaterDepthMm = 0.0f;
    VehicleScalar tireDrainageDemandRatio = 0.0f;
    VehicleScalar tireWaterWedgeFraction = 0.0f;
    VehicleScalar tireHydroplaningFraction = 0.0f;
    VehicleScalar tirePavementContactFraction = 1.0f;
    VehicleScalar tireHydrodynamicLiftN = 0.0f;
    VehicleScalar tireHydrodynamicDragN = 0.0f;
    VehicleScalar tireWetFrictionScale = 1.0f;
    VehicleScalar tireClassicalHydroplaningSpeedKph = 0.0f;
    // TIRE13 compacted-snow / hard-ice telemetry. Snow/ice remain one MF6.2
    // force evaluation with a dedicated winter-surface response around it.
    VehicleScalar tireWinterSurfaceFraction = 0.0f;
    VehicleScalar tireSnowSurfaceFraction = 0.0f;
    VehicleScalar tireIceSurfaceFraction = 0.0f;
    VehicleScalar tireWinterFrictionScale = 1.0f;
    VehicleScalar tireWinterStiffnessScale = 1.0f;
    VehicleScalar tirePackedSnowFraction = 0.0f;
    VehicleScalar tireIceMeltFilmMicrometers = 0.0f;
    VehicleScalar tireStudFrictionContribution = 0.0f;
    VehicleScalar tireSnowInterlockContribution = 0.0f;
    VehicleScalar tireWinterSurfaceTemperatureC = -5.0f;
    // TIRE14 shallow gravel / hard-dirt hybrid telemetry. These values describe
    // the loose layer around the one MF6.2 tire solve; fully deformable terrain
    // and persistent rut/compaction memory remain TIRE15 SurfaceField work.
    VehicleScalar tireGranularSurfaceFraction = 0.0f;
    VehicleScalar tireGranularSinkageMm = 0.0f;
    VehicleScalar tireGranularContactPressureKPa = 0.0f;
    VehicleScalar tireGranularTreadEffectiveness = 0.0f;
    VehicleScalar tireGranularShearCapacityN = 0.0f;
    VehicleScalar tireGranularLongitudinalShearN = 0.0f;
    VehicleScalar tireGranularLateralShearN = 0.0f;
    VehicleScalar tireGranularBulldozingN = 0.0f;
    VehicleScalar tireGranularPlowingDragN = 0.0f;
    VehicleScalar tireGranularCompactionPowerW = 0.0f;
    VehicleScalar tireGranularFrictionScale = 1.0f;
    // TIRE15 persistent deformable-terrain / SurfaceField telemetry.
    VehicleScalar tireTerrainSurfaceFraction = 0.0f;
    VehicleScalar tireTerrainSinkageMm = 0.0f;
    VehicleScalar tireTerrainRutDepthMm = 0.0f;
    VehicleScalar tireTerrainCompaction = 0.0f;
    VehicleScalar tireTerrainMoisture = 0.0f;
    VehicleScalar tireTerrainLooseDepthMm = 0.0f;
    VehicleScalar tireTerrainShearCapacityN = 0.0f;
    VehicleScalar tireTerrainLongitudinalShearN = 0.0f;
    VehicleScalar tireTerrainLateralShearN = 0.0f;
    VehicleScalar tireTerrainBulldozingN = 0.0f;
    VehicleScalar tireTerrainPlowingDragN = 0.0f;
    VehicleScalar tireTerrainMfFrictionScale = 1.0f;
    VehicleScalar tireTerrainPassCount = 0.0f;
    // TIRE15C world-owned dynamic track-rubber telemetry. Deposited rubber is
    // the rubbered racing-line state; loose rubber is the local marble/debris
    // concentration sampled before this contact's force evaluation.
    VehicleScalar tireTrackDepositedRubber = 0.0f;
    VehicleScalar tireTrackLooseRubber = 0.0f;
    VehicleScalar tireTrackMarbleMaturity = 0.0f;
    VehicleScalar tireTrackRubberFrictionScale = 1.0f;
    VehicleScalar tireTrackRubberPassCount = 0.0f;
    VehicleScalar normalForce = 0.0f;
    VehicleScalar longitudinalForce = 0.0f;
    VehicleScalar lateralForce = 0.0f;
    VehicleScalar steerAngleDegrees = 0.0f;
    VehicleScalar camberAngleDegrees = 0.0f;
    VehicleScalar toeAngleDegrees = 0.0f;
    bool suspensionKinematicsValid = true;
    bool suspensionTravelClamped = false;
    VehicleScalar bumpSteerDegrees = 0.0f;
    VehicleScalar strutCompression = 0.0f;
    VehicleScalar instantaneousMotionRatio = 1.0f;
    heritage::math::Vec3 localUprightRotationDegrees{};
    heritage::math::Vec3 worldSteeringAxis{ 0.0f, 1.0f, 0.0f };
    bool steeringAxisPointValid = false;
    heritage::math::Vec3 worldSteeringAxisPoint{};
    bool steeringGroundGeometryValid = false;
    heritage::math::Vec3 steeringAxisGroundPointWorld{};
    VehicleScalar signedScrubRadiusM = 0.0;
    VehicleScalar scrubRadiusMagnitudeM = 0.0;
    VehicleScalar mechanicalTrailM = 0.0;
    heritage::math::Vec3 worldWheelForward{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 worldWheelRight{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 worldWheelUp{ 0.0f, 1.0f, 0.0f };
    VehicleScalar wheelAngularVelocity = 0.0f;
    VehicleScalar appliedDriveTorque = 0.0f;
    VehicleScalar appliedBrakeTorque = 0.0f;
    VehicleScalar serviceBrakeTorque = 0.0f;
    VehicleScalar handbrakeTorque = 0.0f;
    VehicleScalar antiLockModulation = 1.0f;
    VehicleScalar tractionControlModulation = 1.0f;
    bool antiLockActive = false;
    bool tractionControlActive = false;
    VehicleScalar wheelRotationDegrees = 0.0f;
    VehicleScalar longitudinalSpeed = 0.0f;
    VehicleScalar lateralSpeed = 0.0f;
    VehicleScalar slipRatio = 0.0f;
    VehicleScalar slipAngleDegrees = 0.0f;
    VehicleScalar relaxedSlipRatio = 0.0f;
    VehicleScalar relaxedSlipAngleDegrees = 0.0f;
    VehicleScalar turnSlipPerM = 0.0f;
    VehicleScalar normalizedTurnSlip = 0.0f;
    VehicleScalar contactPatchTwistDegrees = 0.0f;
    VehicleScalar parkingTurnMoment = 0.0f;
    VehicleScalar turnSlipMoment = 0.0f;
    VehicleScalar turnSlipLongitudinalReduction = 1.0f;
    VehicleScalar turnSlipLateralReduction = 1.0f;
    VehicleScalar turnSlipCorneringReduction = 1.0f;
    VehicleScalar turnSlipTrailReduction = 1.0f;
    VehicleScalar effectiveFriction = 0.0f;
    VehicleScalar gripUtilization = 0.0f;
    VehicleScalar pureLongitudinalForce = 0.0f;
    VehicleScalar pureLateralForce = 0.0f;
    VehicleScalar combinedSlipScale = 1.0f;
    VehicleScalar pneumaticTrail = 0.0f;
    VehicleScalar aligningTorque = 0.0f;
    VehicleScalar overturningMoment = 0.0f;
    VehicleScalar rollingResistanceMoment = 0.0f;
    VehicleScalar residualAligningTorque = 0.0f;
    VehicleScalar longitudinalSlipStiffness = 0.0f;
    VehicleScalar corneringStiffness = 0.0f;
    VehicleScalar camberStiffness = 0.0f;
    bool motorcycleContourValid = false;
    VehicleScalar motorcycleContactLateralOffset = 0.0f;
    VehicleScalar motorcycleCenterToRoad = 0.0f;
    heritage::physics::ColliderHandle contactCollider =
        heritage::physics::InvalidCollider;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    VehicleScalar surfaceWetness = 0.0f;
    VehicleScalar surfaceTemperatureC = 20.0f;
    heritage::math::Vec3 worldCenter{};
    heritage::math::Vec3 contactPoint{};
    heritage::math::Vec3 contactNormal{ 0.0f, 1.0f, 0.0f };
};

const char* wheelContactStatusName(WheelContactStatus value);

// Step 29H: generation-checked arbitrary-wheel vehicle foundation with
// Ackermann steering, powertrain, per-wheel advanced transient road-tire data,
// high-rate driver aids, and per-wheel physical-surface detection from suspension
// contact queries. Service and parking brakes remain configurable per wheel so
// cars, motorcycles, ATVs and multi-axle vehicles do not inherit a hard-coded
// four-wheel layout.
class VehicleSystem
{
public:
    void clear();
    void resetClock();

    VehicleHandle create(
        const VehicleDescription& description,
        const heritage::physics::RigidBodySystem& bodies);
    bool destroy(VehicleHandle handle);
    bool exists(VehicleHandle handle) const;
    std::size_t count() const { return m_aliveCount; }
    void destroyForBody(heritage::physics::BodyHandle body);
    void removeInvalidBodies(const heritage::physics::RigidBodySystem& bodies);

    bool addWheel(VehicleHandle handle, const WheelDescription& description);
    std::size_t wheelCount(VehicleHandle handle) const;
    bool wheelState(VehicleHandle handle, std::size_t wheelIndex, WheelState& value) const;
    // TIRE26/VIS18: render-time contact probing needs the authored physical
    // wheel mount/width/radius without reaching into VehicleSystem internals.
    // This is read-only configuration state; physics remains authoritative.
    bool wheelDescription(
        VehicleHandle handle,
        std::size_t wheelIndex,
        WheelDescription& value) const;
    bool setWheelSuspensionModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const SuspensionModelDescription& value);
    bool wheelSuspensionModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        SuspensionModelDescription& value) const;
    bool setWheelSuspensionGeometry(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const SuspensionGeometryDescription& value);
    bool wheelSuspensionGeometry(
        VehicleHandle handle,
        std::size_t wheelIndex,
        SuspensionGeometryDescription& value) const;
    bool setWheelFitment(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const WheelFitmentDescription& value);
    bool wheelFitment(
        VehicleHandle handle,
        std::size_t wheelIndex,
        WheelFitmentDescription& value,
        WheelFitmentResolved& resolved) const;
    bool wheelFitmentGeometry(
        VehicleHandle handle,
        std::size_t wheelIndex,
        WheelFitmentGeometryState& value) const;
    bool setWheelAlignment(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const WheelAlignmentSetup& value);
    bool wheelAlignment(
        VehicleHandle handle,
        std::size_t wheelIndex,
        WheelAlignmentSetup& value) const;
    bool setAntiRollBar(
        VehicleHandle handle,
        std::size_t antiRollBarIndex,
        const SuspensionAntiRollBarDescription& value);
    bool antiRollBar(
        VehicleHandle handle,
        std::size_t antiRollBarIndex,
        SuspensionAntiRollBarDescription& description,
        SuspensionAntiRollBarOutput& state) const;
    std::size_t antiRollBarCount(VehicleHandle handle) const;
    bool setChassisTorsionalCompliance(
        VehicleHandle handle,
        const ChassisTorsionalComplianceDescription& value);
    bool chassisTorsionalCompliance(
        VehicleHandle handle,
        ChassisTorsionalComplianceDescription& description,
        ChassisTorsionalComplianceState& state) const;
    bool setWheelUnsprungMassModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const UnsprungMassDescription& value);
    bool wheelUnsprungMassModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        UnsprungMassDescription& value) const;

    bool setInputs(
        VehicleHandle handle,
        float throttle,
        float brake,
        float steering,
        float handbrake = 0.0f);
    bool setTuning(
        VehicleHandle handle,
        float maximumDriveForce,
        float maximumBrakeForce,
        float maximumSteerAngleDegrees,
        float tireFriction,
        float lateralStiffness,
        float rollingResistance);
    bool setHighRateHertz(VehicleHandle handle, float hertz);
    bool setSteeringGeometry(
        VehicleHandle handle,
        float ackermannPercent,
        float steeringRateDegreesPerSecond,
        float steeringReturnRateDegreesPerSecond,
        float highSpeedSteeringRateFactor,
        float highSpeedReferenceMps);
    bool steeringState(VehicleHandle handle, SteeringState& value) const;

    bool setWheelBrakeFactors(
        VehicleHandle handle,
        std::size_t wheelIndex,
        float serviceBrakeFactor,
        float handbrakeFactor);
    bool setDriverAids(
        VehicleHandle handle,
        bool antiLockBrakesEnabled,
        bool tractionControlEnabled,
        float antiLockTargetSlip,
        float tractionControlTargetSlip,
        float minimumActivationSpeed,
        float modulationRate,
        float maximumHandbrakeTorque);
    bool driverAidState(VehicleHandle handle, DriverAidState& value) const;

    bool setPowertrain(
        VehicleHandle handle,
        float idleRpm,
        float redlineRpm,
        float maximumTorque,
        float engineBrakingTorque,
        float finalDriveRatio,
        float drivetrainEfficiency,
        float shiftDurationSeconds,
        float clutchEngagementRate);
    bool setGearRatios(
        VehicleHandle handle,
        float reverseGearRatio,
        const std::vector<float>& forwardGearRatios);
    bool setDifferential(
        VehicleHandle handle,
        DifferentialMode mode,
        float biasRatio);
    bool setGear(VehicleHandle handle, int gear);
    bool shiftUp(VehicleHandle handle);
    bool shiftDown(VehicleHandle handle);
    bool drivetrainState(VehicleHandle handle, DrivetrainState& value) const;
    std::size_t forwardGearCount(VehicleHandle handle) const;

    bool setTireModel(
        VehicleHandle handle,
        VehicleScalar nominalLoad,
        VehicleScalar peakFriction,
        VehicleScalar longitudinalStiffness,
        VehicleScalar corneringStiffness,
        VehicleScalar loadSensitivity,
        VehicleScalar longitudinalRelaxationLength,
        VehicleScalar lateralRelaxationLength,
        VehicleScalar wheelInertia,
        VehicleScalar pneumaticTrail,
        VehicleScalar stiffnessLoadExponent,
        VehicleScalar longitudinalShapeFactor,
        VehicleScalar lateralShapeFactor,
        VehicleScalar longitudinalCurvatureFactor,
        VehicleScalar lateralCurvatureFactor,
        VehicleScalar combinedSlipExponent,
        VehicleScalar pneumaticTrailFalloff);
    bool setWheelTireModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        VehicleScalar nominalLoad,
        VehicleScalar peakFriction,
        VehicleScalar longitudinalStiffness,
        VehicleScalar corneringStiffness,
        VehicleScalar loadSensitivity,
        VehicleScalar longitudinalRelaxationLength,
        VehicleScalar lateralRelaxationLength,
        VehicleScalar wheelInertia,
        VehicleScalar pneumaticTrail,
        VehicleScalar stiffnessLoadExponent,
        VehicleScalar longitudinalShapeFactor,
        VehicleScalar lateralShapeFactor,
        VehicleScalar longitudinalCurvatureFactor,
        VehicleScalar lateralCurvatureFactor,
        VehicleScalar combinedSlipExponent,
        VehicleScalar pneumaticTrailFalloff);
    bool setWheelTireProvider(
        VehicleHandle handle,
        std::size_t wheelIndex,
        TireProviderKind provider);
    bool setWheelTireDescription(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const TireModelDescription& description);
    bool loadWheelTirePropertyFile(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const std::filesystem::path& path,
        const std::string& provenance = {},
        VehicleScalar confidence = 0.0);
    bool assignWheelTirePart(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const tires::TirePartDefinition& definition,
        const std::filesystem::path& propertyRoot = {},
        const tires::TirePartFitment& fitment = {});
    // TIRE17C1 development/fitment pressure controls. Pressure is gauge Pa.
    // Changing cold pressure preserves thermal temperatures; the live pressure
    // continues to follow the ideal-gas model from the new cold reference.
    bool setWheelTireColdInflationPressure(
        VehicleHandle handle, std::size_t wheelIndex, VehicleScalar pressurePa);
    bool setTireColdInflationPressure(
        VehicleHandle handle, VehicleScalar pressurePa);
    bool triggerWheelTireFailure(
        VehicleHandle handle, std::size_t wheelIndex,
        tires::TireFailureStage stage);
    bool triggerTireFailure(
        VehicleHandle handle, tires::TireFailureStage stage);
    bool tireColdInflationPressureRange(
        VehicleHandle handle, VehicleScalar& minimumPa,
        VehicleScalar& maximumPa, VehicleScalar& representativePressurePa) const;
    bool wheelTirePartAssignment(
        VehicleHandle handle,
        std::size_t wheelIndex,
        tires::TirePartAssignmentInfo& value) const;
    bool wheelTireModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        TireModelDescription& value) const;
    bool resetTirePhysicalState(VehicleHandle handle);
    bool setSurfacePreset(VehicleHandle handle, TireSurface surface);
    TireSurface surfacePreset(VehicleHandle handle) const;

    float speed(VehicleHandle handle) const;
    std::size_t groundedWheelCount(VehicleHandle handle) const;
    int lastHighRateStepCount(VehicleHandle handle) const;
    std::uint64_t totalHighRateStepCount(VehicleHandle handle) const;
    float highRateHertz(VehicleHandle handle) const;
    heritage::physics::BodyHandle chassisBody(VehicleHandle handle) const;
    bool restState(VehicleHandle handle, VehicleRestState& value) const;

    // Opt-in native high-rate telemetry. Only explicitly recorded vehicles
    // allocate sample storage, keeping full race fields free of lab overhead.
    bool startDynamicsLabCapture(
        VehicleHandle handle,
        float maximumDurationSeconds,
        float captureHertz);
    bool stopDynamicsLabCapture(VehicleHandle handle);
    bool clearDynamicsLabCapture(VehicleHandle handle);
    bool dynamicsLabSummary(
        VehicleHandle handle,
        DynamicsLabSummary& value) const;
    bool dynamicsLabMetricSeries(
        VehicleHandle handle,
        DynamicsLabMetric metric,
        std::size_t wheelIndex,
        std::size_t maximumPoints,
        std::vector<float>& values) const;
    bool exportDynamicsLabCsv(
        VehicleHandle handle,
        const std::filesystem::path& path);

    void simulate(
        heritage::physics::RigidBodySystem& bodies,
        const heritage::physics::CollisionSystem& collisions,
        heritage::physics::SurfaceWorld& surfaces,
        float worldDeltaTime,
        const heritage::math::Vec3& gravity = { 0.0f, -9.80665f, 0.0f });

    const std::string& lastError() const { return m_lastError; }

private:
    struct WheelRecord
    {
        WheelDescription description;
        WheelState state;
        TireModelDescription tireModel;
        tires::TirePartAssignmentInfo tirePartAssignment;
        VehicleScalar previousSuspensionLength = 0.0;
        // TIRE16 stable wheel-owned stream identity for continuous surface marks.
        std::uint64_t tireMarkStreamId = 0;
        VehicleScalar previousSteerAngleDegrees = 0.0;
        bool steerRateInitialized = false;
        tires::TireContactPatchState contactPatchState;
        tires::TireRigidRingState rigidRingState;
        tires::TireThermalState thermalState;
        tires::TireFailureState failureState;
        tires::TireWearState wearState;
        VehicleScalar roadEnvelopeQueryAccumulatorSeconds = 0.0;
        bool roadEnvelopeInitialized = false;
        VehicleScalar cachedRoadEnvelopeOffsetM = 0.0;
        VehicleScalar cachedRoadEnvelopeSlopeRadians = 0.0;
        VehicleScalar cachedRoadEnvelopeCrossSlopeRadians = 0.0;
        VehicleScalar cachedRoadEnvelopeRoughnessRangeM = 0.0;
        VehicleScalar cachedRoadEnvelopeSupportedFraction = 0.0;
        std::size_t cachedRoadEnvelopeValidSamples = 0;
        std::size_t cachedRoadEnvelopeTotalSamples = 0;
        bool cachedRoadEnvelopeComplex = false;
        bool cachedFootprintRefined = false;
        bool cachedFootprintSurfaceValid = false;
        VehicleScalar cachedFootprintFrictionMultiplier = 1.0;
        VehicleScalar cachedFootprintStiffnessMultiplier = 1.0;
        VehicleScalar cachedFootprintRollingResistanceMultiplier = 1.0;
        VehicleScalar cachedFootprintRelaxationMultiplier = 1.0;
        // TIRE12 base surface blend: hard-surface samples are restored to
        // their dry coefficients before the spatial water provider applies
        // thin-film/hydroplaning physics; non-hard materials keep legacy wet
        // behavior until their dedicated providers arrive.
        VehicleScalar cachedFootprintWetBaseFrictionMultiplier = 1.0;
        VehicleScalar cachedFootprintWetBaseStiffnessMultiplier = 1.0;
        VehicleScalar cachedFootprintWetBaseRollingResistanceMultiplier = 1.0;
        VehicleScalar cachedFootprintWetBaseRelaxationMultiplier = 1.0;
        // TIRE13 combined dedicated-provider base: hard wet surfaces are dry
        // before TIRE12, while snow/ice are neutral before TIRE13.
        VehicleScalar cachedFootprintProviderBaseFrictionMultiplier = 1.0;
        VehicleScalar cachedFootprintProviderBaseStiffnessMultiplier = 1.0;
        VehicleScalar cachedFootprintProviderBaseRollingResistanceMultiplier = 1.0;
        VehicleScalar cachedFootprintProviderBaseRelaxationMultiplier = 1.0;
        VehicleScalar cachedFootprintFrictionSpread = 0.0;
        bool cachedFootprintMaterialBlendValid = false;
        VehicleScalar cachedFootprintGrassFraction = 0.0;
        VehicleScalar cachedFootprintDirtFraction = 0.0;
        VehicleScalar cachedFootprintGravelFraction = 0.0;
        VehicleScalar cachedFootprintSnowFraction = 0.0;
        VehicleScalar cachedFootprintIceFraction = 0.0;
        VehicleScalar cachedFootprintMudFraction = 0.0;
        VehicleScalar cachedFootprintSandFraction = 0.0;
        VehicleScalar cachedFootprintSoftSoilFraction = 0.0;
        VehicleScalar cachedFootprintDeepSnowFraction = 0.0;
        VehicleScalar cachedFootprintCleanHardFraction = 0.0;
        VehicleScalar cachedFootprintAverageWetness = 0.0;
        VehicleScalar cachedFootprintAverageSurfaceTemperatureC = 20.0;
        bool cachedFootprintDeformablePropertiesValid = false;
        heritage::physics::SurfaceDeformableProperties
            cachedFootprintDeformableProperties{};
        UnsprungMassState unsprungMass;
    };

    struct AntiRollBarRecord
    {
        SuspensionAntiRollBarDescription description;
        SuspensionAntiRollBarOutput state;
    };

    struct Record
    {
        VehicleDescription description;
        std::vector<WheelRecord> wheels;
        float throttle = 0.0f;
        float brake = 0.0f;
        float steering = 0.0f;
        float handbrake = 0.0f;
        double highRateAccumulator = 0.0;
        float speed = 0.0f;
        float currentSteerCenterDegrees = 0.0f;
        float targetSteerCenterDegrees = 0.0f;
        float innerSteerAngleDegrees = 0.0f;
        float outerSteerAngleDegrees = 0.0f;
        float detectedWheelbase = 0.0f;
        float detectedSteerTrack = 0.0f;
        float currentSteeringRateFactor = 1.0f;
        PowertrainDescription powertrain;
        TireModelDescription tireModel;
        DriverAidDescription driverAids;
        TireSurface surface = TireSurface::DryAsphalt;
        int currentGear = 1;
        int requestedGear = 1;
        bool shifting = false;
        float shiftTimeRemaining = 0.0f;
        float engineRpm = 900.0f;
        float engineTorque = 0.0f;
        float clutchEngagement = 0.0f;
        float clutchSlipRpm = 0.0f;
        float wheelCoupledRpm = 0.0f;
        float selectedGearRatio = 3.40f;
        float outputTorque = 0.0f;
        float drivenWheelSpeedDifferenceRpm = 0.0f;
        std::size_t groundedWheelCount = 0;
        int antiLockActiveWheelCount = 0;
        int tractionControlActiveWheelCount = 0;
        int lastHighRateStepCount = 0;
        std::uint64_t totalHighRateStepCount = 0;
        float restTimer = 0.0f;
        bool parkedResting = false;
        bool parkedRestRequiresBrake = false;
        float parkedRestBrakeInput = 0.0f;
        float parkedRestHandbrakeInput = 0.0f;
        bool restCandidate = false;
        float requiredHoldForce = 0.0f;
        float availableBrakeHoldForce = 0.0f;
        VehicleDynamicsLab dynamicsLab;
        // Reused high-rate scratch storage. Keeping this with the vehicle record
        // avoids allocating a fresh drive-share vector on every 1000 Hz substep.
        std::vector<VehicleScalar> driveSharesScratch;
        // Suspension cross-coupling is solved from a same-instant snapshot so
        // wheel iteration order cannot change anti-roll-bar forces.
        std::vector<AntiRollBarRecord> antiRollBars;
        std::vector<VehicleScalar> antiRollForcesScratch;
        ChassisTorsionalComplianceDescription chassisFlex;
        ChassisTorsionalComplianceState chassisFlexState;
    };

    struct SteeringSubstepState
    {
        float axleCenterX = 0.0f;
        float centerMagnitudeDegrees = 0.0f;
        float innerMagnitudeDegrees = 0.0f;
        float outerMagnitudeDegrees = 0.0f;
        float centerSign = 0.0f;
    };

    struct DrivelineSubstepState
    {
        VehicleScalar drivenOmega = 0.0;
        float totalBrakeFactor = 1.0f;
        float totalHandbrakeFactor = 1.0f;
    };

    struct Slot
    {
        std::uint32_t generation = 1;
        bool alive = false;
        Record record;
    };

    static VehicleHandle makeHandle(std::uint32_t index, std::uint32_t generation);
    static bool decodeHandle(
        VehicleHandle handle,
        std::uint32_t& index,
        std::uint32_t& generation);
    Slot* resolve(VehicleHandle handle);
    const Slot* resolve(VehicleHandle handle) const;
    bool destroyResolved(std::uint32_t index, Slot& slot);

    SteeringSubstepState updateSteeringSubstep(
        Record& vehicle,
        float chassisSpeed,
        float substepDeltaTime);
    DrivelineSubstepState updateDrivelineSubstep(
        Record& vehicle,
        float substepDeltaTime);
    void simulateWheelSubstep(
        Record& vehicle,
        std::size_t wheelIndex,
        const SteeringSubstepState& steering,
        const DrivelineSubstepState& driveline,
        const heritage::physics::RigidBodyPose& chassisPose,
        const heritage::math::Vec3& chassisCenterOfMassLocal,
        heritage::math::Vec3& chassisLinearVelocity,
        heritage::math::Vec3& chassisAngularVelocityDegrees,
        heritage::physics::RigidBodySystem& bodies,
        const heritage::physics::CollisionSystem& collisions,
        heritage::physics::SurfaceWorld& surfaces,
        float substepDeltaTime,
        VehicleScalar antiRollBarForceN,
        VehicleScalar chassisSectionTwistRadians);
    void prepareAntiRollBarForces(Record& vehicle);
    void updateChassisFlexSubstep(
        Record& vehicle,
        float substepDeltaTime);
    void simulateVehicleSubstep(
        Record& vehicle,
        heritage::physics::RigidBodySystem& bodies,
        const heritage::physics::CollisionSystem& collisions,
        heritage::physics::SurfaceWorld& surfaces,
        float substepDeltaTime);
    void captureDynamicsLabFrame(
        Record& vehicle,
        const heritage::physics::RigidBodySystem& bodies,
        float sourceDeltaTime);
    void setError(const std::string& message) const;
    void clearError() const;

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeIndices;
    std::size_t m_aliveCount = 0;
    mutable std::string m_lastError;
};

} // namespace heritage::vehicles
