#pragma once

// CLEAN02 transitional implementation-only helpers for VehicleSystem's split
// translation units. This preserves the pre-split math/validation behavior.
// CLEAN04A moves reusable quaternion algebra into Core/Math; remaining helpers stay local until their transform-space contracts genuinely match.

#include "VehicleSystem.hpp"
#include "../Core/Math/Quaternion.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles::vehicle_system_detail {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kMinimumHighRateHertz = 120.0f;
inline constexpr float kMaximumHighRateHertz = 2000.0f;
inline constexpr int kMaximumHighRateStepsPerWorldStep = 32;
inline constexpr float kVectorEpsilon = 1.0e-6f;
inline constexpr float kVehicleRestDelaySeconds = 0.75f;
inline constexpr float kVehicleRestLinearSpeed = 0.04f;
inline constexpr float kVehicleRestAngularSpeedDegrees = 1.0f;
inline constexpr float kVehicleRestWheelSpeed = 0.15f;
inline constexpr float kVehicleRestFlatSlopeDegrees = 0.5f;
inline constexpr float kLowSpeedTireBlendStart = 0.50f;
inline constexpr float kLowSpeedTireBlendEnd = 2.00f;

inline bool finiteFloat(float value)
{
    return std::isfinite(static_cast<double>(value));
}

inline bool finiteVec3(const heritage::math::Vec3& value)
{
    return finiteFloat(value.x) && finiteFloat(value.y) && finiteFloat(value.z);
}

inline heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    VehicleScalar scalar)
{
    return {
        static_cast<float>(static_cast<VehicleScalar>(value.x) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.y) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.z) * scalar)
    };
}

inline float dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

inline float length(const heritage::math::Vec3& value)
{
    return std::sqrt(lengthSquared(value));
}

inline VehicleScalar smoothStep01(VehicleScalar value)
{
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

inline heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitude = length(value);
    return magnitude > kVectorEpsilon
        ? scale(value, 1.0f / magnitude)
        : fallback;
}

inline float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

inline VehicleScalar radians(VehicleScalar degreesValue)
{
    constexpr VehicleScalar pi = 3.141592653589793238462643383279502884;
    return degreesValue * (pi / 180.0);
}

inline VehicleScalar degrees(VehicleScalar radiansValue)
{
    constexpr VehicleScalar pi = 3.141592653589793238462643383279502884;
    return radiansValue * (180.0 / pi);
}

using Quaternion = heritage::math::Quaternion;

inline Quaternion quaternionFromEulerDegrees(const heritage::math::Vec3& value)
{
    const Quaternion result = heritage::math::makeQuaternionFromEulerDegrees(value);
    const float magnitude = std::sqrt(heritage::math::quaternionLengthSquared(result));
    if (magnitude <= kVectorEpsilon)
        return {};
    Quaternion normalizedResult = result;
    normalizedResult.w /= magnitude;
    normalizedResult.x /= magnitude;
    normalizedResult.y /= magnitude;
    normalizedResult.z /= magnitude;
    return normalizedResult;
}

inline heritage::math::Vec3 rotateVector(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    return heritage::math::rotateVectorUnit(rotation, value);
}

inline heritage::math::Vec3 inverseRotateVector(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    return heritage::math::rotateVectorUnit(
        heritage::math::conjugate(rotation),
        value);
}

inline heritage::math::Vec3 pointVelocityFromOffset(
    const heritage::math::Vec3& linearVelocity,
    const heritage::math::Vec3& angularVelocityDegrees,
    const heritage::math::Vec3& pointOffsetFromBody)
{
    const heritage::math::Vec3 angularRadians{
        radians(angularVelocityDegrees.x),
        radians(angularVelocityDegrees.y),
        radians(angularVelocityDegrees.z)
    };
    return add(
        linearVelocity,
        cross(angularRadians, pointOffsetFromBody));
}

inline heritage::math::Vec3 pointVelocity(
    const heritage::math::Vec3& linearVelocity,
    const heritage::math::Vec3& angularVelocityDegrees,
    const heritage::math::Vec3& bodyPosition,
    const heritage::math::Vec3& worldPoint)
{
    return pointVelocityFromOffset(
        linearVelocity,
        angularVelocityDegrees,
        subtract(worldPoint, bodyPosition));
}

