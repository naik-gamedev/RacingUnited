#include "PhysicsRegressionCommon.hpp"
#include "../Vehicles/Suspension/Authoring/MacPhersonHardpointEstimator.hpp"
#include "../Vehicles/Suspension/Authoring/TrailingArmHardpointEstimator.hpp"
#include "../Vehicles/Suspension/Springs/TorsionBar.hpp"
#include "../Vehicles/Suspension/Common/SuspensionAntiRollBar.hpp"
#include "../Vehicles/Suspension/Geometry/PushrodDoubleWishbone/PushrodDoubleWishboneKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/LiveAxle/LiveAxleKinematics.hpp"
#include "../Vehicles/Suspension/Springs/LeafSpring/LeafSpringLiveAxle.hpp"
#include "../Vehicles/Suspension/Springs/LeafSpring/LeafSpringAxleDynamics.hpp"
#include "../Vehicles/Suspension/Geometry/MotorcycleFork/MotorcycleForkKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/MotorcycleSwingarm/MotorcycleSwingarmKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/Kart/KartChassisKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/MultiLink/MultiLinkKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/SemiTrailingArm/SemiTrailingArmKinematics.hpp"
#include "../Vehicles/Suspension/Geometry/TwistBeam/TwistBeamKinematics.hpp"

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

heritage::vehicles::DoubleWishboneHardpoints syntheticDoubleWishboneCorner(
    bool leftSide)
{
    heritage::vehicles::DoubleWishboneHardpoints h;
    h.authored = true;
    h.upperArmInnerFront = { 0.34f, 0.74f, 1.45f };
    h.upperArmInnerRear = { 0.34f, 0.74f, 0.95f };
    h.upperBallJoint = { 0.71f, 0.67f, 1.20f };
    h.lowerArmInnerFront = { 0.25f, 0.35f, 1.45f };
    h.lowerArmInnerRear = { 0.25f, 0.35f, 0.95f };
    h.lowerBallJoint = { 0.78f, 0.31f, 1.20f };
    h.tieRodInner = { 0.31f, 0.47f, 1.08f };
    h.tieRodOuter = { 0.69f, 0.40f, 1.13f };
    h.wheelCenter = { 0.735f, 0.45f, 1.20f };
    h.damperUpperMount = { 0.47f, 1.02f, 1.20f };
    h.damperLowerMount = { 0.49f, 0.37f, 1.20f };
    if (leftSide)
    {
        heritage::math::Vec3* points[] = {
            &h.upperArmInnerFront, &h.upperArmInnerRear, &h.upperBallJoint,
            &h.lowerArmInnerFront, &h.lowerArmInnerRear, &h.lowerBallJoint,
            &h.tieRodInner, &h.tieRodOuter, &h.wheelCenter,
            &h.damperUpperMount, &h.damperLowerMount
        };
        for (heritage::math::Vec3* point : points)
            point->x = -point->x;
    }
    return h;
}

heritage::vehicles::PushrodDoubleWishboneHardpoints
syntheticPushrodDoubleWishboneCorner(bool leftSide)
{
    heritage::vehicles::PushrodDoubleWishboneHardpoints h;
    h.authored = true;
    h.wishbone = syntheticDoubleWishboneCorner(false);
    // SUSP07 uses linkage-only wishbone authority, so direct damper points are
    // irrelevant here. Keep them authored for compatibility with the shared
    // source helper while the rocker owns actual spring/damper actuation.
    h.pushrodLowerArmMount = { 0.60f, 0.33f, 1.20f };
    h.rockerPivotFront = { 0.38f, 0.90f, 1.32f };
    h.rockerPivotRear = { 0.38f, 0.90f, 1.08f };
    h.rockerPushrodMount = { 0.52f, 0.90f, 1.20f };
    h.springRockerMount = { 0.38f, 1.04f, 1.20f };
    h.springChassisMount = { 0.18f, 1.04f, 1.20f };
    h.damperRockerMount = { 0.38f, 1.00f, 1.20f };
    h.damperChassisMount = { 0.18f, 1.00f, 1.20f };
    if (leftSide)
    {
        h.wishbone = syntheticDoubleWishboneCorner(true);
        heritage::math::Vec3* points[] = {
            &h.pushrodLowerArmMount,
            &h.rockerPivotFront,
            &h.rockerPivotRear,
            &h.rockerPushrodMount,
            &h.springRockerMount,
            &h.springChassisMount,
            &h.damperRockerMount,
            &h.damperChassisMount
        };
        for (heritage::math::Vec3* point : points)
            point->x = -point->x;
    }
    return h;
}

heritage::vehicles::MultiLinkHardpoints syntheticMultiLinkCorner()
{
    heritage::vehicles::MultiLinkHardpoints h; h.authored = true;
    h.link1Inner={.30f,.66f,.12f}; h.link1Outer={.65f,.62f,.10f};
    h.link2Inner={.28f,.60f,-.20f}; h.link2Outer={.66f,.58f,-.10f};
    h.link3Inner={.32f,.36f,.28f}; h.link3Outer={.68f,.38f,.15f};
    h.link4Inner={.35f,.32f,-.30f}; h.link4Outer={.70f,.34f,-.12f};
    h.toeLinkInner={.30f,.46f,-.22f}; h.toeLinkOuter={.72f,.46f,-.18f};
    h.wheelCenter={.75f,.48f,0.0f};
    h.springUpperMount={.45f,.90f,.05f}; h.springLowerMount={.68f,.35f,.05f};
    h.damperUpperMount={.42f,.88f,-.08f}; h.damperLowerMount={.70f,.38f,-.08f};
    h.steeringRackAxisStart={.10f,.46f,-.22f}; h.steeringRackAxisEnd={.50f,.46f,-.22f};
    return h;
}


heritage::vehicles::SemiTrailingArmHardpoints syntheticSemiTrailingArm(bool leftSide)
{
    const float side = leftSide ? -1.0f : 1.0f;
    heritage::vehicles::SemiTrailingArmHardpoints h; h.authored=true;
    h.armPivotInner={0.20f*side,0.0f,-0.95f};
    h.armPivotOuter={0.62f*side,0.01f,-1.08f};
    h.wheelCenter={0.76f*side,-0.34f,-1.28f};
    h.springUpperMount={0.46f*side,0.34f,-1.05f};
    h.springLowerMount={0.60f*side,-0.18f,-1.18f};
    h.damperUpperMount={0.52f*side,0.36f,-0.96f};
    h.damperLowerMount={0.64f*side,-0.20f,-1.23f};
    return h;
}

