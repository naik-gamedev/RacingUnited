#include "PhysicsRegressionCommon.hpp"
#include "../Vehicles/Suspension/Authoring/MacPhersonHardpointEstimator.hpp"
#include "../Vehicles/Suspension/Authoring/TrailingArmHardpointEstimator.hpp"
#include "../Vehicles/Suspension/Springs/TorsionBar.hpp"
#include "../Vehicles/Suspension/Common/SuspensionAntiRollBar.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace heritage::tests {


namespace {

heritage::vehicles::MacPhersonHardpoints syntheticMacPhersonCorner(
    bool leftSide)
{
    heritage::vehicles::MacPhersonHardpoints hardpoints;
    hardpoints.authored = true;
    hardpoints.strutTopMount = { 0.55f, 1.15f, 1.16f };
    hardpoints.strutUprightMount = { 0.68f, 0.62f, 1.19f };
    hardpoints.lowerArmInnerFront = { 0.25f, 0.38f, 1.45f };
    hardpoints.lowerArmInnerRear = { 0.25f, 0.38f, 0.95f };
    hardpoints.lowerBallJoint = { 0.78f, 0.30f, 1.20f };
    hardpoints.tieRodInner = { 0.30f, 0.48f, 1.08f };
    hardpoints.tieRodOuter = { 0.68f, 0.36f, 1.13f };
    hardpoints.wheelCenter = { 0.72f, 0.34f, 1.20f };
    if (leftSide)
    {
        heritage::math::Vec3* points[] = {
            &hardpoints.strutTopMount,
            &hardpoints.strutUprightMount,
            &hardpoints.lowerArmInnerFront,
            &hardpoints.lowerArmInnerRear,
            &hardpoints.lowerBallJoint,
            &hardpoints.tieRodInner,
            &hardpoints.tieRodOuter,
            &hardpoints.wheelCenter
        };
        for (heritage::math::Vec3* point : points)
            point->x = -point->x;
    }
    return hardpoints;
}

} // namespace

bool suspensionGeometryProducesAuthoritativePose()
{
    heritage::vehicles::SuspensionGeometryDescription description;
    description.localSteeringAxis = { 0.0f, 1.0f, 0.0f };
    description.staticCamberDegrees = 1.5f;
    description.camberGainDegreesPerM = -5.0f;
    description.camberProgressionDegreesPerM2 = 20.0f;
    description.staticToeDegrees = -0.25f;
    description.toeGainDegreesPerM = 3.0f;
    description.toeProgressionDegreesPerM2 = -10.0f;
    const auto output = heritage::vehicles::evaluateSuspensionGeometry(
        description,
        { 0.10f, 12.0f });
    const float forwardLength = magnitude(output.localWheelForward);
    const float rightLength = magnitude(output.localWheelRight);
    const float upLength = magnitude(output.localWheelUp);
    const float forwardRightDot =
        output.localWheelForward.x * output.localWheelRight.x
        + output.localWheelForward.y * output.localWheelRight.y
        + output.localWheelForward.z * output.localWheelRight.z;
    const float expectedForwardX = std::sin(12.0f
        * 3.14159265358979323846f / 180.0f);
    const bool poseWorked =
        std::abs(output.camberDegrees - 1.10f) <= 0.0001f
        && std::abs(output.toeDegrees) <= 0.0001f
        && std::abs(forwardLength - 1.0f) <= 0.0001f
        && std::abs(rightLength - 1.0f) <= 0.0001f
        && std::abs(upLength - 1.0f) <= 0.0001f
        && std::abs(forwardRightDot) <= 0.0001f
        && std::abs(output.localWheelForward.x - expectedForwardX)
            <= 0.0001f
        && std::abs(output.localUprightRotationDegrees.z - 1.10f)
            <= 0.05f;
    std::cout
        << "suspension_geometry camber_deg=" << output.camberDegrees
        << " toe_deg=" << output.toeDegrees
        << " upright_xyz_deg="
        << output.localUprightRotationDegrees.x << ','
        << output.localUprightRotationDegrees.y << ','
        << output.localUprightRotationDegrees.z
        << " orthogonality=" << forwardRightDot
        << '\n';
    return poseWorked;
}

