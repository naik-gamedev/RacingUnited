#pragma once

#include "../../VehiclePrecision.hpp"
#include "../../../Core/Math/Math.hpp"

namespace heritage::vehicles {

// First structural-compliance mode for a nominally rigid vehicle chassis.
// The main rigid body still owns gross 6-DOF motion. This mechanism models the
// small relative torsional deformation between the front and rear structure so
// diagonal suspension loads can move pickup frames by realistic millimetres
// without requiring a many-body chassis for every car in a large race field.
struct ChassisTorsionalComplianceDescription
{
    bool enabled = false;
    // Creator-facing chassis rigidity is commonly discussed as Nm/degree.
    // Runtime integration converts it to Nm/radian internally.
    VehicleScalar torsionalRigidityNmPerDegree = 10000.0;
    VehicleScalar torsionalDampingNmsPerRad = 12000.0;
    VehicleScalar effectiveTorsionalInertiaKgM2 = 500.0;
    // Local Y coordinate of the virtual longitudinal torsion axis.
    VehicleScalar torsionAxisLocalY = 0.45;
    // Reference longitudinal stations. Relative twist is +half at the front
    // station and -half at the rear station, interpolated continuously between.
    VehicleScalar frontReferenceLocalZ = 1.20;
    VehicleScalar rearReferenceLocalZ = -1.20;
    VehicleScalar maximumTwistDegrees = 1.0;
};

struct ChassisTorsionalComplianceState
{
    VehicleScalar twistRadians = 0.0;
    VehicleScalar twistRateRadiansPerSecond = 0.0;
    VehicleScalar frontRollMomentNm = 0.0;
    VehicleScalar rearRollMomentNm = 0.0;
    VehicleScalar driveTorqueNm = 0.0;
    VehicleScalar springTorqueNm = 0.0;
    VehicleScalar dampingTorqueNm = 0.0;
    VehicleScalar angularAccelerationRadiansPerSecondSquared = 0.0;
    bool saturated = false;
};

bool validChassisTorsionalComplianceDescription(
    const ChassisTorsionalComplianceDescription& description);

void integrateChassisTorsionalCompliance(
    const ChassisTorsionalComplianceDescription& description,
    VehicleScalar frontRollMomentNm,
    VehicleScalar rearRollMomentNm,
    VehicleScalar deltaTimeSeconds,
    ChassisTorsionalComplianceState& state);

// Relative local section rotation around the chassis longitudinal (+Z) axis.
// A point at the front/rear reference stations receives +/- half of the total
// front-to-rear twist. Intermediate structures interpolate continuously.
VehicleScalar chassisSectionTwistRadians(
    const ChassisTorsionalComplianceDescription& description,
    const ChassisTorsionalComplianceState& state,
    VehicleScalar localZ);

heritage::math::Vec3 applyChassisSectionTwistToPoint(
    const heritage::math::Vec3& localPoint,
    VehicleScalar torsionAxisLocalY,
    VehicleScalar sectionTwistRadians);

heritage::math::Vec3 applyChassisSectionTwistToVector(
    const heritage::math::Vec3& localVector,
    VehicleScalar sectionTwistRadians);

} // namespace heritage::vehicles
