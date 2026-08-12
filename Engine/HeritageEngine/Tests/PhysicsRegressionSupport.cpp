#include "PhysicsRegressionCommon.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace heritage::tests {

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

void stepWorld(PrototypeWorld& world, float worldDeltaTime)
{
    world.vehicles.simulate(
        world.bodies,
        world.collisions,
        world.surfaces,
        worldDeltaTime,
        kGravity);
    world.bodies.integrate(worldDeltaTime, kGravity);
    world.collisions.simulate(world.bodies, worldDeltaTime);
}

StabilitySample sampleStability(
    PrototypeWorld& world,
    float settleSeconds,
    float measureSeconds,
    float worldDeltaTime)
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

} // namespace heritage::tests
