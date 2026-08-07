#include "EntityRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace heritage::entities {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kMaximumHierarchyDepth = 4096;

bool validVec3(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

bool validScale(const heritage::math::Vec3& value)
{
    return validVec3(value)
        && std::abs(value.x) > 0.000001f
        && std::abs(value.y) > 0.000001f
        && std::abs(value.z) > 0.000001f;
}

std::string trimmed(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char character) { return std::isspace(character) != 0; });
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char character) { return std::isspace(character) != 0; }).base();

    return first < last ? std::string(first, last) : std::string{};
}

heritage::math::Vec3 add(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x + right.x, left.y + right.y, left.z + right.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

heritage::math::Vec3 multiplyComponents(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x * right.x, left.y * right.y, left.z * right.z };
}

heritage::math::Vec3 divideComponents(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x / right.x, left.y / right.y, left.z / right.z };
}

float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

float degrees(float radiansValue)
{
    return radiansValue * (180.0f / kPi);
}

} // namespace

void EntityRegistry::resetForModule(const std::string& moduleId)
{
    clear();
    m_moduleId = moduleId;
}

void EntityRegistry::clear()
{
    m_slots.clear();
    m_freeIndices.clear();
    m_nextPersistentId = 1;
    m_aliveCount = 0;
    m_lastError.clear();
}

EntityHandle EntityRegistry::create(const std::string& requestedName)
{
    return createWithPersistentId(requestedName, m_nextPersistentId);
}

EntityHandle EntityRegistry::createWithPersistentId(
    const std::string& requestedName,
    std::uint64_t requestedPersistentId)
{
    if (requestedPersistentId == 0)
    {
        setError("Entity persistent IDs must be greater than zero.");
        return InvalidEntity;
    }

    for (const Slot& slot : m_slots)
    {
        if (slot.alive && slot.record.persistentId == requestedPersistentId)
        {
            setError(
                "Entity persistent ID "
                + std::to_string(requestedPersistentId)
                + " is already in use.");
            return InvalidEntity;
        }
    }

    return allocate(requestedName, requestedPersistentId);
}

EntityHandle EntityRegistry::allocate(
    const std::string& requestedName,
    std::uint64_t requestedPersistentId)
{
    std::uint32_t index = 0;
    if (!m_freeIndices.empty())
    {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    }
    else
    {
        if (m_slots.size() >= static_cast<std::size_t>(
                (std::numeric_limits<std::uint32_t>::max)() - 1u))
        {
            setError("Entity registry has exhausted its handle index space.");
            return InvalidEntity;
        }

        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }

    Slot& slot = m_slots[index];
    slot.alive = true;
    slot.record = {};
    slot.record.persistentId = requestedPersistentId;

    if (requestedPersistentId >= m_nextPersistentId)
    {
        m_nextPersistentId = requestedPersistentId ==
            (std::numeric_limits<std::uint64_t>::max)()
            ? requestedPersistentId
            : requestedPersistentId + 1;
    }

    const std::string cleanName = trimmed(requestedName);
    slot.record.name = cleanName.empty()
        ? "Entity " + std::to_string(slot.record.persistentId)
        : cleanName;

    ++m_aliveCount;
    clearError();
    return makeHandle(index, slot.generation);
}

bool EntityRegistry::destroy(EntityHandle handle)
{
    if (!resolve(handle))
    {
        setError("Entity.Destroy received an invalid, stale, or already destroyed handle.");
        return false;
    }

    if (!destroySubtree(handle))
        return false;

    clearError();
    return true;
}

bool EntityRegistry::destroySubtree(EntityHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
        return false;

    const std::vector<EntityHandle> children = slot->record.children;
    for (EntityHandle child : children)
    {
        if (resolve(child))
            destroySubtree(child);
    }

    // The recursive calls may reallocate neither slots nor this record, but
    // resolve again to make the lifetime rule explicit.
    slot = resolve(handle);
    if (!slot)
        return false;

    const EntityHandle oldParent = slot->record.parent;
    if (oldParent != InvalidEntity)
        removeChildReference(oldParent, handle);

    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation) || index >= m_slots.size())
        return false;

    Slot& target = m_slots[index];
    target.alive = false;
    target.record = {};
    ++target.generation;
    if (target.generation == 0)
        target.generation = 1;

    m_freeIndices.push_back(index);
    if (m_aliveCount > 0)
        --m_aliveCount;
    return true;
}

