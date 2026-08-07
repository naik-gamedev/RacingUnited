#include "../Physics/CollisionSystem.hpp"
#include "../Physics/RigidBodySystem.hpp"
#include "../Vehicles/VehicleSystem.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

using heritage::math::Vec3;
using heritage::physics::BodyHandle;
using heritage::physics::BodyMotionType;
using heritage::physics::ColliderHandle;
using heritage::physics::CollisionSystem;
using heritage::physics::RigidBodyDescription;
using heritage::physics::RigidBodyPose;
using heritage::physics::RigidBodySystem;
using heritage::physics::StaticSceneTriangle;
using heritage::vehicles::VehicleDescription;
using heritage::vehicles::VehicleHandle;
using heritage::vehicles::VehicleRestState;
using heritage::vehicles::VehicleSystem;
using heritage::vehicles::WheelDescription;
using heritage::vehicles::WheelState;

constexpr float kWorldDeltaTime = 1.0f / 120.0f;
constexpr Vec3 kGravity{ 0.0f, -9.81f, 0.0f };

struct PrototypeWorld
{
    RigidBodySystem bodies;
    CollisionSystem collisions;
    VehicleSystem vehicles;
    BodyHandle floor = heritage::physics::InvalidBody;
    ColliderHandle floorCollider = heritage::physics::InvalidCollider;
    BodyHandle chassis = heritage::physics::InvalidBody;
    ColliderHandle chassisCollider = heritage::physics::InvalidCollider;
    VehicleHandle vehicle = heritage::vehicles::InvalidVehicle;
};

struct StabilitySample
{
    Vec3 startPosition{};
    Vec3 endPosition{};
    float maximumHorizontalSpeed = 0.0f;
    float maximumVerticalSpeed = 0.0f;
    float maximumAngularSpeedDegrees = 0.0f;
    float verticalPositionSpan = 0.0f;
    float maximumSuspensionVelocity = 0.0f;
    float maximumWheelAngularSpeed = 0.0f;
    std::size_t minimumGroundedWheels = 4;
    bool sleepingAtEnd = false;
};

