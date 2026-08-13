#include "../../LuaModuleRuntime.hpp"
#include "../../../../Physics/PhysicsWorld.hpp"
#include <array>
#include "LuaEntityBindingHandlers.hpp"
#include "LuaEntityTireFlexibleRingBridge.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../Entities/EntityRegistry.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaEntityBindingHandlers::luaEntitySetMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 color{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.72)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.78)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.88))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMesh(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            color,
            LuaModuleRuntime::booleanArgument(*runtime, state, 6, false),
            LuaModuleRuntime::booleanArgument(*runtime, state, 7, false),
            LuaModuleRuntime::booleanArgument(*runtime, state, 8, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityRemoveMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->removeMesh(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityHasMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->hasMesh(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshVisible(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshVisible(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodePrefixFilter(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string prefix = LuaModuleRuntime::stringArgument(*runtime, state, 2, "");
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodePrefixFilter(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            prefix);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshColor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 color{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.72)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.78)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.88))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshColor(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNormalize(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNormalize(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshDoubleSided(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshDoubleSided(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityPlayMeshAnimation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->playMeshAnimation(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            LuaModuleRuntime::booleanArgument(*runtime, state, 3, true),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.15)),
            LuaModuleRuntime::booleanArgument(*runtime, state, 5, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshAnimationPlaying(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshAnimationPlaying(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshAnimationSpeed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshAnimationSpeed(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySeekMeshAnimation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->seekMeshAnimation(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetMeshAnimation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::entities::MeshComponent component;
    if (!runtime->m_entities
        || !runtime->m_entities->mesh(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), component))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushlstring(
        state, component.animationClip.c_str(), component.animationClip.size());
    runtime->m_api.lua_pushboolean(state, component.animationPlaying ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.animationLoop ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, component.animationSpeed);
    runtime->m_api.lua_pushnumber(state, component.animationCrossFadeSeconds);
    runtime->m_api.lua_pushnumber(state, component.animationSeekSeconds);
    return 6;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodeWorldPose(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 position{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)) };
    const heritage::math::Vec3 rotation{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0)) };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodeWorldPose(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            position,
            rotation);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodeLocalRotationOffset(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 rotation{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)) };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodeLocalRotationOffset(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            rotation);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodeAnchoredWorldPose(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 position{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)) };
    const heritage::math::Vec3 rotation{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.0)) };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodeAnchoredWorldPose(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            LuaModuleRuntime::stringArgument(*runtime, state, 3),
            position,
            rotation);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodeAnchoredWorldDelta(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 translationDelta{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)) };
    const heritage::math::Vec3 rotationDelta{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.0)) };

    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodeAnchoredWorldDelta(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            LuaModuleRuntime::stringArgument(*runtime, state, 3),
            translationDelta,
            rotationDelta);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}