bool unsprungMassSettlesAndRespondsToRoadStep()
{
    heritage::vehicles::UnsprungMassDescription description;
    description.effectiveMassKg = 38.0f;
    description.tireRadialStiffnessNPerM = 220000.0f;
    description.tireRadialDampingNsPerM = 1800.0f;
    description.maximumTireDeflectionM = 0.08f;
    description.maximumNormalForceN = 250000.0f;
    heritage::vehicles::UnsprungMassState state;
    heritage::vehicles::UnsprungMassInput input;
    input.deltaTimeSeconds = 0.001f;
    input.restLengthM = 0.55f;
    input.minimumLengthM = 0.35f;
    input.maximumLengthM = 0.70f;
    input.suspensionForceN = 2700.0f;
    input.roadAvailable = true;
    input.roadHubLengthM = 0.50f;
    input.roadHubLengthVelocityMps = 0.0f;
    input.roadNormalAlignment = 1.0f;

    heritage::vehicles::UnsprungMassOutput output;
    for (int step = 0; step < 3000; ++step)
    {
        output = heritage::vehicles::advanceUnsprungMassModel(
            description, input, state);
    }
    const heritage::vehicles::VehicleScalar expectedDeflection = input.suspensionForceN
        / description.tireRadialStiffnessNPerM;
    const bool settled = std::abs(
            output.tireDeflectionM - expectedDeflection) < 0.0002f
        && std::abs(output.suspensionLengthVelocityMps) < 0.002f
        && std::abs(output.normalForceN - input.suspensionForceN) < 15.0f;

    input.roadHubLengthM -= 0.02f;
    heritage::vehicles::VehicleScalar peakNormalForce = 0.0;
    heritage::vehicles::VehicleScalar peakUnsprungSpeed = 0.0;
    int velocityReversals = 0;
    heritage::vehicles::VehicleScalar previousVelocity = output.suspensionLengthVelocityMps;
    for (int step = 0; step < 2000; ++step)
    {
        output = heritage::vehicles::advanceUnsprungMassModel(
            description, input, state);
        peakNormalForce = std::max(peakNormalForce, output.normalForceN);
        peakUnsprungSpeed = std::max(
            peakUnsprungSpeed,
            std::abs(output.suspensionLengthVelocityMps));
        if (previousVelocity * output.suspensionLengthVelocityMps < 0.0f)
            ++velocityReversals;
        previousVelocity = output.suspensionLengthVelocityMps;
    }
    const bool roadStepResponded = peakNormalForce > 5000.0f
        && peakUnsprungSpeed > 0.10f
        && velocityReversals >= 2
        && std::abs(output.tireDeflectionM - expectedDeflection) < 0.0002f
        && std::abs(output.suspensionLengthVelocityMps) < 0.002f;
    std::cout
        << "unsprung_mass settled_deflection_m=" << output.tireDeflectionM
        << " expected_deflection_m=" << expectedDeflection
        << " road_step_peak_load_n=" << peakNormalForce
        << " road_step_peak_speed_mps=" << peakUnsprungSpeed
        << " velocity_reversals=" << velocityReversals
        << '\n';
    return settled && roadStepResponded;
}

