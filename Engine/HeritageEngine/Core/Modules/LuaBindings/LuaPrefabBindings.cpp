#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../Entities/EntityPrefabDocument.hpp"
#include "../../Entities/EntityRegistry.hpp"
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

int LuaCoreBindingHandlers::luaPrefabIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool available = runtime->m_entities && runtime->m_context.has_value();
    runtime->m_api.lua_pushboolean(state, available ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaPrefabExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path path = runtime->m_context
        ? runtime->m_context->resolvePrefabPath(relativePath)
        : std::filesystem::path{};

    const bool exists = !path.empty()
        && path.extension() == ".hprefab"
        && std::filesystem::is_regular_file(path);
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaPrefabInstantiate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPrefabError.clear();
    if (!runtime->m_entities || !runtime->m_context)
    {
        runtime->m_lastPrefabError = "Prefab service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path path =
        runtime->m_context->resolvePrefabPath(relativePath);
    if (path.empty() || path.extension() != ".hprefab")
    {
        runtime->m_lastPrefabError =
            "Prefab.Instantiate requires a safe module-Prefabs-relative .hprefab path.";
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    heritage::entities::PrefabInstantiationOptions options;
    options.rootName = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    options.position = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0))
    };
    options.rotationDegrees = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0))
    };
    options.scale = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 10, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 11, 1.0))
    };
    options.namePrefix = LuaModuleRuntime::stringArgument(*runtime, state, 12);

    heritage::entities::PrefabInstantiationResult result;
    if (!heritage::entities::EntityPrefabDocument::instantiate(
            path,
            *runtime->m_entities,
            options,
            result,
            runtime->m_lastPrefabError))
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(result.root));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(result.entities.size()));
    return 2;
}

int LuaCoreBindingHandlers::luaPrefabGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastPrefabError.c_str(),
        runtime->m_lastPrefabError.size());
    return 1;
}

void LuaModuleRuntime::registerPrefabBindings()
{
    registerFunction("Prefab", "IsAvailable", &LuaCoreBindingHandlers::luaPrefabIsAvailable);
    registerFunction("Prefab", "Exists", &LuaCoreBindingHandlers::luaPrefabExists);
    registerFunction("Prefab", "Instantiate", &LuaCoreBindingHandlers::luaPrefabInstantiate);
    registerFunction("Prefab", "GetLastError", &LuaCoreBindingHandlers::luaPrefabGetLastError);
}

} // namespace heritage::modules