inline float signOrZero(float value)
{
    if (value > 0.0001f)
        return 1.0f;
    if (value < -0.0001f)
        return -1.0f;
    return 0.0f;
}

inline float moveTowards(float current, float target, float maximumDelta)
{
    if (maximumDelta <= 0.0f)
        return current;
    const float difference = target - current;
    if (std::abs(difference) <= maximumDelta)
        return target;
    return current + signOrZero(difference) * maximumDelta;
}

inline VehicleScalar moveTowardsScalar(
    VehicleScalar current,
    VehicleScalar target,
    VehicleScalar maximumDelta)
{
    if (maximumDelta <= 0.0)
        return current;
    const VehicleScalar difference = target - current;
    if (std::abs(difference) <= maximumDelta)
        return target;
    return current + (difference > 0.0 ? maximumDelta : -maximumDelta);
}

// Steering convention used by the native vehicle solver:
//   negative road-wheel angle = LEFT turn (-X)
//   positive road-wheel angle = RIGHT turn (+X)
// Native vehicle coordinates are +X right, +Y up, +Z forward. Keeping this
// convention identical from input -> Ackermann -> suspension geometry -> tire
// basis prevents the sign from being silently inverted at different layers.
struct AckermannSolution
{
    float innerMagnitudeDegrees = 0.0f;
    float outerMagnitudeDegrees = 0.0f;
};

inline AckermannSolution solveAckermann(
    float centerAngleDegrees,
    float wheelbase,
    float track,
    float ackermannPercent)
{
    AckermannSolution result;
    const double centerMagnitude = std::abs(
        static_cast<double>(centerAngleDegrees));
    result.innerMagnitudeDegrees = static_cast<float>(centerMagnitude);
    result.outerMagnitudeDegrees = static_cast<float>(centerMagnitude);

    if (centerMagnitude <= 0.001
        || wheelbase <= 0.01f
        || track <= 0.01f)
    {
        return result;
    }

    constexpr double pi = 3.141592653589793238462643383279502884;
    const double wheelbaseD = static_cast<double>(wheelbase);
    const double halfTrack = static_cast<double>(track) * 0.5;
    const double centerRadians = centerMagnitude * (pi / 180.0);
    const double tangent = std::tan(centerRadians);
    if (!std::isfinite(tangent) || std::abs(tangent) <= 1.0e-9)
        return result;

    // Bicycle-model turn radius measured from the steering-axle centre to the
    // instantaneous centre of rotation. atan2 remains stable at high lock.
    const double centerRadius = wheelbaseD / std::abs(tangent);
    const double innerRadius = std::max(0.01, centerRadius - halfTrack);
    const double outerRadius = centerRadius + halfTrack;
    const double idealInner = std::atan2(wheelbaseD, innerRadius)
        * (180.0 / pi);
    const double idealOuter = std::atan2(wheelbaseD, outerRadius)
        * (180.0 / pi);

    // 0 = parallel steering; 1 = geometrically ideal Ackermann. Values outside
    // that range remain supported for anti-/over-Ackermann experimentation.
    const double blend = static_cast<double>(ackermannPercent);
    const double inner = centerMagnitude
        + (idealInner - centerMagnitude) * blend;
    const double outer = centerMagnitude
        + (idealOuter - centerMagnitude) * blend;
    result.innerMagnitudeDegrees = static_cast<float>(
        std::clamp(inner, 0.0, 85.0));
    result.outerMagnitudeDegrees = static_cast<float>(
        std::clamp(outer, 0.0, 85.0));
    return result;
}