bool EntityRegistry::exists(EntityHandle handle) const
{
    return resolve(handle) != nullptr;
}

std::uint64_t EntityRegistry::persistentId(EntityHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetPersistentId received an invalid or stale handle.");
        return 0;
    }

    clearError();
    return slot->record.persistentId;
}

EntityHandle EntityRegistry::findByName(const std::string& requestedName) const
{
    const std::string cleanName = trimmed(requestedName);
    if (cleanName.empty())
    {
        setError("Entity.FindByName requires a non-empty name.");
        return InvalidEntity;
    }

    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (slot.alive && slot.record.name == cleanName)
        {
            clearError();
            return makeHandle(index, slot.generation);
        }
    }

    clearError();
    return InvalidEntity;
}

EntityHandle EntityRegistry::findFirstWithTag(const std::string& requestedTag) const
{
    const std::string cleanTag = trimmed(requestedTag);
    if (cleanTag.empty())
    {
        setError("Entity.FindFirstWithTag requires a non-empty tag.");
        return InvalidEntity;
    }

    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (slot.alive && slot.record.tags.contains(cleanTag))
        {
            clearError();
            return makeHandle(index, slot.generation);
        }
    }

    clearError();
    return InvalidEntity;
}

bool EntityRegistry::setName(EntityHandle handle, const std::string& requestedName)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetName received an invalid or stale handle.");
        return false;
    }

    const std::string cleanName = trimmed(requestedName);
    if (cleanName.empty())
    {
        setError("Entity.SetName requires a non-empty name.");
        return false;
    }

    slot->record.name = cleanName;
    clearError();
    return true;
}

std::string EntityRegistry::name(EntityHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetName received an invalid or stale handle.");
        return {};
    }

    clearError();
    return slot->record.name;
}

bool EntityRegistry::addTag(EntityHandle handle, const std::string& requestedTag)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.AddTag received an invalid or stale handle.");
        return false;
    }

    const std::string cleanTag = trimmed(requestedTag);
    if (cleanTag.empty())
    {
        setError("Entity.AddTag requires a non-empty tag.");
        return false;
    }

    slot->record.tags.insert(cleanTag);
    clearError();
    return true;
}

bool EntityRegistry::removeTag(EntityHandle handle, const std::string& requestedTag)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.RemoveTag received an invalid or stale handle.");
        return false;
    }

    const std::string cleanTag = trimmed(requestedTag);
    if (cleanTag.empty())
    {
        setError("Entity.RemoveTag requires a non-empty tag.");
        return false;
    }

    slot->record.tags.erase(cleanTag);
    clearError();
    return true;
}

bool EntityRegistry::hasTag(EntityHandle handle, const std::string& requestedTag) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.HasTag received an invalid or stale handle.");
        return false;
    }

    const std::string cleanTag = trimmed(requestedTag);
    if (cleanTag.empty())
    {
        setError("Entity.HasTag requires a non-empty tag.");
        return false;
    }

    clearError();
    return slot->record.tags.contains(cleanTag);
}

bool EntityRegistry::tags(
    EntityHandle handle,
    std::vector<std::string>& output) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity tag enumeration received an invalid or stale handle.");
        output.clear();
        return false;
    }

    output.assign(slot->record.tags.begin(), slot->record.tags.end());
    std::sort(output.begin(), output.end());
    clearError();
    return true;
}

std::vector<EntityHandle> EntityRegistry::handles() const
{
    std::vector<EntityHandle> result;
    result.reserve(m_aliveCount);

    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (slot.alive)
            result.push_back(makeHandle(index, slot.generation));
    }

    clearError();
    return result;
}

