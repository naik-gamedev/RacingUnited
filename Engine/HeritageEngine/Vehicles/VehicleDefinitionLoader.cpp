#include "VehicleDefinitionLoader.hpp"
#include "Suspension/Geometry/DoubleWishbone/DoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/PushrodDoubleWishbone/PushrodDoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/LiveAxle/LiveAxleKinematics.hpp"
#include "Suspension/Springs/LeafSpring/LeafSpringLiveAxle.hpp"
#include "Suspension/Geometry/MotorcycleFork/MotorcycleForkKinematics.hpp"
#include "Suspension/Geometry/MotorcycleSwingarm/MotorcycleSwingarmKinematics.hpp"
#include "Suspension/Geometry/Kart/KartChassisKinematics.hpp"
#include "Suspension/Geometry/MultiLink/MultiLinkKinematics.hpp"
#include "Suspension/Geometry/SemiTrailingArm/SemiTrailingArmKinematics.hpp"
#include "Suspension/Geometry/TwistBeam/TwistBeamKinematics.hpp"

#include <string_view>

namespace heritage::vehicles {
namespace {

bool copySuspensionHardpoint(
    const VehicleSuspensionDefinition& suspension,
    std::string_view id,
    heritage::math::Vec3& destination)
{
    for (const VehicleSuspensionHardpointDefinition& hardpoint
         : suspension.hardpoints)
    {
        if (hardpoint.id == id)
        {
            destination = hardpoint.localPosition;
            return true;
        }
    }
    return false;
}

bool buildMacPhersonHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MacPhersonHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(
            suspension, "strut_top_mount", hardpoints.strutTopMount)
        && copySuspensionHardpoint(
            suspension, "strut_upright_mount", hardpoints.strutUprightMount)
        && copySuspensionHardpoint(
            suspension, "lower_arm_inner_front",
            hardpoints.lowerArmInnerFront)
        && copySuspensionHardpoint(
            suspension, "lower_arm_inner_rear",
            hardpoints.lowerArmInnerRear)
        && copySuspensionHardpoint(
            suspension, "lower_ball_joint", hardpoints.lowerBallJoint)
        && copySuspensionHardpoint(
            suspension, "tie_rod_inner", hardpoints.tieRodInner)
        && copySuspensionHardpoint(
            suspension, "tie_rod_outer", hardpoints.tieRodOuter)
        && copySuspensionHardpoint(
            suspension, "wheel_center", hardpoints.wheelCenter);
    hardpoints.authored = complete;
    return complete && validMacPhersonHardpoints(hardpoints);
}

bool safeModuleRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory())
        return false;
    for (const auto& component : path)
    {
        if (component == "..")
            return false;
    }
    return true;
}

bool parseTireProvider(
    std::string_view provider,
    TireProviderKind& value)
{
    if (provider == "advanced_road" || provider == "mf62_road")
    {
        value = TireProviderKind::MagicFormula62;
        return true;
    }
    if (provider == "motorcycle_profile" || provider == "mf62_motorcycle")
    {
        value = TireProviderKind::MagicFormula62Motorcycle;
        return true;
    }
    if (provider == "legacy_generalized_road")
    {
        value = TireProviderKind::LegacyGeneralizedRoad;
        return true;
    }
    return false;
}


