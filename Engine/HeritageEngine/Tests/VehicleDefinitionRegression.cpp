#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/Suspension/Authoring/TrailingArmHardpointEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace heritage::tests {

namespace {

void authorMacPhersonHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    bool leftSide)
{
    auto mirrorX = [leftSide](Vec3 value) {
        if (leftSide) value.x = -value.x;
        return value;
    };
    suspension.hardpoints = {
        { "strut_top_mount", mirrorX({ 0.55f, 1.15f, 1.16f }), "measured", 1.0f },
        { "strut_upright_mount", mirrorX({ 0.68f, 0.62f, 1.19f }), "measured", 1.0f },
        { "lower_arm_inner_front", mirrorX({ 0.25f, 0.38f, 1.45f }), "measured", 1.0f },
        { "lower_arm_inner_rear", mirrorX({ 0.25f, 0.38f, 0.95f }), "measured", 1.0f },
        { "lower_ball_joint", mirrorX({ 0.78f, 0.30f, 1.20f }), "measured", 1.0f },
        { "tie_rod_inner", mirrorX({ 0.30f, 0.48f, 1.08f }), "measured", 1.0f },
        { "tie_rod_outer", mirrorX({ 0.68f, 0.36f, 1.13f }), "measured", 1.0f },
        { "wheel_center", mirrorX({ 0.72f, 0.34f, 1.20f }), "measured", 1.0f }
    };
}

void authorTrailingArmHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter)
{
    heritage::vehicles::TrailingArmHardpointEstimateInput input;
    input.wheelCenter = wheelCenter;
    input.referencePackageScaleM = 0.2979f;
    const auto estimate =
        heritage::vehicles::estimateTrailingArmHardpointsV1(input);
    if (!estimate.valid)
        return;

    const auto add = [&](const char* id, const Vec3& position) {
        suspension.hardpoints.push_back(
            { id, position, "estimated", estimate.confidence });
    };
    add("arm_pivot_inner", estimate.hardpoints.armPivotInner);
    add("arm_pivot_outer", estimate.hardpoints.armPivotOuter);
    add("wheel_center", estimate.hardpoints.wheelCenter);
    add("damper_upper_mount", estimate.hardpoints.damperUpperMount);
    add("damper_lower_mount", estimate.hardpoints.damperLowerMount);
}

} // namespace