bool EntityRegistry::setParent(
    EntityHandle child,
    EntityHandle newParent,
    bool keepWorldTransform)
{
    Slot* childSlot = resolve(child);
    Slot* parentSlot = resolve(newParent);
    if (!childSlot)
    {
        setError("Entity.SetParent received an invalid or stale child handle.");
        return false;
    }
    if (!parentSlot)
    {
        setError("Entity.SetParent received an invalid or stale parent handle.");
        return false;
    }
    if (child == newParent)
    {
        setError("An entity cannot be parented to itself.");
        return false;
    }
    if (isDescendantOf(newParent, child))
    {
        setError("Entity.SetParent rejected a hierarchy cycle.");
        return false;
    }
    if (childSlot->record.parent == newParent)
    {
        clearError();
        return true;
    }

    WorldTransform previousWorld;
    if (keepWorldTransform && !computeWorldTransform(child, previousWorld))
        return false;

    const EntityHandle oldParent = childSlot->record.parent;
    if (oldParent != InvalidEntity)
        removeChildReference(oldParent, child);

    childSlot = resolve(child);
    parentSlot = resolve(newParent);
    if (!childSlot || !parentSlot)
    {
        setError("Entity.SetParent lost a hierarchy handle unexpectedly.");
        return false;
    }

    childSlot->record.parent = newParent;
    if (std::find(parentSlot->record.children.begin(), parentSlot->record.children.end(), child)
        == parentSlot->record.children.end())
    {
        parentSlot->record.children.push_back(child);
    }

    if (keepWorldTransform && !applyWorldTransform(child, previousWorld))
        return false;

    clearError();
    return true;
}

bool EntityRegistry::clearParent(
    EntityHandle child,
    bool keepWorldTransform)
{
    Slot* childSlot = resolve(child);
    if (!childSlot)
    {
        setError("Entity.ClearParent received an invalid or stale child handle.");
        return false;
    }
    if (childSlot->record.parent == InvalidEntity)
    {
        clearError();
        return true;
    }

    WorldTransform previousWorld;
    if (keepWorldTransform && !computeWorldTransform(child, previousWorld))
        return false;

    const EntityHandle oldParent = childSlot->record.parent;
    removeChildReference(oldParent, child);

    childSlot = resolve(child);
    if (!childSlot)
    {
        setError("Entity.ClearParent lost the child handle unexpectedly.");
        return false;
    }
    childSlot->record.parent = InvalidEntity;

    if (keepWorldTransform && !applyWorldTransform(child, previousWorld))
        return false;

    clearError();
    return true;
}

EntityHandle EntityRegistry::parent(EntityHandle child) const
{
    const Slot* slot = resolve(child);
    if (!slot)
    {
        setError("Entity.GetParent received an invalid or stale handle.");
        return InvalidEntity;
    }

    clearError();
    return resolve(slot->record.parent) ? slot->record.parent : InvalidEntity;
}

std::size_t EntityRegistry::childCount(EntityHandle parentHandle) const
{
    const Slot* slot = resolve(parentHandle);
    if (!slot)
    {
        setError("Entity.GetChildCount received an invalid or stale handle.");
        return 0;
    }

    clearError();
    return slot->record.children.size();
}

EntityHandle EntityRegistry::childAt(
    EntityHandle parentHandle,
    std::size_t index) const
{
    const Slot* slot = resolve(parentHandle);
    if (!slot)
    {
        setError("Entity.GetChildAt received an invalid or stale parent handle.");
        return InvalidEntity;
    }
    if (index >= slot->record.children.size())
    {
        setError("Entity.GetChildAt index is outside the child list.");
        return InvalidEntity;
    }

    const EntityHandle child = slot->record.children[index];
    if (!resolve(child))
    {
        setError("Entity.GetChildAt encountered a stale child reference.");
        return InvalidEntity;
    }

    clearError();
    return child;
}

bool EntityRegistry::isDescendantOf(
    EntityHandle entity,
    EntityHandle ancestor) const
{
    if (!resolve(entity) || !resolve(ancestor))
        return false;

    EntityHandle current = parent(entity);
    std::size_t depth = 0;
    while (current != InvalidEntity && depth++ < kMaximumHierarchyDepth)
    {
        if (current == ancestor)
        {
            clearError();
            return true;
        }
        const Slot* slot = resolve(current);
        current = slot ? slot->record.parent : InvalidEntity;
    }

    clearError();
    return false;
}

