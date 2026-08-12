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

int LuaVehicleBindingHandlers::luaVehicleStartDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().startDynamicsLabCapture(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 20.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1000.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleStopDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().stopDynamicsLabCapture(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleClearDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().clearDynamicsLabCapture(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetDynamicsLabSummary(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::DynamicsLabSummary value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().dynamicsLabSummary(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 25; ++index)
            runtime->m_api.lua_pushnil(state);
        return 25;
    }

    runtime->m_api.lua_pushboolean(state, value.recording ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, value.captureComplete ? 1 : 0);
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.sampleCount));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.sampleCapacity));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.wheelCount));
    runtime->m_api.lua_pushnumber(state, value.durationSeconds);
    runtime->m_api.lua_pushnumber(state, value.requestedCaptureHertz);
    runtime->m_api.lua_pushnumber(state, value.peakSpeedKph);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteRollRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsolutePitchRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteYawRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteSuspensionVelocityMps);
    runtime->m_api.lua_pushnumber(state, value.peakAbsoluteSlipRatio);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteSlipAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.peakGripUtilizationPercent);
    runtime->m_api.lua_pushnumber(
        state, value.minimumGroundedNormalForceNewtons);
    runtime->m_api.lua_pushnumber(
        state, value.maximumGroundedNormalForceNewtons);
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.groundContactLossEvents));
    runtime->m_api.lua_pushnumber(
        state, value.peakSuspensionTravelStopForceNewtons);
    runtime->m_api.lua_pushnumber(
        state, value.peakDamperDissipationWatts);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteUnsprungVelocityMps);
    runtime->m_api.lua_pushnumber(
        state, value.peakTireDeflectionMillimeters);
    runtime->m_api.lua_pushnumber(
        state, value.peakTireRadialDissipationWatts);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteCamberDegrees);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteToeDegrees);
    return 25;
}

int LuaVehicleBindingHandlers::luaVehicleGetDynamicsLabSeries(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;

    heritage::vehicles::DynamicsLabMetric metric;
    if (!heritage::vehicles::parseDynamicsLabMetric(
            LuaModuleRuntime::stringArgument(*runtime, state, 2),
            metric))
    {
        return 0;
    }

    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 3, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : 0u;
    const std::size_t maximumPoints = static_cast<std::size_t>(
        (std::max)(16.0, (std::min)(240.0,
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 180.0))));

    std::vector<float> values;
    if (!runtime->m_physics->vehicles().dynamicsLabMetricSeries(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            metric,
            wheelIndex,
            maximumPoints,
            values))
    {
        return 0;
    }
    if (!runtime->m_api.lua_checkstack(
            state,
            static_cast<int>(values.size())))
    {
        return 0;
    }
    for (const float value : values)
        runtime->m_api.lua_pushnumber(state, value);
    return static_cast<int>(values.size());
}

int LuaVehicleBindingHandlers::luaVehicleExportDynamicsLabCsv(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string requestedName = LuaModuleRuntime::stringArgument(
        *runtime, state, 2, "latest_vehicle_dynamics.csv");
    const std::filesystem::path name(requestedName);
    const bool safeName = !requestedName.empty()
        && !name.has_parent_path()
        && name.filename() == name
        && name.extension() == ".csv";
    if (!runtime->m_physics || !runtime->m_context || !safeName)
    {
        const std::string message = !safeName
            ? "Dynamics lab export name must be a plain .csv filename."
            : "Dynamics lab export requires Vehicle and Module services.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::filesystem::path path = runtime->m_context->resolveSavePath(
        std::filesystem::path("DynamicsLab") / name);
    const bool result = !path.empty()
        && runtime->m_physics->vehicles().exportDynamicsLabCsv(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            path);
    const std::string message = result
        ? path.string()
        : runtime->m_physics->vehicles().lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
}

} // namespace heritage::modules
