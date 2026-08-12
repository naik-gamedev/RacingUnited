#pragma once

#include "../../LuaApi.hpp"
#include "../../../../Vehicles/VehicleDefinition.hpp"

#include <string>

namespace heritage::modules::lua_binding_detail {

bool parseLuaVehicleDefinitionV2(
    const LuaApi& api,
    lua_State* state,
    int definitionIndex,
    heritage::vehicles::VehicleDefinitionV2Source& source,
    std::string& errorMessage);

} // namespace heritage::modules::lua_binding_detail
