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

        // GLB semantic wheel names remain authoritative for presentation. The
        // native wheel vector may be reordered without swapping FL/FR/RL/RR.
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
                std::vector<heritage::vehicles::WheelDescription> descriptions(
                    wheelCount);
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
                }
            }
        }

        heritage::vehicles::WheelState wheelState;
        heritage::vehicles::WheelDescription wheelDescription;
        heritage::vehicles::tires::TireFlexibleRingFieldOutput carcassField;
        if (runtime->m_physics->vehicles().wheelState(
                vehicleHandle, wheelIndex, wheelState)
            && runtime->m_physics->vehicles().wheelDescription(
                vehicleHandle, wheelIndex, wheelDescription)
            && runtime->m_physics->vehicles().wheelTireFlexibleRingField(
                vehicleHandle, wheelIndex, carcassField))
        {
            std::array<float, heritage::entities::TireVisualDeformationFieldCount>
                forwardM{};
            std::array<float, heritage::entities::TireVisualDeformationFieldCount>
                downM{};
            std::array<float, heritage::entities::TireVisualDeformationFieldCount>
                lateralM{};
            static_assert(
                heritage::entities::TireVisualDeformationFieldCount
                    == heritage::vehicles::tires::TireFlexibleRingFieldCount);
            float maximumForwardM = 0.0f;
            float maximumDownM = 0.0f;
            float maximumLateralM = 0.0f;
            for (std::size_t index = 0; index < forwardM.size(); ++index)
            {
                forwardM[index] = static_cast<float>(
                    carcassField.forwardDisplacementM[index]);
                downM[index] = static_cast<float>(
                    carcassField.downDisplacementM[index]);
                lateralM[index] = static_cast<float>(
                    carcassField.lateralDisplacementM[index]);
                maximumForwardM = (std::max)(maximumForwardM, std::abs(forwardM[index]));
                maximumDownM = (std::max)(maximumDownM, std::abs(downM[index]));
                maximumLateralM = (std::max)(maximumLateralM, std::abs(lateralM[index]));
            }

            // TIRE45C live fault trace.  This is deliberately emitted from the
            // final native->renderer bridge so one log line tells us whether a
            // grotesque visible tire already exists in the physical 24x13 field
            // or is introduced only while mapping that sane field onto the GLB.
            // Rate-limit per wheel: normal driving must not spam the console.
            static std::array<std::uint64_t, 64> traceCounters{};
            const std::size_t traceSlot = wheelIndex % traceCounters.size();
            const std::uint64_t traceCounter = ++traceCounters[traceSlot];
            const float maximumAnyM = (std::max)(maximumForwardM,
                (std::max)(maximumDownM, maximumLateralM));
            const bool suspiciousField = maximumAnyM >= 0.040f
                || maximumLateralM >= 0.020f;
            const bool suspiciousTrace = suspiciousField
                && (traceCounter % 30u) == 0u;
            const bool rollingTrace = std::abs(
                static_cast<float>(wheelState.wheelAngularVelocity)) >= 1.0f
                && (traceCounter % 120u) == 0u;
            if (suspiciousTrace || rollingTrace)
            {
                const auto dot3 = [](const heritage::math::Vec3& a,
                    const heritage::math::Vec3& b) {
                    return a.x * b.x + a.y * b.y + a.z * b.z;
                };
                std::cout
                    << "TIRE45C TRACE node=" << nodeName
                    << " wheel=" << wheelIndex
                    << " fieldValid=" << (carcassField.valid ? 1 : 0)
                    << " max_mm(F,D,L)="
                    << maximumForwardM * 1000.0f << ','
                    << maximumDownM * 1000.0f << ','
                    << maximumLateralM * 1000.0f
                    << " omega=" << wheelState.wheelAngularVelocity
                    << " speed=" << wheelState.longitudinalSpeed
                    << " slip=" << wheelState.slipRatio
                    << " loadN=" << wheelState.normalForce
                    << " camber=" << wheelState.camberAngleDegrees
                    << " toe=" << wheelState.toeAngleDegrees
                    << " ring_mm(Lon,Lat)="
                    << wheelState.tireRingLongitudinalOffset * 1000.0 << ','
                    << wheelState.tireRingLateralOffset * 1000.0
                    << " yaw=" << wheelState.tireRingYawDegrees
                    << " windup=" << wheelState.tireRingWindupDegrees
                    << " twist=" << wheelState.contactPatchTwistDegrees
                    << " basisDot(FR,FU,RU)="
                    << dot3(wheelState.worldWheelForward, wheelState.worldWheelRight) << ','
                    << dot3(wheelState.worldWheelForward, wheelState.worldWheelUp) << ','
                    << dot3(wheelState.worldWheelRight, wheelState.worldWheelUp)
                    << '\n';
            }

            const float tireReferenceRadiusM = std::clamp(
                static_cast<float>(wheelState.tireFreeRollingRadius > 0.05
                    ? wheelState.tireFreeRollingRadius
                    : wheelDescription.radius),
                0.05f,
                2.5f);
            result = runtime->m_entities->setMeshNodeTireDeformationField(
                entityHandle,
                nodeName,
                carcassField.valid,
                tireReferenceRadiusM,
                forwardM,
                downM,
                lateralM,
                wheelState.tireFailureStage
                    == heritage::vehicles::tires::TireFailureStage::BareRimRunning,
                static_cast<std::uint8_t>(wheelState.tireFailureStage),
                static_cast<float>(wheelState.tireTreadAttachment),
                static_cast<float>(wheelState.tireStructuralIntegrity),
                static_cast<float>(wheelIndex * 17u
                    + (wheelState.tireFailureEventSerial % 997u)),
                static_cast<float>(wheelState.tireFailureEventElapsedSeconds),
                static_cast<float>(wheelState.wheelAngularVelocity),
                static_cast<float>(wheelState.wheelRotationDegrees
                    * 0.01745329251994329577),
                wheelState.worldWheelForward,
                wheelState.worldWheelRight,
                wheelState.worldWheelUp);
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
