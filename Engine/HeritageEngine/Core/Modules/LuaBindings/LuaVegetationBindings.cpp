#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../Graphics/VegetationSystem.hpp"
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

int LuaCoreBindingHandlers::luaVegetationIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushboolean(state, runtime->m_vegetation ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationReset(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_vegetation)
        runtime->m_vegetation->reset();
    runtime->m_api.lua_pushboolean(state, runtime->m_vegetation ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationRegisterSpecies(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string id = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string kindText = LuaModuleRuntime::stringArgument(*runtime, state, 2, "generic");
    const bool clusterOctahedral = LuaModuleRuntime::booleanArgument(*runtime, state, 3, false);
    const bool wholePlantOctahedral = LuaModuleRuntime::booleanArgument(*runtime, state, 4, false);
    const bool terrainCoverage = LuaModuleRuntime::booleanArgument(*runtime, state, 5, false);

    const bool result = runtime->m_vegetation
        && runtime->m_vegetation->registerSpeciesWithDefaults(
            id,
            heritage::graphics::VegetationSystem::parseKind(kindText),
            clusterOctahedral,
            wholePlantOctahedral,
            terrainCoverage);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationSetSpeciesLod(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::graphics::VegetationLodPolicy policy;
    policy.detailedEndMeters = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 50.0));
    policy.mergedClusterEndMeters = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 200.0));
    policy.wholePlantEndMeters = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 1800.0));
    policy.terrainCoverageEndMeters = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, 8000.0));

    const bool result = runtime->m_vegetation
        && runtime->m_vegetation->setSpeciesLodPolicy(
            LuaModuleRuntime::stringArgument(*runtime, state, 1), policy);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationSetSpeciesWindResponse(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_vegetation
        && runtime->m_vegetation->setSpeciesWindResponse(
            LuaModuleRuntime::stringArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.25)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.65)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationAddInstance(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const double seedNumber = LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0);
    const double clampedSeed = (std::max)(
        0.0,
        (std::min)(
            static_cast<double>(std::numeric_limits<std::uint32_t>::max()),
            seedNumber));
    const bool result = runtime->m_vegetation
        && runtime->m_vegetation->addInstance(
            LuaModuleRuntime::stringArgument(*runtime, state, 1),
            {
                LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0),
                LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0),
                LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 1.0)),
            static_cast<std::uint32_t>(clampedSeed));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationClearInstances(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_vegetation)
        runtime->m_vegetation->clearInstances();
    runtime->m_api.lua_pushboolean(state, runtime->m_vegetation ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationGetRepresentation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::graphics::VegetationRepresentation representation =
        runtime->m_vegetation
        ? runtime->m_vegetation->representationForDistance(
            LuaModuleRuntime::stringArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)))
        : heritage::graphics::VegetationRepresentation::Culled;
    const char* name =
        heritage::graphics::VegetationSystem::representationName(representation);
    runtime->m_api.lua_pushstring(state, name);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationGetStats(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::graphics::VegetationStats stats = runtime->m_vegetation
        ? runtime->m_vegetation->stats()
        : heritage::graphics::VegetationStats{};

    runtime->m_api.lua_createtable(state, 0, 6);
    auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setNumber("species_count", static_cast<double>(stats.speciesCount));
    setNumber("instance_count", static_cast<double>(stats.instanceCount));
    setNumber("occupied_chunk_count", static_cast<double>(stats.occupiedChunkCount));
    setNumber("packed_bytes", static_cast<double>(stats.packedBytes));
    setNumber("chunk_size_m", heritage::graphics::VegetationSystem::kChunkSizeMeters);
    setNumber(
        "local_quantization_mm",
        heritage::graphics::VegetationSystem::kChunkSizeMeters / 65535.0 * 1000.0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationSetWind(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_vegetation)
    {
        heritage::graphics::VegetationWindState wind;
        wind.velocityMetersPerSecond = {
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0))
        };
        wind.gust = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0));
        wind.turbulence = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0));
        runtime->m_vegetation->setWind(wind);
    }
    runtime->m_api.lua_pushboolean(state, runtime->m_vegetation ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaVegetationGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string message = runtime->m_vegetation
        ? runtime->m_vegetation->lastError()
        : "Vegetation system unavailable.";
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 1;
}

void LuaModuleRuntime::registerVegetationBindings()
{
    registerFunction("Vegetation", "IsAvailable", &LuaCoreBindingHandlers::luaVegetationIsAvailable);
    registerFunction("Vegetation", "Reset", &LuaCoreBindingHandlers::luaVegetationReset);
    registerFunction("Vegetation", "RegisterSpecies", &LuaCoreBindingHandlers::luaVegetationRegisterSpecies);
    registerFunction("Vegetation", "SetSpeciesLod", &LuaCoreBindingHandlers::luaVegetationSetSpeciesLod);
    registerFunction("Vegetation", "SetSpeciesWindResponse", &LuaCoreBindingHandlers::luaVegetationSetSpeciesWindResponse);
    registerFunction("Vegetation", "AddInstance", &LuaCoreBindingHandlers::luaVegetationAddInstance);
    registerFunction("Vegetation", "ClearInstances", &LuaCoreBindingHandlers::luaVegetationClearInstances);
    registerFunction("Vegetation", "GetRepresentation", &LuaCoreBindingHandlers::luaVegetationGetRepresentation);
    registerFunction("Vegetation", "GetStats", &LuaCoreBindingHandlers::luaVegetationGetStats);
    registerFunction("Vegetation", "SetWind", &LuaCoreBindingHandlers::luaVegetationSetWind);
    registerFunction("Vegetation", "GetLastError", &LuaCoreBindingHandlers::luaVegetationGetLastError);
}

} // namespace heritage::modules
