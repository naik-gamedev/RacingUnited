#include "EngineSimulation.hpp"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Diagnostics/PerformanceMonitor.hpp"
#include "../../Core/Entities/EntityRegistry.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Modules/ModuleRuntimeManager.hpp"
#include "../../Graphics/EnvironmentSystem.hpp"
#include "../../Physics/PhysicsWorld.hpp"

namespace heritage::engine {

void updateEngineSimulation(
    float dt,
    double now,
    bool menuOpen,
    heritage::physics::PhysicsWorld& physics,
    heritage::modules::ModuleRuntimeManager& moduleRuntime,
    heritage::graphics::EnvironmentSystem& environmentSystem,
    heritage::entities::EntityRegistry& entityRegistry,
    heritage::camera::ChaseCamera& chaseCamera,
    heritage::camera::CameraFrame& entityCameraFrame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor)
{
    using heritage::diagnostics::PerformanceSection;
    using heritage::math::Vec3;

    // Physics owns a deterministic fixed-step clock independent from the
    // rendered frame rate. Module fixed updates, rigid bodies, contacts and
    // future vehicle systems all execute from this world clock.
    const double physicsCpuStart = glfwGetTime();
    physics.advance(
        static_cast<double>(dt),
        !menuOpen,
        [&](float fixedDeltaTime)
        {
            moduleRuntime.fixedUpdate(fixedDeltaTime);
        });
    performanceMonitor.recordSection(
        PerformanceSection::Physics,
        (glfwGetTime() - physicsCpuStart) * 1000.0);

    // TIRE15B2 transient spray/dust/debris ages on simulation time rather
    // than wall-clock time, so pause/single-step/time-scale remain coherent.
    const float surfacePresentationDelta =
        static_cast<float>(physics.lastWorldStepCount()) * physics.fixedDeltaTime();
    physics.surfaces().advancePresentation(surfacePresentationDelta);

    const double gameUpdateCpuStart = glfwGetTime();
    moduleRuntime.update(dt, !menuOpen);
    environmentSystem.update(menuOpen ? 0.0f : dt);

    // Rigid bodies remain on the deterministic fixed clock. Only their
    // interpolated poses are copied to bound entities for this render.
    physics.synchronizeEntityTransforms(entityRegistry);

    // CAM01: one camera simulation update per rendered frame. Rendering may
    // happen once per monitor, so chase-camera springs must live here rather
    // than inside an individual renderer draw call.
    entityCameraFrame = {};
    const heritage::entities::EntityHandle cameraPlayer =
        entityRegistry.findByName("Player Vehicle Root");
    // CAM01A: a driveable player entity is sufficient to own the chase
    // camera. Do NOT gate it on the scene-visual entity: GLB hot reload,
    // world reload, or a transient visual-entity rebuild used to kick the
    // camera into the old time-based orbit, which could put it anywhere
    // around the car including directly in front.
    const bool playerChaseCameraActive =
        cameraPlayer != heritage::entities::InvalidEntity;

    if (playerChaseCameraActive)
    {
        // CAM03: use the interpolated rigid-body quaternion basis directly.
        // The chase camera therefore follows the chassis' ACTUAL +Z forward
        // direction rather than extracting an Euler-Y angle. This removes
        // +/-180 wrap/gimbal ambiguity and keeps roundabouts/spins rear-safe.
        const heritage::physics::BodyHandle cameraBody =
            physics.rigidBodies().bodyForEntity(cameraPlayer);
        heritage::physics::RigidBodyPose cameraPose{};
        Vec3 cameraRightWorld{ 1.0f, 0.0f, 0.0f };
        Vec3 cameraUpWorld{ 0.0f, 1.0f, 0.0f };
        Vec3 cameraForwardWorld{ 0.0f, 0.0f, 1.0f };

        const bool havePhysicsCameraPose =
            cameraBody != heritage::physics::InvalidBody
            && physics.rigidBodies().interpolatedPose(
                cameraBody,
                physics.interpolationAlpha(),
                cameraPose)
            && physics.rigidBodies().interpolatedBasis(
                cameraBody,
                physics.interpolationAlpha(),
                cameraRightWorld,
                cameraUpWorld,
                cameraForwardWorld);

        bool haveCameraPose = havePhysicsCameraPose;
        if (!haveCameraPose)
        {
            // Non-physics/module fallback only. Racing United's player car
            // should always take the quaternion-basis path above.
            haveCameraPose =
                entityRegistry.worldPosition(cameraPlayer, cameraPose.position)
                && entityRegistry.worldRotationDegrees(
                    cameraPlayer, cameraPose.rotationDegrees);
            if (haveCameraPose)
            {
                constexpr float kPiF = 3.14159265358979323846f;
                const float yaw =
                    cameraPose.rotationDegrees.y * (kPiF / 180.0f);
                cameraForwardWorld = {
                    std::sin(yaw),
                    0.0f,
                    std::cos(yaw)
                };
            }
        }

        if (haveCameraPose)
        {
            const heritage::math::DVec3 globalPlayerPosition =
                physics.localToGlobal(cameraPose.position);
            chaseCamera.update(
                globalPlayerPosition,
                cameraForwardWorld,
                dt);

            // CAM03 collision: ray from a point above the chassis toward
            // the DESIRED full-distance eye. Static scene triangles are
            // included by CollisionSystem::raycast, so ditches, hills and
            // walls pull the camera inward instead of letting it pass
            // through terrain. The player chassis body is explicitly ignored.
            Vec3 collisionAnchorLocal{};
            Vec3 desiredEyeLocal{};
            double collisionDistanceLimit = 1000000.0;
            if (physics.globalToLocal(
                    chaseCamera.collisionAnchorGlobal(),
                    collisionAnchorLocal)
                && physics.globalToLocal(
                    chaseCamera.desiredEyeGlobal(),
                    desiredEyeLocal))
            {
                const Vec3 collisionRay{
                    desiredEyeLocal.x - collisionAnchorLocal.x,
                    desiredEyeLocal.y - collisionAnchorLocal.y,
                    desiredEyeLocal.z - collisionAnchorLocal.z
                };
                const float probeDistance = std::sqrt(
                    collisionRay.x * collisionRay.x
                    + collisionRay.y * collisionRay.y
                    + collisionRay.z * collisionRay.z);
                collisionDistanceLimit = static_cast<double>(probeDistance);
                if (probeDistance > 0.0001f)
                {
                    heritage::physics::CollisionQueryFilter filter{};
                    filter.includeTriggers = false;
                    filter.ignoredBody = cameraBody;
                    heritage::physics::RaycastHit cameraHit{};
                    if (physics.collisions().raycast(
                            collisionAnchorLocal,
                            collisionRay,
                            probeDistance,
                            filter,
                            physics.rigidBodies(),
                            cameraHit))
                    {
                        collisionDistanceLimit = std::max(
                            chaseCamera.tuning().minimumCollisionDistanceMeters,
                            static_cast<double>(cameraHit.distance)
                                - chaseCamera.tuning().collisionPaddingMeters);
                    }
                }
            }

            chaseCamera.resolveCollisionDistance(
                collisionDistanceLimit,
                dt);
            chaseCamera.buildLocalFrame(
                physics.globalOrigin(),
                entityCameraFrame);
        }
    }
    else
    {
        chaseCamera.reset();
    }

    // Only scenes with NO player vehicle use the old showroom orbit. If
    // player-camera state is temporarily invalid, use a deterministic rear
    // view derived from the rigid body's forward basis instead of orbiting.
    if (!entityCameraFrame.valid)
    {
        Vec3 cameraTarget{ 0.0f, 1.0f, 0.0f };
        heritage::physics::RigidBodyPose fallbackPose{};
        Vec3 fallbackForward{ 0.0f, 0.0f, 1.0f };
        bool havePlayerPose = false;
        if (cameraPlayer != heritage::entities::InvalidEntity)
        {
            const heritage::physics::BodyHandle cameraBody =
                physics.rigidBodies().bodyForEntity(cameraPlayer);
            Vec3 fallbackRight{};
            Vec3 fallbackUp{};
            havePlayerPose = cameraBody != heritage::physics::InvalidBody
                && physics.rigidBodies().interpolatedPose(
                    cameraBody,
                    physics.interpolationAlpha(),
                    fallbackPose)
                && physics.rigidBodies().interpolatedBasis(
                    cameraBody,
                    physics.interpolationAlpha(),
                    fallbackRight,
                    fallbackUp,
                    fallbackForward);

            if (!havePlayerPose)
            {
                havePlayerPose =
                    entityRegistry.worldPosition(
                        cameraPlayer, fallbackPose.position)
                    && entityRegistry.worldRotationDegrees(
                        cameraPlayer, fallbackPose.rotationDegrees);
                if (havePlayerPose)
                {
                    constexpr float kPiF = 3.14159265358979323846f;
                    const float yaw =
                        fallbackPose.rotationDegrees.y * (kPiF / 180.0f);
                    fallbackForward = {
                        std::sin(yaw),
                        0.0f,
                        std::cos(yaw)
                    };
                }
            }
        }

        if (havePlayerPose)
        {
            const float horizontalLength = std::sqrt(
                fallbackForward.x * fallbackForward.x
                + fallbackForward.z * fallbackForward.z);
            if (horizontalLength > 0.0001f)
            {
                fallbackForward.x /= horizontalLength;
                fallbackForward.y = 0.0f;
                fallbackForward.z /= horizontalLength;
            }
            else
            {
                fallbackForward = { 0.0f, 0.0f, 1.0f };
            }

            cameraTarget = {
                fallbackPose.position.x + fallbackForward.x * 2.0f,
                fallbackPose.position.y + 0.85f,
                fallbackPose.position.z + fallbackForward.z * 2.0f
            };
            entityCameraFrame.eyeLocal = {
                fallbackPose.position.x - fallbackForward.x * 6.6f,
                fallbackPose.position.y + 2.70f,
                fallbackPose.position.z - fallbackForward.z * 6.6f
            };
        }
        else
        {
            const float orbitAngle = static_cast<float>(now) * 0.18f;
            entityCameraFrame.eyeLocal = {
                cameraTarget.x + std::sin(orbitAngle) * 8.5f,
                cameraTarget.y + 3.4f,
                cameraTarget.z + std::cos(orbitAngle) * 8.5f
            };
        }

        entityCameraFrame.targetLocal = cameraTarget;
        entityCameraFrame.up = { 0.0f, 1.0f, 0.0f };
        entityCameraFrame.valid = true;
    }

    performanceMonitor.recordSection(
        PerformanceSection::GameUpdate,
        (glfwGetTime() - gameUpdateCpuStart) * 1000.0);
}

} // namespace heritage::engine
