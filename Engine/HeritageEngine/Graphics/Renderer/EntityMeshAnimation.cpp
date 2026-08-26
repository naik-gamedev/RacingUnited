#include "EntityMeshRenderer.hpp"
#include "EntityMeshRendererInternal.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace heritage::graphics::entity_mesh_internal {

struct RuntimeNodeState
{
    bool hasMatrix = false;
    std::array<float, 16> matrix{};
    std::array<float, 3> translation{ 0.0f, 0.0f, 0.0f };
    std::array<float, 4> rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
    std::array<float, 3> scale{ 1.0f, 1.0f, 1.0f };
};

const heritage::graphics::AnimationClip* findAnimationClip(
    const heritage::graphics::Mesh& mesh,
    const std::string& requestedName)
{
    if (mesh.animations.empty())
        return nullptr;

    if (requestedName.empty())
        return &mesh.animations.front();

    const auto found = std::find_if(
        mesh.animations.begin(),
        mesh.animations.end(),
        [&](const heritage::graphics::AnimationClip& clip)
        {
            return clip.name == requestedName;
        });
    return found != mesh.animations.end() ? &*found : nullptr;
}

float animationSampleTime(
    const heritage::graphics::AnimationClip& clip,
    double playbackSeconds,
    bool loop)
{
    if (clip.durationSeconds <= 0.0f)
        return 0.0f;

    const double duration = static_cast<double>(clip.durationSeconds);
    if (!loop)
        return static_cast<float>(std::clamp(playbackSeconds, 0.0, duration));

    double wrapped = std::fmod(playbackSeconds, duration);
    if (wrapped < 0.0)
        wrapped += duration;
    return static_cast<float>(wrapped);
}

std::size_t animationValueOffset(
    const heritage::graphics::AnimationChannel& channel,
    std::size_t keyIndex)
{
    if (channel.interpolation == heritage::graphics::AnimationInterpolation::CubicSpline)
        return keyIndex * channel.componentCount * 3 + channel.componentCount;
    return keyIndex * channel.componentCount;
}

void sampleChannelValues(
    const heritage::graphics::AnimationChannel& channel,
    float time,
    std::array<float, 4>& out)
{
    out = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (channel.times.empty() || channel.values.empty())
        return;

    const std::size_t components = channel.componentCount;
    auto copyKey = [&](std::size_t keyIndex)
    {
        const std::size_t offset = animationValueOffset(channel, keyIndex);
        if (offset + components > channel.values.size())
            return;
        for (std::size_t i = 0; i < components; ++i)
            out[i] = channel.values[offset + i];
        if (channel.path == heritage::graphics::AnimationTargetPath::Rotation)
            out = normalizedQuaternion(out);
    };

    if (channel.times.size() == 1 || time <= channel.times.front())
    {
        copyKey(0);
        return;
    }
    if (time >= channel.times.back())
    {
        copyKey(channel.times.size() - 1);
        return;
    }

    const auto upper = std::upper_bound(channel.times.begin(), channel.times.end(), time);
    const std::size_t right = static_cast<std::size_t>(upper - channel.times.begin());
    const std::size_t left = right - 1;
    if (channel.interpolation == heritage::graphics::AnimationInterpolation::Step)
    {
        copyKey(left);
        return;
    }

    const float t0 = channel.times[left];
    const float t1 = channel.times[right];
    const float delta = std::max(t1 - t0, 1.0e-6f);
    const float alpha = std::clamp((time - t0) / delta, 0.0f, 1.0f);

    if (channel.interpolation == heritage::graphics::AnimationInterpolation::CubicSpline)
    {
        const std::size_t leftBase = left * components * 3;
        const std::size_t rightBase = right * components * 3;
        if (rightBase + components * 2 > channel.values.size())
            return;

        const float alpha2 = alpha * alpha;
        const float alpha3 = alpha2 * alpha;
        const float h00 = 2.0f * alpha3 - 3.0f * alpha2 + 1.0f;
        const float h10 = alpha3 - 2.0f * alpha2 + alpha;
        const float h01 = -2.0f * alpha3 + 3.0f * alpha2;
        const float h11 = alpha3 - alpha2;

        // glTF CUBICSPLINE stores each key as:
        // in tangent, value, out tangent. Tangents are derivatives and are
        // therefore multiplied by the key interval before Hermite evaluation.
        for (std::size_t i = 0; i < components; ++i)
        {
            const float p0 = channel.values[leftBase + components + i];
            const float m0 = channel.values[leftBase + components * 2 + i] * delta;
            const float p1 = channel.values[rightBase + components + i];
            const float m1 = channel.values[rightBase + i] * delta;
            out[i] = h00 * p0 + h10 * m0 + h01 * p1 + h11 * m1;
        }
        if (channel.path == heritage::graphics::AnimationTargetPath::Rotation)
            out = normalizedQuaternion(out);
        return;
    }

    if (channel.path == heritage::graphics::AnimationTargetPath::Rotation)
    {
        std::array<float, 4> qa{};
        std::array<float, 4> qb{};
        const std::size_t leftOffset = animationValueOffset(channel, left);
        const std::size_t rightOffset = animationValueOffset(channel, right);
        for (std::size_t i = 0; i < 4; ++i)
        {
            qa[i] = channel.values[leftOffset + i];
            qb[i] = channel.values[rightOffset + i];
        }
        out = lerpQuaternion(qa, qb, alpha);
    }
    else
    {
        const std::size_t leftOffset = animationValueOffset(channel, left);
        const std::size_t rightOffset = animationValueOffset(channel, right);
        for (std::size_t i = 0; i < components; ++i)
        {
            out[i] = channel.values[leftOffset + i]
                + (channel.values[rightOffset + i] - channel.values[leftOffset + i]) * alpha;
        }
    }
}