void EntityRegistry::removeChildReference(
    EntityHandle parentHandle,
    EntityHandle childHandle)
{
    Slot* parentSlot = resolve(parentHandle);
    if (!parentSlot)
        return;

    auto& children = parentSlot->record.children;
    children.erase(
        std::remove(children.begin(), children.end(), childHandle),
        children.end());
}

bool EntityRegistry::setPosition(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetPosition received an invalid or stale handle.");
        return false;
    }
    if (!validVec3(value))
    {
        setError("Entity.SetPosition requires finite numeric values.");
        return false;
    }

    slot->record.localTransform.position = value;
    clearError();
    return true;
}

bool EntityRegistry::position(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetPosition received an invalid or stale handle.");
        return false;
    }

    value = slot->record.localTransform.position;
    clearError();
    return true;
}

bool EntityRegistry::setRotationDegrees(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetRotation received an invalid or stale handle.");
        return false;
    }
    if (!validVec3(value))
    {
        setError("Entity.SetRotation requires finite numeric values.");
        return false;
    }

    slot->record.localTransform.rotationDegrees = value;
    clearError();
    return true;
}

bool EntityRegistry::rotationDegrees(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetRotation received an invalid or stale handle.");
        return false;
    }

    value = slot->record.localTransform.rotationDegrees;
    clearError();
    return true;
}

bool EntityRegistry::setScale(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetScale received an invalid or stale handle.");
        return false;
    }
    if (!validScale(value))
    {
        setError("Entity.SetScale requires finite, non-zero values.");
        return false;
    }

    slot->record.localTransform.scale = value;
    clearError();
    return true;
}

bool EntityRegistry::scale(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetScale received an invalid or stale handle.");
        return false;
    }

    value = slot->record.localTransform.scale;
    clearError();
    return true;
}

bool EntityRegistry::setWorldPosition(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    if (!validVec3(value))
    {
        setError("Entity.SetWorldPosition requires finite numeric values.");
        return false;
    }

    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    world.position = value;
    return applyWorldTransform(handle, world);
}

bool EntityRegistry::worldPosition(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    value = world.position;
    clearError();
    return true;
}

bool EntityRegistry::setWorldRotationDegrees(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    if (!validVec3(value))
    {
        setError("Entity.SetWorldRotation requires finite numeric values.");
        return false;
    }

    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    world.rotation = quaternionFromEulerDegrees(value);
    return applyWorldTransform(handle, world);
}

bool EntityRegistry::worldRotationDegrees(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    value = eulerDegreesFromQuaternion(world.rotation);
    clearError();
    return true;
}

bool EntityRegistry::setWorldScale(
    EntityHandle handle,
    const heritage::math::Vec3& value)
{
    if (!validScale(value))
    {
        setError("Entity.SetWorldScale requires finite, non-zero values.");
        return false;
    }

    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    world.scale = value;
    return applyWorldTransform(handle, world);
}

bool EntityRegistry::worldScale(
    EntityHandle handle,
    heritage::math::Vec3& value) const
{
    WorldTransform world;
    if (!computeWorldTransform(handle, world))
        return false;
    value = world.scale;
    clearError();
    return true;
}

bool EntityRegistry::setDebugPrimitive(
    EntityHandle handle,
    DebugPrimitiveType type,
    const heritage::math::Vec3& color)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetDebugPrimitive received an invalid or stale handle.");
        return false;
    }
    if (!validVec3(color))
    {
        setError("Entity.SetDebugPrimitive requires a finite RGB color.");
        return false;
    }

    DebugPrimitiveComponent component;
    component.type = type;
    component.color = {
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    };
    component.visible = slot->record.debugPrimitive
        ? slot->record.debugPrimitive->visible
        : true;
    slot->record.debugPrimitive = component;
    clearError();
    return true;
}

bool EntityRegistry::removeDebugPrimitive(EntityHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.RemoveDebugPrimitive received an invalid or stale handle.");
        return false;
    }

    slot->record.debugPrimitive.reset();
    clearError();
    return true;
}

bool EntityRegistry::hasDebugPrimitive(EntityHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.HasDebugPrimitive received an invalid or stale handle.");
        return false;
    }

    clearError();
    return slot->record.debugPrimitive.has_value();
}

