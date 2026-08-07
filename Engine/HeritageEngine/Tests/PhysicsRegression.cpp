#include "../Physics/CollisionSystem.hpp"
#include "../Physics/RigidBodySystem.hpp"
#include "../Vehicles/VehicleDefinitionCompiler.hpp"
#include "../Vehicles/VehicleDefinitionLoader.hpp"
#include "../Vehicles/VehicleSystem.hpp"
#include "../Vehicles/SuspensionGeometry.hpp"
#include "../Vehicles/UnsprungMassModel.hpp"

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
using heritage::vehicles::VehicleDefinitionCompiler;
using heritage::vehicles::VehicleDefinitionLoadSettings;
using heritage::vehicles::VehicleDefinitionLoader;
using heritage::vehicles::VehicleDefinitionV2Source;
using heritage::vehicles::VehicleHandle;
using heritage::vehicles::VehicleRestState;
using heritage::vehicles::VehicleSystem;
using heritage::vehicles::WheelContactStatus;
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
    wheel.springPreload = 0.0f;
    wheel.springRate = 35000.0f;
    wheel.springProgression = 15000.0f;
    wheel.bumpDamping = 3200.0f;
    wheel.bumpHighSpeedDamping = 1800.0f;
    wheel.bumpDampingKneeVelocity = 0.25f;
    wheel.reboundDamping = 4200.0f;
    wheel.reboundHighSpeedDamping = 2600.0f;
    wheel.reboundDampingKneeVelocity = 0.30f;
    wheel.bumpStopEngagement = 0.15f;
    wheel.bumpStopRate = 120000.0f;
    wheel.bumpStopProgression = 1000000.0f;
    wheel.droopStopEngagement = 0.1275f;
    wheel.droopStopRate = 35000.0f;
    wheel.suspensionMotionRatio = 1.0f;
    wheel.maximumSuspensionForce = 250000.0f;
    wheel.effectiveUnsprungMass = 38.0f;
    wheel.tireRadialStiffness = 220000.0f;
    wheel.tireRadialDamping = 1800.0f;
    wheel.maximumTireDeflection = 0.08f;
    wheel.maximumTireNormalForce = 250000.0f;
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
    const float suspensionSpeedDifference = std::abs(
        nativeRateSample.maximumSuspensionVelocity
            - highRateInsideWorldRateSample.maximumSuspensionVelocity);
    std::cout
        << "rate_comparison final_height_difference_m="
        << finalHeightDifference
        << " peak_vertical_speed_difference_mps="
        << verticalSpeedDifference
        << " peak_suspension_speed_difference_mps="
        << suspensionSpeedDifference
        << '\n';

    return finalHeightDifference <= 0.020f
        && verticalSpeedDifference <= 0.20f
        && suspensionSpeedDifference <= 0.15f;
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

bool turnThenBrakeRemainsStableAtLowSpeed()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the turn-and-brake world.\n";
        return false;
    }

    const int settleSteps = static_cast<int>(std::round(2.0f / kWorldDeltaTime));
    for (int index = 0; index < settleSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.75f, 0.0f, 0.42f, 0.0f);
    const int cornerSteps = static_cast<int>(std::round(2.0f / kWorldDeltaTime));
    for (int index = 0; index < cornerSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.65f, 0.0f, 0.0f, 0.0f);
    const int straightenSteps = static_cast<int>(std::round(
        0.75f / kWorldDeltaTime));
    for (int index = 0; index < straightenSteps; ++index)
        stepWorld(world);

    world.vehicles.setInputs(world.vehicle, 0.0f, 1.0f, 0.0f, 0.0f);
    const int brakeSteps = static_cast<int>(std::round(5.0f / kWorldDeltaTime));
    int lowSpeedYawReversals = 0;
    int lowSpeedRollReversals = 0;
    float previousSignificantYawSign = 0.0f;
    float previousSignificantRollSign = 0.0f;
    float maximumLowSpeedYawRate = 0.0f;
    float maximumLowSpeedRollRate = 0.0f;
    float maximumLowSpeedRearLateralSpeed = 0.0f;
    float finalSpeed = 0.0f;
    for (int index = 0; index < brakeSteps; ++index)
    {
        stepWorld(world);

        Vec3 linearVelocity{};
        Vec3 angularVelocityDegrees{};
        world.bodies.linearVelocity(world.chassis, linearVelocity);
        world.bodies.angularVelocityDegrees(
            world.chassis,
            angularVelocityDegrees);
        RigidBodyPose brakingPose;
        world.bodies.pose(world.chassis, brakingPose);
        const float brakingYawRadians = brakingPose.rotationDegrees.y
            * 3.14159265358979323846f / 180.0f;
        const Vec3 chassisForward{
            std::sin(brakingYawRadians),
            0.0f,
            std::cos(brakingYawRadians)
        };
        const float localRollRate = angularVelocityDegrees.x * chassisForward.x
            + angularVelocityDegrees.z * chassisForward.z;
        finalSpeed = magnitude(linearVelocity);
        if (finalSpeed > 2.0f)
            continue;

        maximumLowSpeedYawRate = std::max(
            maximumLowSpeedYawRate,
            std::abs(angularVelocityDegrees.y));
        maximumLowSpeedRollRate = std::max(
            maximumLowSpeedRollRate,
            std::abs(localRollRate));
        for (std::size_t wheelIndex = 2; wheelIndex < 4; ++wheelIndex)
        {
            WheelState state;
            if (world.vehicles.wheelState(world.vehicle, wheelIndex, state))
            {
                maximumLowSpeedRearLateralSpeed = std::max(
                    maximumLowSpeedRearLateralSpeed,
                    std::abs(state.lateralSpeed));
            }
        }

        if (std::abs(angularVelocityDegrees.y) >= 0.35f)
        {
            const float yawSign = angularVelocityDegrees.y > 0.0f
                ? 1.0f
                : -1.0f;
            if (previousSignificantYawSign != 0.0f
                && yawSign != previousSignificantYawSign)
            {
                ++lowSpeedYawReversals;
            }
            previousSignificantYawSign = yawSign;
        }

        if (std::abs(localRollRate) >= 0.35f)
        {
            const float rollSign = localRollRate > 0.0f
                ? 1.0f
                : -1.0f;
            if (previousSignificantRollSign != 0.0f
                && rollSign != previousSignificantRollSign)
            {
                ++lowSpeedRollReversals;
            }
            previousSignificantRollSign = rollSign;
        }
    }

    std::cout
        << "turn_then_brake final_speed_mps=" << finalSpeed
        << " max_low_speed_yaw_degps=" << maximumLowSpeedYawRate
        << " max_low_speed_local_roll_degps=" << maximumLowSpeedRollRate
        << " max_rear_lateral_speed_mps="
        << maximumLowSpeedRearLateralSpeed
        << " low_speed_yaw_reversals=" << lowSpeedYawReversals
        << " low_speed_roll_reversals=" << lowSpeedRollReversals
        << '\n';

    return finalSpeed <= 0.15f
        && maximumLowSpeedYawRate <= 0.10f
        && lowSpeedYawReversals <= 1
        && maximumLowSpeedRollRate <= 0.25f
        && lowSpeedRollReversals <= 1;
}

