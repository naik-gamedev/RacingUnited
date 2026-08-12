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

int LuaEntityBindingHandlers::luaEntitySetDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string requestedType = LuaModuleRuntime::stringArgument(*runtime, state, 2, "box");
    std::string normalizedType = requestedType;
    std::transform(normalizedType.begin(), normalizedType.end(), normalizedType.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    heritage::entities::DebugPrimitiveType type = heritage::entities::DebugPrimitiveType::Box;
    bool validType = true;
    if (normalizedType == "box" || normalizedType == "cube")
        type = heritage::entities::DebugPrimitiveType::Box;
    else if (normalizedType == "cylinder" || normalizedType == "wheel")
        type = heritage::entities::DebugPrimitiveType::Cylinder;
    else if (normalizedType == "sphere")
        type = heritage::entities::DebugPrimitiveType::Sphere;
    else
        validType = false;

    const heritage::math::Vec3 color{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.65)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.72)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.82))
    };

    const bool result = validType
        && runtime->m_entities
        && runtime->m_entities->setDebugPrimitive(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), type, color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityRemoveDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->removeDebugPrimitive(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityHasDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->hasDebugPrimitive(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetDebugVisible(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setDebugPrimitiveVisible(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntitySetDebugColor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 color{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.65)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.72)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.82))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setDebugPrimitiveColor(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaEntityBindingHandlers::luaEntityGetDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::entities::DebugPrimitiveComponent component;
    if (!runtime->m_entities
        || !runtime->m_entities->debugPrimitive(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1), component))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const char* typeName = "box";
    if (component.type == heritage::entities::DebugPrimitiveType::Cylinder)
        typeName = "cylinder";
    else if (component.type == heritage::entities::DebugPrimitiveType::Sphere)
        typeName = "sphere";

    runtime->m_api.lua_pushstring(state, typeName);
    runtime->m_api.lua_pushnumber(state, component.color.x);
    runtime->m_api.lua_pushnumber(state, component.color.y);
    runtime->m_api.lua_pushnumber(state, component.color.z);
    runtime->m_api.lua_pushboolean(state, component.visible ? 1 : 0);
    return 5;
}

} // namespace heritage::modules
