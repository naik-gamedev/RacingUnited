#include "EntityMeshRenderer.hpp"
#include "EntityMeshRendererInternal.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace heritage::graphics::entity_mesh_internal {

heritage::math::Mat4 scaleSafeLocalRotationOffset(
    const heritage::math::Mat4& authoredLocal,
    const heritage::math::Vec3& rotationOffsetDegrees)
{
    // VA02J: local wheel-spin offsets used to be composed as (T*R*S)*Q.
    // That is only equivalent to a real local rotation when S is uniform. A
    // Blender-authored/mirrored/non-uniform Pivot therefore made visible rim
    // spokes wobble/shear while the round tire looked deceptively normal.
    // Recover an orthogonal TRS local transform and compose T*R*Q*S instead.
    const auto columnLength = [&](int base) {
        return std::sqrt(
            authoredLocal.m[base] * authoredLocal.m[base]
            + authoredLocal.m[base + 1] * authoredLocal.m[base + 1]
            + authoredLocal.m[base + 2] * authoredLocal.m[base + 2]);
    };
    float sx = columnLength(0);
    float sy = columnLength(4);
    float sz = columnLength(8);
    if (sx <= 1.0e-7f || sy <= 1.0e-7f || sz <= 1.0e-7f)
        return multiply(authoredLocal, eulerRotationDegrees(rotationOffsetDegrees));

    heritage::math::Vec3 x{
        authoredLocal.m[0] / sx, authoredLocal.m[1] / sx, authoredLocal.m[2] / sx };
    heritage::math::Vec3 y{
        authoredLocal.m[4] / sy, authoredLocal.m[5] / sy, authoredLocal.m[6] / sy };
    heritage::math::Vec3 z{
        authoredLocal.m[8] / sz, authoredLocal.m[9] / sz, authoredLocal.m[10] / sz };
    const float xy = x.x * y.x + x.y * y.y + x.z * y.z;
    const float xz = x.x * z.x + x.y * z.y + x.z * z.z;
    const float yz = y.x * z.x + y.y * z.y + y.z * z.z;
    if (std::max({ std::abs(xy), std::abs(xz), std::abs(yz) }) > 2.0e-3f)
    {
        // A genuine authored shear cannot be represented as TRS without loss.
        // Preserve legacy semantics rather than silently changing such assets.
        return multiply(authoredLocal, eulerRotationDegrees(rotationOffsetDegrees));
    }

    const float determinant =
        x.x * (y.y * z.z - y.z * z.y)
        - y.x * (x.y * z.z - x.z * z.y)
        + z.x * (x.y * y.z - x.z * y.y);
    if (determinant < 0.0f)
    {
        // Move the reflection sign into X scale while keeping the reconstructed
        // bind matrix exactly equivalent. Wheel spin itself then remains a
        // proper right-handed rotation between R and S.
        x.x = -x.x; x.y = -x.y; x.z = -x.z;
        sx = -sx;
    }

    heritage::math::Mat4 rotation = heritage::math::identity();
    rotation.m[0] = x.x; rotation.m[1] = x.y; rotation.m[2] = x.z;
    rotation.m[4] = y.x; rotation.m[5] = y.y; rotation.m[6] = y.z;
    rotation.m[8] = z.x; rotation.m[9] = z.y; rotation.m[10] = z.z;
    const heritage::math::Vec3 position{
        authoredLocal.m[12], authoredLocal.m[13], authoredLocal.m[14] };
    return multiply(
        translation(position),
        multiply(
            rotation,
            multiply(
                eulerRotationDegrees(rotationOffsetDegrees),
                scaleMatrix({ sx, sy, sz }))));
}


