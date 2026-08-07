#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "../Math/Math.hpp"

namespace heritage::entities {

using EntityHandle = std::uint64_t;
inline constexpr EntityHandle InvalidEntity = 0;

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
    // relative to the active module's Assets directory. Step 27E supports OBJ
    // geometry and a simple tint; materials and textures arrive later.
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
    bool setMeshColor(EntityHandle handle, const heritage::math::Vec3& color);
    bool setMeshNormalize(EntityHandle handle, bool normalize);
    bool setMeshDoubleSided(EntityHandle handle, bool doubleSided);
    bool mesh(EntityHandle handle, MeshComponent& component) const;
    std::vector<MeshInstance> meshInstances() const;

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

    struct Quaternion
    {
        float w = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

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