inline bool validVehicleDescription(const VehicleDescription& value)
{
    return finiteFloat(value.highRateHertz)
        && value.highRateHertz >= kMinimumHighRateHertz
        && value.highRateHertz <= kMaximumHighRateHertz
        && finiteFloat(value.maximumDriveForce)
        && value.maximumDriveForce >= 0.0f
        && finiteFloat(value.maximumBrakeForce)
        && value.maximumBrakeForce >= 0.0f
        && finiteFloat(value.maximumSteerAngleDegrees)
        && value.maximumSteerAngleDegrees >= 0.0f
        && value.maximumSteerAngleDegrees <= 85.0f
        && finiteFloat(value.ackermannPercent)
        && value.ackermannPercent >= -1.0f
        && value.ackermannPercent <= 2.0f
        && finiteFloat(value.steeringRateDegreesPerSecond)
        && value.steeringRateDegreesPerSecond >= 1.0f
        && value.steeringRateDegreesPerSecond <= 1440.0f
        && finiteFloat(value.steeringReturnRateDegreesPerSecond)
        && value.steeringReturnRateDegreesPerSecond >= 1.0f
        && value.steeringReturnRateDegreesPerSecond <= 1440.0f
        && finiteFloat(value.highSpeedSteeringRateFactor)
        && value.highSpeedSteeringRateFactor >= 0.05f
        && value.highSpeedSteeringRateFactor <= 1.0f
        && finiteFloat(value.highSpeedReferenceMps)
        && value.highSpeedReferenceMps >= 1.0f
        && value.highSpeedReferenceMps <= 150.0f
        && finiteFloat(value.tireFriction)
        && value.tireFriction >= 0.0f
        && value.tireFriction <= 5.0f
        && finiteFloat(value.lateralStiffness)
        && value.lateralStiffness >= 0.0f
        && finiteFloat(value.rollingResistance)
        && value.rollingResistance >= 0.0f;
}

