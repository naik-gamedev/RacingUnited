#include "EntityRegistryInternal.hpp"
#include "../Paths/Utf8Path.hpp"

#include <iterator>

namespace heritage::entities {
using namespace entity_registry_internal;

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

    const std::filesystem::path requested = heritage::paths::fromUtf8(assetPath);
    if (assetPath.empty() || requested.is_absolute() || requested.has_root_name())
    {
        setError("Entity.SetMesh requires a module-asset-relative .obj or .glb path.");
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
    if (extension != ".obj" && extension != ".glb")
    {
        setError("Entity.SetMesh currently supports .obj and .glb assets.");
        return false;
    }
    if (!validVec3(color))
    {
        setError("Entity.SetMesh requires a finite RGB color.");
        return false;
    }

    MeshComponent component;
    component.assetPath = heritage::paths::toUtf8(requested.lexically_normal());
    component.color = {
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    };
    component.visible = slot->record.mesh ? slot->record.mesh->visible : true;
    component.nodeNamePrefixFilter = slot->record.mesh
        ? slot->record.mesh->nodeNamePrefixFilter
        : std::string{};
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

bool EntityRegistry::setMeshNodePrefixFilter(
    EntityHandle handle,
    const std::string& prefix)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodePrefixFilter received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodePrefixFilter requires a Mesh component.");
        return false;
    }

    slot->record.mesh->nodeNamePrefixFilter = prefix;
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


bool EntityRegistry::playMeshAnimation(
    EntityHandle handle,
    const std::string& clipName,
    bool loop,
    float crossFadeSeconds,
    bool restart)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.PlayMeshAnimation received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.PlayMeshAnimation requires a Mesh component.");
        return false;
    }
    if (!std::isfinite(crossFadeSeconds) || crossFadeSeconds < 0.0f)
    {
        setError("Entity.PlayMeshAnimation requires a finite non-negative crossfade time.");
        return false;
    }

    MeshComponent& mesh = *slot->record.mesh;
    const bool clipChanged = mesh.animationClip != clipName;
    mesh.animationClip = clipName;
    mesh.animationLoop = loop;
    mesh.animationPlaying = true;
    mesh.animationCrossFadeSeconds = crossFadeSeconds;
    if (restart || clipChanged)
        ++mesh.animationPlaySerial;
    clearError();
    return true;
}

bool EntityRegistry::setMeshAnimationPlaying(EntityHandle handle, bool playing)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshAnimationPlaying received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshAnimationPlaying requires a Mesh component.");
        return false;
    }
    slot->record.mesh->animationPlaying = playing;
    clearError();
    return true;
}

bool EntityRegistry::setMeshAnimationSpeed(EntityHandle handle, float speed)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshAnimationSpeed received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshAnimationSpeed requires a Mesh component.");
        return false;
    }
    if (!std::isfinite(speed) || std::abs(speed) > 32.0f)
    {
        setError("Entity.SetMeshAnimationSpeed requires a finite speed between -32 and 32.");
        return false;
    }
    slot->record.mesh->animationSpeed = speed;
    clearError();
    return true;
}

