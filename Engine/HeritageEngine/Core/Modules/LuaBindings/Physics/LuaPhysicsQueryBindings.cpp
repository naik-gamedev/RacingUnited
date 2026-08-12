#include "../../LuaModuleRuntime.hpp"
#include "LuaPhysicsBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaPhysicsBindingHandlers::luaPhysicsRaycast(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RaycastHit hit;
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = LuaModuleRuntime::numberArgument(
            *runtime, state, 8, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = LuaModuleRuntime::booleanArgument(*runtime, state, 9, false);
        filter.ignoredBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 10);

        result = runtime->m_physics->collisions().raycast(
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
            },
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, -1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies(),
            hit);
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.collider));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.body));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.distance));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.z));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.z));
    runtime->m_api.lua_pushboolean(state, hit.trigger ? 1 : 0);
    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(hit.surfaceMaterial));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(hit.surfaceMaterial));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(hit.surfaceWetness));
    return 14;
}

int LuaPhysicsBindingHandlers::luaPhysicsRaycastAny(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = LuaModuleRuntime::numberArgument(
            *runtime, state, 8, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = LuaModuleRuntime::booleanArgument(*runtime, state, 9, false);
        filter.ignoredBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 10);

        result = runtime->m_physics->collisions().raycastAny(
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
            },
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, -1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSphereCast(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SphereCastHit hit;
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = LuaModuleRuntime::numberArgument(
            *runtime, state, 9, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = LuaModuleRuntime::booleanArgument(*runtime, state, 10, false);
        filter.ignoredBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 11);

        result = runtime->m_physics->collisions().sphereCast(
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.5)),
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies(),
            hit);
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.collider));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.body));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.distance));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.z));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.z));
    runtime->m_api.lua_pushboolean(state, hit.trigger ? 1 : 0);
    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(hit.surfaceMaterial));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(hit.surfaceMaterial));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(hit.surfaceWetness));
    return 14;
}

int LuaPhysicsBindingHandlers::luaPhysicsSphereCastAny(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = LuaModuleRuntime::numberArgument(
            *runtime, state, 9, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = LuaModuleRuntime::booleanArgument(*runtime, state, 10, false);
        filter.ignoredBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 11);

        result = runtime->m_physics->collisions().sphereCastAny(
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.5)),
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsOverlapSphereCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    std::size_t result = 0;
    if (runtime->m_physics)
    {
        const double rawMask = LuaModuleRuntime::numberArgument(
            *runtime, state, 5, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = LuaModuleRuntime::booleanArgument(*runtime, state, 6, false);
        filter.ignoredBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 7);

        result = runtime->m_physics->collisions().overlapSphereCount(
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.5)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(result));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetLastQueryCandidateCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().lastQueryCandidateCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetLastQueryExactTestCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().lastQueryExactTestCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().contactCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().contactCountForBody(
                LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsIsBodyTouching(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().bodyTouching(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBroadphaseCandidateCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().broadphaseCandidateCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetNarrowphaseTestCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().narrowphaseTestCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetResolvedContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().resolvedContactCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSimulationIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().simulationIslandCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetActiveIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().activeIslandCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSleepingIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().sleepingIslandCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetWarmStartedContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().warmStartedContactCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetPersistentContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().persistentContactCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContinuousCollisionBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionBodyCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContinuousCollisionSweepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionSweepCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContinuousCollisionHitCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionHitCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContinuousCollisionClampedBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionClampedBodyCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetContinuousCollisionUnsupportedBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionUnsupportedBodyCount())
            : 0);
    return 1;
}

} // namespace heritage::modules