inline bool validWheelDescription(const WheelDescription& value)
{
    return finiteVec3(value.localMount)
        && finiteVec3(value.localSuspensionDirection)
        && lengthSquared(value.localSuspensionDirection) > kVectorEpsilon
        && finiteFloat(value.radius) && value.radius > 0.01f && value.radius <= 5.0f
        && finiteFloat(value.restLength) && value.restLength >= 0.01f && value.restLength <= 5.0f
        && finiteFloat(value.maximumCompression) && value.maximumCompression >= 0.0f
        && finiteFloat(value.maximumDroop) && value.maximumDroop >= 0.0f
        && value.maximumCompression < value.restLength
        && finiteFloat(value.springPreload) && value.springPreload >= 0.0f
        && finiteFloat(value.springRate) && value.springRate >= 0.0f
        && finiteFloat(value.springProgression) && value.springProgression >= 0.0f
        && finiteFloat(value.bumpDamping) && value.bumpDamping >= 0.0f
        && finiteFloat(value.bumpHighSpeedDamping)
        && value.bumpHighSpeedDamping >= 0.0f
        && finiteFloat(value.bumpDampingKneeVelocity)
        && value.bumpDampingKneeVelocity >= 0.0f
        && finiteFloat(value.reboundDamping) && value.reboundDamping >= 0.0f
        && finiteFloat(value.reboundHighSpeedDamping)
        && value.reboundHighSpeedDamping >= 0.0f
        && finiteFloat(value.reboundDampingKneeVelocity)
        && value.reboundDampingKneeVelocity >= 0.0f
        && finiteFloat(value.bumpStopEngagement)
        && value.bumpStopEngagement >= 0.0f
        && finiteFloat(value.bumpStopRate) && value.bumpStopRate >= 0.0f
        && finiteFloat(value.bumpStopProgression)
        && value.bumpStopProgression >= 0.0f
        && finiteFloat(value.droopStopEngagement)
        && value.droopStopEngagement >= 0.0f
        && finiteFloat(value.droopStopRate) && value.droopStopRate >= 0.0f
        && (value.suspensionProvider == SuspensionProviderKind::LinearRaycastV1
            || (value.suspensionProvider == SuspensionProviderKind::MacPhersonStrutV1
                && validMacPhersonHardpoints(value.macPhersonHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::DoubleWishboneV1
                && validDoubleWishboneHardpoints(value.doubleWishboneHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::PushrodDoubleWishboneV1
                && validPushrodDoubleWishboneHardpoints(
                    value.pushrodDoubleWishboneHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::TrailingArmTorsionBarV1
                && validTrailingArmHardpoints(value.trailingArmHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::LiveAxleV1
                && validLiveAxleHardpoints(value.liveAxleHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::LeafSpringLiveAxleV1
                && validLeafSpringLiveAxleHardpoints(value.leafSpringLiveAxleHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::MotorcycleTelescopicForkV1
                && validMotorcycleForkHardpoints(value.motorcycleForkHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::MotorcycleSwingarmLinkageV1
                && validMotorcycleSwingarmHardpoints(value.motorcycleSwingarmHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::KartChassisFlexV1
                && validKartChassisHardpoints(value.kartChassisHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::MultiLinkV1
                && validMultiLinkHardpoints(value.multiLinkHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::SemiTrailingArmV1
                && validSemiTrailingArmHardpoints(value.semiTrailingArmHardpoints))
            || (value.suspensionProvider == SuspensionProviderKind::TwistBeamV1
                && validTwistBeamHardpoints(value.twistBeamHardpoints)))
        && finiteVec3(value.localSteeringAxis)
        && lengthSquared(value.localSteeringAxis) > kVectorEpsilon
        && finiteFloat(value.staticCamberDegrees)
        && std::abs(value.staticCamberDegrees) <= 45.0f
        && finiteFloat(value.camberGainDegreesPerM)
        && std::abs(value.camberGainDegreesPerM) <= 1000.0f
        && finiteFloat(value.camberProgressionDegreesPerM2)
        && std::abs(value.camberProgressionDegreesPerM2) <= 10000.0f
        && finiteFloat(value.staticToeDegrees)
        && std::abs(value.staticToeDegrees) <= 45.0f
        && (!value.casterOverrideEnabled
            || (finiteFloat(value.staticCasterDegrees)
                && value.staticCasterDegrees >= -20.0f
                && value.staticCasterDegrees <= 30.0f))
        && validWheelFitmentDescription(value.fitment)
        && finiteFloat(value.toeGainDegreesPerM)
        && std::abs(value.toeGainDegreesPerM) <= 1000.0f
        && finiteFloat(value.toeProgressionDegreesPerM2)
        && std::abs(value.toeProgressionDegreesPerM2) <= 10000.0f
        && finiteFloat(value.suspensionMotionRatio)
        && value.suspensionMotionRatio > 0.0f
        && value.suspensionMotionRatio <= 10.0f
        && finiteFloat(value.maximumSuspensionForce)
        && value.maximumSuspensionForce > 0.0f
        && finiteFloat(value.leafInterleafFrictionN) && value.leafInterleafFrictionN >= 0.0f
        && finiteFloat(value.leafInterleafVelocityScaleMps) && value.leafInterleafVelocityScaleMps > 0.0001f
        && finiteFloat(value.leafInterleafViscousNsPerM) && value.leafInterleafViscousNsPerM >= 0.0f
        && finiteFloat(value.leafAxleWrapStiffnessNmPerRad) && value.leafAxleWrapStiffnessNmPerRad >= 0.0f
        && finiteFloat(value.leafAxleWrapDampingNmsPerRad) && value.leafAxleWrapDampingNmsPerRad >= 0.0f
        && finiteFloat(value.leafAxleWrapInertiaKgM2) && value.leafAxleWrapInertiaKgM2 > 0.01f
        && finiteFloat(value.leafAxleWrapJackingNPerRad) && value.leafAxleWrapJackingNPerRad >= 0.0f
        && finiteFloat(value.motorcycleRearSprocketPitchRadiusM)
        && value.motorcycleRearSprocketPitchRadiusM >= 0.02f
        && value.motorcycleRearSprocketPitchRadiusM <= 0.30f
        && finiteFloat(value.twistBeamTorsionalStiffnessNmPerRad)
        && value.twistBeamTorsionalStiffnessNmPerRad >= 0.0f
        && finiteFloat(value.twistBeamTorsionalDampingNmsPerRad)
        && value.twistBeamTorsionalDampingNmsPerRad >= 0.0f
        && finiteFloat(value.effectiveUnsprungMass)
        && value.effectiveUnsprungMass >= 0.0f
        && value.effectiveUnsprungMass <= 1000.0f
        && finiteFloat(value.tireRadialStiffness)
        && value.tireRadialStiffness > 0.0f
        && value.tireRadialStiffness <= 10000000.0f
        && finiteFloat(value.tireRadialDamping)
        && value.tireRadialDamping >= 0.0f
        && value.tireRadialDamping <= 1000000.0f
        && finiteFloat(value.maximumTireDeflection)
        && value.maximumTireDeflection > 0.0f
        && value.maximumTireDeflection <= 1.0f
        && finiteFloat(value.maximumTireNormalForce)
        && value.maximumTireNormalForce > 0.0f
        && value.maximumTireNormalForce <= 10000000.0f
        && finiteFloat(value.driveFactor) && value.driveFactor >= 0.0f
        && finiteFloat(value.steerFactor) && value.steerFactor >= -1.0f && value.steerFactor <= 1.0f
        && finiteFloat(value.brakeFactor) && value.brakeFactor >= 0.0f
        && finiteFloat(value.handbrakeFactor) && value.handbrakeFactor >= 0.0f;
}

inline SuspensionModelDescription suspensionModelDescription(
    const WheelDescription& value)
{
    SuspensionModelDescription result;
    result.provider = value.suspensionProvider;
    result.springPreloadN = value.springPreload;
    result.springRateNPerM = value.springRate;
    result.springProgressionNPerM2 = value.springProgression;
    result.bumpDampingNsPerM = value.bumpDamping;
    result.bumpHighSpeedDampingNsPerM = value.bumpHighSpeedDamping;
    result.bumpDampingKneeVelocityMps = value.bumpDampingKneeVelocity;
    result.reboundDampingNsPerM = value.reboundDamping;
    result.reboundHighSpeedDampingNsPerM =
        value.reboundHighSpeedDamping;
    result.reboundDampingKneeVelocityMps =
        value.reboundDampingKneeVelocity;
    result.bumpStopEngagementM = value.bumpStopEngagement;
    result.bumpStopRateNPerM = value.bumpStopRate;
    result.bumpStopProgressionNPerM2 = value.bumpStopProgression;
    result.droopStopEngagementM = value.droopStopEngagement;
    result.droopStopRateNPerM = value.droopStopRate;
    result.motionRatio = value.suspensionMotionRatio;
    result.maximumForceN = value.maximumSuspensionForce;
    result.leafInterleafFrictionN = value.leafInterleafFrictionN;
    result.leafInterleafVelocityScaleMps = value.leafInterleafVelocityScaleMps;
    result.leafInterleafViscousNsPerM = value.leafInterleafViscousNsPerM;
    result.leafAxleWrapStiffnessNmPerRad = value.leafAxleWrapStiffnessNmPerRad;
    result.leafAxleWrapDampingNmsPerRad = value.leafAxleWrapDampingNmsPerRad;
    result.leafAxleWrapInertiaKgM2 = value.leafAxleWrapInertiaKgM2;
    result.leafAxleWrapJackingNPerRad = value.leafAxleWrapJackingNPerRad;
    result.motorcycleRearSprocketPitchRadiusM = value.motorcycleRearSprocketPitchRadiusM;
    result.twistBeamTorsionalStiffnessNmPerRad = value.twistBeamTorsionalStiffnessNmPerRad;
    result.twistBeamTorsionalDampingNmsPerRad = value.twistBeamTorsionalDampingNmsPerRad;
    return result;
}

inline SuspensionGeometryDescription suspensionGeometryDescription(
    const WheelDescription& value)
{
    SuspensionGeometryDescription result;
    result.provider = value.suspensionProvider;
    result.localSteeringAxisPoint = value.localMount;
    result.localSteeringAxis = value.localSteeringAxis;
    result.staticCamberDegrees = value.staticCamberDegrees;
    result.camberGainDegreesPerM = value.camberGainDegreesPerM;
    result.camberProgressionDegreesPerM2 =
        value.camberProgressionDegreesPerM2;
    result.staticToeDegrees = value.staticToeDegrees;
    result.casterOverrideEnabled = value.casterOverrideEnabled;
    result.staticCasterDegrees = value.staticCasterDegrees;
    result.toeGainDegreesPerM = value.toeGainDegreesPerM;
    result.toeProgressionDegreesPerM2 =
        value.toeProgressionDegreesPerM2;
    result.macPherson = value.macPhersonHardpoints;
    result.doubleWishbone = value.doubleWishboneHardpoints;
    result.pushrodDoubleWishbone = value.pushrodDoubleWishboneHardpoints;
    result.trailingArm = value.trailingArmHardpoints;
    result.liveAxle = value.liveAxleHardpoints;
    result.leafSpringLiveAxle = value.leafSpringLiveAxleHardpoints;
    result.motorcycleFork = value.motorcycleForkHardpoints;
    result.motorcycleSwingarm = value.motorcycleSwingarmHardpoints;
    result.kartChassis = value.kartChassisHardpoints;
    result.multiLink = value.multiLinkHardpoints;
    result.semiTrailingArm = value.semiTrailingArmHardpoints;
    result.twistBeam = value.twistBeamHardpoints;
    return result;
}

inline heritage::math::Vec3 wheelCenterlineFitmentOffsetLocal(
    const WheelDescription& value)
{
    return wheelCenterlineOffsetLocal(value.localMount, value.fitment);
}

inline bool validPowertrainDescription(const PowertrainDescription& value)
{
    if (!finiteFloat(value.idleRpm)
        || !finiteFloat(value.redlineRpm)
        || value.idleRpm < 300.0f
        || value.idleRpm > 4000.0f
        || value.redlineRpm <= value.idleRpm + 250.0f
        || value.redlineRpm > 30000.0f
        || !finiteFloat(value.maximumTorque)
        || value.maximumTorque < 0.0f
        || value.maximumTorque > 10000.0f
        || !finiteFloat(value.engineBrakingTorque)
        || value.engineBrakingTorque < 0.0f
        || value.engineBrakingTorque > 5000.0f
        || !finiteFloat(value.engineResponse)
        || value.engineResponse < 0.1f
        || value.engineResponse > 100.0f
        || !finiteFloat(value.finalDriveRatio)
        || value.finalDriveRatio < 0.05f
        || value.finalDriveRatio > 30.0f
        || !finiteFloat(value.drivetrainEfficiency)
        || value.drivetrainEfficiency < 0.05f
        || value.drivetrainEfficiency > 1.0f
        || !finiteFloat(value.shiftDurationSeconds)
        || value.shiftDurationSeconds < 0.0f
        || value.shiftDurationSeconds > 5.0f
        || !finiteFloat(value.clutchEngagementRate)
        || value.clutchEngagementRate < 0.1f
        || value.clutchEngagementRate > 100.0f
        || !finiteFloat(value.reverseGearRatio)
        || value.reverseGearRatio >= -0.05f
        || value.reverseGearRatio < -30.0f
        || !finiteFloat(value.differentialBiasRatio)
        || value.differentialBiasRatio < 1.0f
        || value.differentialBiasRatio > 20.0f
        || value.forwardGearRatios.empty()
        || value.forwardGearRatios.size() > 16)
    {
        return false;
    }

    for (float ratio : value.forwardGearRatios)
    {
        if (!finiteFloat(ratio) || ratio <= 0.05f || ratio > 30.0f)
            return false;
    }
    return true;
}

struct SurfaceProfile
{
    float frictionMultiplier = 1.0f;
    float stiffnessMultiplier = 1.0f;
    float rollingResistanceMultiplier = 1.0f;
    float relaxationMultiplier = 1.0f;
};

inline bool validDriverAidDescription(const DriverAidDescription& value)
{
    return finiteFloat(value.antiLockTargetSlip)
        && value.antiLockTargetSlip >= 0.02f
        && value.antiLockTargetSlip <= 1.0f
        && finiteFloat(value.tractionControlTargetSlip)
        && value.tractionControlTargetSlip >= 0.02f
        && value.tractionControlTargetSlip <= 2.0f
        && finiteFloat(value.minimumActivationSpeed)
        && value.minimumActivationSpeed >= 0.0f
        && value.minimumActivationSpeed <= 50.0f
        && finiteFloat(value.modulationRate)
        && value.modulationRate >= 0.5f
        && value.modulationRate <= 200.0f
        && finiteFloat(value.maximumHandbrakeTorque)
        && value.maximumHandbrakeTorque >= 0.0f
        && value.maximumHandbrakeTorque <= 50000.0f;
}

inline bool validSurface(TireSurface surface)
{
    const int value = static_cast<int>(surface);
    return value >= static_cast<int>(TireSurface::DryAsphalt)
        && value <= static_cast<int>(TireSurface::Ice);
}

inline SurfaceProfile legacySurfaceProfile(TireSurface surface)
{
    switch (surface)
    {
    case TireSurface::WetAsphalt:
        return { 0.74f, 0.86f, 1.15f, 1.15f };
    case TireSurface::Gravel:
        return { 0.62f, 0.58f, 2.10f, 1.40f };
    case TireSurface::Dirt:
        return { 0.54f, 0.48f, 2.55f, 1.60f };
    case TireSurface::Snow:
        return { 0.31f, 0.31f, 3.10f, 1.85f };
    case TireSurface::Ice:
        return { 0.095f, 0.18f, 1.35f, 2.20f };
    case TireSurface::DryAsphalt:
    default:
        return {};
    }
}

inline SurfaceProfile blendSurfaceProfile(
    const SurfaceProfile& dry,
    const SurfaceProfile& wet,
    float wetness)
{
    const float amount = std::clamp(wetness, 0.0f, 1.0f);
    const auto blend = [amount](float first, float second) {
        return first + (second - first) * amount;
    };
    return {
        blend(dry.frictionMultiplier, wet.frictionMultiplier),
        blend(dry.stiffnessMultiplier, wet.stiffnessMultiplier),
        blend(dry.rollingResistanceMultiplier, wet.rollingResistanceMultiplier),
        blend(dry.relaxationMultiplier, wet.relaxationMultiplier)
    };
}

inline bool hardWetSurfaceMaterial(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Default:
    case SurfaceMaterial::Asphalt:
    case SurfaceMaterial::Kerb:
    case SurfaceMaterial::PaintedLine:
        return true;
    default:
        return false;
    }
}

inline SurfaceProfile surfaceProfile(
    heritage::physics::SurfaceMaterial material,
    float wetness,
    TireSurface fallback);

inline bool winterSurfaceMaterial(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    return material == SurfaceMaterial::Snow || material == SurfaceMaterial::Ice;
}

inline bool shallowGranularSurfaceMaterial(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    return material == SurfaceMaterial::Gravel || material == SurfaceMaterial::Dirt;
}

inline bool deformableTerrainSurfaceMaterial(heritage::physics::SurfaceMaterial material)
{
    using heritage::physics::SurfaceMaterial;
    return material == SurfaceMaterial::Mud
        || material == SurfaceMaterial::Sand
        || material == SurfaceMaterial::SoftSoil
        || material == SurfaceMaterial::DeepSnow;
}

inline heritage::physics::SurfaceMaterial dominantDeformableTerrainMaterial(
    heritage::physics::SurfaceMaterial centerMaterial,
    VehicleScalar mudFraction,
    VehicleScalar sandFraction,
    VehicleScalar softSoilFraction,
    VehicleScalar deepSnowFraction)
{
    using heritage::physics::SurfaceMaterial;
    if (deformableTerrainSurfaceMaterial(centerMaterial))
        return centerMaterial;

    SurfaceMaterial material = SurfaceMaterial::Default;
    VehicleScalar largest = VehicleScalar{0.0};
    auto consider = [&](SurfaceMaterial candidate, VehicleScalar fraction) {
        if (fraction > largest)
        {
            largest = fraction;
            material = candidate;
        }
    };
    consider(SurfaceMaterial::Mud, mudFraction);
    consider(SurfaceMaterial::Sand, sandFraction);
    consider(SurfaceMaterial::SoftSoil, softSoilFraction);
    consider(SurfaceMaterial::DeepSnow, deepSnowFraction);
    return material;
}

inline SurfaceProfile providerBaseSurfaceProfile(
    heritage::physics::SurfaceMaterial material,
    float wetness,
    TireSurface fallback,
    bool wetProviderEnabled,
    bool winterProviderEnabled,
    bool shallowGranularProviderEnabled,
    bool deformableTerrainProviderEnabled)
{
    if (winterProviderEnabled && winterSurfaceMaterial(material))
        return {};
    if (shallowGranularProviderEnabled && shallowGranularSurfaceMaterial(material))
        return {};
    if (deformableTerrainProviderEnabled && deformableTerrainSurfaceMaterial(material))
        return {};
    if (wetProviderEnabled && hardWetSurfaceMaterial(material))
        return surfaceProfile(material, 0.0f, fallback);
    return surfaceProfile(material, wetness, fallback);
}

inline VehicleScalar combineDedicatedSurfaceScale(
    VehicleScalar wetScale,
    VehicleScalar winterScale,
    VehicleScalar shallowGranularScale,
    VehicleScalar deformableTerrainScale,
    VehicleScalar minimum,
    VehicleScalar maximum)
{
    return std::clamp(
        VehicleScalar{1.0}
            + (wetScale - VehicleScalar{1.0})
            + (winterScale - VehicleScalar{1.0})
            + (shallowGranularScale - VehicleScalar{1.0})
            + (deformableTerrainScale - VehicleScalar{1.0}),
        minimum, maximum);
}

inline SurfaceProfile surfaceProfile(
    heritage::physics::SurfaceMaterial material,
    float wetness,
    TireSurface fallback)
{
    using heritage::physics::SurfaceMaterial;
    switch (material)
    {
    case SurfaceMaterial::Default:
        return legacySurfaceProfile(fallback);
    case SurfaceMaterial::Asphalt:
        return blendSurfaceProfile(
            {},
            { 0.74f, 0.86f, 1.15f, 1.15f },
            wetness);
    case SurfaceMaterial::Gravel:
        return blendSurfaceProfile(
            { 0.62f, 0.58f, 2.10f, 1.40f },
            { 0.54f, 0.50f, 2.35f, 1.55f },
            wetness);
    case SurfaceMaterial::Dirt:
        return blendSurfaceProfile(
            { 0.54f, 0.48f, 2.55f, 1.60f },
            { 0.45f, 0.40f, 2.90f, 1.75f },
            wetness);
    case SurfaceMaterial::Grass:
        return blendSurfaceProfile(
            { 0.46f, 0.38f, 3.10f, 1.75f },
            { 0.31f, 0.29f, 3.65f, 1.95f },
            wetness);
    case SurfaceMaterial::Snow:
        return { 0.31f, 0.31f, 3.10f, 1.85f };
    case SurfaceMaterial::Ice:
        return { 0.095f, 0.18f, 1.35f, 2.20f };
    // TIRE15 compatibility fallbacks. When the deformable-terrain provider is
    // enabled these are neutralized by providerBaseSurfaceProfile; they remain
    // conservative safety behavior for content that disables the provider.
    case SurfaceMaterial::Mud:
        return { 0.22f, 0.22f, 4.20f, 2.60f };
    case SurfaceMaterial::Sand:
        return { 0.28f, 0.24f, 3.60f, 2.30f };
    case SurfaceMaterial::SoftSoil:
        return { 0.30f, 0.28f, 3.40f, 2.20f };
    case SurfaceMaterial::DeepSnow:
        return { 0.20f, 0.20f, 4.80f, 2.80f };
    case SurfaceMaterial::Kerb:
        return blendSurfaceProfile(
            { 0.94f, 0.98f, 1.25f, 1.05f },
            { 0.68f, 0.82f, 1.35f, 1.18f },
            wetness);
    case SurfaceMaterial::PaintedLine:
        return blendSurfaceProfile(
            { 0.82f, 0.88f, 1.05f, 1.10f },
            { 0.55f, 0.70f, 1.10f, 1.25f },
            wetness);
    default:
        return legacySurfaceProfile(fallback);
    }
}

inline float selectedGearRatio(
    const PowertrainDescription& powertrain,
    int gear)
{
    if (gear < 0)
        return powertrain.reverseGearRatio;
    if (gear == 0)
        return 0.0f;
    const std::size_t index = static_cast<std::size_t>(gear - 1);
    return index < powertrain.forwardGearRatios.size()
        ? powertrain.forwardGearRatios[index]
        : 0.0f;
}

inline float engineTorqueCurveFactor(
    float engineRpm,
    float idleRpm,
    float redlineRpm)
{
    const float normalizedRpm = std::clamp(
        (engineRpm - idleRpm) / std::max(redlineRpm - idleRpm, 1.0f),
        0.0f,
        1.0f);
    constexpr float peakLocation = 0.55f;
    if (normalizedRpm <= peakLocation)
    {
        return 0.64f + 0.36f * (normalizedRpm / peakLocation);
    }
    const float falling = (normalizedRpm - peakLocation)
        / (1.0f - peakLocation);
    return 1.0f - 0.42f * std::pow(falling, 1.25f);
}

} // namespace heritage::vehicles::vehicle_system_detail
