#include "EntityRegistryInternal.hpp"

namespace heritage::entities {
using namespace entity_registry_internal;

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

void EntityRegistry::rebaseRootPositions(
    const heritage::math::Vec3& shift)
{
    if (!validVec3(shift))
        return;

    for (Slot& slot : m_slots)
    {
        if (!slot.alive || slot.record.parent != InvalidEntity)
            continue;

        slot.record.localTransform.position.x -= shift.x;
        slot.record.localTransform.position.y -= shift.y;
        slot.record.localTransform.position.z -= shift.z;
    }

    clearError();
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
    return heritage::math::makeQuaternionFromEulerDegrees(value);
}

heritage::math::Vec3 EntityRegistry::eulerDegreesFromQuaternion(
    const Quaternion& value)
{
    return heritage::math::eulerDegreesFromUnitQuaternion(value);
}

EntityRegistry::Quaternion EntityRegistry::multiply(
    const Quaternion& left,
    const Quaternion& right)
{
    return heritage::math::multiply(left, right);
}

EntityRegistry::Quaternion EntityRegistry::inverse(const Quaternion& value)
{
    return heritage::math::inverse(value, 0.000001f);
}

heritage::math::Vec3 EntityRegistry::rotate(
    const Quaternion& rotation,
    const heritage::math::Vec3& value)
{
    return heritage::math::rotateVectorGeneral(rotation, value);
}


} // namespace heritage::entities