VehicleDefinitionV2Source makeCompiledRoadCarDefinition()
{
    VehicleDefinitionV2Source source;
    source.id = "native_compiler_test";
    source.displayName = "Native Compiler Test";
    source.classification = "car";
    source.bodyAsset = "Vehicles/Player/PlayerCar.obj";
    heritage::vehicles::VehicleBodyDefinition primaryBody;
    primaryBody.id = "chassis";
    primaryBody.role = "primary";
    primaryBody.massKg = 1100.0f;
    source.bodies.push_back(primaryBody);
    source.bodies[0].hasCenterOfMassLocal = true;
    source.bodies[0].centerOfMassLocal = { 0.0f, 0.52f, 0.20f };
    source.bodies[0].hasInertiaLocalKgM2 = true;
    source.bodies[0].inertiaLocalKgM2 = { 1212.8886f, 1511.3550f, 564.3155f };
    source.bodies[0].frontStaticLoadFraction = 0.5819001f;
    source.bodies[0].leftStaticLoadFraction = 0.50f;
    source.bodies[0].massPropertiesProvenance =
        "estimated_mass_properties_road_car_v1";
    source.bodies[0].massPropertiesConfidence = 0.20f;

    heritage::vehicles::VehiclePowerUnitDefinition power;
    power.id = "engine";
    power.kind = "combustion";
    power.mountBody = "chassis";
    power.location = "front";
    power.maximumTorqueNm = 250.0f;
    source.powerUnits.push_back(power);

    heritage::vehicles::VehicleTransmissionDefinition transmission;
    transmission.id = "gearbox";
    transmission.kind = "manual";
    transmission.powerUnit = "engine";
    transmission.forwardRatios = { 3.40f, 2.10f, 1.45f, 1.12f, 0.89f, 0.74f };
    source.transmissions.push_back(transmission);

    const Vec3 mounts[] = {
        { -0.7185f, 0.85f, 1.221f },
        { 0.7185f, 0.85f, 1.221f },
        { -0.7140f, 0.85f, -1.221f },
        { 0.7140f, 0.85f, -1.221f }
    };
    for (std::size_t index = 0; index < 4; ++index)
    {
        heritage::vehicles::VehicleSuspensionDefinition suspension;
        suspension.id = "suspension_" + std::to_string(index + 1);
        suspension.provider = "linear_raycast_v1";
        suspension.mountBody = "chassis";
        suspension.restLengthM = 0.55f;
        suspension.maximumCompressionM = 0.20f;
        suspension.maximumDroopM = 0.15f;
        suspension.springRateNPerM = 35000.0f;
        suspension.bumpDampingNsPerM = 3200.0f;
        suspension.reboundDampingNsPerM = 4200.0f;
        suspension.staticCamberDegrees = index % 2 == 0 ? -0.75f : 0.75f;
        suspension.camberGainDegreesPerM = index % 2 == 0 ? -4.0f : 4.0f;
        suspension.staticToeDegrees = index % 2 == 0 ? 0.10f : -0.10f;
        if (index == 0)
        {
            suspension.hardpoints.push_back(
                {
                    "strut_top_mount",
                    { -0.61f, 1.08f, 1.20f },
                    "estimated",
                    0.35f
                });
        }
        source.suspensions.push_back(suspension);

        heritage::vehicles::VehicleContactUnitDefinition contact;
        contact.id = "wheel_" + std::to_string(index + 1);
        contact.kind = "wheel";
        contact.mountBody = "chassis";
        contact.axle = index < 2 ? "front" : "rear";
        contact.localMount = mounts[index];
        contact.steering = index < 2;
        contact.parkingBrake = index >= 2;
        contact.suspension = suspension.id;
        contact.tireProvider = "advanced_road";
        contact.radiusM = 0.2979f;
        contact.effectiveUnsprungMassKg = 38.0f;
        contact.tireRadialStiffnessNPerM = 220000.0f;
        contact.tireRadialDampingNsPerM = 1800.0f;
        contact.maximumTireDeflectionM = 0.08f;
        contact.maximumTireNormalForceN = 250000.0f;
        contact.serviceBrakeFactor = index < 2 ? 0.31f : 0.19f;
        contact.parkingBrakeFactor = index >= 2 ? 0.50f : 0.0f;
        source.contactUnits.push_back(contact);
    }

    heritage::vehicles::VehicleAntiRollBarDefinition frontBar;
    frontBar.id = "front_anti_roll_bar";
    frontBar.leftContactUnit = "wheel_1";
    frontBar.rightContactUnit = "wheel_2";
    frontBar.torsionalStiffnessNmPerRad = 520.0f;
    frontBar.torsionalDampingNmsPerRad = 18.0f;
    frontBar.leftLeverArmM = 0.20f;
    frontBar.rightLeverArmM = 0.20f;
    frontBar.leftLinkMotionRatio = 1.0f;
    frontBar.rightLinkMotionRatio = 1.0f;
    frontBar.maximumWheelForceN = 7000.0f;
    frontBar.provenance = "estimated";
    frontBar.confidence = 0.20f;
    source.antiRollBars.push_back(frontBar);

    heritage::vehicles::VehicleAntiRollBarDefinition rearBar = frontBar;
    rearBar.id = "rear_anti_roll_bar";
    rearBar.leftContactUnit = "wheel_3";
    rearBar.rightContactUnit = "wheel_4";
    rearBar.torsionalStiffnessNmPerRad = 380.0f;
    rearBar.torsionalDampingNmsPerRad = 14.0f;
    rearBar.maximumWheelForceN = 6000.0f;
    source.antiRollBars.push_back(rearBar);

    source.chassisFlex.enabled = true;
    source.chassisFlex.provider = "chassis_torsional_mode_v1";
    source.chassisFlex.mountBody = "chassis";
    source.chassisFlex.torsionalRigidityNmPerDegree = 8700.0f;
    source.chassisFlex.torsionalDampingNmsPerRad = 11300.0f;
    source.chassisFlex.effectiveTorsionalInertiaKgM2 = 525.0f;
    source.chassisFlex.torsionAxisLocalY = 0.364f;
    source.chassisFlex.frontReferenceLocalZ = 1.221f;
    source.chassisFlex.rearReferenceLocalZ = -1.221f;
    source.chassisFlex.maximumTwistDegrees = 1.25f;
    source.chassisFlex.provenance = "estimated_chassis_flex_closed_unibody_v1";
    source.chassisFlex.confidence = 0.18f;

    heritage::vehicles::VehicleDriveConnectionDefinition drive;
    drive.id = "front_drive";
    drive.transmission = "gearbox";
    drive.contactUnits = { "wheel_1", "wheel_2" };
    source.driveConnections.push_back(std::move(drive));
    return source;
}

