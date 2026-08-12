#include "EntityRegistryInternal.hpp"

namespace heritage::entities {
using namespace entity_registry_internal;

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


} // namespace heritage::entities
