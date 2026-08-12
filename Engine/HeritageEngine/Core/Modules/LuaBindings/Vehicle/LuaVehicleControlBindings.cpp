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

int LuaVehicleBindingHandlers::luaVehicleSetInputs(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setInputs(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetWheelBrakeFactors(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelBrakeFactors(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndex,
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetDriverAids(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setDriverAids(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true),
            LuaModuleRuntime::booleanArgument(*runtime, state, 3, true),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.16)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.12)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 2.5)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 18.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 3500.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetDriverAidState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::DriverAidState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().driverAidState(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 8; ++index)
            runtime->m_api.lua_pushnil(state);
        return 8;
    }

    runtime->m_api.lua_pushboolean(
        state, value.antiLockBrakesEnabled ? 1 : 0);
    runtime->m_api.lua_pushboolean(
        state, value.tractionControlEnabled ? 1 : 0);
    runtime->m_api.lua_pushinteger(
        state, value.antiLockActiveWheelCount);
    runtime->m_api.lua_pushinteger(
        state, value.tractionControlActiveWheelCount);
    runtime->m_api.lua_pushnumber(state, value.antiLockTargetSlip);
    runtime->m_api.lua_pushnumber(state, value.tractionControlTargetSlip);
    runtime->m_api.lua_pushnumber(state, value.minimumActivationSpeed);
    runtime->m_api.lua_pushnumber(state, value.handbrakeInput);
    return 8;
}

int LuaVehicleBindingHandlers::luaVehicleSetTuning(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setTuning(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 7000.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 12000.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 38.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.15)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 11000.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 90.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetHighRateHertz(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setHighRateHertz(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1000.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetSteeringGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setSteeringGeometry(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 260.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 360.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.35)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 40.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetSteeringState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::SteeringState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().steeringState(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 8; ++index)
            runtime->m_api.lua_pushnil(state);
        return 8;
    }

    runtime->m_api.lua_pushnumber(state, value.input);
    runtime->m_api.lua_pushnumber(state, value.targetCenterAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.currentCenterAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.innerWheelAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.outerWheelAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.detectedWheelbase);
    runtime->m_api.lua_pushnumber(state, value.detectedSteerTrack);
    runtime->m_api.lua_pushnumber(state, value.currentRateFactor);
    return 8;
}

int LuaVehicleBindingHandlers::luaVehicleGetHighRateHertz(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushnumber(state, runtime->m_physics
        ? runtime->m_physics->vehicles().highRateHertz(LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1)) : 0.0);
    return 1;
}

} // namespace heritage::modules
