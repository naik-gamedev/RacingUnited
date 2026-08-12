#include "../../LuaModuleRuntime.hpp"
#include "../../../../Physics/PhysicsWorld.hpp"
#include <array>
#include "LuaEntityBindingHandlers.hpp"
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

int LuaEntityBindingHandlers::luaEntitySetMeshNodeTireDeformation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNodeTireDeformation(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            LuaModuleRuntime::booleanArgument(*runtime, state, 3, false),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.30)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 10, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 11, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 12, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 13, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 14, 0.0)),
            heritage::math::Vec3{
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 15, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 16, 1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 17, 0.0)) },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 18, 0.30)),
            LuaModuleRuntime::booleanArgument(*runtime, state, 19, false),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 20, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 21, 0.0)),
            std::array<float, 9>{
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 22, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 23, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 24, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 25, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 26, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 27, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 28, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 29, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 30, 0.0)) },
            heritage::math::Vec3{
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 31, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 32, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 33, 1.0)) },
            heritage::math::Vec3{
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 34, 1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 35, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 36, 0.0)) },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 37, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 38, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 39, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 40, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetMeshNodeTireColliderTrianglesFromWheel(lua_State* state)
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
            std::array<float, heritage::entities::TireVisualProbeCount>
                compressionM{};
            std::array<float, heritage::entities::TireVisualProbeCount>
                compressionCapacityM{};

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

            std::size_t activeProbeCount = 0;
            float maximumProbeCompression = 0.0f;
            for (std::size_t station = 0;
                 station < heritage::entities::TireVisualProbeCircumferenceStations;
                 ++station)
            {
                // TIRE27: non-uniform station spacing.  Indices 4..16 cover
                // 65..115 degrees, concentrating most of the longitudinal
                // resolution in the loaded region below the user's green-line
                // chord while retaining front/rear lower-half coverage.
                const float phi =
                    heritage::entities::TireVisualProbeStationPhiRadians[station];
                const heritage::math::Vec3 radialDirection = normalize3(
                    add3(
                        scale3(wheelForward, std::cos(phi)),
                        scale3(wheelUp, -std::sin(phi))),
                    scale3(wheelUp, -1.0f));

                for (std::size_t band = 0;
                     band < heritage::entities::TireVisualProbeWidthBands;
                     ++band)
                {
                    const float widthCoordinate =
                        heritage::entities::TireVisualProbeWidthCoordinates[band];
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
                        * heritage::entities::TireVisualProbeWidthBands + band;
                    compressionM[index] = compression;
                    compressionCapacityM[index] = regionalCompressionCapacityM;
                    if (compression > 0.0005f)
                    {
                        ++activeProbeCount;
                        maximumProbeCompression = (std::max)(
                            maximumProbeCompression, compression);
                    }
                }
            }

            // TIRE35/VIS28 separates the low-frequency pneumatic equilibrium
            // shape from high-frequency road detail. Previously the baseline
            // footprint and collider penetration were blurred together, then the
            // vertex shader applied the resulting scalar only to nearby vertices.
            // That is why a loaded tire could look like three or four vertices had
            // been pinched upward while the rest of the lower carcass stayed round.
            //
            // The equilibrium field below is derived from the authoritative native
            // deflection and finite contact-patch length. It is sent in the same
            // total-compression grid for compatibility, but relaxation operates
            // only on the collider residual above this field. The shader consumes
            // the equilibrium as one broad carcass mode and applies only the
            // residual locally for kerbs, rocks and broken road.
            std::array<float, heritage::entities::TireVisualProbeCount>
                equilibriumCompressionM{};
            if (wheelState.grounded && wheelState.tireDeflection > 0.0001)
            {
                const float baseline = std::clamp(
                    static_cast<float>(wheelState.tireDeflection),
                    0.0f,
                    maximumCompressionM);
                const float patchLengthM = std::clamp(
                    static_cast<float>(wheelState.tireContactPatchLength > 0.01
                        ? wheelState.tireContactPatchLength
                        : tireRadiusM * 0.34f),
                    0.025f,
                    tireRadiusM * 0.95f);
                const float patchHalfAngle = std::clamp(
                    0.62f * patchLengthM / (std::max)(tireRadiusM, 0.05f),
                    0.075f,
                    0.34f);

                for (std::size_t station = 0;
                     station < heritage::entities::TireVisualProbeCircumferenceStations;
                     ++station)
                {
                    const float phi =
                        heritage::entities::TireVisualProbeStationPhiRadians[station];
                    const float angleFromBottom = std::abs(phi - 0.5f * kPi);
                    if (angleFromBottom >= patchHalfAngle)
                        continue;
                    const float longitudinalT = angleFromBottom / patchHalfAngle;
                    const float longitudinalWeight =
                        1.0f - longitudinalT * longitudinalT
                            * (3.0f - 2.0f * longitudinalT);

                    for (std::size_t band = 0;
                         band < heritage::entities::TireVisualProbeWidthBands;
                         ++band)
                    {
                        const float widthCoordinate =
                            heritage::entities::TireVisualProbeWidthCoordinates[band];
                        const float absWidth = std::abs(widthCoordinate);
                        const float widthWeight = std::clamp(
                            (1.0f - absWidth) / 0.42f, 0.0f, 1.0f);
                        const float shoulderWeight = std::clamp(
                            (0.90f - absWidth) / 0.48f, 0.0f, 1.0f);
                        const float treadWeight = (std::max)(
                            widthWeight, 0.42f * shoulderWeight);
                        if (treadWeight <= 0.001f)
                            continue;

                        const float shapedBaseline = baseline
                            * longitudinalWeight * treadWeight;
                        const std::size_t index = station
                            * heritage::entities::TireVisualProbeWidthBands + band;
                        equilibriumCompressionM[index] = shapedBaseline;
                    }
                }
            }

            // Remove the analytically represented equilibrium from the direct
            // collision samples before carcass coupling. Adding the fields back at
            // the end preserves max(equilibrium, direct collision) exactly at every
            // probe while preventing normal flat-road load from becoming a second,
            // narrow deformation layered over the broad carcass mode.
            std::array<float, heritage::entities::TireVisualProbeCount>
                rawIrregularCompressionM{};
            for (std::size_t index = 0; index < rawIrregularCompressionM.size(); ++index)
            {
                rawIrregularCompressionM[index] = (std::max)(
                    compressionM[index] - equilibriumCompressionM[index], 0.0f);
            }

            // Retain the dense TIRE33 reduced-order belt/sidewall relaxation, but
            // solve only the irregular residual. Local road detail spreads smoothly
            // through its neighboring carcass; the ordinary loaded shape is handled
            // once, by the physics-driven equilibrium mode.
            const auto rawCompressionM = rawIrregularCompressionM;
            auto locallyCoupledCompressionM = rawIrregularCompressionM;
            for (std::size_t station = 0;
                 station < heritage::entities::TireVisualProbeCircumferenceStations;
                 ++station)
            {
                for (std::size_t band = 0;
                     band < heritage::entities::TireVisualProbeWidthBands;
                     ++band)
                {
                    const std::size_t index = station
                        * heritage::entities::TireVisualProbeWidthBands + band;
                    float coupled = rawCompressionM[index];

                    auto inherit = [&](int stationOffset, int bandOffset, float weight)
                    {
                        const int sampleStation = static_cast<int>(station) + stationOffset;
                        const int sampleBand = static_cast<int>(band) + bandOffset;
                        if (sampleStation < 0
                            || sampleStation >= static_cast<int>(
                                heritage::entities::TireVisualProbeCircumferenceStations)
                            || sampleBand < 0
                            || sampleBand >= static_cast<int>(
                                heritage::entities::TireVisualProbeWidthBands))
                        {
                            return;
                        }
                        const std::size_t sampleIndex = static_cast<std::size_t>(sampleStation)
                            * heritage::entities::TireVisualProbeWidthBands
                            + static_cast<std::size_t>(sampleBand);
                        coupled = (std::max)(
                            coupled, rawCompressionM[sampleIndex] * weight);
                    };

                    inherit(-1,  0, 0.72f);
                    inherit(+1,  0, 0.72f);
                    inherit(-2,  0, 0.38f);
                    inherit(+2,  0, 0.38f);
                    inherit( 0, -1, 0.64f);
                    inherit( 0, +1, 0.64f);
                    inherit( 0, -2, 0.34f);
                    inherit( 0, +2, 0.34f);
                    inherit(-1, -1, 0.44f);
                    inherit(-1, +1, 0.44f);
                    inherit(+1, -1, 0.44f);
                    inherit(+1, +1, 0.44f);

                    locallyCoupledCompressionM[index] = coupled;
                }
            }

            constexpr int kDenseBottomStationFirst = 4;
            constexpr int kDenseBottomStationLast = 16;
            constexpr int kDenseBottomFeatherFirst = 3;
            constexpr int kDenseBottomFeatherLast = 17;
            auto relaxedCompressionM = locallyCoupledCompressionM;
            std::array<float, heritage::entities::TireVisualProbeCount> relaxationScratch{};

            // Three iterations of a compact [1 4 6 4 1]-style separable kernel
            // are enough to involve essentially the whole 13x13 dense bottom domain
            // while remaining cheap for large grids.  Clamp at the green-line
            // boundary so upper/front/rear probe rows do not get dragged into normal
            // road-footprint shear.
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                relaxationScratch = relaxedCompressionM;
                for (int station = kDenseBottomFeatherFirst;
                     station <= kDenseBottomFeatherLast;
                     ++station)
                {
                    for (int band = 0;
                         band < static_cast<int>(heritage::entities::TireVisualProbeWidthBands);
                         ++band)
                    {
                        const auto sample = [&](int sampleStation, int sampleBand)
                        {
                            sampleStation = std::clamp(
                                sampleStation,
                                kDenseBottomFeatherFirst,
                                kDenseBottomFeatherLast);
                            sampleBand = std::clamp(
                                sampleBand,
                                0,
                                static_cast<int>(heritage::entities::TireVisualProbeWidthBands) - 1);
                            return relaxedCompressionM[
                                static_cast<std::size_t>(sampleStation)
                                    * heritage::entities::TireVisualProbeWidthBands
                                + static_cast<std::size_t>(sampleBand)];
                        };
                        const float circumferentiallyRelaxed =
                            (sample(station - 2, band)
                                + 4.0f * sample(station - 1, band)
                                + 6.0f * sample(station, band)
                                + 4.0f * sample(station + 1, band)
                                + sample(station + 2, band)) / 16.0f;
                        const std::size_t index = static_cast<std::size_t>(station)
                            * heritage::entities::TireVisualProbeWidthBands
                            + static_cast<std::size_t>(band);
                        relaxationScratch[index] = (std::max)(
                            rawCompressionM[index], circumferentiallyRelaxed);
                    }
                }

                relaxedCompressionM = relaxationScratch;
                for (int station = kDenseBottomFeatherFirst;
                     station <= kDenseBottomFeatherLast;
                     ++station)
                {
                    for (int band = 0;
                         band < static_cast<int>(heritage::entities::TireVisualProbeWidthBands);
                         ++band)
                    {
                        const auto sample = [&](int sampleBand)
                        {
                            sampleBand = std::clamp(
                                sampleBand,
                                0,
                                static_cast<int>(heritage::entities::TireVisualProbeWidthBands) - 1);
                            return relaxedCompressionM[
                                static_cast<std::size_t>(station)
                                    * heritage::entities::TireVisualProbeWidthBands
                                + static_cast<std::size_t>(sampleBand)];
                        };
                        const float laterallyRelaxed =
                            (sample(band - 2)
                                + 4.0f * sample(band - 1)
                                + 6.0f * sample(band)
                                + 4.0f * sample(band + 1)
                                + sample(band + 2)) / 16.0f;
                        const std::size_t index = static_cast<std::size_t>(station)
                            * heritage::entities::TireVisualProbeWidthBands
                            + static_cast<std::size_t>(band);
                        relaxationScratch[index] = (std::max)(
                            rawCompressionM[index], laterallyRelaxed);
                    }
                }
                relaxedCompressionM = relaxationScratch;
            }

            // Core rows below the green line use the fully relaxed field.  The two
            // neighbouring rows are blended at 50% so there is no visible hinge at
            // the dense-region boundary.  Outside that feather the original local
            // contact coupling remains authoritative for curb-face/rock contacts.
            auto coupledIrregularCompressionM = locallyCoupledCompressionM;
            for (int station = kDenseBottomFeatherFirst;
                 station <= kDenseBottomFeatherLast;
                 ++station)
            {
                const float regionWeight =
                    (station >= kDenseBottomStationFirst
                        && station <= kDenseBottomStationLast)
                    ? 1.0f : 0.50f;
                for (int band = 0;
                     band < static_cast<int>(heritage::entities::TireVisualProbeWidthBands);
                     ++band)
                {
                    const std::size_t index = static_cast<std::size_t>(station)
                        * heritage::entities::TireVisualProbeWidthBands
                        + static_cast<std::size_t>(band);
                    const float blended = locallyCoupledCompressionM[index]
                        + (relaxedCompressionM[index]
                            - locallyCoupledCompressionM[index]) * regionWeight;
                    coupledIrregularCompressionM[index] = (std::max)(
                        rawCompressionM[index], blended);
                }
            }

            for (std::size_t index = 0; index < compressionM.size(); ++index)
            {
                // equilibrium + max(collision - equilibrium, 0) is exactly
                // max(equilibrium, collision). Relaxation may raise neighboring
                // residuals, but never beyond this tire section's geometric room.
                compressionM[index] = std::clamp(
                    equilibriumCompressionM[index]
                        + coupledIrregularCompressionM[index],
                    0.0f,
                    compressionCapacityM[index]);
            }

            // Recompute diagnostics from the actual coupled field sent to the GPU.
            activeProbeCount = 0;
            maximumProbeCompression = 0.0f;
            for (float compression : compressionM)
            {
                if (compression > 0.0005f)
                    ++activeProbeCount;
                maximumProbeCompression = (std::max)(
                    maximumProbeCompression, compression);
            }

            const bool probeValid = maximumProbeCompression > 0.0001f;

            // TIRE30 diagnostic: report which physical half of the 13-band field
            // carries obstacle compression.  Positive bands are generated along
            // +wheelRight and must be consumed with this same basis by the shader.
            float negativeWidthCompression = 0.0f;
            float positiveWidthCompression = 0.0f;
            for (std::size_t station = 0;
                 station < heritage::entities::TireVisualProbeCircumferenceStations;
                 ++station)
            {
                for (std::size_t band = 0;
                     band < heritage::entities::TireVisualProbeWidthBands;
                     ++band)
                {
                    const float width =
                        heritage::entities::TireVisualProbeWidthCoordinates[band];
                    const float value = compressionM[station
                        * heritage::entities::TireVisualProbeWidthBands + band];
                    if (width < -0.001f) negativeWidthCompression += value;
                    if (width >  0.001f) positiveWidthCompression += value;
                }
            }
            static std::unordered_set<std::string> reportedProbeBasisNodes;
            if (reportedProbeBasisNodes.insert(nodeName).second)
            {
                std::cout
                    << "TIRE30 VIS23 probe basis node=" << nodeName
                    << " resolved=" << (wheelIndex + 1)
                    << " right=(" << wheelRight.x << ',' << wheelRight.y << ','
                    << wheelRight.z << ")"
                    << " widthSum[-]=" << negativeWidthCompression * 1000.0f
                    << " widthSum[+]=" << positiveWidthCompression * 1000.0f
                    << " mm\n";
            }

            static std::unordered_set<std::string> reportedLiveProbeNodes;
            static std::unordered_set<std::string> reportedDeepProbeNodes;
            if (probeValid && reportedLiveProbeNodes.insert(nodeName).second)
            {
                std::cout
                    << "TIRE27 VIS20 dense probe lattice LIVE node=" << nodeName
                    << " active=" << activeProbeCount
                    << " max_mm=" << (maximumProbeCompression * 1000.0f)
                    << " render_center=(" << probeCenter.x << ','
                    << probeCenter.y << ',' << probeCenter.z << ")\n";
            }
            if (maximumProbeCompression >= 0.020f
                && reportedDeepProbeNodes.insert(nodeName).second)
            {
                std::cout
                    << "TIRE27 VIS20 DEEP CONTACT node=" << nodeName
                    << " active=" << activeProbeCount
                    << " max_mm=" << (maximumProbeCompression * 1000.0f)
                    << "\n";
            }

            // TIRE30/VIS23: publish the compression field together with the exact
            // resolved wheel basis used to generate it.  This removes the last
            // split-brain path where SetMeshNodeTireDeformation() could leave a
            // different wheel's right-vector on the same visual node.
            const bool probeResult = runtime->m_entities->setMeshNodeTireProbeGrid(
                entityHandle,
                nodeName,
                probeValid,
                compressionM,
                wheelForward,
                wheelRight);

            // Disable the TIRE17/TIRE25 world-triangle presentation bridge.
            // TIRE27 intentionally has one visual irregular-contact authority:
            // the bottom-biased 21x13 CollisionSystem probe lattice above.
            std::array<
                heritage::entities::TireVisualColliderTriangle,
                heritage::entities::TireVisualColliderTriangleLimit> noTriangles{};
            const bool triangleResult = runtime->m_entities->setMeshNodeTireColliderTriangles(
                entityHandle, nodeName, false, 0, noTriangles);
            result = probeResult && triangleResult;
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