std::vector<RuntimeNodeState> restNodeStates(
    const heritage::graphics::Mesh& mesh)
{
    std::vector<RuntimeNodeState> states(mesh.nodes.size());
    for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
    {
        const auto& source = mesh.nodes[i];
        states[i].hasMatrix = source.hasMatrix;
        states[i].matrix = source.localMatrix;
        states[i].translation = source.translation;
        states[i].rotation = source.rotation;
        states[i].scale = source.scale;
    }
    return states;
}

void applyAnimationClip(
    const heritage::graphics::AnimationClip* clip,
    float sampleTime,
    std::vector<RuntimeNodeState>& states)
{
    if (!clip)
        return;

    for (const auto& channel : clip->channels)
    {
        if (channel.nodeIndex < 0
            || static_cast<std::size_t>(channel.nodeIndex) >= states.size())
        {
            continue;
        }

        std::array<float, 4> value{};
        sampleChannelValues(channel, sampleTime, value);
        RuntimeNodeState& state = states[static_cast<std::size_t>(channel.nodeIndex)];
        state.hasMatrix = false;
        if (channel.path == heritage::graphics::AnimationTargetPath::Translation)
            state.translation = { value[0], value[1], value[2] };
        else if (channel.path == heritage::graphics::AnimationTargetPath::Scale)
            state.scale = { value[0], value[1], value[2] };
        else
            state.rotation = normalizedQuaternion({ value[0], value[1], value[2], value[3] });
    }
}

std::vector<RuntimeNodeState> blendNodeStates(
    const std::vector<RuntimeNodeState>& from,
    const std::vector<RuntimeNodeState>& to,
    float alpha)
{
    const std::size_t count = std::min(from.size(), to.size());
    std::vector<RuntimeNodeState> result(count);
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    for (std::size_t i = 0; i < count; ++i)
    {
        const RuntimeNodeState& a = from[i];
        const RuntimeNodeState& b = to[i];
        RuntimeNodeState& value = result[i];

        if (a.hasMatrix && b.hasMatrix)
        {
            value.hasMatrix = alpha < 0.5f ? a.hasMatrix : b.hasMatrix;
            value.matrix = alpha < 0.5f ? a.matrix : b.matrix;
            value.translation = alpha < 0.5f ? a.translation : b.translation;
            value.rotation = alpha < 0.5f ? a.rotation : b.rotation;
            value.scale = alpha < 0.5f ? a.scale : b.scale;
            continue;
        }

        value.hasMatrix = false;
        for (int component = 0; component < 3; ++component)
        {
            value.translation[static_cast<std::size_t>(component)] =
                a.translation[static_cast<std::size_t>(component)]
                + (b.translation[static_cast<std::size_t>(component)]
                    - a.translation[static_cast<std::size_t>(component)]) * alpha;
            value.scale[static_cast<std::size_t>(component)] =
                a.scale[static_cast<std::size_t>(component)]
                + (b.scale[static_cast<std::size_t>(component)]
                    - a.scale[static_cast<std::size_t>(component)]) * alpha;
        }
        value.rotation = lerpQuaternion(a.rotation, b.rotation, alpha);
    }
    return result;
}

