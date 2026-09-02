#include "VehicleDefinitionCompiler.hpp"
#include "Suspension/Geometry/MacPherson/MacPhersonKinematics.hpp"
#include "Suspension/Geometry/DoubleWishbone/DoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/PushrodDoubleWishbone/PushrodDoubleWishboneKinematics.hpp"
#include "Suspension/Geometry/TrailingArm/TrailingArmKinematics.hpp"
#include "Suspension/Geometry/LiveAxle/LiveAxleKinematics.hpp"
#include "Suspension/Springs/LeafSpring/LeafSpringLiveAxle.hpp"
#include "Suspension/Geometry/MotorcycleFork/MotorcycleForkKinematics.hpp"
#include "Suspension/Geometry/MotorcycleSwingarm/MotorcycleSwingarmKinematics.hpp"
#include "Suspension/Geometry/Kart/KartChassisKinematics.hpp"
#include "Suspension/Geometry/MultiLink/MultiLinkKinematics.hpp"
#include "Suspension/Geometry/SemiTrailingArm/SemiTrailingArmKinematics.hpp"
#include "Suspension/Geometry/TwistBeam/TwistBeamKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace heritage::vehicles {
namespace {

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(const heritage::math::Vec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}


bool readHardpoint(
    const VehicleSuspensionDefinition& suspension,
    const char* id,
    heritage::math::Vec3& value)
{
    for (const VehicleSuspensionHardpointDefinition& hardpoint
         : suspension.hardpoints)
    {
        if (hardpoint.id == id)
        {
            value = hardpoint.localPosition;
            return true;
        }
    }
    return false;
}

bool buildMacPhersonHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MacPhersonHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "strut_top_mount", value.strutTopMount)
        && readHardpoint(
            suspension, "strut_upright_mount", value.strutUprightMount)
        && readHardpoint(
            suspension, "lower_arm_inner_front", value.lowerArmInnerFront)
        && readHardpoint(
            suspension, "lower_arm_inner_rear", value.lowerArmInnerRear)
        && readHardpoint(
            suspension, "lower_ball_joint", value.lowerBallJoint)
        && readHardpoint(suspension, "tie_rod_inner", value.tieRodInner)
        && readHardpoint(suspension, "tie_rod_outer", value.tieRodOuter)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter);
    value.authored = complete;
    return complete;
}


bool buildDoubleWishboneHardpoints(
    const VehicleSuspensionDefinition& suspension,
    DoubleWishboneHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "upper_arm_inner_front", value.upperArmInnerFront)
        && readHardpoint(suspension, "upper_arm_inner_rear", value.upperArmInnerRear)
        && readHardpoint(suspension, "upper_ball_joint", value.upperBallJoint)
        && readHardpoint(suspension, "lower_arm_inner_front", value.lowerArmInnerFront)
        && readHardpoint(suspension, "lower_arm_inner_rear", value.lowerArmInnerRear)
        && readHardpoint(suspension, "lower_ball_joint", value.lowerBallJoint)
        && readHardpoint(suspension, "tie_rod_inner", value.tieRodInner)
        && readHardpoint(suspension, "tie_rod_outer", value.tieRodOuter)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter)
        && readHardpoint(suspension, "damper_upper_mount", value.damperUpperMount)
        && readHardpoint(suspension, "damper_lower_mount", value.damperLowerMount);
    value.authored = complete;
    return complete;
}
bool buildPushrodDoubleWishboneHardpoints(
    const VehicleSuspensionDefinition& suspension,
    PushrodDoubleWishboneHardpoints& value)
{
    value = {};
    DoubleWishboneHardpoints& wishbone = value.wishbone;
    const bool complete =
        readHardpoint(suspension, "upper_arm_inner_front", wishbone.upperArmInnerFront)
        && readHardpoint(suspension, "upper_arm_inner_rear", wishbone.upperArmInnerRear)
        && readHardpoint(suspension, "upper_ball_joint", wishbone.upperBallJoint)
        && readHardpoint(suspension, "lower_arm_inner_front", wishbone.lowerArmInnerFront)
        && readHardpoint(suspension, "lower_arm_inner_rear", wishbone.lowerArmInnerRear)
        && readHardpoint(suspension, "lower_ball_joint", wishbone.lowerBallJoint)
        && readHardpoint(suspension, "tie_rod_inner", wishbone.tieRodInner)
        && readHardpoint(suspension, "tie_rod_outer", wishbone.tieRodOuter)
        && readHardpoint(suspension, "wheel_center", wishbone.wheelCenter)
        && readHardpoint(suspension, "pushrod_lower_arm_mount", value.pushrodLowerArmMount)
        && readHardpoint(suspension, "rocker_pivot_front", value.rockerPivotFront)
        && readHardpoint(suspension, "rocker_pivot_rear", value.rockerPivotRear)
        && readHardpoint(suspension, "rocker_pushrod_mount", value.rockerPushrodMount)
        && readHardpoint(suspension, "spring_chassis_mount", value.springChassisMount)
        && readHardpoint(suspension, "spring_rocker_mount", value.springRockerMount)
        && readHardpoint(suspension, "damper_chassis_mount", value.damperChassisMount)
        && readHardpoint(suspension, "damper_rocker_mount", value.damperRockerMount);
    wishbone.authored = complete;
    value.authored = complete;
    return complete;
}


bool buildMultiLinkHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MultiLinkHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "link1_inner", value.link1Inner)
        && readHardpoint(suspension, "link1_outer", value.link1Outer)
        && readHardpoint(suspension, "link2_inner", value.link2Inner)
        && readHardpoint(suspension, "link2_outer", value.link2Outer)
        && readHardpoint(suspension, "link3_inner", value.link3Inner)
        && readHardpoint(suspension, "link3_outer", value.link3Outer)
        && readHardpoint(suspension, "link4_inner", value.link4Inner)
        && readHardpoint(suspension, "link4_outer", value.link4Outer)
        && readHardpoint(suspension, "toe_link_inner", value.toeLinkInner)
        && readHardpoint(suspension, "toe_link_outer", value.toeLinkOuter)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter)
        && readHardpoint(suspension, "spring_upper_mount", value.springUpperMount)
        && readHardpoint(suspension, "spring_lower_mount", value.springLowerMount)
        && readHardpoint(suspension, "damper_upper_mount", value.damperUpperMount)
        && readHardpoint(suspension, "damper_lower_mount", value.damperLowerMount)
        && readHardpoint(suspension, "steering_rack_axis_start", value.steeringRackAxisStart)
        && readHardpoint(suspension, "steering_rack_axis_end", value.steeringRackAxisEnd);
    value.authored = complete;
    return complete;
}
bool buildMotorcycleForkHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MotorcycleForkHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "steering_stem_upper", value.steeringStemUpper)
        && readHardpoint(suspension, "steering_stem_lower", value.steeringStemLower)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter);
    value.authored = complete;
    return complete;
}

bool buildMotorcycleSwingarmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    MotorcycleSwingarmHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "swingarm_pivot_left", value.swingarmPivotLeft)
        && readHardpoint(suspension, "swingarm_pivot_right", value.swingarmPivotRight)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter)
        && readHardpoint(suspension, "linkage_swingarm_mount", value.linkageSwingarmMount)
        && readHardpoint(suspension, "rocker_pivot_left", value.rockerPivotLeft)
        && readHardpoint(suspension, "rocker_pivot_right", value.rockerPivotRight)
        && readHardpoint(suspension, "rocker_link_mount", value.rockerLinkMount)
        && readHardpoint(suspension, "shock_chassis_mount", value.shockChassisMount)
        && readHardpoint(suspension, "shock_rocker_mount", value.shockRockerMount)
        && readHardpoint(suspension, "countershaft_center", value.countershaftCenter);
    value.authored = complete;
    return complete;
}

bool buildKartChassisHardpoints(
    const VehicleSuspensionDefinition& suspension,
    KartChassisHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "front_left_kingpin_upper", value.frontLeftKingpinUpper)
        && readHardpoint(suspension, "front_left_kingpin_lower", value.frontLeftKingpinLower)
        && readHardpoint(suspension, "front_left_wheel_center", value.frontLeftWheelCenter)
        && readHardpoint(suspension, "front_right_kingpin_upper", value.frontRightKingpinUpper)
        && readHardpoint(suspension, "front_right_kingpin_lower", value.frontRightKingpinLower)
        && readHardpoint(suspension, "front_right_wheel_center", value.frontRightWheelCenter)
        && readHardpoint(suspension, "rear_axle_bearing_left", value.rearAxleBearingLeft)
        && readHardpoint(suspension, "rear_axle_bearing_right", value.rearAxleBearingRight)
        && readHardpoint(suspension, "rear_left_wheel_center", value.rearLeftWheelCenter)
        && readHardpoint(suspension, "rear_right_wheel_center", value.rearRightWheelCenter);
    value.authored = complete;
    return complete;
}

bool buildTrailingArmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    TrailingArmHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "arm_pivot_inner", value.armPivotInner)
        && readHardpoint(suspension, "arm_pivot_outer", value.armPivotOuter)
        && readHardpoint(suspension, "wheel_center", value.wheelCenter)
        && readHardpoint(
            suspension, "damper_upper_mount", value.damperUpperMount)
        && readHardpoint(
            suspension, "damper_lower_mount", value.damperLowerMount);
    value.authored = complete;
    return complete;
}


bool buildSemiTrailingArmHardpoints(
    const VehicleSuspensionDefinition& suspension,
    SemiTrailingArmHardpoints& value,
    const char* prefix = "")
{
    value = {};
    const auto id = [&](const char* suffix) { return std::string(prefix) + suffix; };
    const bool complete =
        readHardpoint(suspension, id("arm_pivot_inner").c_str(), value.armPivotInner)
        && readHardpoint(suspension, id("arm_pivot_outer").c_str(), value.armPivotOuter)
        && readHardpoint(suspension, id("wheel_center").c_str(), value.wheelCenter)
        && readHardpoint(suspension, id("spring_upper_mount").c_str(), value.springUpperMount)
        && readHardpoint(suspension, id("spring_lower_mount").c_str(), value.springLowerMount)
        && readHardpoint(suspension, id("damper_upper_mount").c_str(), value.damperUpperMount)
        && readHardpoint(suspension, id("damper_lower_mount").c_str(), value.damperLowerMount);
    value.authored = complete;
    return complete;
}

bool buildTwistBeamHardpoints(
    const VehicleSuspensionDefinition& suspension,
    TwistBeamHardpoints& value)
{
    value = {};
    const bool complete = buildSemiTrailingArmHardpoints(suspension,value.leftArm,"left_")
        && buildSemiTrailingArmHardpoints(suspension,value.rightArm,"right_")
        && readHardpoint(suspension,"beam_left_attachment",value.beamLeftAttachment)
        && readHardpoint(suspension,"beam_right_attachment",value.beamRightAttachment);
    value.authored = complete;
    return complete;
}

bool buildLiveAxleHardpoints(
    const VehicleSuspensionDefinition& suspension,
    LiveAxleHardpoints& value)
{
    value = {};
    const bool complete =
        readHardpoint(suspension, "axle_center", value.axleCenter)
        && readHardpoint(suspension, "left_wheel_center", value.leftWheelCenter)
        && readHardpoint(suspension, "right_wheel_center", value.rightWheelCenter)
        && readHardpoint(suspension, "panhard_chassis_mount", value.panhardChassisMount)
        && readHardpoint(suspension, "panhard_axle_mount", value.panhardAxleMount)
        && readHardpoint(suspension, "left_trailing_chassis_mount", value.leftTrailingChassisMount)
        && readHardpoint(suspension, "left_trailing_axle_mount", value.leftTrailingAxleMount)
        && readHardpoint(suspension, "right_trailing_chassis_mount", value.rightTrailingChassisMount)
        && readHardpoint(suspension, "right_trailing_axle_mount", value.rightTrailingAxleMount)
        && readHardpoint(suspension, "left_spring_chassis_mount", value.leftSpringChassisMount)
        && readHardpoint(suspension, "left_spring_axle_mount", value.leftSpringAxleMount)
        && readHardpoint(suspension, "right_spring_chassis_mount", value.rightSpringChassisMount)
        && readHardpoint(suspension, "right_spring_axle_mount", value.rightSpringAxleMount)
        && readHardpoint(suspension, "left_damper_chassis_mount", value.leftDamperChassisMount)
        && readHardpoint(suspension, "left_damper_axle_mount", value.leftDamperAxleMount)
        && readHardpoint(suspension, "right_damper_chassis_mount", value.rightDamperChassisMount)
        && readHardpoint(suspension, "right_damper_axle_mount", value.rightDamperAxleMount);
    value.authored = complete;
    return complete;
}

bool buildLeafSpringLiveAxleHardpoints(
    const VehicleSuspensionDefinition& suspension,
    LeafSpringLiveAxleHardpoints& value)
{
    value = {};
    const bool baseComplete = buildLiveAxleHardpoints(suspension, value.axle);
    const bool complete = baseComplete
        && readHardpoint(suspension, "left_leaf_front_eye", value.leftLeafFrontEye)
        && readHardpoint(suspension, "left_leaf_rear_shackle_pivot", value.leftLeafRearShacklePivot)
        && readHardpoint(suspension, "left_leaf_rear_eye", value.leftLeafRearEye)
        && readHardpoint(suspension, "left_leaf_axle_clamp", value.leftLeafAxleClamp)
        && readHardpoint(suspension, "right_leaf_front_eye", value.rightLeafFrontEye)
        && readHardpoint(suspension, "right_leaf_rear_shackle_pivot", value.rightLeafRearShacklePivot)
        && readHardpoint(suspension, "right_leaf_rear_eye", value.rightLeafRearEye)
        && readHardpoint(suspension, "right_leaf_axle_clamp", value.rightLeafAxleClamp);
    value.authored = complete;
    return complete;
}

