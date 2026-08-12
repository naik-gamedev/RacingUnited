#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../Graphics/EnvironmentSystem.hpp"
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

int LuaCoreBindingHandlers::luaEnvironmentGetTimeOfDay(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_environment
            ? static_cast<LuaNumber>(runtime->m_environment->timeOfDayHours())
            : 0.0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentSetTimeOfDay(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_environment)
    {
        runtime->m_environment->setTimeOfDayHours(static_cast<float>(
            LuaModuleRuntime::numberArgument(*runtime, state, 1, 12.0)));
    }
    runtime->m_api.lua_pushboolean(state, runtime->m_environment ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentIsCycleEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_environment && runtime->m_environment->cycleEnabled() ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentSetCycleEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_environment)
    {
        runtime->m_environment->setCycleEnabled(
            LuaModuleRuntime::booleanArgument(*runtime, state, 1, true));
    }
    runtime->m_api.lua_pushboolean(state, runtime->m_environment ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentGetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_environment
            ? static_cast<LuaNumber>(runtime->m_environment->timeScale())
            : 0.0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentSetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_environment)
    {
        runtime->m_environment->setTimeScale(static_cast<float>(
            LuaModuleRuntime::numberArgument(*runtime, state, 1, 240.0)));
    }
    runtime->m_api.lua_pushboolean(state, runtime->m_environment ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentGetSunDirection(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 direction = runtime->m_environment
        ? runtime->m_environment->lighting().sunDirection
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(direction.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(direction.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(direction.z));
    return 3;
}

void LuaModuleRuntime::registerEnvironmentBindings()
{
    registerFunction("Environment", "GetTimeOfDay", &LuaCoreBindingHandlers::luaEnvironmentGetTimeOfDay);
    registerFunction("Environment", "SetTimeOfDay", &LuaCoreBindingHandlers::luaEnvironmentSetTimeOfDay);
    registerFunction("Environment", "IsCycleEnabled", &LuaCoreBindingHandlers::luaEnvironmentIsCycleEnabled);
    registerFunction("Environment", "SetCycleEnabled", &LuaCoreBindingHandlers::luaEnvironmentSetCycleEnabled);
    registerFunction("Environment", "GetTimeScale", &LuaCoreBindingHandlers::luaEnvironmentGetTimeScale);
    registerFunction("Environment", "SetTimeScale", &LuaCoreBindingHandlers::luaEnvironmentSetTimeScale);
    registerFunction("Environment", "GetSunDirection", &LuaCoreBindingHandlers::luaEnvironmentGetSunDirection);
}

} // namespace heritage::modules