bool buildDoubleWishboneHardpoints(
    const VehicleSuspensionDefinition& suspension,
    DoubleWishboneHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "upper_arm_inner_front", hardpoints.upperArmInnerFront)
        && copySuspensionHardpoint(suspension, "upper_arm_inner_rear", hardpoints.upperArmInnerRear)
        && copySuspensionHardpoint(suspension, "upper_ball_joint", hardpoints.upperBallJoint)
        && copySuspensionHardpoint(suspension, "lower_arm_inner_front", hardpoints.lowerArmInnerFront)
        && copySuspensionHardpoint(suspension, "lower_arm_inner_rear", hardpoints.lowerArmInnerRear)
        && copySuspensionHardpoint(suspension, "lower_ball_joint", hardpoints.lowerBallJoint)
        && copySuspensionHardpoint(suspension, "tie_rod_inner", hardpoints.tieRodInner)
        && copySuspensionHardpoint(suspension, "tie_rod_outer", hardpoints.tieRodOuter)
        && copySuspensionHardpoint(suspension, "wheel_center", hardpoints.wheelCenter)
        && copySuspensionHardpoint(suspension, "damper_upper_mount", hardpoints.damperUpperMount)
        && copySuspensionHardpoint(suspension, "damper_lower_mount", hardpoints.damperLowerMount);
    hardpoints.authored = complete;
    return complete && validDoubleWishboneHardpoints(hardpoints);
}
bool buildPushrodDoubleWishboneHardpoints(
    const VehicleSuspensionDefinition& suspension,
    PushrodDoubleWishboneHardpoints& hardpoints)
{
    hardpoints = {};
    DoubleWishboneHardpoints& wishbone = hardpoints.wishbone;
    const bool complete =
        copySuspensionHardpoint(suspension, "upper_arm_inner_front", wishbone.upperArmInnerFront)
        && copySuspensionHardpoint(suspension, "upper_arm_inner_rear", wishbone.upperArmInnerRear)
        && copySuspensionHardpoint(suspension, "upper_ball_joint", wishbone.upperBallJoint)
        && copySuspensionHardpoint(suspension, "lower_arm_inner_front", wishbone.lowerArmInnerFront)
        && copySuspensionHardpoint(suspension, "lower_arm_inner_rear", wishbone.lowerArmInnerRear)
        && copySuspensionHardpoint(suspension, "lower_ball_joint", wishbone.lowerBallJoint)
        && copySuspensionHardpoint(suspension, "tie_rod_inner", wishbone.tieRodInner)
        && copySuspensionHardpoint(suspension, "tie_rod_outer", wishbone.tieRodOuter)
        && copySuspensionHardpoint(suspension, "wheel_center", wishbone.wheelCenter)
        && copySuspensionHardpoint(suspension, "pushrod_lower_arm_mount", hardpoints.pushrodLowerArmMount)
        && copySuspensionHardpoint(suspension, "rocker_pivot_front", hardpoints.rockerPivotFront)
        && copySuspensionHardpoint(suspension, "rocker_pivot_rear", hardpoints.rockerPivotRear)
        && copySuspensionHardpoint(suspension, "rocker_pushrod_mount", hardpoints.rockerPushrodMount)
        && copySuspensionHardpoint(suspension, "spring_chassis_mount", hardpoints.springChassisMount)
        && copySuspensionHardpoint(suspension, "spring_rocker_mount", hardpoints.springRockerMount)
        && copySuspensionHardpoint(suspension, "damper_chassis_mount", hardpoints.damperChassisMount)
        && copySuspensionHardpoint(suspension, "damper_rocker_mount", hardpoints.damperRockerMount);
    wishbone.authored = complete;
    hardpoints.authored = complete;
    return complete && validPushrodDoubleWishboneHardpoints(hardpoints);
}


bool buildMultiLinkHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MultiLinkHardpoints& value)
{
    value = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "link1_inner", value.link1Inner)
        && copySuspensionHardpoint(suspension, "link1_outer", value.link1Outer)
        && copySuspensionHardpoint(suspension, "link2_inner", value.link2Inner)
        && copySuspensionHardpoint(suspension, "link2_outer", value.link2Outer)
        && copySuspensionHardpoint(suspension, "link3_inner", value.link3Inner)
        && copySuspensionHardpoint(suspension, "link3_outer", value.link3Outer)
        && copySuspensionHardpoint(suspension, "link4_inner", value.link4Inner)
        && copySuspensionHardpoint(suspension, "link4_outer", value.link4Outer)
        && copySuspensionHardpoint(suspension, "toe_link_inner", value.toeLinkInner)
        && copySuspensionHardpoint(suspension, "toe_link_outer", value.toeLinkOuter)
        && copySuspensionHardpoint(suspension, "wheel_center", value.wheelCenter)
        && copySuspensionHardpoint(suspension, "spring_upper_mount", value.springUpperMount)
        && copySuspensionHardpoint(suspension, "spring_lower_mount", value.springLowerMount)
        && copySuspensionHardpoint(suspension, "damper_upper_mount", value.damperUpperMount)
        && copySuspensionHardpoint(suspension, "damper_lower_mount", value.damperLowerMount)
        && copySuspensionHardpoint(suspension, "steering_rack_axis_start", value.steeringRackAxisStart)
        && copySuspensionHardpoint(suspension, "steering_rack_axis_end", value.steeringRackAxisEnd);
    value.authored = complete;
    return complete && validMultiLinkHardpoints(value);
}
bool buildMotorcycleForkHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MotorcycleForkHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "steering_stem_upper", hardpoints.steeringStemUpper)
        && copySuspensionHardpoint(suspension, "steering_stem_lower", hardpoints.steeringStemLower)
        && copySuspensionHardpoint(suspension, "wheel_center", hardpoints.wheelCenter);
    hardpoints.authored = complete;
    return complete && validMotorcycleForkHardpoints(hardpoints);
}

bool buildMotorcycleSwingarmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MotorcycleSwingarmHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "swingarm_pivot_left", hardpoints.swingarmPivotLeft)
        && copySuspensionHardpoint(suspension, "swingarm_pivot_right", hardpoints.swingarmPivotRight)
        && copySuspensionHardpoint(suspension, "wheel_center", hardpoints.wheelCenter)
        && copySuspensionHardpoint(suspension, "linkage_swingarm_mount", hardpoints.linkageSwingarmMount)
        && copySuspensionHardpoint(suspension, "rocker_pivot_left", hardpoints.rockerPivotLeft)
        && copySuspensionHardpoint(suspension, "rocker_pivot_right", hardpoints.rockerPivotRight)
        && copySuspensionHardpoint(suspension, "rocker_link_mount", hardpoints.rockerLinkMount)
        && copySuspensionHardpoint(suspension, "shock_chassis_mount", hardpoints.shockChassisMount)
        && copySuspensionHardpoint(suspension, "shock_rocker_mount", hardpoints.shockRockerMount)
        && copySuspensionHardpoint(suspension, "countershaft_center", hardpoints.countershaftCenter);
    hardpoints.authored = complete;
    return complete && validMotorcycleSwingarmHardpoints(hardpoints);
}

bool buildKartChassisHardpoints(
    const VehicleSuspensionDefinition& suspension,
    KartChassisHardpoints& value)
{
    value = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "front_left_kingpin_upper", value.frontLeftKingpinUpper)
        && copySuspensionHardpoint(suspension, "front_left_kingpin_lower", value.frontLeftKingpinLower)
        && copySuspensionHardpoint(suspension, "front_left_wheel_center", value.frontLeftWheelCenter)
        && copySuspensionHardpoint(suspension, "front_right_kingpin_upper", value.frontRightKingpinUpper)
        && copySuspensionHardpoint(suspension, "front_right_kingpin_lower", value.frontRightKingpinLower)
        && copySuspensionHardpoint(suspension, "front_right_wheel_center", value.frontRightWheelCenter)
        && copySuspensionHardpoint(suspension, "rear_axle_bearing_left", value.rearAxleBearingLeft)
        && copySuspensionHardpoint(suspension, "rear_axle_bearing_right", value.rearAxleBearingRight)
        && copySuspensionHardpoint(suspension, "rear_left_wheel_center", value.rearLeftWheelCenter)
        && copySuspensionHardpoint(suspension, "rear_right_wheel_center", value.rearRightWheelCenter);
    value.authored = complete;
    return complete && validKartChassisHardpoints(value);
}


