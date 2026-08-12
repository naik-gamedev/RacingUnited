#include "../../LuaModuleRuntime.hpp"
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
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaEntityBindingHandlers::luaEntitySetPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setPosition(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->position(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaEntityBindingHandlers::luaEntitySetRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setRotationDegrees(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->rotationDegrees(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaEntityBindingHandlers::luaEntitySetScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setScale(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->scale(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaEntityBindingHandlers::luaEntitySetWorldPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldPosition(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetWorldPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldPosition(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaEntityBindingHandlers::luaEntitySetWorldRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldRotationDegrees(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetWorldRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldRotationDegrees(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaEntityBindingHandlers::luaEntitySetWorldScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldScale(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetWorldScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldScale(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

} // namespace heritage::modules
