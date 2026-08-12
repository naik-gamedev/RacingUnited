#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/Dynamics/ChassisFlex/ChassisFlexDiagnostics.hpp"
#include "../Vehicles/Dynamics/ChassisFlex/ChassisFlexEstimator.hpp"
#include "../Vehicles/Dynamics/ChassisFlex/ChassisTorsionalCompliance.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace heritage::tests {
namespace {

constexpr heritage::vehicles::VehicleScalar kDegreesPerRadian =
    180.0 / 3.141592653589793238462643383279502884;

heritage::vehicles::ChassisTorsionalComplianceDescription
peugeotLikeFlexDescription()
{
    heritage::vehicles::ChassisTorsionalComplianceDescription description;
    description.enabled = true;
    description.torsionalRigidityNmPerDegree = 8700.0;
    description.torsionalDampingNmsPerRad = 11300.0;
    description.effectiveTorsionalInertiaKgM2 = 525.0;
    description.torsionAxisLocalY = 0.364;
    description.frontReferenceLocalZ = 1.221;
    description.rearReferenceLocalZ = -1.221;
    description.maximumTwistDegrees = 1.25;
    return description;
}

} // namespace

bool chassisFlexEstimatorProducesBoundedEpistemicEstimate()
{
    heritage::vehicles::ChassisFlexEstimateInput closedInput;
    closedInput.massKg = 1100.0;
    closedInput.wheelbaseM = 2.442;
    closedInput.frontTrackM = 1.437;
    closedInput.rearTrackM = 1.428;
    closedInput.centerOfMassHeightM = 0.52;
    closedInput.modelYear = 2003;
    closedInput.construction =
        heritage::vehicles::ChassisConstructionKind::ClosedUnibody;

    auto openInput = closedInput;
    openInput.construction =
        heritage::vehicles::ChassisConstructionKind::OpenUnibody;
    auto carbonInput = closedInput;
    carbonInput.construction =
        heritage::vehicles::ChassisConstructionKind::CarbonMonocoque;

    const auto closed = heritage::vehicles::estimateChassisFlex(closedInput);
    const auto open = heritage::vehicles::estimateChassisFlex(openInput);
    const auto carbon = heritage::vehicles::estimateChassisFlex(carbonInput);

    std::cout
        << "chassis_flex_estimate closed_nm_per_deg="
        << closed.description.torsionalRigidityNmPerDegree
        << " open_nm_per_deg=" << open.description.torsionalRigidityNmPerDegree
        << " carbon_nm_per_deg="
        << carbon.description.torsionalRigidityNmPerDegree
        << " damping_nms_per_rad="
        << closed.description.torsionalDampingNmsPerRad
        << " inertia_kgm2="
        << closed.description.effectiveTorsionalInertiaKgM2
        << " confidence=" << closed.confidence
        << '\n';

    return closed.valid && open.valid && carbon.valid
        && closed.provenance == "estimated_chassis_flex_closed_unibody_v1"
        && closed.confidence > 0.0 && closed.confidence < 0.30
        && open.description.torsionalRigidityNmPerDegree
            < closed.description.torsionalRigidityNmPerDegree
        && carbon.description.torsionalRigidityNmPerDegree
            > closed.description.torsionalRigidityNmPerDegree
        && std::abs(closed.description.frontReferenceLocalZ - 1.221)
            <= 0.000001
        && std::abs(closed.description.rearReferenceLocalZ + 1.221)
            <= 0.000001
        && std::abs(closed.description.torsionAxisLocalY - 0.364)
            <= 0.000001;
}

