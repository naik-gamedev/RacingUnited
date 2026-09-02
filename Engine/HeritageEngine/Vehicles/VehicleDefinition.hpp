#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"

namespace heritage::vehicles {

inline constexpr int kVehicleDefinitionSchemaVersion = 2;
inline constexpr std::size_t kInvalidVehicleComponentIndex =
    (std::numeric_limits<std::size_t>::max)();

struct VehicleBodyDefinition
{
    std::string id;
    std::string role;
    float massKg = 1.0f;

    // MASS01: body mass properties are distinct from collision geometry.
    // Optional values let measured/CAD/estimated data override collider-derived
    // COM/inertia without forcing every legacy definition to provide them.
    bool hasCenterOfMassLocal = false;
    heritage::math::Vec3 centerOfMassLocal{};
    bool hasInertiaLocalKgM2 = false;
    heritage::math::Vec3 inertiaLocalKgM2{};
    float frontStaticLoadFraction = 0.50f;
    float leftStaticLoadFraction = 0.50f;
    std::string massPropertiesProvenance;
    float massPropertiesConfidence = 0.0f;
};

struct VehiclePowerUnitDefinition
{
    std::string id;
    std::string kind;
    std::string mountBody;
    std::string location;
    float maximumTorqueNm = 0.0f;
    float idleRpm = 900.0f;
    float redlineRpm = 7000.0f;
    float engineBrakingTorqueNm = 70.0f;
};

struct VehicleTransmissionDefinition
{
    std::string id;
    std::string kind;
    std::string powerUnit;
    float reverseRatio = -3.20f;
    std::vector<float> forwardRatios;
    float finalDriveRatio = 3.90f;
    float efficiency = 0.88f;
    float shiftDurationSeconds = 0.22f;
    float clutchEngagementRate = 5.0f;
};

struct VehicleSuspensionHardpointDefinition
{
    std::string id;
    heritage::math::Vec3 localPosition{};
    // Epistemic provenance is authoring metadata, not a simulation branch.
    // Examples: measured, asset_authored, estimated. Unknown/legacy data may
    // leave provenance empty and confidence at zero.
    std::string provenance;
    float confidence = 0.0f;
};

struct VehicleSuspensionDefinition
{
    std::string id;
    std::string provider;
    std::string mountBody;
    // Optional creator/asset/estimator linkage anchors. Compatibility providers
    // ignore them; mechanism-specific providers such as MacPherson consume them.
    std::vector<VehicleSuspensionHardpointDefinition> hardpoints;
    float restLengthM = 0.50f;
    float maximumCompressionM = 0.18f;
    float maximumDroopM = 0.15f;
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
    heritage::math::Vec3 localSteeringAxis{ 0.0f, 1.0f, 0.0f };
    float staticCamberDegrees = 0.0f;
    float camberGainDegreesPerM = 0.0f;
    float camberProgressionDegreesPerM2 = 0.0f;
    float staticToeDegrees = 0.0f;
    float toeGainDegreesPerM = 0.0f;
    float toeProgressionDegreesPerM2 = 0.0f;
    float motionRatio = 1.0f;
    float maximumForceN = 250000.0f;
    float leafInterleafFrictionN = 450.0f;
    float leafInterleafVelocityScaleMps = 0.025f;
    float leafInterleafViscousNsPerM = 250.0f;
    float leafAxleWrapStiffnessNmPerRad = 16000.0f;
    float leafAxleWrapDampingNmsPerRad = 1200.0f;
    float leafAxleWrapInertiaKgM2 = 5.0f;
    float leafAxleWrapJackingNPerRad = 1800.0f;
    float motorcycleRearSprocketPitchRadiusM = 0.105f;
    float twistBeamTorsionalStiffnessNmPerRad = 14000.0f;
    float twistBeamTorsionalDampingNmsPerRad = 900.0f;
};

struct VehicleContactUnitDefinition
{
    std::string id;
    std::string kind;
    std::string mountBody;
    std::string axle;
    heritage::math::Vec3 localMount{};
    heritage::math::Vec3 suspensionDirection{ 0.0f, -1.0f, 0.0f };
    std::string suspension;
    bool steering = false;
    bool serviceBrake = true;
    bool parkingBrake = false;
    std::string tireProvider;
    std::string tireParameterFile;
    std::string tireParameterProvenance;
    float tireParameterConfidence = 0.0f;
    float radiusM = 0.35f;
    float effectiveUnsprungMassKg = 0.0f;
    float tireRadialStiffnessNPerM = 220000.0f;
    float tireRadialDampingNsPerM = 1800.0f;
    float maximumTireDeflectionM = 0.08f;
    float maximumTireNormalForceN = 250000.0f;
    float serviceBrakeFactor = 0.25f;
    float parkingBrakeFactor = 0.0f;
};

struct VehicleAntiRollBarDefinition
{
    std::string id;
    std::string leftContactUnit;
    std::string rightContactUnit;
    bool enabled = true;
    float torsionalStiffnessNmPerRad = 0.0f;
    float torsionalDampingNmsPerRad = 0.0f;
    float leftLeverArmM = 0.20f;
    float rightLeverArmM = 0.20f;
    float leftLinkMotionRatio = 1.0f;
    float rightLinkMotionRatio = 1.0f;
    float maximumWheelForceN = 12000.0f;
    std::string provenance;
    float confidence = 0.0f;
};

struct VehicleChassisFlexDefinition
{
    bool enabled = false;
    std::string provider = "chassis_torsional_mode_v1";
    std::string mountBody = "chassis";
    float torsionalRigidityNmPerDegree = 10000.0f;
    float torsionalDampingNmsPerRad = 12000.0f;
    float effectiveTorsionalInertiaKgM2 = 500.0f;
    float torsionAxisLocalY = 0.45f;
    float frontReferenceLocalZ = 1.20f;
    float rearReferenceLocalZ = -1.20f;
    float maximumTwistDegrees = 1.0f;
    std::string provenance;
    float confidence = 0.0f;
};

struct VehicleDriveConnectionDefinition
{
    std::string id;
    std::string transmission;
    std::vector<std::string> contactUnits;
};

struct VehicleDefinitionRequirements
{
    bool leanDynamics = false;
    bool articulation = false;
    bool trackContacts = false;
};

struct VehicleDefinitionV2Source
{
    int schemaVersion = kVehicleDefinitionSchemaVersion;
    std::string id;
    std::string displayName;
    std::string classification;
    std::string bodyAsset;
    std::string driveLayoutIntent;
    std::string powerUnitPlacementIntent;
    VehicleDefinitionRequirements requirements;
    std::vector<VehicleBodyDefinition> bodies;
    std::vector<VehiclePowerUnitDefinition> powerUnits;
    std::vector<VehicleTransmissionDefinition> transmissions;
    std::vector<VehicleSuspensionDefinition> suspensions;
    std::vector<VehicleContactUnitDefinition> contactUnits;
    std::vector<VehicleAntiRollBarDefinition> antiRollBars;
    VehicleChassisFlexDefinition chassisFlex;
    std::vector<VehicleDriveConnectionDefinition> driveConnections;
};

enum class VehicleDefinitionIssueSeverity
{
    Warning,
    Error
};

struct VehicleDefinitionIssue
{
    VehicleDefinitionIssueSeverity severity =
        VehicleDefinitionIssueSeverity::Error;
    std::string code;
    std::string message;
};

struct CompiledVehiclePowerUnit
{
    VehiclePowerUnitDefinition authored;
    std::size_t mountBodyIndex = kInvalidVehicleComponentIndex;
};

struct CompiledVehicleTransmission
{
    VehicleTransmissionDefinition authored;
    std::size_t powerUnitIndex = kInvalidVehicleComponentIndex;
};

struct CompiledVehicleSuspension
{
    VehicleSuspensionDefinition authored;
    std::size_t mountBodyIndex = kInvalidVehicleComponentIndex;
};

struct CompiledVehicleContactUnit
{
    VehicleContactUnitDefinition authored;
    std::size_t mountBodyIndex = kInvalidVehicleComponentIndex;
    std::size_t suspensionIndex = kInvalidVehicleComponentIndex;
    float driveFactor = 0.0f;
};

struct CompiledVehicleAntiRollBar
{
    VehicleAntiRollBarDefinition authored;
    std::size_t leftContactUnitIndex = kInvalidVehicleComponentIndex;
    std::size_t rightContactUnitIndex = kInvalidVehicleComponentIndex;
};

struct CompiledVehicleChassisFlex
{
    VehicleChassisFlexDefinition authored;
    std::size_t mountBodyIndex = kInvalidVehicleComponentIndex;
};

struct CompiledVehicleDriveConnection
{
    std::string id;
    std::size_t transmissionIndex = kInvalidVehicleComponentIndex;
    std::vector<std::size_t> contactUnitIndices;
};

struct CompiledVehicleDefinition
{
    int schemaVersion = kVehicleDefinitionSchemaVersion;
    std::string id;
    std::string displayName;
    std::string classification;
    std::string bodyAsset;
    VehicleDefinitionRequirements requirements;
    std::vector<VehicleBodyDefinition> bodies;
    std::vector<CompiledVehiclePowerUnit> powerUnits;
    std::vector<CompiledVehicleTransmission> transmissions;
    std::vector<CompiledVehicleSuspension> suspensions;
    std::vector<CompiledVehicleContactUnit> contactUnits;
    std::vector<CompiledVehicleAntiRollBar> antiRollBars;
    CompiledVehicleChassisFlex chassisFlex;
    std::vector<CompiledVehicleDriveConnection> driveConnections;
    std::string runtimeProvider;
};

struct VehicleDefinitionCompileResult
{
    bool valid = false;
    bool currentSolverReady = false;
    CompiledVehicleDefinition definition;
    std::vector<VehicleDefinitionIssue> issues;
    std::string summary;

    std::string issueSummary() const;
};

} // namespace heritage::vehicles
