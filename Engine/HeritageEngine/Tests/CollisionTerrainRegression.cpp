#include "PhysicsRegressionCommon.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace heritage::tests {

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

    // TIRE17C7/VIS10: the visual tire GPU path consumes an exact, bounded
    // neighborhood from this same immutable BVH. Preserve creator triangle
    // geometry rather than reconstructing a height field.
    std::vector<StaticSceneTriangle> visualNeighborhood;
    queryCollisions.nearbyStaticSceneTriangles(
        { 0.0f, 0.25f, 0.0f }, 0.75f, 1, visualNeighborhood);
    if (visualNeighborhood.size() != 1
        || std::abs(visualNeighborhood.front().a.y) > 0.0001f
        || std::abs(visualNeighborhood.front().b.y) > 0.0001f
        || std::abs(visualNeighborhood.front().c.y) > 0.0001f)
    {
        std::cerr << "Static-triangle visual neighborhood regression.\n";
        return 1;
    }

    // TIRE22/VIS14: a huge creator triangle can pass directly beneath the
    // wheel while all of its vertices and its centroid are far away. Nearest
    // geometry ranking must therefore use the true closest point on the
    // finite triangle, not authored corners/centroid.
    StaticSceneTriangle hugeNearbySurface;
    hugeNearbySurface.a = { -100.0f, 0.0f, -100.0f };
    hugeNearbySurface.b = { -100.0f, 0.0f, 100.0f };
    hugeNearbySurface.c = { 100.0f, 0.0f, 100.0f };
    hugeNearbySurface.normal = { 0.0f, 1.0f, 0.0f };
    StaticSceneTriangle misleadingSmallTriangle;
    misleadingSmallTriangle.a = { 0.0f, 0.60f, 0.0f };
    misleadingSmallTriangle.b = { 0.10f, 0.60f, 0.0f };
    misleadingSmallTriangle.c = { 0.0f, 0.60f, 0.10f };
    misleadingSmallTriangle.normal = { 0.0f, -1.0f, 0.0f };
    queryCollisions.setStaticSceneTriangles({
        hugeNearbySurface, misleadingSmallTriangle });
    visualNeighborhood.clear();
    queryCollisions.nearbyStaticSceneTriangles(
        { 0.0f, 0.25f, 0.0f }, 1.0f, 1, visualNeighborhood);
    if (visualNeighborhood.size() != 1
        || std::abs(visualNeighborhood.front().a.y) > 0.0001f)
    {
        std::cerr << "TIRE22 true triangle-distance ranking regression.\n";
        return 1;
    }
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

    // CAM04: swept camera spheres must collide with creator-authored static
    // triangles, not only primitive box/sphere colliders. A sphere descending
    // toward the same query terrain should stop one radius above the surface.
    heritage::physics::SphereCastHit staticSphereHit{};
    const bool staticSphereCastWorked = queryCollisions.sphereCast(
        { seamHit.point.x, seamHit.point.y + 2.0f, seamHit.point.z },
        0.30f,
        { 0.0f, -1.0f, 0.0f },
        4.0f,
        {},
        queryBodies,
        staticSphereHit);
    if (!staticSphereCastWorked
        || staticSphereHit.body != heritage::physics::InvalidBody
        || staticSphereHit.distance < 1.60f
        || staticSphereHit.distance > 1.80f)
    {
        std::cerr << "Static-triangle swept-sphere camera query regression.\n";
        return 1;
    }
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
            world.surfaces,
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

bool staticTriangleRigidBodyContactsSettle()
{
    auto runCase = [](bool sphere) {
        RigidBodySystem bodies;
        CollisionSystem collisions;

        StaticSceneTriangle first;
        first.a = { -5.0f, 0.0f, -5.0f };
        first.b = { -5.0f, 0.0f, 5.0f };
        first.c = { 5.0f, 0.0f, 5.0f };
        first.normal = { 0.0f, 1.0f, 0.0f };
        first.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;

        StaticSceneTriangle second;
        second.a = { -5.0f, 0.0f, -5.0f };
        second.b = { 5.0f, 0.0f, 5.0f };
        second.c = { 5.0f, 0.0f, -5.0f };
        second.normal = { 0.0f, 1.0f, 0.0f };
        second.surfaceMaterial = heritage::physics::SurfaceMaterial::Asphalt;
        collisions.setStaticSceneTriangles({ first, second });

        RigidBodyDescription description;
        description.motionType = BodyMotionType::Dynamic;
        description.position = sphere
            ? Vec3{ -1.0f, 2.0f, 0.0f }
            : Vec3{ 1.0f, 2.0f, 0.0f };
        description.mass = 50.0f;
        description.gravityFactor = 1.0f;
        description.linearDamping = 0.02f;
        description.angularDamping = 0.05f;
        const BodyHandle body = bodies.create(description);
        if (body == heritage::physics::InvalidBody)
            return false;

        const ColliderHandle collider = sphere
            ? collisions.createSphere(
                body, 0.5f, {}, 0.90f, 0.0f, false, bodies)
            : collisions.createBox(
                body, { 0.5f, 0.5f, 0.5f }, {},
                0.90f, 0.0f, false, bodies);
        if (collider == heritage::physics::InvalidCollider)
            return false;

        bool observedStaticContact = false;
        for (int step = 0; step < 600; ++step)
        {
            bodies.integrate(kWorldDeltaTime, kGravity);
            collisions.simulate(bodies, kWorldDeltaTime);
            observedStaticContact = observedStaticContact
                || collisions.staticTriangleContactCount() > 0;
        }

        RigidBodyPose pose;
        Vec3 linearVelocity{};
        if (!bodies.pose(body, pose)
            || !bodies.linearVelocity(body, linearVelocity))
        {
            return false;
        }

        const bool restingHeight = pose.position.y >= 0.45f
            && pose.position.y <= 0.65f;
        const bool verticallyQuiet = std::abs(linearVelocity.y) <= 0.20f;
        const bool touching = collisions.contactCountForBody(body) > 0;

        std::cout
            << (sphere ? "static_triangle_sphere" : "static_triangle_box")
            << " y=" << pose.position.y
            << " vy=" << linearVelocity.y
            << " contacts=" << collisions.contactCountForBody(body)
            << " static_contacts=" << collisions.staticTriangleContactCount()
            << '\n';

        return observedStaticContact
            && restingHeight
            && verticallyQuiet
            && touching;
    };

    return runCase(true) && runCase(false);
}

} // namespace heritage::tests