float magnitude(const Vec3& value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

float horizontalMagnitude(const Vec3& value)
{
    return std::sqrt(value.x * value.x + value.z * value.z);
}

bool addPrototypeWheel(
    PrototypeWorld& world,
    float x,
    float z,
    float driveFactor,
    float steerFactor,
    float serviceBrakeFactor,
    float handbrakeFactor)
{
    WheelDescription wheel;
    wheel.localMount = { x, 0.85f, z };
    wheel.localSuspensionDirection = { 0.0f, -1.0f, 0.0f };
    wheel.radius = 0.2979f;
    wheel.restLength = 0.55f;
    wheel.maximumCompression = 0.20f;
    wheel.maximumDroop = 0.15f;
    wheel.springRate = 35000.0f;
    wheel.bumpDamping = 3200.0f;
    wheel.reboundDamping = 4200.0f;
    wheel.driveFactor = driveFactor;
    wheel.steerFactor = steerFactor;
    wheel.brakeFactor = serviceBrakeFactor;
    wheel.handbrakeFactor = handbrakeFactor;
    return world.vehicles.addWheel(world.vehicle, wheel);
}

bool createPrototypeWorld(PrototypeWorld& world, float highRateHertz)
{
    RigidBodyDescription floorDescription;
    floorDescription.motionType = BodyMotionType::Static;
    floorDescription.position = { 0.0f, -0.5f, 0.0f };
    floorDescription.mass = 1.0f;
    floorDescription.gravityFactor = 0.0f;
    world.floor = world.bodies.create(floorDescription);
    world.floorCollider = world.collisions.createBox(
        world.floor,
        { 50.0f, 0.5f, 50.0f },
        {},
        1.0f,
        0.0f,
        false,
        world.bodies);

    RigidBodyDescription chassisDescription;
    chassisDescription.motionType = BodyMotionType::Dynamic;
    chassisDescription.position = { 0.0f, 0.05f, 0.0f };
    chassisDescription.mass = 1100.0f;
    chassisDescription.gravityFactor = 1.0f;
    chassisDescription.linearDamping = 0.015f;
    chassisDescription.angularDamping = 0.18f;
    world.chassis = world.bodies.create(chassisDescription);
    world.chassisCollider = world.collisions.createBox(
        world.chassis,
        { 1.08f, 0.36f, 1.72f },
        { 0.0f, 0.82f, 0.0f },
        0.35f,
        0.05f,
        false,
        world.bodies);

    VehicleDescription vehicleDescription;
    vehicleDescription.chassisBody = world.chassis;
    vehicleDescription.highRateHertz = highRateHertz;
    vehicleDescription.maximumDriveForce = 7000.0f;
    vehicleDescription.maximumBrakeForce = 12000.0f;
    vehicleDescription.maximumSteerAngleDegrees = 38.0f;
    vehicleDescription.tireFriction = 1.15f;
    vehicleDescription.lateralStiffness = 11000.0f;
    vehicleDescription.rollingResistance = 90.0f;
    world.vehicle = world.vehicles.create(vehicleDescription, world.bodies);

    const bool wheelsCreated =
        addPrototypeWheel(world, -0.7185f, 1.221f, 0.5f, 1.0f, 0.31f, 0.0f)
        && addPrototypeWheel(world, 0.7185f, 1.221f, 0.5f, 1.0f, 0.31f, 0.0f)
        && addPrototypeWheel(world, -0.7140f, -1.221f, 0.0f, 0.0f, 0.19f, 0.5f)
        && addPrototypeWheel(world, 0.7140f, -1.221f, 0.0f, 0.0f, 0.19f, 0.5f);

    const bool tireCreated = world.vehicles.setTireModel(
        world.vehicle,
        3300.0f,
        1.16f,
        88000.0f,
        85000.0f,
        0.12f,
        0.33f,
        0.41f,
        1.48f,
        0.080f,
        0.85f,
        1.65f,
        1.30f,
        0.20f,
        0.15f,
        2.00f,
        0.70f);

    const bool rearLeftTireCreated = world.vehicles.setWheelTireModel(
        world.vehicle,
        2,
        3600.0f,
        1.13f,
        94000.0f,
        76000.0f,
        0.12f,
        0.38f,
        0.48f,
        1.64f,
        0.070f,
        0.85f,
        1.65f,
        1.30f,
        0.20f,
        0.15f,
        2.00f,
        0.70f);
    const bool rearRightTireCreated = world.vehicles.setWheelTireModel(
        world.vehicle,
        3,
        3600.0f,
        1.13f,
        94000.0f,
        76000.0f,
        0.12f,
        0.38f,
        0.48f,
        1.64f,
        0.070f,
        0.85f,
        1.65f,
        1.30f,
        0.20f,
        0.15f,
        2.00f,
        0.70f);

    world.bodies.setAllowSleep(world.chassis, false);
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 0.0f);

    return world.floor != heritage::physics::InvalidBody
        && world.floorCollider != heritage::physics::InvalidCollider
        && world.chassis != heritage::physics::InvalidBody
        && world.chassisCollider != heritage::physics::InvalidCollider
        && world.vehicle != heritage::vehicles::InvalidVehicle
        && wheelsCreated
        && tireCreated
        && rearLeftTireCreated
        && rearRightTireCreated;
}

