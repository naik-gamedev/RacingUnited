#include "EntityRegistryInternal.hpp"

namespace heritage::entities {
using namespace entity_registry_internal;

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


} // namespace heritage::entities