bool chassisTorsionalComplianceRespondsToDiagonalLoad()
{
    using heritage::vehicles::ChassisTorsionalComplianceState;
    const auto description = peugeotLikeFlexDescription();

    // Equal front/rear roll reaction is gross rigid-body roll, not structural
    // torsion. The modal flex state should remain exactly neutral.
    ChassisTorsionalComplianceState symmetricState;
    for (int index = 0; index < 1000; ++index)
    {
        heritage::vehicles::integrateChassisTorsionalCompliance(
            description,
            1800.0,
            1800.0,
            0.001,
            symmetricState);
    }

    // A front/rear reaction mismatch represents diagonal/torsional loading.
    ChassisTorsionalComplianceState diagonalState;
    heritage::vehicles::VehicleScalar maximumTwistDegrees = 0.0;
    for (int index = 0; index < 1000; ++index)
    {
        heritage::vehicles::integrateChassisTorsionalCompliance(
            description,
            2600.0,
            -600.0,
            0.001,
            diagonalState);
        maximumTwistDegrees = std::max(
            maximumTwistDegrees,
            std::abs(diagonalState.twistRadians * kDegreesPerRadian));
    }

    ChassisTorsionalComplianceState mirroredState;
    for (int index = 0; index < 1000; ++index)
    {
        heritage::vehicles::integrateChassisTorsionalCompliance(
            description,
            -2600.0,
            600.0,
            0.001,
            mirroredState);
    }

    const auto diagnostics = heritage::vehicles::evaluateChassisFlexDiagnostics(
        description,
        diagonalState);
    const Vec3 referencePickup{ 0.72f, 0.85f, 1.221f };
    const auto frontSectionRadians = heritage::vehicles::chassisSectionTwistRadians(
        description, diagonalState, description.frontReferenceLocalZ);
    const Vec3 flexedPickup = heritage::vehicles::applyChassisSectionTwistToPoint(
        referencePickup,
        description.torsionAxisLocalY,
        frontSectionRadians);
    const float pickupDisplacementMm = magnitude({
        flexedPickup.x - referencePickup.x,
        flexedPickup.y - referencePickup.y,
        flexedPickup.z - referencePickup.z
    }) * 1000.0f;

    // Remove the drive torque and verify damping/stiffness pull the structure
    // back toward its undeformed state rather than preserving a fake offset.
    const auto twistBeforeRelease = std::abs(diagonalState.twistRadians);
    for (int index = 0; index < 1800; ++index)
    {
        heritage::vehicles::integrateChassisTorsionalCompliance(
            description,
            0.0,
            0.0,
            0.001,
            diagonalState);
    }
    const auto twistAfterRelease = std::abs(diagonalState.twistRadians);

    std::cout
        << "chassis_flex_core symmetric_twist_deg="
        << symmetricState.twistRadians * kDegreesPerRadian
        << " peak_diagonal_twist_deg=" << maximumTwistDegrees
        << " settled_diagonal_twist_deg=" << diagnostics.twistDegrees
        << " mirrored_twist_deg="
        << mirroredState.twistRadians * kDegreesPerRadian
        << " front_section_deg=" << diagnostics.frontSectionTwistDegrees
        << " rear_section_deg=" << diagnostics.rearSectionTwistDegrees
        << " front_pickup_delta_mm=" << pickupDisplacementMm
        << " released_twist_deg="
        << diagonalState.twistRadians * kDegreesPerRadian
        << '\n';

    return std::abs(symmetricState.twistRadians) <= 0.000000001
        && maximumTwistDegrees >= 0.002
        && maximumTwistDegrees <= 0.50
        && diagnostics.twistDegrees > 0.0
        && mirroredState.twistRadians < 0.0
        && std::abs(
            diagnostics.frontSectionTwistDegrees
            + diagnostics.rearSectionTwistDegrees) <= 0.000001
        && pickupDisplacementMm >= 0.10f
        && pickupDisplacementMm <= 10.0f
        && twistAfterRelease < twistBeforeRelease * 0.05
        && !diagonalState.saturated;
}