bool replaceFloorWithSlope(PrototypeWorld& world, float slopeDegrees)
{
    if (world.floorCollider != heritage::physics::InvalidCollider)
    {
        if (!world.collisions.destroy(world.floorCollider))
            return false;
        world.floorCollider = heritage::physics::InvalidCollider;
    }

    const float tangent = std::tan(
        slopeDegrees * 3.14159265358979323846f / 180.0f);
    const auto heightAt = [tangent](float z) {
        return -tangent * z;
    };
    const Vec3 normal = {
        0.0f,
        1.0f / std::sqrt(1.0f + tangent * tangent),
        tangent / std::sqrt(1.0f + tangent * tangent)
    };

    StaticSceneTriangle first;
    first.a = { -50.0f, heightAt(-50.0f), -50.0f };
    first.b = { -50.0f, heightAt(50.0f), 50.0f };
    first.c = { 50.0f, heightAt(50.0f), 50.0f };
    first.normal = normal;
    first.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;

    StaticSceneTriangle second;
    second.a = { -50.0f, heightAt(-50.0f), -50.0f };
    second.b = { 50.0f, heightAt(50.0f), 50.0f };
    second.c = { 50.0f, heightAt(-50.0f), -50.0f };
    second.normal = normal;
    second.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;

    world.collisions.setStaticSceneTriangles({ first, second });
    return true;
}

void stepWorld(PrototypeWorld& world, float worldDeltaTime = kWorldDeltaTime)
{
    world.vehicles.simulate(
        world.bodies,
        world.collisions,
        worldDeltaTime,
        kGravity);
    world.bodies.integrate(worldDeltaTime, kGravity);
    world.collisions.simulate(world.bodies, worldDeltaTime);
}

StabilitySample sampleStability(
    PrototypeWorld& world,
    float settleSeconds,
    float measureSeconds,
    float worldDeltaTime = kWorldDeltaTime)
{
    const int settleSteps = static_cast<int>(std::round(
        settleSeconds / worldDeltaTime));
    for (int index = 0; index < settleSteps; ++index)
        stepWorld(world, worldDeltaTime);

    StabilitySample sample;
    RigidBodyPose pose;
    world.bodies.pose(world.chassis, pose);
    sample.startPosition = pose.position;
    float minimumY = pose.position.y;
    float maximumY = pose.position.y;

    const int measureSteps = static_cast<int>(std::round(
        measureSeconds / worldDeltaTime));
    for (int index = 0; index < measureSteps; ++index)
    {
        stepWorld(world, worldDeltaTime);

        Vec3 linearVelocity{};
        Vec3 angularVelocityDegrees{};
        world.bodies.pose(world.chassis, pose);
        world.bodies.linearVelocity(world.chassis, linearVelocity);
        world.bodies.angularVelocityDegrees(
            world.chassis,
            angularVelocityDegrees);

        sample.maximumHorizontalSpeed = std::max(
            sample.maximumHorizontalSpeed,
            horizontalMagnitude(linearVelocity));
        sample.maximumVerticalSpeed = std::max(
            sample.maximumVerticalSpeed,
            std::abs(linearVelocity.y));
        sample.maximumAngularSpeedDegrees = std::max(
            sample.maximumAngularSpeedDegrees,
            magnitude(angularVelocityDegrees));
        minimumY = std::min(minimumY, pose.position.y);
        maximumY = std::max(maximumY, pose.position.y);
        sample.minimumGroundedWheels = std::min(
            sample.minimumGroundedWheels,
            world.vehicles.groundedWheelCount(world.vehicle));

        for (std::size_t wheelIndex = 0; wheelIndex < 4; ++wheelIndex)
        {
            WheelState state;
            if (world.vehicles.wheelState(
                    world.vehicle,
                    wheelIndex,
                    state))
            {
                sample.maximumSuspensionVelocity = std::max(
                    sample.maximumSuspensionVelocity,
                    std::abs(state.compressionVelocity));
                sample.maximumWheelAngularSpeed = std::max(
                    sample.maximumWheelAngularSpeed,
                    std::abs(state.wheelAngularVelocity));
            }
        }
    }

    world.bodies.pose(world.chassis, pose);
    sample.endPosition = pose.position;
    sample.verticalPositionSpan = maximumY - minimumY;
    world.bodies.sleeping(world.chassis, sample.sleepingAtEnd);
    return sample;
}

