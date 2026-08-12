#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "../../../../Physics/PhysicsWorld.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

namespace {

std::size_t wheelIndexArgument(
    LuaApi& api,
    lua_State* state,
    int argumentIndex)
{
    int converted = 0;
    const LuaInteger luaIndex = api.lua_tointegerx(
        state, argumentIndex, &converted);
    return converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
}

void pushVec3Array(
    LuaApi& api,
    lua_State* state,
    const heritage::math::Vec3& value)
{
    api.lua_createtable(state, 3, 0);
    api.lua_pushnumber(state, value.x);
    api.lua_rawseti(state, -2, 1);
    api.lua_pushnumber(state, value.y);
    api.lua_rawseti(state, -2, 2);
    api.lua_pushnumber(state, value.z);
    api.lua_rawseti(state, -2, 3);
}

} // namespace

int LuaVehicleBindingHandlers::luaVehicleSetWheelFitment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelFitmentDescription value;
    value.enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 3, true);
    value.referenceOffsetEtMm = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0));
    value.installedOffsetEtMm = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, value.referenceOffsetEtMm));
    value.spacerThicknessMm = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0));
    value.rimDiameterIn = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 7, 17.0));
    value.rimWidthIn = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 8, 7.0));
    value.tireWidthMm = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 205.0));
    value.tireAspectRatio = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 10, 40.0));
    value.tireRimDiameterIn = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 11, value.rimDiameterIn));

    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelFitment(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndexArgument(runtime->m_api, state, 2),
            value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelFitment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelFitmentDescription value;
    heritage::vehicles::WheelFitmentResolved resolved;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelFitment(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndexArgument(runtime->m_api, state, 2),
            value,
            resolved);
    if (!result)
    {
        for (int index = 0; index < 11; ++index)
            runtime->m_api.lua_pushnil(state);
        return 11;
    }

    runtime->m_api.lua_pushboolean(state, value.enabled ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.referenceOffsetEtMm);
    runtime->m_api.lua_pushnumber(state, value.installedOffsetEtMm);
    runtime->m_api.lua_pushnumber(state, value.spacerThicknessMm);
    runtime->m_api.lua_pushnumber(state, value.rimDiameterIn);
    runtime->m_api.lua_pushnumber(state, value.rimWidthIn);
    runtime->m_api.lua_pushnumber(state, value.tireWidthMm);
    runtime->m_api.lua_pushnumber(state, value.tireAspectRatio);
    runtime->m_api.lua_pushnumber(state, value.tireRimDiameterIn);
    runtime->m_api.lua_pushnumber(state, resolved.nominalTireRadiusM);
    runtime->m_api.lua_pushnumber(state, resolved.outwardCenterlineDeltaM);
    return 11;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelFitmentGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelFitmentGeometryState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelFitmentGeometry(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndexArgument(runtime->m_api, state, 2),
            value);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_createtable(state, 0, 15);
    runtime->m_api.lua_pushboolean(state, value.hubReferenceValid ? 1 : 0);
    runtime->m_api.lua_setfield(state, -2, "hubReferenceValid");
    pushVec3Array(runtime->m_api, state, value.referenceWheelCenterLocal);
    runtime->m_api.lua_setfield(state, -2, "referenceWheelCenterLocal");
    pushVec3Array(runtime->m_api, state, value.referenceHubFaceCenterLocal);
    runtime->m_api.lua_setfield(state, -2, "referenceHubFaceCenterLocal");
    pushVec3Array(runtime->m_api, state, value.installedMountFaceCenterLocal);
    runtime->m_api.lua_setfield(state, -2, "installedMountFaceCenterLocal");
    pushVec3Array(runtime->m_api, state, value.installedWheelCenterLocal);
    runtime->m_api.lua_setfield(state, -2, "installedWheelCenterLocal");
    pushVec3Array(runtime->m_api, state, value.installedInnerTirePlaneLocal);
    runtime->m_api.lua_setfield(state, -2, "installedInnerTirePlaneLocal");
    pushVec3Array(runtime->m_api, state, value.installedOuterTirePlaneLocal);
    runtime->m_api.lua_setfield(state, -2, "installedOuterTirePlaneLocal");
    runtime->m_api.lua_pushnumber(
        state, value.inboardTireExtensionFromReferenceHubM * 1000.0);
    runtime->m_api.lua_setfield(state, -2, "inboardTireExtensionFromReferenceHubMm");
    runtime->m_api.lua_pushnumber(
        state, value.outboardTireExtensionFromReferenceHubM * 1000.0);
    runtime->m_api.lua_setfield(state, -2, "outboardTireExtensionFromReferenceHubMm");
    runtime->m_api.lua_pushboolean(
        state, value.steeringGroundGeometryValid ? 1 : 0);
    runtime->m_api.lua_setfield(state, -2, "steeringGroundGeometryValid");
    pushVec3Array(runtime->m_api, state, value.worldSteeringAxisPoint);
    runtime->m_api.lua_setfield(state, -2, "worldSteeringAxisPoint");
    pushVec3Array(runtime->m_api, state, value.steeringAxisGroundPointWorld);
    runtime->m_api.lua_setfield(state, -2, "steeringAxisGroundPointWorld");
    runtime->m_api.lua_pushnumber(state, value.signedScrubRadiusM * 1000.0);
    runtime->m_api.lua_setfield(state, -2, "signedScrubRadiusMm");
    runtime->m_api.lua_pushnumber(state, value.scrubRadiusMagnitudeM * 1000.0);
    runtime->m_api.lua_setfield(state, -2, "scrubRadiusMagnitudeMm");
    runtime->m_api.lua_pushnumber(state, value.mechanicalTrailM * 1000.0);
    runtime->m_api.lua_setfield(state, -2, "mechanicalTrailMm");
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetWheelAlignment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelAlignmentSetup value;
    value.camberDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0));
    value.toeDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0));
    value.casterOverrideEnabled = LuaModuleRuntime::booleanArgument(*runtime, state, 5, false);
    value.casterDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0));

    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelAlignment(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndexArgument(runtime->m_api, state, 2),
            value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelAlignment(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelAlignmentSetup value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelAlignment(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            wheelIndexArgument(runtime->m_api, state, 2),
            value);
    if (!result)
    {
        for (int index = 0; index < 4; ++index)
            runtime->m_api.lua_pushnil(state);
        return 4;
    }

    runtime->m_api.lua_pushnumber(state, value.camberDegrees);
    runtime->m_api.lua_pushnumber(state, value.toeDegrees);
    runtime->m_api.lua_pushboolean(state, value.casterOverrideEnabled ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.casterDegrees);
    return 4;
}

} // namespace heritage::modules
