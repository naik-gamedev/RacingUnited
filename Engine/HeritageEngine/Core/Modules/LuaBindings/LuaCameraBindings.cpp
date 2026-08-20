#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"
#include "../../../Camera/VehicleCameraController.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaCoreBindingHandlers::luaCameraIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushboolean(state, runtime->m_vehicleCamera ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraSetVehicleViewActive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    const bool active = LuaModuleRuntime::booleanArgument(*runtime, state, 1, false);
    runtime->m_vehicleCamera->setActive(active);
    if (!active)
        runtime->m_vehicleCamera->setFlyEnabled(false);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraIsVehicleViewActive(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    runtime->m_api.lua_pushboolean(
        state, runtime->m_vehicleCamera->active() ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraSetVehiclePose(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;

    heritage::camera::VehicleCameraPose pose = runtime->m_vehicleCamera->pose();
    pose.positionMeters.x = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, pose.positionMeters.x));
    pose.positionMeters.y = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, pose.positionMeters.y));
    pose.positionMeters.z = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, pose.positionMeters.z));
    pose.pitchDegrees = LuaModuleRuntime::numberArgument(
        *runtime, state, 4, pose.pitchDegrees);
    pose.yawDegrees = LuaModuleRuntime::numberArgument(
        *runtime, state, 5, pose.yawDegrees);
    pose.rollDegrees = LuaModuleRuntime::numberArgument(
        *runtime, state, 6, pose.rollDegrees);
    runtime->m_vehicleCamera->setPose(pose);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraGetVehiclePose(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    const heritage::camera::VehicleCameraPose& pose = runtime->m_vehicleCamera->pose();
    runtime->m_api.lua_pushnumber(state, pose.positionMeters.x);
    runtime->m_api.lua_pushnumber(state, pose.positionMeters.y);
    runtime->m_api.lua_pushnumber(state, pose.positionMeters.z);
    runtime->m_api.lua_pushnumber(state, pose.pitchDegrees);
    runtime->m_api.lua_pushnumber(state, pose.yawDegrees);
    runtime->m_api.lua_pushnumber(state, pose.rollDegrees);
    return 6;
}

int LuaCoreBindingHandlers::luaCameraSetFlyEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    runtime->m_vehicleCamera->setFlyEnabled(
        LuaModuleRuntime::booleanArgument(*runtime, state, 1, false));
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraIsFlyEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    runtime->m_api.lua_pushboolean(
        state, runtime->m_vehicleCamera->flyEnabled() ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaCameraSetFlySpeed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    runtime->m_vehicleCamera->setFlySpeedMetersPerSecond(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, 1.5));
    runtime->m_api.lua_pushnumber(
        state, runtime->m_vehicleCamera->flySpeedMetersPerSecond());
    return 1;
}

int LuaCoreBindingHandlers::luaCameraGetFlySpeed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleCamera)
        return 0;
    runtime->m_api.lua_pushnumber(
        state, runtime->m_vehicleCamera->flySpeedMetersPerSecond());
    return 1;
}

void LuaModuleRuntime::registerCameraBindings()
{
    registerFunction("Camera", "IsAvailable", &LuaCoreBindingHandlers::luaCameraIsAvailable);
    registerFunction("Camera", "SetVehicleViewActive", &LuaCoreBindingHandlers::luaCameraSetVehicleViewActive);
    registerFunction("Camera", "IsVehicleViewActive", &LuaCoreBindingHandlers::luaCameraIsVehicleViewActive);
    registerFunction("Camera", "SetVehiclePose", &LuaCoreBindingHandlers::luaCameraSetVehiclePose);
    registerFunction("Camera", "GetVehiclePose", &LuaCoreBindingHandlers::luaCameraGetVehiclePose);
    registerFunction("Camera", "SetFlyEnabled", &LuaCoreBindingHandlers::luaCameraSetFlyEnabled);
    registerFunction("Camera", "IsFlyEnabled", &LuaCoreBindingHandlers::luaCameraIsFlyEnabled);
    registerFunction("Camera", "SetFlySpeed", &LuaCoreBindingHandlers::luaCameraSetFlySpeed);
    registerFunction("Camera", "GetFlySpeed", &LuaCoreBindingHandlers::luaCameraGetFlySpeed);
}

} // namespace heritage::modules
