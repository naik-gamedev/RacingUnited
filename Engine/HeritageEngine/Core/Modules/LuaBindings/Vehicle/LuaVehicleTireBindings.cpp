#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../../Physics/PhysicsWorld.hpp"
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

int LuaVehicleBindingHandlers::luaVehicleSetTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setTireModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::numberArgument(*runtime, state, 2, 3500.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.15),
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 90000.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 5, 80000.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.12),
            LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.35),
            LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.45),
            LuaModuleRuntime::numberArgument(*runtime, state, 9, 1.55),
            LuaModuleRuntime::numberArgument(*runtime, state, 10, 0.075),
            LuaModuleRuntime::numberArgument(*runtime, state, 11, 0.85),
            LuaModuleRuntime::numberArgument(*runtime, state, 12, 1.65),
            LuaModuleRuntime::numberArgument(*runtime, state, 13, 1.30),
            LuaModuleRuntime::numberArgument(*runtime, state, 14, 0.20),
            LuaModuleRuntime::numberArgument(*runtime, state, 15, 0.15),
            LuaModuleRuntime::numberArgument(*runtime, state, 16, 2.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 17, 0.70));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetWheelTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelTireModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndex,
            LuaModuleRuntime::numberArgument(*runtime, state, 3, 3500.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.15),
            LuaModuleRuntime::numberArgument(*runtime, state, 5, 90000.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 6, 80000.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.12),
            LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.35),
            LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.45),
            LuaModuleRuntime::numberArgument(*runtime, state, 10, 1.55),
            LuaModuleRuntime::numberArgument(*runtime, state, 11, 0.075),
            LuaModuleRuntime::numberArgument(*runtime, state, 12, 0.85),
            LuaModuleRuntime::numberArgument(*runtime, state, 13, 1.65),
            LuaModuleRuntime::numberArgument(*runtime, state, 14, 1.30),
            LuaModuleRuntime::numberArgument(*runtime, state, 15, 0.20),
            LuaModuleRuntime::numberArgument(*runtime, state, 16, 0.15),
            LuaModuleRuntime::numberArgument(*runtime, state, 17, 2.0),
            LuaModuleRuntime::numberArgument(*runtime, state, 18, 0.70));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleLoadWheelTirePropertyFile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    const std::string provenance = LuaModuleRuntime::stringArgument(
        *runtime, state, 4, "unspecified_property_file");
    const heritage::vehicles::VehicleScalar confidence = LuaModuleRuntime::numberArgument(
        *runtime, state, 5, 0.0);

    const std::filesystem::path resolved = runtime->m_context
        ? runtime->m_context->resolveModulePath(relativePath)
        : std::filesystem::path{};
    const bool result = runtime->m_physics
        && !resolved.empty()
        && runtime->m_physics->vehicles().loadWheelTirePropertyFile(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndex,
            resolved,
            provenance,
            confidence);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::TireModelDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelTireModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 16; ++index)
            runtime->m_api.lua_pushnil(state);
        return 16;
    }

    runtime->m_api.lua_pushnumber(state, value.nominalLoad);
    runtime->m_api.lua_pushnumber(state, value.peakFriction);
    runtime->m_api.lua_pushnumber(state, value.longitudinalStiffness);
    runtime->m_api.lua_pushnumber(state, value.corneringStiffness);
    runtime->m_api.lua_pushnumber(state, value.loadSensitivity);
    runtime->m_api.lua_pushnumber(state, value.longitudinalRelaxationLength);
    runtime->m_api.lua_pushnumber(state, value.lateralRelaxationLength);
    runtime->m_api.lua_pushnumber(state, value.wheelInertia);
    runtime->m_api.lua_pushnumber(state, value.pneumaticTrail);
    runtime->m_api.lua_pushnumber(state, value.stiffnessLoadExponent);
    runtime->m_api.lua_pushnumber(state, value.longitudinalShapeFactor);
    runtime->m_api.lua_pushnumber(state, value.lateralShapeFactor);
    runtime->m_api.lua_pushnumber(state, value.longitudinalCurvatureFactor);
    runtime->m_api.lua_pushnumber(state, value.lateralCurvatureFactor);
    runtime->m_api.lua_pushnumber(state, value.combinedSlipExponent);
    runtime->m_api.lua_pushnumber(state, value.pneumaticTrailFalloff);
    return 16;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelTireParameterInfo(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);

    heritage::vehicles::TireModelDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelTireModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 8; ++index)
            runtime->m_api.lua_pushnil(state);
        return 8;
    }

    runtime->m_api.lua_pushboolean(state, value.importedPropertyFile ? 1 : 0);
    runtime->m_api.lua_pushinteger(state, value.importedFitType);
    runtime->m_api.lua_pushlstring(
        state, value.parameterSource.c_str(), value.parameterSource.size());
    runtime->m_api.lua_pushlstring(
        state, value.parameterProvenance.c_str(), value.parameterProvenance.size());
    runtime->m_api.lua_pushnumber(state, value.parameterConfidence);
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.importedMappedParameterCount));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.importedUnsupportedParameterCount));
    runtime->m_api.lua_pushlstring(
        state, value.parameterTireSide.c_str(), value.parameterTireSide.size());
    return 8;
}

int LuaVehicleBindingHandlers::luaVehicleSetTireColdInflationPressure(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setTireColdInflationPressure(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::numberArgument(*runtime, state, 2, 220000.0));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetTireColdInflationPressureRange(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::VehicleScalar minimumPa = 0.0;
    heritage::vehicles::VehicleScalar maximumPa = 0.0;
    heritage::vehicles::VehicleScalar representativePressurePa = 0.0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().tireColdInflationPressureRange(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            minimumPa, maximumPa, representativePressurePa);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }
    runtime->m_api.lua_pushnumber(state, minimumPa);
    runtime->m_api.lua_pushnumber(state, maximumPa);
    runtime->m_api.lua_pushnumber(state, representativePressurePa);
    return 3;
}

int LuaVehicleBindingHandlers::luaVehicleResetTirePhysicalState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().resetTirePhysicalState(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetSurfacePreset(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger rawSurface = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const bool result = converted
        && runtime->m_physics
        && runtime->m_physics->vehicles().setSurfacePreset(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<heritage::vehicles::TireSurface>(
                static_cast<int>(rawSurface)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetSurfacePreset(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->vehicles().surfacePreset(
                LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

} // namespace heritage::modules