bool EntityRegistry::setDebugPrimitiveVisible(EntityHandle handle, bool visible)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetDebugVisible received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.debugPrimitive)
    {
        setError("Entity.SetDebugVisible requires a DebugPrimitive component.");
        return false;
    }

    slot->record.debugPrimitive->visible = visible;
    clearError();
    return true;
}

bool EntityRegistry::setDebugPrimitiveColor(
    EntityHandle handle,
    const heritage::math::Vec3& color)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetDebugColor received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.debugPrimitive)
    {
        setError("Entity.SetDebugColor requires a DebugPrimitive component.");
        return false;
    }
    if (!validVec3(color))
    {
        setError("Entity.SetDebugColor requires a finite RGB color.");
        return false;
    }

    slot->record.debugPrimitive->color = {
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    };
    clearError();
    return true;
}

bool EntityRegistry::debugPrimitive(
    EntityHandle handle,
    DebugPrimitiveComponent& component) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetDebugPrimitive received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.debugPrimitive)
    {
        setError("Entity does not have a DebugPrimitive component.");
        return false;
    }

    component = *slot->record.debugPrimitive;
    clearError();
    return true;
}

std::vector<DebugPrimitiveInstance> EntityRegistry::debugPrimitiveInstances() const
{
    std::vector<DebugPrimitiveInstance> result;
    result.reserve(m_aliveCount);

    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive || !slot.record.debugPrimitive || !slot.record.debugPrimitive->visible)
            continue;

        const EntityHandle handle = makeHandle(index, slot.generation);
        heritage::math::Vec3 position{};
        heritage::math::Vec3 rotation{};
        heritage::math::Vec3 scale{};
        if (!worldPosition(handle, position)
            || !worldRotationDegrees(handle, rotation)
            || !worldScale(handle, scale))
        {
            continue;
        }

        result.push_back({
            handle,
            slot.record.debugPrimitive->type,
            position,
            rotation,
            scale,
            slot.record.debugPrimitive->color
        });
    }

    clearError();
    return result;
}

bool EntityRegistry::setMesh(
    EntityHandle handle,
    const std::string& assetPath,
    const heritage::math::Vec3& color,
    bool normalize,
    bool doubleSided,
    bool blenderCoordinates)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMesh received an invalid or stale handle.");
        return false;
    }

    const std::filesystem::path requested(assetPath);
    if (assetPath.empty() || requested.is_absolute() || requested.has_root_name())
    {
        setError("Entity.SetMesh requires a module-asset-relative OBJ path.");
        return false;
    }
    for (const auto& part : requested)
    {
        if (part == "..")
        {
            setError("Entity.SetMesh cannot traverse outside the module Assets directory.");
            return false;
        }
    }
    std::string extension = requested.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (extension != ".obj")
    {
        setError("Entity.SetMesh currently supports .obj assets only.");
        return false;
    }
    if (!validVec3(color))
    {
        setError("Entity.SetMesh requires a finite RGB color.");
        return false;
    }

    MeshComponent component;
    component.assetPath = requested.lexically_normal().generic_string();
    component.color = {
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    };
    component.visible = slot->record.mesh ? slot->record.mesh->visible : true;
    component.normalize = normalize;
    component.doubleSided = doubleSided;
    component.blenderCoordinates = blenderCoordinates;
    slot->record.mesh = std::move(component);
    clearError();
    return true;
}

bool EntityRegistry::removeMesh(EntityHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.RemoveMesh received an invalid or stale handle.");
        return false;
    }
    slot->record.mesh.reset();
    clearError();
    return true;
}

bool EntityRegistry::hasMesh(EntityHandle handle) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.HasMesh received an invalid or stale handle.");
        return false;
    }
    clearError();
    return slot->record.mesh.has_value();
}

bool EntityRegistry::setMeshVisible(EntityHandle handle, bool visible)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshVisible received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshVisible requires a Mesh component.");
        return false;
    }
    slot->record.mesh->visible = visible;
    clearError();
    return true;
}

bool EntityRegistry::setMeshColor(
    EntityHandle handle,
    const heritage::math::Vec3& color)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshColor received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshColor requires a Mesh component.");
        return false;
    }
    if (!validVec3(color))
    {
        setError("Entity.SetMeshColor requires a finite RGB color.");
        return false;
    }
    slot->record.mesh->color = {
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    };
    clearError();
    return true;
}