bool safeId(const std::string& value)
{
    if (value.empty() || value.size() > 64)
        return false;

    return std::all_of(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return (character >= 'a' && character <= 'z')
                || (character >= '0' && character <= '9')
                || character == '_'
                || character == '-';
        });
}

void addIssue(
    VehicleDefinitionCompileResult& result,
    VehicleDefinitionIssueSeverity severity,
    const std::string& code,
    const std::string& message)
{
    result.issues.push_back({ severity, code, message });
}

void addError(
    VehicleDefinitionCompileResult& result,
    const std::string& code,
    const std::string& message)
{
    addIssue(result, VehicleDefinitionIssueSeverity::Error, code, message);
}

void addWarning(
    VehicleDefinitionCompileResult& result,
    const std::string& code,
    const std::string& message)
{
    addIssue(result, VehicleDefinitionIssueSeverity::Warning, code, message);
}

template<typename Component>
std::unordered_map<std::string, std::size_t> indexComponents(
    const std::vector<Component>& components,
    const char* label,
    VehicleDefinitionCompileResult& result)
{
    std::unordered_map<std::string, std::size_t> indices;
    for (std::size_t index = 0; index < components.size(); ++index)
    {
        const std::string& id = components[index].id;
        if (!safeId(id))
        {
            addError(
                result,
                "unsafe_component_id",
                std::string(label) + " at index " + std::to_string(index + 1)
                    + " needs a lowercase identifier of at most 64 characters.");
            continue;
        }

        if (!indices.emplace(id, index).second)
        {
            addError(
                result,
                "duplicate_component_id",
                std::string(label) + " repeats ID '" + id + "'.");
        }
    }
    return indices;
}

template<typename Map>
std::size_t resolveReference(
    const Map& indices,
    const std::string& id,
    const std::string& owner,
    const char* relationship,
    VehicleDefinitionCompileResult& result)
{
    const auto found = indices.find(id);
    if (found != indices.end())
        return found->second;

    addError(
        result,
        "missing_component_reference",
        owner + " references missing " + relationship + " '" + id + "'.");
    return kInvalidVehicleComponentIndex;
}

void addReason(std::vector<std::string>& reasons, const std::string& reason)
{
    if (std::find(reasons.begin(), reasons.end(), reason) == reasons.end())
        reasons.push_back(reason);
}

std::string join(const std::vector<std::string>& values, const char* separator)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
            output << separator;
        output << values[index];
    }
    return output.str();
}

} // namespace

std::string VehicleDefinitionCompileResult::issueSummary() const
{
    std::ostringstream output;
    for (std::size_t index = 0; index < issues.size(); ++index)
    {
        if (index > 0)
            output << '\n';
        output
            << (issues[index].severity == VehicleDefinitionIssueSeverity::Error
                ? "ERROR" : "WARNING")
            << " [" << issues[index].code << "] "
            << issues[index].message;
    }
    return output.str();
}

