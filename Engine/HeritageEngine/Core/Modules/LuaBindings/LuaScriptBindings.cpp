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
#include "../../Paths/Utf8Path.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaCoreBindingHandlers::luaScriptInclude(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path resolved =
        runtime->m_context->resolveScriptPath(relativePath);

    if (relativePath.empty() || resolved.empty())
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(
            state,
            "Script.Include requires a safe path relative to the module Scripts folder.");
        return 2;
    }

    if (resolved.extension() != ".lua")
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(
            state,
            "Script.Include accepts text .lua files only.");
        return 2;
    }

    if (!std::filesystem::is_regular_file(resolved))
    {
        const std::string message =
            "Included Lua file was not found: " + resolved.string();
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    const int loadStatus = runtime->m_api.luaL_loadfilex(
        state,
        resolved.string().c_str(),
        "t");
    if (loadStatus != kLuaOk)
    {
        const std::string message =
            "Lua syntax/load error in included file " + resolved.string()
            + ":\n" + runtime->stackString(-1);
        runtime->pop(1);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    const int callStatus = runtime->m_api.lua_pcallk(
        state,
        0,
        0,
        0,
        0,
        nullptr);
    if (callStatus != kLuaOk)
    {
        const std::string message =
            "Lua runtime error in included file " + resolved.string()
            + ":\n" + runtime->stackString(-1);
        runtime->pop(1);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

void LuaModuleRuntime::registerScriptBindings()
{
    registerFunction("Script", "Include", &LuaCoreBindingHandlers::luaScriptInclude);
}

} // namespace heritage::modules