void printSample(const std::string& name, const StabilitySample& sample)
{
    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    std::cout
        << name
        << " horizontal_drift_m=" << horizontalMagnitude(displacement)
        << " vertical_drift_m=" << displacement.y
        << " max_horizontal_speed_mps=" << sample.maximumHorizontalSpeed
        << " max_vertical_speed_mps=" << sample.maximumVerticalSpeed
        << " max_angular_speed_degps=" << sample.maximumAngularSpeedDegrees
        << " vertical_span_m=" << sample.verticalPositionSpan
        << " max_suspension_speed_mps=" << sample.maximumSuspensionVelocity
        << " max_wheel_speed_radps=" << sample.maximumWheelAngularSpeed
        << " min_grounded_wheels=" << sample.minimumGroundedWheels
        << " sleeping=" << (sample.sleepingAtEnd ? "true" : "false")
        << '\n';
}

void printWheelStates(const PrototypeWorld& world, const std::string& name)
{
    for (std::size_t wheelIndex = 0; wheelIndex < 4; ++wheelIndex)
    {
        WheelState state;
        if (!world.vehicles.wheelState(world.vehicle, wheelIndex, state))
            continue;
        std::cout
            << name << " wheel=" << wheelIndex
            << " grounded=" << (state.grounded ? "true" : "false")
            << " normal_force_n=" << state.normalForce
            << " mu=" << state.effectiveFriction
            << " brake_torque_nm=" << state.appliedBrakeTorque
            << " wheel_speed_radps=" << state.wheelAngularVelocity
            << " longitudinal_speed_mps=" << state.longitudinalSpeed
            << " lateral_speed_mps=" << state.lateralSpeed
            << '\n';
    }
}

bool parkedVehicleStaysQuiet()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the 1000 Hz prototype world.\n";
        return false;
    }

    const StabilitySample sample = sampleStability(world, 4.0f, 3.0f);
    printSample("parked_1000hz", sample);

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

bool flatRestSleepsAndThrottleWakes()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the flat rest/wake world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    const StabilitySample sample = sampleStability(world, 3.0f, 1.0f);
    printSample("flat_rest", sample);

    world.vehicles.setInputs(world.vehicle, 0.35f, 0.0f, 0.0f, 0.0f);
    stepWorld(world);
    bool sleepingAfterThrottle = true;
    world.bodies.sleeping(world.chassis, sleepingAfterThrottle);
    std::cout << "flat_rest throttle_woke_body="
        << (!sleepingAfterThrottle ? "true" : "false") << '\n';

    return sample.sleepingAtEnd && !sleepingAfterThrottle;
}

bool highRateSuspensionAgreesWithNativeRate()
{
    PrototypeWorld nativeRate;
    PrototypeWorld highRateInsideWorldRate;
    if (!createPrototypeWorld(nativeRate, 1000.0f)
        || !createPrototypeWorld(highRateInsideWorldRate, 1000.0f))
    {
        std::cerr << "Could not create the rate-comparison worlds.\n";
        return false;
    }

    const StabilitySample nativeRateSample = sampleStability(
        nativeRate,
        0.0f,
        2.0f,
        1.0f / 1000.0f);
    const StabilitySample highRateInsideWorldRateSample = sampleStability(
        highRateInsideWorldRate,
        0.0f,
        2.0f);
    printSample("drop_native_1000hz_world", nativeRateSample);
    printSample("drop_1000hz_inside_120hz_world", highRateInsideWorldRateSample);

    const float finalHeightDifference = std::abs(
        nativeRateSample.endPosition.y
            - highRateInsideWorldRateSample.endPosition.y);
    const float verticalSpeedDifference = std::abs(
        nativeRateSample.maximumVerticalSpeed
            - highRateInsideWorldRateSample.maximumVerticalSpeed);
    std::cout
        << "rate_comparison final_height_difference_m="
        << finalHeightDifference
        << " peak_vertical_speed_difference_mps="
        << verticalSpeedDifference
        << '\n';

    return finalHeightDifference <= 0.020f
        && verticalSpeedDifference <= 0.20f;
}

