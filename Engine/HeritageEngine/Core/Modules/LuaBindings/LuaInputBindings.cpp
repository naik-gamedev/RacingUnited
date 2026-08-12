#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../Input/InputSystem.hpp"
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

int LuaCoreBindingHandlers::luaInputIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_input && runtime->m_input->isAvailable() ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputRegisterAction(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string binding = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string group = LuaModuleRuntime::stringArgument(*runtime, state, 3, "Common");
    const bool result = runtime->m_input
        && runtime->m_input->registerAction(action, binding, group);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputDown(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionDown(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputPressed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionPressed(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionReleased(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputValue(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const float value = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->actionValue(action)
        : 0.0f;
    runtime->m_api.lua_pushnumber(state, value);
    return 1;
}

int LuaCoreBindingHandlers::luaInputGetBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string binding = runtime->m_input
        ? runtime->m_input->actionBinding(action)
        : std::string{};
    runtime->m_api.lua_pushlstring(state, binding.c_str(), binding.size());
    return 1;
}

int LuaCoreBindingHandlers::luaInputGetBindingCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::size_t count = runtime->m_input
        ? runtime->m_input->actionBindingCount(action)
        : 0;
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<heritage::modules::LuaInteger>(count));
    return 1;
}

int LuaCoreBindingHandlers::luaInputGetBindingAt(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const int oneBasedIndex = static_cast<int>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const std::string binding = runtime->m_input && oneBasedIndex > 0
        ? runtime->m_input->actionBinding(
            action,
            static_cast<std::size_t>(oneBasedIndex - 1))
        : std::string{};
    runtime->m_api.lua_pushlstring(state, binding.c_str(), binding.size());
    return 1;
}

int LuaCoreBindingHandlers::luaInputBind(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string binding = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const int argumentCount = runtime->m_api.lua_gettop(state);
    bool result = false;
    if (runtime->m_input)
    {
        if (argumentCount >= 3)
        {
            const int oneBasedIndex = static_cast<int>(
                LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));
            result = oneBasedIndex > 0
                && runtime->m_input->setBinding(
                    action,
                    static_cast<std::size_t>(oneBasedIndex - 1),
                    binding);
        }
        else
        {
            result = runtime->m_input->setBinding(action, binding);
        }
    }
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputAddBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string binding = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const bool result = runtime->m_input
        && runtime->m_input->addBinding(action, binding);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputRemoveBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const int oneBasedIndex = static_cast<int>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const bool result = runtime->m_input
        && oneBasedIndex > 0
        && runtime->m_input->removeBinding(
            action,
            static_cast<std::size_t>(oneBasedIndex - 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputResetBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool result = runtime->m_input
        && runtime->m_input->resetBindings(action);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputResetBindings(lua_State* state)
{
    return luaInputResetBinding(state);
}

int LuaCoreBindingHandlers::luaInputKeyDown(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyDown(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputKeyPressed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyPressed(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputKeyReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyReleased(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputMouseDown(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mouseDown(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputMousePressed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mousePressed(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputMouseReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mouseReleased(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputMouseDelta(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const double x = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->mouseDeltaX()
        : 0.0;
    const double y = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->mouseDeltaY()
        : 0.0;
    runtime->m_api.lua_pushnumber(state, x);
    runtime->m_api.lua_pushnumber(state, y);
    return 2;
}

int LuaCoreBindingHandlers::luaInputGamepadConnected(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const int ordinal = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0));
    const bool value = runtime->m_input
        && runtime->m_input->gamepadConnected(ordinal);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaInputGetGamepadName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const int ordinal = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0));
    const std::string name = runtime->m_input
        ? runtime->m_input->gamepadName(ordinal)
        : std::string{};
    runtime->m_api.lua_pushlstring(state, name.c_str(), name.size());
    return 1;
}

int LuaCoreBindingHandlers::luaInputGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string error = runtime->m_input
        ? runtime->m_input->lastError()
        : std::string("InputSystem is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

void LuaModuleRuntime::registerInputBindings()
{
    registerFunction("Input", "IsAvailable", &LuaCoreBindingHandlers::luaInputIsAvailable);
    registerFunction("Input", "RegisterAction", &LuaCoreBindingHandlers::luaInputRegisterAction);
    registerFunction("Input", "Down", &LuaCoreBindingHandlers::luaInputDown);
    registerFunction("Input", "Pressed", &LuaCoreBindingHandlers::luaInputPressed);
    registerFunction("Input", "Released", &LuaCoreBindingHandlers::luaInputReleased);
    registerFunction("Input", "Value", &LuaCoreBindingHandlers::luaInputValue);
    registerFunction("Input", "GetBinding", &LuaCoreBindingHandlers::luaInputGetBinding);
    registerFunction("Input", "GetBindingCount", &LuaCoreBindingHandlers::luaInputGetBindingCount);
    registerFunction("Input", "GetBindingAt", &LuaCoreBindingHandlers::luaInputGetBindingAt);
    registerFunction("Input", "Bind", &LuaCoreBindingHandlers::luaInputBind);
    registerFunction("Input", "AddBinding", &LuaCoreBindingHandlers::luaInputAddBinding);
    registerFunction("Input", "RemoveBinding", &LuaCoreBindingHandlers::luaInputRemoveBinding);
    registerFunction("Input", "ResetBinding", &LuaCoreBindingHandlers::luaInputResetBinding);
    registerFunction("Input", "ResetBindings", &LuaCoreBindingHandlers::luaInputResetBindings);
    registerFunction("Input", "KeyDown", &LuaCoreBindingHandlers::luaInputKeyDown);
    registerFunction("Input", "KeyPressed", &LuaCoreBindingHandlers::luaInputKeyPressed);
    registerFunction("Input", "KeyReleased", &LuaCoreBindingHandlers::luaInputKeyReleased);
    registerFunction("Input", "MouseDown", &LuaCoreBindingHandlers::luaInputMouseDown);
    registerFunction("Input", "MousePressed", &LuaCoreBindingHandlers::luaInputMousePressed);
    registerFunction("Input", "MouseReleased", &LuaCoreBindingHandlers::luaInputMouseReleased);
    registerFunction("Input", "MouseDelta", &LuaCoreBindingHandlers::luaInputMouseDelta);
    registerFunction("Input", "GamepadConnected", &LuaCoreBindingHandlers::luaInputGamepadConnected);
    registerFunction("Input", "GetGamepadName", &LuaCoreBindingHandlers::luaInputGetGamepadName);
    registerFunction("Input", "GetLastError", &LuaCoreBindingHandlers::luaInputGetLastError);
}

} // namespace heritage::modules
