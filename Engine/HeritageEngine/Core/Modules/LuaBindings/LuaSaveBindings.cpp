#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

int LuaCoreBindingHandlers::luaSaveGetString(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string key = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string fallback = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string value = runtime->m_saveStore.getString(key, fallback);
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaCoreBindingHandlers::luaSaveSetString(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool saved = runtime->m_saveStore.setString(
        LuaModuleRuntime::stringArgument(*runtime, state, 1),
        LuaModuleRuntime::stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveGetInt(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    int validFallback = 0;
    const LuaInteger fallback = runtime->m_api.lua_tointegerx(
        state, 2, &validFallback);
    const std::int64_t value = runtime->m_saveStore.getInteger(
        LuaModuleRuntime::stringArgument(*runtime, state, 1),
        validFallback ? static_cast<std::int64_t>(fallback) : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(value));
    return 1;
}

int LuaCoreBindingHandlers::luaSaveSetInt(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    int validValue = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(
        state, 2, &validValue);
    const bool saved = validValue
        && runtime->m_saveStore.setInteger(
            LuaModuleRuntime::stringArgument(*runtime, state, 1),
            static_cast<std::int64_t>(value));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveGetNumber(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const double fallback = LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0);
    const double value = runtime->m_saveStore.getNumber(
        LuaModuleRuntime::stringArgument(*runtime, state, 1), fallback);
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaCoreBindingHandlers::luaSaveSetNumber(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    int validValue = 0;
    const LuaNumber value = runtime->m_api.lua_tonumberx(
        state, 2, &validValue);
    const bool saved = validValue
        && runtime->m_saveStore.setNumber(
            LuaModuleRuntime::stringArgument(*runtime, state, 1),
            static_cast<double>(value));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveGetBool(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool fallback = LuaModuleRuntime::booleanArgument(*runtime, state, 2, false);
    const bool value = runtime->m_saveStore.getBoolean(
        LuaModuleRuntime::stringArgument(*runtime, state, 1), fallback);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveSetBool(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool value = LuaModuleRuntime::booleanArgument(*runtime, state, 2, false);
    const bool saved = runtime->m_saveStore.setBoolean(
        LuaModuleRuntime::stringArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveHas(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool exists = runtime->m_saveStore.has(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveRemove(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool removed = runtime->m_saveStore.remove(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, removed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveClear(lua_State* state)
{
    if (LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state))
    {
        runtime->m_saveStore.clear();
        runtime->m_api.lua_pushboolean(state, 1);
        return 1;
    }
    return 0;
}

int LuaCoreBindingHandlers::luaSaveFlush(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool flushed = runtime->m_saveStore.flush();
    runtime->m_saveFlushTimer = 0.0f;
    runtime->m_api.lua_pushboolean(state, flushed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSaveGetPath(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string value = runtime->m_saveStore.path().string();
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaCoreBindingHandlers::luaSaveGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string& error = runtime->m_saveStore.lastError();
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaCoreBindingHandlers::luaSaveIsDirty(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state, runtime->m_saveStore.isDirty() ? 1 : 0);
    return 1;
}

void LuaModuleRuntime::registerSaveBindings()
{
    registerFunction("Save", "GetString", &LuaCoreBindingHandlers::luaSaveGetString);
    registerFunction("Save", "SetString", &LuaCoreBindingHandlers::luaSaveSetString);
    registerFunction("Save", "GetInt", &LuaCoreBindingHandlers::luaSaveGetInt);
    registerFunction("Save", "SetInt", &LuaCoreBindingHandlers::luaSaveSetInt);
    registerFunction("Save", "GetNumber", &LuaCoreBindingHandlers::luaSaveGetNumber);
    registerFunction("Save", "SetNumber", &LuaCoreBindingHandlers::luaSaveSetNumber);
    registerFunction("Save", "GetBool", &LuaCoreBindingHandlers::luaSaveGetBool);
    registerFunction("Save", "SetBool", &LuaCoreBindingHandlers::luaSaveSetBool);
    registerFunction("Save", "Has", &LuaCoreBindingHandlers::luaSaveHas);
    registerFunction("Save", "Remove", &LuaCoreBindingHandlers::luaSaveRemove);
    registerFunction("Save", "Clear", &LuaCoreBindingHandlers::luaSaveClear);
    registerFunction("Save", "Flush", &LuaCoreBindingHandlers::luaSaveFlush);
    registerFunction("Save", "GetPath", &LuaCoreBindingHandlers::luaSaveGetPath);
    registerFunction("Save", "GetLastError", &LuaCoreBindingHandlers::luaSaveGetLastError);
    registerFunction("Save", "IsDirty", &LuaCoreBindingHandlers::luaSaveIsDirty);
}

} // namespace heritage::modules