bool terrainContactDiagnosticsClassifyFailureModes()
{
    CollisionSystem queryCollisions;
    RigidBodySystem queryBodies;
    StaticSceneTriangle first;
    first.a = { -2.0f, 0.0f, -2.0f };
    first.b = { -2.0f, 0.0f, 2.0f };
    first.c = { 2.0f, 0.0f, 2.0f };
    first.normal = { 0.0f, 1.0f, 0.0f };
    first.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
    StaticSceneTriangle reversed;
    reversed.a = { -2.0f, 0.0f, -2.0f };
    reversed.b = { 2.0f, 0.0f, 2.0f };
    reversed.c = { 2.0f, 0.0f, -2.0f };
    reversed.normal = { 0.0f, -1.0f, 0.0f };
    reversed.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
    queryCollisions.setStaticSceneTriangles({ first, reversed });

    heritage::physics::CollisionQueryFilter queryFilter;
    heritage::physics::RaycastHit seamHit;
    const bool seamWorked = queryCollisions.raycast(
        { 0.0f, 2.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        4.0f,
        queryFilter,
        queryBodies,
        seamHit);
    const auto seamDiagnostics = queryCollisions.lastRaycastDiagnostics();
    heritage::physics::RaycastHit reversedHit;
    const bool reversedWindingWorked = queryCollisions.raycast(
        { 1.0f, 2.0f, -1.0f },
        { 0.0f, -1.0f, 0.0f },
        4.0f,
        queryFilter,
        queryBodies,
        reversedHit)
        && reversedHit.normal.y > 0.99f;

    StaticSceneTriangle steep;
    steep.a = { -2.0f, -2.0f, -2.0f };
    steep.b = { -2.0f, -2.0f, 2.0f };
    steep.c = { 2.0f, 2.0f, 2.0f };
    steep.normal = { -0.70710678f, 0.70710678f, 0.0f };
    queryCollisions.setStaticSceneTriangles({ steep });
    heritage::physics::RaycastHit steepHit;
    const bool steepSlopeWorked = queryCollisions.raycast(
        { 1.0f, 3.0f, 1.5f },
        { 0.0f, -1.0f, 0.0f },
        4.0f,
        queryFilter,
        queryBodies,
        steepHit)
        && std::abs(steepHit.point.y - 1.0f) < 0.0001f
        && steepHit.normal.y > 0.70f;

    StaticSceneTriangle leftA;
    leftA.a = { -2.0f, 0.0f, -2.0f };
    leftA.b = { -2.0f, 0.0f, 2.0f };
    leftA.c = { -0.01f, 0.0f, 2.0f };
    leftA.normal = { 0.0f, 1.0f, 0.0f };
    StaticSceneTriangle leftB;
    leftB.a = { -2.0f, 0.0f, -2.0f };
    leftB.b = { -0.01f, 0.0f, 2.0f };
    leftB.c = { -0.01f, 0.0f, -2.0f };
    leftB.normal = { 0.0f, 1.0f, 0.0f };
    StaticSceneTriangle rightA;
    rightA.a = { 0.01f, 0.0f, -2.0f };
    rightA.b = { 0.01f, 0.0f, 2.0f };
    rightA.c = { 2.0f, 0.0f, 2.0f };
    rightA.normal = { 0.0f, 1.0f, 0.0f };
    StaticSceneTriangle rightB;
    rightB.a = { 0.01f, 0.0f, -2.0f };
    rightB.b = { 2.0f, 0.0f, 2.0f };
    rightB.c = { 2.0f, 0.0f, -2.0f };
    rightB.normal = { 0.0f, 1.0f, 0.0f };
    queryCollisions.setStaticSceneTriangles({
        leftA, leftB, rightA, rightB });
    heritage::physics::RaycastHit gapHit;
    const bool gapMissed = !queryCollisions.raycast(
        { 0.0f, 2.0f, 0.0f },
        { 0.0f, -1.0f, 0.0f },
        4.0f,
        queryFilter,
        queryBodies,
        gapHit);
    const auto gapDiagnostics = queryCollisions.lastRaycastDiagnostics();
    const bool realGapIdentified = gapMissed
        && gapDiagnostics.staticSceneLoaded
        && gapDiagnostics.originInsideStaticSceneHorizontalBounds
        && gapDiagnostics.rayBoundsOverlapStaticScene
        && gapDiagnostics.staticTriangleCandidateCount == 0;

    auto settleOnStaticScene = [](PrototypeWorld& world) {
        if (!createPrototypeWorld(world, 1000.0f)
            || !replaceFloorWithSlope(world, 0.0f))
        {
            return false;
        }
        for (int index = 0; index < 240; ++index)
            stepWorld(world);
        WheelState state;
        return world.vehicles.wheelState(world.vehicle, 0, state)
            && state.grounded
            && (state.contactStatus == WheelContactStatus::Supported
                || state.contactStatus
                    == WheelContactStatus::SuspensionBottomed);
    };
    auto moveChassis = [](PrototypeWorld& world, const Vec3& position) {
        RigidBodyPose pose;
        if (!world.bodies.pose(world.chassis, pose))
            return false;
        pose.position = position;
        return world.bodies.setPose(world.chassis, pose)
            && world.bodies.setLinearVelocity(world.chassis, {})
            && world.bodies.setAngularVelocityDegrees(world.chassis, {});
    };
    auto queryVehicleOnce = [](PrototypeWorld& world, WheelState& state) {
        world.vehicles.resetClock();
        world.vehicles.simulate(
            world.bodies,
            world.collisions,
            0.001f,
            kGravity);
        return world.vehicles.wheelState(world.vehicle, 0, state);
    };

    PrototypeWorld bottomedWorld;
    WheelState bottomedState;
    const bool bottomedWorked = settleOnStaticScene(bottomedWorld)
        && moveChassis(bottomedWorld, { 0.0f, -0.55f, 0.0f })
        && queryVehicleOnce(bottomedWorld, bottomedState)
        && bottomedState.suspensionBottomed
        && bottomedState.bottomOutPenetration > 0.0f
        && bottomedState.contactStatus
            == WheelContactStatus::SuspensionBottomed;

    PrototypeWorld descentWorld;
    WheelState supportedState;
    WheelState descentState;
    bool descentWorked = settleOnStaticScene(descentWorld)
        && descentWorld.vehicles.wheelState(
            descentWorld.vehicle, 0, supportedState)
        && moveChassis(descentWorld, { 0.0f, -1.0f, 0.0f });
    if (descentWorked)
    {
        descentWorld.bodies.setLinearVelocity(
            descentWorld.chassis, { 0.0f, -120.0f, 0.0f });
        descentWorked = queryVehicleOnce(descentWorld, descentState)
            && !descentState.grounded
            && descentState.contactStatus
                == WheelContactStatus::SurfaceBehindRayOrigin
            && descentState.contactLossTransitionCount
                == supportedState.contactLossTransitionCount + 1;
    }

    PrototypeWorld boundaryWorld;
    WheelState boundaryState;
    const bool boundaryWorked = settleOnStaticScene(boundaryWorld)
        && moveChassis(boundaryWorld, { 75.0f, 0.05f, 0.0f })
        && queryVehicleOnce(boundaryWorld, boundaryState)
        && !boundaryState.grounded
        && boundaryState.contactStatus
            == WheelContactStatus::OutsideStaticSceneBounds;

    PrototypeWorld reachWorld;
    WheelState reachState;
    const bool reachWorked = settleOnStaticScene(reachWorld)
        && moveChassis(reachWorld, { 0.0f, 0.50f, 0.0f })
        && queryVehicleOnce(reachWorld, reachState)
        && !reachState.grounded
        && reachState.contactStatus
            == WheelContactStatus::BeyondSuspensionReach
        && reachState.rawSupportDistance > 0.70f;

    PrototypeWorld landingWorld;
    bool landingWorked = createPrototypeWorld(landingWorld, 1000.0f)
        && replaceFloorWithSlope(landingWorld, 0.0f)
        && moveChassis(landingWorld, { 0.0f, 2.0f, 0.0f });
    WheelState landingState;
    if (landingWorked)
    {
        for (int index = 0; index < 360; ++index)
            stepWorld(landingWorld);
        landingWorked = landingWorld.vehicles.wheelState(
                landingWorld.vehicle, 0, landingState)
            && landingState.grounded
            && landingState.selectedHitWasStaticTriangle;
    }

    std::cout
        << "terrain_contact seam=" << (seamWorked ? "hit" : "miss")
        << " seam_static_candidates="
        << seamDiagnostics.staticTriangleCandidateCount
        << " reversed_winding="
        << (reversedWindingWorked ? "hit" : "miss")
        << " steep_slope=" << (steepSlopeWorked ? "hit" : "miss")
        << " real_gap=" << (realGapIdentified ? "identified" : "wrong")
        << " bottomed_status="
        << heritage::vehicles::wheelContactStatusName(
            bottomedState.contactStatus)
        << " descent_status="
        << heritage::vehicles::wheelContactStatusName(
            descentState.contactStatus)
        << " boundary_status="
        << heritage::vehicles::wheelContactStatusName(
            boundaryState.contactStatus)
        << " reach_status="
        << heritage::vehicles::wheelContactStatusName(
            reachState.contactStatus)
        << " landing_status="
        << heritage::vehicles::wheelContactStatusName(
            landingState.contactStatus)
        << '\n';

    return seamWorked
        && seamDiagnostics.selectedHitWasStaticTriangle
        && seamDiagnostics.staticTriangleCandidateCount >= 1
        && reversedWindingWorked
        && steepSlopeWorked
        && realGapIdentified
        && bottomedWorked
        && descentWorked
        && boundaryWorked
        && reachWorked
        && landingWorked;
}

bool dynamicsLabCapturesHighRateTelemetry()
{
    PrototypeWorld world;
    if (!createPrototypeWorld(world, 1000.0f))
    {
        std::cerr << "Could not create the dynamics-lab world.\n";
        return false;
    }
    if (!world.vehicles.startDynamicsLabCapture(
            world.vehicle, 1.0f, 1000.0f))
    {
        std::cerr << "Could not start the dynamics lab: "
            << world.vehicles.lastError() << '\n';
        return false;
    }

    world.vehicles.setInputs(world.vehicle, 0.55f, 0.0f, 0.15f, 0.0f);
    for (int index = 0; index < 120; ++index)
        stepWorld(world);

    heritage::vehicles::DynamicsLabSummary summary;
    if (!world.vehicles.dynamicsLabSummary(world.vehicle, summary))
        return false;

    std::vector<float> speedSeries;
    std::vector<float> wheelLoadSeries;
    std::vector<float> damperPowerSeries;
    std::vector<float> unsprungVelocitySeries;
    std::vector<float> tireDeflectionSeries;
    std::vector<float> camberSeries;
    std::vector<float> toeSeries;
    const bool speedWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::SpeedKph,
        0,
        64,
        speedSeries);
    const bool wheelLoadWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelNormalForceNewtons,
        0,
        64,
        wheelLoadSeries);
    const bool damperPowerWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelDamperDissipationWatts,
        0,
        64,
        damperPowerSeries);
    const bool unsprungVelocityWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelUnsprungVelocityMps,
        0,
        64,
        unsprungVelocitySeries);
    const bool tireDeflectionWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelTireDeflectionMillimeters,
        0,
        64,
        tireDeflectionSeries);
    const bool camberWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelCamberDegrees,
        0,
        64,
        camberSeries);
    const bool toeWorked = world.vehicles.dynamicsLabMetricSeries(
        world.vehicle,
        heritage::vehicles::DynamicsLabMetric::WheelToeDegrees,
        0,
        64,
        toeSeries);

    std::cout
        << "dynamics_lab samples=" << summary.sampleCount
        << " capacity=" << summary.sampleCapacity
        << " duration_s=" << summary.durationSeconds
        << " capture_hz=" << summary.requestedCaptureHertz
        << " peak_speed_kph=" << summary.peakSpeedKph
        << " peak_suspension_speed_mps="
        << summary.peakAbsoluteSuspensionVelocityMps
        << " peak_unsprung_speed_mps="
        << summary.peakAbsoluteUnsprungVelocityMps
        << " peak_tire_deflection_mm="
        << summary.peakTireDeflectionMillimeters
        << " speed_plot_points=" << speedSeries.size()
        << " load_plot_points=" << wheelLoadSeries.size()
        << '\n';

    return summary.captureComplete
        && !summary.recording
        && summary.sampleCount == 1000
        && summary.sampleCapacity == 1000
        && summary.wheelCount == 4
        && std::abs(summary.durationSeconds - 1.0) <= 0.001
        && summary.peakSpeedKph > 0.1f
        && speedWorked
        && wheelLoadWorked
        && damperPowerWorked
        && unsprungVelocityWorked
        && tireDeflectionWorked
        && camberWorked
        && toeWorked
        && summary.peakAbsoluteUnsprungVelocityMps > 0.0f
        && summary.peakTireDeflectionMillimeters > 0.0f
        && !speedSeries.empty()
        && speedSeries.size() <= 64
        && wheelLoadSeries.size() == speedSeries.size()
        && damperPowerSeries.size() == speedSeries.size()
        && unsprungVelocitySeries.size() == speedSeries.size()
        && tireDeflectionSeries.size() == speedSeries.size()
        && camberSeries.size() == speedSeries.size()
        && toeSeries.size() == speedSeries.size();
}