heritage::math::Mat4 axisAngleRotation(
    heritage::math::Vec3 axis,
    float angleDegrees)
{
    const float lengthSquared =
        axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (lengthSquared <= 1.0e-16f)
        return heritage::math::identity();
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    axis.x *= inverseLength;
    axis.y *= inverseLength;
    axis.z *= inverseLength;

    constexpr float kDegreesToRadians =
        3.14159265358979323846f / 180.0f;
    const float angle = angleDegrees * kDegreesToRadians;
    const float c = std::cos(angle);
    const float sn = std::sin(angle);
    const float t = 1.0f - c;

    heritage::math::Mat4 result = heritage::math::identity();
    // Rodrigues matrix in the renderer's column-major Mat4 layout.
    result.m[0] = t * axis.x * axis.x + c;
    result.m[4] = t * axis.x * axis.y - sn * axis.z;
    result.m[8] = t * axis.x * axis.z + sn * axis.y;
    result.m[1] = t * axis.x * axis.y + sn * axis.z;
    result.m[5] = t * axis.y * axis.y + c;
    result.m[9] = t * axis.y * axis.z - sn * axis.x;
    result.m[2] = t * axis.x * axis.z - sn * axis.y;
    result.m[6] = t * axis.y * axis.z + sn * axis.x;
    result.m[10] = t * axis.z * axis.z + c;
    return result;
}

int semanticWheelTireNodeIndex(
    const heritage::graphics::Mesh& mesh,
    int pivotNodeIndex)
{
    if (pivotNodeIndex < 0
        || static_cast<std::size_t>(pivotNodeIndex) >= mesh.nodes.size())
        return -1;
    const std::string& pivotName =
        mesh.nodes[static_cast<std::size_t>(pivotNodeIndex)].name;
    constexpr const char* kPivotSuffix = "_Pivot";
    constexpr std::size_t kPivotSuffixLength = 6;
    if (pivotName.size() <= kPivotSuffixLength
        || pivotName.compare(
            pivotName.size() - kPivotSuffixLength,
            kPivotSuffixLength, kPivotSuffix) != 0)
        return -1;

    const std::string tireName =
        pivotName.substr(0, pivotName.size() - kPivotSuffixLength) + "_Tire";
    for (std::size_t index = 0; index < mesh.nodes.size(); ++index)
    {
        const auto& candidate = mesh.nodes[index];
        if (candidate.name != tireName || !candidate.hasTireVisualGeometry)
            continue;

        // The tire must actually live below this Pivot. Do not let an unrelated
        // similarly-named node redefine a generic local-rotation override.
        int parent = static_cast<int>(index);
        while (parent >= 0
            && static_cast<std::size_t>(parent) < mesh.nodes.size())
        {
            if (parent == pivotNodeIndex)
                return static_cast<int>(index);
            parent = mesh.nodes[static_cast<std::size_t>(parent)].parentIndex;
        }
    }
    return -1;
}

heritage::math::Vec3 transformDirection(
    const heritage::math::Mat4& matrix,
    const heritage::math::Vec3& direction)
{
    return {
        matrix.m[0] * direction.x + matrix.m[4] * direction.y
            + matrix.m[8] * direction.z,
        matrix.m[1] * direction.x + matrix.m[5] * direction.y
            + matrix.m[9] * direction.z,
        matrix.m[2] * direction.x + matrix.m[6] * direction.y
            + matrix.m[10] * direction.z
    };
}

heritage::math::Vec3 transformPoint3(
    const heritage::math::Mat4& matrix,
    const heritage::math::Vec3& point)
{
    return {
        matrix.m[0] * point.x + matrix.m[4] * point.y
            + matrix.m[8] * point.z + matrix.m[12],
        matrix.m[1] * point.x + matrix.m[5] * point.y
            + matrix.m[9] * point.z + matrix.m[13],
        matrix.m[2] * point.x + matrix.m[6] * point.y
            + matrix.m[10] * point.z + matrix.m[14]
    };
}