bool chassisFlexIntegratesWithHighRateVehicleDynamics()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !world.bodies.setCenterOfMassLocal(
            world.chassis,
            { 0.0f, 0.52f, 0.20f }))
    {
        std::cerr << "Could not create the chassis-flex prototype world.\n";
        return false;
    }

    const auto description = peugeotLikeFlexDescription();
    if (!world.vehicles.setChassisTorsionalCompliance(
            world.vehicle,
            description))
    {
        return false;
    }

    // Add realistic left/right coupling while leaving chassis flex as its own
    // independent structural mechanism.
    heritage::vehicles::SuspensionAntiRollBarDescription frontBar;
    frontBar.leftWheelIndex = 0;
    frontBar.rightWheelIndex = 1;
    frontBar.torsionalStiffnessNmPerRad = 520.0;
    frontBar.torsionalDampingNmsPerRad = 18.0;
    frontBar.leftLeverArmM = 0.20;
    frontBar.rightLeverArmM = 0.20;
    frontBar.leftLinkMotionRatio = 1.0;
    frontBar.rightLinkMotionRatio = 1.0;
    frontBar.maximumWheelForceN = 7000.0;
    if (!world.vehicles.setAntiRollBar(world.vehicle, 0, frontBar))
        return false;
    auto rearBar = frontBar;
    rearBar.leftWheelIndex = 2;
    rearBar.rightWheelIndex = 3;
    rearBar.torsionalStiffnessNmPerRad = 380.0;
    rearBar.torsionalDampingNmsPerRad = 14.0;
    rearBar.maximumWheelForceN = 6000.0;
    if (!world.vehicles.setAntiRollBar(world.vehicle, 1, rearBar))
        return false;

    // Settle with the parking brake, then perform a bounded turn-and-brake
    // manoeuvre that develops different front/rear roll reactions.
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    for (int index = 0; index < 360; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.75f, 0.0f, 0.20f, 0.0f);
    for (int index = 0; index < 500; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.0f, 0.60f, 0.38f, 0.0f);
    heritage::vehicles::VehicleScalar maximumTwistDegrees = 0.0;
    heritage::vehicles::VehicleScalar maximumDriveTorqueNm = 0.0;
    heritage::vehicles::VehicleScalar maximumSectionDifferenceDegrees = 0.0;
    float maximumHorizontalSpeedMps = 0.0f;
    float maximumAngularSpeedDegreesPerSecond = 0.0f;
    std::size_t minimumGroundedWheels = 4;
    for (int index = 0; index < 320; ++index)
    {
        stepWorld(world);

        Vec3 linearVelocity{};
        Vec3 angularVelocityDegrees{};
        if (!world.bodies.linearVelocity(world.chassis, linearVelocity)
            || !world.bodies.angularVelocityDegrees(
                world.chassis, angularVelocityDegrees))
        {
            return false;
        }
        maximumHorizontalSpeedMps = std::max(
            maximumHorizontalSpeedMps,
            horizontalMagnitude(linearVelocity));
        maximumAngularSpeedDegreesPerSecond = std::max(
            maximumAngularSpeedDegreesPerSecond,
            magnitude(angularVelocityDegrees));

        heritage::vehicles::ChassisTorsionalComplianceDescription currentDescription;
        heritage::vehicles::ChassisTorsionalComplianceState state;
        if (!world.vehicles.chassisTorsionalCompliance(
                world.vehicle,
                currentDescription,
                state))
        {
            return false;
        }
        const auto diagnostics = heritage::vehicles::evaluateChassisFlexDiagnostics(
            currentDescription,
            state);
        maximumTwistDegrees = std::max(
            maximumTwistDegrees,
            std::abs(diagnostics.twistDegrees));
        maximumDriveTorqueNm = std::max(
            maximumDriveTorqueNm,
            std::abs(state.driveTorqueNm));
        maximumSectionDifferenceDegrees = std::max(
            maximumSectionDifferenceDegrees,
            std::abs(
                diagnostics.frontSectionTwistDegrees
                - diagnostics.rearSectionTwistDegrees));
        minimumGroundedWheels = std::min(
            minimumGroundedWheels,
            world.vehicles.groundedWheelCount(world.vehicle));
    }

    std::cout
        << "chassis_flex_vehicle max_twist_deg=" << maximumTwistDegrees
        << " max_drive_torque_nm=" << maximumDriveTorqueNm
        << " max_section_difference_deg=" << maximumSectionDifferenceDegrees
        << " max_horizontal_speed_mps=" << maximumHorizontalSpeedMps
        << " max_angular_speed_degps="
        << maximumAngularSpeedDegreesPerSecond
        << " min_grounded_wheels=" << minimumGroundedWheels
        << '\n';

    // Real road-car shell twist should be subtle, finite and independent of
    // the much larger gross rigid-body pitch/roll/yaw motion.
    return maximumDriveTorqueNm >= 50.0
        && maximumTwistDegrees >= 0.0005
        && maximumTwistDegrees <= 0.50
        && maximumSectionDifferenceDegrees >= 0.0005
        && maximumSectionDifferenceDegrees <= 0.50
        && maximumHorizontalSpeedMps <= 80.0f
        && maximumAngularSpeedDegreesPerSecond <= 250.0f
        && minimumGroundedWheels >= 3;
}

} // namespace heritage::tests
