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

int LuaPhysicsBindingHandlers::luaPhysicsCreateSpringConstraint(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::SpringConstraintDescription description;
    description.bodyA = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1);
    description.bodyB = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 2);
    description.localAnchorA = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0))
    };
    description.anchorB = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0))
    };
    description.restLength = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 1.0));
    description.stiffness = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 10, 1000.0));
    description.damping = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 11, 100.0));
    description.maximumForce = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 12, 1000000.0));
    description.enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 13, true);

    const heritage::physics::ConstraintHandle handle =
        runtime->m_physics->constraints().createSpring(
            description,
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsDestroyConstraint(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().destroy(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsConstraintExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().exists(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->constraints().count())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetEnabledConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->constraints().enabledCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetActiveConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->constraints().activeCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetConstraintEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().setEnabled(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetConstraintEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().enabled(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1),
            value);
    if (result)
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    else
        runtime->m_api.lua_pushnil(state);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetSpringConstraintProperties(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().setSpringProperties(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1000.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 100.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 1000000.0)),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSpringConstraintState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    heritage::physics::SpringConstraintState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().state(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 4; ++index)
            runtime->m_api.lua_pushnil(state);
        return 4;
    }
    runtime->m_api.lua_pushnumber(state, value.currentLength);
    runtime->m_api.lua_pushnumber(state, value.extension);
    runtime->m_api.lua_pushnumber(state, value.relativeSpeed);
    runtime->m_api.lua_pushnumber(state, value.appliedForce);
    return 4;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetConstraintBodyA(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle body = runtime->m_physics
        ? runtime->m_physics->constraints().bodyA(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(body));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetConstraintBodyB(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle body = runtime->m_physics
        ? runtime->m_physics->constraints().bodyB(
            LuaModuleRuntime::constraintHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(body));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string error = !runtime->m_lastPhysicsError.empty()
        ? runtime->m_lastPhysicsError
        : (runtime->m_physics
            ? runtime->m_physics->lastError()
            : std::string("PhysicsWorld is unavailable."));
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

} // namespace heritage::modules