bool buildSemiTrailingArmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    SemiTrailingArmHardpoints& value,
    std::string_view prefix = {})
{
    value = {};
    const auto key=[&](std::string_view suffix){return std::string(prefix)+std::string(suffix);};
    const bool complete = copySuspensionHardpoint(suspension,key("arm_pivot_inner"),value.armPivotInner)
        && copySuspensionHardpoint(suspension,key("arm_pivot_outer"),value.armPivotOuter)
        && copySuspensionHardpoint(suspension,key("wheel_center"),value.wheelCenter)
        && copySuspensionHardpoint(suspension,key("spring_upper_mount"),value.springUpperMount)
        && copySuspensionHardpoint(suspension,key("spring_lower_mount"),value.springLowerMount)
        && copySuspensionHardpoint(suspension,key("damper_upper_mount"),value.damperUpperMount)
        && copySuspensionHardpoint(suspension,key("damper_lower_mount"),value.damperLowerMount);
    value.authored=complete; return complete&&validSemiTrailingArmHardpoints(value);
}

bool buildTwistBeamHardpoints(const VehicleSuspensionDefinition&suspension,TwistBeamHardpoints&value)
{
    value={}; const bool complete=buildSemiTrailingArmHardpoints(suspension,value.leftArm,"left_")
        && buildSemiTrailingArmHardpoints(suspension,value.rightArm,"right_")
        && copySuspensionHardpoint(suspension,"beam_left_attachment",value.beamLeftAttachment)
        && copySuspensionHardpoint(suspension,"beam_right_attachment",value.beamRightAttachment);
    value.authored=complete; return complete&&validTwistBeamHardpoints(value);
}

bool buildTrailingArmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    TrailingArmHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(
            suspension, "arm_pivot_inner", hardpoints.armPivotInner)
        && copySuspensionHardpoint(
            suspension, "arm_pivot_outer", hardpoints.armPivotOuter)
        && copySuspensionHardpoint(
            suspension, "wheel_center", hardpoints.wheelCenter)
        && copySuspensionHardpoint(
            suspension, "damper_upper_mount", hardpoints.damperUpperMount)
        && copySuspensionHardpoint(
            suspension, "damper_lower_mount", hardpoints.damperLowerMount);
    hardpoints.authored = complete;
    return complete && validTrailingArmHardpoints(hardpoints);
}

bool buildLiveAxleHardpoints(
    const VehicleSuspensionDefinition& suspension,
    LiveAxleHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete =
        copySuspensionHardpoint(suspension, "axle_center", hardpoints.axleCenter)
        && copySuspensionHardpoint(suspension, "left_wheel_center", hardpoints.leftWheelCenter)
        && copySuspensionHardpoint(suspension, "right_wheel_center", hardpoints.rightWheelCenter)
        && copySuspensionHardpoint(suspension, "panhard_chassis_mount", hardpoints.panhardChassisMount)
        && copySuspensionHardpoint(suspension, "panhard_axle_mount", hardpoints.panhardAxleMount)
        && copySuspensionHardpoint(suspension, "left_trailing_chassis_mount", hardpoints.leftTrailingChassisMount)
        && copySuspensionHardpoint(suspension, "left_trailing_axle_mount", hardpoints.leftTrailingAxleMount)
        && copySuspensionHardpoint(suspension, "right_trailing_chassis_mount", hardpoints.rightTrailingChassisMount)
        && copySuspensionHardpoint(suspension, "right_trailing_axle_mount", hardpoints.rightTrailingAxleMount)
        && copySuspensionHardpoint(suspension, "left_spring_chassis_mount", hardpoints.leftSpringChassisMount)
        && copySuspensionHardpoint(suspension, "left_spring_axle_mount", hardpoints.leftSpringAxleMount)
        && copySuspensionHardpoint(suspension, "right_spring_chassis_mount", hardpoints.rightSpringChassisMount)
        && copySuspensionHardpoint(suspension, "right_spring_axle_mount", hardpoints.rightSpringAxleMount)
        && copySuspensionHardpoint(suspension, "left_damper_chassis_mount", hardpoints.leftDamperChassisMount)
        && copySuspensionHardpoint(suspension, "left_damper_axle_mount", hardpoints.leftDamperAxleMount)
        && copySuspensionHardpoint(suspension, "right_damper_chassis_mount", hardpoints.rightDamperChassisMount)
        && copySuspensionHardpoint(suspension, "right_damper_axle_mount", hardpoints.rightDamperAxleMount);
    hardpoints.authored = complete;
    return complete && validLiveAxleHardpoints(hardpoints);
}

