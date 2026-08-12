#include "MacPhersonHardpointEstimator.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinimumPackageScaleM = 0.20f;
constexpr float kMaximumPackageScaleM = 0.60f;

float radians(float degreesValue)
{
    return degreesValue * (kPi / 180.0f);
}

float lengthSquared(const heritage::math::Vec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const float magnitudeSquared = lengthSquared(value);
    if (magnitudeSquared <= 0.000000000001f)
        return fallback;
    const float inverseMagnitude = 1.0f / std::sqrt(magnitudeSquared);
    return {
        value.x * inverseMagnitude,
        value.y * inverseMagnitude,
        value.z * inverseMagnitude
    };
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

bool finite(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

MacPhersonHardpointEstimateResult estimateMacPhersonHardpointsV1(
    const MacPhersonHardpointEstimateInput& input)
{
    MacPhersonHardpointEstimateResult result;
    result.profileId = "estimated_macpherson_road_v1";
    result.confidence = 0.35f;

    if (!finite(input.wheelCenter)
        || !std::isfinite(input.referencePackageScaleM)
        || !std::isfinite(input.casterDegrees)
        || !std::isfinite(input.steeringAxisInclinationDegrees)
        || input.referencePackageScaleM < kMinimumPackageScaleM
        || input.referencePackageScaleM > kMaximumPackageScaleM
        || std::abs(input.casterDegrees) > 20.0f
        || input.steeringAxisInclinationDegrees < 0.0f
        || input.steeringAxisInclinationDegrees > 25.0f)
    {
        return result;
    }

    const float packageScale = input.referencePackageScaleM;
    const float sideSign = input.wheelCenter.x < 0.0f ? -1.0f : 1.0f;
    const float inboardSign = -sideSign;

    // Steering-axis direction is derived from creator-supplied alignment.
    // Heritage native axes are X right, Y up, Z forward; positive caster places
    // the top of the steering axis rearward (-Z) from the lower ball joint.
    const heritage::math::Vec3 steeringAxis = normalized(
        {
            inboardSign * std::tan(radians(
                input.steeringAxisInclinationDegrees)),
            1.0f,
            -std::tan(radians(input.casterDegrees))
        },
        { 0.0f, 1.0f, 0.0f });

    // These ratios are not Peugeot measurements. They are a deterministic
    // packaging template scaled from an immutable chassis reference package dimension and chosen to produce a
    // conventional compact-road-car MacPherson layout with modest bump steer.
    // Every inferred point carries low confidence and is intended to be
    // replaced progressively by GLB-authored or measured coordinates.
    const heritage::math::Vec3 wheelCenter = input.wheelCenter;
    const heritage::math::Vec3 lowerBallJoint = add(
        wheelCenter,
        {
            sideSign * packageScale * 0.185f,
            -packageScale * 0.150f,
            0.0f
        });

    MacPhersonHardpoints hardpoints;
    hardpoints.authored = true;
    hardpoints.wheelCenter = wheelCenter;
    hardpoints.lowerBallJoint = lowerBallJoint;
    hardpoints.strutTopMount = add(
        lowerBallJoint,
        scale(steeringAxis, packageScale * 2.89f));
    hardpoints.strutUprightMount = add(
        lowerBallJoint,
        scale(steeringAxis, packageScale * 1.00f));

    const heritage::math::Vec3 inboardArmOffset{
        inboardSign * packageScale * 1.44f,
        packageScale * 0.20f,
        0.0f
    };
    const heritage::math::Vec3 lowerArmCentre = add(
        lowerBallJoint,
        inboardArmOffset);
    hardpoints.lowerArmInnerFront = add(
        lowerArmCentre,
        { 0.0f, 0.0f, packageScale * 0.60f });
    hardpoints.lowerArmInnerRear = add(
        lowerArmCentre,
        { 0.0f, 0.0f, -packageScale * 0.60f });

    hardpoints.tieRodOuter = add(
        wheelCenter,
        {
            inboardSign * packageScale * 0.185f,
            packageScale * 0.10f,
            packageScale * 0.34f
        });
    hardpoints.tieRodInner = add(
        wheelCenter,
        {
            inboardSign * packageScale * 1.44f,
            packageScale * 0.235f,
            -packageScale * 0.34f
        });

    if (!validMacPhersonHardpoints(hardpoints))
        return result;

    result.hardpoints = hardpoints;
    result.valid = true;
    return result;
}

} // namespace heritage::vehicles