heritage::vehicles::TwistBeamHardpoints syntheticTwistBeam()
{
    heritage::vehicles::TwistBeamHardpoints h; h.authored=true;
    h.leftArm=syntheticSemiTrailingArm(true);
    h.rightArm=syntheticSemiTrailingArm(false);
    h.beamLeftAttachment={-0.48f,-0.08f,-1.05f};
    h.beamRightAttachment={0.48f,-0.08f,-1.05f};
    return h;
}

heritage::vehicles::LiveAxleHardpoints syntheticLiveAxle()
{
    heritage::vehicles::LiveAxleHardpoints h;
    h.authored = true;
    h.axleCenter = { 0.0f, 0.30f, -1.221f };
    h.leftWheelCenter = { -0.714f, 0.30f, -1.221f };
    h.rightWheelCenter = { 0.714f, 0.30f, -1.221f };
    h.panhardChassisMount = { -0.55f, 0.58f, -1.18f };
    h.panhardAxleMount = { 0.55f, 0.30f, -1.18f };
    h.leftTrailingChassisMount = { -0.45f, 0.42f, -0.30f };
    h.leftTrailingAxleMount = { -0.45f, 0.27f, -1.17f };
    h.rightTrailingChassisMount = { 0.45f, 0.42f, -0.30f };
    h.rightTrailingAxleMount = { 0.45f, 0.27f, -1.17f };
    h.leftSpringChassisMount = { -0.50f, 0.78f, -1.221f };
    h.leftSpringAxleMount = { -0.50f, 0.30f, -1.221f };
    h.rightSpringChassisMount = { 0.50f, 0.78f, -1.221f };
    h.rightSpringAxleMount = { 0.50f, 0.30f, -1.221f };
    h.leftDamperChassisMount = { -0.58f, 0.82f, -1.03f };
    h.leftDamperAxleMount = { -0.48f, 0.28f, -1.28f };
    h.rightDamperChassisMount = { 0.58f, 0.82f, -1.03f };
    h.rightDamperAxleMount = { 0.48f, 0.28f, -1.28f };
    return h;
}

heritage::vehicles::LeafSpringLiveAxleHardpoints syntheticLeafSpringLiveAxle()
{
    heritage::vehicles::LeafSpringLiveAxleHardpoints h;
    h.authored = true;
    h.axle = syntheticLiveAxle();
    h.leftLeafFrontEye = { -0.50f, 0.55f, -0.35f };
    h.leftLeafRearShacklePivot = { -0.50f, 0.72f, -2.08f };
    h.leftLeafRearEye = { -0.50f, 0.54f, -2.02f };
    h.leftLeafAxleClamp = { -0.50f, 0.30f, -1.221f };
    h.rightLeafFrontEye = { 0.50f, 0.55f, -0.35f };
    h.rightLeafRearShacklePivot = { 0.50f, 0.72f, -2.08f };
    h.rightLeafRearEye = { 0.50f, 0.54f, -2.02f };
    h.rightLeafAxleClamp = { 0.50f, 0.30f, -1.221f };
    return h;
}

