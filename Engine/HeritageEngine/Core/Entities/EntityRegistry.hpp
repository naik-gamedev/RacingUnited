#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "../Math/Math.hpp"
#include "../Math/Quaternion.hpp"

namespace heritage::entities {

using EntityHandle = std::uint64_t;
inline constexpr EntityHandle InvalidEntity = 0;

// The direct collision lattice is input to one flexible-ring solve.  It never
// reaches the renderer as a second deformation authority.
inline constexpr std::size_t TireVisualContactSampleStations = 21;
inline constexpr std::size_t TireVisualContactSampleBands = 13;
inline constexpr std::size_t TireVisualContactSampleCount =
    TireVisualContactSampleStations * TireVisualContactSampleBands;

// The solver's sole final presentation output covers the complete belt.
inline constexpr std::size_t TireVisualDeformationFieldStations = 24;
inline constexpr std::size_t TireVisualDeformationFieldBands = 13;
inline constexpr std::size_t TireVisualDeformationFieldCount =
    TireVisualDeformationFieldStations * TireVisualDeformationFieldBands;

inline constexpr std::array<float, TireVisualContactSampleStations>
    TireVisualContactSamplePhiRadians{
        0.0f,
        0.392699082f,  // 22.5 deg
        0.698131701f,  // 40
        0.959931089f,  // 55
        1.134464014f,  // 65
        1.265363708f,  // 72.5
        1.361356817f,  // 78
        1.439896633f,  // 82.5
        1.500983157f,  // 86
        1.544616389f,  // 88.5
        1.570796327f,  // 90: straight bottom
        1.596976265f,  // 91.5
        1.640609496f,  // 94
        1.701696021f,  // 97.5
        1.780235837f,  // 102
        1.876228945f,  // 107.5
        2.007128640f,  // 115
        2.181661565f,  // 125
        2.443460953f,  // 140
        2.748893572f,  // 157.5
        3.141592654f   // 180: rear equator
    };

inline constexpr std::array<float, TireVisualContactSampleBands>
    TireVisualContactSampleWidthCoordinates{
        -1.00f, -0.82f, -0.65f, -0.49f, -0.34f, -0.18f, 0.00f,
         0.18f,  0.34f,  0.49f,  0.65f,  0.82f, 1.00f
    };
enum class DebugPrimitiveType
{
    Box,
    Cylinder,
    Sphere
};

struct DebugPrimitiveComponent
{
    DebugPrimitiveType type = DebugPrimitiveType::Box;
    heritage::math::Vec3 color{ 0.65f, 0.72f, 0.82f };
    bool visible = true;
};

struct DebugPrimitiveInstance
{
    EntityHandle entity = InvalidEntity;
    DebugPrimitiveType type = DebugPrimitiveType::Box;
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
    heritage::math::Vec3 color{ 0.65f, 0.72f, 0.82f };
};



struct MeshNodeOverride
{
    std::string nodeName;
    bool hasWorldPose = false;
    heritage::math::Vec3 worldPosition{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 worldRotationDegrees{ 0.0f, 0.0f, 0.0f };
    bool hasLocalRotationOffset = false;
    heritage::math::Vec3 localRotationOffsetDegrees{ 0.0f, 0.0f, 0.0f };

    // Anchored subtree pose: move/rotate this node's complete subtree so the
    // named descendant anchor lands at an authoritative world position while
    // preserving the GLB-authored relative layout and side-specific facing.
    // localRotationDegrees is a vehicle/instance-local delta composed around
    // the authored anchor, not an absolute world orientation.
    bool hasAnchoredWorldPose = false;
    std::string anchorNodeName;
    heritage::math::Vec3 anchoredWorldPosition{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 anchoredLocalRotationDegrees{ 0.0f, 0.0f, 0.0f };

    // Bind-pose delta: zero deltas preserve the authored GLB transform exactly.
    bool hasAnchoredWorldDelta = false;
    std::string deltaAnchorNodeName;
    heritage::math::Vec3 anchoredWorldTranslationDelta{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 anchoredLocalRotationDeltaDegrees{ 0.0f, 0.0f, 0.0f };

    // Physics-driven tire presentation. VehicleSystem and the flexible-ring
    // provider are authoritative; the renderer receives only their final field.
    bool hasTireVisualDeformation = false;
    // At the terminal bare-rim stage the semantic tire node is omitted while
    // the separately authored rim/brake nodes remain visible and physical.
    bool tireVisualBareRim = false;
    std::uint8_t tireFailureVisualStage = 0;
    float tireFailureVisualTreadAttachment = 1.0f;
    float tireFailureVisualStructuralIntegrity = 1.0f;
    float tireFailureVisualEventSeed = 0.0f;
    float tireFailureVisualEventAgeSeconds = 0.0f;
    float tireFailureVisualWheelAngularVelocity = 0.0f;
    float tireFailureVisualWheelRotationRadians = 0.0f;
    float tireReferenceRadiusM = 0.30f;
    heritage::math::Vec3 tireWheelForwardWorld{ 0.0f, 0.0f, 1.0f };
    heritage::math::Vec3 tireWheelRightWorld{ 1.0f, 0.0f, 0.0f };
    heritage::math::Vec3 tireWheelUpWorld{ 0.0f, 1.0f, 0.0f };
    // Sole tire-mesh deformation authority. Each component is a final physical
    // displacement in metres in the wheel forward/down/right basis.
    bool tireVisualDeformationFieldValid = false;
    std::array<float, TireVisualDeformationFieldCount>
        tireVisualForwardDisplacementM{};
    std::array<float, TireVisualDeformationFieldCount>
        tireVisualDownDisplacementM{};
    std::array<float, TireVisualDeformationFieldCount>
        tireVisualLateralDisplacementM{};
};

struct MeshComponent
{
    // Module-asset-relative path. Absolute paths and parent traversal are
    // rejected so modules cannot attach assets outside their own Assets tree.
    std::string assetPath;
    heritage::math::Vec3 color{ 0.72f, 0.78f, 0.88f };
    bool visible = true;
    bool normalize = false;
    bool doubleSided = false;
    bool blenderCoordinates = false;

    // Optional GLB animation playback state. An empty clip name means the
    // first clip in the asset. Serial values let the renderer distinguish a
    // deliberate replay/seek from an unchanged component snapshot.
    std::string animationClip;
    bool animationPlaying = true;
    bool animationLoop = true;
    float animationSpeed = 1.0f;
    float animationCrossFadeSeconds = 0.15f;
    float animationSeekSeconds = 0.0f;
    std::uint64_t animationPlaySerial = 0;
    std::uint64_t animationSeekSerial = 0;

    // Optional diagnostic/render filter. When non-empty, only GLB draw ranges
    // whose node or an ancestor starts with this prefix are submitted.
    std::string nodeNamePrefixFilter;

    // Per-instance GLB node overrides. These keep generic render-node control
    // in the Entity layer while vehicle Lua decides which semantic nodes are
    // driven by native suspension/upright telemetry.
    std::vector<MeshNodeOverride> nodeOverrides;
};

struct MeshInstance
{
    EntityHandle entity = InvalidEntity;
    std::string assetPath;
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
    heritage::math::Vec3 color{ 0.72f, 0.78f, 0.88f };
    bool normalize = false;
    bool doubleSided = false;
    bool blenderCoordinates = false;
    std::string animationClip;
    bool animationPlaying = true;
    bool animationLoop = true;
    float animationSpeed = 1.0f;
    float animationCrossFadeSeconds = 0.15f;
    float animationSeekSeconds = 0.0f;
    std::uint64_t animationPlaySerial = 0;
    std::uint64_t animationSeekSerial = 0;
    std::string nodeNamePrefixFilter;
    std::vector<MeshNodeOverride> nodeOverrides;
};

struct Transform
{
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
};

// Generation-checked entity registry owned by one active module.
//
// Every entity stores a local transform and may have one parent. World
// transforms are calculated from the hierarchy. Destroying an entity destroys
// its complete child subtree and invalidates every old handle safely.
class EntityRegistry
{
public:
    void resetForModule(const std::string& moduleId);
    void clear();

    EntityHandle create(const std::string& name = {});
    EntityHandle createWithPersistentId(
        const std::string& name,
        std::uint64_t persistentId);
    bool destroy(EntityHandle handle);
    bool exists(EntityHandle handle) const;

    std::size_t count() const { return m_aliveCount; }
    const std::string& moduleId() const { return m_moduleId; }

    std::uint64_t persistentId(EntityHandle handle) const;

    EntityHandle findByName(const std::string& name) const;
    EntityHandle findFirstWithTag(const std::string& tag) const;

    bool setName(EntityHandle handle, const std::string& name);
    std::string name(EntityHandle handle) const;

    bool addTag(EntityHandle handle, const std::string& tag);
    bool removeTag(EntityHandle handle, const std::string& tag);
    bool hasTag(EntityHandle handle, const std::string& tag) const;
    bool tags(EntityHandle handle, std::vector<std::string>& output) const;

    // Snapshot-friendly enumeration used by scene serialization and future
    // editor tooling. Handles are returned in stable registry-slot order.
    std::vector<EntityHandle> handles() const;

    // Parent/child hierarchy. keepWorldTransform preserves the child's current
    // world pose while changing only its local transform.
    bool setParent(
        EntityHandle child,
        EntityHandle newParent,
        bool keepWorldTransform = false);
    bool clearParent(
        EntityHandle child,
        bool keepWorldTransform = true);
    EntityHandle parent(EntityHandle child) const;
    std::size_t childCount(EntityHandle parentHandle) const;
    EntityHandle childAt(EntityHandle parentHandle, std::size_t index) const;
    bool isDescendantOf(EntityHandle entity, EntityHandle ancestor) const;

    // Existing Step 27A names remain aliases for local-space transforms.
    bool setPosition(EntityHandle handle, const heritage::math::Vec3& value);
    bool position(EntityHandle handle, heritage::math::Vec3& value) const;
    bool setRotationDegrees(EntityHandle handle, const heritage::math::Vec3& value);
    bool rotationDegrees(EntityHandle handle, heritage::math::Vec3& value) const;
    bool setScale(EntityHandle handle, const heritage::math::Vec3& value);
    bool scale(EntityHandle handle, heritage::math::Vec3& value) const;

    bool setWorldPosition(EntityHandle handle, const heritage::math::Vec3& value);
    bool worldPosition(EntityHandle handle, heritage::math::Vec3& value) const;
    bool setWorldRotationDegrees(EntityHandle handle, const heritage::math::Vec3& value);
    bool worldRotationDegrees(EntityHandle handle, heritage::math::Vec3& value) const;
    bool setWorldScale(EntityHandle handle, const heritage::math::Vec3& value);
    bool worldScale(EntityHandle handle, heritage::math::Vec3& value) const;

    // Floating-origin support. Only root entities move; children retain their
    // authored/local transforms and therefore preserve complete hierarchies.
    // Physics-bound roots are subsequently synchronized to the same rebased
    // local frame by RigidBodySystem.
    void rebaseRootPositions(const heritage::math::Vec3& shift);

    // First optional render component. It is deliberately called a debug
    // primitive because production mesh/material components arrive later.
    bool setDebugPrimitive(
        EntityHandle handle,
        DebugPrimitiveType type,
        const heritage::math::Vec3& color);
    bool removeDebugPrimitive(EntityHandle handle);
    bool hasDebugPrimitive(EntityHandle handle) const;
    bool setDebugPrimitiveVisible(EntityHandle handle, bool visible);
    bool setDebugPrimitiveColor(
        EntityHandle handle,
        const heritage::math::Vec3& color);
    bool debugPrimitive(
        EntityHandle handle,
        DebugPrimitiveComponent& component) const;
    std::vector<DebugPrimitiveInstance> debugPrimitiveInstances() const;

    // Production render-mesh component foundation. The asset path is always
    // relative to the active module's Assets directory. OBJ/MTL and glTF
    // binary (.glb) assets share the same mesh component path; the renderer
    // consumes UVs, materials and module-local or embedded texture maps.
    bool setMesh(
        EntityHandle handle,
        const std::string& assetPath,
        const heritage::math::Vec3& color,
        bool normalize = false,
        bool doubleSided = false,
        bool blenderCoordinates = false);
    bool removeMesh(EntityHandle handle);
    bool hasMesh(EntityHandle handle) const;
    bool setMeshVisible(EntityHandle handle, bool visible);
    bool setMeshNodePrefixFilter(EntityHandle handle, const std::string& prefix);
    bool setMeshColor(EntityHandle handle, const heritage::math::Vec3& color);
    bool setMeshNormalize(EntityHandle handle, bool normalize);
    bool setMeshDoubleSided(EntityHandle handle, bool doubleSided);
    bool playMeshAnimation(
        EntityHandle handle,
        const std::string& clipName,
        bool loop = true,
        float crossFadeSeconds = 0.15f,
        bool restart = true);
    bool setMeshAnimationPlaying(EntityHandle handle, bool playing);
    bool setMeshAnimationSpeed(EntityHandle handle, float speed);
    bool seekMeshAnimation(EntityHandle handle, float timeSeconds);

    // Generic GLB node presentation overrides. World-pose overrides are useful
    // for semantic attachment roots (for example WH_FL_Root); local rotation
    // offsets are useful for descendants such as a wheel-spin pivot.
    bool setMeshNodeWorldPose(
        EntityHandle handle,
        const std::string& nodeName,
        const heritage::math::Vec3& position,
        const heritage::math::Vec3& rotationDegrees);
    bool setMeshNodeLocalRotationOffset(
        EntityHandle handle,
        const std::string& nodeName,
        const heritage::math::Vec3& rotationDegrees);
    bool setMeshNodeAnchoredWorldPose(
        EntityHandle handle,
        const std::string& rootNodeName,
        const std::string& anchorNodeName,
        const heritage::math::Vec3& worldPosition,
        const heritage::math::Vec3& localRotationDegrees);
    bool setMeshNodeAnchoredWorldDelta(
        EntityHandle handle,
        const std::string& rootNodeName,
        const std::string& anchorNodeName,
        const heritage::math::Vec3& worldTranslationDelta,
        const heritage::math::Vec3& localRotationDeltaDegrees);
    bool setMeshNodeTireDeformationField(
        EntityHandle handle,
        const std::string& nodeName,
        bool valid,
        float referenceRadiusM,
        const std::array<float, TireVisualDeformationFieldCount>&
            forwardDisplacementM,
        const std::array<float, TireVisualDeformationFieldCount>&
            downDisplacementM,
        const std::array<float, TireVisualDeformationFieldCount>&
            lateralDisplacementM,
        bool bareRim,
        std::uint8_t failureStage,
        float failureTreadAttachment,
        float failureStructuralIntegrity,
        float failureEventSeed,
        float failureEventAgeSeconds,
        float wheelAngularVelocity,
        float wheelRotationRadians,
        const heritage::math::Vec3& wheelForwardWorld,
        const heritage::math::Vec3& wheelRightWorld,
        const heritage::math::Vec3& wheelUpWorld);
    bool clearMeshNodeOverrides(EntityHandle handle);

    bool mesh(EntityHandle handle, MeshComponent& component) const;
    std::vector<MeshInstance> meshInstances() const;
    void meshInstances(std::vector<MeshInstance>& result) const;

    const std::string& lastError() const { return m_lastError; }

private:
    struct Record
    {
        std::uint64_t persistentId = 0;
        std::string name;
        std::unordered_set<std::string> tags;
        Transform localTransform;
        std::optional<DebugPrimitiveComponent> debugPrimitive;
        std::optional<MeshComponent> mesh;
        EntityHandle parent = InvalidEntity;
        std::vector<EntityHandle> children;
    };

    struct Slot
    {
        std::uint32_t generation = 1;
        bool alive = false;
        Record record;
    };

    using Quaternion = heritage::math::Quaternion;

    struct WorldTransform
    {
        heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
        Quaternion rotation;
        heritage::math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
    };

    static EntityHandle makeHandle(std::uint32_t index, std::uint32_t generation);
    static bool decodeHandle(
        EntityHandle handle,
        std::uint32_t& index,
        std::uint32_t& generation);

    EntityHandle allocate(
        const std::string& requestedName,
        std::uint64_t persistentId);

    Slot* resolve(EntityHandle handle);
    const Slot* resolve(EntityHandle handle) const;

    bool computeWorldTransform(
        EntityHandle handle,
        WorldTransform& result,
        std::size_t depth = 0) const;
    bool applyWorldTransform(EntityHandle handle, const WorldTransform& world);
    bool destroySubtree(EntityHandle handle);
    void removeChildReference(EntityHandle parentHandle, EntityHandle childHandle);

    static Quaternion quaternionFromEulerDegrees(const heritage::math::Vec3& value);
    static heritage::math::Vec3 eulerDegreesFromQuaternion(const Quaternion& value);
    static Quaternion multiply(const Quaternion& left, const Quaternion& right);
    static Quaternion inverse(const Quaternion& value);
    static heritage::math::Vec3 rotate(
        const Quaternion& rotation,
        const heritage::math::Vec3& value);

    void setError(const std::string& message) const;
    void clearError() const;

    std::string m_moduleId;
    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeIndices;
    std::uint64_t m_nextPersistentId = 1;
    std::size_t m_aliveCount = 0;
    mutable std::string m_lastError;
};

} // namespace heritage::entities
