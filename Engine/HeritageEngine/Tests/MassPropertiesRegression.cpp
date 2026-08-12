#include "PhysicsRegressionCommon.hpp"

#include "../Vehicles/Dynamics/MassProperties/VehicleMassPropertiesAccumulator.hpp"
#include "../Vehicles/Dynamics/MassProperties/VehicleMassPropertiesEstimator.hpp"

#include <cmath>
#include <iostream>

namespace heritage::tests {

bool vehicleMassPropertiesEstimatorProducesBoundedEstimate()
{
    heritage::vehicles::VehicleMassPropertiesEstimateInput input;
    input.totalMassKg = 1100.0;
    input.wheelbaseM = 2.442;
    input.frontTrackM = 1.437;
    input.rearTrackM = 1.428;
    input.centerOfMassHeightM = 0.52;
    input.frontStaticLoadFraction = 0.5819001;
    input.leftStaticLoadFraction = 0.50;
    input.massClass = heritage::vehicles::VehicleMassClass::RoadCar;

    const auto estimate =
        heritage::vehicles::estimateVehicleMassProperties(input);
    const bool passed = estimate.valid
        && std::abs(estimate.totalMassKg - 1100.0) <= 0.000001
        && std::abs(estimate.centerOfMassLocal.x) <= 0.000001f
        && std::abs(estimate.centerOfMassLocal.y - 0.52f) <= 0.000001f
        && std::abs(estimate.centerOfMassLocal.z - 0.20f) <= 0.00001f
        && std::abs(estimate.inertiaLocalKgM2.x - 1212.8886f) <= 0.05f
        && std::abs(estimate.inertiaLocalKgM2.y - 1511.3550f) <= 0.05f
        && std::abs(estimate.inertiaLocalKgM2.z - 564.3155f) <= 0.05f
        && std::abs(estimate.frontStaticMassKg - 640.0901) <= 0.05
        && std::abs(estimate.rearStaticMassKg - 459.9099) <= 0.05
        && std::abs(estimate.leftStaticMassKg - 550.0) <= 0.0001
        && std::abs(estimate.rightStaticMassKg - 550.0) <= 0.0001
        && estimate.provenance == "estimated_mass_properties_road_car_v1"
        && std::abs(estimate.confidence - 0.20) <= 0.000001;

    if (!passed)
    {
        std::cout << "  mass estimate com="
            << estimate.centerOfMassLocal.x << ','
            << estimate.centerOfMassLocal.y << ','
            << estimate.centerOfMassLocal.z
            << " inertia="
            << estimate.inertiaLocalKgM2.x << ','
            << estimate.inertiaLocalKgM2.y << ','
            << estimate.inertiaLocalKgM2.z
            << " frontMass=" << estimate.frontStaticMassKg
            << " provenance=" << estimate.provenance << '\n';
    }
    return passed;
}


bool vehicleMassComponentAccumulationUsesParallelAxisTheorem()
{
    std::vector<heritage::vehicles::VehicleMassComponent> components;
    components.push_back({
        "left", 50.0, { -1.0f, 0.0f, 0.0f }, { 10.0f, 20.0f, 30.0f } });
    components.push_back({
        "right", 50.0, { 1.0f, 0.0f, 0.0f }, { 10.0f, 20.0f, 30.0f } });

    const auto accumulated =
        heritage::vehicles::accumulateVehicleMassProperties(components);
    if (!accumulated.valid)
        return false;

    // Symmetric point placement leaves COM at the origin. X-axis inertia does
    // not gain a parallel-axis term because the offsets are purely along X;
    // yaw/roll inertia each gain 2 * 50 kg * 1 m^2.
    const bool passed = std::abs(accumulated.totalMassKg - 100.0) <= 0.000001
        && std::abs(accumulated.centerOfMassLocal.x) <= 0.000001f
        && std::abs(accumulated.centerOfMassLocal.y) <= 0.000001f
        && std::abs(accumulated.centerOfMassLocal.z) <= 0.000001f
        && std::abs(accumulated.inertiaLocalKgM2.x - 20.0f) <= 0.0001f
        && std::abs(accumulated.inertiaLocalKgM2.y - 140.0f) <= 0.0001f
        && std::abs(accumulated.inertiaLocalKgM2.z - 160.0f) <= 0.0001f;

    if (!passed)
    {
        std::cout << "  accumulated mass=" << accumulated.totalMassKg
            << " com=" << accumulated.centerOfMassLocal.x << ','
            << accumulated.centerOfMassLocal.y << ','
            << accumulated.centerOfMassLocal.z
            << " inertia=" << accumulated.inertiaLocalKgM2.x << ','
            << accumulated.inertiaLocalKgM2.y << ','
            << accumulated.inertiaLocalKgM2.z << '\n';
    }
    return passed;
}

bool rigidBodyExplicitInertiaIsAuthoritative()
{
    RigidBodySystem bodies;
    CollisionSystem collisions;

    RigidBodyDescription description;
    description.motionType = BodyMotionType::Dynamic;
    description.mass = 1000.0f;
    description.gravityFactor = 0.0f;
    description.linearDamping = 0.0f;
    description.angularDamping = 0.0f;
    const BodyHandle body = bodies.create(description);
    if (body == heritage::physics::InvalidBody)
        return false;

    const ColliderHandle collider = collisions.createBox(
        body,
        { 1.0f, 0.4f, 1.8f },
        { 0.0f, 0.6f, 0.0f },
        0.5f,
        0.0f,
        false,
        bodies);
    if (collider == heritage::physics::InvalidCollider)
        return false;

    const Vec3 authoredInertia{ 1000.0f, 2000.0f, 500.0f };
    if (!bodies.setInertiaLocal(body, authoredInertia))
        return false;

    // Force CollisionSystem to rebuild mass properties. The explicit tensor
    // must survive even though the attached box would derive different values.
    collisions.simulate(bodies, kWorldDeltaTime);

    Vec3 actualInertia{};
    bool overridden = false;
    if (!bodies.inertiaLocal(body, actualInertia)
        || !bodies.inertiaLocalOverridden(body, overridden))
    {
        return false;
    }

    if (!overridden
        || std::abs(actualInertia.x - authoredInertia.x) > 0.001f
        || std::abs(actualInertia.y - authoredInertia.y) > 0.001f
        || std::abs(actualInertia.z - authoredInertia.z) > 0.001f)
    {
        return false;
    }

    // Verify the tensor is not metadata-only: equal angular impulse about each
    // local principal axis must produce angular acceleration inversely
    // proportional to its inertia.
    constexpr float impulse = 100.0f;
    Vec3 angularVelocity{};

    bodies.setAngularVelocityDegrees(body, {});
    if (!bodies.applyAngularImpulse(body, { impulse, 0.0f, 0.0f })
        || !bodies.angularVelocityDegrees(body, angularVelocity))
    {
        return false;
    }
    const float pitchSpeed = std::abs(angularVelocity.x);

    bodies.setAngularVelocityDegrees(body, {});
    if (!bodies.applyAngularImpulse(body, { 0.0f, impulse, 0.0f })
        || !bodies.angularVelocityDegrees(body, angularVelocity))
    {
        return false;
    }
    const float yawSpeed = std::abs(angularVelocity.y);

    bodies.setAngularVelocityDegrees(body, {});
    if (!bodies.applyAngularImpulse(body, { 0.0f, 0.0f, impulse })
        || !bodies.angularVelocityDegrees(body, angularVelocity))
    {
        return false;
    }
    const float rollSpeed = std::abs(angularVelocity.z);

    const bool angularResponse = pitchSpeed > 0.0f
        && std::abs((yawSpeed / pitchSpeed) - 0.5f) <= 0.001f
        && std::abs((rollSpeed / pitchSpeed) - 2.0f) <= 0.001f;
    if (!angularResponse)
    {
        std::cout << "  inertia angular speeds pitch/yaw/roll="
            << pitchSpeed << '/' << yawSpeed << '/' << rollSpeed << '\n';
    }
    return angularResponse;
}

} // namespace heritage::tests