bool vehicleDefinitionCompilerAndLoaderWork()
{
    const VehicleDefinitionV2Source source = makeCompiledRoadCarDefinition();
    const auto compiled = VehicleDefinitionCompiler::compile(source);
    const bool referencesResolved = compiled.definition.bodies.size() == 1
        && compiled.definition.bodies[0].hasCenterOfMassLocal
        && std::abs(compiled.definition.bodies[0].centerOfMassLocal.y - 0.52f)
            <= 0.000001f
        && compiled.definition.bodies[0].hasInertiaLocalKgM2
        && std::abs(compiled.definition.bodies[0].inertiaLocalKgM2.z - 564.3155f)
            <= 0.001f
        && compiled.definition.bodies[0].massPropertiesProvenance
            == "estimated_mass_properties_road_car_v1"
        && std::abs(compiled.definition.bodies[0].massPropertiesConfidence - 0.20f)
            <= 0.000001f
        && compiled.definition.powerUnits.size() == 1
        && compiled.definition.powerUnits[0].mountBodyIndex == 0
        && compiled.definition.transmissions.size() == 1
        && compiled.definition.transmissions[0].powerUnitIndex == 0
        && compiled.definition.suspensions.size() == 4
        && compiled.definition.suspensions[0].mountBodyIndex == 0
        && compiled.definition.suspensions[0].authored.hardpoints.size() == 1
        && compiled.definition.suspensions[0].authored.hardpoints[0].id
            == "strut_top_mount"
        && std::abs(
            compiled.definition.suspensions[0].authored.hardpoints[0]
                    .localPosition.y
                - 1.08f) <= 0.000001f
        && compiled.definition.suspensions[0].authored.hardpoints[0].provenance
            == "estimated"
        && std::abs(
            compiled.definition.suspensions[0].authored.hardpoints[0].confidence
                - 0.35f) <= 0.000001f
        && compiled.definition.antiRollBars.size() == 2
        && compiled.definition.antiRollBars[0].leftContactUnitIndex == 0
        && compiled.definition.antiRollBars[0].rightContactUnitIndex == 1
        && compiled.definition.antiRollBars[1].leftContactUnitIndex == 2
        && compiled.definition.antiRollBars[1].rightContactUnitIndex == 3
        && compiled.definition.antiRollBars[0].authored.provenance == "estimated"
        && std::abs(compiled.definition.antiRollBars[0].authored.confidence - 0.20f)
            <= 0.000001f
        && compiled.definition.chassisFlex.authored.enabled
        && compiled.definition.chassisFlex.mountBodyIndex == 0
        && compiled.definition.chassisFlex.authored.provider
            == "chassis_torsional_mode_v1"
        && std::abs(
            compiled.definition.chassisFlex.authored
                    .torsionalRigidityNmPerDegree
                - 8700.0f) <= 0.0001f
        && compiled.definition.chassisFlex.authored.provenance
            == "estimated_chassis_flex_closed_unibody_v1"
        && std::abs(compiled.definition.chassisFlex.authored.confidence - 0.18f)
            <= 0.000001f
        && compiled.definition.driveConnections.size() == 1
        && compiled.definition.driveConnections[0].contactUnitIndices.size() == 2
        && compiled.definition.contactUnits.size() == 4
        && compiled.definition.contactUnits[0].suspensionIndex == 0
        && std::abs(
            compiled.definition.suspensions[0].authored.staticCamberDegrees
                + 0.75f) <= 0.000001f
        && std::abs(compiled.definition.contactUnits[0].driveFactor - 0.5f)
            <= 0.000001f
        && std::abs(compiled.definition.contactUnits[2].driveFactor)
            <= 0.000001f;

    RigidBodySystem bodies;
    RigidBodyDescription bodyDescription;
    bodyDescription.motionType = BodyMotionType::Dynamic;
    bodyDescription.mass = 1100.0f;
    const BodyHandle chassis = bodies.create(bodyDescription);
    VehicleSystem vehicles;
    VehicleDefinitionLoadSettings loadSettings;
    loadSettings.vehicle.chassisBody = chassis;
    loadSettings.vehicle.highRateHertz = 1000.0f;
    std::string loadMessage;
    const VehicleHandle loaded = VehicleDefinitionLoader::create(
        compiled.definition,
        loadSettings,
        bodies,
        vehicles,
        loadMessage);
    heritage::vehicles::SuspensionAntiRollBarDescription loadedFrontBar;
    heritage::vehicles::SuspensionAntiRollBarOutput loadedFrontBarState;
    heritage::vehicles::ChassisTorsionalComplianceDescription loadedFlex;
    heritage::vehicles::ChassisTorsionalComplianceState loadedFlexState;
    float loadedMass = 0.0f;
    Vec3 loadedCenterOfMass{};
    Vec3 loadedInertia{};
    bool loadedInertiaOverride = false;
    const bool loadedMassProperties = bodies.mass(chassis, loadedMass)
        && bodies.centerOfMassLocal(chassis, loadedCenterOfMass)
        && bodies.inertiaLocal(chassis, loadedInertia)
        && bodies.inertiaLocalOverridden(chassis, loadedInertiaOverride);
    const bool runtimeLoaded = loaded != heritage::vehicles::InvalidVehicle
        && loadedMassProperties
        && std::abs(loadedMass - 1100.0f) <= 0.001f
        && std::abs(loadedCenterOfMass.y - 0.52f) <= 0.000001f
        && std::abs(loadedCenterOfMass.z - 0.20f) <= 0.000001f
        && std::abs(loadedInertia.x - 1212.8886f) <= 0.01f
        && std::abs(loadedInertia.y - 1511.3550f) <= 0.01f
        && std::abs(loadedInertia.z - 564.3155f) <= 0.01f
        && loadedInertiaOverride
        && vehicles.wheelCount(loaded) == 4
        && vehicles.forwardGearCount(loaded) == 6
        && vehicles.antiRollBarCount(loaded) == 2
        && vehicles.antiRollBar(loaded, 0, loadedFrontBar, loadedFrontBarState)
        && loadedFrontBar.leftWheelIndex == 0
        && loadedFrontBar.rightWheelIndex == 1
        && std::abs(loadedFrontBar.torsionalStiffnessNmPerRad - 520.0f) <= 0.0001f
        && vehicles.chassisTorsionalCompliance(
            loaded, loadedFlex, loadedFlexState)
        && loadedFlex.enabled
        && std::abs(loadedFlex.torsionalRigidityNmPerDegree - 8700.0) <= 0.0001
        && std::abs(loadedFlex.torsionAxisLocalY - 0.364) <= 0.000001;

    VehicleDefinitionV2Source invalidAntiRollBarReference = source;
    invalidAntiRollBarReference.antiRollBars[0].leftContactUnit = "missing_wheel";
    const auto invalidAntiRollBarReferenceResult =
        VehicleDefinitionCompiler::compile(invalidAntiRollBarReference);

    VehicleDefinitionV2Source invalidAntiRollBarTopology = source;
    invalidAntiRollBarTopology.antiRollBars[0].rightContactUnit = "wheel_1";
    const auto invalidAntiRollBarTopologyResult =
        VehicleDefinitionCompiler::compile(invalidAntiRollBarTopology);

    VehicleDefinitionV2Source invalidAntiRollBarParameters = source;
    invalidAntiRollBarParameters.antiRollBars[0].leftLeverArmM = 0.0f;
    const auto invalidAntiRollBarParametersResult =
        VehicleDefinitionCompiler::compile(invalidAntiRollBarParameters);

    VehicleDefinitionV2Source invalidChassisFlexReference = source;
    invalidChassisFlexReference.chassisFlex.mountBody = "missing_chassis";
    const auto invalidChassisFlexReferenceResult =
        VehicleDefinitionCompiler::compile(invalidChassisFlexReference);

    VehicleDefinitionV2Source invalidChassisFlexProvider = source;
    invalidChassisFlexProvider.chassisFlex.provider = "magic_flex_v99";
    const auto invalidChassisFlexProviderResult =
        VehicleDefinitionCompiler::compile(invalidChassisFlexProvider);

    VehicleDefinitionV2Source invalidChassisFlexParameters = source;
    invalidChassisFlexParameters.chassisFlex.torsionalRigidityNmPerDegree = 0.0f;
    const auto invalidChassisFlexParametersResult =
        VehicleDefinitionCompiler::compile(invalidChassisFlexParameters);

    VehicleDefinitionV2Source broken = source;
    broken.transmissions[0].powerUnit = "missing_engine";
    const auto brokenResult = VehicleDefinitionCompiler::compile(broken);

    VehicleDefinitionV2Source brokenSuspension = source;
    brokenSuspension.contactUnits[0].suspension = "missing_suspension";
    const auto brokenSuspensionResult =
        VehicleDefinitionCompiler::compile(brokenSuspension);

    VehicleDefinitionV2Source invalidSuspensionParameters = source;
    invalidSuspensionParameters.suspensions[0].bumpHighSpeedDampingNsPerM =
        -1.0f;
    const auto invalidSuspensionParametersResult =
        VehicleDefinitionCompiler::compile(invalidSuspensionParameters);

    VehicleDefinitionV2Source invalidUnsprungParameters = source;
    invalidUnsprungParameters.contactUnits[0].effectiveUnsprungMassKg =
        -1.0f;
    const auto invalidUnsprungParametersResult =
        VehicleDefinitionCompiler::compile(invalidUnsprungParameters);

    VehicleDefinitionV2Source invalidGeometryParameters = source;
    invalidGeometryParameters.suspensions[0].localSteeringAxis = {};
    const auto invalidGeometryParametersResult =
        VehicleDefinitionCompiler::compile(invalidGeometryParameters);

    VehicleDefinitionV2Source duplicateHardpoint = source;
    duplicateHardpoint.suspensions[0].hardpoints.push_back(
        duplicateHardpoint.suspensions[0].hardpoints[0]);
    const auto duplicateHardpointResult =
        VehicleDefinitionCompiler::compile(duplicateHardpoint);

    VehicleDefinitionV2Source invalidHardpointPosition = source;
    invalidHardpointPosition.suspensions[0].hardpoints[0].localPosition.x =
        std::numeric_limits<float>::quiet_NaN();
    const auto invalidHardpointPositionResult =
        VehicleDefinitionCompiler::compile(invalidHardpointPosition);

    VehicleDefinitionV2Source invalidHardpointProvenance = source;
    invalidHardpointProvenance.suspensions[0].hardpoints[0].provenance =
        "estimated data with spaces";
    const auto invalidHardpointProvenanceResult =
        VehicleDefinitionCompiler::compile(invalidHardpointProvenance);

    VehicleDefinitionV2Source invalidHardpointConfidence = source;
    invalidHardpointConfidence.suspensions[0].hardpoints[0].confidence = 1.5f;
    const auto invalidHardpointConfidenceResult =
        VehicleDefinitionCompiler::compile(invalidHardpointConfidence);

    VehicleDefinitionV2Source future = source;
    future.classification = "motorcycle";
    future.requirements.leanDynamics = true;
    future.contactUnits.resize(2);
    future.antiRollBars.resize(1);
    const auto futureResult = VehicleDefinitionCompiler::compile(future);

    VehicleDefinitionV2Source categoryOnly = source;
    categoryOnly.classification = "fictional_hovering_potato";
    const auto categoryOnlyResult = VehicleDefinitionCompiler::compile(categoryOnly);

    VehicleDefinitionV2Source futureSuspension = source;
    futureSuspension.suspensions[0].provider = "double_wishbone_v1";
    const auto futureSuspensionResult =
        VehicleDefinitionCompiler::compile(futureSuspension);

    VehicleDefinitionV2Source macPhersonSource = source;
    for (std::size_t index = 0; index < 2; ++index)
    {
        macPhersonSource.suspensions[index].provider = "macpherson_strut_v1";
        authorMacPhersonHardpoints(
            macPhersonSource.suspensions[index], index == 0);
    }
    const auto macPhersonResult =
        VehicleDefinitionCompiler::compile(macPhersonSource);
    std::string macPhersonLoadMessage;
    VehicleDefinitionLoadSettings macPhersonLoadSettings = loadSettings;
    macPhersonLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle macPhersonVehicle = VehicleDefinitionLoader::create(
        macPhersonResult.definition,
        macPhersonLoadSettings,
        bodies,
        vehicles,
        macPhersonLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription macPhersonReadback;
    const bool macPhersonRuntimeReady = macPhersonResult.valid
        && macPhersonResult.currentSolverReady
        && macPhersonVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            macPhersonVehicle, 0, macPhersonReadback)
        && macPhersonReadback.provider
            == heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1
        && macPhersonReadback.macPherson.authored;

    VehicleDefinitionV2Source incompleteMacPherson = macPhersonSource;
    incompleteMacPherson.suspensions[0].hardpoints.pop_back();
    const auto incompleteMacPhersonResult =
        VehicleDefinitionCompiler::compile(incompleteMacPherson);

    VehicleDefinitionV2Source trailingArmSource = macPhersonSource;
    for (std::size_t index = 2; index < 4; ++index)
    {
        trailingArmSource.suspensions[index].provider =
            "trailing_arm_torsion_bar_v1";
        const auto& contact = trailingArmSource.contactUnits[index];
        const auto& suspension = trailingArmSource.suspensions[index];
        const Vec3 wheelCenter{
            contact.localMount.x
                + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y
                + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z
                + contact.suspensionDirection.z * suspension.restLengthM
        };
        authorTrailingArmHardpoints(
            trailingArmSource.suspensions[index], wheelCenter);
    }
    const auto trailingArmResult =
        VehicleDefinitionCompiler::compile(trailingArmSource);
    std::string trailingArmLoadMessage;
    VehicleDefinitionLoadSettings trailingArmLoadSettings = loadSettings;
    trailingArmLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle trailingArmVehicle = VehicleDefinitionLoader::create(
        trailingArmResult.definition,
        trailingArmLoadSettings,
        bodies,
        vehicles,
        trailingArmLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription trailingArmReadback;
    const bool trailingArmRuntimeReady = trailingArmResult.valid
        && trailingArmResult.currentSolverReady
        && trailingArmVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            trailingArmVehicle, 2, trailingArmReadback)
        && trailingArmReadback.provider
            == heritage::vehicles::SuspensionProviderKind::TrailingArmTorsionBarV1
        && trailingArmReadback.trailingArm.authored;

    VehicleDefinitionV2Source incompleteTrailingArm = trailingArmSource;
    incompleteTrailingArm.suspensions[2].hardpoints.pop_back();
    const auto incompleteTrailingArmResult =
        VehicleDefinitionCompiler::compile(incompleteTrailingArm);

    heritage::vehicles::SuspensionModelDescription suspensionDescription;
    suspensionDescription.springRateNPerM = 10000.0f;
    suspensionDescription.bumpDampingNsPerM = 1000.0f;
    suspensionDescription.reboundDampingNsPerM = 2000.0f;
    suspensionDescription.motionRatio = 0.5f;
    suspensionDescription.maximumForceN = 5000.0f;
    const auto bumpOutput = heritage::vehicles::evaluateSuspensionModel(
        suspensionDescription,
        { 0.10f, 0.20f });
    const auto reboundOutput = heritage::vehicles::evaluateSuspensionModel(
        suspensionDescription,
        { 0.10f, -0.20f });
    const bool suspensionForcesWorked =
        std::abs(bumpOutput.normalForceN - 300.0f) <= 0.0001f
        && std::abs(reboundOutput.normalForceN - 150.0f) <= 0.0001f;

    heritage::vehicles::SuspensionModelDescription nonlinearSuspension;
    nonlinearSuspension.springPreloadN = 100.0f;
    nonlinearSuspension.springRateNPerM = 1000.0f;
    nonlinearSuspension.springProgressionNPerM2 = 2000.0f;
    nonlinearSuspension.bumpDampingNsPerM = 100.0f;
    nonlinearSuspension.bumpHighSpeedDampingNsPerM = 50.0f;
    nonlinearSuspension.bumpDampingKneeVelocityMps = 0.10f;
    nonlinearSuspension.reboundDampingNsPerM = 200.0f;
    nonlinearSuspension.reboundHighSpeedDampingNsPerM = 100.0f;
    nonlinearSuspension.reboundDampingKneeVelocityMps = 0.10f;
    nonlinearSuspension.bumpStopEngagementM = 0.10f;
    nonlinearSuspension.bumpStopRateNPerM = 1000.0f;
    nonlinearSuspension.bumpStopProgressionNPerM2 = 2000.0f;
    nonlinearSuspension.droopStopEngagementM = 0.10f;
    nonlinearSuspension.droopStopRateNPerM = 500.0f;
    nonlinearSuspension.motionRatio = 1.0f;
    nonlinearSuspension.maximumForceN = 10000.0f;
    const auto nonlinearBump = heritage::vehicles::evaluateSuspensionModel(
        nonlinearSuspension,
        { 0.20f, 0.30f });
    const auto nonlinearRebound = heritage::vehicles::evaluateSuspensionModel(
        nonlinearSuspension,
        { -0.20f, -0.30f });
    const bool nonlinearSuspensionWorked =
        std::abs(nonlinearBump.springForceN - 340.0f) <= 0.0001f
        && std::abs(nonlinearBump.dampingForceN - 20.0f) <= 0.0001f
        && std::abs(nonlinearBump.bumpStopForceN - 110.0f) <= 0.0001f
        && std::abs(nonlinearBump.normalForceN - 470.0f) <= 0.0001f
        && std::abs(nonlinearBump.damperDissipationW - 6.0f) <= 0.0001f
        && std::abs(nonlinearRebound.droopStopForceN - 50.0f) <= 0.0001f
        && nonlinearRebound.normalForceN == 0.0f;

    heritage::vehicles::SuspensionModelDescription suspensionReadback;
    const bool liveSuspensionSet = vehicles.setWheelSuspensionModel(
        loaded, 0, nonlinearSuspension);
    const bool liveSuspensionRead = vehicles.wheelSuspensionModel(
        loaded, 0, suspensionReadback);
    heritage::vehicles::SuspensionModelDescription invalidLiveSuspension =
        nonlinearSuspension;
    invalidLiveSuspension.motionRatio = 0.0f;
    const bool invalidLiveSuspensionRejected =
        !vehicles.setWheelSuspensionModel(
            loaded, 0, invalidLiveSuspension);
    const bool liveSuspensionRoundTrip = liveSuspensionSet
        && liveSuspensionRead
        && std::abs(suspensionReadback.springPreloadN - 100.0f)
            <= 0.0001f
        && std::abs(suspensionReadback.bumpHighSpeedDampingNsPerM - 50.0f)
            <= 0.0001f
        && std::abs(suspensionReadback.bumpStopProgressionNPerM2 - 2000.0f)
            <= 0.0001f
        && invalidLiveSuspensionRejected;

    heritage::vehicles::UnsprungMassDescription unsprungReadback;
    heritage::vehicles::UnsprungMassDescription liveUnsprung;
    liveUnsprung.effectiveMassKg = 42.0f;
    liveUnsprung.tireRadialStiffnessNPerM = 240000.0f;
    liveUnsprung.tireRadialDampingNsPerM = 2000.0f;
    liveUnsprung.maximumTireDeflectionM = 0.09f;
    liveUnsprung.maximumNormalForceN = 275000.0f;
    const bool liveUnsprungSet = vehicles.setWheelUnsprungMassModel(
        loaded, 0, liveUnsprung);
    const bool liveUnsprungRead = vehicles.wheelUnsprungMassModel(
        loaded, 0, unsprungReadback);
    heritage::vehicles::UnsprungMassDescription invalidLiveUnsprung =
        liveUnsprung;
    invalidLiveUnsprung.tireRadialStiffnessNPerM = 0.0f;
    const bool invalidLiveUnsprungRejected =
        !vehicles.setWheelUnsprungMassModel(
            loaded, 0, invalidLiveUnsprung);
    const bool liveUnsprungRoundTrip = liveUnsprungSet
        && liveUnsprungRead
        && std::abs(unsprungReadback.effectiveMassKg - 42.0f) <= 0.0001f
        && std::abs(
            unsprungReadback.tireRadialStiffnessNPerM - 240000.0f)
            <= 0.0001f
        && std::abs(unsprungReadback.maximumTireDeflectionM - 0.09f)
            <= 0.0001f
        && invalidLiveUnsprungRejected;

    heritage::vehicles::SuspensionGeometryDescription geometryReadback;
    heritage::vehicles::SuspensionGeometryDescription liveGeometry;
    liveGeometry.localSteeringAxis = { 0.10f, 0.98f, -0.12f };
    liveGeometry.staticCamberDegrees = -1.25f;
    liveGeometry.camberGainDegreesPerM = -8.0f;
    liveGeometry.camberProgressionDegreesPerM2 = 25.0f;
    liveGeometry.staticToeDegrees = 0.15f;
    liveGeometry.toeGainDegreesPerM = 2.5f;
    liveGeometry.toeProgressionDegreesPerM2 = -12.0f;
    const bool liveGeometrySet = vehicles.setWheelSuspensionGeometry(
        loaded, 0, liveGeometry);
    const bool liveGeometryRead = vehicles.wheelSuspensionGeometry(
        loaded, 0, geometryReadback);
    heritage::vehicles::SuspensionGeometryDescription invalidLiveGeometry =
        liveGeometry;
    invalidLiveGeometry.localSteeringAxis = {};
    const bool invalidLiveGeometryRejected =
        !vehicles.setWheelSuspensionGeometry(
            loaded, 0, invalidLiveGeometry);
    const bool liveGeometryRoundTrip = liveGeometrySet
        && liveGeometryRead
        && std::abs(geometryReadback.staticCamberDegrees + 1.25f)
            <= 0.0001f
        && std::abs(geometryReadback.camberGainDegreesPerM + 8.0f)
            <= 0.0001f
        && std::abs(geometryReadback.toeProgressionDegreesPerM2 + 12.0f)
            <= 0.0001f
        && std::abs(magnitude(geometryReadback.localSteeringAxis) - 1.0f)
            <= 0.0001f
        && invalidLiveGeometryRejected;

    std::cout
        << "definition_compiler provider="
        << compiled.definition.runtimeProvider
        << " wheels=" << vehicles.wheelCount(loaded)
        << " gears=" << vehicles.forwardGearCount(loaded)
        << " invalid_reference_rejected=" << (!brokenResult.valid)
        << " suspension_reference_rejected=" << (!brokenSuspensionResult.valid)
        << " suspension_parameters_rejected="
        << (!invalidSuspensionParametersResult.valid)
        << " unsprung_parameters_rejected="
        << (!invalidUnsprungParametersResult.valid)
        << " hardpoint_provenance_rejected="
        << (!invalidHardpointProvenanceResult.valid)
        << " hardpoint_confidence_rejected="
        << (!invalidHardpointConfidenceResult.valid)
        << " geometry_parameters_rejected="
        << (!invalidGeometryParametersResult.valid)
        << " anti_roll_bar_loaded=" << (vehicles.antiRollBarCount(loaded) == 2)
        << " anti_roll_bar_reference_rejected="
        << (!invalidAntiRollBarReferenceResult.valid)
        << " anti_roll_bar_topology_rejected="
        << (!invalidAntiRollBarTopologyResult.valid)
        << " anti_roll_bar_parameters_rejected="
        << (!invalidAntiRollBarParametersResult.valid)
        << " chassis_flex_loaded=" << loadedFlex.enabled
        << " chassis_flex_reference_rejected="
        << (!invalidChassisFlexReferenceResult.valid)
        << " chassis_flex_provider_rejected="
        << (!invalidChassisFlexProviderResult.valid)
        << " chassis_flex_parameters_rejected="
        << (!invalidChassisFlexParametersResult.valid)
        << " future_topology_valid=" << futureResult.valid
        << " future_topology_ready=" << futureResult.currentSolverReady
        << " category_ignored=" << categoryOnlyResult.currentSolverReady
        << " future_suspension_ready="
        << futureSuspensionResult.currentSolverReady
        << " macpherson_valid=" << macPhersonResult.valid
        << " macpherson_solver_ready=" << macPhersonResult.currentSolverReady
        << " macpherson_handle=" << (macPhersonVehicle != heritage::vehicles::InvalidVehicle)
        << " macpherson_load_msg=" << macPhersonLoadMessage
        << " macpherson_ready=" << macPhersonRuntimeReady
        << " incomplete_macpherson_rejected="
        << (!incompleteMacPhersonResult.valid)
        << " trailing_arm_valid=" << trailingArmResult.valid
        << " trailing_arm_solver_ready=" << trailingArmResult.currentSolverReady
        << " trailing_arm_handle="
        << (trailingArmVehicle != heritage::vehicles::InvalidVehicle)
        << " trailing_arm_load_msg=" << trailingArmLoadMessage
        << " trailing_arm_ready=" << trailingArmRuntimeReady
        << " incomplete_trailing_arm_rejected="
        << (!incompleteTrailingArmResult.valid)
        << " motion_ratio_force_n=" << bumpOutput.normalForceN
        << " nonlinear_force_n=" << nonlinearBump.normalForceN
        << " damper_power_w=" << nonlinearBump.damperDissipationW
        << " live_suspension_roundtrip=" << liveSuspensionRoundTrip
        << " live_unsprung_roundtrip=" << liveUnsprungRoundTrip
        << " live_geometry_roundtrip=" << liveGeometryRoundTrip
        << '\n';

    return compiled.valid
        && compiled.currentSolverReady
        && compiled.definition.runtimeProvider == "raycast_wheel_v1"
        && referencesResolved
        && runtimeLoaded
        && !brokenResult.valid
        && !brokenSuspensionResult.valid
        && !invalidSuspensionParametersResult.valid
        && !invalidUnsprungParametersResult.valid
        && !invalidGeometryParametersResult.valid
        && !invalidAntiRollBarReferenceResult.valid
        && !invalidAntiRollBarTopologyResult.valid
        && !invalidAntiRollBarParametersResult.valid
        && !invalidChassisFlexReferenceResult.valid
        && !invalidChassisFlexProviderResult.valid
        && !invalidChassisFlexParametersResult.valid
        && !duplicateHardpointResult.valid
        && !invalidHardpointPositionResult.valid
        && !invalidHardpointProvenanceResult.valid
        && !invalidHardpointConfidenceResult.valid
        && futureResult.valid
        && !futureResult.currentSolverReady
        && futureResult.issueSummary().find("lean_dynamics") != std::string::npos
        && categoryOnlyResult.currentSolverReady
        && futureSuspensionResult.valid
        && !futureSuspensionResult.currentSolverReady
        && futureSuspensionResult.issueSummary().find("double_wishbone_v1")
            != std::string::npos
        && macPhersonRuntimeReady
        && !incompleteMacPhersonResult.valid
        && incompleteMacPhersonResult.issueSummary().find(
            "eight named hardpoints") != std::string::npos
        && trailingArmRuntimeReady
        && !incompleteTrailingArmResult.valid
        && incompleteTrailingArmResult.issueSummary().find(
            "five named hardpoints") != std::string::npos
        && suspensionForcesWorked
        && nonlinearSuspensionWorked
        && liveSuspensionRoundTrip
        && liveUnsprungRoundTrip
        && liveGeometryRoundTrip;
}

} // namespace heritage::tests