VehicleDefinitionV2Source makeCompiledRoadCarDefinition()
{
    VehicleDefinitionV2Source source;
    source.id = "native_compiler_test";
    source.displayName = "Native Compiler Test";
    source.classification = "car";
    source.bodyAsset = "Vehicles/Player/PlayerCar.obj";
    source.bodies.push_back({ "chassis", "primary", 1100.0f });

    heritage::vehicles::VehiclePowerUnitDefinition power;
    power.id = "engine";
    power.kind = "combustion";
    power.mountBody = "chassis";
    power.location = "front";
    power.maximumTorqueNm = 250.0f;
    source.powerUnits.push_back(power);

    heritage::vehicles::VehicleTransmissionDefinition transmission;
    transmission.id = "gearbox";
    transmission.kind = "manual";
    transmission.powerUnit = "engine";
    transmission.forwardRatios = { 3.40f, 2.10f, 1.45f, 1.12f, 0.89f, 0.74f };
    source.transmissions.push_back(transmission);

    const Vec3 mounts[] = {
        { -0.7185f, 0.85f, 1.221f },
        { 0.7185f, 0.85f, 1.221f },
        { -0.7140f, 0.85f, -1.221f },
        { 0.7140f, 0.85f, -1.221f }
    };
    for (std::size_t index = 0; index < 4; ++index)
    {
        heritage::vehicles::VehicleSuspensionDefinition suspension;
        suspension.id = "suspension_" + std::to_string(index + 1);
        suspension.provider = "linear_raycast_v1";
        suspension.mountBody = "chassis";
        suspension.restLengthM = 0.55f;
        suspension.maximumCompressionM = 0.20f;
        suspension.maximumDroopM = 0.15f;
        suspension.springRateNPerM = 35000.0f;
        suspension.bumpDampingNsPerM = 3200.0f;
        suspension.reboundDampingNsPerM = 4200.0f;
        suspension.staticCamberDegrees = index % 2 == 0 ? -0.75f : 0.75f;
        suspension.camberGainDegreesPerM = index % 2 == 0 ? -4.0f : 4.0f;
        suspension.staticToeDegrees = index % 2 == 0 ? 0.10f : -0.10f;
        source.suspensions.push_back(suspension);

        heritage::vehicles::VehicleContactUnitDefinition contact;
        contact.id = "wheel_" + std::to_string(index + 1);
        contact.kind = "wheel";
        contact.mountBody = "chassis";
        contact.axle = index < 2 ? "front" : "rear";
        contact.localMount = mounts[index];
        contact.steering = index < 2;
        contact.parkingBrake = index >= 2;
        contact.suspension = suspension.id;
        contact.tireProvider = "advanced_road";
        contact.radiusM = 0.2979f;
        contact.effectiveUnsprungMassKg = 38.0f;
        contact.tireRadialStiffnessNPerM = 220000.0f;
        contact.tireRadialDampingNsPerM = 1800.0f;
        contact.maximumTireDeflectionM = 0.08f;
        contact.maximumTireNormalForceN = 250000.0f;
        contact.serviceBrakeFactor = index < 2 ? 0.31f : 0.19f;
        contact.parkingBrakeFactor = index >= 2 ? 0.50f : 0.0f;
        source.contactUnits.push_back(contact);
    }

    heritage::vehicles::VehicleDriveConnectionDefinition drive;
    drive.id = "front_drive";
    drive.transmission = "gearbox";
    drive.contactUnits = { "wheel_1", "wheel_2" };
    source.driveConnections.push_back(std::move(drive));
    return source;
}