bool buildLeafSpringLiveAxleHardpoints(
    const VehicleSuspensionDefinition& suspension,
    LeafSpringLiveAxleHardpoints& hardpoints)
{
    hardpoints = {};
    const bool complete = buildLiveAxleHardpoints(suspension, hardpoints.axle)
        && copySuspensionHardpoint(suspension, "left_leaf_front_eye", hardpoints.leftLeafFrontEye)
        && copySuspensionHardpoint(suspension, "left_leaf_rear_shackle_pivot", hardpoints.leftLeafRearShacklePivot)
        && copySuspensionHardpoint(suspension, "left_leaf_rear_eye", hardpoints.leftLeafRearEye)
        && copySuspensionHardpoint(suspension, "left_leaf_axle_clamp", hardpoints.leftLeafAxleClamp)
        && copySuspensionHardpoint(suspension, "right_leaf_front_eye", hardpoints.rightLeafFrontEye)
        && copySuspensionHardpoint(suspension, "right_leaf_rear_shackle_pivot", hardpoints.rightLeafRearShacklePivot)
        && copySuspensionHardpoint(suspension, "right_leaf_rear_eye", hardpoints.rightLeafRearEye)
        && copySuspensionHardpoint(suspension, "right_leaf_axle_clamp", hardpoints.rightLeafAxleClamp);
    hardpoints.authored = complete;
    return complete && validLeafSpringLiveAxleHardpoints(hardpoints);
}

} // namespace

