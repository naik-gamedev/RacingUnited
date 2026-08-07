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

struct VehicleSuspensionDefinition
{
    std::string id;
    std::string provider;
    std::string mountBody;
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
    float motionRatio = 1.0f;
    float maximumForceN = 250000.0f;
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
    float radiusM = 0.35f;
    float serviceBrakeFactor = 0.25f;
    float parkingBrakeFactor = 0.0f;
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
