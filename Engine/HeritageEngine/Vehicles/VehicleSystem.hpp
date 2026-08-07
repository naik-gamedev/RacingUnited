#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../Core/Math/Math.hpp"
#include "../Physics/CollisionSystem.hpp"
#include "../Physics/RigidBodySystem.hpp"
#include "TireModel.hpp"
#include "SuspensionModel.hpp"
#include "UnsprungMassModel.hpp"
#include "VehicleDynamicsLab.hpp"

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

struct WheelState
{
    bool grounded = false;
    float suspensionLength = 0.0f;
    float compression = 0.0f;
    float compressionVelocity = 0.0f;
    float suspensionSpringForce = 0.0f;
    float suspensionDampingForce = 0.0f;
    float suspensionBumpStopForce = 0.0f;
    float suspensionDroopStopForce = 0.0f;
    float suspensionUnclampedForce = 0.0f;
    float damperDissipationWatts = 0.0f;
    float unsprungVelocity = 0.0f;
    float tireDeflection = 0.0f;
    float tireDeflectionVelocity = 0.0f;
    float tireRadialDissipationWatts = 0.0f;
    float normalForce = 0.0f;
    float longitudinalForce = 0.0f;
    float lateralForce = 0.0f;
    float steerAngleDegrees = 0.0f;
    float wheelAngularVelocity = 0.0f;
    float appliedDriveTorque = 0.0f;
    float appliedBrakeTorque = 0.0f;
    float serviceBrakeTorque = 0.0f;
    float handbrakeTorque = 0.0f;
    float antiLockModulation = 1.0f;
    float tractionControlModulation = 1.0f;
    bool antiLockActive = false;
    bool tractionControlActive = false;
    float wheelRotationDegrees = 0.0f;
    float longitudinalSpeed = 0.0f;
    float lateralSpeed = 0.0f;
    float slipRatio = 0.0f;
    float slipAngleDegrees = 0.0f;
    float relaxedSlipRatio = 0.0f;
    float relaxedSlipAngleDegrees = 0.0f;
    float effectiveFriction = 0.0f;
    float gripUtilization = 0.0f;
    float pureLongitudinalForce = 0.0f;
    float pureLateralForce = 0.0f;
    float combinedSlipScale = 1.0f;
    float pneumaticTrail = 0.0f;
    float aligningTorque = 0.0f;
    heritage::physics::ColliderHandle contactCollider =
        heritage::physics::InvalidCollider;
    heritage::physics::SurfaceMaterial surfaceMaterial =
        heritage::physics::SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    heritage::math::Vec3 worldCenter{};
    heritage::math::Vec3 contactPoint{};
    heritage::math::Vec3 contactNormal{ 0.0f, 1.0f, 0.0f };
};

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
    bool setWheelSuspensionModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        const SuspensionModelDescription& value);
    bool wheelSuspensionModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        SuspensionModelDescription& value) const;
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
        float nominalLoad,
        float peakFriction,
        float longitudinalStiffness,
        float corneringStiffness,
        float loadSensitivity,
        float longitudinalRelaxationLength,
        float lateralRelaxationLength,
        float wheelInertia,
        float pneumaticTrail,
        float stiffnessLoadExponent,
        float longitudinalShapeFactor,
        float lateralShapeFactor,
        float longitudinalCurvatureFactor,
        float lateralCurvatureFactor,
        float combinedSlipExponent,
        float pneumaticTrailFalloff);
    bool setWheelTireModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        float nominalLoad,
        float peakFriction,
        float longitudinalStiffness,
        float corneringStiffness,
        float loadSensitivity,
        float longitudinalRelaxationLength,
        float lateralRelaxationLength,
        float wheelInertia,
        float pneumaticTrail,
        float stiffnessLoadExponent,
        float longitudinalShapeFactor,
        float lateralShapeFactor,
        float longitudinalCurvatureFactor,
        float lateralCurvatureFactor,
        float combinedSlipExponent,
        float pneumaticTrailFalloff);
    bool wheelTireModel(
        VehicleHandle handle,
        std::size_t wheelIndex,
        TireModelDescription& value) const;
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
        float worldDeltaTime,
        const heritage::math::Vec3& gravity = { 0.0f, -9.80665f, 0.0f });

    const std::string& lastError() const { return m_lastError; }

private:
    struct WheelRecord
    {
        WheelDescription description;
        WheelState state;
        TireModelDescription tireModel;
        float previousSuspensionLength = 0.0f;
        UnsprungMassState unsprungMass;
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

    void simulateVehicleSubstep(
        Record& vehicle,
        heritage::physics::RigidBodySystem& bodies,
        const heritage::physics::CollisionSystem& collisions,
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
