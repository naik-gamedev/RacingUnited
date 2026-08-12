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

bool EntityRegistry::setMeshNodeTireDeformation(
    EntityHandle handle,
    const std::string& nodeName,
    bool grounded,
    float referenceRadiusM,
    float radialDeflectionM,
    float contactPatchLengthM,
    float contactPatchWidthM,
    float ringRadialOffsetM,
    float ringLongitudinalOffsetM,
    float ringLateralOffsetM,
    float ringYawDegrees,
    float ringWindupDegrees,
    float flatSpotDepthM,
    float flatSpotSector,
    const heritage::math::Vec3& contactNormalWorld,
    float contactPlaneDistanceM,
    bool supportGridValid,
    float supportHalfLengthM,
    float supportHalfWidthM,
    const std::array<float, 9>& supportHeightResidualM,
    const heritage::math::Vec3& wheelForwardWorld,
    const heritage::math::Vec3& wheelRightWorld,
    float normalForceN,
    float longitudinalForceN,
    float lateralForceN,
    float visualMotionSpeedMps)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeTireDeformation received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeTireDeformation requires a Mesh component.");
        return false;
    }
    if (nodeName.empty())
    {
        setError("Entity.SetMeshNodeTireDeformation requires a non-empty GLB tire node name.");
        return false;
    }

    const float values[] = {
        referenceRadiusM, radialDeflectionM, contactPatchLengthM,
        contactPatchWidthM, ringRadialOffsetM, ringLongitudinalOffsetM,
        ringLateralOffsetM, ringYawDegrees, ringWindupDegrees,
        flatSpotDepthM, flatSpotSector, contactNormalWorld.x,
        contactNormalWorld.y, contactNormalWorld.z, contactPlaneDistanceM,
        supportHalfLengthM, supportHalfWidthM,
        wheelForwardWorld.x, wheelForwardWorld.y, wheelForwardWorld.z,
        wheelRightWorld.x, wheelRightWorld.y, wheelRightWorld.z,
        normalForceN, longitudinalForceN, lateralForceN, visualMotionSpeedMps
    };
    for (float value : values)
    {
        if (!std::isfinite(value))
        {
            setError("Entity.SetMeshNodeTireDeformation requires finite physics state.");
            return false;
        }
    }
    for (float value : supportHeightResidualM)
    {
        if (!std::isfinite(value))
        {
            setError("Entity.SetMeshNodeTireDeformation support-grid heights must be finite.");
            return false;
        }
    }
    if (referenceRadiusM <= 0.02f
        || radialDeflectionM < 0.0f
        || contactPatchLengthM < 0.0f
        || contactPatchWidthM < 0.0f
        || flatSpotDepthM < 0.0f
        || contactPlaneDistanceM < 0.0f
        || supportHalfLengthM < 0.0f
        || supportHalfWidthM < 0.0f)
    {
        setError("Entity.SetMeshNodeTireDeformation received invalid tire dimensions/state.");
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

    found->hasTireVisualDeformation = true;
    found->tireGrounded = grounded;
    found->tireReferenceRadiusM = referenceRadiusM;
    found->tireRadialDeflectionM = std::clamp(radialDeflectionM, 0.0f, referenceRadiusM * 0.35f);
    found->tireContactPatchLengthM = std::clamp(contactPatchLengthM, 0.0f, referenceRadiusM * 1.5f);
    found->tireContactPatchWidthM = std::max(contactPatchWidthM, 0.0f);
    found->tireRingRadialOffsetM = std::clamp(ringRadialOffsetM, -0.05f, 0.05f);
    found->tireRingLongitudinalOffsetM = std::clamp(ringLongitudinalOffsetM, -0.05f, 0.05f);
    found->tireRingLateralOffsetM = std::clamp(ringLateralOffsetM, -0.05f, 0.05f);
    found->tireRingYawDegrees = std::clamp(ringYawDegrees, -20.0f, 20.0f);
    found->tireRingWindupDegrees = std::clamp(ringWindupDegrees, -20.0f, 20.0f);
    found->tireFlatSpotDepthM = std::clamp(flatSpotDepthM, 0.0f, referenceRadiusM * 0.08f);
    found->tireFlatSpotSector = std::fmod(flatSpotSector, 16.0f);
    if (found->tireFlatSpotSector < 0.0f)
        found->tireFlatSpotSector += 16.0f;
    const float normalLength = std::sqrt(
        contactNormalWorld.x * contactNormalWorld.x
        + contactNormalWorld.y * contactNormalWorld.y
        + contactNormalWorld.z * contactNormalWorld.z);
    found->tireContactNormalWorld = normalLength > 1.0e-5f
        ? heritage::math::Vec3{
            contactNormalWorld.x / normalLength,
            contactNormalWorld.y / normalLength,
            contactNormalWorld.z / normalLength }
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const auto normalizedOr = [](
        const heritage::math::Vec3& value,
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
        wheelForwardWorld, heritage::math::Vec3{ 0.0f, 0.0f, 1.0f });
    found->tireWheelRightWorld = normalizedOr(
        wheelRightWorld, heritage::math::Vec3{ 1.0f, 0.0f, 0.0f });
    found->tireNormalForceN = std::clamp(normalForceN, 0.0f, 250000.0f);
    found->tireLongitudinalForceN = std::clamp(longitudinalForceN, -250000.0f, 250000.0f);
    found->tireLateralForceN = std::clamp(lateralForceN, -250000.0f, 250000.0f);
    found->tireVisualMotionSpeedMps = std::clamp(visualMotionSpeedMps, 0.0f, 400.0f);
    found->tireContactPlaneDistanceM = std::clamp(
        contactPlaneDistanceM, 0.0f, referenceRadiusM * 1.5f);
    found->tireVisualSupportGridValid = supportGridValid
        && supportHalfLengthM > 0.005f && supportHalfWidthM > 0.005f;
    found->tireVisualSupportHalfLengthM = std::clamp(
        supportHalfLengthM, 0.0f, referenceRadiusM * 1.5f);
    found->tireVisualSupportHalfWidthM = std::clamp(
        supportHalfWidthM, 0.0f, std::max(contactPatchWidthM, 0.01f));
    found->tireVisualSupportHeightResidualM = supportHeightResidualM;
    const float supportLimit = std::min(referenceRadiusM * 0.20f, 0.06f);
    for (float& value : found->tireVisualSupportHeightResidualM)
        value = std::clamp(value, -supportLimit, supportLimit);
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeTireColliderTriangles(
    EntityHandle handle,
    const std::string& nodeName,
    bool valid,
    std::uint32_t triangleCount,
    const std::array<TireVisualColliderTriangle, TireVisualColliderTriangleLimit>& triangles)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity.SetMeshNodeTireColliderTriangles received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity.SetMeshNodeTireColliderTriangles requires a Mesh component.");
        return false;
    }
    if (nodeName.empty())
    {
        setError("Entity.SetMeshNodeTireColliderTriangles requires a non-empty GLB tire node name.");
        return false;
    }

    const std::uint32_t clampedCount = (std::min)(
        triangleCount, static_cast<std::uint32_t>(TireVisualColliderTriangleLimit));
    auto finiteVec = [](const heritage::math::Vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    for (std::uint32_t i = 0; i < clampedCount; ++i)
    {
        if (!finiteVec(triangles[i].a) || !finiteVec(triangles[i].b)
            || !finiteVec(triangles[i].c) || !finiteVec(triangles[i].normal))
        {
            setError("Entity.SetMeshNodeTireColliderTriangles requires finite triangle geometry.");
            return false;
        }
    }

    MeshNodeOverride* found = nullptr;
    for (auto& nodeOverride : slot->record.mesh->nodeOverrides)
    {
        if (nodeOverride.nodeName == nodeName)
        {
            found = &nodeOverride;
            break;
        }
    }
    if (!found)
    {
        slot->record.mesh->nodeOverrides.push_back({});
        found = &slot->record.mesh->nodeOverrides.back();
        found->nodeName = nodeName;
    }
    found->tireVisualColliderTrianglesValid = valid && clampedCount > 0;
    found->tireVisualColliderTriangleCount = clampedCount;
    found->tireVisualColliderTriangles = triangles;
    clearError();
    return true;
}