std::vector<heritage::math::Mat4> globalNodeTransformsFromStates(
    const heritage::graphics::Mesh& mesh,
    const std::vector<RuntimeNodeState>& states)
{
    std::vector<heritage::math::Mat4> globals(mesh.nodes.size(), heritage::math::identity());
    if (mesh.nodes.empty())
        return globals;

    std::vector<char> visited(mesh.nodes.size(), 0);
    std::vector<char> visiting(mesh.nodes.size(), 0);
    auto evaluateNode = [&](auto&& self, int nodeIndex) -> heritage::math::Mat4
    {
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= mesh.nodes.size())
            return heritage::math::identity();
        const std::size_t index = static_cast<std::size_t>(nodeIndex);
        if (visited[index])
            return globals[index];
        if (visiting[index])
            return heritage::math::identity();
        visiting[index] = 1;

        const RuntimeNodeState& state = states[index];
        heritage::math::Mat4 local = state.hasMatrix
            ? arrayMatrix(state.matrix)
            : trsMatrix(state.translation, state.rotation, state.scale);
        const int parent = mesh.nodes[index].parentIndex;
        globals[index] = parent >= 0
            ? multiply(self(self, parent), local)
            : local;
        visiting[index] = 0;
        visited[index] = 1;
        return globals[index];
    };

    for (std::size_t i = 0; i < mesh.nodes.size(); ++i)
        evaluateNode(evaluateNode, static_cast<int>(i));
    return globals;
}

std::vector<heritage::math::Mat4> evaluateGlobalNodeTransforms(
    const heritage::graphics::Mesh& mesh,
    const heritage::graphics::AnimationClip* activeClip,
    double activePlaybackSeconds,
    bool activeLoop,
    const heritage::graphics::AnimationClip* previousClip,
    double previousPlaybackSeconds,
    bool previousLoop,
    float blendAlpha)
{
    std::vector<RuntimeNodeState> activeStates = restNodeStates(mesh);
    if (activeClip)
    {
        applyAnimationClip(
            activeClip,
            animationSampleTime(*activeClip, activePlaybackSeconds, activeLoop),
            activeStates);
    }

    if (!previousClip || blendAlpha >= 1.0f)
        return globalNodeTransformsFromStates(mesh, activeStates);

    std::vector<RuntimeNodeState> previousStates = restNodeStates(mesh);
    applyAnimationClip(
        previousClip,
        animationSampleTime(*previousClip, previousPlaybackSeconds, previousLoop),
        previousStates);
    return globalNodeTransformsFromStates(
        mesh,
        blendNodeStates(previousStates, activeStates, blendAlpha));
}

bool nodeMatchesPrefixFilter(
    const heritage::graphics::Mesh& mesh,
    int nodeIndex,
    const std::string& prefix)
{
    if (prefix.empty())
        return true;

    int current = nodeIndex;
    std::size_t guard = 0;
    while (current >= 0
        && static_cast<std::size_t>(current) < mesh.nodes.size()
        && guard++ <= mesh.nodes.size())
    {
        const auto& node = mesh.nodes[static_cast<std::size_t>(current)];
        if (node.name.size() >= prefix.size()
            && node.name.compare(0, prefix.size(), prefix) == 0)
        {
            return true;
        }
        current = node.parentIndex;
    }

    return false;
}