VehicleDefinitionCompileResult VehicleDefinitionCompiler::compile(
    const VehicleDefinitionV2Source& source)
{
    VehicleDefinitionCompileResult result;
    result.definition.schemaVersion = source.schemaVersion;
    result.definition.id = source.id;
    result.definition.displayName = source.displayName;
    result.definition.classification = source.classification;
    result.definition.bodyAsset = source.bodyAsset;
    result.definition.requirements = source.requirements;
    result.definition.bodies = source.bodies;

    if (source.schemaVersion != kVehicleDefinitionSchemaVersion)
        addError(result, "schema_version", "schemaVersion must be 2.");
    if (!safeId(source.id))
    {
        addError(
            result,
            "unsafe_definition_id",
            "Definition ID must use lowercase letters, numbers, underscores or hyphens.");
    }
    if (source.displayName.empty() || source.displayName.size() > 160)
    {
        addError(
            result,
            "display_name",
            "Display name must contain 1 to 160 characters.");
    }

    if (source.bodies.empty() || source.bodies.size() > 16)
        addError(result, "body_count", "A vehicle requires 1 to 16 bodies.");
    if (source.powerUnits.size() > 8)
        addError(result, "power_unit_count", "At most 8 power units are allowed.");
    if (source.transmissions.size() > 8)
        addError(result, "transmission_count", "At most 8 transmissions are allowed.");
    if (source.suspensions.empty() || source.suspensions.size() > 32)
        addError(result, "suspension_count", "A vehicle requires 1 to 32 suspension components.");
    if (source.contactUnits.empty() || source.contactUnits.size() > 32)
        addError(result, "contact_count", "A vehicle requires 1 to 32 contact units.");
    if (source.antiRollBars.size() > 16)
        addError(result, "anti_roll_bar_count", "At most 16 anti-roll bars are allowed.");
    if (source.driveConnections.size() > 16)
        addError(result, "drive_connection_count", "At most 16 drive connections are allowed.");

    const auto bodyIndices = indexComponents(source.bodies, "Body", result);
    const auto powerIndices = indexComponents(source.powerUnits, "Power unit", result);
    const auto transmissionIndices = indexComponents(
        source.transmissions, "Transmission", result);
    const auto suspensionIndices = indexComponents(
        source.suspensions, "Suspension", result);
    const auto contactIndices = indexComponents(
        source.contactUnits, "Contact unit", result);
    indexComponents(source.antiRollBars, "Anti-roll bar", result);
    indexComponents(source.driveConnections, "Drive connection", result);

    std::size_t primaryBodyCount = 0;
    for (const VehicleBodyDefinition& body : source.bodies)
    {
        if (body.role == "primary")
            ++primaryBodyCount;
        if (!finite(body.massKg) || body.massKg <= 0.0f || body.massKg > 1000000.0f)
        {
            addError(
                result,
                "body_mass",
                "Body '" + body.id + "' needs a finite positive mass.");
        }
        if (body.hasCenterOfMassLocal && !finite(body.centerOfMassLocal))
        {
            addError(
                result,
                "body_center_of_mass",
                "Body '" + body.id + "' has a non-finite centre of mass.");
        }
        if (body.hasInertiaLocalKgM2
            && (!finite(body.inertiaLocalKgM2)
                || body.inertiaLocalKgM2.x <= 0.0f
                || body.inertiaLocalKgM2.y <= 0.0f
                || body.inertiaLocalKgM2.z <= 0.0f))
        {
            addError(
                result,
                "body_inertia",
                "Body '" + body.id + "' needs finite positive local inertia values.");
        }
        if (!finite(body.frontStaticLoadFraction)
            || body.frontStaticLoadFraction <= 0.0f
            || body.frontStaticLoadFraction >= 1.0f
            || !finite(body.leftStaticLoadFraction)
            || body.leftStaticLoadFraction <= 0.0f
            || body.leftStaticLoadFraction >= 1.0f)
        {
            addError(
                result,
                "body_static_load_fraction",
                "Body '" + body.id + "' needs front/left static-load fractions between zero and one.");
        }
        if (!finite(body.massPropertiesConfidence)
            || body.massPropertiesConfidence < 0.0f
            || body.massPropertiesConfidence > 1.0f)
        {
            addError(
                result,
                "body_mass_properties_confidence",
                "Body '" + body.id + "' mass-property confidence must be between zero and one.");
        }
        if ((body.hasCenterOfMassLocal || body.hasInertiaLocalKgM2)
            && body.massPropertiesProvenance.empty())
        {
            addWarning(
                result,
                "body_mass_properties_provenance",
                "Body '" + body.id + "' has explicit mass properties without provenance metadata.");
        }
    }
    if (primaryBodyCount != 1)
    {
        addError(
            result,
            "primary_body_count",
            "A definition must identify exactly one primary body.");
    }

    for (const VehiclePowerUnitDefinition& power : source.powerUnits)
    {
        CompiledVehiclePowerUnit compiled;
        compiled.authored = power;
        compiled.mountBodyIndex = resolveReference(
            bodyIndices,
            power.mountBody,
            "Power unit '" + power.id + "'",
            "body",
            result);
        result.definition.powerUnits.push_back(std::move(compiled));

        if (!finite(power.maximumTorqueNm) || power.maximumTorqueNm < 0.0f
            || !finite(power.idleRpm) || power.idleRpm < 0.0f
            || !finite(power.redlineRpm) || power.redlineRpm <= power.idleRpm
            || !finite(power.engineBrakingTorqueNm)
            || power.engineBrakingTorqueNm < 0.0f)
        {
            addError(
                result,
                "power_unit_parameters",
                "Power unit '" + power.id + "' has invalid torque or speed limits.");
        }
    }

    for (const VehicleTransmissionDefinition& transmission : source.transmissions)
    {
        CompiledVehicleTransmission compiled;
        compiled.authored = transmission;
        compiled.powerUnitIndex = resolveReference(
            powerIndices,
            transmission.powerUnit,
            "Transmission '" + transmission.id + "'",
            "power unit",
            result);
        result.definition.transmissions.push_back(std::move(compiled));

        if (transmission.forwardRatios.size() > 32)
        {
            addError(
                result,
                "gear_count",
                "Transmission '" + transmission.id
                    + "' exceeds 32 forward ratios.");
        }
        for (float ratio : transmission.forwardRatios)
        {
            if (!finite(ratio) || ratio <= 0.0f)
            {
                addError(
                    result,
                    "forward_ratio",
                    "Transmission '" + transmission.id
                        + "' contains a non-positive forward ratio.");
                break;
            }
        }
        if (!finite(transmission.reverseRatio) || transmission.reverseRatio >= 0.0f
            || !finite(transmission.finalDriveRatio)
            || transmission.finalDriveRatio <= 0.0f
            || !finite(transmission.efficiency)
            || transmission.efficiency <= 0.0f
            || transmission.efficiency > 1.0f
            || !finite(transmission.shiftDurationSeconds)
            || transmission.shiftDurationSeconds < 0.0f
            || !finite(transmission.clutchEngagementRate)
            || transmission.clutchEngagementRate <= 0.0f)
        {
            addError(
                result,
                "transmission_parameters",
                "Transmission '" + transmission.id + "' has invalid runtime parameters.");
        }
    }

    for (const VehicleSuspensionDefinition& suspension : source.suspensions)
    {
        CompiledVehicleSuspension compiled;
        compiled.authored = suspension;
        compiled.mountBodyIndex = resolveReference(
            bodyIndices,
            suspension.mountBody,
            "Suspension '" + suspension.id + "'",
            "body",
            result);
        result.definition.suspensions.push_back(std::move(compiled));

        if (suspension.hardpoints.size() > 32)
        {
            addError(
                result,
                "suspension_hardpoint_count",
                "Suspension '" + suspension.id
                    + "' contains more than 32 authored hardpoints.");
        }
        std::unordered_set<std::string> hardpointIds;
        for (const VehicleSuspensionHardpointDefinition& hardpoint
             : suspension.hardpoints)
        {
            if (!safeId(hardpoint.id))
            {
                addError(
                    result,
                    "unsafe_suspension_hardpoint_id",
                    "Suspension '" + suspension.id
                        + "' contains an unsafe hardpoint identifier.");
            }
            else if (!hardpointIds.insert(hardpoint.id).second)
            {
                addError(
                    result,
                    "duplicate_suspension_hardpoint_id",
                    "Suspension '" + suspension.id
                        + "' repeats hardpoint ID '" + hardpoint.id + "'.");
            }
            if (!finite(hardpoint.localPosition))
            {
                addError(
                    result,
                    "suspension_hardpoint_position",
                    "Suspension '" + suspension.id
                        + "' contains a non-finite hardpoint position.");
            }
            if (!hardpoint.provenance.empty()
                && !safeId(hardpoint.provenance))
            {
                addError(
                    result,
                    "suspension_hardpoint_provenance",
                    "Suspension '" + suspension.id
                        + "' contains an unsafe hardpoint provenance ID.");
            }
            if (!finite(hardpoint.confidence)
                || hardpoint.confidence < 0.0f
                || hardpoint.confidence > 1.0f)
            {
                addError(
                    result,
                    "suspension_hardpoint_confidence",
                    "Suspension '" + suspension.id
                        + "' contains a hardpoint confidence outside 0..1.");
            }
        }

        if (suspension.provider == "macpherson_strut_v1")
        {
            MacPhersonHardpoints macPherson;
            if (!buildMacPhersonHardpoints(suspension, macPherson))
            {
                addError(
                    result,
                    "macpherson_required_hardpoints",
                    "MacPherson suspension '" + suspension.id
                        + "' requires all eight named hardpoints.");
            }
            else if (!validMacPhersonHardpoints(macPherson))
            {
                addError(
                    result,
                    "macpherson_hardpoint_geometry",
                    "MacPherson suspension '" + suspension.id
                        + "' has degenerate hardpoint geometry.");
            }
        }

        if (suspension.provider == "double_wishbone_v1")
        {
            DoubleWishboneHardpoints wishbone;
            if (!buildDoubleWishboneHardpoints(suspension, wishbone))
            {
                addError(
                    result,
                    "double_wishbone_required_hardpoints",
                    "Double-wishbone suspension '" + suspension.id
                        + "' requires all eleven named hardpoints.");
            }
            else if (!validDoubleWishboneHardpoints(wishbone))
            {
                addError(
                    result,
                    "double_wishbone_hardpoint_geometry",
                    "Double-wishbone suspension '" + suspension.id
                        + "' has degenerate hardpoint geometry.");
            }
        }

        if (suspension.provider == "pushrod_double_wishbone_v1")
        {
            PushrodDoubleWishboneHardpoints pushrod;
            if (!buildPushrodDoubleWishboneHardpoints(suspension, pushrod))
            {
                addError(
                    result,
                    "pushrod_double_wishbone_required_hardpoints",
                    "Pushrod double-wishbone suspension '" + suspension.id
                        + "' requires all seventeen named hardpoints.");
            }
            else if (!validPushrodDoubleWishboneHardpoints(pushrod))
            {
                addError(
                    result,
                    "pushrod_double_wishbone_hardpoint_geometry",
                    "Pushrod double-wishbone suspension '" + suspension.id
                        + "' has degenerate wishbone/pushrod/rocker geometry.");
            }
        }


        if (suspension.provider == "multilink_v1")
        {
            MultiLinkHardpoints multiLink;
            if (!buildMultiLinkHardpoints(suspension, multiLink))
            {
                addError(result, "multilink_required_hardpoints",
                    "Multi-link suspension '" + suspension.id
                        + "' requires all seventeen named link/actuator/rack hardpoints.");
            }
            else if (!validMultiLinkHardpoints(multiLink))
            {
                addError(result, "multilink_hardpoint_geometry",
                    "Multi-link suspension '" + suspension.id
                        + "' has degenerate five-link/upright/actuator geometry.");
            }
        }

        if (suspension.provider == "live_axle_v1")
        {
            LiveAxleHardpoints liveAxle;
            if (!buildLiveAxleHardpoints(suspension, liveAxle))
            {
                addError(
                    result,
                    "live_axle_required_hardpoints",
                    "Live-axle suspension '" + suspension.id
                        + "' requires all seventeen named hardpoints.");
            }
            else if (!validLiveAxleHardpoints(liveAxle))
            {
                addError(
                    result,
                    "live_axle_hardpoint_geometry",
                    "Live-axle suspension '" + suspension.id
                        + "' has degenerate axle/link/spring geometry.");
            }
        }

        if (suspension.provider == "live_axle_leaf_v1")
        {
            LeafSpringLiveAxleHardpoints leafAxle;
            if (!buildLeafSpringLiveAxleHardpoints(suspension, leafAxle))
            {
                addError(result, "leaf_live_axle_required_hardpoints",
                    "Leaf-spring live-axle suspension '" + suspension.id
                        + "' requires the SUSP08 seventeen-point axle package plus eight leaf/shackle points.");
            }
            else if (!validLeafSpringLiveAxleHardpoints(leafAxle))
            {
                addError(result, "leaf_live_axle_hardpoint_geometry",
                    "Leaf-spring live-axle suspension '" + suspension.id
                        + "' has degenerate axle/leaf/shackle geometry.");
            }
        }

        if (suspension.provider == "motorcycle_telescopic_fork_v1")
        {
            MotorcycleForkHardpoints fork;
            if (!buildMotorcycleForkHardpoints(suspension, fork))
            {
                addError(result, "motorcycle_fork_required_hardpoints",
                    "Motorcycle telescopic-fork suspension '" + suspension.id
                        + "' requires steering_stem_upper, steering_stem_lower and wheel_center hardpoints.");
            }
            else if (!validMotorcycleForkHardpoints(fork))
            {
                addError(result, "motorcycle_fork_hardpoint_geometry",
                    "Motorcycle telescopic-fork suspension '" + suspension.id
                        + "' has degenerate steering-axis/axle geometry.");
            }
        }

        if (suspension.provider == "motorcycle_swingarm_linkage_v1")
        {
            MotorcycleSwingarmHardpoints swingarm;
            if (!buildMotorcycleSwingarmHardpoints(suspension, swingarm))
            {
                addError(result, "motorcycle_swingarm_required_hardpoints",
                    "Motorcycle swingarm-linkage suspension '" + suspension.id
                        + "' requires all ten named swingarm/linkage/chain hardpoints.");
            }
            else if (!validMotorcycleSwingarmHardpoints(swingarm))
            {
                addError(result, "motorcycle_swingarm_hardpoint_geometry",
                    "Motorcycle swingarm-linkage suspension '" + suspension.id
                        + "' has degenerate swingarm/dogbone/rocker/shock geometry.");
            }
        }

        if (suspension.provider == "kart_chassis_flex_v1")
        {
            KartChassisHardpoints kart;
            if (!buildKartChassisHardpoints(suspension, kart))
            {
                addError(result, "kart_chassis_required_hardpoints",
                    "Kart chassis suspension '" + suspension.id
                        + "' requires the complete ten-point front-kingpin/rear-axle package.");
            }
            else if (!validKartChassisHardpoints(kart))
            {
                addError(result, "kart_chassis_hardpoint_geometry",
                    "Kart chassis suspension '" + suspension.id
                        + "' has degenerate kingpin or rigid rear-axle geometry.");
            }
            if (std::abs(suspension.maximumCompressionM) > 1.0e-6f
                || std::abs(suspension.maximumDroopM) > 1.0e-6f)
            {
                addError(result, "kart_chassis_no_suspension_travel",
                    "Kart chassis suspension '" + suspension.id
                        + "' must author zero bump/droop travel; tire compliance and frame torsion are the suspension.");
            }
            if (!source.chassisFlex.enabled
                || source.chassisFlex.provider != "chassis_torsional_mode_v1")
            {
                addError(result, "kart_chassis_requires_frame_torsion",
                    "Kart chassis suspension requires enabled chassis_torsional_mode_v1 frame compliance.");
            }
        }

        if (suspension.provider == "semi_trailing_arm_v1")
        {
            SemiTrailingArmHardpoints arm;
            if (!buildSemiTrailingArmHardpoints(suspension, arm))
                addError(result,"semi_trailing_arm_required_hardpoints","Semi-trailing-arm suspension '"+suspension.id+"' requires seven named pivot/wheel/spring/damper hardpoints.");
            else if (!validSemiTrailingArmHardpoints(arm))
                addError(result,"semi_trailing_arm_hardpoint_geometry","Semi-trailing-arm suspension '"+suspension.id+"' has degenerate arm/actuator geometry.");
        }

        if (suspension.provider == "twist_beam_v1")
        {
            TwistBeamHardpoints beam;
            if (!buildTwistBeamHardpoints(suspension, beam))
                addError(result,"twist_beam_required_hardpoints","Twist-beam suspension '"+suspension.id+"' requires left/right seven-point semi-trailing-arm packages plus two beam attachment points.");
            else if (!validTwistBeamHardpoints(beam))
                addError(result,"twist_beam_hardpoint_geometry","Twist-beam suspension '"+suspension.id+"' has degenerate arm or crossbeam geometry.");
        }

        if (suspension.provider == "trailing_arm_torsion_bar_v1")
        {
            TrailingArmHardpoints trailingArm;
            if (!buildTrailingArmHardpoints(suspension, trailingArm))
            {
                addError(
                    result,
                    "trailing_arm_required_hardpoints",
                    "Trailing-arm torsion-bar suspension '" + suspension.id
                        + "' requires all five named hardpoints.");
            }
            else if (!validTrailingArmHardpoints(trailingArm))
            {
                addError(
                    result,
                    "trailing_arm_hardpoint_geometry",
                    "Trailing-arm torsion-bar suspension '" + suspension.id
                        + "' has degenerate hardpoint geometry.");
            }
        }

        const bool kartRigidProvider = suspension.provider == "kart_chassis_flex_v1";

        if (!finite(suspension.restLengthM) || suspension.restLengthM <= 0.0f
            || !finite(suspension.maximumCompressionM)
            || suspension.maximumCompressionM < 0.0f
            || suspension.maximumCompressionM >= suspension.restLengthM
            || !finite(suspension.maximumDroopM)
            || suspension.maximumDroopM < 0.0f
            || !finite(suspension.springPreloadN)
            || suspension.springPreloadN < 0.0f
            || suspension.springPreloadN > 10000000.0f
            || !finite(suspension.springRateNPerM)
            || suspension.springRateNPerM < 0.0f
            || (!kartRigidProvider && suspension.springRateNPerM <= 0.0f)
            || suspension.springRateNPerM > 1000000000.0f
            || !finite(suspension.springProgressionNPerM2)
            || suspension.springProgressionNPerM2 < 0.0f
            || suspension.springProgressionNPerM2 > 10000000000.0f
            || !finite(suspension.bumpDampingNsPerM)
            || suspension.bumpDampingNsPerM < 0.0f
            || suspension.bumpDampingNsPerM > 100000000.0f
            || !finite(suspension.bumpHighSpeedDampingNsPerM)
            || suspension.bumpHighSpeedDampingNsPerM < 0.0f
            || suspension.bumpHighSpeedDampingNsPerM > 100000000.0f
            || !finite(suspension.bumpDampingKneeVelocityMps)
            || suspension.bumpDampingKneeVelocityMps < 0.0f
            || suspension.bumpDampingKneeVelocityMps > 100.0f
            || !finite(suspension.reboundDampingNsPerM)
            || suspension.reboundDampingNsPerM < 0.0f
            || suspension.reboundDampingNsPerM > 100000000.0f
            || !finite(suspension.reboundHighSpeedDampingNsPerM)
            || suspension.reboundHighSpeedDampingNsPerM < 0.0f
            || suspension.reboundHighSpeedDampingNsPerM > 100000000.0f
            || !finite(suspension.reboundDampingKneeVelocityMps)
            || suspension.reboundDampingKneeVelocityMps < 0.0f
            || suspension.reboundDampingKneeVelocityMps > 100.0f
            || !finite(suspension.bumpStopEngagementM)
            || suspension.bumpStopEngagementM < 0.0f
            || !finite(suspension.bumpStopRateNPerM)
            || suspension.bumpStopRateNPerM < 0.0f
            || suspension.bumpStopRateNPerM > 1000000000.0f
            || !finite(suspension.bumpStopProgressionNPerM2)
            || suspension.bumpStopProgressionNPerM2 < 0.0f
            || suspension.bumpStopProgressionNPerM2 > 10000000000.0f
            || !finite(suspension.droopStopEngagementM)
            || suspension.droopStopEngagementM < 0.0f
            || !finite(suspension.droopStopRateNPerM)
            || suspension.droopStopRateNPerM < 0.0f
            || suspension.droopStopRateNPerM > 1000000000.0f
            || !finite(suspension.localSteeringAxis)
            || suspension.localSteeringAxis.x * suspension.localSteeringAxis.x
                + suspension.localSteeringAxis.y * suspension.localSteeringAxis.y
                + suspension.localSteeringAxis.z * suspension.localSteeringAxis.z
                <= 0.000001f
            || !finite(suspension.staticCamberDegrees)
            || std::abs(suspension.staticCamberDegrees) > 45.0f
            || !finite(suspension.camberGainDegreesPerM)
            || std::abs(suspension.camberGainDegreesPerM) > 1000.0f
            || !finite(suspension.camberProgressionDegreesPerM2)
            || std::abs(suspension.camberProgressionDegreesPerM2) > 10000.0f
            || !finite(suspension.staticToeDegrees)
            || std::abs(suspension.staticToeDegrees) > 45.0f
            || !finite(suspension.toeGainDegreesPerM)
            || std::abs(suspension.toeGainDegreesPerM) > 1000.0f
            || !finite(suspension.toeProgressionDegreesPerM2)
            || std::abs(suspension.toeProgressionDegreesPerM2) > 10000.0f
            || !finite(suspension.motionRatio)
            || suspension.motionRatio <= 0.0f
            || suspension.motionRatio > 10.0f
            || !finite(suspension.maximumForceN)
            || suspension.maximumForceN <= 0.0f
            || suspension.maximumForceN > 100000000.0f
            || !finite(suspension.leafInterleafFrictionN)
            || suspension.leafInterleafFrictionN < 0.0f
            || !finite(suspension.leafInterleafVelocityScaleMps)
            || suspension.leafInterleafVelocityScaleMps <= 0.0001f
            || !finite(suspension.leafInterleafViscousNsPerM)
            || suspension.leafInterleafViscousNsPerM < 0.0f
            || !finite(suspension.leafAxleWrapStiffnessNmPerRad)
            || suspension.leafAxleWrapStiffnessNmPerRad < 0.0f
            || !finite(suspension.leafAxleWrapDampingNmsPerRad)
            || suspension.leafAxleWrapDampingNmsPerRad < 0.0f
            || !finite(suspension.leafAxleWrapInertiaKgM2)
            || suspension.leafAxleWrapInertiaKgM2 <= 0.01f
            || !finite(suspension.leafAxleWrapJackingNPerRad)
            || suspension.leafAxleWrapJackingNPerRad < 0.0f
            || !finite(suspension.motorcycleRearSprocketPitchRadiusM)
            || suspension.motorcycleRearSprocketPitchRadiusM < 0.02f
            || suspension.motorcycleRearSprocketPitchRadiusM > 0.30f
            || !finite(suspension.twistBeamTorsionalStiffnessNmPerRad)
            || suspension.twistBeamTorsionalStiffnessNmPerRad < 0.0f
            || !finite(suspension.twistBeamTorsionalDampingNmsPerRad)
            || suspension.twistBeamTorsionalDampingNmsPerRad < 0.0f)
        {
            addError(
                result,
                "suspension_parameters",
                "Suspension '" + suspension.id + "' has invalid runtime parameters.");
        }
    }

    for (const VehicleContactUnitDefinition& contact : source.contactUnits)
    {
        CompiledVehicleContactUnit compiled;
        compiled.authored = contact;
        compiled.mountBodyIndex = resolveReference(
            bodyIndices,
            contact.mountBody,
            "Contact unit '" + contact.id + "'",
            "body",
            result);
        compiled.suspensionIndex = resolveReference(
            suspensionIndices,
            contact.suspension,
            "Contact unit '" + contact.id + "'",
            "suspension",
            result);
        result.definition.contactUnits.push_back(std::move(compiled));

        if (!finite(contact.localMount) || !finite(contact.suspensionDirection)
            || !finite(contact.radiusM) || contact.radiusM <= 0.0f
            || !finite(contact.effectiveUnsprungMassKg)
            || contact.effectiveUnsprungMassKg < 0.0f
            || contact.effectiveUnsprungMassKg > 1000.0f
            || !finite(contact.tireRadialStiffnessNPerM)
            || contact.tireRadialStiffnessNPerM <= 0.0f
            || contact.tireRadialStiffnessNPerM > 10000000.0f
            || !finite(contact.tireRadialDampingNsPerM)
            || contact.tireRadialDampingNsPerM < 0.0f
            || contact.tireRadialDampingNsPerM > 1000000.0f
            || !finite(contact.maximumTireDeflectionM)
            || contact.maximumTireDeflectionM <= 0.0f
            || contact.maximumTireDeflectionM > 1.0f
            || !finite(contact.maximumTireNormalForceN)
            || contact.maximumTireNormalForceN <= 0.0f
            || contact.maximumTireNormalForceN > 10000000.0f
            || !finite(contact.serviceBrakeFactor) || contact.serviceBrakeFactor < 0.0f
            || !finite(contact.parkingBrakeFactor) || contact.parkingBrakeFactor < 0.0f)
        {
            addError(
                result,
                "contact_parameters",
                "Contact unit '" + contact.id + "' has invalid wheel or suspension data.");
        }
    }

    for (const VehicleAntiRollBarDefinition& bar : source.antiRollBars)
    {
        CompiledVehicleAntiRollBar compiled;
        compiled.authored = bar;
        compiled.leftContactUnitIndex = resolveReference(
            contactIndices,
            bar.leftContactUnit,
            "Anti-roll bar '" + bar.id + "'",
            "left contact unit",
            result);
        compiled.rightContactUnitIndex = resolveReference(
            contactIndices,
            bar.rightContactUnit,
            "Anti-roll bar '" + bar.id + "'",
            "right contact unit",
            result);
        result.definition.antiRollBars.push_back(std::move(compiled));

        if (bar.leftContactUnit == bar.rightContactUnit)
        {
            addError(
                result,
                "anti_roll_bar_same_contact",
                "Anti-roll bar '" + bar.id
                    + "' must couple two different contact units.");
        }
        if (!finite(bar.torsionalStiffnessNmPerRad)
            || bar.torsionalStiffnessNmPerRad < 0.0f
            || bar.torsionalStiffnessNmPerRad > 10000000.0f
            || !finite(bar.torsionalDampingNmsPerRad)
            || bar.torsionalDampingNmsPerRad < 0.0f
            || bar.torsionalDampingNmsPerRad > 1000000.0f
            || !finite(bar.leftLeverArmM) || bar.leftLeverArmM <= 0.0f
            || bar.leftLeverArmM > 10.0f
            || !finite(bar.rightLeverArmM) || bar.rightLeverArmM <= 0.0f
            || bar.rightLeverArmM > 10.0f
            || !finite(bar.leftLinkMotionRatio)
            || bar.leftLinkMotionRatio <= 0.0f
            || bar.leftLinkMotionRatio > 10.0f
            || !finite(bar.rightLinkMotionRatio)
            || bar.rightLinkMotionRatio <= 0.0f
            || bar.rightLinkMotionRatio > 10.0f
            || !finite(bar.maximumWheelForceN)
            || bar.maximumWheelForceN < 0.0f
            || bar.maximumWheelForceN > 10000000.0f
            || !finite(bar.confidence)
            || bar.confidence < 0.0f || bar.confidence > 1.0f
            || (!bar.provenance.empty() && !safeId(bar.provenance)))
        {
            addError(
                result,
                "anti_roll_bar_parameters",
                "Anti-roll bar '" + bar.id
                    + "' has invalid geometry, stiffness, damping or provenance data.");
        }
    }

    result.definition.chassisFlex.authored = source.chassisFlex;
    if (source.chassisFlex.enabled)
    {
        result.definition.chassisFlex.mountBodyIndex = resolveReference(
            bodyIndices,
            source.chassisFlex.mountBody,
            "Chassis-flex component",
            "body",
            result);
        if (source.chassisFlex.provider != "chassis_torsional_mode_v1")
        {
            addError(
                result,
                "chassis_flex_provider",
                "Enabled chassis flex requires provider 'chassis_torsional_mode_v1'.");
        }
        if (!finite(source.chassisFlex.torsionalRigidityNmPerDegree)
            || source.chassisFlex.torsionalRigidityNmPerDegree <= 0.0f
            || source.chassisFlex.torsionalRigidityNmPerDegree > 1000000.0f
            || !finite(source.chassisFlex.torsionalDampingNmsPerRad)
            || source.chassisFlex.torsionalDampingNmsPerRad < 0.0f
            || source.chassisFlex.torsionalDampingNmsPerRad > 10000000.0f
            || !finite(source.chassisFlex.effectiveTorsionalInertiaKgM2)
            || source.chassisFlex.effectiveTorsionalInertiaKgM2 <= 0.001f
            || source.chassisFlex.effectiveTorsionalInertiaKgM2 > 10000000.0f
            || !finite(source.chassisFlex.torsionAxisLocalY)
            || std::abs(source.chassisFlex.torsionAxisLocalY) > 10.0f
            || !finite(source.chassisFlex.frontReferenceLocalZ)
            || !finite(source.chassisFlex.rearReferenceLocalZ)
            || source.chassisFlex.frontReferenceLocalZ
                - source.chassisFlex.rearReferenceLocalZ < 0.10f
            || !finite(source.chassisFlex.maximumTwistDegrees)
            || source.chassisFlex.maximumTwistDegrees <= 0.0f
            || source.chassisFlex.maximumTwistDegrees > 20.0f
            || !finite(source.chassisFlex.confidence)
            || source.chassisFlex.confidence < 0.0f
            || source.chassisFlex.confidence > 1.0f
            || (!source.chassisFlex.provenance.empty()
                && !safeId(source.chassisFlex.provenance)))
        {
            addError(
                result,
                "chassis_flex_parameters",
                "Chassis-flex stiffness, damping, modal inertia, axis, references or provenance are invalid.");
        }
    }

    std::vector<std::size_t> driveReferenceCounts(source.contactUnits.size(), 0);
    for (const VehicleDriveConnectionDefinition& connection : source.driveConnections)
    {
        CompiledVehicleDriveConnection compiled;
        compiled.id = connection.id;
        compiled.transmissionIndex = resolveReference(
            transmissionIndices,
            connection.transmission,
            "Drive connection '" + connection.id + "'",
            "transmission",
            result);

        std::unordered_set<std::size_t> uniqueContacts;
        for (const std::string& contactId : connection.contactUnits)
        {
            const std::size_t contactIndex = resolveReference(
                contactIndices,
                contactId,
                "Drive connection '" + connection.id + "'",
                "contact unit",
                result);
            if (contactIndex == kInvalidVehicleComponentIndex)
                continue;
            if (!uniqueContacts.insert(contactIndex).second)
            {
                addError(
                    result,
                    "duplicate_drive_target",
                    "Drive connection '" + connection.id
                        + "' repeats contact unit '" + contactId + "'.");
                continue;
            }
            compiled.contactUnitIndices.push_back(contactIndex);
            ++driveReferenceCounts[contactIndex];
        }
        result.definition.driveConnections.push_back(std::move(compiled));
    }

    for (const CompiledVehicleDriveConnection& connection
        : result.definition.driveConnections)
    {
        if (connection.contactUnitIndices.empty())
            continue;
        const float factor = 1.0f
            / static_cast<float>(connection.contactUnitIndices.size());
        for (std::size_t contactIndex : connection.contactUnitIndices)
            result.definition.contactUnits[contactIndex].driveFactor += factor;
    }

    if (source.powerUnits.empty())
        addWarning(result, "unpowered", "Definition is an unpowered vehicle or trailer.");
    else if (source.driveConnections.empty())
        addWarning(result, "undriven", "No drive connection reaches a contact unit.");
    if (!source.powerUnits.empty())
    {
        addWarning(
            result,
            "placement_metadata_only",
            "Power-unit placement is retained but does not yet derive mass distribution.");
    }

    std::vector<std::string> providerReasons;
    if (source.bodies.size() != 1)
        addReason(providerReasons, "one rigid body");
    if (source.powerUnits.size() != 1)
        addReason(providerReasons, "one power unit");
    if (source.transmissions.size() != 1)
        addReason(providerReasons, "one transmission");
    if (source.contactUnits.size() != 4)
        addReason(providerReasons, "four wheel contacts");
    if (source.driveConnections.size() != 1)
        addReason(providerReasons, "one drivetrain route");
    if (source.requirements.leanDynamics)
        addReason(providerReasons, "lean_dynamics provider");
    if (source.requirements.articulation)
        addReason(providerReasons, "articulation provider");
    if (source.requirements.trackContacts)
        addReason(providerReasons, "continuous_track provider");

    for (const VehiclePowerUnitDefinition& power : source.powerUnits)
    {
        if (power.kind != "combustion")
            addReason(providerReasons, "power unit provider '" + power.kind + "'");
    }
    for (const VehicleTransmissionDefinition& transmission : source.transmissions)
    {
        if (transmission.kind != "manual" && transmission.kind != "direct")
            addReason(providerReasons, "transmission provider '" + transmission.kind + "'");
        if (transmission.forwardRatios.empty()
            || transmission.forwardRatios.size() > 16)
        {
            addReason(providerReasons, "1 to 16 native forward ratios");
        }
    }
    for (const VehicleSuspensionDefinition& suspension : source.suspensions)
    {
        if (suspension.provider != "linear_raycast_v1"
            && suspension.provider != "macpherson_strut_v1"
            && suspension.provider != "double_wishbone_v1"
            && suspension.provider != "pushrod_double_wishbone_v1"
            && suspension.provider != "live_axle_v1"
            && suspension.provider != "live_axle_leaf_v1"
            && suspension.provider != "motorcycle_telescopic_fork_v1"
            && suspension.provider != "motorcycle_swingarm_linkage_v1"
            && suspension.provider != "kart_chassis_flex_v1"
            && suspension.provider != "multilink_v1"
            && suspension.provider != "semi_trailing_arm_v1"
            && suspension.provider != "twist_beam_v1"
            && suspension.provider != "trailing_arm_torsion_bar_v1")
        {
            addReason(
                providerReasons,
                "suspension provider '" + suspension.provider + "'");
        }
    }
    for (const VehicleContactUnitDefinition& contact : source.contactUnits)
    {
        if (contact.kind != "wheel")
            addReason(providerReasons, "contact provider '" + contact.kind + "'");
        if (contact.tireProvider != "advanced_road"
            && contact.tireProvider != "mf62_road"
            && contact.tireProvider != "motorcycle_profile"
            && contact.tireProvider != "mf62_motorcycle"
            && contact.tireProvider != "legacy_generalized_road")
        {
            addReason(providerReasons, "tire provider '" + contact.tireProvider + "'");
        }
    }
    if (source.chassisFlex.enabled
        && source.chassisFlex.provider != "chassis_torsional_mode_v1")
    {
        addReason(
            providerReasons,
            "chassis-flex provider '" + source.chassisFlex.provider + "'");
    }
    if (!source.contactUnits.empty()
        && std::all_of(
            driveReferenceCounts.begin(),
            driveReferenceCounts.end(),
            [](std::size_t count) { return count == 0; }))
    {
        addReason(providerReasons, "at least one driven contact");
    }

    result.valid = std::none_of(
        result.issues.begin(),
        result.issues.end(),
        [](const VehicleDefinitionIssue& issue) {
            return issue.severity == VehicleDefinitionIssueSeverity::Error;
        });
    result.currentSolverReady = result.valid && providerReasons.empty();
    result.definition.runtimeProvider = result.currentSolverReady
        ? "raycast_wheel_v1"
        : "unresolved";

    if (result.valid && !result.currentSolverReady)
    {
        addWarning(
            result,
            "future_runtime_providers",
            "Definition is valid but awaits: " + join(providerReasons, ", ") + ".");
    }

    std::ostringstream summary;
    summary
        << "schema v" << source.schemaVersion
        << " | " << source.bodies.size() << " bodies"
        << " | " << source.powerUnits.size() << " power units"
        << " | " << source.transmissions.size() << " transmissions"
        << " | " << source.suspensions.size() << " suspensions"
        << " | " << source.contactUnits.size() << " contacts"
        << " | " << source.antiRollBars.size() << " anti-roll bars"
        << " | chassis flex " << (source.chassisFlex.enabled ? "enabled" : "rigid")
        << " | provider " << result.definition.runtimeProvider;
    result.summary = summary.str();
    return result;
}

} // namespace heritage::vehicles
