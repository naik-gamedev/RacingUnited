#include "EntityRegistryInternal.hpp"
#include "../Paths/Utf8Path.hpp"

#include <iterator>

namespace heritage::entities {
using namespace entity_registry_internal;

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