bool EntityRegistry::seekMeshAnimation(EntityHandle handle, float timeSeconds)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SeekMeshAnimation received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SeekMeshAnimation requires a Mesh component.");
        return false;
    }
    if (!std::isfinite(timeSeconds) || timeSeconds < 0.0f)
    {
        setError("Entity.SeekMeshAnimation requires a finite non-negative time.");
        return false;
    }
    slot->record.mesh->animationSeekSeconds = timeSeconds;
    ++slot->record.mesh->animationSeekSerial;
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeWorldPose(
    EntityHandle handle,
    const std::string& nodeName,
    const heritage::math::Vec3& position,
    const heritage::math::Vec3& rotationDegrees)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeWorldPose received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeWorldPose requires a Mesh component.");
        return false;
    }
    if (nodeName.empty())
    {
        setError("Entity.SetMeshNodeWorldPose requires a non-empty GLB node name.");
        return false;
    }
    if (!validVec3(position) || !validVec3(rotationDegrees))
    {
        setError("Entity.SetMeshNodeWorldPose requires finite position/rotation values.");
        return false;
    }

    auto& overrides = slot->record.mesh->nodeOverrides;
    auto found = std::find_if(
        overrides.begin(), overrides.end(),
        [&](const MeshNodeOverride& value) { return value.nodeName == nodeName; });
    if (found == overrides.end())
    {
        overrides.push_back({});
        found = std::prev(overrides.end());
        found->nodeName = nodeName;
    }
    found->hasWorldPose = true;
    found->worldPosition = position;
    found->worldRotationDegrees = rotationDegrees;
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeLocalRotationOffset(
    EntityHandle handle,
    const std::string& nodeName,
    const heritage::math::Vec3& rotationDegrees)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeLocalRotationOffset received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeLocalRotationOffset requires a Mesh component.");
        return false;
    }
    if (nodeName.empty())
    {
        setError("Entity.SetMeshNodeLocalRotationOffset requires a non-empty GLB node name.");
        return false;
    }
    if (!validVec3(rotationDegrees))
    {
        setError("Entity.SetMeshNodeLocalRotationOffset requires finite rotation values.");
        return false;
    }

    auto& overrides = slot->record.mesh->nodeOverrides;
    auto found = std::find_if(
        overrides.begin(), overrides.end(),
        [&](const MeshNodeOverride& value) { return value.nodeName == nodeName; });
    if (found == overrides.end())
    {
        overrides.push_back({});
        found = std::prev(overrides.end());
        found->nodeName = nodeName;
    }
    found->hasLocalRotationOffset = true;
    found->localRotationOffsetDegrees = rotationDegrees;
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeAnchoredWorldPose(
    EntityHandle handle,
    const std::string& rootNodeName,
    const std::string& anchorNodeName,
    const heritage::math::Vec3& worldPosition,
    const heritage::math::Vec3& localRotationDegrees)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeAnchoredWorldPose received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeAnchoredWorldPose requires a Mesh component.");
        return false;
    }
    if (rootNodeName.empty() || anchorNodeName.empty())
    {
        setError("Entity.SetMeshNodeAnchoredWorldPose requires non-empty root and anchor GLB node names.");
        return false;
    }
    if (!validVec3(worldPosition) || !validVec3(localRotationDegrees))
    {
        setError("Entity.SetMeshNodeAnchoredWorldPose requires finite position/rotation values.");
        return false;
    }

    auto& overrides = slot->record.mesh->nodeOverrides;
    auto found = std::find_if(
        overrides.begin(), overrides.end(),
        [&](const MeshNodeOverride& value) { return value.nodeName == rootNodeName; });
    if (found == overrides.end())
    {
        overrides.push_back({});
        found = std::prev(overrides.end());
        found->nodeName = rootNodeName;
    }
    found->hasAnchoredWorldPose = true;
    found->anchorNodeName = anchorNodeName;
    found->anchoredWorldPosition = worldPosition;
    found->anchoredLocalRotationDegrees = localRotationDegrees;
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeAnchoredWorldDelta(
    EntityHandle handle,
    const std::string& rootNodeName,
    const std::string& anchorNodeName,
    const heritage::math::Vec3& worldTranslationDelta,
    const heritage::math::Vec3& localRotationDeltaDegrees)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeAnchoredWorldDelta received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeAnchoredWorldDelta requires a Mesh component.");
        return false;
    }
    if (rootNodeName.empty() || anchorNodeName.empty())
    {
        setError("Entity.SetMeshNodeAnchoredWorldDelta requires non-empty root and anchor GLB node names.");
        return false;
    }
    if (!validVec3(worldTranslationDelta) || !validVec3(localRotationDeltaDegrees))
    {
        setError("Entity.SetMeshNodeAnchoredWorldDelta requires finite translation/rotation deltas.");
        return false;
    }

    auto& overrides = slot->record.mesh->nodeOverrides;
    auto found = std::find_if(
        overrides.begin(), overrides.end(),
        [&](const MeshNodeOverride& value) { return value.nodeName == rootNodeName; });
    if (found == overrides.end())
    {
        overrides.push_back({});
        found = std::prev(overrides.end());
        found->nodeName = rootNodeName;
    }

    found->hasAnchoredWorldDelta = true;
    found->deltaAnchorNodeName = anchorNodeName;
    found->anchoredWorldTranslationDelta = worldTranslationDelta;
    found->anchoredLocalRotationDeltaDegrees = localRotationDeltaDegrees;
    clearError();
    return true;
}

/* Legacy scalar/plane/probe deformation ownership was removed.  The only tire
   mesh mutation accepted by EntityRegistry is the final flexible-ring field. */