bool EntityRegistry::setMeshNormalize(EntityHandle handle, bool normalize)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNormalize received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNormalize requires a Mesh component.");
        return false;
    }
    slot->record.mesh->normalize = normalize;
    clearError();
    return true;
}

bool EntityRegistry::setMeshDoubleSided(EntityHandle handle, bool doubleSided)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshDoubleSided received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshDoubleSided requires a Mesh component.");
        return false;
    }
    slot->record.mesh->doubleSided = doubleSided;
    clearError();
    return true;
}

bool EntityRegistry::mesh(EntityHandle handle, MeshComponent& component) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.GetMesh received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity does not have a Mesh component.");
        return false;
    }
    component = *slot->record.mesh;
    clearError();
    return true;
}

std::vector<MeshInstance> EntityRegistry::meshInstances() const
{
    std::vector<MeshInstance> result;
    result.reserve(m_aliveCount);

    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive || !slot.record.mesh || !slot.record.mesh->visible)
            continue;

        const EntityHandle handle = makeHandle(index, slot.generation);
        heritage::math::Vec3 position{};
        heritage::math::Vec3 rotation{};
        heritage::math::Vec3 scale{};
        if (!worldPosition(handle, position)
            || !worldRotationDegrees(handle, rotation)
            || !worldScale(handle, scale))
        {
            continue;
        }

        result.push_back({
            handle,
            slot.record.mesh->assetPath,
            position,
            rotation,
            scale,
            slot.record.mesh->color,
            slot.record.mesh->normalize,
            slot.record.mesh->doubleSided,
            slot.record.mesh->blenderCoordinates
        });
    }

    clearError();
    return result;
}

bool EntityRegistry::computeWorldTransform(
    EntityHandle handle,
    WorldTransform& result,
    std::size_t depth) const
{
    const Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("World-transform query received an invalid or stale entity handle.");
        return false;
    }
    if (depth > kMaximumHierarchyDepth)
    {
        setError("Entity hierarchy exceeded the maximum safe depth.");
        return false;
    }

    WorldTransform local;
    local.position = slot->record.localTransform.position;
    local.rotation = quaternionFromEulerDegrees(slot->record.localTransform.rotationDegrees);
    local.scale = slot->record.localTransform.scale;

    if (slot->record.parent == InvalidEntity)
    {
        result = local;
        clearError();
        return true;
    }

    WorldTransform parentWorld;
    if (!computeWorldTransform(slot->record.parent, parentWorld, depth + 1))
        return false;

    result.scale = multiplyComponents(parentWorld.scale, local.scale);
    result.rotation = multiply(parentWorld.rotation, local.rotation);
    result.position = add(
        parentWorld.position,
        rotate(
            parentWorld.rotation,
            multiplyComponents(parentWorld.scale, local.position)));
    clearError();
    return true;
}

bool EntityRegistry::applyWorldTransform(
    EntityHandle handle,
    const WorldTransform& world)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("World-transform assignment received an invalid or stale entity handle.");
        return false;
    }
    if (!validVec3(world.position) || !validScale(world.scale))
    {
        setError("World-transform assignment requires finite values and non-zero scale.");
        return false;
    }

    if (slot->record.parent == InvalidEntity)
    {
        slot->record.localTransform.position = world.position;
        slot->record.localTransform.rotationDegrees = eulerDegreesFromQuaternion(world.rotation);
        slot->record.localTransform.scale = world.scale;
        clearError();
        return true;
    }

    WorldTransform parentWorld;
    if (!computeWorldTransform(slot->record.parent, parentWorld))
        return false;
    if (!validScale(parentWorld.scale))
    {
        setError("Parent world scale cannot be inverted.");
        return false;
    }

    const Quaternion inverseParentRotation = inverse(parentWorld.rotation);
    const heritage::math::Vec3 parentSpaceOffset = rotate(
        inverseParentRotation,
        subtract(world.position, parentWorld.position));

    slot->record.localTransform.position = divideComponents(
        parentSpaceOffset,
        parentWorld.scale);
    slot->record.localTransform.rotationDegrees = eulerDegreesFromQuaternion(
        multiply(inverseParentRotation, world.rotation));
    slot->record.localTransform.scale = divideComponents(
        world.scale,
        parentWorld.scale);

    clearError();
    return true;
}

