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

int LuaVehicleBindingHandlers::luaVehicleSetPowertrain(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setPowertrain(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 900.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 7000.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 250.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 70.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 3.90)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.88)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.22)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 5.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetGearRatios(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    std::vector<float> forwardRatios;
    const int argumentCount = runtime->m_api.lua_gettop(state);
    for (int argument = 3; argument <= argumentCount; ++argument)
    {
        forwardRatios.push_back(static_cast<float>(
            LuaModuleRuntime::numberArgument(*runtime, state, argument, 0.0)));
    }
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setGearRatios(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, -3.20)),
            forwardRatios);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetDifferential(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    int converted = 0;
    const LuaInteger rawMode = runtime->m_api.lua_tointegerx(
        state,
        2,
        &converted);
    const int modeValue = converted ? static_cast<int>(rawMode) : 1;
    heritage::vehicles::DifferentialMode mode =
        heritage::vehicles::DifferentialMode::LimitedSlip;
    if (modeValue <= 0)
        mode = heritage::vehicles::DifferentialMode::Open;
    else if (modeValue >= 2)
        mode = heritage::vehicles::DifferentialMode::Locked;

    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setDifferential(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            mode,
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 2.25)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetGear(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger gear = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const bool result = converted
        && runtime->m_physics
        && runtime->m_physics->vehicles().setGear(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            static_cast<int>(gear));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleShiftUp(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().shiftUp(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleShiftDown(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().shiftDown(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetDrivetrainState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::DrivetrainState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().drivetrainState(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 14; ++index)
            runtime->m_api.lua_pushnil(state);
        return 14;
    }

    runtime->m_api.lua_pushinteger(state, value.currentGear);
    runtime->m_api.lua_pushinteger(state, value.requestedGear);
    runtime->m_api.lua_pushboolean(state, value.shifting ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.shiftTimeRemaining);
    runtime->m_api.lua_pushnumber(state, value.engineRpm);
    runtime->m_api.lua_pushnumber(state, value.engineTorque);
    runtime->m_api.lua_pushnumber(state, value.clutchEngagement);
    runtime->m_api.lua_pushnumber(state, value.clutchSlipRpm);
    runtime->m_api.lua_pushnumber(state, value.wheelCoupledRpm);
    runtime->m_api.lua_pushnumber(state, value.selectedGearRatio);
    runtime->m_api.lua_pushnumber(state, value.finalDriveRatio);
    runtime->m_api.lua_pushnumber(state, value.outputTorque);
    runtime->m_api.lua_pushnumber(state, value.drivenWheelSpeedDifferenceRpm);
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(value.differentialMode));
    return 14;
}

int LuaVehicleBindingHandlers::luaVehicleGetForwardGearCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->vehicles().forwardGearCount(
                    LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

} // namespace heritage::modules
