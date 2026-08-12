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
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../../Diagnostics/BuildIdentity.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaCoreBindingHandlers::luaEngineOpenSettings(lua_State* state)
{
    if (LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state))
        runtime->queueAction(ModuleRuntimeActionType::OpenEngineSettings);
    return 0;
}

int LuaCoreBindingHandlers::luaEngineExit(lua_State* state)
{
    if (LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state))
        runtime->queueAction(ModuleRuntimeActionType::ExitApplication);
    return 0;
}

int LuaCoreBindingHandlers::luaEngineSetClearColor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_clearColor = {
        clampFloat(static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)), 0.0f, 1.0f),
        clampFloat(static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)), 0.0f, 1.0f),
        clampFloat(static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)), 0.0f, 1.0f)
    };
    return 0;
}

int LuaCoreBindingHandlers::luaEngineLog(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime)
    {
        std::cout << "[Lua:";
        std::cout << (runtime->m_context ? runtime->m_context->module().id : "?");
        std::cout << "] " << LuaModuleRuntime::stringArgument(*runtime, state, 1) << '\n';
    }
    return 0;
}

int LuaCoreBindingHandlers::luaEngineGetBuildIdentity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string identity = heritage::diagnostics::buildIdentity();
    runtime->m_api.lua_pushlstring(state, identity.data(), identity.size());
    return 1;
}

int LuaCoreBindingHandlers::luaEngineGetBuildStep(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::generated::kMilestone);
    return 1;
}

int LuaCoreBindingHandlers::luaEngineGetGitCommit(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::generated::kGitCommit);
    return 1;
}

int LuaCoreBindingHandlers::luaEngineGetBuildConfiguration(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::compiledConfiguration());
    return 1;
}

int LuaCoreBindingHandlers::luaEngineGetLuaApiCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_registeredLuaFunctions.size()));
    return 1;
}

int LuaCoreBindingHandlers::luaEngineGetLuaApiName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const double rawIndex = LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0);
    if (rawIndex < 1.0)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const std::size_t index = static_cast<std::size_t>(rawIndex - 1.0);
    if (index >= runtime->m_registeredLuaFunctions.size())
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const std::string& name = runtime->m_registeredLuaFunctions[index];
    runtime->m_api.lua_pushlstring(state, name.data(), name.size());
    return 1;
}

int LuaCoreBindingHandlers::luaEngineDumpLuaAPI(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    std::vector<std::string> names = runtime->m_registeredLuaFunctions;
    std::sort(names.begin(), names.end());

    std::cout << "[LuaAPI] " << heritage::diagnostics::buildIdentity() << '\n';
    std::cout << "[LuaAPI] Registered functions: " << names.size() << '\n';
    for (const std::string& name : names)
        std::cout << "[LuaAPI] " << name << '\n';

    runtime->writeLuaApiManifest();
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(names.size()));
    return 1;
}

int LuaCoreBindingHandlers::luaEngineRunSafetySmokeTests(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    std::string summary;
    std::filesystem::path reportPath;
    const bool passed = runtime->runSafetySmokeTests(summary, reportPath);

    runtime->m_api.lua_pushboolean(state, passed ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, summary.data(), summary.size());
    const std::string pathString = reportPath.string();
    runtime->m_api.lua_pushlstring(
        state,
        pathString.data(),
        pathString.size());
    return 3;
}

int LuaCoreBindingHandlers::luaEngineGetLastSafetyReport(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastSafetyReport.data(),
        runtime->m_lastSafetyReport.size());
    const std::string pathString = runtime->m_lastSafetyReportPath.string();
    runtime->m_api.lua_pushlstring(
        state,
        pathString.data(),
        pathString.size());
    return 2;
}

void LuaModuleRuntime::registerEngineBindings()
{
    registerFunction("Engine", "OpenSettings", &LuaCoreBindingHandlers::luaEngineOpenSettings);
    registerFunction("Engine", "Exit", &LuaCoreBindingHandlers::luaEngineExit);
    registerFunction("Engine", "SetClearColor", &LuaCoreBindingHandlers::luaEngineSetClearColor);
    registerFunction("Engine", "Log", &LuaCoreBindingHandlers::luaEngineLog);
    registerFunction("Engine", "GetBuildIdentity", &LuaCoreBindingHandlers::luaEngineGetBuildIdentity);
    registerFunction("Engine", "GetBuildStep", &LuaCoreBindingHandlers::luaEngineGetBuildStep);
    registerFunction("Engine", "GetGitCommit", &LuaCoreBindingHandlers::luaEngineGetGitCommit);
    registerFunction("Engine", "GetBuildConfiguration", &LuaCoreBindingHandlers::luaEngineGetBuildConfiguration);
    registerFunction("Engine", "GetLuaApiCount", &LuaCoreBindingHandlers::luaEngineGetLuaApiCount);
    registerFunction("Engine", "GetLuaApiName", &LuaCoreBindingHandlers::luaEngineGetLuaApiName);
    registerFunction("Engine", "DumpLuaAPI", &LuaCoreBindingHandlers::luaEngineDumpLuaAPI);
    registerFunction("Engine", "RunSafetySmokeTests", &LuaCoreBindingHandlers::luaEngineRunSafetySmokeTests);
    registerFunction("Engine", "GetLastSafetyReport", &LuaCoreBindingHandlers::luaEngineGetLastSafetyReport);
}

} // namespace heritage::modules
