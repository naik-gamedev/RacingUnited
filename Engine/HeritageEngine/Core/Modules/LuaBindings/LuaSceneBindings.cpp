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

int LuaCoreBindingHandlers::luaSceneLoad(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string sceneId = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    if (sceneId.empty())
    {
        runtime->m_lastSceneError = "Scene.Load requires a non-empty scene ID.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    if (!runtime->m_sceneManager.exists(sceneId, *runtime->m_context))
    {
        runtime->m_lastSceneError = "Scene '" + sceneId
            + "' does not exist in this module and is not a registered built-in scene.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastSceneError.clear();
    runtime->requestSceneLoad(sceneId, false);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaSceneReload(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string current = runtime->m_sceneManager.activeSceneId();
    if (current.empty())
    {
        runtime->m_lastSceneError = "Scene.Reload cannot run because no scene is active.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastSceneError.clear();
    runtime->requestSceneLoad(current, true);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaSceneGetCurrent(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string current = runtime->m_sceneManager.activeSceneId();
    runtime->m_api.lua_pushlstring(state, current.c_str(), current.size());
    return 1;
}

int LuaCoreBindingHandlers::luaSceneExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string sceneId = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool exists = !sceneId.empty()
        && runtime->m_sceneManager.exists(sceneId, *runtime->m_context);
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaSceneGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastSceneError.c_str(),
        runtime->m_lastSceneError.size());
    return 1;
}

void LuaModuleRuntime::registerSceneBindings()
{
    registerFunction("Scene", "Load", &LuaCoreBindingHandlers::luaSceneLoad);
    registerFunction("Scene", "Reload", &LuaCoreBindingHandlers::luaSceneReload);
    registerFunction("Scene", "GetCurrent", &LuaCoreBindingHandlers::luaSceneGetCurrent);
    registerFunction("Scene", "Exists", &LuaCoreBindingHandlers::luaSceneExists);
    registerFunction("Scene", "GetLastError", &LuaCoreBindingHandlers::luaSceneGetLastError);
}

} // namespace heritage::modules