void applyMeshNodeOverrides(
    const heritage::graphics::Mesh& mesh,
    const heritage::entities::MeshInstance& instance,
    const heritage::math::Mat4& instanceModel,
    const heritage::math::Vec3& cameraOrigin,
    std::vector<heritage::math::Mat4>& globals)
{
    if (instance.nodeOverrides.empty()
        || mesh.nodes.empty()
        || globals.size() != mesh.nodes.size())
    {
        return;
    }

    std::unordered_map<std::string, const heritage::entities::MeshNodeOverride*> overrides;
    overrides.reserve(instance.nodeOverrides.size());
    for (const auto& value : instance.nodeOverrides)
    {
        if (!value.nodeName.empty())
            overrides[value.nodeName] = &value;
    }
    if (overrides.empty())
        return;

    const std::vector<heritage::math::Mat4> baseGlobals = globals;
    const heritage::math::Mat4 inverseInstanceModel = inverseMatrix(instanceModel);
    std::vector<char> visited(mesh.nodes.size(), 0);
    std::vector<char> visiting(mesh.nodes.size(), 0);

    auto evaluate = [&](auto&& self, int nodeIndex) -> heritage::math::Mat4
    {
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= mesh.nodes.size())
            return heritage::math::identity();
        const std::size_t index = static_cast<std::size_t>(nodeIndex);
        if (visited[index])
            return globals[index];
        if (visiting[index])
            return baseGlobals[index];
        visiting[index] = 1;

        const int parentIndex = mesh.nodes[index].parentIndex;
        heritage::math::Mat4 baseLocal = baseGlobals[index];
        if (parentIndex >= 0
            && static_cast<std::size_t>(parentIndex) < baseGlobals.size())
        {
            baseLocal = multiply(
                inverseMatrix(baseGlobals[static_cast<std::size_t>(parentIndex)]),
                baseGlobals[index]);
        }

        heritage::math::Mat4 local = baseLocal;
        const auto found = overrides.find(mesh.nodes[index].name);

        heritage::math::Mat4 current = parentIndex >= 0
            ? multiply(self(self, parentIndex), local)
            : local;

        if (found != overrides.end() && found->second->hasAnchoredWorldDelta)
        {
            int anchorIndex = -1;
            for (std::size_t candidate = 0; candidate < mesh.nodes.size(); ++candidate)
            {
                if (mesh.nodes[candidate].name == found->second->deltaAnchorNodeName)
                {
                    anchorIndex = static_cast<int>(candidate);
                    break;
                }
            }
            if (anchorIndex >= 0
                && static_cast<std::size_t>(anchorIndex) < baseGlobals.size())
            {
                const heritage::math::Mat4& baseAnchor =
                    baseGlobals[static_cast<std::size_t>(anchorIndex)];
                const heritage::math::Vec3 baseAnchorPosition{
                    baseAnchor.m[12], baseAnchor.m[13], baseAnchor.m[14] };

                const heritage::math::Vec3& worldDelta =
                    found->second->anchoredWorldTranslationDelta;
                const heritage::math::Vec3 localDelta{
                    inverseInstanceModel.m[0] * worldDelta.x
                        + inverseInstanceModel.m[4] * worldDelta.y
                        + inverseInstanceModel.m[8] * worldDelta.z,
                    inverseInstanceModel.m[1] * worldDelta.x
                        + inverseInstanceModel.m[5] * worldDelta.y
                        + inverseInstanceModel.m[9] * worldDelta.z,
                    inverseInstanceModel.m[2] * worldDelta.x
                        + inverseInstanceModel.m[6] * worldDelta.y
                        + inverseInstanceModel.m[10] * worldDelta.z };

                const heritage::math::Vec3 movedAnchor{
                    baseAnchorPosition.x + localDelta.x,
                    baseAnchorPosition.y + localDelta.y,
                    baseAnchorPosition.z + localDelta.z };

                const heritage::math::Mat4 subtreeDelta = multiply(
                    multiply(
                        translation(movedAnchor),
                        eulerRotationDegrees(
                            found->second->anchoredLocalRotationDeltaDegrees)),
                    translation({
                        -baseAnchorPosition.x,
                        -baseAnchorPosition.y,
                        -baseAnchorPosition.z }));

                // With zero deltas, subtreeDelta is identity and baseGlobals
                // passes through untouched: exact Blender/glTF bind pose.
                current = multiply(subtreeDelta, baseGlobals[index]);
            }
        }
        else if (found != overrides.end() && found->second->hasAnchoredWorldPose)
        {
            // Preserve the complete GLB-authored wheel-corner subtree instead
            // of forcing Root and Pivot into independent absolute poses. Find
            // the authored anchor (for VA02C this is WH_*_Pivot), translate it
            // to the native wheel center, then compose ONLY the native local
            // upright delta around that anchor. Applying one rigid delta to the
            // subtree keeps calipers beside their discs and preserves the GLB's
            // independently authored left/right wheel facing.
            int anchorIndex = -1;
            for (std::size_t candidate = 0; candidate < mesh.nodes.size(); ++candidate)
            {
                if (mesh.nodes[candidate].name == found->second->anchorNodeName)
                {
                    anchorIndex = static_cast<int>(candidate);
                    break;
                }
            }

            if (anchorIndex >= 0
                && static_cast<std::size_t>(anchorIndex) < baseGlobals.size())
            {
                const heritage::math::Mat4& baseAnchor =
                    baseGlobals[static_cast<std::size_t>(anchorIndex)];
                const heritage::math::Vec3 baseAnchorPosition{
                    baseAnchor.m[12], baseAnchor.m[13], baseAnchor.m[14] };

                const heritage::math::Vec3& absoluteWorldPosition =
                    found->second->anchoredWorldPosition;
                const heritage::math::Vec3 worldPosition{
                    absoluteWorldPosition.x - cameraOrigin.x,
                    absoluteWorldPosition.y - cameraOrigin.y,
                    absoluteWorldPosition.z - cameraOrigin.z
                };
                const heritage::math::Vec3 desiredLocalPosition{
                    inverseInstanceModel.m[0] * worldPosition.x
                        + inverseInstanceModel.m[4] * worldPosition.y
                        + inverseInstanceModel.m[8] * worldPosition.z
                        + inverseInstanceModel.m[12],
                    inverseInstanceModel.m[1] * worldPosition.x
                        + inverseInstanceModel.m[5] * worldPosition.y
                        + inverseInstanceModel.m[9] * worldPosition.z
                        + inverseInstanceModel.m[13],
                    inverseInstanceModel.m[2] * worldPosition.x
                        + inverseInstanceModel.m[6] * worldPosition.y
                        + inverseInstanceModel.m[10] * worldPosition.z
                        + inverseInstanceModel.m[14] };

                const heritage::math::Mat4 subtreeDelta = multiply(
                    multiply(
                        translation(desiredLocalPosition),
                        eulerRotationDegrees(
                            found->second->anchoredLocalRotationDegrees)),
                    translation({
                        -baseAnchorPosition.x,
                        -baseAnchorPosition.y,
                        -baseAnchorPosition.z }));
                current = multiply(subtreeDelta, baseGlobals[index]);
            }
        }
        else if (found != overrides.end() && found->second->hasWorldPose)
        {
            // Convert the requested engine-world pose into the GLB instance's
            // local space. Descendants then inherit from this authoritative
            // node while preserving their authored relative transforms.
            const heritage::math::Vec3& absoluteWorldPosition =
                found->second->worldPosition;
            const heritage::math::Vec3 cameraRelativeWorldPosition{
                absoluteWorldPosition.x - cameraOrigin.x,
                absoluteWorldPosition.y - cameraOrigin.y,
                absoluteWorldPosition.z - cameraOrigin.z
            };
            current = multiply(
                inverseInstanceModel,
                worldPoseMatrix(
                    cameraRelativeWorldPosition,
                    found->second->worldRotationDegrees));
        }

        if (found != overrides.end() && found->second->hasLocalRotationOffset)
        {
            // Local offsets are deliberately composed AFTER an authoritative
            // world pose. VA02A uses this for WH_*_Pivot: the native wheel
            // center/upright establishes the pivot pose, then wheel spin is
            // applied around that exact runtime axis. This avoids requiring
            // the authored Root->Pivot translation to be perfectly centered.
            current = multiply(
                current,
                eulerRotationDegrees(found->second->localRotationOffsetDegrees));
        }

        globals[index] = current;
        visiting[index] = 0;
        visited[index] = 1;
        return current;
    };

    for (std::size_t index = 0; index < mesh.nodes.size(); ++index)
        evaluate(evaluate, static_cast<int>(index));
}

