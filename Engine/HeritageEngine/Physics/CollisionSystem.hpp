#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Core/Math/Math.hpp"
#include "RigidBodySystem.hpp"
#include "StaticTriangleBvh.hpp"
#include "Surfaces/SurfaceMaterialProperties.hpp"

namespace heritage::physics {

using ColliderHandle = std::uint64_t;
inline constexpr ColliderHandle InvalidCollider = 0;

enum class ColliderShapeType
{
    Sphere,
    Box
};

// Generic world-surface identity carried by colliders and collision queries.
// Vehicle tires may interpret these materials differently, but the collision
// system deliberately does not contain tire-specific force laws.
enum class SurfaceMaterial
{
    Default = 0,
    Asphalt = 1,
    Gravel = 2,
    Dirt = 3,
    Grass = 4,
    Snow = 5,
    Ice = 6,
    Kerb = 7,
    PaintedLine = 8,
    Mud = 9,
    Sand = 10,
    SoftSoil = 11,
    DeepSnow = 12
};

struct StaticSceneTriangle
{
    heritage::math::Vec3 a{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 b{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 c{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    SurfaceMaterialProperties surfaceProperties{};
};

struct ColliderDescription
{
    BodyHandle body = InvalidBody;
    ColliderShapeType shapeType = ColliderShapeType::Sphere;
    heritage::math::Vec3 localPosition{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
    float radius = 0.5f;
    float friction = 0.75f;
    float restitution = 0.15f;
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    bool trigger = false;
    std::uint32_t layer = 1u;
    std::uint32_t mask = 0xffffffffu;
};

// A contact is generated during the most recently completed fixed world step.
// normal points from collider A toward collider B. Trigger contacts are
// reported but do not apply positional or velocity response.
struct CollisionContact
{
    ColliderHandle colliderA = InvalidCollider;
    ColliderHandle colliderB = InvalidCollider;
    BodyHandle bodyA = InvalidBody;
    BodyHandle bodyB = InvalidBody;
    heritage::math::Vec3 point{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    float penetration = 0.0f;
    bool trigger = false;

    // Solver state retained for warm starting during this fixed step and cached
    // for the following step when the same contact remains coherent.
    heritage::math::Vec3 tangent{ 1.0f, 0.0f, 0.0f };
    float accumulatedNormalImpulse = 0.0f;
    float accumulatedTangentImpulse = 0.0f;
    bool warmStarted = false;
};

struct CollisionQueryFilter
{
    // A query sees a collider when at least one collider layer bit appears in
    // this mask. Query filtering is intentionally independent from pairwise
    // collider masks because a ray or overlap volume is not itself a collider.
    std::uint32_t layerMask = 0xffffffffu;
    bool includeTriggers = false;
    BodyHandle ignoredBody = InvalidBody;
};

struct RaycastHit
{
    ColliderHandle collider = InvalidCollider;
    BodyHandle body = InvalidBody;
    heritage::math::Vec3 point{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    float distance = 0.0f;
    float fraction = 0.0f;
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    SurfaceMaterialProperties surfaceProperties{};
    bool trigger = false;
};

// Read-only accounting for the most recently completed closest-hit raycast.
// Vehicle contact diagnostics copy this immediately after their support query;
// it is diagnostic state only and never changes the query result.
struct RaycastQueryDiagnostics
{
    std::size_t colliderCandidateCount = 0;
    std::size_t staticTriangleCandidateCount = 0;
    std::size_t staticBvhNodeTestCount = 0;
    std::size_t exactTestCount = 0;
    bool staticSceneLoaded = false;
    bool originInsideStaticSceneHorizontalBounds = false;
    bool rayBoundsOverlapStaticScene = false;
    bool staticTriangleHit = false;
    bool selectedHitWasStaticTriangle = false;
};

// Closest hit produced by sweeping a sphere through the world. point is the
// contact point on the swept sphere at impact, while normal points outward
// from the hit collider toward the swept sphere centre.
struct SphereCastHit
{
    ColliderHandle collider = InvalidCollider;
    BodyHandle body = InvalidBody;
    heritage::math::Vec3 point{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
    float distance = 0.0f;
    float fraction = 0.0f;
    SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
    float surfaceWetness = 0.0f;
    SurfaceMaterialProperties surfaceProperties{};
    bool trigger = false;
};

// Generation-checked primitive collision shapes, deterministic sweep-and-prune
// broadphase and narrowphase contact generation. Step 28E adds a persistent
// warm-start cache, dynamic-body simulation islands and sleep/wake propagation.
// Step 28F adds read-only raycasts and sphere-overlap scene queries used by
// suspension, tires, cameras, traffic sensors, AI and gameplay scripts.
// Step 28G adds swept-sphere queries plus opt-in continuous collision
// protection for fast centred sphere bodies against static and kinematic
// world geometry.
class CollisionSystem
{
public:
    void clear();

    ColliderHandle create(
        const ColliderDescription& description,
        const RigidBodySystem& bodies);
    ColliderHandle createSphere(
        BodyHandle body,
        float radius,
        const heritage::math::Vec3& localPosition,
        float friction,
        float restitution,
        bool trigger,
        const RigidBodySystem& bodies);
    ColliderHandle createBox(
        BodyHandle body,
        const heritage::math::Vec3& halfExtents,
        const heritage::math::Vec3& localPosition,
        float friction,
        float restitution,
        bool trigger,
        const RigidBodySystem& bodies);

    bool destroy(ColliderHandle handle);
    bool exists(ColliderHandle handle) const;
    std::size_t count() const { return m_aliveCount; }
    std::size_t countForBody(BodyHandle body) const;
    void destroyForBody(BodyHandle body);
    void removeInvalidBodies(const RigidBodySystem& bodies);

    bool shapeType(ColliderHandle handle, ColliderShapeType& value) const;
    BodyHandle body(ColliderHandle handle) const;
    bool setMaterial(ColliderHandle handle, float friction, float restitution);
    bool setSurface(
        ColliderHandle handle,
        SurfaceMaterial material,
        float wetness = 0.0f);
    bool surface(
        ColliderHandle handle,
        SurfaceMaterial& material,
        float& wetness) const;
    bool setTrigger(ColliderHandle handle, bool trigger);
    bool setFilter(ColliderHandle handle, std::uint32_t layer, std::uint32_t mask);

    // Exact closest-hit raycasts against sphere and oriented-box colliders.
    // Direction does not need to be normalized. Queries never wake bodies or
    // otherwise mutate simulation state.
    bool raycast(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& direction,
        float maximumDistance,
        const CollisionQueryFilter& filter,
        const RigidBodySystem& bodies,
        RaycastHit& hit) const;
    bool raycastAny(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& direction,
        float maximumDistance,
        const CollisionQueryFilter& filter,
        const RigidBodySystem& bodies) const;

    // Sweeps a sphere against sphere/oriented-box colliders and the creator
    // static-triangle scene. Primitive targets use the existing exact/conservative
    // narrowphase; static triangles use conservative advancement against exact
    // point-to-triangle distance so camera/AI probes also see authored terrain.
    bool sphereCast(
        const heritage::math::Vec3& origin,
        float radius,
        const heritage::math::Vec3& direction,
        float maximumDistance,
        const CollisionQueryFilter& filter,
        const RigidBodySystem& bodies,
        SphereCastHit& hit) const;
    bool sphereCastAny(
        const heritage::math::Vec3& origin,
        float radius,
        const heritage::math::Vec3& direction,
        float maximumDistance,
        const CollisionQueryFilter& filter,
        const RigidBodySystem& bodies) const;

    // Counts exact sphere overlaps against sphere and oriented-box colliders.
    std::size_t overlapSphereCount(
        const heritage::math::Vec3& center,
        float radius,
        const CollisionQueryFilter& filter,
        const RigidBodySystem& bodies) const;

    std::size_t lastQueryCandidateCount() const { return m_lastQueryCandidateCount; }
    std::size_t lastQueryExactTestCount() const { return m_lastQueryExactTestCount; }
    const RaycastQueryDiagnostics& lastRaycastDiagnostics() const
    {
        return m_lastRaycastDiagnostics;
    }

    // Immutable creator/world static-triangle scene. The same BVH-backed data
    // participates in ray/sphere queries and dynamic primitive-vs-static-triangle
    // rigid-body contacts. It is not a general mesh ColliderHandle: dynamic
    // triangle meshes, mesh-vs-mesh contact and deformable meshes remain outside
    // this contract.
    void setStaticSceneTriangles(std::vector<StaticSceneTriangle> triangles);
    void clearStaticSceneTriangles();

    // Floating-origin support for creator-authored immutable triangle worlds.
    // Primitive colliders follow their rigid bodies; only detached static-scene
    // triangles need explicit translation and BVH rebuild.
    void rebaseLocalOrigin(const heritage::math::Vec3& shift);
    std::size_t staticSceneTriangleCount() const { return m_staticSceneTriangles.size(); }
    bool staticSceneTriangle(std::uint32_t triangleIndex, StaticSceneTriangle& triangle) const;
    void nearbyStaticSceneTriangles(
        const heritage::math::Vec3& center,
        float halfExtent,
        std::size_t maximumTriangles,
        std::vector<StaticSceneTriangle>& triangles) const;
    std::size_t staticSceneBvhNodeCount() const
    {
        return m_staticTriangleBvh.nodeCount();
    }
    std::size_t staticSceneBvhLeafCount() const
    {
        return m_staticTriangleBvh.leafCount();
    }
    std::size_t staticSceneBvhMaximumDepth() const
    {
        return m_staticTriangleBvh.maximumDepth();
    }

    void simulate(RigidBodySystem& bodies, float fixedDeltaTime);

    const std::vector<CollisionContact>& contacts() const { return m_contacts; }
    std::size_t contactCount() const { return m_contacts.size(); }
    std::size_t contactCountForBody(BodyHandle body) const;
    bool bodyTouching(BodyHandle body) const;

    std::size_t broadphaseCandidateCount() const { return m_broadphaseCandidateCount; }
    std::size_t narrowphaseTestCount() const { return m_narrowphaseTestCount; }
    std::size_t resolvedContactCount() const { return m_resolvedContactCount; }
    int velocitySolverIterations() const { return m_velocitySolverIterations; }
    std::size_t simulationIslandCount() const { return m_simulationIslandCount; }
    std::size_t activeIslandCount() const { return m_activeIslandCount; }
    std::size_t sleepingIslandCount() const { return m_sleepingIslandCount; }
    std::size_t warmStartedContactCount() const { return m_warmStartedContactCount; }
    std::size_t persistentContactCount() const { return m_contactCache.size(); }
    std::size_t staticBroadphaseNodeTestCount() const
    {
        return m_staticBroadphaseNodeTestCount;
    }
    std::size_t staticTriangleCandidateCount() const
    {
        return m_staticTriangleCandidateCount;
    }
    std::size_t staticTriangleNarrowphaseTestCount() const
    {
        return m_staticTriangleNarrowphaseTestCount;
    }
    std::size_t staticTriangleContactCount() const
    {
        return m_staticTriangleContactCount;
    }

    // Statistics from the most recently completed fixed world step.
    std::size_t continuousCollisionBodyCount() const { return m_continuousCollisionBodyCount; }
    std::size_t continuousCollisionSweepCount() const { return m_continuousCollisionSweepCount; }
    std::size_t continuousCollisionHitCount() const { return m_continuousCollisionHitCount; }
    std::size_t continuousCollisionClampedBodyCount() const { return m_continuousCollisionClampedBodyCount; }
    std::size_t continuousCollisionUnsupportedBodyCount() const { return m_continuousCollisionUnsupportedBodyCount; }

    const std::string& lastError() const { return m_lastError; }

private:
    struct Record
    {
        BodyHandle body = InvalidBody;
        ColliderShapeType shapeType = ColliderShapeType::Sphere;
        heritage::math::Vec3 localPosition{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f };
        float radius = 0.5f;
        float friction = 0.75f;
        float restitution = 0.15f;
        SurfaceMaterial surfaceMaterial = SurfaceMaterial::Default;
        float surfaceWetness = 0.0f;
        bool trigger = false;
        std::uint32_t layer = 1u;
        std::uint32_t mask = 0xffffffffu;
    };

    struct Slot
    {
        std::uint32_t generation = 1;
        bool alive = false;
        Record record;
    };

    struct Aabb
    {
        heritage::math::Vec3 minimum{ 0.0f, 0.0f, 0.0f };
        heritage::math::Vec3 maximum{ 0.0f, 0.0f, 0.0f };
    };

    struct BroadphaseProxy
    {
        std::uint32_t slotIndex = 0;
        ColliderHandle handle = InvalidCollider;
        Aabb bounds;
    };

    struct ContactPairKey
    {
        ColliderHandle first = InvalidCollider;
        ColliderHandle second = InvalidCollider;

        bool operator==(const ContactPairKey& other) const
        {
            return first == other.first && second == other.second;
        }
    };

    struct ContactPairKeyHash
    {
        std::size_t operator()(const ContactPairKey& value) const;
    };

    struct CachedContact
    {
        heritage::math::Vec3 point{};
        heritage::math::Vec3 normal{ 0.0f, 1.0f, 0.0f };
        heritage::math::Vec3 tangent{ 1.0f, 0.0f, 0.0f };
        float normalImpulse = 0.0f;
        float tangentImpulse = 0.0f;
        std::uint64_t lastSeenStep = 0;
    };

    static RigidBodySystem::Quaternion conjugateRotation(
        const RigidBodySystem::Quaternion& value);
    static heritage::math::Vec3 rotateVector(
        const RigidBodySystem::Quaternion& rotation,
        const heritage::math::Vec3& value);
    static bool aabbOverlap(const Aabb& left, const Aabb& right);
    static float inverseMassForContact(const RigidBodySystem::Record& body);
    static heritage::math::Vec3 angularVelocityRadians(
        const RigidBodySystem::Record& body);
    static heritage::math::Vec3 pointVelocity(
        const RigidBodySystem::Record& body,
        const heritage::math::Vec3& worldPoint);
    static heritage::math::Vec3 supportPoint(
        const heritage::math::Vec3& center,
        const heritage::math::Vec3 axes[3],
        const heritage::math::Vec3& halfExtents,
        const heritage::math::Vec3& direction);

    static ColliderHandle makeHandle(std::uint32_t index, std::uint32_t generation);
    static bool decodeHandle(
        ColliderHandle handle,
        std::uint32_t& index,
        std::uint32_t& generation);

    Slot* resolve(ColliderHandle handle);
    const Slot* resolve(ColliderHandle handle) const;
    bool destroyResolved(std::uint32_t index, Slot& slot);

    Aabb worldAabb(
        const Record& collider,
        const RigidBodySystem::Record& body) const;
    heritage::math::Vec3 worldCenter(
        const Record& collider,
        const RigidBodySystem::Record& body) const;
    bool queryAllows(
        const Record& collider,
        const CollisionQueryFilter& filter) const;
    bool raySphere(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        const Record& collider,
        const RigidBodySystem::Record& body,
        RaycastHit& hit) const;
    bool rayBox(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        const Record& collider,
        const RigidBodySystem::Record& body,
        RaycastHit& hit) const;
    bool sphereCastSphere(
        const heritage::math::Vec3& origin,
        float castRadius,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        const Record& collider,
        const RigidBodySystem::Record& body,
        SphereCastHit& hit) const;
    bool sphereCastBox(
        const heritage::math::Vec3& origin,
        float castRadius,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        const Record& collider,
        const RigidBodySystem::Record& body,
        SphereCastHit& hit) const;
    bool sphereCastCollider(
        const heritage::math::Vec3& origin,
        float castRadius,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        ColliderHandle colliderHandle,
        const Record& collider,
        const RigidBodySystem::Record& body,
        SphereCastHit& hit) const;
    bool sphereCastStaticSceneTriangle(
        const heritage::math::Vec3& origin,
        float castRadius,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        std::uint32_t triangleIndex,
        SphereCastHit& hit) const;
    bool sphereOverlapsCollider(
        const heritage::math::Vec3& center,
        float radius,
        const Record& collider,
        const RigidBodySystem::Record& body) const;
    bool rayStaticSceneTriangle(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        const StaticSceneTriangle& triangle,
        RaycastHit& hit) const;
    void queryStaticSceneTriangles(
        const Aabb& bounds,
        std::vector<std::uint32_t>& triangleIndices,
        std::size_t& nodeTestCount) const;
    bool raycastStaticSceneOnly(
        const heritage::math::Vec3& origin,
        const heritage::math::Vec3& normalizedDirection,
        float maximumDistance,
        RaycastHit& hit,
        std::uint32_t& triangleIndex,
        std::size_t& nodeTestCount) const;
    bool sphereStaticTriangleContact(
        ColliderHandle sphereHandle,
        const Record& sphere,
        const RigidBodySystem::Record& sphereBody,
        std::uint32_t triangleIndex,
        CollisionContact& contact) const;
    bool boxStaticTriangleContact(
        ColliderHandle boxHandle,
        const Record& box,
        const RigidBodySystem::Record& boxBody,
        std::uint32_t triangleIndex,
        CollisionContact& contact) const;
    bool generateStaticTriangleContact(
        ColliderHandle colliderHandle,
        const Record& collider,
        const RigidBodySystem::Record& body,
        std::uint32_t triangleIndex,
        CollisionContact& contact) const;
    static ColliderHandle staticTriangleColliderHandle(
        std::uint32_t triangleIndex);
    static bool staticTriangleIndexFromColliderHandle(
        ColliderHandle handle,
        std::uint32_t& triangleIndex);

    void collectBroadphaseContacts(RigidBodySystem& bodies);

    bool generateContact(
        ColliderHandle handleA,
        const Record& colliderA,
        const RigidBodySystem::Record& bodyA,
        ColliderHandle handleB,
        const Record& colliderB,
        const RigidBodySystem::Record& bodyB,
        CollisionContact& contact) const;
    bool sphereSphereContact(
        ColliderHandle handleA,
        const Record& colliderA,
        const RigidBodySystem::Record& bodyA,
        ColliderHandle handleB,
        const Record& colliderB,
        const RigidBodySystem::Record& bodyB,
        CollisionContact& contact) const;
    bool sphereBoxContact(
        ColliderHandle sphereHandle,
        const Record& sphere,
        const RigidBodySystem::Record& sphereBody,
        ColliderHandle boxHandle,
        const Record& box,
        const RigidBodySystem::Record& boxBody,
        CollisionContact& contact) const;
    float centredSphereRadiusForContinuousCollision(
        BodyHandle body,
        const RigidBodySystem& bodies,
        const Record*& sourceCollider) const;
    void applyContinuousCollisionDetection(
        RigidBodySystem& bodies,
        float fixedDeltaTime);

    bool boxBoxContact(
        ColliderHandle handleA,
        const Record& colliderA,
        const RigidBodySystem::Record& bodyA,
        ColliderHandle handleB,
        const Record& colliderB,
        const RigidBodySystem::Record& bodyB,
        CollisionContact& contact) const;

    void rebuildMassProperties(RigidBodySystem& bodies);
    void resolvePosition(
        RigidBodySystem::Record& bodyA,
        RigidBodySystem::Record& bodyB,
        const CollisionContact& contact);
    void resolveVelocity(
        const Record& colliderA,
        RigidBodySystem::Record& bodyA,
        const Record& colliderB,
        RigidBodySystem::Record& bodyB,
        CollisionContact& contact);
    static ContactPairKey contactPairKey(const CollisionContact& contact);
    static void canonicalizeContact(CollisionContact& contact);
    void restoreCachedImpulse(CollisionContact& contact);
    void warmStartContact(
        RigidBodySystem::Record& bodyA,
        RigidBodySystem::Record& bodyB,
        CollisionContact& contact);
    void resolveStaticPosition(
        RigidBodySystem::Record& body,
        const CollisionContact& contact);
    void warmStartStaticContact(
        RigidBodySystem::Record& body,
        CollisionContact& contact);
    void resolveStaticVelocity(
        const Record& collider,
        const StaticSceneTriangle& triangle,
        RigidBodySystem::Record& body,
        CollisionContact& contact);
    float staticEffectiveMassDenominator(
        const RigidBodySystem::Record& body,
        const heritage::math::Vec3& point,
        const heritage::math::Vec3& direction) const;
    void persistContactCache();
    void updateSimulationIslandsAndSleeping(
        RigidBodySystem& bodies,
        float fixedDeltaTime,
        bool finalizeSleep);
    float effectiveMassDenominator(
        const RigidBodySystem::Record& bodyA,
        const RigidBodySystem::Record& bodyB,
        const heritage::math::Vec3& point,
        const heritage::math::Vec3& direction) const;

    void setError(const std::string& message) const;
    void clearError() const;

    std::vector<Slot> m_slots;
    std::vector<StaticSceneTriangle> m_staticSceneTriangles;
    StaticTriangleBvh m_staticTriangleBvh;
    Aabb m_staticSceneBounds;
    bool m_staticSceneBoundsValid = false;
    std::vector<std::uint32_t> m_freeIndices;
    std::vector<CollisionContact> m_contacts;
    std::size_t m_aliveCount = 0;
    std::size_t m_broadphaseCandidateCount = 0;
    std::size_t m_narrowphaseTestCount = 0;
    std::size_t m_resolvedContactCount = 0;
    std::size_t m_simulationIslandCount = 0;
    std::size_t m_activeIslandCount = 0;
    std::size_t m_sleepingIslandCount = 0;
    std::size_t m_warmStartedContactCount = 0;
    std::size_t m_staticBroadphaseNodeTestCount = 0;
    std::size_t m_staticTriangleCandidateCount = 0;
    std::size_t m_staticTriangleNarrowphaseTestCount = 0;
    std::size_t m_staticTriangleContactCount = 0;
    std::size_t m_continuousCollisionBodyCount = 0;
    std::size_t m_continuousCollisionSweepCount = 0;
    std::size_t m_continuousCollisionHitCount = 0;
    std::size_t m_continuousCollisionClampedBodyCount = 0;
    std::size_t m_continuousCollisionUnsupportedBodyCount = 0;
    mutable std::size_t m_lastQueryCandidateCount = 0;
    mutable std::size_t m_lastQueryExactTestCount = 0;
    mutable RaycastQueryDiagnostics m_lastRaycastDiagnostics;
    std::uint64_t m_simulationSequence = 0;
    std::unordered_map<ContactPairKey, CachedContact, ContactPairKeyHash> m_contactCache;
    std::uint64_t m_topologyRevision = 1;
    std::uint64_t m_cachedTopologyRevision = 0;
    std::uint64_t m_cachedBodyMassRevision = 0;
    int m_velocitySolverIterations = 8;
    mutable std::string m_lastError;
};

const char* colliderShapeTypeName(ColliderShapeType value);
const char* surfaceMaterialName(SurfaceMaterial value);
bool parseSurfaceMaterial(const std::string& text, SurfaceMaterial& value);

} // namespace heritage::physics
