#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "../../../../Vehicles/Dynamics/MassProperties/VehicleMassPropertiesEstimator.hpp"

#include <string>
#include "../../../../Physics/PhysicsWorld.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaVehicleBindingHandlers::luaVehicleEstimateMassProperties(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::VehicleMassPropertiesEstimateInput input;
    input.totalMassKg = LuaModuleRuntime::numberArgument(*runtime, state, 1, 1200.0);
    input.wheelbaseM = LuaModuleRuntime::numberArgument(*runtime, state, 2, 2.50);
    input.frontTrackM = LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.50);
    input.rearTrackM = LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.50);
    input.centerOfMassHeightM = LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.50);
    input.frontStaticLoadFraction = LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.50);
    input.leftStaticLoadFraction = LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.50);

    const std::string massClassId = LuaModuleRuntime::stringArgument(*runtime, state, 8);
    if (!heritage::vehicles::parseVehicleMassClass(
            massClassId.empty() ? std::string("unknown") : massClassId,
            input.massClass))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "Unknown vehicle mass-property class.");
        return 2;
    }

    const heritage::vehicles::VehicleMassPropertiesEstimate estimate =
        heritage::vehicles::estimateVehicleMassProperties(input);
    if (!estimate.valid)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(
            state, "Could not derive valid vehicle mass properties.");
        return 2;
    }

    runtime->m_api.lua_createtable(state, 0, 12);
    runtime->m_api.lua_pushnumber(state, estimate.totalMassKg);
    runtime->m_api.lua_setfield(state, -2, "totalMassKg");

    runtime->m_api.lua_createtable(state, 3, 0);
    runtime->m_api.lua_pushnumber(state, estimate.centerOfMassLocal.x);
    runtime->m_api.lua_rawseti(state, -2, 1);
    runtime->m_api.lua_pushnumber(state, estimate.centerOfMassLocal.y);
    runtime->m_api.lua_rawseti(state, -2, 2);
    runtime->m_api.lua_pushnumber(state, estimate.centerOfMassLocal.z);
    runtime->m_api.lua_rawseti(state, -2, 3);
    runtime->m_api.lua_setfield(state, -2, "centerOfMassLocal");

    runtime->m_api.lua_createtable(state, 3, 0);
    runtime->m_api.lua_pushnumber(state, estimate.inertiaLocalKgM2.x);
    runtime->m_api.lua_rawseti(state, -2, 1);
    runtime->m_api.lua_pushnumber(state, estimate.inertiaLocalKgM2.y);
    runtime->m_api.lua_rawseti(state, -2, 2);
    runtime->m_api.lua_pushnumber(state, estimate.inertiaLocalKgM2.z);
    runtime->m_api.lua_rawseti(state, -2, 3);
    runtime->m_api.lua_setfield(state, -2, "inertiaLocalKgM2");

    runtime->m_api.lua_pushnumber(state, estimate.frontStaticMassKg);
    runtime->m_api.lua_setfield(state, -2, "frontStaticMassKg");
    runtime->m_api.lua_pushnumber(state, estimate.rearStaticMassKg);
    runtime->m_api.lua_setfield(state, -2, "rearStaticMassKg");
    runtime->m_api.lua_pushnumber(state, estimate.leftStaticMassKg);
    runtime->m_api.lua_setfield(state, -2, "leftStaticMassKg");
    runtime->m_api.lua_pushnumber(state, estimate.rightStaticMassKg);
    runtime->m_api.lua_setfield(state, -2, "rightStaticMassKg");
    runtime->m_api.lua_pushlstring(
        state, estimate.provenance.c_str(), estimate.provenance.size());
    runtime->m_api.lua_setfield(state, -2, "provenance");
    runtime->m_api.lua_pushnumber(state, estimate.confidence);
    runtime->m_api.lua_setfield(state, -2, "confidence");
    runtime->m_api.lua_pushnil(state);
    return 2;
}

} // namespace heritage::modules