EntityRegistry::Quaternion EntityRegistry::quaternionFromEulerDegrees(
    const heritage::math::Vec3& value)
{
    const float halfX = radians(value.x) * 0.5f;
    const float halfY = radians(value.y) * 0.5f;
    const float halfZ = radians(value.z) * 0.5f;

    const float cx = std::cos(halfX);
    const float sx = std::sin(halfX);
    const float cy = std::cos(halfY);
    const float sy = std::sin(halfY);
    const float cz = std::cos(halfZ);
    const float sz = std::sin(halfZ);

    // Intrinsic X-Y-Z rotation represented as qZ * qY * qX.
    return {
        cz * cy * cx + sz * sy * sx,
        cz * cy * sx - sz * sy * cx,
        cz * sy * cx + sz * cy * sx,
        sz * cy * cx - cz * sy * sx
    };
}

heritage::math::Vec3 EntityRegistry::eulerDegreesFromQuaternion(
    const Quaternion& value)
{
    const float sinXCosY = 2.0f * (value.w * value.x + value.y * value.z);
    const float cosXCosY = 1.0f - 2.0f * (value.x * value.x + value.y * value.y);
    const float angleX = std::atan2(sinXCosY, cosXCosY);

    const float sinY = std::clamp(
        2.0f * (value.w * value.y - value.z * value.x),
        -1.0f,
        1.0f);
    const float angleY = std::asin(sinY);

    const float sinZCosY = 2.0f * (value.w * value.z + value.x * value.y);
    const float cosZCosY = 1.0f - 2.0f * (value.y * value.y + value.z * value.z);
    const float angleZ = std::atan2(sinZCosY, cosZCosY);

    return { degrees(angleX), degrees(angleY), degrees(angleZ) };
}

EntityRegistry::Quaternion EntityRegistry::multiply(
    const Quaternion& left,
    const Quaternion& right)
{
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w
    };
}

EntityRegistry::Quaternion EntityRegistry::inverse(const Quaternion& value)
{
    const float lengthSquared = value.w * value.w
        + value.x * value.x
        + value.y * value.y
        + value.z * value.z;
    if (lengthSquared <= 0.000001f)
        return {};

    return {
        value.w / lengthSquared,
        -value.x / lengthSquared,
        -value.y / lengthSquared,
        -value.z / lengthSquared
    };
}

heritage::math::Vec3 EntityRegistry::rotate(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    const Quaternion vectorQuaternion{ 0.0f, value.x, value.y, value.z };
    const Quaternion rotated = multiply(
        multiply(rotation, vectorQuaternion),
        inverse(rotation));
    return { rotated.x, rotated.y, rotated.z };
}

EntityHandle EntityRegistry::makeHandle(
    std::uint32_t index,
    std::uint32_t generation)
{
    return (static_cast<EntityHandle>(generation) << 32u)
        | static_cast<EntityHandle>(index + 1u);
}

bool EntityRegistry::decodeHandle(
    EntityHandle handle,
    std::uint32_t& index,
    std::uint32_t& generation)
{
    if (handle == InvalidEntity)
        return false;

    const std::uint32_t encodedIndex = static_cast<std::uint32_t>(
        handle & 0xffffffffull);
    generation = static_cast<std::uint32_t>(handle >> 32u);

    if (encodedIndex == 0 || generation == 0)
        return false;

    index = encodedIndex - 1u;
    return true;
}

EntityRegistry::Slot* EntityRegistry::resolve(EntityHandle handle)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        return nullptr;
    }

    Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

const EntityRegistry::Slot* EntityRegistry::resolve(EntityHandle handle) const
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    if (!decodeHandle(handle, index, generation)
        || index >= m_slots.size())
    {
        return nullptr;
    }

    const Slot& slot = m_slots[index];
    return slot.alive && slot.generation == generation ? &slot : nullptr;
}

void EntityRegistry::setError(const std::string& message) const
{
    m_lastError = message;
}

void EntityRegistry::clearError() const
{
    m_lastError.clear();
}

} // namespace heritage::entities
