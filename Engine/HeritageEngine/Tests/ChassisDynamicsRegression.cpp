#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/Suspension/Authoring/MacPhersonHardpointEstimator.hpp"
#include "../Vehicles/Suspension/Authoring/TrailingArmHardpointEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace heritage::tests {

namespace {

struct TestQuaternion
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

TestQuaternion quaternionFromEulerDegreesForTest(const Vec3& value)
{
    constexpr float kPi = 3.14159265358979323846f;
    const float halfX = value.x * kPi / 180.0f * 0.5f;
    const float halfY = value.y * kPi / 180.0f * 0.5f;
    const float halfZ = value.z * kPi / 180.0f * 0.5f;
    const float cx = std::cos(halfX);
    const float sx = std::sin(halfX);
    const float cy = std::cos(halfY);
    const float sy = std::sin(halfY);
    const float cz = std::cos(halfZ);
    const float sz = std::sin(halfZ);
    return {
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    };
}

Vec3 rotateVectorForTest(const TestQuaternion& rotation, const Vec3& value)
{
    const Vec3 qv{ rotation.x, rotation.y, rotation.z };
    const auto cross = [](const Vec3& a, const Vec3& b) {
        return Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    };
    const Vec3 firstRaw = cross(qv, value);
    const Vec3 first{
        firstRaw.x * 2.0f,
        firstRaw.y * 2.0f,
        firstRaw.z * 2.0f
    };
    const Vec3 second = cross(qv, first);
    return {
        value.x + first.x * rotation.w + second.x,
        value.y + first.y * rotation.w + second.y,
        value.z + first.z * rotation.w + second.z
    };
}

Vec3 bodyLocalAngularVelocityDegreesForTest(
    const RigidBodyPose& pose,
    const Vec3& worldAngularVelocityDegrees)
{
    const TestQuaternion rotation =
        quaternionFromEulerDegreesForTest(pose.rotationDegrees);
    const Vec3 bodyRight = rotateVectorForTest(rotation, { 1.0f, 0.0f, 0.0f });
    const Vec3 bodyUp = rotateVectorForTest(rotation, { 0.0f, 1.0f, 0.0f });
    const Vec3 bodyForward = rotateVectorForTest(rotation, { 0.0f, 0.0f, 1.0f });
    const auto dot = [](const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    return {
        dot(worldAngularVelocityDegrees, bodyRight),
        dot(worldAngularVelocityDegrees, bodyUp),
        dot(worldAngularVelocityDegrees, bodyForward)
    };
}

} // namespace

bool rigidBodyCenterOfMassOffsetGeneratesTorque()
{
    RigidBodySystem bodies;
    CollisionSystem collisions;

    RigidBodyDescription description;
    description.motionType = BodyMotionType::Dynamic;
    description.position = { 0.0f, 0.0f, 0.0f };
    description.mass = 1100.0f;
    description.gravityFactor = 0.0f;
    description.linearDamping = 0.0f;
    description.angularDamping = 0.0f;
    const BodyHandle body = bodies.create(description);
    const ColliderHandle collider = collisions.createBox(
        body,
        { 1.08f, 0.36f, 1.72f },
        { 0.0f, 0.82f, 0.0f },
        0.35f,
        0.0f,
        false,
        bodies);
    if (body == heritage::physics::InvalidBody
        || collider == heritage::physics::InvalidCollider
        || !bodies.setCenterOfMassLocal(body, { 0.0f, 0.52f, 0.20f }))
    {
        return false;
    }

    // Rebuild collider-derived inertia around the configured physical COM.
    collisions.simulate(bodies, kWorldDeltaTime);

    Vec3 localCom{};
    Vec3 worldCom{};
    if (!bodies.centerOfMassLocal(body, localCom)
        || !bodies.centerOfMassWorld(body, worldCom))
    {
        return false;
    }

    // A lateral impulse at road/reference-origin height must create roll torque
    // because the physical COM sits above it. The same body-origin coordinates
    // remain untouched; only the mass properties differ.
    const Vec3 roadImpulse{ 1000.0f, 0.0f, 0.0f };
    if (!bodies.applyImpulseAtPoint(body, roadImpulse, { 0.0f, 0.0f, 0.0f }))
        return false;

    Vec3 angularVelocityDegrees{};
    if (!bodies.angularVelocityDegrees(body, angularVelocityDegrees))
        return false;

    // With gravity and linear velocity disabled, integrating the new angular
    // velocity should rotate the authored origin around a stationary COM.
    bodies.setLinearVelocity(body, {});
    Vec3 centerBefore{};
    bodies.centerOfMassWorld(body, centerBefore);
    bodies.integrate(kWorldDeltaTime, {});
    Vec3 centerAfter{};
    RigidBodyPose poseAfter{};
    bodies.centerOfMassWorld(body, centerAfter);
    bodies.pose(body, poseAfter);

    const float centerDrift = magnitude({
        centerAfter.x - centerBefore.x,
        centerAfter.y - centerBefore.y,
        centerAfter.z - centerBefore.z
    });
    const float originMotion = magnitude(poseAfter.position);

    std::cout
        << "body_com local=" << localCom.x << ',' << localCom.y << ',' << localCom.z
        << " world=" << worldCom.x << ',' << worldCom.y << ',' << worldCom.z
        << " roll_rate_degps=" << angularVelocityDegrees.z
        << " center_drift_m=" << centerDrift
        << " origin_motion_m=" << originMotion
        << '\n';

    return std::abs(localCom.y - 0.52f) <= 0.000001f
        && std::abs(localCom.z - 0.20f) <= 0.000001f
        && std::abs(angularVelocityDegrees.z) > 5.0f
        && centerDrift <= 0.00001f
        && originMotion > 0.0001f;
}

bool vehicleChassisRollRespondsToCornering()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !world.bodies.setCenterOfMassLocal(
            world.chassis,
            { 0.0f, 0.52f, 0.20f }))
    {
        std::cerr << "Could not create the chassis-roll prototype world.\n";
        return false;
    }

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

    // Settle without free-wheel creep, then release the parking brake and ask
    // the real tire/suspension loop for a moderate accelerating turn.
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    for (int index = 0; index < 360; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.55f, 0.0f, 0.25f, 0.0f);
    float maximumAbsoluteRollDegrees = 0.0f;
    float maximumAbsoluteLocalRollRate = 0.0f;
    float maximumLeftRightLoadDifference = 0.0f;
    float maximumSpeed = 0.0f;
    std::size_t minimumGroundedWheels = 4;
    for (int index = 0; index < 300; ++index)
    {
        stepWorld(world);

        RigidBodyPose pose{};
        Vec3 linearVelocity{};
        Vec3 angularVelocityDegrees{};
        world.bodies.pose(world.chassis, pose);
        world.bodies.linearVelocity(world.chassis, linearVelocity);
        world.bodies.angularVelocityDegrees(world.chassis, angularVelocityDegrees);

        // During this bounded low/moderate-speed test the Z Euler component is
        // a stable roll-angle diagnostic; Dynamics Lab separately records the
        // true body-local roll rate around the chassis forward axis.
        maximumAbsoluteRollDegrees = std::max(
            maximumAbsoluteRollDegrees,
            std::abs(pose.rotationDegrees.z));
        maximumSpeed = std::max(maximumSpeed, magnitude(linearVelocity));
        minimumGroundedWheels = std::min(
            minimumGroundedWheels,
            world.vehicles.groundedWheelCount(world.vehicle));

        const float yawRadians = pose.rotationDegrees.y
            * 3.14159265358979323846f / 180.0f;
        const Vec3 chassisForward{
            std::sin(yawRadians),
            0.0f,
            std::cos(yawRadians)
        };
        const float localRollRate =
            angularVelocityDegrees.x * chassisForward.x
            + angularVelocityDegrees.z * chassisForward.z;
        maximumAbsoluteLocalRollRate = std::max(
            maximumAbsoluteLocalRollRate,
            std::abs(localRollRate));

        WheelState frontLeft{};
        WheelState frontRight{};
        WheelState rearLeft{};
        WheelState rearRight{};
        if (!world.vehicles.wheelState(world.vehicle, 0, frontLeft)
            || !world.vehicles.wheelState(world.vehicle, 1, frontRight)
            || !world.vehicles.wheelState(world.vehicle, 2, rearLeft)
            || !world.vehicles.wheelState(world.vehicle, 3, rearRight))
        {
            return false;
        }
        const float leftLoad = static_cast<float>(
            frontLeft.normalForce + rearLeft.normalForce);
        const float rightLoad = static_cast<float>(
            frontRight.normalForce + rearRight.normalForce);
        maximumLeftRightLoadDifference = std::max(
            maximumLeftRightLoadDifference,
            std::abs(leftLoad - rightLoad));
    }

    std::cout
        << "chassis_roll max_roll_deg=" << maximumAbsoluteRollDegrees
        << " max_local_roll_rate_degps=" << maximumAbsoluteLocalRollRate
        << " max_left_right_load_diff_n=" << maximumLeftRightLoadDifference
        << " max_speed_mps=" << maximumSpeed
        << " min_grounded_wheels=" << minimumGroundedWheels
        << '\n';

    // The lower bounds prevent the old road-level-COM "brick" behavior from
    // returning. Upper bounds catch a sign/inertia error that turns modest body
    // roll into an immediate rollover.
    return maximumAbsoluteRollDegrees >= 0.25f
        && maximumAbsoluteRollDegrees <= 8.0f
        && maximumAbsoluteLocalRollRate >= 0.15f
        && maximumAbsoluteLocalRollRate <= 8.0f
        && maximumLeftRightLoadDifference >= 500.0f
        && maximumSpeed >= 2.0f
        && minimumGroundedWheels >= 3;
}