bool parkingBrakeHoldsOnSlope()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !replaceFloorWithSlope(world, 5.0f))
    {
        std::cerr << "Could not create the parking-brake slope world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 1.0f);
    const StabilitySample sample = sampleStability(world, 5.0f, 3.0f);
    printSample("parked_5deg_handbrake", sample);
    printWheelStates(world, "parked_5deg_handbrake");
    VehicleRestState restState;
    if (world.vehicles.restState(world.vehicle, restState))
    {
        std::cout
            << "parked_5deg_handbrake rest_candidate="
            << (restState.candidate ? "true" : "false")
            << " quiet_time_s=" << restState.quietTimeSeconds
            << " required_hold_n=" << restState.requiredHoldForce
            << " available_brake_hold_n="
            << restState.availableBrakeHoldForce
            << '\n';
    }

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    const bool held = horizontalMagnitude(displacement) <= 0.020f
        && sample.maximumHorizontalSpeed <= 0.030f
        && sample.sleepingAtEnd;

    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 0.0f);
    stepWorld(world);
    bool sleepingAfterRelease = true;
    world.bodies.sleeping(world.chassis, sleepingAfterRelease);
    std::cout << "parked_5deg_handbrake release_woke_body="
        << (!sleepingAfterRelease ? "true" : "false") << '\n';
    return held && !sleepingAfterRelease;
}

bool unbrakedVehicleRollsOnSlope()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f)
        || !replaceFloorWithSlope(world, 5.0f))
    {
        std::cerr << "Could not create the unbraked slope world.\n";
        return false;
    }

    world.bodies.setAllowSleep(world.chassis, true);
    world.vehicles.setInputs(world.vehicle, 0.0f, 0.0f, 0.0f, 0.0f);
    const StabilitySample sample = sampleStability(world, 2.0f, 2.0f);
    printSample("unbraked_5deg", sample);

    const Vec3 displacement{
        sample.endPosition.x - sample.startPosition.x,
        sample.endPosition.y - sample.startPosition.y,
        sample.endPosition.z - sample.startPosition.z
    };
    return horizontalMagnitude(displacement) >= 0.050f
        && !sample.sleepingAtEnd;
}

} // namespace

int main()
{
    std::cout << std::fixed << std::setprecision(6);

    int failed = 0;
    const bool parkedPassed = parkedVehicleStaysQuiet();
    std::cout << (parkedPassed ? "PASS" : "FAIL")
        << " parked vehicle stability\n";
    failed += parkedPassed ? 0 : 1;

    const bool restWakePassed = flatRestSleepsAndThrottleWakes();
    std::cout << (restWakePassed ? "PASS" : "FAIL")
        << " flat parked sleep and throttle wake\n";
    failed += restWakePassed ? 0 : 1;

    const bool ratePassed = highRateSuspensionAgreesWithNativeRate();
    std::cout << (ratePassed ? "PASS" : "FAIL")
        << " high-rate suspension timing\n";
    failed += ratePassed ? 0 : 1;

    const bool parkingBrakePassed = parkingBrakeHoldsOnSlope();
    std::cout << (parkingBrakePassed ? "PASS" : "FAIL")
        << " parking brake slope hold\n";
    failed += parkingBrakePassed ? 0 : 1;

    const bool unbrakedSlopePassed = unbrakedVehicleRollsOnSlope();
    std::cout << (unbrakedSlopePassed ? "PASS" : "FAIL")
        << " unbraked slope roll\n";
    failed += unbrakedSlopePassed ? 0 : 1;

    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
        << " count=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
