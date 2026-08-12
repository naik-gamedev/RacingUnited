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

int LuaEntityBindingHandlers::luaEntityIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(state, runtime->m_entities ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityCreate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string name = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->create(name)
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaEntityBindingHandlers::luaEntityDestroy(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->destroy(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->exists(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const LuaInteger count = runtime->m_entities
        ? static_cast<LuaInteger>(runtime->m_entities->count())
        : 0;
    runtime->m_api.lua_pushinteger(state, count);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetPersistentId(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::uint64_t id = runtime->m_entities
        ? runtime->m_entities->persistentId(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1))
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(id));
    return 1;
}

int LuaEntityBindingHandlers::luaEntityFindByName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->findByName(LuaModuleRuntime::stringArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->setName(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string value = runtime->m_entities
        ? runtime->m_entities->name(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1))
        : std::string{};
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaEntityBindingHandlers::luaEntityAddTag(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->addTag(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityRemoveTag(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->removeTag(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityHasTag(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->hasTag(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityFindFirstWithTag(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->findFirstWithTag(LuaModuleRuntime::stringArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetParent(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool keepWorld = LuaModuleRuntime::booleanArgument(*runtime, state, 3, false);
    const bool result = runtime->m_entities
        && runtime->m_entities->setParent(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 2),
            keepWorld);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityClearParent(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool keepWorld = LuaModuleRuntime::booleanArgument(*runtime, state, 2, true);
    const bool result = runtime->m_entities
        && runtime->m_entities->clearParent(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            keepWorld);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetParent(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->parent(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetChildCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::size_t count = runtime->m_entities
        ? runtime->m_entities->childCount(LuaModuleRuntime::entityHandleArgument(*runtime, state, 1))
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetChildAt(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const double requestedIndex = LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0);
    const std::size_t zeroBasedIndex = requestedIndex >= 1.0
        ? static_cast<std::size_t>(requestedIndex - 1.0)
        : static_cast<std::size_t>(-1);
    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->childAt(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            zeroBasedIndex)
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaEntityBindingHandlers::luaEntityIsDescendantOf(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->isDescendantOf(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

} // namespace heritage::modules