std::vector<heritage::math::Mat4> buildSkinPalette(
    const heritage::graphics::Mesh& mesh,
    const heritage::graphics::MeshDrawRange& range,
    const std::vector<heritage::math::Mat4>& nodeGlobals)
{
    if (range.skinIndex < 0
        || static_cast<std::size_t>(range.skinIndex) >= mesh.skins.size()
        || range.nodeIndex < 0
        || static_cast<std::size_t>(range.nodeIndex) >= nodeGlobals.size())
    {
        return {};
    }

    const auto& skin = mesh.skins[static_cast<std::size_t>(range.skinIndex)];
    const heritage::math::Mat4 inverseMeshNode = inverseMatrix(nodeGlobals[static_cast<std::size_t>(range.nodeIndex)]);
    std::vector<heritage::math::Mat4> palette;
    palette.reserve(std::min<std::size_t>(skin.joints.size(), kMaxSkinJoints));
    for (std::size_t jointIndex = 0; jointIndex < skin.joints.size() && jointIndex < static_cast<std::size_t>(kMaxSkinJoints); ++jointIndex)
    {
        const int nodeIndex = skin.joints[jointIndex];
        heritage::math::Mat4 jointGlobal = heritage::math::identity();
        if (nodeIndex >= 0 && static_cast<std::size_t>(nodeIndex) < nodeGlobals.size())
            jointGlobal = nodeGlobals[static_cast<std::size_t>(nodeIndex)];
        palette.push_back(multiply(multiply(inverseMeshNode, jointGlobal), arrayMatrix(skin.inverseBindMatrices[jointIndex])));
    }
    return palette;
}

} // namespace heritage::graphics::entity_mesh_internal