bool EntityRegistry::setMeshNodeTireProbeGrid(
    EntityHandle handle,
    const std::string& nodeName,
    bool valid,
    const std::array<float, TireVisualProbeCount>& compressionM,
    const heritage::math::Vec3& wheelForwardWorld,
    const heritage::math::Vec3& wheelRightWorld)
{
    Slot* slot = resolve(handle);
    if (!slot)
    {
        setError("Entity tire-probe grid received an invalid or stale handle.");
        return false;
    }
    if (!slot->record.mesh)
    {
        setError("Entity tire-probe grid requires a Mesh component.");
        return false;
    }
    if (nodeName.empty())
    {
        setError("Entity tire-probe grid requires a non-empty GLB tire node name.");
        return false;
    }

    for (float value : compressionM)
    {
        if (!std::isfinite(value))
        {
            setError("Entity tire-probe grid requires finite compression values.");
            return false;
        }
    }

    MeshNodeOverride* found = nullptr;
    for (auto& nodeOverride : slot->record.mesh->nodeOverrides)
    {
        if (nodeOverride.nodeName == nodeName)
        {
            found = &nodeOverride;
            break;
        }
    }
    if (!found)
    {
        slot->record.mesh->nodeOverrides.push_back({});
        found = &slot->record.mesh->nodeOverrides.back();
        found->nodeName = nodeName;
    }

    // TIRE33/VIS26 temporal carcass relaxation. The probe solver is sampled
    // from collision queries and can move by fractions of a millimetre even while
    // the car is standing still. Feeding those samples straight to the GPU made
    // the rubber visibly buzz. Keep the physics/contact result untouched, but
    // present the reduced-order carcass state with fast compression, slower release
    // and a tiny sub-millimetre deadband. This mirrors the fact that a pneumatic
    // carcass has inertia/damping and cannot teleport between equilibrium shapes.
    const bool hadPreviousProbeGrid = found->tireVisualProbeGridValid;
    const auto previousProbeCompressionM = found->tireVisualProbeCompressionM;
    float maximumFilteredCompressionM = 0.0f;
    for (std::size_t index = 0; index < found->tireVisualProbeCompressionM.size(); ++index)
    {
        const float target = valid
            ? std::clamp(compressionM[index], 0.0f, 0.12f)
            : 0.0f;
        float filtered = target;
        if (hadPreviousProbeGrid)
        {
            const float previous = previousProbeCompressionM[index];
            float delta = target - previous;
            if (valid && std::abs(delta) < 0.00015f)
                delta = 0.0f;
            const float response = delta >= 0.0f ? 0.88f : 0.20f;
            filtered = previous + delta * response;
            if (!valid && previous < 0.00015f)
                filtered = 0.0f;
        }
        if (filtered < 0.00003f)
            filtered = 0.0f;
        found->tireVisualProbeCompressionM[index] = std::clamp(
            filtered, 0.0f, 0.12f);
        maximumFilteredCompressionM = (std::max)(
            maximumFilteredCompressionM,
            found->tireVisualProbeCompressionM[index]);
    }
    found->tireVisualProbeGridValid = valid
        || maximumFilteredCompressionM > 0.00008f;

    // TIRE30/VIS23: the probe grid and the basis used to interpret its
    // width/circumference coordinates are one atomic presentation state.
    // Do not leave tireWheelRightWorld/tireWheelForwardWorld owned by an
    // earlier Lua telemetry call that may have resolved a different native
    // wheel. TIRE28C live testing exposed exactly that split ownership on RL:
    // the correct contact magnitude reached the correct visible tire, but the
    // width coordinate was mirrored inside that tire.
    const auto normalizedOr = [](
        const heritage::math::Vec3& value,
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
        wheelForwardWorld, heritage::math::Vec3{ 0.0f, 0.0f, 1.0f });
    found->tireWheelRightWorld = normalizedOr(
        wheelRightWorld, heritage::math::Vec3{ 1.0f, 0.0f, 0.0f });
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
