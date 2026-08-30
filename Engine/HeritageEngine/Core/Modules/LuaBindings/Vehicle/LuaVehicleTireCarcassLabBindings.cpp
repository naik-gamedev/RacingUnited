#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"

#include "../../../../Physics/PhysicsWorld.hpp"
#include "../../../../Vehicles/Tires/TireCarcassDevelopmentLab.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace heritage::modules {
namespace {

std::size_t oneBasedIndex(LuaApi& api, lua_State* state, int argument)
{
    int converted = 0;
    const LuaInteger value = api.lua_tointegerx(state, argument, &converted);
    return converted && value >= 1
        ? static_cast<std::size_t>(value - 1)
        : static_cast<std::size_t>(-1);
}

std::uint64_t unsignedIntegerArgument(
    LuaApi& api, lua_State* state, int argument,
    std::uint64_t fallback)
{
    int converted = 0;
    const LuaInteger value = api.lua_tointegerx(
        state, argument, &converted);
    return converted && value >= 0
        ? static_cast<std::uint64_t>(value) : fallback;
}

void pushNumberArray(
    LuaApi& api, lua_State* state, const char* field,
    const heritage::vehicles::VehicleScalar* values, std::size_t count)
{
    api.lua_createtable(state, static_cast<int>(count), 0);
    for (std::size_t index = 0; index < count; ++index)
    {
        api.lua_pushnumber(state, values[index]);
        api.lua_rawseti(state, -2, static_cast<LuaInteger>(index + 1));
    }
    api.lua_setfield(state, -2, field);
}

void pushSyntheticResult(
    LuaApi& api, lua_State* state,
    const heritage::vehicles::tires::TireCarcassSyntheticResult& result)
{
    api.lua_createtable(state, 0, 25);
    const auto number = [&](const char* field, double value) {
        api.lua_pushnumber(state, value);
        api.lua_setfield(state, -2, field);
    };
    const auto integer = [&](const char* field, LuaInteger value) {
        api.lua_pushinteger(state, value);
        api.lua_setfield(state, -2, field);
    };
    api.lua_pushboolean(state, result.valid ? 1 : 0);
    api.lua_setfield(state, -2, "valid");
    api.lua_pushlstring(state, result.scenario.c_str(), result.scenario.size());
    api.lua_setfield(state, -2, "scenario");
    integer("integration_steps", static_cast<LuaInteger>(result.integrationSteps));
    number("pathology_score", result.pathologyScore);
    number("road_penetration_mm", result.roadPenetrationMm);
    number("rim_penetration_mm", result.rimPenetrationMm);
    number("lower_hook_mm", result.lowerHookMm);
    number("static_asymmetry_mm", result.staticAsymmetryMm);
    number("footprint_height_range_mm", result.footprintHeightRangeMm);
    number("maximum_displacement_mm", result.maximumDisplacementMm);
    number("rms_velocity_mps", result.rmsVelocityMps);
    number("center_bottom_height_mm", result.centerBottomHeightMm);
    number("front_bottom_height_mm", result.frontBottomHeightMm);
    number("rear_bottom_height_mm", result.rearBottomHeightMm);
    number("center_forward_displacement_mm", result.centerForwardDisplacementMm);
    number("center_down_displacement_mm", result.centerDownDisplacementMm);
    number("center_lateral_displacement_mm", result.centerLateralDisplacementMm);
    number("center_radial_displacement_mm", result.centerRadialDisplacementMm);
    number("center_tangential_displacement_mm", result.centerTangentialDisplacementMm);
    pushNumberArray(api, state, "radial_profile_mm",
        result.radialProfileMm.data(), result.radialProfileMm.size());
    pushNumberArray(api, state, "bottom_cross_section_mm",
        result.bottomCrossSectionMm.data(), result.bottomCrossSectionMm.size());
}

bool scenarioFromString(
    const std::string& name,
    heritage::vehicles::tires::TireCarcassSyntheticScenario& scenario)
{
    return heritage::vehicles::tires::tireCarcassSyntheticScenarioFromName(
        name, scenario);
}

} // namespace