heritage::math::Mat4 geometricWheelSpinDelta(
    const heritage::graphics::Mesh& mesh,
    const std::vector<heritage::math::Mat4>& baseGlobals,
    int pivotNodeIndex,
    int tireNodeIndex,
    const heritage::math::Mat4& currentPivotGlobal,
    float spinDegrees)
{
    const auto& tireNode =
        mesh.nodes[static_cast<std::size_t>(tireNodeIndex)];
    const auto& tireBindGlobal =
        baseGlobals[static_cast<std::size_t>(tireNodeIndex)];
    const auto& pivotBindGlobal =
        baseGlobals[static_cast<std::size_t>(pivotNodeIndex)];

    const heritage::math::Vec3 tireCenterLocal{
        tireNode.tireVisualCenter[0],
        tireNode.tireVisualCenter[1],
        tireNode.tireVisualCenter[2] };
    heritage::math::Vec3 axleLocal{};
    if (tireNode.tireVisualAxleAxis == 1)
        axleLocal = { 0.0f, 1.0f, 0.0f };
    else if (tireNode.tireVisualAxleAxis == 2)
        axleLocal = { 0.0f, 0.0f, 1.0f };
    else
        axleLocal = { 1.0f, 0.0f, 0.0f };

    // VA02K: derive the spin axle from the tire's measured geometry rather than
    // assuming the authored Pivot local X basis is perfectly coaxial. A round
    // tire can hide a few tenths of a degree of Pivot-axis error while the rim
    // spokes visibly precess/wobble. The runtime delta maps the tire's bind-space
    // centre/axle through suspension/upright motion, then one rigid axis-angle
    // rotation is applied in mesh-global space. Pivot scale, mirror and tiny
    // authored basis errors therefore cannot turn wheel spin into precession.
    const heritage::math::Mat4 runtimeFromBind = multiply(
        currentPivotGlobal, inverseMatrix(pivotBindGlobal));
    // VA02L: the semantic Pivot origin is the mechanical hub/rotation centre.
    // VA02K used the tire AABB centre for both centre and axle derivation. That
    // made the rotationally-symmetric tire look perfect by construction, but a
    // mirrored tire copy whose AABB centre is even slightly offset from the
    // authored hub centre makes the non-symmetric rim orbit around that tire
    // centre and appear as a bent-wheel wobble. Keep geometry as the AXLE
    // direction authority, but restore the authored Pivot origin as the centre
    // of the spin line.
    const heritage::math::Vec3 bindPivotCenter =
        transformPoint3(pivotBindGlobal, { 0.0f, 0.0f, 0.0f });
    const heritage::math::Vec3 inferredTireCenter =
        transformPoint3(tireBindGlobal, tireCenterLocal);
    const heritage::math::Vec3 runtimeCenter =
        transformPoint3(currentPivotGlobal, { 0.0f, 0.0f, 0.0f });
    heritage::math::Vec3 bindAxle =
        transformDirection(tireBindGlobal, axleLocal);
    // Keep the old Pivot-local +X angular sign even when the tire mesh itself
    // is mirrored. Geometry chooses the coaxial LINE; Pivot +X chooses which
    // direction along that line is positive for the existing telemetry angle.
    const heritage::math::Vec3 pivotPositiveAxle =
        transformDirection(pivotBindGlobal, { 1.0f, 0.0f, 0.0f });
    if (bindAxle.x * pivotPositiveAxle.x
        + bindAxle.y * pivotPositiveAxle.y
        + bindAxle.z * pivotPositiveAxle.z < 0.0f)
    {
        bindAxle.x = -bindAxle.x;
        bindAxle.y = -bindAxle.y;
        bindAxle.z = -bindAxle.z;
    }
    // Log the exact discrepancy once per semantic pivot. This is intentionally
    // diagnostic-only: axial offset along the spin line is harmless, while the
    // radial component is the amount that VA02K would have converted directly
    // into visible rim orbit/precession.
    {
        const float axleLengthSquared =
            bindAxle.x * bindAxle.x + bindAxle.y * bindAxle.y
            + bindAxle.z * bindAxle.z;
        if (axleLengthSquared > 1.0e-16f)
        {
            const float inverseAxleLength = 1.0f / std::sqrt(axleLengthSquared);
            const heritage::math::Vec3 unitAxle{
                bindAxle.x * inverseAxleLength,
                bindAxle.y * inverseAxleLength,
                bindAxle.z * inverseAxleLength };
            const heritage::math::Vec3 centerDelta{
                inferredTireCenter.x - bindPivotCenter.x,
                inferredTireCenter.y - bindPivotCenter.y,
                inferredTireCenter.z - bindPivotCenter.z };
            const float axialOffset =
                centerDelta.x * unitAxle.x + centerDelta.y * unitAxle.y
                + centerDelta.z * unitAxle.z;
            const heritage::math::Vec3 radialDelta{
                centerDelta.x - unitAxle.x * axialOffset,
                centerDelta.y - unitAxle.y * axialOffset,
                centerDelta.z - unitAxle.z * axialOffset };
            const float radialOffset = std::sqrt(
                radialDelta.x * radialDelta.x + radialDelta.y * radialDelta.y
                + radialDelta.z * radialDelta.z);
            static std::unordered_map<std::string, bool> loggedPivotOffsets;
            const std::string& pivotName =
                mesh.nodes[static_cast<std::size_t>(pivotNodeIndex)].name;
            if (!loggedPivotOffsets[pivotName])
            {
                loggedPivotOffsets[pivotName] = true;
                std::cout
                    << "VA02L wheel spin line: " << pivotName
                    << " tire_vs_pivot_radial_mm=" << radialOffset * 1000.0f
                    << " axial_mm=" << axialOffset * 1000.0f
                    << '\n';
            }
        }
    }

    const heritage::math::Vec3 runtimeAxle =
        transformDirection(runtimeFromBind, bindAxle);

    return multiply(
        multiply(
            translation(runtimeCenter),
            axisAngleRotation(runtimeAxle, spinDegrees)),
        translation({
            -runtimeCenter.x, -runtimeCenter.y, -runtimeCenter.z }));
}

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
        const int wheelTireNodeIndex = found != overrides.end()
            && found->second->hasLocalRotationOffset
            ? semanticWheelTireNodeIndex(mesh, nodeIndex) : -1;
        const bool useGeometricWheelSpin = wheelTireNodeIndex >= 0
            && std::abs(found->second->localRotationOffsetDegrees.y) <= 1.0e-5f
            && std::abs(found->second->localRotationOffsetDegrees.z) <= 1.0e-5f;
        bool localRotationOffsetConsumed = false;
        if (found != overrides.end()
            && found->second->hasLocalRotationOffset
            && !useGeometricWheelSpin
            && !found->second->hasAnchoredWorldDelta
            && !found->second->hasAnchoredWorldPose
            && !found->second->hasWorldPose)
        {
            local = scaleSafeLocalRotationOffset(
                baseLocal, found->second->localRotationOffsetDegrees);
            localRotationOffsetConsumed = true;
        }

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

        if (found != overrides.end() && found->second->hasLocalRotationOffset
            && !localRotationOffsetConsumed)
        {
            if (useGeometricWheelSpin)
            {
                // VA02L: WH_*_Pivot wheel spin is a rigid mesh-global rotation
                // around the authored Pivot ORIGIN and measured WH_*_Tire axle.
                // VA02K also used the tire AABB centre, which can make the tire
                // look stable while an offset rim visibly orbits that inferred
                // centre. The Pivot owns hub centre; tire geometry owns axis.
                current = multiply(
                    geometricWheelSpinDelta(
                        mesh, baseGlobals, nodeIndex, wheelTireNodeIndex,
                        current, found->second->localRotationOffsetDegrees.x),
                    current);
            }
            else
            {
                // Generic local offsets keep their historical node-local
                // semantics. Only semantic embedded wheel pivots take the
                // geometry-derived rigid spin path above.
                current = multiply(
                    current,
                    eulerRotationDegrees(found->second->localRotationOffsetDegrees));
            }
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