namespace heritage::graphics {
using namespace entity_mesh_internal;

std::vector<heritage::math::Mat4> EntityMeshRenderer::animationTransformsForInstance(
    const Mesh& mesh,
    const heritage::entities::MeshInstance& instance,
    double elapsedSeconds)
{
    if (mesh.nodes.empty())
        return {};

    AnimationRuntimeState& runtime = m_animationStates[instance.entity];
    const AnimationClip* requestedClip = findAnimationClip(mesh, instance.animationClip);
    const std::string requestedName = requestedClip ? requestedClip->name : std::string();
    if (!instance.animationClip.empty() && !requestedClip)
    {
        const std::string warning =
            "Animation clip '" + instance.animationClip
            + "' was not found in " + instance.assetPath;
        if (m_reportedAnimationWarnings.insert(warning).second)
            std::cerr << "GLB animation warning: " << warning << '\n';
    }

    if (!runtime.initialized)
    {
        runtime.initialized = true;
        runtime.playSerial = instance.animationPlaySerial;
        runtime.seekSerial = instance.animationSeekSerial;
        runtime.activeClip = requestedName;
        runtime.activeTimeSeconds = instance.animationSeekSeconds;
        runtime.lastEngineTimeSeconds = elapsedSeconds;
    }

    const double deltaSeconds = std::clamp(
        elapsedSeconds - runtime.lastEngineTimeSeconds,
        0.0,
        0.25);
    runtime.lastEngineTimeSeconds = elapsedSeconds;

    if (runtime.playSerial != instance.animationPlaySerial
        || runtime.activeClip != requestedName)
    {
        const bool canBlend =
            !runtime.activeClip.empty()
            && !requestedName.empty()
            && runtime.activeClip != requestedName
            && instance.animationCrossFadeSeconds > 0.0f;

        if (canBlend)
        {
            runtime.previousClip = runtime.activeClip;
            runtime.previousTimeSeconds = runtime.activeTimeSeconds;
            runtime.blendDurationSeconds = instance.animationCrossFadeSeconds;
            runtime.blendElapsedSeconds = 0.0f;
        }
        else
        {
            runtime.previousClip.clear();
            runtime.previousTimeSeconds = 0.0;
            runtime.blendDurationSeconds = 0.0f;
            runtime.blendElapsedSeconds = 0.0f;
        }

        runtime.activeClip = requestedName;
        runtime.activeTimeSeconds = 0.0;
        runtime.playSerial = instance.animationPlaySerial;
    }

    if (runtime.seekSerial != instance.animationSeekSerial)
    {
        runtime.activeTimeSeconds = instance.animationSeekSeconds;
        runtime.seekSerial = instance.animationSeekSerial;
    }

    if (instance.animationPlaying)
    {
        const double scaledDelta = deltaSeconds * static_cast<double>(instance.animationSpeed);
        runtime.activeTimeSeconds += scaledDelta;
        if (!runtime.previousClip.empty())
            runtime.previousTimeSeconds += scaledDelta;
        if (runtime.blendDurationSeconds > 0.0f)
            runtime.blendElapsedSeconds += static_cast<float>(deltaSeconds);
    }

    const AnimationClip* activeClip = findAnimationClip(mesh, runtime.activeClip);
    const AnimationClip* previousClip = findAnimationClip(mesh, runtime.previousClip);

    float blendAlpha = 1.0f;
    if (previousClip && runtime.blendDurationSeconds > 0.0f)
    {
        blendAlpha = std::clamp(
            runtime.blendElapsedSeconds / runtime.blendDurationSeconds,
            0.0f,
            1.0f);
        if (blendAlpha >= 1.0f)
        {
            runtime.previousClip.clear();
            previousClip = nullptr;
        }
    }

    return evaluateGlobalNodeTransforms(
        mesh,
        activeClip,
        runtime.activeTimeSeconds,
        instance.animationLoop,
        previousClip,
        runtime.previousTimeSeconds,
        instance.animationLoop,
        blendAlpha);
}

void EntityMeshRenderer::prepareFrameInstances(
    const std::vector<heritage::entities::MeshInstance>& instances,
    const heritage::math::Vec3& eye,
    float elapsedSeconds)
{
    using PrepareClock = std::chrono::steady_clock;

    // Keep one slot per registry instance so the visible pass can index this
    // cache without a hash lookup. Existing nested-vector capacity survives
    // across frames when the instance count is stable.
    if (m_preparedFrameInstanceScratch.size() < instances.size())
        m_preparedFrameInstanceScratch.resize(instances.size());

    for (std::size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
    {
        const auto prepareStart = PrepareClock::now();
        const auto& instance = instances[instanceIndex];
        PreparedFrameInstance& prepared =
            m_preparedFrameInstanceScratch[instanceIndex];

        prepared.instance = &instance;
        prepared.mesh = acquireMesh(
            instance.assetPath,
            instance.normalize,
            instance.blenderCoordinates);
        prepared.nodeGlobals.clear();
        prepared.tireVisualOverrides.clear();

        if (prepared.mesh)
        {
            heritage::entities::MeshInstance cameraRelativeInstance = instance;
            cameraRelativeInstance.position = {
                instance.position.x - eye.x,
                instance.position.y - eye.y,
                instance.position.z - eye.z
            };
            prepared.instanceModel = modelMatrix(cameraRelativeInstance);
            prepared.nodeGlobals = animationTransformsForInstance(
                *prepared.mesh,
                instance,
                elapsedSeconds);
            applyMeshNodeOverrides(
                *prepared.mesh,
                instance,
                prepared.instanceModel,
                eye,
                prepared.nodeGlobals);

            // Resolve the sparse tire-deformation overrides once per instance.
            // Both the shadow and material passes used to repeat string scans.
            prepared.tireVisualOverrides.assign(
                prepared.mesh->nodes.size(), nullptr);
            for (const auto& overrideValue : instance.nodeOverrides)
            {
                if (!overrideValue.hasTireVisualDeformation)
                    continue;
                for (std::size_t nodeIndex = 0;
                     nodeIndex < prepared.mesh->nodes.size();
                     ++nodeIndex)
                {
                    if (prepared.mesh->nodes[nodeIndex].name == overrideValue.nodeName)
                    {
                        prepared.tireVisualOverrides[nodeIndex] = &overrideValue;
                        break;
                    }
                }
            }
        }
        else
        {
            prepared.instanceModel = heritage::math::identity();
        }

        prepared.prepareCpuMs = std::chrono::duration<double, std::milli>(
            PrepareClock::now() - prepareStart).count();
    }

    if (m_preparedFrameInstanceScratch.size() > instances.size())
        m_preparedFrameInstanceScratch.resize(instances.size());
}

} // namespace heritage::graphics