bool macPhersonHardpointKinematicsAreDeterministic()
{
    heritage::vehicles::SuspensionGeometryDescription rightDescription;
    rightDescription.provider =
        heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1;
    rightDescription.macPherson = syntheticMacPhersonCorner(false);
    rightDescription.staticToeDegrees = 0.10f;

    heritage::vehicles::SuspensionGeometryDescription leftDescription =
        rightDescription;
    leftDescription.macPherson = syntheticMacPhersonCorner(true);
    leftDescription.staticToeDegrees = -0.10f;

    const auto rightRest = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { 0.0f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto rightBump = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto rightSteered = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { 0.08f, 12.0f, { 0.0f, -1.0f, 0.0f } });
    const auto leftBump = heritage::vehicles::evaluateSuspensionGeometry(
        leftDescription, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });

    const bool restReproduced = rightRest.kinematicsValid
        && std::abs(rightRest.localWheelCenter.x - 0.72f) <= 0.0001f
        && std::abs(rightRest.localWheelCenter.y - 0.34f) <= 0.0001f
        && std::abs(rightRest.toeDegrees - 0.10f) <= 0.001f
        && std::abs(rightRest.strutCompressionM) <= 0.0001f;
    const bool bumpSolved = rightBump.kinematicsValid
        && !rightBump.travelClamped
        && rightBump.localWheelCenter.y > rightRest.localWheelCenter.y + 0.06f
        && rightBump.strutCompressionM > 0.06f
        && rightBump.springMotionRatio > 0.70f
        && rightBump.springMotionRatio < 1.20f
        && std::abs(rightBump.camberDegrees - rightRest.camberDegrees) > 0.5f
        && std::abs(rightBump.bumpSteerDegrees) > 0.1f;
    const bool mirrored = leftBump.kinematicsValid
        && std::abs(leftBump.localWheelCenter.x + rightBump.localWheelCenter.x)
            <= 0.0001f
        && std::abs(leftBump.camberDegrees + rightBump.camberDegrees)
            <= 0.001f
        && std::abs(leftBump.bumpSteerDegrees + rightBump.bumpSteerDegrees)
            <= 0.001f
        && std::abs(leftBump.springMotionRatio - rightBump.springMotionRatio)
            <= 0.0001f;
    const bool steeringApplied = rightSteered.kinematicsValid
        && std::abs(
            rightSteered.localWheelForward.x
            - rightBump.localWheelForward.x) > 0.05f;

    std::cout
        << "macpherson bump_camber_deg=" << rightBump.camberDegrees
        << " bump_steer_deg=" << rightBump.bumpSteerDegrees
        << " strut_compression_m=" << rightBump.strutCompressionM
        << " motion_ratio=" << rightBump.springMotionRatio
        << " mirrored_camber_deg=" << leftBump.camberDegrees
        << '\n';
    return restReproduced && bumpSolved && mirrored && steeringApplied;
}


bool assistedFrontMacPhersonVehicleStaysStable()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the assisted-MacPherson prototype world.\n";
        return false;
    }

    constexpr float kFrontHalfTrackM = 0.7185f;
    constexpr float kFrontWheelCenterYM = 0.30f;
    constexpr float kFrontAxleZM = 1.221f;
    constexpr float kWheelRadiusM = 0.2979f;
    constexpr float kCasterDegrees = 3.266667f;
    constexpr float kSaiDegrees = 9.70f;
    constexpr float kToeOutPerWheelDegrees = 0.116667f;

    for (std::size_t wheelIndex = 0; wheelIndex < 2; ++wheelIndex)
    {
        const bool leftSide = wheelIndex == 0;
        heritage::vehicles::MacPhersonHardpointEstimateInput input;
        input.wheelCenter = {
            leftSide ? -kFrontHalfTrackM : kFrontHalfTrackM,
            kFrontWheelCenterYM,
            kFrontAxleZM
        };
        input.referencePackageScaleM = kWheelRadiusM;
        input.casterDegrees = kCasterDegrees;
        input.steeringAxisInclinationDegrees = kSaiDegrees;
        const auto estimate =
            heritage::vehicles::estimateMacPhersonHardpointsV1(input);
        if (!estimate.valid)
            return false;

        heritage::vehicles::SuspensionGeometryDescription geometry;
        if (!world.vehicles.wheelSuspensionGeometry(
                world.vehicle, wheelIndex, geometry))
        {
            return false;
        }
        geometry.provider =
            heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1;
        geometry.macPherson = estimate.hardpoints;
        geometry.staticCamberDegrees = 0.0f;
        geometry.staticToeDegrees = leftSide
            ? -kToeOutPerWheelDegrees
            : kToeOutPerWheelDegrees;
        if (!world.vehicles.setWheelSuspensionGeometry(
                world.vehicle, wheelIndex, geometry))
        {
            return false;
        }
    }

    // Isolate suspension stability from the current tire model's intentionally
    // minimal free-rolling resistance. The rear parking brake keeps the chassis
    // stationary while the estimated front linkage settles at 1000 Hz.
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    const StabilitySample sample = sampleStability(world, 4.0f, 3.0f);
    printSample("assisted_front_macpherson_1000hz", sample);
    printWheelStates(world, "assisted_front_macpherson_1000hz");

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    return horizontalMagnitude(displacement) <= 0.010f
        && std::abs(displacement.y) <= 0.010f
        && sample.maximumHorizontalSpeed <= 0.020f
        && sample.maximumVerticalSpeed <= 0.030f
        && sample.maximumAngularSpeedDegrees <= 0.75f
        && sample.verticalPositionSpan <= 0.010f
        && sample.minimumGroundedWheels == 4;
}