VehicleHandle VehicleDefinitionLoader::create(
    const CompiledVehicleDefinition& definition,
    const VehicleDefinitionLoadSettings& settings,
    heritage::physics::RigidBodySystem& bodies,
    VehicleSystem& vehicles,
    std::string& errorMessage)
{
    errorMessage.clear();
    if (definition.runtimeProvider != "raycast_wheel_v1")
    {
        errorMessage = "Compiled definition has no supported runtime provider.";
        return InvalidVehicle;
    }
    if (definition.powerUnits.size() != 1
        || definition.transmissions.size() != 1
        || definition.contactUnits.size() != 4)
    {
        errorMessage = "raycast_wheel_v1 received an incompatible compiled topology.";
        return InvalidVehicle;
    }

    const VehicleBodyDefinition* primaryBody = nullptr;
    for (const VehicleBodyDefinition& body : definition.bodies)
    {
        if (body.role == "primary")
        {
            primaryBody = &body;
            break;
        }
    }
    if (!primaryBody)
    {
        errorMessage = "Compiled definition has no primary body.";
        return InvalidVehicle;
    }
    if (!bodies.setMass(settings.vehicle.chassisBody, primaryBody->massKg))
    {
        errorMessage = bodies.lastError();
        return InvalidVehicle;
    }
    const heritage::math::Vec3 desiredCenterOfMass =
        primaryBody->hasCenterOfMassLocal
        ? primaryBody->centerOfMassLocal
        : heritage::math::Vec3{};
    if (!bodies.setCenterOfMassLocal(
            settings.vehicle.chassisBody, desiredCenterOfMass))
    {
        errorMessage = bodies.lastError();
        return InvalidVehicle;
    }
    if (primaryBody->hasInertiaLocalKgM2)
    {
        if (!bodies.setInertiaLocal(
                settings.vehicle.chassisBody, primaryBody->inertiaLocalKgM2))
        {
            errorMessage = bodies.lastError();
            return InvalidVehicle;
        }
    }
    else if (!bodies.clearInertiaLocalOverride(settings.vehicle.chassisBody))
    {
        // A compiled definition is authoritative. Loading one without explicit
        // inertia must not inherit an override from a previously loaded vehicle
        // that happened to reuse the same rigid body. Collider-derived inertia
        // will be rebuilt on the next physics step.
        errorMessage = bodies.lastError();
        return InvalidVehicle;
    }

    const VehicleHandle handle = vehicles.create(settings.vehicle, bodies);
    if (handle == InvalidVehicle)
    {
        errorMessage = vehicles.lastError();
        return InvalidVehicle;
    }

    const auto rollback = [&]() -> VehicleHandle {
        vehicles.destroy(handle);
        return InvalidVehicle;
    };

    const VehiclePowerUnitDefinition& power =
        definition.powerUnits.front().authored;
    const VehicleTransmissionDefinition& transmission =
        definition.transmissions.front().authored;
    if (!vehicles.setPowertrain(
            handle,
            power.idleRpm,
            power.redlineRpm,
            power.maximumTorqueNm,
            power.engineBrakingTorqueNm,
            transmission.finalDriveRatio,
            transmission.efficiency,
            transmission.shiftDurationSeconds,
            transmission.clutchEngagementRate))
    {
        errorMessage = vehicles.lastError();
        return rollback();
    }
    if (!vehicles.setGearRatios(
            handle,
            transmission.reverseRatio,
            transmission.forwardRatios))
    {
        errorMessage = vehicles.lastError();
        return rollback();
    }

    for (std::size_t contactIndex = 0;
         contactIndex < definition.contactUnits.size();
         ++contactIndex)
    {
        const CompiledVehicleContactUnit& contact =
            definition.contactUnits[contactIndex];
        if (contact.suspensionIndex >= definition.suspensions.size())
        {
            errorMessage = "Compiled contact has no resolved suspension component.";
            return rollback();
        }
        const VehicleSuspensionDefinition& suspension =
            definition.suspensions[contact.suspensionIndex].authored;
        SuspensionProviderKind suspensionProvider{};
        if (!parseSuspensionProvider(suspension.provider, suspensionProvider))
        {
            errorMessage = "Compiled suspension provider is not available.";
            return rollback();
        }

        WheelDescription wheel;
        wheel.localMount = contact.authored.localMount;
        wheel.suspensionAxleId = contact.authored.axle;
        wheel.localSuspensionDirection = contact.authored.suspensionDirection;
        wheel.radius = contact.authored.radiusM;
        wheel.restLength = suspension.restLengthM;
        wheel.maximumCompression = suspension.maximumCompressionM;
        wheel.maximumDroop = suspension.maximumDroopM;
        wheel.springPreload = suspension.springPreloadN;
        wheel.springRate = suspension.springRateNPerM;
        wheel.springProgression = suspension.springProgressionNPerM2;
        wheel.bumpDamping = suspension.bumpDampingNsPerM;
        wheel.bumpHighSpeedDamping =
            suspension.bumpHighSpeedDampingNsPerM;
        wheel.bumpDampingKneeVelocity =
            suspension.bumpDampingKneeVelocityMps;
        wheel.reboundDamping = suspension.reboundDampingNsPerM;
        wheel.reboundHighSpeedDamping =
            suspension.reboundHighSpeedDampingNsPerM;
        wheel.reboundDampingKneeVelocity =
            suspension.reboundDampingKneeVelocityMps;
        wheel.bumpStopEngagement = suspension.bumpStopEngagementM;
        wheel.bumpStopRate = suspension.bumpStopRateNPerM;
        wheel.bumpStopProgression = suspension.bumpStopProgressionNPerM2;
        wheel.droopStopEngagement = suspension.droopStopEngagementM;
        wheel.droopStopRate = suspension.droopStopRateNPerM;
        wheel.localSteeringAxis = suspension.localSteeringAxis;
        wheel.staticCamberDegrees = suspension.staticCamberDegrees;
        wheel.camberGainDegreesPerM = suspension.camberGainDegreesPerM;
        wheel.camberProgressionDegreesPerM2 =
            suspension.camberProgressionDegreesPerM2;
        wheel.staticToeDegrees = suspension.staticToeDegrees;
        wheel.toeGainDegreesPerM = suspension.toeGainDegreesPerM;
        wheel.toeProgressionDegreesPerM2 =
            suspension.toeProgressionDegreesPerM2;
        wheel.suspensionProvider = suspensionProvider;
        if (suspensionProvider == SuspensionProviderKind::MacPhersonStrutV1
            && !buildMacPhersonHardpoints(
                suspension, wheel.macPhersonHardpoints))
        {
            errorMessage = "MacPherson suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::DoubleWishboneV1
            && !buildDoubleWishboneHardpoints(
                suspension, wheel.doubleWishboneHardpoints))
        {
            errorMessage = "Double-wishbone suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider
                == SuspensionProviderKind::PushrodDoubleWishboneV1
            && !buildPushrodDoubleWishboneHardpoints(
                suspension, wheel.pushrodDoubleWishboneHardpoints))
        {
            errorMessage = "Pushrod double-wishbone suspension '"
                + suspension.id + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::MultiLinkV1
            && !buildMultiLinkHardpoints(
                suspension, wheel.multiLinkHardpoints))
        {
            errorMessage = "Multi-link suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::LiveAxleV1
            && !buildLiveAxleHardpoints(
                suspension, wheel.liveAxleHardpoints))
        {
            errorMessage = "Live-axle suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::LeafSpringLiveAxleV1
            && !buildLeafSpringLiveAxleHardpoints(
                suspension, wheel.leafSpringLiveAxleHardpoints))
        {
            errorMessage = "Leaf-spring live-axle suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::MotorcycleTelescopicForkV1
            && !buildMotorcycleForkHardpoints(
                suspension, wheel.motorcycleForkHardpoints))
        {
            errorMessage = "Motorcycle telescopic-fork suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1
            && !buildMotorcycleSwingarmHardpoints(
                suspension, wheel.motorcycleSwingarmHardpoints))
        {
            errorMessage = "Motorcycle swingarm-linkage suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::KartChassisFlexV1
            && !buildKartChassisHardpoints(
                suspension, wheel.kartChassisHardpoints))
        {
            errorMessage = "Kart chassis suspension '" + suspension.id
                + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::SemiTrailingArmV1
            && !buildSemiTrailingArmHardpoints(suspension,wheel.semiTrailingArmHardpoints))
        {
            errorMessage = "Semi-trailing-arm suspension '" + suspension.id + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider == SuspensionProviderKind::TwistBeamV1
            && !buildTwistBeamHardpoints(suspension,wheel.twistBeamHardpoints))
        {
            errorMessage = "Twist-beam suspension '" + suspension.id + "' is missing valid required hardpoints.";
            return rollback();
        }
        if (suspensionProvider
                == SuspensionProviderKind::TrailingArmTorsionBarV1
            && !buildTrailingArmHardpoints(
                suspension, wheel.trailingArmHardpoints))
        {
            errorMessage = "Trailing-arm torsion-bar suspension '"
                + suspension.id + "' is missing valid required hardpoints.";
            return rollback();
        }
        wheel.suspensionMotionRatio = suspension.motionRatio;
        wheel.maximumSuspensionForce = suspension.maximumForceN;
        wheel.leafInterleafFrictionN = suspension.leafInterleafFrictionN;
        wheel.leafInterleafVelocityScaleMps = suspension.leafInterleafVelocityScaleMps;
        wheel.leafInterleafViscousNsPerM = suspension.leafInterleafViscousNsPerM;
        wheel.leafAxleWrapStiffnessNmPerRad = suspension.leafAxleWrapStiffnessNmPerRad;
        wheel.leafAxleWrapDampingNmsPerRad = suspension.leafAxleWrapDampingNmsPerRad;
        wheel.leafAxleWrapInertiaKgM2 = suspension.leafAxleWrapInertiaKgM2;
        wheel.leafAxleWrapJackingNPerRad = suspension.leafAxleWrapJackingNPerRad;
        wheel.motorcycleRearSprocketPitchRadiusM = suspension.motorcycleRearSprocketPitchRadiusM;
        wheel.twistBeamTorsionalStiffnessNmPerRad = suspension.twistBeamTorsionalStiffnessNmPerRad;
        wheel.twistBeamTorsionalDampingNmsPerRad = suspension.twistBeamTorsionalDampingNmsPerRad;
        wheel.effectiveUnsprungMass =
            contact.authored.effectiveUnsprungMassKg;
        wheel.tireRadialStiffness =
            contact.authored.tireRadialStiffnessNPerM;
        wheel.tireRadialDamping =
            contact.authored.tireRadialDampingNsPerM;
        wheel.maximumTireDeflection =
            contact.authored.maximumTireDeflectionM;
        wheel.maximumTireNormalForce =
            contact.authored.maximumTireNormalForceN;
        wheel.driveFactor = contact.driveFactor;
        wheel.steerFactor = contact.authored.steering ? 1.0f : 0.0f;
        wheel.brakeFactor = contact.authored.serviceBrake
            ? contact.authored.serviceBrakeFactor : 0.0f;
        wheel.handbrakeFactor = contact.authored.parkingBrake
            ? contact.authored.parkingBrakeFactor : 0.0f;
        if (!vehicles.addWheel(handle, wheel))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }

        TireProviderKind tireProvider{};
        if (!parseTireProvider(contact.authored.tireProvider, tireProvider)
            || !vehicles.setWheelTireProvider(
                handle, contactIndex, tireProvider))
        {
            errorMessage = vehicles.lastError().empty()
                ? "Compiled tire provider is not available."
                : vehicles.lastError();
            return rollback();
        }

        if (!contact.authored.tireParameterFile.empty())
        {
            const std::filesystem::path relative = contact.authored.tireParameterFile;
            if (settings.moduleRoot.empty() || !safeModuleRelativePath(relative))
            {
                errorMessage = "Tire property path must be a safe module-relative path: "
                    + contact.authored.tireParameterFile;
                return rollback();
            }
            const std::filesystem::path resolved =
                (settings.moduleRoot / relative).lexically_normal();
            if (!vehicles.loadWheelTirePropertyFile(
                    handle,
                    contactIndex,
                    resolved,
                    contact.authored.tireParameterProvenance,
                    contact.authored.tireParameterConfidence))
            {
                errorMessage = vehicles.lastError();
                return rollback();
            }
        }
    }

    for (std::size_t index = 0; index < definition.antiRollBars.size(); ++index)
    {
        const CompiledVehicleAntiRollBar& source = definition.antiRollBars[index];
        SuspensionAntiRollBarDescription bar;
        bar.leftWheelIndex = source.leftContactUnitIndex;
        bar.rightWheelIndex = source.rightContactUnitIndex;
        bar.enabled = source.authored.enabled;
        bar.torsionalStiffnessNmPerRad = source.authored.torsionalStiffnessNmPerRad;
        bar.torsionalDampingNmsPerRad = source.authored.torsionalDampingNmsPerRad;
        bar.leftLeverArmM = source.authored.leftLeverArmM;
        bar.rightLeverArmM = source.authored.rightLeverArmM;
        bar.leftLinkMotionRatio = source.authored.leftLinkMotionRatio;
        bar.rightLinkMotionRatio = source.authored.rightLinkMotionRatio;
        bar.maximumWheelForceN = source.authored.maximumWheelForceN;
        if (!vehicles.setAntiRollBar(handle, index, bar))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }
    }

    if (definition.chassisFlex.authored.enabled)
    {
        const VehicleChassisFlexDefinition& source =
            definition.chassisFlex.authored;
        ChassisTorsionalComplianceDescription flex;
        flex.enabled = true;
        flex.torsionalRigidityNmPerDegree =
            source.torsionalRigidityNmPerDegree;
        flex.torsionalDampingNmsPerRad = source.torsionalDampingNmsPerRad;
        flex.effectiveTorsionalInertiaKgM2 =
            source.effectiveTorsionalInertiaKgM2;
        flex.torsionAxisLocalY = source.torsionAxisLocalY;
        flex.frontReferenceLocalZ = source.frontReferenceLocalZ;
        flex.rearReferenceLocalZ = source.rearReferenceLocalZ;
        flex.maximumTwistDegrees = source.maximumTwistDegrees;
        if (!vehicles.setChassisTorsionalCompliance(handle, flex))
        {
            errorMessage = vehicles.lastError();
            return rollback();
        }
    }

    errorMessage = "Compiled definition loaded with raycast_wheel_v1.";
    return handle;
}

} // namespace heritage::vehicles
