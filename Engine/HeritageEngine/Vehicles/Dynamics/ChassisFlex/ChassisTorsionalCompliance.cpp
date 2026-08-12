#include "ChassisTorsionalCompliance.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles {
namespace {

constexpr VehicleScalar kPi = 3.141592653589793238462643383279502884;
constexpr VehicleScalar kMinimumReferenceSpanM = 0.10;
constexpr VehicleScalar kMinimumTimeStepSeconds = 0.0000001;

bool finite(VehicleScalar value)
{
    return std::isfinite(value);
}

VehicleScalar radians(VehicleScalar degreesValue)
{
    return degreesValue * (kPi / 180.0);
}

} // namespace

bool validChassisTorsionalComplianceDescription(
    const ChassisTorsionalComplianceDescription& description)
{
    return finite(description.torsionalRigidityNmPerDegree)
        && description.torsionalRigidityNmPerDegree > 0.0
        && description.torsionalRigidityNmPerDegree <= 1000000.0
        && finite(description.torsionalDampingNmsPerRad)
        && description.torsionalDampingNmsPerRad >= 0.0
        && description.torsionalDampingNmsPerRad <= 10000000.0
        && finite(description.effectiveTorsionalInertiaKgM2)
        && description.effectiveTorsionalInertiaKgM2 > 0.001
        && description.effectiveTorsionalInertiaKgM2 <= 10000000.0
        && finite(description.torsionAxisLocalY)
        && std::abs(description.torsionAxisLocalY) <= 10.0
        && finite(description.frontReferenceLocalZ)
        && finite(description.rearReferenceLocalZ)
        && description.frontReferenceLocalZ
            - description.rearReferenceLocalZ >= kMinimumReferenceSpanM
        && finite(description.maximumTwistDegrees)
        && description.maximumTwistDegrees > 0.0
        && description.maximumTwistDegrees <= 20.0;
}

void integrateChassisTorsionalCompliance(
    const ChassisTorsionalComplianceDescription& description,
    VehicleScalar frontRollMomentNm,
    VehicleScalar rearRollMomentNm,
    VehicleScalar deltaTimeSeconds,
    ChassisTorsionalComplianceState& state)
{
    if (!description.enabled
        || !validChassisTorsionalComplianceDescription(description)
        || !finite(frontRollMomentNm)
        || !finite(rearRollMomentNm)
        || !finite(deltaTimeSeconds)
        || deltaTimeSeconds <= kMinimumTimeStepSeconds)
    {
        state = {};
        return;
    }

    state.frontRollMomentNm = frontRollMomentNm;
    state.rearRollMomentNm = rearRollMomentNm;

    // First symmetric torsion mode: gross body roll belongs to the rigid body;
    // only the DIFFERENCE between front and rear axle roll reactions drives
    // relative structural twist. Half the moment difference is the connector
    // torque for equalized front/rear modal participation.
    state.driveTorqueNm = 0.5 * (frontRollMomentNm - rearRollMomentNm);

    const VehicleScalar stiffnessNmPerRad =
        description.torsionalRigidityNmPerDegree * (180.0 / kPi);
    state.springTorqueNm = -stiffnessNmPerRad * state.twistRadians;
    state.dampingTorqueNm = -description.torsionalDampingNmsPerRad
        * state.twistRateRadiansPerSecond;
    const VehicleScalar netTorqueNm = state.driveTorqueNm
        + state.springTorqueNm + state.dampingTorqueNm;
    state.angularAccelerationRadiansPerSecondSquared = netTorqueNm
        / description.effectiveTorsionalInertiaKgM2;

    // Semi-implicit integration is stable for the small, damped structural
    // mode at the vehicle high-rate step and avoids an extra allocation/state.
    state.twistRateRadiansPerSecond +=
        state.angularAccelerationRadiansPerSecondSquared * deltaTimeSeconds;
    state.twistRadians += state.twistRateRadiansPerSecond * deltaTimeSeconds;

    const VehicleScalar maximumTwistRadians = radians(
        description.maximumTwistDegrees);
    state.saturated = std::abs(state.twistRadians) > maximumTwistRadians;
    if (state.saturated)
    {
        state.twistRadians = std::clamp(
            state.twistRadians,
            -maximumTwistRadians,
            maximumTwistRadians);
        // Do not let the structural mode continue integrating into a hard stop.
        if ((state.twistRadians > 0.0
                && state.twistRateRadiansPerSecond > 0.0)
            || (state.twistRadians < 0.0
                && state.twistRateRadiansPerSecond < 0.0))
        {
            state.twistRateRadiansPerSecond = 0.0;
        }
    }
}

VehicleScalar chassisSectionTwistRadians(
    const ChassisTorsionalComplianceDescription& description,
    const ChassisTorsionalComplianceState& state,
    VehicleScalar localZ)
{
    if (!description.enabled
        || !validChassisTorsionalComplianceDescription(description)
        || !finite(localZ))
    {
        return 0.0;
    }
    const VehicleScalar span = description.frontReferenceLocalZ
        - description.rearReferenceLocalZ;
    const VehicleScalar midpoint = 0.5 * (
        description.frontReferenceLocalZ
        + description.rearReferenceLocalZ);
    // +/-0.5 gives a total front-to-rear relative angle equal to state.twist.
    const VehicleScalar sectionFactor = std::clamp(
        (localZ - midpoint) / span,
        -0.5,
        0.5);
    return state.twistRadians * sectionFactor;
}

heritage::math::Vec3 applyChassisSectionTwistToPoint(
    const heritage::math::Vec3& localPoint,
    VehicleScalar torsionAxisLocalY,
    VehicleScalar sectionTwistRadians)
{
    const VehicleScalar cosine = std::cos(sectionTwistRadians);
    const VehicleScalar sine = std::sin(sectionTwistRadians);
    const VehicleScalar relativeY = static_cast<VehicleScalar>(localPoint.y)
        - torsionAxisLocalY;
    return {
        static_cast<float>(
            static_cast<VehicleScalar>(localPoint.x) * cosine
            - relativeY * sine),
        static_cast<float>(
            torsionAxisLocalY
            + static_cast<VehicleScalar>(localPoint.x) * sine
            + relativeY * cosine),
        localPoint.z
    };
}

heritage::math::Vec3 applyChassisSectionTwistToVector(
    const heritage::math::Vec3& localVector,
    VehicleScalar sectionTwistRadians)
{
    const VehicleScalar cosine = std::cos(sectionTwistRadians);
    const VehicleScalar sine = std::sin(sectionTwistRadians);
    return {
        static_cast<float>(
            static_cast<VehicleScalar>(localVector.x) * cosine
            - static_cast<VehicleScalar>(localVector.y) * sine),
        static_cast<float>(
            static_cast<VehicleScalar>(localVector.x) * sine
            + static_cast<VehicleScalar>(localVector.y) * cosine),
        localVector.z
    };
}

} // namespace heritage::vehicles