heritage::vehicles::KartChassisHardpoints syntheticKartChassis()
{
    heritage::vehicles::KartChassisHardpoints h;
    h.authored = true;
    h.frontLeftKingpinUpper = { -0.44f, 0.48f, 0.62f };
    h.frontLeftKingpinLower = { -0.49f, 0.15f, 0.68f };
    h.frontLeftWheelCenter = { -0.56f, 0.22f, 0.70f };
    h.frontRightKingpinUpper = { 0.44f, 0.48f, 0.62f };
    h.frontRightKingpinLower = { 0.49f, 0.15f, 0.68f };
    h.frontRightWheelCenter = { 0.56f, 0.22f, 0.70f };
    h.rearAxleBearingLeft = { -0.34f, 0.22f, -0.63f };
    h.rearAxleBearingRight = { 0.34f, 0.22f, -0.63f };
    h.rearLeftWheelCenter = { -0.59f, 0.22f, -0.63f };
    h.rearRightWheelCenter = { 0.59f, 0.22f, -0.63f };
    return h;
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

bool hardpointWheelPathMovesPhysicalSupportQuery()
{
    heritage::vehicles::SuspensionGeometryDescription right;
    right.provider = heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1;
    right.macPherson = syntheticMacPhersonCorner(false);

    heritage::vehicles::SuspensionGeometryDescription left = right;
    left.macPherson = syntheticMacPhersonCorner(true);

    const auto rest = heritage::vehicles::evaluateSuspensionSupportOffset(
        right, { 0.0f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto bump = heritage::vehicles::evaluateSuspensionSupportOffset(
        right, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto steer = heritage::vehicles::evaluateSuspensionSupportOffset(
        right, { 0.0f, 20.0f, { 0.0f, -1.0f, 0.0f } });
    const auto mirroredBump = heritage::vehicles::evaluateSuspensionSupportOffset(
        left, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });

    heritage::vehicles::SuspensionGeometryDescription linear;
    const auto linearOffset = heritage::vehicles::evaluateSuspensionSupportOffset(
        linear, { 0.08f, 20.0f, { 0.0f, -1.0f, 0.0f } });

    const float bumpMagnitude = std::sqrt(
        bump.localTransverseOffset.x * bump.localTransverseOffset.x
        + bump.localTransverseOffset.z * bump.localTransverseOffset.z);
    const float steerMagnitude = std::sqrt(
        steer.localTransverseOffset.x * steer.localTransverseOffset.x
        + steer.localTransverseOffset.z * steer.localTransverseOffset.z);
    const bool contract = rest.valid && bump.valid && steer.valid
        && mirroredBump.valid && linearOffset.valid
        && std::abs(rest.localTransverseOffset.x) <= 0.000001f
        && std::abs(rest.localTransverseOffset.y) <= 0.000001f
        && std::abs(rest.localTransverseOffset.z) <= 0.000001f
        && bumpMagnitude > 0.005f
        && steerMagnitude > 0.010f
        && std::abs(bump.localTransverseOffset.x
            + mirroredBump.localTransverseOffset.x) <= 0.0001f
        && std::abs(bump.localTransverseOffset.z
            - mirroredBump.localTransverseOffset.z) <= 0.0001f
        && std::abs(linearOffset.localTransverseOffset.x) <= 0.000001f
        && std::abs(linearOffset.localTransverseOffset.y) <= 0.000001f
        && std::abs(linearOffset.localTransverseOffset.z) <= 0.000001f;

    std::cout
        << "suspension_support_path bump_offset_mm="
        << bump.localTransverseOffset.x * 1000.0f << ','
        << bump.localTransverseOffset.z * 1000.0f
        << " steer_offset_mm="
        << steer.localTransverseOffset.x * 1000.0f << ','
        << steer.localTransverseOffset.z * 1000.0f
        << " mirrored_bump_x_mm="
        << mirroredBump.localTransverseOffset.x * 1000.0f
        << '\n';
    return contract;
}

bool doubleWishboneHardpointKinematicsAreDeterministic()
{
    heritage::vehicles::SuspensionGeometryDescription right;
    right.provider = heritage::vehicles::SuspensionProviderKind::DoubleWishboneV1;
    right.doubleWishbone = syntheticDoubleWishboneCorner(false);

    heritage::vehicles::SuspensionGeometryDescription left = right;
    left.doubleWishbone = syntheticDoubleWishboneCorner(true);

    const auto rest = heritage::vehicles::evaluateSuspensionGeometry(
        right, { 0.0f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto bump = heritage::vehicles::evaluateSuspensionGeometry(
        right, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto droop = heritage::vehicles::evaluateSuspensionGeometry(
        right, { -0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto steer = heritage::vehicles::evaluateSuspensionGeometry(
        right, { 0.0f, 20.0f, { 0.0f, -1.0f, 0.0f } });
    const auto mirroredBump = heritage::vehicles::evaluateSuspensionGeometry(
        left, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto support = heritage::vehicles::evaluateSuspensionSupportOffset(
        right, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });

    const bool solved = rest.kinematicsValid && bump.kinematicsValid
        && droop.kinematicsValid && steer.kinematicsValid
        && mirroredBump.kinematicsValid && support.valid
        && !bump.travelClamped && !droop.travelClamped
        && std::abs(rest.localWheelCenter.y - 0.45f) <= 0.0001f
        && bump.localWheelCenter.y > rest.localWheelCenter.y + 0.075f
        && droop.localWheelCenter.y < rest.localWheelCenter.y - 0.075f
        && bump.camberDegrees < -0.5f
        && droop.camberDegrees > 1.0f
        && bump.damperMotionRatio > 0.35f
        && bump.damperMotionRatio < 0.60f
        && std::abs(steer.localWheelForward.x) > 0.20f
        && std::abs(support.localTransverseOffset.x) > 0.001f;
    const bool mirrored = std::abs(
            mirroredBump.localWheelCenter.x + bump.localWheelCenter.x) <= 0.0002f
        && std::abs(mirroredBump.localWheelCenter.y - bump.localWheelCenter.y) <= 0.0002f
        && std::abs(mirroredBump.localWheelCenter.z - bump.localWheelCenter.z) <= 0.0002f
        && std::abs(mirroredBump.camberDegrees + bump.camberDegrees) <= 0.02f
        && std::abs(mirroredBump.kingpinInclinationDegrees
            + bump.kingpinInclinationDegrees) <= 0.02f;

    std::cout
        << "double_wishbone bump_camber_deg=" << bump.camberDegrees
        << " bump_steer_deg=" << bump.bumpSteerDegrees
        << " damper_ratio=" << bump.damperMotionRatio
        << " support_scrub_mm=" << support.localTransverseOffset.x * 1000.0f
        << " caster_deg=" << bump.casterDegrees
        << " kpi_deg=" << bump.kingpinInclinationDegrees
        << '\n';
    return solved && mirrored;
}

bool pushrodDoubleWishboneActuationIsNonlinearAndConservative()
{
    using namespace heritage::vehicles;
    const PushrodDoubleWishboneHardpoints rightHardpoints =
        syntheticPushrodDoubleWishboneCorner(false);
    const PushrodDoubleWishboneHardpoints leftHardpoints =
        syntheticPushrodDoubleWishboneCorner(true);

    const DoubleWishboneKinematicsInput restInput{
        0.0f, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f, 0.0f };
    const DoubleWishboneKinematicsInput bumpInput{
        0.08f, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f, 0.0f };
    const DoubleWishboneKinematicsInput droopInput{
        -0.08f, 0.0f, { 0.0f, -1.0f, 0.0f }, 0.0f, 0.0f };

    const auto rest = evaluatePushrodDoubleWishboneKinematics(
        rightHardpoints, restInput);
    const auto bump = evaluatePushrodDoubleWishboneKinematics(
        rightHardpoints, bumpInput);
    const auto droop = evaluatePushrodDoubleWishboneKinematics(
        rightHardpoints, droopInput);
    const auto leftBump = evaluatePushrodDoubleWishboneKinematics(
        leftHardpoints, bumpInput);
    const auto baseBump = evaluateDoubleWishboneLinkageKinematics(
        rightHardpoints.wishbone, bumpInput);

    SuspensionGeometryDescription geometry;
    geometry.provider = SuspensionProviderKind::PushrodDoubleWishboneV1;
    geometry.pushrodDoubleWishbone = rightHardpoints;
    const auto support = evaluateSuspensionSupportOffset(
        geometry, { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f } });

    SuspensionModelDescription model;
    model.provider = SuspensionProviderKind::PushrodDoubleWishboneV1;
    model.springPreloadN = 1000.0;
    model.springRateNPerM = 100000.0;
    model.springProgressionNPerM2 = 0.0;
    model.bumpDampingNsPerM = 1000.0;
    model.bumpHighSpeedDampingNsPerM = 1000.0;
    model.bumpDampingKneeVelocityMps = 2.0;
    model.reboundDampingNsPerM = 1000.0;
    model.reboundHighSpeedDampingNsPerM = 1000.0;
    model.reboundDampingKneeVelocityMps = 2.0;
    model.maximumForceN = 250000.0;
    const auto force = evaluateSuspensionModel(
        model,
        { 0.08,
          0.50,
          0.0,
          0.0,
          0.0,
          bump.springCompressionM,
          bump.springMotionRatio,
          bump.damperMotionRatio });
    const double expectedSpring = (1000.0
        + 100000.0 * static_cast<double>(bump.springCompressionM))
        * static_cast<double>(bump.springMotionRatio);
    const double damperShaftVelocity = 0.50
        * static_cast<double>(bump.damperMotionRatio);
    const double expectedDamper = 1000.0 * damperShaftVelocity
        * static_cast<double>(bump.damperMotionRatio);

    const bool geometryContract =
        validPushrodDoubleWishboneHardpoints(rightHardpoints)
        && rest.valid && bump.valid && droop.valid && leftBump.valid
        && baseBump.valid && support.valid
        && std::abs(rest.springCompressionM) <= 0.00001f
        && std::abs(rest.damperCompressionM) <= 0.00001f
        && bump.springCompressionM > 0.045f
        && bump.damperCompressionM > 0.030f
        && droop.springCompressionM < -0.045f
        && droop.damperCompressionM < -0.030f
        && std::abs(bump.springMotionRatio - droop.springMotionRatio) > 0.04f
        && std::abs(bump.damperMotionRatio - droop.damperMotionRatio) > 0.02f
        && std::abs(bump.pushrodLengthErrorM) <= 0.00001f
        && std::abs(bump.linkage.localWheelCenter.x
            - baseBump.localWheelCenter.x) <= 0.00001f
        && std::abs(bump.linkage.localWheelCenter.y
            - baseBump.localWheelCenter.y) <= 0.00001f
        && std::abs(bump.linkage.camberDegrees
            - baseBump.camberDegrees) <= 0.00001f
        && std::abs(leftBump.linkage.localWheelCenter.x
            + bump.linkage.localWheelCenter.x) <= 0.0002f
        && std::abs(leftBump.springMotionRatio
            - bump.springMotionRatio) <= 0.001f
        && std::abs(support.localTransverseOffset.x) > 0.001f;
    const bool forceContract =
        std::abs(force.springForceN - expectedSpring) <= 0.05
        && std::abs(force.dampingForceN - expectedDamper) <= 0.05
        && force.damperDissipationW > 0.0;

    std::cout
        << "pushrod_wishbone rocker_deg="
        << bump.rockerAngleRadians * 180.0f / 3.14159265358979323846f
        << " spring_compression_mm=" << bump.springCompressionM * 1000.0f
        << " spring_ratio=" << bump.springMotionRatio
        << " damper_compression_mm=" << bump.damperCompressionM * 1000.0f
        << " damper_ratio=" << bump.damperMotionRatio
        << " droop_spring_ratio=" << droop.springMotionRatio
        << " pushrod_error_mm=" << bump.pushrodLengthErrorM * 1000.0f
        << " spring_force_n=" << force.springForceN
        << " damper_force_n=" << force.dampingForceN
        << '\n';
    return geometryContract && forceContract;
}

bool liveAxleRigidPairKinematicsAreCoupledAndConservative()
{
    using namespace heritage::vehicles;
    const LiveAxleHardpoints h = syntheticLiveAxle();
    const auto restLeft = evaluateLiveAxleKinematics(
        h, { 0.0f, 0.0f, true, 0.0f, 0.0f, 0.0f });
    const auto bumpLeft = evaluateLiveAxleKinematics(
        h, { 0.08f, 0.08f, true, 0.0f, 0.0f, 0.0f });
    const auto articulatedLeft = evaluateLiveAxleKinematics(
        h, { 0.08f, -0.02f, true, 0.0f, 0.0f, 0.0f });
    const auto articulatedRight = evaluateLiveAxleKinematics(
        h, { 0.08f, -0.02f, false, 0.0f, 0.0f, 0.0f });

    const float restTrack = std::sqrt(
        std::pow(h.rightWheelCenter.x - h.leftWheelCenter.x, 2.0f)
        + std::pow(h.rightWheelCenter.y - h.leftWheelCenter.y, 2.0f)
        + std::pow(h.rightWheelCenter.z - h.leftWheelCenter.z, 2.0f));
    const float currentTrack = std::sqrt(
        std::pow(articulatedRight.localWheelCenter.x - articulatedLeft.localWheelCenter.x, 2.0f)
        + std::pow(articulatedRight.localWheelCenter.y - articulatedLeft.localWheelCenter.y, 2.0f)
        + std::pow(articulatedRight.localWheelCenter.z - articulatedLeft.localWheelCenter.z, 2.0f));

    SuspensionGeometryDescription leftGeometry;
    leftGeometry.provider = SuspensionProviderKind::LiveAxleV1;
    leftGeometry.localSteeringAxisPoint = h.leftWheelCenter;
    leftGeometry.liveAxle = h;
    SuspensionGeometryDescription rightGeometry = leftGeometry;
    rightGeometry.localSteeringAxisPoint = h.rightWheelCenter;
    const auto leftSupport = evaluateSuspensionSupportOffset(
        leftGeometry,
        { 0.08f, 0.0f, { 0.0f, -1.0f, 0.0f }, true, -0.02f });
    const auto rightSupport = evaluateSuspensionSupportOffset(
        rightGeometry,
        { -0.02f, 0.0f, { 0.0f, -1.0f, 0.0f }, true, 0.08f });

    SuspensionModelDescription model;
    model.provider = SuspensionProviderKind::LiveAxleV1;
    model.springPreloadN = 500.0;
    model.springRateNPerM = 40000.0;
    model.bumpDampingNsPerM = 2500.0;
    model.bumpHighSpeedDampingNsPerM = 2500.0;
    model.reboundDampingNsPerM = 3000.0;
    model.reboundHighSpeedDampingNsPerM = 3000.0;
    model.bumpDampingKneeVelocityMps = 1.0;
    model.reboundDampingKneeVelocityMps = 1.0;
    const auto force = evaluateSuspensionModel(
        model,
        { 0.08,
          0.30,
          0.0,
          0.0,
          0.0,
          articulatedLeft.springCompressionM,
          articulatedLeft.springMotionRatio,
          articulatedLeft.damperMotionRatio });

    const double expectedSpring = (500.0
        + 40000.0 * articulatedLeft.springCompressionM)
        * articulatedLeft.springMotionRatio;
    const double shaftVelocity = 0.30 * articulatedLeft.damperMotionRatio;
    const double expectedDamper = 2500.0 * shaftVelocity
        * articulatedLeft.damperMotionRatio;

    const bool geometryContract = validLiveAxleHardpoints(h)
        && restLeft.valid && bumpLeft.valid
        && articulatedLeft.valid && articulatedRight.valid
        && std::abs(currentTrack - restTrack) <= 0.0005f
        && articulatedLeft.axleRollRadians < -0.05f
        && std::abs(articulatedLeft.axleRollRadians
            - articulatedRight.axleRollRadians) <= 0.000001f
        && articulatedLeft.localWheelCenter.y
            > articulatedRight.localWheelCenter.y + 0.09f
        && std::abs(bumpLeft.lateralShiftM) > 0.005f
        && std::abs(bumpLeft.longitudinalShiftM) > 0.003f
        && articulatedLeft.springMotionRatio > 0.60f
        && articulatedLeft.springMotionRatio < 1.10f
        && articulatedLeft.damperMotionRatio > 0.50f
        && articulatedLeft.damperMotionRatio < 1.05f
        && leftSupport.valid && rightSupport.valid
        && std::abs(leftSupport.localTransverseOffset.x) > 0.0002f
        && std::abs(rightSupport.localTransverseOffset.x) > 0.0002f;
    const bool forceContract = std::abs(force.springForceN - expectedSpring) <= 0.05
        && std::abs(force.dampingForceN - expectedDamper) <= 0.05
        && force.damperDissipationW > 0.0;

    std::cout
        << "live_axle roll_deg=" << articulatedLeft.axleRollRadians * 180.0f / 3.14159265358979323846f
        << " bump_lateral_shift_mm=" << bumpLeft.lateralShiftM * 1000.0f
        << " bump_longitudinal_shift_mm=" << bumpLeft.longitudinalShiftM * 1000.0f
        << " track_error_mm=" << (currentTrack - restTrack) * 1000.0f
        << " left_spring_mr=" << articulatedLeft.springMotionRatio
        << " left_damper_mr=" << articulatedLeft.damperMotionRatio
        << " spring_force_n=" << force.springForceN
        << " damper_force_n=" << force.dampingForceN
        << '\n';
    return geometryContract && forceContract;
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


bool leafSpringLiveAxleShackleAndHysteresisArePhysical()
{
    using namespace heritage::vehicles;
    const auto h = syntheticLeafSpringLiveAxle();
    const auto rest = evaluateLeafSpringLiveAxle(h, { 0.0f, 0.0f, true, 0.0f, 0.0f, 0.0f });
    const auto bump = evaluateLeafSpringLiveAxle(h, { 0.08f, 0.08f, true, 0.0f, 0.0f, 0.0f });
    const auto droop = evaluateLeafSpringLiveAxle(h, { -0.05f, -0.05f, true, 0.0f, 0.0f, 0.0f });
    const auto articulated = evaluateLeafSpringLiveAxle(h, { 0.08f, -0.02f, true, 0.0f, 0.0f, 0.0f });

    SuspensionModelDescription model;
    model.provider = SuspensionProviderKind::LeafSpringLiveAxleV1;
    model.springPreloadN = 700.0;
    model.springRateNPerM = 52000.0;
    model.springProgressionNPerM2 = 90000.0;
    model.bumpDampingNsPerM = 1800.0;
    model.bumpHighSpeedDampingNsPerM = 1800.0;
    model.reboundDampingNsPerM = 2200.0;
    model.reboundHighSpeedDampingNsPerM = 2200.0;
    model.leafInterleafFrictionN = 600.0;
    model.leafInterleafVelocityScaleMps = 0.02;
    model.leafInterleafViscousNsPerM = 220.0;
    model.leafAxleWrapJackingNPerRad = 1800.0;

    const auto bumpForce = evaluateSuspensionModel(model,
        { 0.08, 0.25, 0.0, 0.0, 0.0,
          bump.leafCompressionM, std::abs(bump.leafMotionRatio),
          bump.axle.damperMotionRatio, 0.06, 0.0 });
    const auto reboundForce = evaluateSuspensionModel(model,
        { 0.08, -0.25, 0.0, 0.0, 0.0,
          bump.leafCompressionM, std::abs(bump.leafMotionRatio),
          bump.axle.damperMotionRatio, 0.06, 0.0 });

    const LeafSpringAxleWrapDescription wrapDescription{
        16000.0, 1200.0, 5.0, 0.22 };
    const auto runWrap = [&](double dt) {
        LeafSpringAxleWrapState state{};
        const int drivenSteps = static_cast<int>(0.30 / dt);
        const int releaseSteps = static_cast<int>(0.70 / dt);
        double peak = 0.0;
        for (int i = 0; i < drivenSteps; ++i)
        {
            state = advanceLeafSpringAxleWrap(
                wrapDescription, state, { 2200.0, dt });
            peak = std::max(peak, std::abs(state.angleRadians));
        }
        for (int i = 0; i < releaseSteps; ++i)
        {
            state = advanceLeafSpringAxleWrap(
                wrapDescription, state, { 0.0, dt });
            peak = std::max(peak, std::abs(state.angleRadians));
        }
        return std::pair<LeafSpringAxleWrapState,double>{state,peak};
    };
    const auto wrap1000 = runWrap(0.001);
    const auto wrap500 = runWrap(0.002);

    const bool contract = validLeafSpringLiveAxleHardpoints(h)
        && rest.valid && bump.valid && droop.valid && articulated.valid
        && std::abs(rest.leafCompressionM) < 0.00001f
        && bump.leafCompressionM > 0.070f
        && droop.leafCompressionM < -0.045f
        && bump.leafMotionRatio > 0.70f && bump.leafMotionRatio < 1.10f
        && std::abs(bump.shackleTravelRadians) > 0.10f
        && articulated.axle.axleRollRadians < -0.05f
        && bumpForce.dampingForceN > reboundForce.dampingForceN
        && bumpForce.normalForceN > 0.0
        && reboundForce.normalForceN > 0.0
        && wrap1000.second > 0.025 && wrap1000.second < 0.22
        && std::abs(wrap1000.first.angleRadians) < 0.01
        && std::abs(wrap1000.first.rateRadiansPerSecond) < 0.25
        && std::abs(wrap1000.first.angleRadians - wrap500.first.angleRadians) < 0.0015
        && std::abs(wrap1000.first.rateRadiansPerSecond - wrap500.first.rateRadiansPerSecond) < 0.08;

    std::cout << "leaf_live_axle leaf_mm=" << bump.leafCompressionM * 1000.0f
        << " leaf_ratio=" << bump.leafMotionRatio
        << " shackle_deg=" << bump.shackleTravelRadians * 180.0f / 3.14159265358979323846f
        << " articulation_roll_deg=" << articulated.axle.axleRollRadians * 180.0f / 3.14159265358979323846f
        << " bump_force_n=" << bumpForce.normalForceN
        << " rebound_force_n=" << reboundForce.normalForceN
        << " wrap_peak_deg=" << wrap1000.second * 180.0 / 3.14159265358979323846
        << " wrap_settled_deg=" << wrap1000.first.angleRadians * 180.0 / 3.14159265358979323846
        << " wrap_500_1000_delta_deg="
        << std::abs(wrap1000.first.angleRadians - wrap500.first.angleRadians)
            * 180.0 / 3.14159265358979323846 << '\n';
    return contract;
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



bool motorcycleForkAndSwingarmKinematicsArePhysical()
{
    heritage::vehicles::MotorcycleForkHardpoints fork;
    fork.authored = true;
    fork.steeringStemUpper = { 0.0f, 0.95f, 0.75f };
    fork.steeringStemLower = { 0.0f, 0.60f, 0.58f };
    fork.wheelCenter = { 0.0f, 0.34f, 1.28f };

    const heritage::math::Vec3 forkDirection{ 0.0f, -0.90f, -0.435f };
    const auto forkRest = heritage::vehicles::evaluateMotorcycleForkKinematics(
        fork, { 0.0f, 0.0f, forkDirection, 0.0f, 0.0f });
    const auto forkBump = heritage::vehicles::evaluateMotorcycleForkKinematics(
        fork, { 0.10f, 0.0f, forkDirection, 0.0f, 0.0f });
    const auto forkSteer = heritage::vehicles::evaluateMotorcycleForkKinematics(
        fork, { 0.0f, 25.0f, forkDirection, 0.0f, 0.0f });

    heritage::vehicles::SuspensionGeometryDescription forkGeometry;
    forkGeometry.provider = heritage::vehicles::SuspensionProviderKind::MotorcycleTelescopicForkV1;
    forkGeometry.motorcycleFork = fork;
    forkGeometry.localSteeringAxisPoint = fork.wheelCenter;
    const auto forkSupport = heritage::vehicles::evaluateSuspensionSupportOffset(
        forkGeometry, { 0.0f, 25.0f, forkDirection });

    heritage::vehicles::MotorcycleSwingarmHardpoints rear;
    rear.authored = true;
    rear.swingarmPivotLeft = { -0.18f, 0.50f, -0.25f };
    rear.swingarmPivotRight = { 0.18f, 0.50f, -0.25f };
    rear.wheelCenter = { 0.0f, 0.34f, -1.48f };
    rear.linkageSwingarmMount = { 0.0f, 0.30f, -1.05f };
    rear.rockerPivotLeft = { -0.06f, 0.62f, -0.48f };
    rear.rockerPivotRight = { 0.06f, 0.62f, -0.48f };
    rear.rockerLinkMount = { 0.0f, 0.52f, -0.75f };
    rear.shockChassisMount = { 0.0f, 0.75f, -0.40f };
    rear.shockRockerMount = { 0.0f, 0.76f, -0.72f };
    rear.countershaftCenter = { 0.0f, 0.54f, -0.18f };

    const auto rearDroop = heritage::vehicles::evaluateMotorcycleSwingarmKinematics(
        rear, { -0.03f, { 0.0f, -1.0f, 0.0f } });
    const auto rearRest = heritage::vehicles::evaluateMotorcycleSwingarmKinematics(
        rear, { 0.0f, { 0.0f, -1.0f, 0.0f } });
    const auto rearBump = heritage::vehicles::evaluateMotorcycleSwingarmKinematics(
        rear, { 0.08f, { 0.0f, -1.0f, 0.0f } });

    heritage::vehicles::SuspensionModelDescription rearModel;
    rearModel.provider = heritage::vehicles::SuspensionProviderKind::MotorcycleSwingarmLinkageV1;
    rearModel.springRateNPerM = 90000.0;
    rearModel.bumpDampingNsPerM = 3500.0;
    rearModel.bumpHighSpeedDampingNsPerM = 2500.0;
    rearModel.reboundDampingNsPerM = 4200.0;
    rearModel.reboundHighSpeedDampingNsPerM = 3000.0;
    rearModel.motorcycleRearSprocketPitchRadiusM = 0.105;
    heritage::vehicles::SuspensionModelInput rearInput;
    rearInput.compressionM = 0.08;
    rearInput.compressionVelocityMps = 0.20;
    rearInput.springCompressionM = rearBump.shockCompressionM;
    rearInput.springMotionRatio = rearBump.shockMotionRatio;
    rearInput.damperMotionRatio = rearBump.shockMotionRatio;
    rearInput.previousLongitudinalTireForceN = 2200.0;
    rearInput.wheelEffectiveRadiusM = 0.30;
    rearInput.motorcycleChainDistanceMotionRatio =
        rearBump.chainCenterDistanceMotionRatio;
    const auto rearForce = heritage::vehicles::evaluateSuspensionModel(
        rearModel, rearInput);

    const bool forkContract = forkRest.valid && forkBump.valid && forkSteer.valid
        && forkSupport.valid
        && forkBump.localWheelCenter.y > forkRest.localWheelCenter.y + 0.07f
        && forkBump.localWheelCenter.z > forkRest.localWheelCenter.z + 0.02f
        && forkBump.rakeDegreesFromVertical > 20.0f
        && forkBump.rakeDegreesFromVertical < 35.0f
        && std::abs(forkBump.springMotionRatio - 1.0f) < 0.0001f
        && std::abs(forkSteer.localWheelCenter.x) > 0.20f
        && std::abs(forkSupport.localTransverseOffset.x) > 0.20f;
    const bool rearContract = rearDroop.valid && rearRest.valid && rearBump.valid
        && rearBump.swingarmAngleRadians > 0.04f
        && rearBump.shockCompressionM > 0.02f
        && rearBump.shockMotionRatio > 0.20f
        && rearBump.shockMotionRatio < 0.70f
        && rearDroop.shockMotionRatio > rearBump.shockMotionRatio
        && std::abs(rearBump.dogboneLengthErrorM) < 0.0001f
        && rearBump.chainCenterDistanceMotionRatio < -0.01f
        && rearForce.motorcycleChainJackingForceN > 0.0
        && rearForce.motorcycleChainJackingForceN < 5000.0;

    std::cout
        << "motorcycle_suspension fork_rake_deg=" << forkBump.rakeDegreesFromVertical
        << " fork_bump_wheelbase_mm=" << forkBump.wheelbaseDeltaM * 1000.0f
        << " fork_25deg_lateral_mm=" << forkSupport.localTransverseOffset.x * 1000.0f
        << " rear_swingarm_deg=" << rearBump.swingarmAngleRadians * 57.2957795131f
        << " rear_shock_mm=" << rearBump.shockCompressionM * 1000.0f
        << " rear_mr=" << rearBump.shockMotionRatio
        << " rear_droop_mr=" << rearDroop.shockMotionRatio
        << " chain_dLdx=" << rearBump.chainCenterDistanceMotionRatio
        << " chain_jacking_n=" << rearForce.motorcycleChainJackingForceN
        << '\n';
    return forkContract && rearContract;
}

bool multiLinkRigidUprightConstraintsAndRackSteeringArePhysical()
{
    using namespace heritage::vehicles;
    const MultiLinkHardpoints h = syntheticMultiLinkCorner();
    if (!validMultiLinkHardpoints(h)) return false;
    const auto rest = evaluateMultiLinkKinematics(h,{0.0f,0.0f,{0,-1,0},-1.0f,0.1f});
    const auto bump = evaluateMultiLinkKinematics(h,{0.08f,0.0f,{0,-1,0},-1.0f,0.1f});
    const auto steer = evaluateMultiLinkKinematics(h,{0.04f,10.0f,{0,-1,0},-1.0f,0.1f});
    SuspensionGeometryDescription geometry; geometry.provider=SuspensionProviderKind::MultiLinkV1; geometry.localSteeringAxisPoint=h.wheelCenter; geometry.multiLink=h; geometry.staticCamberDegrees=-1.0f; geometry.staticToeDegrees=0.1f;
    const auto support=evaluateSuspensionSupportOffset(geometry,{0.08f,0.0f,{0,-1,0},false,0.0f});
    SuspensionModelDescription model; model.provider=SuspensionProviderKind::MultiLinkV1; model.springPreloadN=1200.0; model.springRateNPerM=42000.0; model.bumpDampingNsPerM=3000.0; model.reboundDampingNsPerM=4000.0;
    SuspensionModelInput forceInput; forceInput.compressionM=0.08; forceInput.compressionVelocityMps=0.2; forceInput.springCompressionM=bump.springCompressionM; forceInput.springMotionRatio=bump.springMotionRatio; forceInput.damperMotionRatio=bump.damperMotionRatio;
    const auto force=evaluateSuspensionModel(model,forceInput);
    const bool ok=rest.valid&&bump.valid&&steer.valid&&support.valid
        && std::abs(rest.toeDegrees-0.1f)<0.02f
        && bump.localWheelCenter.y>rest.localWheelCenter.y+0.075f
        && bump.camberDegrees<rest.camberDegrees-2.0f
        && std::abs(bump.bumpSteerDegrees)>0.5f
        && std::abs(steer.toeDegrees-10.0f)<1.0f
        && std::abs(steer.steeringRackDisplacementM)>0.005f
        && bump.springMotionRatio>0.5f && bump.springMotionRatio<2.0f
        && bump.damperMotionRatio>0.4f && bump.damperMotionRatio<1.5f
        && std::abs(support.localTransverseOffset.x)>0.005f
        && force.springForceN>0.0 && force.dampingForceN>0.0;
    std::cout << "multilink bump_camber_deg=" << bump.camberDegrees
        << " bump_steer_deg=" << bump.bumpSteerDegrees
        << " rack_10deg_mm=" << steer.steeringRackDisplacementM*1000.0f
        << " spring_mr=" << bump.springMotionRatio
        << " damper_mr=" << bump.damperMotionRatio
        << " scrub_x_mm=" << support.localTransverseOffset.x*1000.0f << "\n";
    return ok;
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




bool semiTrailingArmAndTwistBeamArePhysical()
{
    using namespace heritage::vehicles;
    const auto arm = syntheticSemiTrailingArm(true);
    const auto rest = evaluateSemiTrailingArmKinematics(arm,{0.0f,{0,-1,0},-1.0f,0.1f});
    const auto bump = evaluateSemiTrailingArmKinematics(arm,{0.06f,{0,-1,0},-1.0f,0.1f});
    if(!rest.valid||!bump.valid) return false;

    SuspensionGeometryDescription independent;
    independent.provider=SuspensionProviderKind::SemiTrailingArmV1;
    independent.localSteeringAxisPoint=arm.wheelCenter;
    independent.semiTrailingArm=arm;
    independent.staticCamberDegrees=-1.0f;
    independent.staticToeDegrees=0.1f;
    const auto support=evaluateSuspensionSupportOffset(independent,{0.06f,0.0f,{0,-1,0},false,0.0f});

    const auto beam=syntheticTwistBeam();
    const auto symmetric=evaluateTwistBeamKinematics(beam,{0.05f,0.05f,0.20f,0.20f,true,{0,-1,0},-1.0f,0.1f});
    const auto left=evaluateTwistBeamKinematics(beam,{0.06f,-0.01f,0.18f,-0.06f,true,{0,-1,0},-1.0f,0.1f});
    const auto right=evaluateTwistBeamKinematics(beam,{0.06f,-0.01f,0.18f,-0.06f,false,{0,-1,0},-1.0f,0.1f});
    if(!symmetric.valid||!left.valid||!right.valid) return false;

    SuspensionModelDescription model; model.provider=SuspensionProviderKind::TwistBeamV1;
    model.springRateNPerM=30000.0; model.bumpDampingNsPerM=2500.0; model.reboundDampingNsPerM=3000.0;
    model.twistBeamTorsionalStiffnessNmPerRad=2500.0; model.twistBeamTorsionalDampingNmsPerRad=120.0;
    SuspensionModelInput li; li.compressionM=0.06;li.compressionVelocityMps=0.18;li.springCompressionM=left.arm.springCompressionM;li.springMotionRatio=left.arm.springMotionRatio;li.damperMotionRatio=left.arm.damperMotionRatio;li.twistBeamTwistRadians=left.beamTwistRadians;li.twistBeamTwistRateRadiansPerSecond=left.beamTwistRateRadiansPerSecond;li.twistBeamAngularMotionRatioRadPerM=left.beamAngularMotionRatioRadPerM;
    auto ri=li;ri.compressionM=-0.01;ri.compressionVelocityMps=-0.06;ri.springCompressionM=right.arm.springCompressionM;ri.springMotionRatio=right.arm.springMotionRatio;ri.damperMotionRatio=right.arm.damperMotionRatio;ri.twistBeamAngularMotionRatioRadPerM=right.beamAngularMotionRatioRadPerM;
    const auto lf=evaluateSuspensionModel(model,li),rf=evaluateSuspensionModel(model,ri);

    std::cout << "semi_trailing_twist_beam camber_gain_deg=" << (bump.camberDegrees-rest.camberDegrees)
        << " bump_steer_deg=" << bump.bumpSteerDegrees
        << " scrub_x_mm=" << (bump.localWheelCenter.x-arm.wheelCenter.x)*1000.0f
        << " spring_mr=" << bump.springMotionRatio
        << " symmetric_twist_deg=" << symmetric.beamTwistRadians*57.2957795f
        << " split_twist_deg=" << left.beamTwistRadians*57.2957795f
        << " left_coupling_n=" << lf.twistBeamCouplingForceN
        << " right_coupling_n=" << rf.twistBeamCouplingForceN << '\n';

    return support.valid
        && std::abs(bump.camberDegrees-rest.camberDegrees)>0.20f
        && std::abs(bump.bumpSteerDegrees)>0.05f
        && std::abs(bump.localWheelCenter.x-arm.wheelCenter.x)>0.001f
        && bump.springMotionRatio>0.05f && bump.damperMotionRatio>0.05f
        && std::abs(symmetric.beamTwistRadians)<1.0e-5f
        && std::abs(left.beamTwistRadians)>0.02f
        && left.beamTwistRateRadiansPerSecond*left.beamTwistRateRadiansPerSecond>1.0e-5f
        && lf.twistBeamCouplingForceN*rf.twistBeamCouplingForceN<0.0
        && lf.twistBeamDissipationW>0.0;
}

bool kartChassisKingpinJackingAndRigidRearArePhysical()
{
    using namespace heritage::vehicles;
    const KartChassisHardpoints h = syntheticKartChassis();
    if (!validKartChassisHardpoints(h))
        return false;

    SuspensionGeometryDescription left;
    left.provider = SuspensionProviderKind::KartChassisFlexV1;
    left.localSteeringAxisPoint = h.frontLeftWheelCenter;
    left.kartChassis = h;
    SuspensionGeometryDescription right = left;
    right.localSteeringAxisPoint = h.frontRightWheelCenter;
    SuspensionGeometryDescription rear = left;
    rear.localSteeringAxisPoint = h.rearLeftWheelCenter;

    const auto leftTurnInside = evaluateSuspensionGeometry(
        left, { 0.0f, -20.0f, {0.0f,-1.0f,0.0f}, false, 0.0f });
    const auto leftTurnOutside = evaluateSuspensionGeometry(
        right, { 0.0f, -20.0f, {0.0f,-1.0f,0.0f}, false, 0.0f });
    const auto rearLoaded = evaluateSuspensionGeometry(
        rear, { 0.08f, -20.0f, {0.0f,-1.0f,0.0f}, false, 0.0f });
    const auto leftSupport = evaluateSuspensionSupportOffset(
        left, { 0.0f, -20.0f, {0.0f,-1.0f,0.0f}, false, 0.0f });

    SuspensionModelDescription model;
    model.provider = SuspensionProviderKind::KartChassisFlexV1;
    model.springPreloadN = 5000.0;
    model.springRateNPerM = 1000000.0;
    model.bumpDampingNsPerM = 100000.0;
    model.reboundDampingNsPerM = 100000.0;
    const auto force = evaluateSuspensionModel(model, { 0.01, 1.0 });

    const float rearDelta = std::sqrt(
        std::pow(rearLoaded.localWheelCenter.x - h.rearLeftWheelCenter.x, 2.0f)
        + std::pow(rearLoaded.localWheelCenter.y - h.rearLeftWheelCenter.y, 2.0f)
        + std::pow(rearLoaded.localWheelCenter.z - h.rearLeftWheelCenter.z, 2.0f));

    std::cout
        << "kart_chassis inside_jack_mm=" << leftTurnInside.kartSteeringJackingM * 1000.0f
        << " outside_jack_mm=" << leftTurnOutside.kartSteeringJackingM * 1000.0f
        << " caster_deg=" << leftTurnInside.casterDegrees
        << " kpi_deg=" << leftTurnInside.kingpinInclinationDegrees
        << " spindle_offset_mm=" << leftTurnInside.kartKingpinRadialOffsetM * 1000.0f
        << " support_vertical_mm=" << leftSupport.localTransverseOffset.y * 1000.0f
        << " rear_independent_travel_mm=" << rearDelta * 1000.0f
        << " hidden_spring_force_n=" << force.normalForceN
        << "\n";

    return leftTurnInside.kinematicsValid
        && leftTurnOutside.kinematicsValid
        && rearLoaded.kinematicsValid
        && leftSupport.valid
        && leftTurnInside.kartSteeringJackingM
            * leftTurnOutside.kartSteeringJackingM < 0.0f
        && std::abs(leftTurnInside.kartSteeringJackingM) > 0.001f
        && std::abs(leftTurnOutside.kartSteeringJackingM) > 0.001f
        && std::abs(leftSupport.localTransverseOffset.y
            - leftTurnInside.kartSteeringJackingM) < 1.0e-5f
        && rearDelta < 1.0e-7f
        && rearLoaded.travelClamped
        && std::abs(force.springForceN) < 1.0e-9
        && std::abs(force.dampingForceN) < 1.0e-9
        && std::abs(force.normalForceN) < 1.0e-9;
}

bool staticRideHeightCalibrationReconstructsPeugeotStance()
{
    using namespace heritage::vehicles;
    constexpr double peugeotKerbMassKg = 1125.0;
    StaticRideHeightInput front;
    front.provider = SuspensionProviderKind::MacPhersonStrutV1;
    front.supportedLoadN = peugeotKerbMassKg
        * 9.80665 * 0.5819001 * 0.5;
    front.targetBodyOffsetM = 0.0;
    front.mountHeightFromAuthoredGroundM = 0.85;
    front.unloadedTireRadiusM = 0.2979;
    front.suspensionRestLengthM = 0.55;
    front.maximumCompressionM = 0.20;
    front.maximumDroopM = 0.15;
    front.springRateNPerM = 35000.0;
    front.springProgressionNPerM2 = 15000.0;
    front.motionRatio = 1.0;
    front.tireVerticalStiffnessNPerM = 220000.0;

    StaticRideHeightInput rear = front;
    rear.provider = SuspensionProviderKind::TrailingArmTorsionBarV1;
    rear.supportedLoadN = peugeotKerbMassKg
        * 9.80665 * (1.0 - 0.5819001) * 0.5;
    const StaticRideHeightOutput frontResult = solveStaticRideHeight(front);
    const StaticRideHeightOutput rearResult = solveStaticRideHeight(rear);
    if (!frontResult.valid || !rearResult.valid)
        return false;

    const auto reconstructedBodyOffset = [](const StaticRideHeightInput& input,
        const StaticRideHeightOutput& output) {
        return output.targetSuspensionLengthM
            - output.staticTireDeflectionM
            - (input.mountHeightFromAuthoredGroundM
                - input.unloadedTireRadiusM);
    };
    std::cout
        << "ride_height front_preload_n="
        << frontResult.requiredSpringPreloadN
        << " rear_preload_n=" << rearResult.requiredSpringPreloadN
        << " front_tire_deflection_mm="
        << frontResult.staticTireDeflectionM * 1000.0
        << " rear_tire_deflection_mm="
        << rearResult.staticTireDeflectionM * 1000.0
        << " front_compression_mm="
        << frontResult.targetCompressionM * 1000.0
        << " rear_compression_mm="
        << rearResult.targetCompressionM * 1000.0 << '\n';

    return std::abs(frontResult.reconstructedSupportForceN
            - front.supportedLoadN) < 1.0e-6
        && std::abs(rearResult.reconstructedSupportForceN
            - rear.supportedLoadN) < 1.0e-6
        && std::abs(reconstructedBodyOffset(front, frontResult)) < 1.0e-9
        && std::abs(reconstructedBodyOffset(rear, rearResult)) < 1.0e-9
        && frontResult.requiredSpringPreloadN > 3500.0
        && frontResult.requiredSpringPreloadN < 3900.0
        && rearResult.requiredSpringPreloadN > 2500.0
        && rearResult.requiredSpringPreloadN < 2900.0
        && frontResult.targetCompressionM < 0.0
        && rearResult.targetCompressionM < 0.0;
}

} // namespace heritage::tests