bool vehicleCombinedPitchRollYawRespondsToBrakingTurn()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !world.bodies.setCenterOfMassLocal(
            world.chassis,
            { 0.0f, 0.52f, 0.20f }))
    {
        std::cerr << "Could not create the combined-dynamics prototype world.\n";
        return false;
    }

    constexpr float kReferencePackageScaleM = 0.2979f;
    constexpr float kCasterDegrees = 3.266667f;
    constexpr float kSaiDegrees = 9.70f;
    constexpr float kFrontToeDegrees = 0.116667f;
    constexpr float kRearToeDegrees = 0.266667f;

    // Exercise the same mixed mechanism architecture used by the current
    // Racing United prototype: MacPherson front, trailing-arm/torsion-bar rear.
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

    // Settle, accelerate straight, then brake and steer simultaneously. This
    // forces pitch, roll, yaw and asymmetric four-corner response to coexist.
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    for (int index = 0; index < 360; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.82f, 0.0f, 0.0f, 0.0f);
    for (int index = 0; index < 420; ++index)
        stepWorld(world);

    Vec3 entryVelocity{};
    world.bodies.linearVelocity(world.chassis, entryVelocity);
    const float entrySpeed = magnitude(entryVelocity);

    world.vehicles.setInputs(world.vehicle, 0.0f, 0.58f, 0.42f, 0.0f);

    float maximumPitchDegrees = 0.0f;
    float maximumRollDegrees = 0.0f;
    float maximumYawDegrees = 0.0f;
    float maximumPitchRateDegreesPerSecond = 0.0f;
    float maximumRollRateDegreesPerSecond = 0.0f;
    float maximumYawRateDegreesPerSecond = 0.0f;
    float maximumFrontLoadBiasN = 0.0f;
    float maximumLeftRightLoadBiasN = 0.0f;
    float maximumCornerLoadSpreadN = 0.0f;
    float maximumCompressionSpreadM = 0.0f;
    float maximumDamperForceN = 0.0f;
    float maximumAntiRollTorqueNm = 0.0f;
    std::size_t minimumGroundedWheels = 4;

    for (int index = 0; index < 260; ++index)
    {
        stepWorld(world);

        RigidBodyPose pose{};
        Vec3 angularVelocityDegrees{};
        if (!world.bodies.pose(world.chassis, pose)
            || !world.bodies.angularVelocityDegrees(
                world.chassis, angularVelocityDegrees))
        {
            return false;
        }

        maximumPitchDegrees = std::max(
            maximumPitchDegrees, std::abs(pose.rotationDegrees.x));
        maximumRollDegrees = std::max(
            maximumRollDegrees, std::abs(pose.rotationDegrees.z));
        maximumYawDegrees = std::max(
            maximumYawDegrees, std::abs(pose.rotationDegrees.y));
        const Vec3 localAngularVelocityDegrees =
            bodyLocalAngularVelocityDegreesForTest(
                pose, angularVelocityDegrees);
        maximumPitchRateDegreesPerSecond = std::max(
            maximumPitchRateDegreesPerSecond,
            std::abs(localAngularVelocityDegrees.x));
        maximumYawRateDegreesPerSecond = std::max(
            maximumYawRateDegreesPerSecond,
            std::abs(localAngularVelocityDegrees.y));
        maximumRollRateDegreesPerSecond = std::max(
            maximumRollRateDegreesPerSecond,
            std::abs(localAngularVelocityDegrees.z));

        WheelState wheel[4]{};
        for (std::size_t wheelIndex = 0; wheelIndex < 4; ++wheelIndex)
        {
            if (!world.vehicles.wheelState(
                    world.vehicle, wheelIndex, wheel[wheelIndex]))
            {
                return false;
            }
            maximumDamperForceN = std::max(
                maximumDamperForceN,
                static_cast<float>(std::abs(
                    wheel[wheelIndex].suspensionDampingForce)));
        }

        const float frontLoad = static_cast<float>(
            wheel[0].normalForce + wheel[1].normalForce);
        const float rearLoad = static_cast<float>(
            wheel[2].normalForce + wheel[3].normalForce);
        const float leftLoad = static_cast<float>(
            wheel[0].normalForce + wheel[2].normalForce);
        const float rightLoad = static_cast<float>(
            wheel[1].normalForce + wheel[3].normalForce);
        maximumFrontLoadBiasN = std::max(
            maximumFrontLoadBiasN, frontLoad - rearLoad);
        maximumLeftRightLoadBiasN = std::max(
            maximumLeftRightLoadBiasN, std::abs(leftLoad - rightLoad));

        float minimumCornerLoad = static_cast<float>(wheel[0].normalForce);
        float maximumCornerLoad = minimumCornerLoad;
        float minimumCompression = static_cast<float>(wheel[0].compression);
        float maximumCompression = minimumCompression;
        for (std::size_t wheelIndex = 1; wheelIndex < 4; ++wheelIndex)
        {
            minimumCornerLoad = std::min(
                minimumCornerLoad,
                static_cast<float>(wheel[wheelIndex].normalForce));
            maximumCornerLoad = std::max(
                maximumCornerLoad,
                static_cast<float>(wheel[wheelIndex].normalForce));
            minimumCompression = std::min(
                minimumCompression,
                static_cast<float>(wheel[wheelIndex].compression));
            maximumCompression = std::max(
                maximumCompression,
                static_cast<float>(wheel[wheelIndex].compression));
        }
        maximumCornerLoadSpreadN = std::max(
            maximumCornerLoadSpreadN,
            maximumCornerLoad - minimumCornerLoad);
        maximumCompressionSpreadM = std::max(
            maximumCompressionSpreadM,
            maximumCompression - minimumCompression);

        for (std::size_t barIndex = 0;
            barIndex < world.vehicles.antiRollBarCount(world.vehicle);
            ++barIndex)
        {
            heritage::vehicles::SuspensionAntiRollBarDescription description;
            heritage::vehicles::SuspensionAntiRollBarOutput state;
            if (!world.vehicles.antiRollBar(
                    world.vehicle, barIndex, description, state))
            {
                return false;
            }
            maximumAntiRollTorqueNm = std::max(
                maximumAntiRollTorqueNm,
                static_cast<float>(std::abs(state.totalTorqueNm)));
        }

        minimumGroundedWheels = std::min(
            minimumGroundedWheels,
            world.vehicles.groundedWheelCount(world.vehicle));
    }

    std::cout
        << "combined_dynamics entry_speed_mps=" << entrySpeed
        << " max_pitch_deg=" << maximumPitchDegrees
        << " max_roll_deg=" << maximumRollDegrees
        << " max_yaw_deg=" << maximumYawDegrees
        << " max_pitch_rate_degps=" << maximumPitchRateDegreesPerSecond
        << " max_roll_rate_degps=" << maximumRollRateDegreesPerSecond
        << " max_yaw_rate_degps=" << maximumYawRateDegreesPerSecond
        << " max_front_load_bias_n=" << maximumFrontLoadBiasN
        << " max_left_right_load_bias_n=" << maximumLeftRightLoadBiasN
        << " max_corner_load_spread_n=" << maximumCornerLoadSpreadN
        << " max_compression_spread_m=" << maximumCompressionSpreadM
        << " max_damper_force_n=" << maximumDamperForceN
        << " max_arb_torque_nm=" << maximumAntiRollTorqueNm
        << " min_grounded_wheels=" << minimumGroundedWheels
        << '\n';

    // This is a behavior contract, not a Peugeot setup target. The bounds
    // prevent either a return to brick-like response or an unstable blow-up.
    return entrySpeed >= 5.0f
        && maximumPitchDegrees >= 0.15f
        && maximumRollDegrees >= 0.20f
        && maximumYawRateDegreesPerSecond >= 1.0f
        && maximumPitchRateDegreesPerSecond >= 0.15f
        && maximumRollRateDegreesPerSecond >= 0.15f
        && maximumFrontLoadBiasN >= 300.0f
        && maximumLeftRightLoadBiasN >= 300.0f
        && maximumCornerLoadSpreadN >= 600.0f
        && maximumCompressionSpreadM >= 0.005f
        && maximumDamperForceN >= 50.0f
        && maximumAntiRollTorqueNm >= 5.0f
        && minimumGroundedWheels >= 3;
}

} // namespace heritage::tests