bool assistedMacPhersonEstimateIsPlausibleAndMirrored()
{
    heritage::vehicles::MacPhersonHardpointEstimateInput rightInput;
    rightInput.wheelCenter = { 0.7185f, 0.30f, 1.221f };
    rightInput.referencePackageScaleM = 0.2979f;
    rightInput.casterDegrees = 3.266667f;
    rightInput.steeringAxisInclinationDegrees = 9.70f;
    const auto rightEstimate =
        heritage::vehicles::estimateMacPhersonHardpointsV1(rightInput);

    auto leftInput = rightInput;
    leftInput.wheelCenter.x = -leftInput.wheelCenter.x;
    const auto leftEstimate =
        heritage::vehicles::estimateMacPhersonHardpointsV1(leftInput);

    const auto& right = rightEstimate.hardpoints;
    const auto& left = leftEstimate.hardpoints;
    const bool sourceContract = rightEstimate.valid
        && leftEstimate.valid
        && rightEstimate.profileId == "estimated_macpherson_road_v1"
        && leftEstimate.profileId == rightEstimate.profileId
        && std::abs(rightEstimate.confidence - 0.35f) <= 0.0001f
        && std::abs(leftEstimate.confidence - rightEstimate.confidence)
            <= 0.0001f;
    const bool packagePlausible = sourceContract
        && std::abs(right.wheelCenter.x - rightInput.wheelCenter.x)
            <= 0.0001f
        && right.strutTopMount.y > right.wheelCenter.y + 0.60f
        && right.strutTopMount.x < right.lowerBallJoint.x
        && right.lowerArmInnerFront.x < right.lowerBallJoint.x
        && right.lowerArmInnerRear.x < right.lowerBallJoint.x
        && right.tieRodInner.x < right.tieRodOuter.x;

    const heritage::math::Vec3* rightPoints[] = {
        &right.strutTopMount,
        &right.strutUprightMount,
        &right.lowerArmInnerFront,
        &right.lowerArmInnerRear,
        &right.lowerBallJoint,
        &right.tieRodInner,
        &right.tieRodOuter,
        &right.wheelCenter
    };
    const heritage::math::Vec3* leftPoints[] = {
        &left.strutTopMount,
        &left.strutUprightMount,
        &left.lowerArmInnerFront,
        &left.lowerArmInnerRear,
        &left.lowerBallJoint,
        &left.tieRodInner,
        &left.tieRodOuter,
        &left.wheelCenter
    };
    bool mirrored = true;
    for (std::size_t index = 0; index < 8; ++index)
    {
        mirrored = mirrored
            && std::abs(leftPoints[index]->x + rightPoints[index]->x)
                <= 0.0001f
            && std::abs(leftPoints[index]->y - rightPoints[index]->y)
                <= 0.0001f
            && std::abs(leftPoints[index]->z - rightPoints[index]->z)
                <= 0.0001f;
    }

    heritage::vehicles::SuspensionGeometryDescription description;
    description.provider =
        heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1;
    description.macPherson = right;
    const auto rest = heritage::vehicles::evaluateSuspensionGeometry(
        description, { 0.0f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto bump = heritage::vehicles::evaluateSuspensionGeometry(
        description, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto droop = heritage::vehicles::evaluateSuspensionGeometry(
        description, { -0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const bool usableKinematics = rest.kinematicsValid
        && bump.kinematicsValid
        && droop.kinematicsValid
        && !bump.travelClamped
        && !droop.travelClamped
        && bump.springMotionRatio > 0.70f
        && bump.springMotionRatio < 1.20f
        && std::abs(bump.bumpSteerDegrees) < 2.0f
        && std::abs(droop.bumpSteerDegrees) < 2.0f
        && std::abs(bump.camberDegrees - rest.camberDegrees) > 0.25f
        && std::abs(droop.camberDegrees - rest.camberDegrees) > 0.25f;

    heritage::vehicles::MacPhersonHardpointEstimateInput invalidInput = rightInput;
    invalidInput.referencePackageScaleM = 0.10f;
    const bool invalidRejected =
        !heritage::vehicles::estimateMacPhersonHardpointsV1(invalidInput).valid;

    std::cout
        << "macpherson_estimate confidence=" << rightEstimate.confidence
        << " bump_camber_deg=" << bump.camberDegrees
        << " bump_steer_deg=" << bump.bumpSteerDegrees
        << " droop_camber_deg=" << droop.camberDegrees
        << " motion_ratio=" << bump.springMotionRatio
        << '\n';
    return packagePlausible && mirrored && usableKinematics && invalidRejected;
}


bool trailingArmTorsionBarKinematicsAreDeterministic()
{
    heritage::vehicles::TrailingArmHardpointEstimateInput rightInput;
    rightInput.wheelCenter = { 0.7140f, 0.30f, -1.221f };
    rightInput.referencePackageScaleM = 0.2979f;
    const auto rightEstimate =
        heritage::vehicles::estimateTrailingArmHardpointsV1(rightInput);

    auto leftInput = rightInput;
    leftInput.wheelCenter.x = -leftInput.wheelCenter.x;
    const auto leftEstimate =
        heritage::vehicles::estimateTrailingArmHardpointsV1(leftInput);
    if (!rightEstimate.valid || !leftEstimate.valid)
        return false;

    heritage::vehicles::SuspensionGeometryDescription rightDescription;
    rightDescription.provider =
        heritage::vehicles::SuspensionProviderKind::TrailingArmTorsionBarV1;
    rightDescription.trailingArm = rightEstimate.hardpoints;
    rightDescription.staticCamberDegrees = 1.0f;
    rightDescription.staticToeDegrees = -0.266667f;

    heritage::vehicles::SuspensionGeometryDescription leftDescription;
    leftDescription.provider =
        heritage::vehicles::SuspensionProviderKind::TrailingArmTorsionBarV1;
    leftDescription.trailingArm = leftEstimate.hardpoints;
    leftDescription.staticCamberDegrees = -1.0f;
    leftDescription.staticToeDegrees = 0.266667f;

    const auto rightRest = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { 0.0f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto rightBump = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto rightDroop = heritage::vehicles::evaluateSuspensionGeometry(
        rightDescription, { -0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto leftBump = heritage::vehicles::evaluateSuspensionGeometry(
        leftDescription, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });

    heritage::vehicles::SuspensionModelDescription model;
    model.provider =
        heritage::vehicles::SuspensionProviderKind::TrailingArmTorsionBarV1;
    model.springRateNPerM = 35000.0;
    model.springProgressionNPerM2 = 15000.0;
    model.bumpDampingNsPerM = 3200.0;
    model.reboundDampingNsPerM = 4200.0;
    model.motionRatio = rightBump.damperMotionRatio;
    const auto spring = heritage::vehicles::evaluateSuspensionModel(
        model,
        { 0.08,
          0.0,
          rightBump.springTwistRadians,
          rightBump.springAngularMotionRatioRadPerM,
          rightBump.referenceSpringAngularMotionRatioRadPerM });

    const auto& r = rightEstimate.hardpoints;
    const auto& l = leftEstimate.hardpoints;
    const heritage::math::Vec3* rightPoints[] = {
        &r.armPivotInner, &r.armPivotOuter, &r.wheelCenter,
        &r.damperUpperMount, &r.damperLowerMount
    };
    const heritage::math::Vec3* leftPoints[] = {
        &l.armPivotInner, &l.armPivotOuter, &l.wheelCenter,
        &l.damperUpperMount, &l.damperLowerMount
    };
    bool mirroredHardpoints = true;
    for (std::size_t index = 0; index < 5; ++index)
    {
        mirroredHardpoints = mirroredHardpoints
            && std::abs(leftPoints[index]->x + rightPoints[index]->x)
                <= 0.0001f
            && std::abs(leftPoints[index]->y - rightPoints[index]->y)
                <= 0.0001f
            && std::abs(leftPoints[index]->z - rightPoints[index]->z)
                <= 0.0001f;
    }

    const bool sourceContract = rightEstimate.profileId
            == "estimated_trailing_arm_torsion_bar_road_v1"
        && leftEstimate.profileId == rightEstimate.profileId
        && std::abs(rightEstimate.confidence - 0.30f) <= 0.0001f;
    const bool travelSolved = rightRest.kinematicsValid
        && rightBump.kinematicsValid
        && rightDroop.kinematicsValid
        && !rightBump.travelClamped
        && !rightDroop.travelClamped
        && std::abs(rightRest.localWheelCenter.y - 0.30f) <= 0.0001f
        && rightBump.localWheelCenter.y > rightRest.localWheelCenter.y + 0.07f
        && rightDroop.localWheelCenter.y < rightRest.localWheelCenter.y - 0.07f
        && rightBump.springTwistRadians > 0.05f
        && rightDroop.springTwistRadians < -0.05f
        && rightBump.springAngularMotionRatioRadPerM > 1.0f
        && rightBump.springAngularMotionRatioRadPerM < 3.0f
        && rightBump.damperMotionRatio > 0.30f
        && rightBump.damperMotionRatio < 1.20f
        && rightBump.damperCompressionM > 0.03f;
    const bool mirroredMotion = leftBump.kinematicsValid
        && std::abs(leftBump.localWheelCenter.x + rightBump.localWheelCenter.x)
            <= 0.0001f
        && std::abs(leftBump.localWheelCenter.y - rightBump.localWheelCenter.y)
            <= 0.0001f
        && std::abs(leftBump.localWheelCenter.z - rightBump.localWheelCenter.z)
            <= 0.0001f
        && std::abs(leftBump.springTwistRadians - rightBump.springTwistRadians)
            <= 0.0001f
        && std::abs(leftBump.damperMotionRatio - rightBump.damperMotionRatio)
            <= 0.0001f;
    const bool torsionSpringWorked = spring.normalForceN > 2600.0
        && spring.normalForceN < 3200.0
        && spring.springForceN > 2600.0
        && spring.springForceN < 3200.0;

    heritage::vehicles::TrailingArmHardpointEstimateInput invalidInput = rightInput;
    invalidInput.referencePackageScaleM = 0.10f;
    const bool invalidRejected =
        !heritage::vehicles::estimateTrailingArmHardpointsV1(invalidInput).valid;

    std::cout
        << "trailing_arm bump_twist_rad=" << rightBump.springTwistRadians
        << " angular_ratio_rad_per_m="
        << rightBump.springAngularMotionRatioRadPerM
        << " damper_ratio=" << rightBump.damperMotionRatio
        << " spring_force_n=" << spring.springForceN
        << " wheel_path_z_m="
        << (rightBump.localWheelCenter.z - rightRest.localWheelCenter.z)
        << '\n';
    return sourceContract && mirroredHardpoints && travelSolved
        && mirroredMotion && torsionSpringWorked && invalidRejected;
}

bool assistedFrontRearSuspensionVehicleStaysStable()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the full assisted-suspension prototype world.\n";
        return false;
    }

    constexpr float kReferencePackageScaleM = 0.2979f;
    constexpr float kCasterDegrees = 3.266667f;
    constexpr float kSaiDegrees = 9.70f;
    constexpr float kFrontToeDegrees = 0.116667f;
    constexpr float kRearToeDegrees = 0.266667f;

    for (std::size_t wheelIndex = 0; wheelIndex < 4; ++wheelIndex)
    {
        const bool leftSide = wheelIndex == 0 || wheelIndex == 2;
        const bool front = wheelIndex < 2;
        const float x = front
            ? (leftSide ? -0.7185f : 0.7185f)
            : (leftSide ? -0.7140f : 0.7140f);
        const float z = front ? 1.221f : -1.221f;

        heritage::vehicles::SuspensionGeometryDescription geometry;
        if (!world.vehicles.wheelSuspensionGeometry(
                world.vehicle, wheelIndex, geometry))
        {
            return false;
        }

        if (front)
        {
            heritage::vehicles::MacPhersonHardpointEstimateInput input;
            input.wheelCenter = { x, 0.30f, z };
            input.referencePackageScaleM = kReferencePackageScaleM;
            input.casterDegrees = kCasterDegrees;
            input.steeringAxisInclinationDegrees = kSaiDegrees;
            const auto estimate =
                heritage::vehicles::estimateMacPhersonHardpointsV1(input);
            if (!estimate.valid)
                return false;
            geometry.provider =
                heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1;
            geometry.macPherson = estimate.hardpoints;
            geometry.trailingArm = {};
            geometry.staticCamberDegrees = 0.0f;
            geometry.staticToeDegrees = leftSide
                ? -kFrontToeDegrees : kFrontToeDegrees;
        }
        else
        {
            heritage::vehicles::TrailingArmHardpointEstimateInput input;
            input.wheelCenter = { x, 0.30f, z };
            input.referencePackageScaleM = kReferencePackageScaleM;
            const auto estimate =
                heritage::vehicles::estimateTrailingArmHardpointsV1(input);
            if (!estimate.valid)
                return false;
            geometry.provider = heritage::vehicles::SuspensionProviderKind::
                TrailingArmTorsionBarV1;
            geometry.trailingArm = estimate.hardpoints;
            geometry.macPherson = {};
            geometry.staticCamberDegrees = leftSide ? -1.0f : 1.0f;
            geometry.staticToeDegrees = leftSide
                ? kRearToeDegrees : -kRearToeDegrees;
        }

        if (!world.vehicles.setWheelSuspensionGeometry(
                world.vehicle, wheelIndex, geometry))
        {
            return false;
        }
    }

    // Parking brake isolates suspension settling from free rolling while all
    // four corners run mechanism-specific geometry at the 1000 Hz vehicle rate.
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    const StabilitySample sample = sampleStability(world, 4.0f, 3.0f);
    printSample("assisted_front_rear_suspension_1000hz", sample);
    printWheelStates(world, "assisted_front_rear_suspension_1000hz");

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    return horizontalMagnitude(displacement) <= 0.015f
        && std::abs(displacement.y) <= 0.015f
        && sample.maximumHorizontalSpeed <= 0.030f
        && sample.maximumVerticalSpeed <= 0.040f
        && sample.maximumAngularSpeedDegrees <= 1.00f
        && sample.verticalPositionSpan <= 0.015f
        && sample.minimumGroundedWheels == 4;
}


bool suspensionAntiRollBarCouplesWheelPairs()
{
    heritage::vehicles::SuspensionAntiRollBarDescription bar;
    bar.leftWheelIndex = 0;
    bar.rightWheelIndex = 1;
    bar.torsionalStiffnessNmPerRad = 800.0;
    bar.torsionalDampingNmsPerRad = 20.0;
    bar.leftLeverArmM = 0.20;
    bar.rightLeverArmM = 0.20;
    bar.leftLinkMotionRatio = 1.0;
    bar.rightLinkMotionRatio = 1.0;
    bar.maximumWheelForceN = 12000.0;

    const auto equal = heritage::vehicles::evaluateSuspensionAntiRollBar(
        bar, { 0.05, 0.05, 0.0, 0.0 });
    const auto loadedLeft = heritage::vehicles::evaluateSuspensionAntiRollBar(
        bar, { 0.08, 0.02, 0.20, -0.10 });
    const auto loadedRight = heritage::vehicles::evaluateSuspensionAntiRollBar(
        bar, { 0.02, 0.08, -0.10, 0.20 });

    heritage::vehicles::SuspensionAntiRollBarDescription invalid = bar;
    invalid.leftLeverArmM = 0.0;
    const bool mathContract =
        std::abs(equal.totalTorqueNm) <= 0.000001
        && loadedLeft.twistRadians > 0.25
        && loadedLeft.leftWheelForceN > 1000.0
        && loadedLeft.rightWheelForceN < -1000.0
        && std::abs(loadedLeft.leftWheelForceN
            + loadedLeft.rightWheelForceN) <= 0.000001
        && std::abs(loadedLeft.totalTorqueNm
            + loadedRight.totalTorqueNm) <= 0.000001
        && !heritage::vehicles::validSuspensionAntiRollBarDescription(invalid);

    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
        return false;
    if (!world.vehicles.setAntiRollBar(world.vehicle, 0, bar))
        return false;

    auto rearBar = bar;
    rearBar.leftWheelIndex = 2;
    rearBar.rightWheelIndex = 3;
    rearBar.torsionalStiffnessNmPerRad = 600.0;
    if (!world.vehicles.setAntiRollBar(world.vehicle, 1, rearBar)
        || world.vehicles.antiRollBarCount(world.vehicle) != 2)
    {
        return false;
    }

    heritage::physics::RigidBodyPose pose;
    if (!world.bodies.pose(world.chassis, pose))
        return false;
    pose.rotationDegrees.z = 3.0f;
    if (!world.bodies.setPose(world.chassis, pose))
        return false;
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    stepWorld(world);
    stepWorld(world);

    heritage::vehicles::SuspensionAntiRollBarDescription queried;
    heritage::vehicles::SuspensionAntiRollBarOutput state;
    const bool queriedOk = world.vehicles.antiRollBar(
        world.vehicle, 0, queried, state);
    const bool integrationContract = queriedOk
        && queried.leftWheelIndex == 0
        && queried.rightWheelIndex == 1
        && std::isfinite(state.totalTorqueNm)
        && std::isfinite(state.leftWheelForceN)
        && std::isfinite(state.rightWheelForceN)
        && std::abs(state.totalTorqueNm) > 0.01;

    std::cout
        << "anti_roll_bar twist_rad=" << loadedLeft.twistRadians
        << " torque_nm=" << loadedLeft.totalTorqueNm
        << " left_force_n=" << loadedLeft.leftWheelForceN
        << " right_force_n=" << loadedLeft.rightWheelForceN
        << " integrated_torque_nm=" << state.totalTorqueNm
        << '\n';
    return mathContract && integrationContract;
}


} // namespace heritage::tests