bool vehicleDefinitionCompilerAndLoaderWork()
{
    const VehicleDefinitionV2Source source = makeCompiledRoadCarDefinition();
    const auto compiled = VehicleDefinitionCompiler::compile(source);
    const bool referencesResolved = compiled.definition.powerUnits.size() == 1
        && compiled.definition.powerUnits[0].mountBodyIndex == 0
        && compiled.definition.transmissions.size() == 1
        && compiled.definition.transmissions[0].powerUnitIndex == 0
        && compiled.definition.suspensions.size() == 4
        && compiled.definition.suspensions[0].mountBodyIndex == 0
        && compiled.definition.driveConnections.size() == 1
        && compiled.definition.driveConnections[0].contactUnitIndices.size() == 2
        && compiled.definition.contactUnits.size() == 4
        && compiled.definition.contactUnits[0].suspensionIndex == 0
        && std::abs(
            compiled.definition.suspensions[0].authored.staticCamberDegrees
                + 0.75f) <= 0.000001f
        && std::abs(compiled.definition.contactUnits[0].driveFactor - 0.5f)
            <= 0.000001f
        && std::abs(compiled.definition.contactUnits[2].driveFactor)
            <= 0.000001f;

    RigidBodySystem bodies;
    RigidBodyDescription bodyDescription;
    bodyDescription.motionType = BodyMotionType::Dynamic;
    bodyDescription.mass = 1100.0f;
    const BodyHandle chassis = bodies.create(bodyDescription);
    VehicleSystem vehicles;
    VehicleDefinitionLoadSettings loadSettings;
    loadSettings.vehicle.chassisBody = chassis;
    loadSettings.vehicle.highRateHertz = 1000.0f;
    std::string loadMessage;
    const VehicleHandle loaded = VehicleDefinitionLoader::create(
        compiled.definition,
        loadSettings,
        bodies,
        vehicles,
        loadMessage);
    const bool runtimeLoaded = loaded != heritage::vehicles::InvalidVehicle
        && vehicles.wheelCount(loaded) == 4
        && vehicles.forwardGearCount(loaded) == 6;

    VehicleDefinitionV2Source broken = source;
    broken.transmissions[0].powerUnit = "missing_engine";
    const auto brokenResult = VehicleDefinitionCompiler::compile(broken);

    VehicleDefinitionV2Source brokenSuspension = source;
    brokenSuspension.contactUnits[0].suspension = "missing_suspension";
    const auto brokenSuspensionResult =
        VehicleDefinitionCompiler::compile(brokenSuspension);

    VehicleDefinitionV2Source invalidSuspensionParameters = source;
    invalidSuspensionParameters.suspensions[0].bumpHighSpeedDampingNsPerM =
        -1.0f;
    const auto invalidSuspensionParametersResult =
        VehicleDefinitionCompiler::compile(invalidSuspensionParameters);

    VehicleDefinitionV2Source invalidUnsprungParameters = source;
    invalidUnsprungParameters.contactUnits[0].effectiveUnsprungMassKg =
        -1.0f;
    const auto invalidUnsprungParametersResult =
        VehicleDefinitionCompiler::compile(invalidUnsprungParameters);

    VehicleDefinitionV2Source invalidGeometryParameters = source;
    invalidGeometryParameters.suspensions[0].localSteeringAxis = {};
    const auto invalidGeometryParametersResult =
        VehicleDefinitionCompiler::compile(invalidGeometryParameters);

    VehicleDefinitionV2Source future = source;
    future.classification = "motorcycle";
    future.requirements.leanDynamics = true;
    future.contactUnits.resize(2);
    const auto futureResult = VehicleDefinitionCompiler::compile(future);

    VehicleDefinitionV2Source categoryOnly = source;
    categoryOnly.classification = "fictional_hovering_potato";
    const auto categoryOnlyResult = VehicleDefinitionCompiler::compile(categoryOnly);

    VehicleDefinitionV2Source futureSuspension = source;
    futureSuspension.suspensions[0].provider = "double_wishbone_v1";
    const auto futureSuspensionResult =
        VehicleDefinitionCompiler::compile(futureSuspension);

    heritage::vehicles::SuspensionModelDescription suspensionDescription;
    suspensionDescription.springRateNPerM = 10000.0f;
    suspensionDescription.bumpDampingNsPerM = 1000.0f;
    suspensionDescription.reboundDampingNsPerM = 2000.0f;
    suspensionDescription.motionRatio = 0.5f;
    suspensionDescription.maximumForceN = 5000.0f;
    const auto bumpOutput = heritage::vehicles::evaluateSuspensionModel(
        suspensionDescription,
        { 0.10f, 0.20f });
    const auto reboundOutput = heritage::vehicles::evaluateSuspensionModel(
        suspensionDescription,
        { 0.10f, -0.20f });
    const bool suspensionForcesWorked =
        std::abs(bumpOutput.normalForceN - 300.0f) <= 0.0001f
        && std::abs(reboundOutput.normalForceN - 150.0f) <= 0.0001f;

    heritage::vehicles::SuspensionModelDescription nonlinearSuspension;
    nonlinearSuspension.springPreloadN = 100.0f;
    nonlinearSuspension.springRateNPerM = 1000.0f;
    nonlinearSuspension.springProgressionNPerM2 = 2000.0f;
    nonlinearSuspension.bumpDampingNsPerM = 100.0f;
    nonlinearSuspension.bumpHighSpeedDampingNsPerM = 50.0f;
    nonlinearSuspension.bumpDampingKneeVelocityMps = 0.10f;
    nonlinearSuspension.reboundDampingNsPerM = 200.0f;
    nonlinearSuspension.reboundHighSpeedDampingNsPerM = 100.0f;
    nonlinearSuspension.reboundDampingKneeVelocityMps = 0.10f;
    nonlinearSuspension.bumpStopEngagementM = 0.10f;
    nonlinearSuspension.bumpStopRateNPerM = 1000.0f;
    nonlinearSuspension.bumpStopProgressionNPerM2 = 2000.0f;
    nonlinearSuspension.droopStopEngagementM = 0.10f;
    nonlinearSuspension.droopStopRateNPerM = 500.0f;
    nonlinearSuspension.motionRatio = 1.0f;
    nonlinearSuspension.maximumForceN = 10000.0f;
    const auto nonlinearBump = heritage::vehicles::evaluateSuspensionModel(
        nonlinearSuspension,
        { 0.20f, 0.30f });
    const auto nonlinearRebound = heritage::vehicles::evaluateSuspensionModel(
        nonlinearSuspension,
        { -0.20f, -0.30f });
    const bool nonlinearSuspensionWorked =
        std::abs(nonlinearBump.springForceN - 340.0f) <= 0.0001f
        && std::abs(nonlinearBump.dampingForceN - 20.0f) <= 0.0001f
        && std::abs(nonlinearBump.bumpStopForceN - 110.0f) <= 0.0001f
        && std::abs(nonlinearBump.normalForceN - 470.0f) <= 0.0001f
        && std::abs(nonlinearBump.damperDissipationW - 6.0f) <= 0.0001f
        && std::abs(nonlinearRebound.droopStopForceN - 50.0f) <= 0.0001f
        && nonlinearRebound.normalForceN == 0.0f;

    heritage::vehicles::SuspensionModelDescription suspensionReadback;
    const bool liveSuspensionSet = vehicles.setWheelSuspensionModel(
        loaded, 0, nonlinearSuspension);
    const bool liveSuspensionRead = vehicles.wheelSuspensionModel(
        loaded, 0, suspensionReadback);
    heritage::vehicles::SuspensionModelDescription invalidLiveSuspension =
        nonlinearSuspension;
    invalidLiveSuspension.motionRatio = 0.0f;
    const bool invalidLiveSuspensionRejected =
        !vehicles.setWheelSuspensionModel(
            loaded, 0, invalidLiveSuspension);
    const bool liveSuspensionRoundTrip = liveSuspensionSet
        && liveSuspensionRead
        && std::abs(suspensionReadback.springPreloadN - 100.0f)
            <= 0.0001f
        && std::abs(suspensionReadback.bumpHighSpeedDampingNsPerM - 50.0f)
            <= 0.0001f
        && std::abs(suspensionReadback.bumpStopProgressionNPerM2 - 2000.0f)
            <= 0.0001f
        && invalidLiveSuspensionRejected;

    heritage::vehicles::UnsprungMassDescription unsprungReadback;
    heritage::vehicles::UnsprungMassDescription liveUnsprung;
    liveUnsprung.effectiveMassKg = 42.0f;
    liveUnsprung.tireRadialStiffnessNPerM = 240000.0f;
    liveUnsprung.tireRadialDampingNsPerM = 2000.0f;
    liveUnsprung.maximumTireDeflectionM = 0.09f;
    liveUnsprung.maximumNormalForceN = 275000.0f;
    const bool liveUnsprungSet = vehicles.setWheelUnsprungMassModel(
        loaded, 0, liveUnsprung);
    const bool liveUnsprungRead = vehicles.wheelUnsprungMassModel(
        loaded, 0, unsprungReadback);
    heritage::vehicles::UnsprungMassDescription invalidLiveUnsprung =
        liveUnsprung;
    invalidLiveUnsprung.tireRadialStiffnessNPerM = 0.0f;
    const bool invalidLiveUnsprungRejected =
        !vehicles.setWheelUnsprungMassModel(
            loaded, 0, invalidLiveUnsprung);
    const bool liveUnsprungRoundTrip = liveUnsprungSet
        && liveUnsprungRead
        && std::abs(unsprungReadback.effectiveMassKg - 42.0f) <= 0.0001f
        && std::abs(
            unsprungReadback.tireRadialStiffnessNPerM - 240000.0f)
            <= 0.0001f
        && std::abs(unsprungReadback.maximumTireDeflectionM - 0.09f)
            <= 0.0001f
        && invalidLiveUnsprungRejected;

    heritage::vehicles::SuspensionGeometryDescription geometryReadback;
    heritage::vehicles::SuspensionGeometryDescription liveGeometry;
    liveGeometry.localSteeringAxis = { 0.10f, 0.98f, -0.12f };
    liveGeometry.staticCamberDegrees = -1.25f;
    liveGeometry.camberGainDegreesPerM = -8.0f;
    liveGeometry.camberProgressionDegreesPerM2 = 25.0f;
    liveGeometry.staticToeDegrees = 0.15f;
    liveGeometry.toeGainDegreesPerM = 2.5f;
    liveGeometry.toeProgressionDegreesPerM2 = -12.0f;
    const bool liveGeometrySet = vehicles.setWheelSuspensionGeometry(
        loaded, 0, liveGeometry);
    const bool liveGeometryRead = vehicles.wheelSuspensionGeometry(
        loaded, 0, geometryReadback);
    heritage::vehicles::SuspensionGeometryDescription invalidLiveGeometry =
        liveGeometry;
    invalidLiveGeometry.localSteeringAxis = {};
    const bool invalidLiveGeometryRejected =
        !vehicles.setWheelSuspensionGeometry(
            loaded, 0, invalidLiveGeometry);
    const bool liveGeometryRoundTrip = liveGeometrySet
        && liveGeometryRead
        && std::abs(geometryReadback.staticCamberDegrees + 1.25f)
            <= 0.0001f
        && std::abs(geometryReadback.camberGainDegreesPerM + 8.0f)
            <= 0.0001f
        && std::abs(geometryReadback.toeProgressionDegreesPerM2 + 12.0f)
            <= 0.0001f
        && std::abs(magnitude(geometryReadback.localSteeringAxis) - 1.0f)
            <= 0.0001f
        && invalidLiveGeometryRejected;

    std::cout
        << "definition_compiler provider="
        << compiled.definition.runtimeProvider
        << " wheels=" << vehicles.wheelCount(loaded)
        << " gears=" << vehicles.forwardGearCount(loaded)
        << " invalid_reference_rejected=" << (!brokenResult.valid)
        << " suspension_reference_rejected=" << (!brokenSuspensionResult.valid)
        << " suspension_parameters_rejected="
        << (!invalidSuspensionParametersResult.valid)
        << " unsprung_parameters_rejected="
        << (!invalidUnsprungParametersResult.valid)
        << " geometry_parameters_rejected="
        << (!invalidGeometryParametersResult.valid)
        << " future_topology_valid=" << futureResult.valid
        << " future_topology_ready=" << futureResult.currentSolverReady
        << " category_ignored=" << categoryOnlyResult.currentSolverReady
        << " future_suspension_ready="
        << futureSuspensionResult.currentSolverReady
        << " motion_ratio_force_n=" << bumpOutput.normalForceN
        << " nonlinear_force_n=" << nonlinearBump.normalForceN
        << " damper_power_w=" << nonlinearBump.damperDissipationW
        << " live_suspension_roundtrip=" << liveSuspensionRoundTrip
        << " live_unsprung_roundtrip=" << liveUnsprungRoundTrip
        << " live_geometry_roundtrip=" << liveGeometryRoundTrip
        << '\n';

    return compiled.valid
        && compiled.currentSolverReady
        && compiled.definition.runtimeProvider == "raycast_wheel_v1"
        && referencesResolved
        && runtimeLoaded
        && !brokenResult.valid
        && !brokenSuspensionResult.valid
        && !invalidSuspensionParametersResult.valid
        && !invalidUnsprungParametersResult.valid
        && !invalidGeometryParametersResult.valid
        && futureResult.valid
        && !futureResult.currentSolverReady
        && futureResult.issueSummary().find("lean_dynamics") != std::string::npos
        && categoryOnlyResult.currentSolverReady
        && futureSuspensionResult.valid
        && !futureSuspensionResult.currentSolverReady
        && futureSuspensionResult.issueSummary().find("double_wishbone_v1")
            != std::string::npos
        && suspensionForcesWorked
        && nonlinearSuspensionWorked
        && liveSuspensionRoundTrip
        && liveUnsprungRoundTrip
        && liveGeometryRoundTrip;
}

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
    const float expectedDeflection = input.suspensionForceN
        / description.tireRadialStiffnessNPerM;
    const bool settled = std::abs(
            output.tireDeflectionM - expectedDeflection) < 0.0002f
        && std::abs(output.suspensionLengthVelocityMps) < 0.002f
        && std::abs(output.normalForceN - input.suspensionForceN) < 15.0f;

    input.roadHubLengthM -= 0.02f;
    float peakNormalForce = 0.0f;
    float peakUnsprungSpeed = 0.0f;
    int velocityReversals = 0;
    float previousVelocity = output.suspensionLengthVelocityMps;
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

    const bool turnBrakePassed = turnThenBrakeRemainsStableAtLowSpeed();
    std::cout << (turnBrakePassed ? "PASS" : "FAIL")
        << " turn-then-brake low-speed stability\n";
    failed += turnBrakePassed ? 0 : 1;

    const bool terrainDiagnosticsPassed =
        terrainContactDiagnosticsClassifyFailureModes();
    std::cout << (terrainDiagnosticsPassed ? "PASS" : "FAIL")
        << " terrain contact diagnostics and boundaries\n";
    failed += terrainDiagnosticsPassed ? 0 : 1;

    const bool dynamicsLabPassed = dynamicsLabCapturesHighRateTelemetry();
    std::cout << (dynamicsLabPassed ? "PASS" : "FAIL")
        << " high-rate vehicle dynamics laboratory\n";
    failed += dynamicsLabPassed ? 0 : 1;

    const bool unsprungMassPassed =
        unsprungMassSettlesAndRespondsToRoadStep();
    std::cout << (unsprungMassPassed ? "PASS" : "FAIL")
        << " scalar unsprung-mass wheel-hop response\n";
    failed += unsprungMassPassed ? 0 : 1;

    const bool suspensionGeometryPassed =
        suspensionGeometryProducesAuthoritativePose();
    std::cout << (suspensionGeometryPassed ? "PASS" : "FAIL")
        << " authoritative suspension upright pose\n";
    failed += suspensionGeometryPassed ? 0 : 1;

    const bool definitionCompilerPassed =
        vehicleDefinitionCompilerAndLoaderWork();
    std::cout << (definitionCompilerPassed ? "PASS" : "FAIL")
        << " native vehicle-definition compiler and loader\n";
    failed += definitionCompilerPassed ? 0 : 1;

    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
        << " count=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