int LuaVehicleBindingHandlers::luaVehicleGetTireCarcassLabParameterCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state,
        static_cast<LuaInteger>(heritage::vehicles::tires::tireCarcassDevelopmentParameterCount()));
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetTireCarcassLabParameterInfo(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::tires::TireCarcassDevelopmentParameterInfo info;
    if (!heritage::vehicles::tires::tireCarcassDevelopmentParameterInfo(
            oneBasedIndex(runtime->m_api, state, 1), info))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_createtable(state, 0, 9);
    const auto setString = [&](const char* field, const std::string& value) {
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
        runtime->m_api.lua_setfield(state, -2, field);
    };
    const auto setNumber = [&](const char* field, double value) {
        runtime->m_api.lua_pushnumber(state, value);
        runtime->m_api.lua_setfield(state, -2, field);
    };
    setString("key", info.key);
    setString("label", info.label);
    setString("group", info.group);
    runtime->m_api.lua_pushinteger(state,
        static_cast<LuaInteger>(info.groupIndex + 1));
    runtime->m_api.lua_setfield(state, -2, "group_index");
    setNumber("minimum", info.minimum);
    setNumber("maximum", info.maximum);
    setNumber("default", info.defaultValue);
    runtime->m_api.lua_pushboolean(state, info.integer ? 1 : 0);
    runtime->m_api.lua_setfield(state, -2, "integer");
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetTireCarcassLabParameter(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics) return 0;
    heritage::vehicles::VehicleScalar value = 0.0;
    if (!runtime->m_physics->vehicles().tireCarcassDevelopmentParameter(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2),
            oneBasedIndex(runtime->m_api, state, 3), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_pushnumber(state, value);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetTireCarcassLabParameter(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setTireCarcassDevelopmentParameter(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2),
            oneBasedIndex(runtime->m_api, state, 3),
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetTireCarcassLabEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics) return 0;
    bool enabled = false;
    if (!runtime->m_physics->vehicles().tireCarcassDevelopmentEnabled(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2), enabled))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_pushboolean(state, enabled ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetTireCarcassLabEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setTireCarcassDevelopmentEnabled(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2),
            LuaModuleRuntime::booleanArgument(*runtime, state, 3, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleResetTireCarcassLab(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().resetTireCarcassDevelopmentTuning(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleResetTireCarcassLabState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().resetTireCarcassDevelopmentState(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleCopyTireCarcassLabToAllWheels(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().copyTireCarcassDevelopmentTuningToAllWheels(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleRunTireCarcassSyntheticScenario(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics) return 0;
    heritage::vehicles::tires::TireCarcassSyntheticScenario scenario;
    if (!scenarioFromString(LuaModuleRuntime::stringArgument(*runtime, state, 3, "static_flat"), scenario))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    const std::size_t steps = static_cast<std::size_t>(std::clamp(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 48.0), 4.0, 240.0));
    heritage::vehicles::tires::TireCarcassSyntheticResult result;
    if (!runtime->m_physics->vehicles().runTireCarcassSyntheticScenario(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2), scenario, steps, result))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    pushSyntheticResult(runtime->m_api, state, result);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleRunTireCarcassSearchBatch(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics) return 0;
    heritage::vehicles::tires::TireCarcassSyntheticScenario scenario;
    if (!scenarioFromString(LuaModuleRuntime::stringArgument(*runtime, state, 3, "static_flat"), scenario))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    const std::uint64_t seed = unsignedIntegerArgument(runtime->m_api, state, 4, 1);
    const std::uint64_t firstTrial = unsignedIntegerArgument(runtime->m_api, state, 5, 0);
    const std::size_t count = static_cast<std::size_t>(std::clamp(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 1.0), 1.0, 256.0));
    const auto spread = static_cast<heritage::vehicles::VehicleScalar>(std::clamp(
        LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.20), 0.0, 1.0));
    const std::uint32_t groupMask = static_cast<std::uint32_t>(
        unsignedIntegerArgument(runtime->m_api, state, 8, 0xffffffffULL));
    const std::size_t steps = static_cast<std::size_t>(std::clamp(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 32.0), 4.0, 240.0));
    heritage::vehicles::tires::TireCarcassSearchBatchResult result;
    if (!runtime->m_physics->vehicles().runTireCarcassSearchBatch(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2), scenario,
            seed, firstTrial, count, spread, groupMask, steps, result))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_createtable(state, 0, 6);
    runtime->m_api.lua_pushinteger(state,
        static_cast<LuaInteger>(result.bestTrialIndex));
    runtime->m_api.lua_setfield(state, -2, "best_trial");
    runtime->m_api.lua_pushnumber(state, result.bestScore);
    runtime->m_api.lua_setfield(state, -2, "best_score");
    runtime->m_api.lua_pushinteger(state,
        static_cast<LuaInteger>(result.evaluatedCount));
    runtime->m_api.lua_setfield(state, -2, "evaluated_count");
    runtime->m_api.lua_pushnumber(state, result.elapsedSeconds);
    runtime->m_api.lua_setfield(state, -2, "elapsed_seconds");
    pushSyntheticResult(runtime->m_api, state, result.bestScenario);
    runtime->m_api.lua_setfield(state, -2, "result");
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleApplyTireCarcassSearchTrial(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().applyTireCarcassSearchTrial(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            oneBasedIndex(runtime->m_api, state, 2),
            unsignedIntegerArgument(runtime->m_api, state, 3, 1),
            unsignedIntegerArgument(runtime->m_api, state, 4, 0),
            static_cast<heritage::vehicles::VehicleScalar>(std::clamp(
                LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.20), 0.0, 1.0)),
            static_cast<std::uint32_t>(
                unsignedIntegerArgument(runtime->m_api, state, 6, 0xffffffffULL)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

} // namespace heritage::modules
