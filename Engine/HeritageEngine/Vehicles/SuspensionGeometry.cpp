#include "SuspensionGeometry.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kVectorEpsilon = 0.000001f;

float radians(float degreesValue)
{
    return degreesValue * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    float factor)
{
    return { value.x * factor, value.y * factor, value.z * factor };
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitudeSquared = dot(value, value);
    if (magnitudeSquared <= kVectorEpsilon * kVectorEpsilon)
        return fallback;
    return scale(value, 1.0f / std::sqrt(magnitudeSquared));
}

heritage::math::Vec3 steeringAxisWithCaster(
    const heritage::math::Vec3& value,
    float casterDegrees)
{
    const heritage::math::Vec3 axis = normalized(
        value, { 0.0f, 1.0f, 0.0f });
    const float yzMagnitude = std::sqrt(
        axis.y * axis.y + axis.z * axis.z);
    if (yzMagnitude <= kVectorEpsilon)
        return axis;
    const float caster = radians(casterDegrees);
    return normalized(
        { axis.x,
          yzMagnitude * std::cos(caster),
          -yzMagnitude * std::sin(caster) },
        axis);
}

heritage::math::Vec3 rotateAroundAxis(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& unitAxis,
    float angleDegrees)
{
    const float angle = radians(angleDegrees);
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return add(
        add(
            scale(value, cosine),
            scale(cross(unitAxis, value), sine)),
        scale(unitAxis, dot(unitAxis, value) * (1.0f - cosine)));
}

float travelCurve(
    float staticDegrees,
    float gainDegreesPerM,
    float progressionDegreesPerM2,
    float compressionM)
{
    return staticDegrees + gainDegreesPerM * compressionM
        + 0.5f * progressionDegreesPerM2
            * compressionM * std::abs(compressionM);
}

heritage::math::Vec3 eulerDegreesFromBasis(
    const heritage::math::Vec3& right,
    const heritage::math::Vec3& up,
    const heritage::math::Vec3& forward)
{
    const float y = std::asin(std::clamp(-right.z, -1.0f, 1.0f));
    const float cosineY = std::cos(y);
    float x = 0.0f;
    float z = 0.0f;
    if (std::abs(cosineY) > kVectorEpsilon)
    {
        x = std::atan2(up.z, forward.z);
        z = std::atan2(right.y, right.x);
    }
    else
    {
        x = std::atan2(-forward.y, up.y);
    }
    return { degrees(x), degrees(y), degrees(z) };
}

} // namespace