bool EntityRegistry::setMeshNodeTireDeformationField(
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
    const heritage::math::Vec3& wheelUpWorld)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity tire deformation field received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh || nodeName.empty())
    {
        setError("Entity tire deformation field requires a Mesh and tire node name.");
        return false;
    }
    const auto finiteArray = [](const auto& values)
    {
        return std::all_of(values.begin(), values.end(), [](float value)
        {
            return std::isfinite(value);
        });
    };
    if (!std::isfinite(referenceRadiusM) || referenceRadiusM <= 0.02f
        || !std::isfinite(failureTreadAttachment)
        || !std::isfinite(failureStructuralIntegrity)
        || !std::isfinite(failureEventSeed)
        || !std::isfinite(failureEventAgeSeconds)
        || !std::isfinite(wheelAngularVelocity)
        || !std::isfinite(wheelRotationRadians)
        || !finiteArray(forwardDisplacementM)
        || !finiteArray(downDisplacementM)
        || !finiteArray(lateralDisplacementM))
    {
        setError("Entity tire deformation field requires finite displacements.");
        return false;
    }

    auto& overrides = slot->record.mesh->nodeOverrides;
    auto found = std::find_if(overrides.begin(), overrides.end(),
        [&](const MeshNodeOverride& value) { return value.nodeName == nodeName; });
    if (found == overrides.end())
    {
        overrides.push_back({});
        found = std::prev(overrides.end());
        found->nodeName = nodeName;
    }

    const bool hadPreviousField = found->tireVisualDeformationFieldValid;
    const auto filter = [&](float previous, float target)
    {
        target = std::clamp(target, -0.15f, 0.15f);
        if (!hadPreviousField)
            return target;
        float delta = target - previous;
        if (std::abs(delta) < 0.00008f)
            delta = 0.0f;
        const float response = std::abs(target) >= std::abs(previous)
            ? 0.82f : 0.24f;
        const float result = previous + delta * response;
        return std::abs(result) < 0.00002f ? 0.0f : result;
    };

    float maximumM = 0.0f;
    for (std::size_t index = 0; index < TireVisualDeformationFieldCount; ++index)
    {
        const float targetForward = valid ? forwardDisplacementM[index] : 0.0f;
        const float targetDown = valid ? downDisplacementM[index] : 0.0f;
        const float targetLateral = valid ? lateralDisplacementM[index] : 0.0f;
        found->tireVisualForwardDisplacementM[index] = filter(
            found->tireVisualForwardDisplacementM[index], targetForward);
        found->tireVisualDownDisplacementM[index] = filter(
            found->tireVisualDownDisplacementM[index], targetDown);
        found->tireVisualLateralDisplacementM[index] = filter(
            found->tireVisualLateralDisplacementM[index], targetLateral);
        maximumM = std::max(maximumM, std::max(
            std::abs(found->tireVisualForwardDisplacementM[index]),
            std::max(std::abs(found->tireVisualDownDisplacementM[index]),
                std::abs(found->tireVisualLateralDisplacementM[index]))));
    }
    found->tireVisualDeformationFieldValid = valid || maximumM > 0.00005f;
    found->hasTireVisualDeformation = true;
    found->tireVisualBareRim = bareRim;
    found->tireFailureVisualStage = failureStage;
    found->tireFailureVisualTreadAttachment = std::clamp(
        failureTreadAttachment, 0.0f, 1.0f);
    found->tireFailureVisualStructuralIntegrity = std::clamp(
        failureStructuralIntegrity, 0.0f, 1.0f);
    found->tireFailureVisualEventSeed = failureEventSeed;
    found->tireFailureVisualEventAgeSeconds = std::clamp(
        failureEventAgeSeconds, 0.0f, 3600.0f);
    found->tireFailureVisualWheelAngularVelocity = std::clamp(
        wheelAngularVelocity, -2000.0f, 2000.0f);
    found->tireFailureVisualWheelRotationRadians = std::fmod(
        wheelRotationRadians, 6.28318530718f);
    found->tireReferenceRadiusM = referenceRadiusM;

    const auto normalizedOr = [](const heritage::math::Vec3& value,
        const heritage::math::Vec3& fallback)
    {
        const float length = std::sqrt(
            value.x * value.x + value.y * value.y + value.z * value.z);
        if (!std::isfinite(length) || length <= 1.0e-6f)
            return fallback;
        return heritage::math::Vec3{
            value.x / length, value.y / length, value.z / length };
    };
    found->tireWheelForwardWorld = normalizedOr(
        wheelForwardWorld, { 0.0f, 0.0f, 1.0f });
    found->tireWheelRightWorld = normalizedOr(
        wheelRightWorld, { 1.0f, 0.0f, 0.0f });
    found->tireWheelUpWorld = normalizedOr(
        wheelUpWorld, { 0.0f, 1.0f, 0.0f });
    clearError();
    return true;
}

bool EntityRegistry::clearMeshNodeOverrides(EntityHandle handle)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.ClearMeshNodeOverrides received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.ClearMeshNodeOverrides requires a Mesh component.");
        return false;
    }
    slot->record.mesh->nodeOverrides.clear();
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
    meshInstances(result);
    return result;
}

void EntityRegistry::meshInstances(std::vector<MeshInstance>& result) const
{
    // PERF03: callers that render every frame can retain vector capacity and
    // avoid allocating a fresh MeshInstance array every submission pass.
    result.clear();
    if (result.capacity() < m_aliveCount)
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
            slot.record.mesh->blenderCoordinates,
            slot.record.mesh->animationClip,
            slot.record.mesh->animationPlaying,
            slot.record.mesh->animationLoop,
            slot.record.mesh->animationSpeed,
            slot.record.mesh->animationCrossFadeSeconds,
            slot.record.mesh->animationSeekSeconds,
            slot.record.mesh->animationPlaySerial,
            slot.record.mesh->animationSeekSerial,
            slot.record.mesh->nodeNamePrefixFilter,
            slot.record.mesh->nodeOverrides
        });
    }

    clearError();
}


} // namespace heritage::entities
