#pragma once

#include "../Physics/CollisionSystem.hpp"
#include "../Physics/RigidBodySystem.hpp"
#include "../Physics/Surfaces/SurfaceWorld.hpp"
#include "../Vehicles/VehicleDefinitionCompiler.hpp"
#include "../Vehicles/VehicleDefinitionLoader.hpp"
#include "../Vehicles/VehicleSystem.hpp"
#include "../Vehicles/SuspensionGeometry.hpp"
#include "../Vehicles/UnsprungMassModel.hpp"

#include <cstddef>
#include <string>

namespace heritage::tests {

using heritage::math::Vec3;
using heritage::physics::BodyHandle;
using heritage::physics::BodyMotionType;
using heritage::physics::ColliderHandle;
using heritage::physics::CollisionSystem;
using heritage::physics::RigidBodyDescription;
using heritage::physics::RigidBodyPose;
using heritage::physics::RigidBodySystem;
using heritage::physics::SurfaceWorld;
using heritage::physics::StaticSceneTriangle;
using heritage::vehicles::VehicleDescription;
using heritage::vehicles::VehicleDefinitionCompiler;
using heritage::vehicles::VehicleDefinitionLoadSettings;
using heritage::vehicles::VehicleDefinitionLoader;
using heritage::vehicles::VehicleDefinitionV2Source;
using heritage::vehicles::VehicleHandle;
using heritage::vehicles::VehicleRestState;
using heritage::vehicles::VehicleSystem;
using heritage::vehicles::WheelContactStatus;
using heritage::vehicles::WheelDescription;
using heritage::vehicles::WheelState;

inline constexpr float kWorldDeltaTime = 1.0f / 120.0f;
inline constexpr Vec3 kGravity{ 0.0f, -9.81f, 0.0f };

struct PrototypeWorld
{
    RigidBodySystem bodies;
    CollisionSystem collisions;
    SurfaceWorld surfaces;
    VehicleSystem vehicles;
    BodyHandle floor = heritage::physics::InvalidBody;
    ColliderHandle floorCollider = heritage::physics::InvalidCollider;
    BodyHandle chassis = heritage::physics::InvalidBody;
    ColliderHandle chassisCollider = heritage::physics::InvalidCollider;
    VehicleHandle vehicle = heritage::vehicles::InvalidVehicle;
};

struct StabilitySample
{
    Vec3 startPosition{};
    Vec3 endPosition{};
    float maximumHorizontalSpeed = 0.0f;
    float maximumVerticalSpeed = 0.0f;
    float maximumAngularSpeedDegrees = 0.0f;
    float verticalPositionSpan = 0.0f;
    heritage::vehicles::VehicleScalar maximumSuspensionVelocity = 0.0;
    heritage::vehicles::VehicleScalar maximumWheelAngularSpeed = 0.0;
    std::size_t minimumGroundedWheels = 4;
    bool sleepingAtEnd = false;
};

float magnitude(const Vec3& value);
float horizontalMagnitude(const Vec3& value);
bool addPrototypeWheel(
    PrototypeWorld& world,
    float x,
    float z,
    float driveFactor,
    float steerFactor,
    float serviceBrakeFactor,
    float handbrakeFactor);
bool createPrototypeWorld(PrototypeWorld& world, float highRateHertz);
bool replaceFloorWithSlope(PrototypeWorld& world, float slopeDegrees);
void stepWorld(
    PrototypeWorld& world,
    float worldDeltaTime = kWorldDeltaTime);
StabilitySample sampleStability(
    PrototypeWorld& world,
    float settleSeconds,
    float measureSeconds,
    float worldDeltaTime = kWorldDeltaTime);
void printSample(const std::string& name, const StabilitySample& sample);
void printWheelStates(const PrototypeWorld& world, const std::string& name);

bool parkedVehicleStaysQuiet();
bool flatRestSleepsAndThrottleWakes();
bool brakeHeldSteeringWakesAndTracks();
bool highRateSuspensionAgreesWithNativeRate();
bool parkingBrakeHoldsOnSlope();
bool unbrakedVehicleRollsOnSlope();
bool turnThenBrakeRemainsStableAtLowSpeed();
bool rigidBodyCenterOfMassOffsetGeneratesTorque();
bool vehicleChassisRollRespondsToCornering();
bool vehicleCombinedPitchRollYawRespondsToBrakingTurn();
bool chassisFlexEstimatorProducesBoundedEpistemicEstimate();
bool chassisTorsionalComplianceRespondsToDiagonalLoad();
bool chassisFlexIntegratesWithHighRateVehicleDynamics();
bool vehicleMassPropertiesEstimatorProducesBoundedEstimate();
bool vehicleMassComponentAccumulationUsesParallelAxisTheorem();
bool rigidBodyExplicitInertiaIsAuthoritative();
bool wheelFitmentAndAlignmentAreReferenceSafe();
bool terrainContactDiagnosticsClassifyFailureModes();
bool staticTriangleRigidBodyContactsSettle();
bool dynamicsLabCapturesHighRateTelemetry();
bool vehicleDefinitionCompilerAndLoaderWork();
bool steeringDirectionAndAckermannAreSymmetric();
bool suspensionGeometryProducesAuthoritativePose();
bool macPhersonHardpointKinematicsAreDeterministic();
bool assistedMacPhersonEstimateIsPlausibleAndMirrored();
bool assistedFrontMacPhersonVehicleStaysStable();
bool trailingArmTorsionBarKinematicsAreDeterministic();
bool assistedFrontRearSuspensionVehicleStaysStable();
bool suspensionAntiRollBarCouplesWheelPairs();
bool unsprungMassSettlesAndRespondsToRoadStep();
bool magicFormula62RoadCoreBehaves();
bool magicFormula62TurnSlipReducesGripAndTrail();
bool magicFormula62MotorcycleLargeCamberBehaves();
bool tireRelaxationDynamicsAreRateStable();
bool tireContactPatchParkingTwistIsRateStable();
bool tireContactGeometryEffectiveRadiusAndFootprintBehave();
bool tireRigidRingStructuralModesAreRateStable();
bool tireRoadEnvelopeFiltersShortObstacle();
bool tireThermalPressureAndGripStateAreRateStable();
bool tireSpatialTreadThermalWearAndFlatSpotBehave();
bool tireTreadContaminationPickupAndCleaningBehave();
bool tireWetSurfaceHydroplaningAndDrainageBehave();
bool tireWinterSurfaceIceSnowAndStudsBehave();
bool tireShallowGranularGravelDirtBehaves();
bool tireDeformableTerrainPersistenceBehaves();
bool tirePropertyFileImporterMapsMf62AndMotorcycleData();
bool tireFamilyBaselinesAreCoherentAndBrandNeutral();
bool tirePerformanceBiasesMapToMechanismsWithoutForceMultipliers();
bool tirePartsResolveAndAssignReusableFitments();

bool surfaceWorldGlobalAddressingAndChunkCacheBehave();
bool surfacePresentationIsBoundedAndWorldAddressed();
bool trackRubberBuildsMigratesAndWashes();
} // namespace heritage::tests