SuspensionGeometryOutput evaluateSuspensionGeometry(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input)
{
    SuspensionGeometryOutput output;
    if (description.provider == SuspensionProviderKind::MacPhersonStrutV1)
    {
        const MacPhersonKinematicsOutput macPherson =
            evaluateMacPhersonKinematics(
                description.macPherson,
                { input.compressionM,
                  input.steeringDegrees,
                  input.localSuspensionDirection,
                  description.staticCamberDegrees,
                  description.staticToeDegrees,
                  description.casterOverrideEnabled,
                  description.staticCasterDegrees });
        if (!macPherson.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = macPherson.camberDegrees;
        output.toeDegrees = macPherson.toeDegrees;
        output.localSteeringAxis = macPherson.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = macPherson.localSteeringAxisPoint;
        output.localWheelForward = macPherson.localWheelForward;
        output.localWheelRight = macPherson.localWheelRight;
        output.localWheelUp = macPherson.localWheelUp;
        output.localUprightRotationDegrees =
            macPherson.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = macPherson.travelClamped;
        output.bumpSteerDegrees = macPherson.bumpSteerDegrees;
        output.strutCompressionM = macPherson.strutCompressionM;
        output.springCompressionM = macPherson.strutCompressionM;
        output.springMotionRatio = macPherson.springMotionRatio;
        output.damperCompressionM = macPherson.strutCompressionM;
        output.damperMotionRatio = macPherson.springMotionRatio;
        output.localWheelCenter = macPherson.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::DoubleWishboneV1)
    {
        const DoubleWishboneKinematicsOutput wishbone =
            evaluateDoubleWishboneKinematics(
                description.doubleWishbone,
                { input.compressionM,
                  input.steeringDegrees,
                  input.localSuspensionDirection,
                  description.staticCamberDegrees,
                  description.staticToeDegrees });
        if (!wishbone.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = wishbone.camberDegrees;
        output.toeDegrees = wishbone.toeDegrees;
        output.localSteeringAxis = wishbone.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = wishbone.localSteeringAxisPoint;
        output.localWheelForward = wishbone.localWheelForward;
        output.localWheelRight = wishbone.localWheelRight;
        output.localWheelUp = wishbone.localWheelUp;
        output.localUprightRotationDegrees =
            wishbone.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = wishbone.travelClamped;
        output.bumpSteerDegrees = wishbone.bumpSteerDegrees;
        output.casterDegrees = wishbone.casterDegrees;
        output.kingpinInclinationDegrees =
            wishbone.kingpinInclinationDegrees;
        output.springCompressionM = wishbone.damperCompressionM;
        output.springMotionRatio = wishbone.springMotionRatio;
        output.damperCompressionM = wishbone.damperCompressionM;
        output.damperMotionRatio = wishbone.damperMotionRatio;
        output.localWheelCenter = wishbone.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::PushrodDoubleWishboneV1)
    {
        const PushrodDoubleWishboneKinematicsOutput pushrod =
            evaluatePushrodDoubleWishboneKinematics(
                description.pushrodDoubleWishbone,
                { input.compressionM,
                  input.steeringDegrees,
                  input.localSuspensionDirection,
                  description.staticCamberDegrees,
                  description.staticToeDegrees });
        if (!pushrod.valid || !pushrod.linkage.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        const DoubleWishboneKinematicsOutput& wishbone = pushrod.linkage;
        output.camberDegrees = wishbone.camberDegrees;
        output.toeDegrees = wishbone.toeDegrees;
        output.localSteeringAxis = wishbone.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = wishbone.localSteeringAxisPoint;
        output.localWheelForward = wishbone.localWheelForward;
        output.localWheelRight = wishbone.localWheelRight;
        output.localWheelUp = wishbone.localWheelUp;
        output.localUprightRotationDegrees = wishbone.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = pushrod.travelClamped;
        output.bumpSteerDegrees = wishbone.bumpSteerDegrees;
        output.casterDegrees = wishbone.casterDegrees;
        output.kingpinInclinationDegrees = wishbone.kingpinInclinationDegrees;
        output.springCompressionM = pushrod.springCompressionM;
        output.springMotionRatio = pushrod.springMotionRatio;
        output.damperCompressionM = pushrod.damperCompressionM;
        output.damperMotionRatio = pushrod.damperMotionRatio;
        output.localWheelCenter = wishbone.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::MultiLinkV1)
    {
        const MultiLinkKinematicsOutput multi = evaluateMultiLinkKinematics(
            description.multiLink,
            { input.compressionM,
              input.steeringDegrees,
              input.localSuspensionDirection,
              description.staticCamberDegrees,
              description.staticToeDegrees });
        if (!multi.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = multi.camberDegrees;
        output.toeDegrees = multi.toeDegrees;
        output.localSteeringAxis = multi.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = multi.localSteeringAxisPoint;
        output.localWheelForward = multi.localWheelForward;
        output.localWheelRight = multi.localWheelRight;
        output.localWheelUp = multi.localWheelUp;
        output.localUprightRotationDegrees = multi.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = multi.travelClamped;
        output.bumpSteerDegrees = multi.bumpSteerDegrees;
        output.casterDegrees = multi.casterDegrees;
        output.kingpinInclinationDegrees = multi.kingpinInclinationDegrees;
        output.springCompressionM = multi.springCompressionM;
        output.springMotionRatio = multi.springMotionRatio;
        output.damperCompressionM = multi.damperCompressionM;
        output.damperMotionRatio = multi.damperMotionRatio;
        output.multiLinkSteeringRackDisplacementM = multi.steeringRackDisplacementM;
        output.localWheelCenter = multi.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::MotorcycleTelescopicForkV1)
    {
        const MotorcycleForkKinematicsOutput fork =
            evaluateMotorcycleForkKinematics(
                description.motorcycleFork,
                { input.compressionM,
                  input.steeringDegrees,
                  input.localSuspensionDirection,
                  description.staticCamberDegrees,
                  description.staticToeDegrees });
        if (!fork.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = fork.camberDegrees;
        output.toeDegrees = fork.toeDegrees;
        output.localSteeringAxis = fork.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = fork.localSteeringAxisPoint;
        output.localWheelForward = fork.localWheelForward;
        output.localWheelRight = fork.localWheelRight;
        output.localWheelUp = fork.localWheelUp;
        output.localUprightRotationDegrees = fork.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = fork.travelClamped;
        output.strutCompressionM = fork.forkCompressionM;
        output.springCompressionM = fork.springCompressionM;
        output.springMotionRatio = fork.springMotionRatio;
        output.damperCompressionM = fork.damperCompressionM;
        output.damperMotionRatio = fork.damperMotionRatio;
        output.motorcycleRakeDegreesFromVertical = fork.rakeDegreesFromVertical;
        output.wheelbaseDeltaM = fork.wheelbaseDeltaM;
        output.localWheelCenter = fork.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1)
    {
        const MotorcycleSwingarmKinematicsOutput swingarm =
            evaluateMotorcycleSwingarmKinematics(
                description.motorcycleSwingarm,
                { input.compressionM, input.localSuspensionDirection });
        if (!swingarm.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = description.staticCamberDegrees;
        output.toeDegrees = description.staticToeDegrees;
        output.localSteeringAxis = normalized(
            description.localSteeringAxis, { 0.0f, 1.0f, 0.0f });
        output.steeringAxisPointValid = false;
        output.localWheelForward = swingarm.localWheelForward;
        output.localWheelRight = swingarm.localWheelRight;
        output.localWheelUp = swingarm.localWheelUp;
        output.localUprightRotationDegrees = swingarm.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = swingarm.travelClamped;
        output.springCompressionM = swingarm.shockCompressionM;
        output.springMotionRatio = swingarm.shockMotionRatio;
        output.damperCompressionM = swingarm.shockCompressionM;
        output.damperMotionRatio = swingarm.shockMotionRatio;
        output.motorcycleSwingarmAngleRadians = swingarm.swingarmAngleRadians;
        output.motorcycleRockerAngleRadians = swingarm.rockerAngleRadians;
        output.motorcycleChainDistanceMotionRatio =
            swingarm.chainCenterDistanceMotionRatio;
        output.wheelbaseDeltaM = swingarm.wheelbaseDeltaM;
        output.localWheelCenter = swingarm.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::KartChassisFlexV1)
    {
        const KartChassisKinematicsOutput kart = evaluateKartChassisKinematics(
            description.kartChassis,
            { input.compressionM,
              input.steeringDegrees,
              description.localSteeringAxisPoint,
              description.staticCamberDegrees,
              description.staticToeDegrees });
        if (!kart.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = kart.camberDegrees;
        output.toeDegrees = kart.toeDegrees;
        output.localSteeringAxis = kart.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = kart.localSteeringAxisPoint;
        output.localWheelForward = kart.localWheelForward;
        output.localWheelRight = kart.localWheelRight;
        output.localWheelUp = kart.localWheelUp;
        output.localUprightRotationDegrees = kart.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = kart.travelClamped;
        output.casterDegrees = kart.casterDegrees;
        output.kingpinInclinationDegrees = kart.kingpinInclinationDegrees;
        output.springCompressionM = 0.0f;
        output.springMotionRatio = 0.0f;
        output.damperCompressionM = 0.0f;
        output.damperMotionRatio = 0.0f;
        output.kartSteeringJackingM = kart.steeringJackingM;
        output.kartKingpinRadialOffsetM = kart.kingpinRadialOffsetM;
        output.localWheelCenter = kart.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::LiveAxleV1)
    {
        if (!description.liveAxle.authored || !input.pairedCompressionValid)
        {
            output.kinematicsValid = false;
            return output;
        }
        const heritage::math::Vec3 toLeft = {
            description.localSteeringAxisPoint.x - description.liveAxle.leftWheelCenter.x,
            description.localSteeringAxisPoint.y - description.liveAxle.leftWheelCenter.y,
            description.localSteeringAxisPoint.z - description.liveAxle.leftWheelCenter.z };
        const heritage::math::Vec3 toRight = {
            description.localSteeringAxisPoint.x - description.liveAxle.rightWheelCenter.x,
            description.localSteeringAxisPoint.y - description.liveAxle.rightWheelCenter.y,
            description.localSteeringAxisPoint.z - description.liveAxle.rightWheelCenter.z };
        const bool leftSide = dot(toLeft, toLeft) <= dot(toRight, toRight);
        const LiveAxleKinematicsOutput axle = evaluateLiveAxleKinematics(
            description.liveAxle,
            { leftSide ? input.compressionM : input.pairedCompressionM,
              leftSide ? input.pairedCompressionM : input.compressionM,
              leftSide,
              input.steeringDegrees,
              description.staticCamberDegrees,
              description.staticToeDegrees });
        if (!axle.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = axle.camberDegrees;
        output.toeDegrees = axle.toeDegrees;
        output.localSteeringAxis = normalized(
            rotateAroundAxis(
                description.localSteeringAxis,
                { 0.0f, 0.0f, 1.0f },
                degrees(axle.axleRollRadians)),
            { 0.0f, 1.0f, 0.0f });
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = axle.localWheelCenter;
        output.localWheelForward = axle.localWheelForward;
        output.localWheelRight = axle.localWheelRight;
        output.localWheelUp = axle.localWheelUp;
        output.localUprightRotationDegrees = axle.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = axle.travelClamped;
        output.springCompressionM = axle.springCompressionM;
        output.springMotionRatio = axle.springMotionRatio;
        output.damperCompressionM = axle.damperCompressionM;
        output.damperMotionRatio = axle.damperMotionRatio;
        output.localWheelCenter = axle.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::LeafSpringLiveAxleV1)
    {
        if (!description.leafSpringLiveAxle.authored || !input.pairedCompressionValid)
        {
            output.kinematicsValid = false;
            return output;
        }
        const auto& h = description.leafSpringLiveAxle;
        const heritage::math::Vec3 toLeft = {
            description.localSteeringAxisPoint.x - h.axle.leftWheelCenter.x,
            description.localSteeringAxisPoint.y - h.axle.leftWheelCenter.y,
            description.localSteeringAxisPoint.z - h.axle.leftWheelCenter.z };
        const heritage::math::Vec3 toRight = {
            description.localSteeringAxisPoint.x - h.axle.rightWheelCenter.x,
            description.localSteeringAxisPoint.y - h.axle.rightWheelCenter.y,
            description.localSteeringAxisPoint.z - h.axle.rightWheelCenter.z };
        const bool leftSide = dot(toLeft, toLeft) <= dot(toRight, toRight);
        const LeafSpringLiveAxleOutput leaf = evaluateLeafSpringLiveAxle(
            h,
            { leftSide ? input.compressionM : input.pairedCompressionM,
              leftSide ? input.pairedCompressionM : input.compressionM,
              leftSide,
              input.steeringDegrees,
              description.staticCamberDegrees,
              description.staticToeDegrees });
        if (!leaf.valid || !leaf.axle.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        const LiveAxleKinematicsOutput& axle = leaf.axle;
        output.camberDegrees = axle.camberDegrees;
        output.toeDegrees = axle.toeDegrees;
        output.localSteeringAxis = normalized(
            rotateAroundAxis(
                description.localSteeringAxis,
                { 0.0f, 0.0f, 1.0f },
                degrees(axle.axleRollRadians)),
            { 0.0f, 1.0f, 0.0f });
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = axle.localWheelCenter;
        output.localWheelForward = axle.localWheelForward;
        output.localWheelRight = axle.localWheelRight;
        output.localWheelUp = axle.localWheelUp;
        output.localUprightRotationDegrees = axle.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = axle.travelClamped;
        output.springCompressionM = leaf.leafCompressionM;
        output.springMotionRatio = std::abs(leaf.leafMotionRatio);
        output.damperCompressionM = axle.damperCompressionM;
        output.damperMotionRatio = axle.damperMotionRatio;
        output.localWheelCenter = axle.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::SemiTrailingArmV1)
    {
        const SemiTrailingArmKinematicsOutput arm = evaluateSemiTrailingArmKinematics(
            description.semiTrailingArm,
            { input.compressionM, input.localSuspensionDirection,
              description.staticCamberDegrees, description.staticToeDegrees });
        if (!arm.valid) { output.kinematicsValid = false; return output; }
        output.camberDegrees = arm.camberDegrees;
        output.toeDegrees = arm.toeDegrees;
        output.localSteeringAxis = description.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = arm.localWheelCenter;
        output.localWheelForward = arm.localWheelForward;
        output.localWheelRight = arm.localWheelRight;
        output.localWheelUp = arm.localWheelUp;
        output.localUprightRotationDegrees = arm.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = arm.travelClamped;
        output.bumpSteerDegrees = arm.bumpSteerDegrees;
        output.springCompressionM = arm.springCompressionM;
        output.springMotionRatio = arm.springMotionRatio;
        output.damperCompressionM = arm.damperCompressionM;
        output.damperMotionRatio = arm.damperMotionRatio;
        output.localWheelCenter = arm.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::TwistBeamV1)
    {
        if (!description.twistBeam.authored || !input.pairedCompressionValid)
        { output.kinematicsValid = false; return output; }
        const auto& h = description.twistBeam;
        const heritage::math::Vec3 toLeft{
            description.localSteeringAxisPoint.x - h.leftArm.wheelCenter.x,
            description.localSteeringAxisPoint.y - h.leftArm.wheelCenter.y,
            description.localSteeringAxisPoint.z - h.leftArm.wheelCenter.z };
        const heritage::math::Vec3 toRight{
            description.localSteeringAxisPoint.x - h.rightArm.wheelCenter.x,
            description.localSteeringAxisPoint.y - h.rightArm.wheelCenter.y,
            description.localSteeringAxisPoint.z - h.rightArm.wheelCenter.z };
        const bool leftSide = dot(toLeft,toLeft) <= dot(toRight,toRight);
        const TwistBeamKinematicsOutput beam = evaluateTwistBeamKinematics(
            h,
            { leftSide ? input.compressionM : input.pairedCompressionM,
              leftSide ? input.pairedCompressionM : input.compressionM,
              leftSide ? input.compressionVelocityMps : input.pairedCompressionVelocityMps,
              leftSide ? input.pairedCompressionVelocityMps : input.compressionVelocityMps,
              leftSide, input.localSuspensionDirection,
              description.staticCamberDegrees, description.staticToeDegrees });
        if (!beam.valid) { output.kinematicsValid = false; return output; }
        const auto& arm = beam.arm;
        output.camberDegrees = arm.camberDegrees;
        output.toeDegrees = arm.toeDegrees;
        output.localSteeringAxis = description.localSteeringAxis;
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = arm.localWheelCenter;
        output.localWheelForward = arm.localWheelForward;
        output.localWheelRight = arm.localWheelRight;
        output.localWheelUp = arm.localWheelUp;
        output.localUprightRotationDegrees = arm.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = beam.travelClamped;
        output.bumpSteerDegrees = arm.bumpSteerDegrees;
        output.springCompressionM = arm.springCompressionM;
        output.springMotionRatio = arm.springMotionRatio;
        output.damperCompressionM = arm.damperCompressionM;
        output.damperMotionRatio = arm.damperMotionRatio;
        output.twistBeamTwistRadians = beam.beamTwistRadians;
        output.twistBeamTwistRateRadiansPerSecond = beam.beamTwistRateRadiansPerSecond;
        output.twistBeamAngularMotionRatioRadPerM = beam.beamAngularMotionRatioRadPerM;
        output.localWheelCenter = arm.localWheelCenter;
        return output;
    }
    if (description.provider == SuspensionProviderKind::TrailingArmTorsionBarV1)
    {
        const TrailingArmKinematicsOutput trailingArm =
            evaluateTrailingArmKinematics(
                description.trailingArm,
                { input.compressionM,
                  input.localSuspensionDirection,
                  description.staticCamberDegrees,
                  description.staticToeDegrees });
        if (!trailingArm.valid)
        {
            output.kinematicsValid = false;
            return output;
        }
        output.camberDegrees = trailingArm.camberDegrees;
        output.toeDegrees = trailingArm.toeDegrees;
        output.localSteeringAxis = { 0.0f, 1.0f, 0.0f };
        output.steeringAxisPointValid = true;
        output.localSteeringAxisPoint = description.localSteeringAxisPoint;
        output.localWheelForward = trailingArm.localWheelForward;
        output.localWheelRight = trailingArm.localWheelRight;
        output.localWheelUp = trailingArm.localWheelUp;
        output.localUprightRotationDegrees =
            trailingArm.localUprightRotationDegrees;
        output.kinematicsValid = true;
        output.travelClamped = trailingArm.travelClamped;
        output.damperCompressionM = trailingArm.damperCompressionM;
        output.damperMotionRatio = trailingArm.damperMotionRatio;
        output.springMotionRatio = trailingArm.damperMotionRatio;
        output.springTwistRadians = trailingArm.torsionBarTwistRadians;
        output.springAngularMotionRatioRadPerM =
            trailingArm.torsionBarAngularMotionRatioRadPerM;
        output.referenceSpringAngularMotionRatioRadPerM =
            trailingArm.referenceTorsionBarAngularMotionRatioRadPerM;
        output.localWheelCenter = trailingArm.localWheelCenter;
        return output;
    }
    if (description.provider != SuspensionProviderKind::LinearRaycastV1)
    {
        output.kinematicsValid = false;
        return output;
    }

    output.camberDegrees = travelCurve(
        description.staticCamberDegrees,
        description.camberGainDegreesPerM,
        description.camberProgressionDegreesPerM2,
        input.compressionM);
    output.toeDegrees = travelCurve(
        description.staticToeDegrees,
        description.toeGainDegreesPerM,
        description.toeProgressionDegreesPerM2,
        input.compressionM);
    output.localSteeringAxis = description.casterOverrideEnabled
        ? steeringAxisWithCaster(
            description.localSteeringAxis,
            description.staticCasterDegrees)
        : normalized(
            description.localSteeringAxis,
            { 0.0f, 1.0f, 0.0f });
    output.steeringAxisPointValid = true;
    output.localSteeringAxisPoint = description.localSteeringAxisPoint;

    heritage::math::Vec3 forward = rotateAroundAxis(
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f },
        output.toeDegrees);
    heritage::math::Vec3 right = rotateAroundAxis(
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        output.toeDegrees);
    heritage::math::Vec3 up{ 0.0f, 1.0f, 0.0f };

    right = rotateAroundAxis(right, forward, output.camberDegrees);
    up = rotateAroundAxis(up, forward, output.camberDegrees);

    forward = rotateAroundAxis(
        forward,
        output.localSteeringAxis,
        input.steeringDegrees);
    right = rotateAroundAxis(
        right,
        output.localSteeringAxis,
        input.steeringDegrees);
    up = rotateAroundAxis(
        up,
        output.localSteeringAxis,
        input.steeringDegrees);

    // Re-orthogonalize after the composed rotations so downstream tire and
    // presentation code receives a stable right-handed upright basis.
    output.localWheelForward = normalized(
        forward,
        { 0.0f, 0.0f, 1.0f });
    output.localWheelRight = normalized(
        right,
        { 1.0f, 0.0f, 0.0f });
    output.localWheelUp = normalized(
        cross(output.localWheelForward, output.localWheelRight),
        { 0.0f, 1.0f, 0.0f });
    output.localWheelRight = normalized(
        cross(output.localWheelUp, output.localWheelForward),
        { 1.0f, 0.0f, 0.0f });
    output.localUprightRotationDegrees = eulerDegreesFromBasis(
        output.localWheelRight,
        output.localWheelUp,
        output.localWheelForward);
    return output;
}


SuspensionSupportOffsetOutput evaluateSuspensionSupportOffset(
    const SuspensionGeometryDescription& description,
    const SuspensionGeometryInput& input)
{
    SuspensionSupportOffsetOutput output;
    heritage::math::Vec3 referenceCenter{};
    switch (description.provider)
    {
    case SuspensionProviderKind::MacPhersonStrutV1:
        if (!description.macPherson.authored)
            return output;
        referenceCenter = description.macPherson.wheelCenter;
        break;
    case SuspensionProviderKind::DoubleWishboneV1:
        if (!description.doubleWishbone.authored)
            return output;
        referenceCenter = description.doubleWishbone.wheelCenter;
        break;
    case SuspensionProviderKind::PushrodDoubleWishboneV1:
        if (!description.pushrodDoubleWishbone.authored)
            return output;
        referenceCenter = description.pushrodDoubleWishbone.wishbone.wheelCenter;
        break;
    case SuspensionProviderKind::SemiTrailingArmV1:
        if (!description.semiTrailingArm.authored) return output;
        referenceCenter = description.semiTrailingArm.wheelCenter;
        break;
    case SuspensionProviderKind::TwistBeamV1:
        if (!description.twistBeam.authored || !input.pairedCompressionValid) return output;
        {
            const auto& h = description.twistBeam;
            const heritage::math::Vec3 toLeft{description.localSteeringAxisPoint.x-h.leftArm.wheelCenter.x,description.localSteeringAxisPoint.y-h.leftArm.wheelCenter.y,description.localSteeringAxisPoint.z-h.leftArm.wheelCenter.z};
            const heritage::math::Vec3 toRight{description.localSteeringAxisPoint.x-h.rightArm.wheelCenter.x,description.localSteeringAxisPoint.y-h.rightArm.wheelCenter.y,description.localSteeringAxisPoint.z-h.rightArm.wheelCenter.z};
            referenceCenter = dot(toLeft,toLeft)<=dot(toRight,toRight)?h.leftArm.wheelCenter:h.rightArm.wheelCenter;
        }
        break;
    case SuspensionProviderKind::TrailingArmTorsionBarV1:
        if (!description.trailingArm.authored)
            return output;
        referenceCenter = description.trailingArm.wheelCenter;
        break;
    case SuspensionProviderKind::MultiLinkV1:
        if (!description.multiLink.authored)
            return output;
        referenceCenter = description.multiLink.wheelCenter;
        break;
    case SuspensionProviderKind::MotorcycleTelescopicForkV1:
        if (!description.motorcycleFork.authored)
            return output;
        referenceCenter = description.motorcycleFork.wheelCenter;
        break;
    case SuspensionProviderKind::MotorcycleSwingarmLinkageV1:
        if (!description.motorcycleSwingarm.authored)
            return output;
        referenceCenter = description.motorcycleSwingarm.wheelCenter;
        break;
    case SuspensionProviderKind::LeafSpringLiveAxleV1:
        if (!description.leafSpringLiveAxle.authored || !input.pairedCompressionValid)
            return output;
        {
            const auto& axle = description.leafSpringLiveAxle.axle;
            const heritage::math::Vec3 toLeft{
                description.localSteeringAxisPoint.x - axle.leftWheelCenter.x,
                description.localSteeringAxisPoint.y - axle.leftWheelCenter.y,
                description.localSteeringAxisPoint.z - axle.leftWheelCenter.z };
            const heritage::math::Vec3 toRight{
                description.localSteeringAxisPoint.x - axle.rightWheelCenter.x,
                description.localSteeringAxisPoint.y - axle.rightWheelCenter.y,
                description.localSteeringAxisPoint.z - axle.rightWheelCenter.z };
            referenceCenter = dot(toLeft, toLeft) <= dot(toRight, toRight)
                ? axle.leftWheelCenter : axle.rightWheelCenter;
        }
        break;
    case SuspensionProviderKind::LiveAxleV1:
        if (!description.liveAxle.authored || !input.pairedCompressionValid)
            return output;
        {
            const heritage::math::Vec3 toLeft{
                description.localSteeringAxisPoint.x - description.liveAxle.leftWheelCenter.x,
                description.localSteeringAxisPoint.y - description.liveAxle.leftWheelCenter.y,
                description.localSteeringAxisPoint.z - description.liveAxle.leftWheelCenter.z };
            const heritage::math::Vec3 toRight{
                description.localSteeringAxisPoint.x - description.liveAxle.rightWheelCenter.x,
                description.localSteeringAxisPoint.y - description.liveAxle.rightWheelCenter.y,
                description.localSteeringAxisPoint.z - description.liveAxle.rightWheelCenter.z };
            referenceCenter = dot(toLeft, toLeft) <= dot(toRight, toRight)
                ? description.liveAxle.leftWheelCenter
                : description.liveAxle.rightWheelCenter;
        }
        break;
    case SuspensionProviderKind::KartChassisFlexV1:
        if (!description.kartChassis.authored)
            return output;
        {
            const KartChassisKinematicsOutput kart = evaluateKartChassisKinematics(
                description.kartChassis,
                { input.compressionM, input.steeringDegrees,
                  description.localSteeringAxisPoint,
                  description.staticCamberDegrees, description.staticToeDegrees });
            if (!kart.valid)
                return output;
            referenceCenter = kart.referenceWheelCenter;
        }
        break;
    case SuspensionProviderKind::LinearRaycastV1:
    default:
        // Compatibility suspension intentionally has no hardpoint wheel path.
        output.valid = true;
        return output;
    }

    const SuspensionGeometryOutput geometry = evaluateSuspensionGeometry(
        description, input);
    if (!geometry.kinematicsValid)
        return output;

    const heritage::math::Vec3 delta{
        geometry.localWheelCenter.x - referenceCenter.x,
        geometry.localWheelCenter.y - referenceCenter.y,
        geometry.localWheelCenter.z - referenceCenter.z
    };
    const heritage::math::Vec3 axis = normalized(
        input.localSuspensionDirection, { 0.0f, -1.0f, 0.0f });
    const float axial = dot(delta, axis);
    const heritage::math::Vec3 transverse{
        delta.x - axis.x * axial,
        delta.y - axis.y * axial,
        delta.z - axis.z * axial
    };
    // SUSP11 exception: kart front-wheel vertical movement is steering jacking,
    // not suspension compression. It must therefore reach the road-support ray
    // in full; removing the suspension-axis component would erase the physical
    // mechanism that twists the kart frame and unloads the inside rear. Rear
    // wheel delta is zero because the solid axle is rigidly chassis-mounted.
    const heritage::math::Vec3 supportDelta =
        description.provider == SuspensionProviderKind::KartChassisFlexV1
            ? delta : transverse;
    const float offsetMagnitudeSquared = dot(supportDelta, supportDelta);
    const float maximumOffsetM =
        description.provider == SuspensionProviderKind::MotorcycleTelescopicForkV1
            ? 0.60f
            : description.provider == SuspensionProviderKind::KartChassisFlexV1
                ? 0.18f : 0.25f;
    if (!std::isfinite(offsetMagnitudeSquared)
        || offsetMagnitudeSquared > maximumOffsetM * maximumOffsetM)
    {
        return output;
    }

    output.localTransverseOffset = supportDelta;
    output.valid = true;
    return output;
}

} // namespace heritage::vehicles
