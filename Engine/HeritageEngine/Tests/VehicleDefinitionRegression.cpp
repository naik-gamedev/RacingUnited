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

void authorDoubleWishboneHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter,
    bool leftSide)
{
    const float side = leftSide ? -1.0f : 1.0f;
    const auto point = [&](float lateral, float vertical, float longitudinal) {
        return Vec3{
            wheelCenter.x + side * lateral,
            wheelCenter.y + vertical,
            wheelCenter.z + longitudinal
        };
    };
    suspension.hardpoints = {
        { "upper_arm_inner_front", point(-0.39f, 0.29f, 0.25f), "measured", 1.0f },
        { "upper_arm_inner_rear", point(-0.39f, 0.29f, -0.25f), "measured", 1.0f },
        { "upper_ball_joint", point(-0.025f, 0.22f, 0.0f), "measured", 1.0f },
        { "lower_arm_inner_front", point(-0.48f, -0.10f, 0.25f), "measured", 1.0f },
        { "lower_arm_inner_rear", point(-0.48f, -0.10f, -0.25f), "measured", 1.0f },
        { "lower_ball_joint", point(0.045f, -0.14f, 0.0f), "measured", 1.0f },
        { "tie_rod_inner", point(-0.42f, 0.02f, -0.12f), "measured", 1.0f },
        { "tie_rod_outer", point(-0.045f, -0.05f, -0.07f), "measured", 1.0f },
        { "wheel_center", wheelCenter, "measured", 1.0f },
        { "damper_upper_mount", point(-0.27f, 0.67f, 0.0f), "measured", 1.0f },
        { "damper_lower_mount", point(-0.25f, -0.08f, 0.0f), "measured", 1.0f }
    };
}

void authorPushrodDoubleWishboneHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter,
    bool leftSide)
{
    authorDoubleWishboneHardpoints(suspension, wheelCenter, leftSide);
    suspension.hardpoints.erase(
        std::remove_if(
            suspension.hardpoints.begin(),
            suspension.hardpoints.end(),
            [](const auto& hardpoint) {
                return hardpoint.id == "damper_upper_mount"
                    || hardpoint.id == "damper_lower_mount";
            }),
        suspension.hardpoints.end());

    const float side = leftSide ? -1.0f : 1.0f;
    const auto point = [&](float lateral, float vertical, float longitudinal) {
        return Vec3{
            wheelCenter.x + side * lateral,
            wheelCenter.y + vertical,
            wheelCenter.z + longitudinal
        };
    };
    const auto add = [&](const char* id, const Vec3& position) {
        suspension.hardpoints.push_back({ id, position, "measured", 1.0f });
    };
    add("pushrod_lower_arm_mount", point(-0.135f, -0.12f, 0.0f));
    add("rocker_pivot_front", point(-0.355f, 0.45f, 0.12f));
    add("rocker_pivot_rear", point(-0.355f, 0.45f, -0.12f));
    add("rocker_pushrod_mount", point(-0.215f, 0.45f, 0.0f));
    add("spring_chassis_mount", point(-0.555f, 0.59f, 0.0f));
    add("spring_rocker_mount", point(-0.355f, 0.59f, 0.0f));
    add("damper_chassis_mount", point(-0.555f, 0.55f, 0.0f));
    add("damper_rocker_mount", point(-0.355f, 0.55f, 0.0f));
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


void authorSemiTrailingArmHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter,
    bool leftSide,
    const std::string& prefix = {})
{
    const float side = leftSide ? -1.0f : 1.0f;
    const auto point = [&](float inboard, float vertical, float longitudinal) {
        return Vec3{ wheelCenter.x + inboard * side,
            wheelCenter.y + vertical, wheelCenter.z + longitudinal };
    };
    const auto add = [&](const std::string& id, const Vec3& position) {
        suspension.hardpoints.push_back({ prefix + id, position, "measured", 1.0f });
    };
    add("arm_pivot_inner", point(-0.56f, 0.34f, 0.33f));
    add("arm_pivot_outer", point(-0.14f, 0.35f, 0.20f));
    add("wheel_center", wheelCenter);
    add("spring_upper_mount", point(-0.30f, 0.68f, 0.23f));
    add("spring_lower_mount", point(-0.16f, 0.16f, 0.10f));
    add("damper_upper_mount", point(-0.24f, 0.70f, 0.32f));
    add("damper_lower_mount", point(-0.12f, 0.14f, 0.05f));
}

void authorTwistBeamHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& leftWheelCenter,
    const Vec3& rightWheelCenter)
{
    suspension.hardpoints.clear();
    authorSemiTrailingArmHardpoints(suspension,leftWheelCenter,true,"left_");
    authorSemiTrailingArmHardpoints(suspension,rightWheelCenter,false,"right_");
    suspension.hardpoints.push_back({"beam_left_attachment",
        {leftWheelCenter.x+0.28f,leftWheelCenter.y+0.26f,leftWheelCenter.z+0.18f},"measured",1.0f});
    suspension.hardpoints.push_back({"beam_right_attachment",
        {rightWheelCenter.x-0.28f,rightWheelCenter.y+0.26f,rightWheelCenter.z+0.18f},"measured",1.0f});
}

void authorMotorcycleForkHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter)
{
    suspension.hardpoints = {
        { "steering_stem_upper", { wheelCenter.x, 0.95f, wheelCenter.z - 0.53f }, "measured", 1.0f },
        { "steering_stem_lower", { wheelCenter.x, 0.60f, wheelCenter.z - 0.70f }, "measured", 1.0f },
        { "wheel_center", wheelCenter, "measured", 1.0f }
    };
}

void authorMotorcycleSwingarmHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& wheelCenter)
{
    const float dx = wheelCenter.x;
    const float dz = wheelCenter.z + 1.48f;
    suspension.hardpoints = {
        { "swingarm_pivot_left", { dx - 0.18f, 0.50f, -0.25f + dz }, "measured", 1.0f },
        { "swingarm_pivot_right", { dx + 0.18f, 0.50f, -0.25f + dz }, "measured", 1.0f },
        { "wheel_center", wheelCenter, "measured", 1.0f },
        { "linkage_swingarm_mount", { dx, 0.30f, -1.05f + dz }, "measured", 1.0f },
        { "rocker_pivot_left", { dx - 0.06f, 0.62f, -0.48f + dz }, "measured", 1.0f },
        { "rocker_pivot_right", { dx + 0.06f, 0.62f, -0.48f + dz }, "measured", 1.0f },
        { "rocker_link_mount", { dx, 0.52f, -0.75f + dz }, "measured", 1.0f },
        { "shock_chassis_mount", { dx, 0.75f, -0.40f + dz }, "measured", 1.0f },
        { "shock_rocker_mount", { dx, 0.76f, -0.72f + dz }, "measured", 1.0f },
        { "countershaft_center", { dx, 0.54f, -0.18f + dz }, "measured", 1.0f }
    };
    suspension.motorcycleRearSprocketPitchRadiusM = 0.105f;
}

void authorKartChassisHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    const Vec3& frontLeft,
    const Vec3& frontRight,
    const Vec3& rearLeft,
    const Vec3& rearRight)
{
    suspension.hardpoints.clear();
    const auto add = [&](const char* id, const Vec3& position) {
        suspension.hardpoints.push_back({ id, position, "measured", 1.0f });
    };
    add("front_left_kingpin_upper",
        { frontLeft.x + 0.12f, frontLeft.y + 0.26f, frontLeft.z - 0.06f });
    add("front_left_kingpin_lower",
        { frontLeft.x + 0.07f, frontLeft.y - 0.07f, frontLeft.z - 0.02f });
    add("front_left_wheel_center", frontLeft);
    add("front_right_kingpin_upper",
        { frontRight.x - 0.12f, frontRight.y + 0.26f, frontRight.z - 0.06f });
    add("front_right_kingpin_lower",
        { frontRight.x - 0.07f, frontRight.y - 0.07f, frontRight.z - 0.02f });
    add("front_right_wheel_center", frontRight);
    add("rear_axle_bearing_left", { -0.34f, rearLeft.y, rearLeft.z });
    add("rear_axle_bearing_right", { 0.34f, rearRight.y, rearRight.z });
    add("rear_left_wheel_center", rearLeft);
    add("rear_right_wheel_center", rearRight);
}

void authorLiveAxleHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    float axleZ)
{
    const float wheelY = 0.30f;
    const auto add = [&](const char* id, const Vec3& position) {
        suspension.hardpoints.push_back({ id, position, "measured", 1.0f });
    };
    add("axle_center", { 0.0f, wheelY, axleZ });
    add("left_wheel_center", { -0.714f, wheelY, axleZ });
    add("right_wheel_center", { 0.714f, wheelY, axleZ });
    add("panhard_chassis_mount", { -0.55f, 0.58f, axleZ + 0.04f });
    add("panhard_axle_mount", { 0.55f, wheelY, axleZ + 0.04f });
    add("left_trailing_chassis_mount", { -0.45f, 0.42f, axleZ + 0.92f });
    add("left_trailing_axle_mount", { -0.45f, 0.27f, axleZ + 0.05f });
    add("right_trailing_chassis_mount", { 0.45f, 0.42f, axleZ + 0.92f });
    add("right_trailing_axle_mount", { 0.45f, 0.27f, axleZ + 0.05f });
    add("left_spring_chassis_mount", { -0.50f, 0.78f, axleZ });
    add("left_spring_axle_mount", { -0.50f, wheelY, axleZ });
    add("right_spring_chassis_mount", { 0.50f, 0.78f, axleZ });
    add("right_spring_axle_mount", { 0.50f, wheelY, axleZ });
    add("left_damper_chassis_mount", { -0.58f, 0.82f, axleZ + 0.19f });
    add("left_damper_axle_mount", { -0.48f, 0.28f, axleZ - 0.06f });
    add("right_damper_chassis_mount", { 0.58f, 0.82f, axleZ + 0.19f });
    add("right_damper_axle_mount", { 0.48f, 0.28f, axleZ - 0.06f });
}


