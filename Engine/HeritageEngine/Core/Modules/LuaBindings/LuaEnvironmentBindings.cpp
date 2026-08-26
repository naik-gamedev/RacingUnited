#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>

#include "../../../Graphics/EnvironmentSystem.hpp"
#include "../../../Graphics/GltfBinary.hpp"
#include "../../Paths/Utf8Path.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;
namespace {

const heritage::graphics::AssetMetadataValue* metadataValue(
    const heritage::graphics::GlbMetadataDocument& document,
    const std::string& key)
{
    const auto sceneFound = document.sceneMetadata.find(key);
    if (sceneFound != document.sceneMetadata.end())
        return &sceneFound->second;

    // Blender's glTF exporter reliably exports object Custom Properties as
    // node extras. Prefer a dedicated top-level Empty named
    // Heritage_SceneMetadata, but accept the same keys on any node so existing
    // creator scenes do not need hierarchy surgery.
    for (const auto& node : document.nodes)
    {
        if (node.name != "Heritage_SceneMetadata")
            continue;
        const auto found = node.metadata.find(key);
        if (found != node.metadata.end())
            return &found->second;
    }
    for (const auto& node : document.nodes)
    {
        const auto found = node.metadata.find(key);
        if (found != node.metadata.end())
            return &found->second;
    }
    return nullptr;
}

bool metadataNumber(
    const heritage::graphics::GlbMetadataDocument& document,
    const std::string& key,
    double& value)
{
    const auto* metadata = metadataValue(document, key);
    if (!metadata || metadata->type != heritage::graphics::AssetMetadataValueType::Number)
        return false;
    value = metadata->numberValue;
    return std::isfinite(value);
}

bool metadataString(
    const heritage::graphics::GlbMetadataDocument& document,
    const std::string& key,
    std::string& value)
{
    const auto* metadata = metadataValue(document, key);
    if (!metadata || metadata->type != heritage::graphics::AssetMetadataValueType::String)
        return false;
    value = metadata->stringValue;
    return !value.empty();
}

} // namespace

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

int LuaCoreBindingHandlers::luaEnvironmentGetDate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const auto date = runtime->m_environment
        ? runtime->m_environment->date()
        : heritage::graphics::EnvironmentCalendarDate{};
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(date.year));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(date.month));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(date.day));
    return 3;
}

int LuaCoreBindingHandlers::luaEnvironmentSetDate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const int year = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 2026.0));
    const int month = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const int day = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));
    const bool result = runtime->m_environment
        && runtime->m_environment->setDate(year, month, day);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentGetLocation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const auto location = runtime->m_environment
        ? runtime->m_environment->location()
        : heritage::graphics::EnvironmentLocation{};
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(location.latitudeDeg));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(location.longitudeDeg));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(location.elevationM));
    runtime->m_api.lua_pushlstring(state, location.timezone.c_str(), location.timezone.size());
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_environment
            ? runtime->m_environment->effectiveUtcOffsetMinutes()
            : 0));
    return 5;
}

int LuaCoreBindingHandlers::luaEnvironmentSetLocation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_environment)
    {
        const double latitude = LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0);
        const double longitude = LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0);
        const double elevation = LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0);
        const std::string timezone = LuaModuleRuntime::stringArgument(*runtime, state, 4, "AUTO");
        const int offset = static_cast<int>(LuaModuleRuntime::numberArgument(
            *runtime, state, 5, static_cast<double>((std::numeric_limits<int>::min)())));
        runtime->m_environment->setLocation(
            latitude, longitude, elevation, timezone, offset);
    }
    runtime->m_api.lua_pushboolean(state, runtime->m_environment ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaEnvironmentApplySceneMetadata(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_environment || !runtime->m_context)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path absolutePath = runtime->m_context->resolveAssetPath(
        heritage::paths::fromUtf8(relativePath));
    heritage::graphics::GlbMetadataDocument document;
    std::string errorMessage;
    if (absolutePath.empty()
        || !heritage::graphics::inspectGlbMetadata(absolutePath, document, errorMessage))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    double latitude = 0.0;
    double longitude = 0.0;
    if (!metadataNumber(document, "heritage.latitude_deg", latitude)
        || !metadataNumber(document, "heritage.longitude_deg", longitude))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    double elevation = 0.0;
    metadataNumber(document, "heritage.elevation_m", elevation);
    std::string timezone = "AUTO";
    metadataString(document, "heritage.timezone", timezone);
    double offsetValue = static_cast<double>((std::numeric_limits<int>::min)());
    metadataNumber(document, "heritage.utc_offset_minutes", offsetValue);
    const int offset = offsetValue <= static_cast<double>((std::numeric_limits<int>::min)()) + 1.0
        ? (std::numeric_limits<int>::min)()
        : static_cast<int>(std::lround(offsetValue));

    runtime->m_environment->setLocation(
        latitude, longitude, elevation, timezone, offset);
    runtime->m_api.lua_pushboolean(state, 1);
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
    registerFunction("Environment", "GetDate", &LuaCoreBindingHandlers::luaEnvironmentGetDate);
    registerFunction("Environment", "SetDate", &LuaCoreBindingHandlers::luaEnvironmentSetDate);
    registerFunction("Environment", "GetLocation", &LuaCoreBindingHandlers::luaEnvironmentGetLocation);
    registerFunction("Environment", "SetLocation", &LuaCoreBindingHandlers::luaEnvironmentSetLocation);
    registerFunction("Environment", "ApplySceneMetadata", &LuaCoreBindingHandlers::luaEnvironmentApplySceneMetadata);
    registerFunction("Environment", "IsCycleEnabled", &LuaCoreBindingHandlers::luaEnvironmentIsCycleEnabled);
    registerFunction("Environment", "SetCycleEnabled", &LuaCoreBindingHandlers::luaEnvironmentSetCycleEnabled);
    registerFunction("Environment", "GetTimeScale", &LuaCoreBindingHandlers::luaEnvironmentGetTimeScale);
    registerFunction("Environment", "SetTimeScale", &LuaCoreBindingHandlers::luaEnvironmentSetTimeScale);
    registerFunction("Environment", "GetSunDirection", &LuaCoreBindingHandlers::luaEnvironmentGetSunDirection);
}

} // namespace heritage::modules