int LuaEntityBindingHandlers::luaEntitySetMeshNodeTireFlexibleRingFromWheel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    bool result = false;
    if (runtime->m_entities && runtime->m_physics)
    {
        const auto entityHandle = LuaModuleRuntime::entityHandleArgument(*runtime, state, 1);
        const std::string nodeName = LuaModuleRuntime::stringArgument(*runtime, state, 2);
        const auto vehicleHandle = LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 3);
        const double requestedWheelIndex = LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0);
        const std::size_t requestedIndex = requestedWheelIndex >= 1.0
            ? static_cast<std::size_t>(requestedWheelIndex - 1.0)
            : std::size_t{0};

        // TIRE28C/VIS21: the GLB semantic node name is authoritative for WHICH
        // physical wheel owns this visual deformation field.  TIRE27 live testing
        // proved that a left-side sidewalk contact could appear on the right-side
        // visible tire.  Do not "fix" creator assets to match a transient native
        // wheel-vector order.  Resolve WH_FL/WH_FR/WH_RL/WH_RR against the actual
        // native WheelDescription mounts every call, then use that resolved wheel
        // for BOTH the probe centre and the wheel basis/state.
        //
        // This also makes the bridge robust to future loaders that reorder contact
        // units internally: presentation semantics stay FL/FR/RL/RR while native
        // vector order is allowed to change.
        auto semanticCorner = [](const std::string& name, bool& left, bool& front)
        {
            const auto has = [&](const char* token) {
                return name.find(token) != std::string::npos;
            };
            if (has("WH_FL")) { left = true;  front = true;  return true; }
            if (has("WH_FR")) { left = false; front = true;  return true; }
            if (has("WH_RL")) { left = true;  front = false; return true; }
            if (has("WH_RR")) { left = false; front = false; return true; }
            return false;
        };

        std::size_t wheelIndex = requestedIndex;
        bool semanticLeft = false;
        bool semanticFront = false;
        if (semanticCorner(nodeName, semanticLeft, semanticFront))
        {
            const std::size_t wheelCount =
                runtime->m_physics->vehicles().wheelCount(vehicleHandle);
            if (wheelCount > 0)
            {
                std::vector<heritage::vehicles::WheelDescription> descriptions;
                descriptions.resize(wheelCount);
                bool complete = true;
                float minX = std::numeric_limits<float>::infinity();
                float maxX = -std::numeric_limits<float>::infinity();
                float minZ = std::numeric_limits<float>::infinity();
                float maxZ = -std::numeric_limits<float>::infinity();
                for (std::size_t i = 0; i < wheelCount; ++i)
                {
                    if (!runtime->m_physics->vehicles().wheelDescription(
                            vehicleHandle, i, descriptions[i]))
                    {
                        complete = false;
                        break;
                    }
                    minX = (std::min)(minX, descriptions[i].localMount.x);
                    maxX = (std::max)(maxX, descriptions[i].localMount.x);
                    minZ = (std::min)(minZ, descriptions[i].localMount.z);
                    maxZ = (std::max)(maxZ, descriptions[i].localMount.z);
                }

                if (complete)
                {
                    // Heritage vehicle-local convention is X=lateral and Z=
                    // longitudinal.  Use the extrema rather than hard-coded wheel
                    // numbers, so a four-wheel definition can arrive in any order.
                    // Racing United authored vehicle semantics follow the GLB itself:
                    // with the vehicle nose authored toward Blender -Y, the
                    // driver's LEFT side is +X in the exported/Heritage vehicle
                    // geometry.  The current native prototype vector still carries
                    // the older opposite side labels, which is exactly the TIRE27
                    // left-contact/right-visual bug.
                    const float targetX = semanticLeft ? maxX : minX;
                    const float targetZ = semanticFront ? maxZ : minZ;
                    float bestScore = std::numeric_limits<float>::infinity();
                    std::size_t bestIndex = requestedIndex < wheelCount
                        ? requestedIndex : std::size_t{0};
                    for (std::size_t i = 0; i < wheelCount; ++i)
                    {
                        const float dx = descriptions[i].localMount.x - targetX;
                        const float dz = descriptions[i].localMount.z - targetZ;
                        const float score = dx * dx + dz * dz;
                        if (score < bestScore)
                        {
                            bestScore = score;
                            bestIndex = i;
                        }
                    }
                    wheelIndex = bestIndex;

                    static std::unordered_set<std::string> reportedSemanticRouting;
                    if (reportedSemanticRouting.insert(nodeName).second)
                    {
                        const auto& resolved = descriptions[wheelIndex];
                        std::cout
                            << "TIRE28C VIS21 semantic routing node=" << nodeName
                            << " requested=" << (requestedIndex + 1)
                            << " resolved=" << (wheelIndex + 1)
                            << " mount=(" << resolved.localMount.x << ','
                            << resolved.localMount.y << ','
                            << resolved.localMount.z << ")\n";
                    }
                }
            }
        }

        heritage::vehicles::WheelState wheelState;
        heritage::vehicles::WheelDescription wheelDescription;
        if (runtime->m_physics->vehicles().wheelState(vehicleHandle, wheelIndex, wheelState)
            && runtime->m_physics->vehicles().wheelDescription(
                vehicleHandle, wheelIndex, wheelDescription))
        {
            // TIRE27/VIS20: keep the proven render-space probe bridge, but replace the
            // coarse uniform 9x7 topology with a dense bottom-biased 21x13
            // lattice. The live curb test proved contact/deformation reaches
            // the visible tire; each old sample simply controlled too much rubber.
            // The live tire shader was proven in TIRE23.  What failed afterward
            // was the contact bridge. Build the requested lower-half contact lattice
            // with the CollisionSystem itself, then send only
            // scalar physical compression to presentation.  No absolute contact
            // position survives this boundary, so stale WheelState/world/model
            // coordinate disagreements cannot make the visual contact disappear.
            std::array<float, heritage::entities::TireVisualContactSampleCount>
                compressionM{};

            heritage::math::Vec3 chassisWorldPosition{};
            heritage::math::Vec3 chassisWorldRotationDegrees{};
            const bool haveRenderChassisPose =
                runtime->m_entities->worldPosition(entityHandle, chassisWorldPosition)
                && runtime->m_entities->worldRotationDegrees(
                    entityHandle, chassisWorldRotationDegrees);

            heritage::math::Vec3 probeCenter = wheelState.worldCenter;
            if (haveRenderChassisPose)
            {
                // EmbeddedWheelBinding renders from the authored bind pose plus
                // suspension travel relative to the *current/interpolated* chassis,
                // deliberately not from WheelState.worldCenter. Reconstruct that
                // same wheel-centre policy here so the probe lattice follows the
                // tire the player actually sees.
                const auto chassisRotation = heritage::math::normalized(
                    heritage::math::makeQuaternionFromEulerDegrees(
                        chassisWorldRotationDegrees));
                const float suspensionLength = static_cast<float>(
                    std::isfinite(wheelState.suspensionLength)
                        ? wheelState.suspensionLength
                        : wheelDescription.restLength);
                const heritage::math::Vec3 localWheelCenter{
                    wheelDescription.localMount.x
                        + wheelDescription.localSuspensionDirection.x * suspensionLength,
                    wheelDescription.localMount.y
                        + wheelDescription.localSuspensionDirection.y * suspensionLength,
                    wheelDescription.localMount.z
                        + wheelDescription.localSuspensionDirection.z * suspensionLength };
                const heritage::math::Vec3 rotatedLocalCenter =
                    heritage::math::rotateVectorUnit(chassisRotation, localWheelCenter);
                probeCenter = {
                    chassisWorldPosition.x + rotatedLocalCenter.x,
                    chassisWorldPosition.y + rotatedLocalCenter.y,
                    chassisWorldPosition.z + rotatedLocalCenter.z };
            }

            auto dot3 = [](const heritage::math::Vec3& a, const heritage::math::Vec3& b)
            {
                return a.x * b.x + a.y * b.y + a.z * b.z;
            };
            auto length3 = [&](const heritage::math::Vec3& v)
            {
                return std::sqrt((std::max)(dot3(v, v), 0.0f));
            };
            auto scale3 = [](const heritage::math::Vec3& v, float s)
            {
                return heritage::math::Vec3{ v.x * s, v.y * s, v.z * s };
            };
            auto add3 = [](const heritage::math::Vec3& a, const heritage::math::Vec3& b)
            {
                return heritage::math::Vec3{ a.x + b.x, a.y + b.y, a.z + b.z };
            };
            auto normalize3 = [&](heritage::math::Vec3 v, const heritage::math::Vec3& fallback)
            {
                const float len = length3(v);
                if (len <= 1.0e-6f)
                    return fallback;
                return scale3(v, 1.0f / len);
            };
            auto cross3 = [](const heritage::math::Vec3& a, const heritage::math::Vec3& b)
            {
                return heritage::math::Vec3{
                    a.y * b.z - a.z * b.y,
                    a.z * b.x - a.x * b.z,
                    a.x * b.y - a.y * b.x };
            };

            heritage::math::Vec3 wheelRight = normalize3(
                wheelState.worldWheelRight, { 1.0f, 0.0f, 0.0f });
            heritage::math::Vec3 wheelForward = wheelState.worldWheelForward;
            wheelForward = add3(
                wheelForward,
                scale3(wheelRight, -dot3(wheelForward, wheelRight)));
            wheelForward = normalize3(wheelForward, { 0.0f, 0.0f, 1.0f });
            heritage::math::Vec3 wheelUp = normalize3(
                cross3(wheelRight, wheelForward),
                wheelState.worldWheelUp);
            // Preserve the WheelState's up sign. Mirrored wheels may otherwise
            // flip the lower hemisphere and probe the roof side of the tire.
            if (dot3(wheelUp, wheelState.worldWheelUp) < 0.0f)
                wheelUp = scale3(wheelUp, -1.0f);

            // The ordinary support plane is already represented by native tire
            // deflection/contact-patch state. Side-facing casts against that same
            // plane are rejected so the contact sampler cannot manufacture a
            // second local indentation on top of the flexible-ring equilibrium.
            const heritage::math::Vec3 primarySupportNormal = normalize3(
                wheelState.contactNormal, wheelUp);
            const heritage::math::Vec3 primarySupportPoint = wheelState.contactPoint;
            const bool primarySupportPlaneValid = wheelState.grounded
                && std::isfinite(primarySupportPoint.x)
                && std::isfinite(primarySupportPoint.y)
                && std::isfinite(primarySupportPoint.z)
                && dot3(primarySupportNormal, primarySupportNormal) > 0.99f;

            const float tireRadiusM = std::clamp(
                static_cast<float>(wheelState.tireFreeRollingRadius > 0.05
                    ? wheelState.tireFreeRollingRadius
                    : wheelDescription.radius),
                0.05f,
                2.5f);
            const float tireHalfWidthM = std::clamp(
                wheelDescription.fitment.tireWidthMm > 20.0f
                    ? wheelDescription.fitment.tireWidthMm * 0.0005f
                    : static_cast<float>(wheelState.tireContactPatchWidth > 0.02
                        ? wheelState.tireContactPatchWidth / 1.5
                        : 0.105),
                0.025f,
                0.55f);
            const float maximumCompressionM = std::clamp(
                wheelDescription.maximumTireDeflection > 0.005f
                    ? wheelDescription.maximumTireDeflection
                    : 0.08f,
                0.015f,
                0.12f);

            // TIRE29/VIS22: deformation capacity is directional.  TIRE28C live
            // testing proved the routing fix, but a rear-left curb contact could
            // collapse a sidewall strip through the opposite half of the tire and
            // produce a long tongue/fin.  One scalar maximum is not geometrically
            // valid in every direction: an 80 mm radial allowance is plausible for
            // this 17-inch tire, while 80 mm laterally consumes almost the complete
            // 102 mm half-width.  Bound each probe by the actual tire section before
            // presentation ever sees it.
            const float authoredRimDiameterIn =
                wheelDescription.fitment.tireRimDiameterIn > 1.0f
                    ? wheelDescription.fitment.tireRimDiameterIn
                    : wheelDescription.fitment.rimDiameterIn;
            const float rimRadiusM = authoredRimDiameterIn > 1.0f
                ? authoredRimDiameterIn * 0.0254f * 0.5f
                : tireRadiusM * 0.72f;
            const float sidewallHeightM = std::clamp(
                tireRadiusM - rimRadiusM, 0.020f, tireRadiusM * 0.45f);
            const float radialCompressionCapacityM = (std::min)(
                maximumCompressionM, sidewallHeightM * 0.90f);
            const float lateralCompressionCapacityM = (std::min)(
                maximumCompressionM, tireHalfWidthM * 0.55f);

            heritage::physics::CollisionQueryFilter filter;
            filter.layerMask = 0xffffffffu;
            filter.includeTriggers = false;
            filter.ignoredBody = runtime->m_physics->vehicles().chassisBody(vehicleHandle);

            constexpr float kPi = 3.14159265358979323846f;
            constexpr float kProbeSphereRadiusM = 0.006f;
            constexpr float kOutsideMarginM = 0.012f;
            const float probeDepthM = maximumCompressionM + 0.025f;
            const float expectedTouchDistanceM = (std::max)(
                probeDepthM - kProbeSphereRadiusM, 0.001f);

            for (std::size_t station = 0;
                 station < heritage::entities::TireVisualContactSampleStations;
                 ++station)
            {
                // TIRE27: non-uniform station spacing.  Indices 4..16 cover
                // 65..115 degrees, concentrating most of the longitudinal
                // resolution in the loaded region below the user's green-line
                // chord while retaining front/rear lower-half coverage.
                const float phi =
                    heritage::entities::TireVisualContactSamplePhiRadians[station];
                const heritage::math::Vec3 radialDirection = normalize3(
                    add3(
                        scale3(wheelForward, std::cos(phi)),
                        scale3(wheelUp, -std::sin(phi))),
                    scale3(wheelUp, -1.0f));

                for (std::size_t band = 0;
                     band < heritage::entities::TireVisualContactSampleBands;
                     ++band)
                {
                    const float widthCoordinate =
                        heritage::entities::TireVisualContactSampleWidthCoordinates[band];
                    const float absWidth = std::abs(widthCoordinate);

                    // Rounded tire cross-section: centre tread probes mainly
                    // radially, shoulders become diagonal, outer bands become
                    // sidewall-facing. The 13 width bands now give the loaded
                    // tread substantially finer lateral resolution while still
                    // seeing a road, kerb face or rock against the sidewall.
                    const float sideNormalWeight = std::clamp(
                        (absWidth - 0.42f) / 0.58f, 0.0f, 1.0f);
                    const float sideNormalBlend = sideNormalWeight
                        * sideNormalWeight * 0.86f;
                    const heritage::math::Vec3 sideDirection = scale3(
                        wheelRight, widthCoordinate >= 0.0f ? 1.0f : -1.0f);
                    const heritage::math::Vec3 surfaceNormal = normalize3(
                        add3(
                            scale3(radialDirection, 1.0f - sideNormalBlend),
                            scale3(sideDirection, sideNormalBlend)),
                        radialDirection);

                    const float radialScale = 1.0f
                        - 0.055f * absWidth * absWidth;
                    const float lateralOffsetM = tireHalfWidthM
                        * widthCoordinate * 0.94f;
                    const heritage::math::Vec3 nominalSurface = add3(
                        add3(
                            probeCenter,
                            scale3(radialDirection, tireRadiusM * radialScale)),
                        scale3(wheelRight, lateralOffsetM));
                    const heritage::math::Vec3 castOrigin = add3(
                        nominalSurface,
                        scale3(surfaceNormal, -probeDepthM));

                    heritage::physics::SphereCastHit hit;
                    const bool hitSomething = runtime->m_physics->collisions().sphereCast(
                        castOrigin,
                        kProbeSphereRadiusM,
                        surfaceNormal,
                        probeDepthM + kOutsideMarginM,
                        filter,
                        runtime->m_physics->rigidBodies(),
                        hit);

                    const float regionalCompressionCapacityM =
                        radialCompressionCapacityM * (1.0f - sideNormalBlend)
                        + lateralCompressionCapacityM * sideNormalBlend;
                    float compression = 0.0f;
                    if (hitSomething)
                    {
                        compression = std::clamp(
                            expectedTouchDistanceM - hit.distance,
                            0.0f,
                            regionalCompressionCapacityM);
                    }

                    const std::size_t index = station
                        * heritage::entities::TireVisualContactSampleBands + band;
                    bool duplicatePrimarySupport = false;
                    if (hitSomething && primarySupportPlaneValid)
                    {
                        const heritage::math::Vec3 hitNormal = normalize3(
                            hit.normal, primarySupportNormal);
                        const float supportNormalAlignment = dot3(
                            hitNormal, primarySupportNormal);
                        const heritage::math::Vec3 pointFromSupport = add3(
                            hit.point, scale3(primarySupportPoint, -1.0f));
                        const float supportPlaneOffsetM = std::abs(dot3(
                            pointFromSupport, primarySupportNormal));
                        duplicatePrimarySupport =
                            supportNormalAlignment >= 0.965f
                            && supportPlaneOffsetM <= 0.006f;
                    }
                    if (duplicatePrimarySupport && sideNormalBlend > 0.08f)
                        compression = 0.0f;
                    compressionM[index] = compression;
                }
            }

            // One contact sampler, one flexible-ring solve, one final renderer
            // field.  No broad plane, curb dent, sidewall bulge or probe pass is
            // permitted to move a vertex after this solve.
            heritage::vehicles::TireModelDescription tireModel;
            const bool haveTireModel = runtime->m_physics->vehicles().wheelTireModel(
                vehicleHandle, wheelIndex, tireModel);
            const auto flexibleRingField = solveTireFlexibleRingPresentationField(
                wheelState, haveTireModel ? &tireModel : nullptr,
                tireRadiusM, rimRadiusM, tireHalfWidthM,
                maximumCompressionM, compressionM);
            result = runtime->m_entities->setMeshNodeTireDeformationField(
                entityHandle,
                nodeName,
                flexibleRingField.valid,
                tireRadiusM,
                flexibleRingField.forwardDisplacementM,
                flexibleRingField.downDisplacementM,
                flexibleRingField.lateralDisplacementM,
                wheelForward,
                wheelRight,
                wheelUp);

        }
    }
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityClearMeshNodeOverrides(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->clearMeshNodeOverrides(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::entities::MeshComponent component;
    if (!runtime->m_entities
        || !runtime->m_entities->mesh(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), component))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushlstring(
        state, component.assetPath.c_str(), component.assetPath.size());
    runtime->m_api.lua_pushnumber(state, component.color.x);
    runtime->m_api.lua_pushnumber(state, component.color.y);
    runtime->m_api.lua_pushnumber(state, component.color.z);
    runtime->m_api.lua_pushboolean(state, component.visible ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.normalize ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.doubleSided ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.blenderCoordinates ? 1 : 0);
    return 8;
}

int LuaEntityBindingHandlers::luaEntityGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string error = runtime->m_entities
        ? runtime->m_entities->lastError()
        : std::string("EntityRegistry is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

} // namespace heritage::modules