void authorLeafSpringHardpoints(
    heritage::vehicles::VehicleSuspensionDefinition& suspension,
    float axleZ)
{
    const auto add = [&](const char* id, const Vec3& position) {
        suspension.hardpoints.push_back({ id, position, "measured", 1.0f });
    };
    add("left_leaf_front_eye", { -0.50f, 0.55f, axleZ + 0.871f });
    add("left_leaf_rear_shackle_pivot", { -0.50f, 0.72f, axleZ - 0.859f });
    add("left_leaf_rear_eye", { -0.50f, 0.54f, axleZ - 0.799f });
    add("left_leaf_axle_clamp", { -0.50f, 0.30f, axleZ });
    add("right_leaf_front_eye", { 0.50f, 0.55f, axleZ + 0.871f });
    add("right_leaf_rear_shackle_pivot", { 0.50f, 0.72f, axleZ - 0.859f });
    add("right_leaf_rear_eye", { 0.50f, 0.54f, axleZ - 0.799f });
    add("right_leaf_axle_clamp", { 0.50f, 0.30f, axleZ });
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
    futureSuspension.suspensions[0].provider = "pullrod_double_wishbone_v1";
    const auto futureSuspensionResult =
        VehicleDefinitionCompiler::compile(futureSuspension);

    VehicleDefinitionV2Source doubleWishboneSource = source;
    for (std::size_t index = 0; index < 4; ++index)
    {
        doubleWishboneSource.suspensions[index].provider = "double_wishbone_v1";
        const auto& contact = doubleWishboneSource.contactUnits[index];
        const auto& suspension = doubleWishboneSource.suspensions[index];
        const Vec3 wheelCenter{
            contact.localMount.x
                + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y
                + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z
                + contact.suspensionDirection.z * suspension.restLengthM
        };
        authorDoubleWishboneHardpoints(
            doubleWishboneSource.suspensions[index],
            wheelCenter,
            index == 0 || index == 2);
    }
    const auto doubleWishboneResult =
        VehicleDefinitionCompiler::compile(doubleWishboneSource);
    std::string doubleWishboneLoadMessage;
    VehicleDefinitionLoadSettings doubleWishboneLoadSettings = loadSettings;
    doubleWishboneLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle doubleWishboneVehicle = VehicleDefinitionLoader::create(
        doubleWishboneResult.definition,
        doubleWishboneLoadSettings,
        bodies,
        vehicles,
        doubleWishboneLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription doubleWishboneReadback;
    const bool doubleWishboneRuntimeReady = doubleWishboneResult.valid
        && doubleWishboneResult.currentSolverReady
        && doubleWishboneVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            doubleWishboneVehicle, 0, doubleWishboneReadback)
        && doubleWishboneReadback.provider
            == heritage::vehicles::SuspensionProviderKind::DoubleWishboneV1
        && doubleWishboneReadback.doubleWishbone.authored;

    VehicleDefinitionV2Source incompleteDoubleWishbone = doubleWishboneSource;
    incompleteDoubleWishbone.suspensions[0].hardpoints.pop_back();
    const auto incompleteDoubleWishboneResult =
        VehicleDefinitionCompiler::compile(incompleteDoubleWishbone);

    VehicleDefinitionV2Source pushrodSource = source;
    for (std::size_t index = 0; index < 4; ++index)
    {
        pushrodSource.suspensions[index].provider =
            "pushrod_double_wishbone_v1";
        const auto& contact = pushrodSource.contactUnits[index];
        const auto& suspension = pushrodSource.suspensions[index];
        const Vec3 wheelCenter{
            contact.localMount.x
                + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y
                + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z
                + contact.suspensionDirection.z * suspension.restLengthM
        };
        authorPushrodDoubleWishboneHardpoints(
            pushrodSource.suspensions[index],
            wheelCenter,
            index == 0 || index == 2);
    }
    const auto pushrodResult = VehicleDefinitionCompiler::compile(pushrodSource);
    std::string pushrodLoadMessage;
    VehicleDefinitionLoadSettings pushrodLoadSettings = loadSettings;
    pushrodLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle pushrodVehicle = VehicleDefinitionLoader::create(
        pushrodResult.definition,
        pushrodLoadSettings,
        bodies,
        vehicles,
        pushrodLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription pushrodReadback;
    const bool pushrodRuntimeReady = pushrodResult.valid
        && pushrodResult.currentSolverReady
        && pushrodVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            pushrodVehicle, 0, pushrodReadback)
        && pushrodReadback.provider
            == heritage::vehicles::SuspensionProviderKind::PushrodDoubleWishboneV1
        && pushrodReadback.pushrodDoubleWishbone.authored;

    VehicleDefinitionV2Source incompletePushrod = pushrodSource;
    incompletePushrod.suspensions[0].hardpoints.pop_back();
    const auto incompletePushrodResult =
        VehicleDefinitionCompiler::compile(incompletePushrod);

    VehicleDefinitionV2Source liveAxleSource = source;
    for (std::size_t index = 2; index < 4; ++index)
    {
        liveAxleSource.suspensions[index].provider = "live_axle_v1";
        liveAxleSource.suspensions[index].hardpoints.clear();
        authorLiveAxleHardpoints(
            liveAxleSource.suspensions[index],
            liveAxleSource.contactUnits[index].localMount.z);
    }
    const auto liveAxleResult = VehicleDefinitionCompiler::compile(liveAxleSource);
    std::string liveAxleLoadMessage;
    VehicleDefinitionLoadSettings liveAxleLoadSettings = loadSettings;
    liveAxleLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle liveAxleVehicle = VehicleDefinitionLoader::create(
        liveAxleResult.definition,
        liveAxleLoadSettings,
        bodies,
        vehicles,
        liveAxleLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription liveAxleReadback;
    const bool liveAxleRuntimeReady = liveAxleResult.valid
        && liveAxleResult.currentSolverReady
        && liveAxleVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            liveAxleVehicle, 2, liveAxleReadback)
        && liveAxleReadback.provider
            == heritage::vehicles::SuspensionProviderKind::LiveAxleV1
        && liveAxleReadback.liveAxle.authored;
    VehicleDefinitionV2Source incompleteLiveAxle = liveAxleSource;
    incompleteLiveAxle.suspensions[2].hardpoints.pop_back();
    const auto incompleteLiveAxleResult =
        VehicleDefinitionCompiler::compile(incompleteLiveAxle);

    VehicleDefinitionV2Source leafAxleSource = liveAxleSource;
    for (std::size_t index = 2; index < 4; ++index)
    {
        leafAxleSource.suspensions[index].provider = "live_axle_leaf_v1";
        authorLeafSpringHardpoints(
            leafAxleSource.suspensions[index],
            leafAxleSource.contactUnits[index].localMount.z);
    }
    const auto leafAxleResult = VehicleDefinitionCompiler::compile(leafAxleSource);
    std::string leafAxleLoadMessage;
    VehicleDefinitionLoadSettings leafAxleLoadSettings = loadSettings;
    leafAxleLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle leafAxleVehicle = VehicleDefinitionLoader::create(
        leafAxleResult.definition,
        leafAxleLoadSettings,
        bodies,
        vehicles,
        leafAxleLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription leafAxleReadback;
    const bool leafAxleRuntimeReady = leafAxleResult.valid
        && leafAxleResult.currentSolverReady
        && leafAxleVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            leafAxleVehicle, 2, leafAxleReadback)
        && leafAxleReadback.provider
            == heritage::vehicles::SuspensionProviderKind::LeafSpringLiveAxleV1
        && leafAxleReadback.leafSpringLiveAxle.authored;
    VehicleDefinitionV2Source incompleteLeafAxle = leafAxleSource;
    incompleteLeafAxle.suspensions[2].hardpoints.pop_back();
    const auto incompleteLeafAxleResult =
        VehicleDefinitionCompiler::compile(incompleteLeafAxle);

    VehicleDefinitionV2Source motorcycleSuspensionSource = source;
    for (std::size_t index = 0; index < 4; ++index)
    {
        const auto& contact = motorcycleSuspensionSource.contactUnits[index];
        const auto& suspension = motorcycleSuspensionSource.suspensions[index];
        const Vec3 wheelCenter{
            contact.localMount.x + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z + contact.suspensionDirection.z * suspension.restLengthM };
        if (index < 2)
        {
            motorcycleSuspensionSource.suspensions[index].provider =
                "motorcycle_telescopic_fork_v1";
            authorMotorcycleForkHardpoints(
                motorcycleSuspensionSource.suspensions[index], wheelCenter);
        }
        else
        {
            motorcycleSuspensionSource.suspensions[index].provider =
                "motorcycle_swingarm_linkage_v1";
            authorMotorcycleSwingarmHardpoints(
                motorcycleSuspensionSource.suspensions[index], wheelCenter);
        }
    }
    const auto motorcycleSuspensionResult =
        VehicleDefinitionCompiler::compile(motorcycleSuspensionSource);
    std::string motorcycleSuspensionLoadMessage;
    VehicleDefinitionLoadSettings motorcycleSuspensionLoadSettings = loadSettings;
    motorcycleSuspensionLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle motorcycleSuspensionVehicle = VehicleDefinitionLoader::create(
        motorcycleSuspensionResult.definition,
        motorcycleSuspensionLoadSettings,
        bodies,
        vehicles,
        motorcycleSuspensionLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription motorcycleForkReadback;
    heritage::vehicles::SuspensionGeometryDescription motorcycleRearReadback;
    const bool motorcycleSuspensionRuntimeReady = motorcycleSuspensionResult.valid
        && motorcycleSuspensionResult.currentSolverReady
        && motorcycleSuspensionVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(
            motorcycleSuspensionVehicle, 0, motorcycleForkReadback)
        && vehicles.wheelSuspensionGeometry(
            motorcycleSuspensionVehicle, 2, motorcycleRearReadback)
        && motorcycleForkReadback.provider
            == heritage::vehicles::SuspensionProviderKind::MotorcycleTelescopicForkV1
        && motorcycleForkReadback.motorcycleFork.authored
        && motorcycleRearReadback.provider
            == heritage::vehicles::SuspensionProviderKind::MotorcycleSwingarmLinkageV1
        && motorcycleRearReadback.motorcycleSwingarm.authored;
    VehicleDefinitionV2Source incompleteMotorcycleFork = motorcycleSuspensionSource;
    incompleteMotorcycleFork.suspensions[0].hardpoints.pop_back();
    const auto incompleteMotorcycleForkResult =
        VehicleDefinitionCompiler::compile(incompleteMotorcycleFork);
    VehicleDefinitionV2Source incompleteMotorcycleRear = motorcycleSuspensionSource;
    incompleteMotorcycleRear.suspensions[2].hardpoints.pop_back();
    const auto incompleteMotorcycleRearResult =
        VehicleDefinitionCompiler::compile(incompleteMotorcycleRear);

    VehicleDefinitionV2Source kartSource = source;
    const auto kartWheelCenter = [&](std::size_t index) {
        const auto& contact = kartSource.contactUnits[index];
        const auto& suspension = kartSource.suspensions[index];
        return Vec3{
            contact.localMount.x
                + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y
                + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z
                + contact.suspensionDirection.z * suspension.restLengthM
        };
    };
    const Vec3 kartFrontLeft = kartWheelCenter(0);
    const Vec3 kartFrontRight = kartWheelCenter(1);
    const Vec3 kartRearLeft = kartWheelCenter(2);
    const Vec3 kartRearRight = kartWheelCenter(3);
    for (std::size_t index = 0; index < 4; ++index)
    {
        auto& suspension = kartSource.suspensions[index];
        suspension.provider = "kart_chassis_flex_v1";
        suspension.maximumCompressionM = 0.0f;
        suspension.maximumDroopM = 0.0f;
        suspension.springPreloadN = 0.0f;
        suspension.springRateNPerM = 0.0f;
        suspension.springProgressionNPerM2 = 0.0f;
        suspension.bumpDampingNsPerM = 0.0f;
        suspension.bumpHighSpeedDampingNsPerM = 0.0f;
        suspension.reboundDampingNsPerM = 0.0f;
        suspension.reboundHighSpeedDampingNsPerM = 0.0f;
        suspension.bumpStopRateNPerM = 0.0f;
        suspension.droopStopRateNPerM = 0.0f;
        authorKartChassisHardpoints(
            suspension, kartFrontLeft, kartFrontRight,
            kartRearLeft, kartRearRight);
    }
    kartSource.chassisFlex.enabled = true;
    kartSource.chassisFlex.provider = "chassis_torsional_mode_v1";
    kartSource.chassisFlex.torsionalRigidityNmPerDegree = 1200.0f;
    kartSource.chassisFlex.torsionalDampingNmsPerRad = 550.0f;
    kartSource.chassisFlex.effectiveTorsionalInertiaKgM2 = 35.0f;
    kartSource.chassisFlex.maximumTwistDegrees = 4.0f;
    const auto kartResult = VehicleDefinitionCompiler::compile(kartSource);
    std::string kartLoadMessage;
    VehicleDefinitionLoadSettings kartLoadSettings = loadSettings;
    kartLoadSettings.vehicle.chassisBody = bodies.create(bodyDescription);
    const VehicleHandle kartVehicle = VehicleDefinitionLoader::create(
        kartResult.definition, kartLoadSettings, bodies, vehicles, kartLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription kartReadback;
    const bool kartRuntimeReady = kartResult.valid
        && kartResult.currentSolverReady
        && kartVehicle != heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(kartVehicle, 0, kartReadback)
        && kartReadback.provider
            == heritage::vehicles::SuspensionProviderKind::KartChassisFlexV1
        && kartReadback.kartChassis.authored;

    VehicleDefinitionV2Source incompleteKart = kartSource;
    incompleteKart.suspensions[0].hardpoints.pop_back();
    const auto incompleteKartResult =
        VehicleDefinitionCompiler::compile(incompleteKart);
    VehicleDefinitionV2Source kartWithFakeTravel = kartSource;
    kartWithFakeTravel.suspensions[0].maximumCompressionM = 0.01f;
    const auto kartWithFakeTravelResult =
        VehicleDefinitionCompiler::compile(kartWithFakeTravel);
    VehicleDefinitionV2Source kartWithoutFlex = kartSource;
    kartWithoutFlex.chassisFlex.enabled = false;
    const auto kartWithoutFlexResult =
        VehicleDefinitionCompiler::compile(kartWithoutFlex);

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

    VehicleDefinitionV2Source semiTrailingSource = macPhersonSource;
    for (std::size_t index = 2; index < 4; ++index)
    {
        auto& suspension = semiTrailingSource.suspensions[index];
        suspension.provider = "semi_trailing_arm_v1";
        suspension.hardpoints.clear();
        const auto& contact = semiTrailingSource.contactUnits[index];
        const Vec3 wheelCenter{
            contact.localMount.x + contact.suspensionDirection.x * suspension.restLengthM,
            contact.localMount.y + contact.suspensionDirection.y * suspension.restLengthM,
            contact.localMount.z + contact.suspensionDirection.z * suspension.restLengthM };
        authorSemiTrailingArmHardpoints(suspension,wheelCenter,index==2);
    }
    const auto semiTrailingResult=VehicleDefinitionCompiler::compile(semiTrailingSource);
    VehicleDefinitionLoadSettings semiTrailingLoadSettings=loadSettings;
    semiTrailingLoadSettings.vehicle.chassisBody=bodies.create(bodyDescription);
    std::string semiTrailingLoadMessage;
    const VehicleHandle semiTrailingVehicle=VehicleDefinitionLoader::create(
        semiTrailingResult.definition,semiTrailingLoadSettings,bodies,vehicles,semiTrailingLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription semiTrailingReadback;
    const bool semiTrailingRuntimeReady=semiTrailingResult.valid
        && semiTrailingResult.currentSolverReady
        && semiTrailingVehicle!=heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(semiTrailingVehicle,2,semiTrailingReadback)
        && semiTrailingReadback.provider==heritage::vehicles::SuspensionProviderKind::SemiTrailingArmV1
        && semiTrailingReadback.semiTrailingArm.authored;
    VehicleDefinitionV2Source incompleteSemiTrailing=semiTrailingSource;
    incompleteSemiTrailing.suspensions[2].hardpoints.pop_back();
    const auto incompleteSemiTrailingResult=VehicleDefinitionCompiler::compile(incompleteSemiTrailing);

    VehicleDefinitionV2Source twistBeamSource = macPhersonSource;
    const auto rearCenter = [&](std::size_t index) {
        const auto& contact=twistBeamSource.contactUnits[index];
        const auto& suspension=twistBeamSource.suspensions[index];
        return Vec3{contact.localMount.x+contact.suspensionDirection.x*suspension.restLengthM,
            contact.localMount.y+contact.suspensionDirection.y*suspension.restLengthM,
            contact.localMount.z+contact.suspensionDirection.z*suspension.restLengthM};
    };
    const Vec3 rearLeftCenter=rearCenter(2),rearRightCenter=rearCenter(3);
    for(std::size_t index=2;index<4;++index)
    {
        auto& suspension=twistBeamSource.suspensions[index];
        suspension.provider="twist_beam_v1";
        suspension.twistBeamTorsionalStiffnessNmPerRad=3500.0f;
        suspension.twistBeamTorsionalDampingNmsPerRad=180.0f;
        authorTwistBeamHardpoints(suspension,rearLeftCenter,rearRightCenter);
    }
    const auto twistBeamResult=VehicleDefinitionCompiler::compile(twistBeamSource);
    VehicleDefinitionLoadSettings twistBeamLoadSettings=loadSettings;
    twistBeamLoadSettings.vehicle.chassisBody=bodies.create(bodyDescription);
    std::string twistBeamLoadMessage;
    const VehicleHandle twistBeamVehicle=VehicleDefinitionLoader::create(
        twistBeamResult.definition,twistBeamLoadSettings,bodies,vehicles,twistBeamLoadMessage);
    heritage::vehicles::SuspensionGeometryDescription twistBeamReadback;
    heritage::vehicles::SuspensionModelDescription twistBeamModelReadback;
    const bool twistBeamRuntimeReady=twistBeamResult.valid
        && twistBeamResult.currentSolverReady
        && twistBeamVehicle!=heritage::vehicles::InvalidVehicle
        && vehicles.wheelSuspensionGeometry(twistBeamVehicle,2,twistBeamReadback)
        && vehicles.wheelSuspensionModel(twistBeamVehicle,2,twistBeamModelReadback)
        && twistBeamReadback.provider==heritage::vehicles::SuspensionProviderKind::TwistBeamV1
        && twistBeamReadback.twistBeam.authored
        && std::abs(twistBeamModelReadback.twistBeamTorsionalStiffnessNmPerRad-3500.0)<=0.001;
    VehicleDefinitionV2Source incompleteTwistBeam=twistBeamSource;
    incompleteTwistBeam.suspensions[2].hardpoints.pop_back();
    const auto incompleteTwistBeamResult=VehicleDefinitionCompiler::compile(incompleteTwistBeam);

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
        << " double_wishbone_valid=" << doubleWishboneResult.valid
        << " double_wishbone_solver_ready="
        << doubleWishboneResult.currentSolverReady
        << " double_wishbone_handle="
        << (doubleWishboneVehicle != heritage::vehicles::InvalidVehicle)
        << " double_wishbone_ready=" << doubleWishboneRuntimeReady
        << " incomplete_double_wishbone_rejected="
        << (!incompleteDoubleWishboneResult.valid)
        << " pushrod_valid=" << pushrodResult.valid
        << " pushrod_solver_ready=" << pushrodResult.currentSolverReady
        << " pushrod_handle="
        << (pushrodVehicle != heritage::vehicles::InvalidVehicle)
        << " pushrod_ready=" << pushrodRuntimeReady
        << " incomplete_pushrod_rejected="
        << (!incompletePushrodResult.valid)
        << " live_axle_valid=" << liveAxleResult.valid
        << " live_axle_solver_ready=" << liveAxleResult.currentSolverReady
        << " live_axle_handle="
        << (liveAxleVehicle != heritage::vehicles::InvalidVehicle)
        << " live_axle_ready=" << liveAxleRuntimeReady
        << " incomplete_live_axle_rejected="
        << (!incompleteLiveAxleResult.valid)
        << " leaf_axle_valid=" << leafAxleResult.valid
        << " leaf_axle_solver_ready=" << leafAxleResult.currentSolverReady
        << " leaf_axle_handle="
        << (leafAxleVehicle != heritage::vehicles::InvalidVehicle)
        << " leaf_axle_ready=" << leafAxleRuntimeReady
        << " incomplete_leaf_axle_rejected="
        << (!incompleteLeafAxleResult.valid)
        << " motorcycle_suspension_valid=" << motorcycleSuspensionResult.valid
        << " motorcycle_suspension_ready=" << motorcycleSuspensionRuntimeReady
        << " incomplete_motorcycle_fork_rejected="
        << (!incompleteMotorcycleForkResult.valid)
        << " incomplete_motorcycle_rear_rejected="
        << (!incompleteMotorcycleRearResult.valid)
        << " kart_valid=" << kartResult.valid
        << " kart_solver_ready=" << kartResult.currentSolverReady
        << " kart_handle=" << (kartVehicle != heritage::vehicles::InvalidVehicle)
        << " kart_ready=" << kartRuntimeReady
        << " incomplete_kart_rejected=" << (!incompleteKartResult.valid)
        << " kart_fake_travel_rejected=" << (!kartWithFakeTravelResult.valid)
        << " kart_missing_flex_rejected=" << (!kartWithoutFlexResult.valid)
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
        << " semi_trailing_valid=" << semiTrailingResult.valid
        << " semi_trailing_ready=" << semiTrailingRuntimeReady
        << " incomplete_semi_trailing_rejected=" << (!incompleteSemiTrailingResult.valid)
        << " twist_beam_valid=" << twistBeamResult.valid
        << " twist_beam_ready=" << twistBeamRuntimeReady
        << " incomplete_twist_beam_rejected=" << (!incompleteTwistBeamResult.valid)
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
        && futureSuspensionResult.issueSummary().find("pullrod_double_wishbone_v1")
            != std::string::npos
        && doubleWishboneRuntimeReady
        && !incompleteDoubleWishboneResult.valid
        && incompleteDoubleWishboneResult.issueSummary().find(
            "eleven named hardpoints") != std::string::npos
        && pushrodRuntimeReady
        && !incompletePushrodResult.valid
        && incompletePushrodResult.issueSummary().find(
            "seventeen named hardpoints") != std::string::npos
        && liveAxleRuntimeReady
        && !incompleteLiveAxleResult.valid
        && incompleteLiveAxleResult.issueSummary().find(
            "seventeen named hardpoints") != std::string::npos
        && leafAxleRuntimeReady
        && !incompleteLeafAxleResult.valid
        && incompleteLeafAxleResult.issueSummary().find(
            "plus eight leaf/shackle points") != std::string::npos
        && motorcycleSuspensionRuntimeReady
        && !incompleteMotorcycleForkResult.valid
        && incompleteMotorcycleForkResult.issueSummary().find(
            "steering_stem_upper") != std::string::npos
        && !incompleteMotorcycleRearResult.valid
        && incompleteMotorcycleRearResult.issueSummary().find(
            "all ten named") != std::string::npos
        && kartRuntimeReady
        && !incompleteKartResult.valid
        && incompleteKartResult.issueSummary().find(
            "complete ten-point") != std::string::npos
        && !kartWithFakeTravelResult.valid
        && kartWithFakeTravelResult.issueSummary().find(
            "zero bump/droop") != std::string::npos
        && !kartWithoutFlexResult.valid
        && kartWithoutFlexResult.issueSummary().find(
            "chassis_torsional_mode_v1") != std::string::npos
        && macPhersonRuntimeReady
        && !incompleteMacPhersonResult.valid
        && incompleteMacPhersonResult.issueSummary().find(
            "eight named hardpoints") != std::string::npos
        && trailingArmRuntimeReady
        && !incompleteTrailingArmResult.valid
        && incompleteTrailingArmResult.issueSummary().find(
            "five named hardpoints") != std::string::npos
        && semiTrailingRuntimeReady
        && !incompleteSemiTrailingResult.valid
        && incompleteSemiTrailingResult.issueSummary().find("seven named") != std::string::npos
        && twistBeamRuntimeReady
        && !incompleteTwistBeamResult.valid
        && incompleteTwistBeamResult.issueSummary().find("two beam attachment") != std::string::npos
        && suspensionForcesWorked
        && nonlinearSuspensionWorked
        && liveSuspensionRoundTrip
        && liveUnsprungRoundTrip
        && liveGeometryRoundTrip;
}

} // namespace heritage::tests
