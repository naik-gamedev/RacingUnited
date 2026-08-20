#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"

#include "../../../../Physics/PhysicsWorld.hpp"
#include "../../../../Vehicles/Tires/TireScenarioLab.hpp"
#include "../../../../Vehicles/Tires/TireFleetBenchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace heritage::modules {
namespace {

using heritage::vehicles::TireScenarioResult;
using heritage::vehicles::TireScenarioSample;

std::size_t scenarioWheelIndex(
    LuaApi& api, lua_State* state, int argument)
{
    int converted = 0;
    const LuaInteger value = api.lua_tointegerx(state, argument, &converted);
    return converted && value >= 1
        ? static_cast<std::size_t>(value - 1)
        : static_cast<std::size_t>(-1);
}

std::vector<const TireScenarioSample*> sampledScenario(
    const TireScenarioResult& result,
    std::size_t maximumPoints)
{
    const std::size_t outputCount = (std::min)(
        result.samples.size(), maximumPoints);
    std::vector<const TireScenarioSample*> samples;
    samples.reserve(outputCount);
    for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        const std::size_t sourceIndex = outputCount <= 1
            ? 0
            : static_cast<std::size_t>(std::llround(
                static_cast<double>(outputIndex)
                * static_cast<double>(result.samples.size() - 1)
                / static_cast<double>(outputCount - 1)));
        samples.push_back(&result.samples[sourceIndex]);
    }
    return samples;
}

void pushScenarioNumberArray(
    LuaApi& api,
    lua_State* state,
    const char* field,
    const std::vector<const TireScenarioSample*>& samples,
    heritage::vehicles::VehicleScalar (*value)(const TireScenarioSample&))
{
    api.lua_createtable(state, static_cast<int>(samples.size()), 0);
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        api.lua_pushnumber(state, value(*samples[index]));
        api.lua_rawseti(state, -2, static_cast<LuaInteger>(index + 1));
    }
    api.lua_setfield(state, -2, field);
}

void pushScenarioTable(
    LuaApi& api,
    lua_State* state,
    const TireScenarioResult& result,
    std::size_t maximumPoints)
{
    const auto samples = sampledScenario(result, maximumPoints);
    api.lua_createtable(state, 0, 24);
    const auto number = [&](const char* field, LuaNumber value) {
        api.lua_pushnumber(state, value);
        api.lua_setfield(state, -2, field);
    };
    const auto integer = [&](const char* field, LuaInteger value) {
        api.lua_pushinteger(state, value);
        api.lua_setfield(state, -2, field);
    };
    const auto string = [&](const char* field, const std::string& value) {
        api.lua_pushlstring(state, value.c_str(), value.size());
        api.lua_setfield(state, -2, field);
    };
    string("name", result.name);
    number("integration_step_s", result.integrationStepSeconds);
    number("sample_interval_s", result.sampleIntervalSeconds);
    integer("native_sample_count", static_cast<LuaInteger>(result.samples.size()));
    integer("sample_count", static_cast<LuaInteger>(samples.size()));

    pushScenarioNumberArray(api, state, "time_s", samples,
        [](const TireScenarioSample& sample) { return sample.timeSeconds; });
    pushScenarioNumberArray(api, state, "target_kappa", samples,
        [](const TireScenarioSample& sample) { return sample.targetLongitudinalSlip; });
    pushScenarioNumberArray(api, state, "effective_kappa", samples,
        [](const TireScenarioSample& sample) { return sample.effectiveLongitudinalSlip; });
    pushScenarioNumberArray(api, state, "target_alpha_deg", samples,
        [](const TireScenarioSample& sample) {
            return sample.targetSlipAngleRadians * 180.0 / 3.14159265358979323846;
        });
    pushScenarioNumberArray(api, state, "effective_alpha_deg", samples,
        [](const TireScenarioSample& sample) {
            return sample.effectiveSlipAngleRadians * 180.0 / 3.14159265358979323846;
        });
    pushScenarioNumberArray(api, state, "fx", samples,
        [](const TireScenarioSample& sample) { return sample.force.longitudinalForce; });
    pushScenarioNumberArray(api, state, "fy", samples,
        [](const TireScenarioSample& sample) { return sample.force.lateralForce; });
    pushScenarioNumberArray(api, state, "mz", samples,
        [](const TireScenarioSample& sample) { return sample.force.aligningTorque; });
    pushScenarioNumberArray(api, state, "tread_temp_c", samples,
        [](const TireScenarioSample& sample) { return sample.treadTemperatureC; });
    pushScenarioNumberArray(api, state, "carcass_temp_c", samples,
        [](const TireScenarioSample& sample) { return sample.carcassTemperatureC; });
    pushScenarioNumberArray(api, state, "gas_temp_c", samples,
        [](const TireScenarioSample& sample) { return sample.gasTemperatureC; });
    pushScenarioNumberArray(api, state, "rim_temp_c", samples,
        [](const TireScenarioSample& sample) { return sample.rimTemperatureC; });
    pushScenarioNumberArray(api, state, "pressure_psi", samples,
        [](const TireScenarioSample& sample) {
            return sample.inflationPressurePa / 6894.757293168;
        });
    pushScenarioNumberArray(api, state, "average_tread_mm", samples,
        [](const TireScenarioSample& sample) { return sample.averageTreadDepthM * 1000.0; });
    pushScenarioNumberArray(api, state, "minimum_tread_mm", samples,
        [](const TireScenarioSample& sample) { return sample.minimumTreadDepthM * 1000.0; });
    pushScenarioNumberArray(api, state, "flat_spot_mm", samples,
        [](const TireScenarioSample& sample) { return sample.flatSpotDepthM * 1000.0; });
    pushScenarioNumberArray(api, state, "gas_mass_percent", samples,
        [](const TireScenarioSample& sample) { return sample.containedGasMassRatio * 100.0; });
    pushScenarioNumberArray(api, state, "structural_percent", samples,
        [](const TireScenarioSample& sample) { return sample.structuralIntegrity * 100.0; });
    pushScenarioNumberArray(api, state, "tread_attachment_percent", samples,
        [](const TireScenarioSample& sample) { return sample.treadAttachment * 100.0; });
    pushScenarioNumberArray(api, state, "rim_contact_percent", samples,
        [](const TireScenarioSample& sample) { return sample.rimContactFraction * 100.0; });
}

bool safeScenarioCsvName(const std::string& requestedName)
{
    const std::filesystem::path name(requestedName);
    return !requestedName.empty() && !name.has_parent_path()
        && name.filename() == name && name.extension() == ".csv";
}

} // namespace

int LuaVehicleBindingHandlers::luaVehicleRunTireScenario(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;
    const std::size_t maximumPoints = static_cast<std::size_t>((std::max)(
        16.0, (std::min)(512.0,
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 180.0))));
    TireScenarioResult result;
    if (!runtime->m_physics->vehicles().wheelTireScenario(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            scenarioWheelIndex(runtime->m_api, state, 2),
            LuaModuleRuntime::stringArgument(
                *runtime, state, 3, "relaxation_step"),
            result))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    pushScenarioTable(runtime->m_api, state, result, maximumPoints);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleRunTireFleetBenchmark(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;

    heritage::vehicles::TireModelDescription tire;
    if (!runtime->m_physics->vehicles().wheelTireModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            scenarioWheelIndex(runtime->m_api, state, 2),
            tire))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    heritage::vehicles::tires::TireFleetBenchmarkDescription description;
    description.vehicleCount = static_cast<std::size_t>((std::max)(
        1.0, (std::min)(1000.0,
            LuaModuleRuntime::numberArgument(*runtime, state, 3, 150.0))));
    description.simulatedDurationSeconds = static_cast<heritage::vehicles::VehicleScalar>(
        (std::max)(0.01, (std::min)(10.0,
            LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.25))));
    description.wetWeather = LuaModuleRuntime::booleanArgument(
        *runtime, state, 5, false);
    const auto weather = runtime->m_physics->surfaces().weatherOutput();
    if (description.wetWeather && weather.valid)
    {
        description.roadWaterDepthM = static_cast<heritage::vehicles::VehicleScalar>(
            weather.waterFilmDepthM);
        description.roadTemperatureC = static_cast<heritage::vehicles::VehicleScalar>(
            weather.roadTemperatureC);
        description.windSpeedMps = static_cast<heritage::vehicles::VehicleScalar>(
            weather.windSpeedMps);
    }
    description.ambientTemperatureC = static_cast<heritage::vehicles::VehicleScalar>(
        runtime->m_physics->surfaces().environment().ambientTemperatureC);

    const heritage::vehicles::VehicleScalar radius =
        tire.contactGeometry.unloadedRadiusM > 0.05
            ? tire.contactGeometry.unloadedRadiusM
            : heritage::vehicles::VehicleScalar{0.30};
    const auto result = heritage::vehicles::tires::runTireFleetBenchmark(
        tire, radius, description);
    if (!result.valid)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_createtable(state, 0, 22);
    const auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto setBoolean = [&](const char* name, bool value) {
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    setBoolean("valid", result.valid);
    setBoolean("wet_weather", description.wetWeather);
    setNumber("vehicles", static_cast<double>(result.vehicleCount));
    setNumber("tires", static_cast<double>(result.tireCount));
    setNumber("tire_steps", static_cast<double>(result.tireSteps));
    setNumber("simulated_seconds", result.simulatedSeconds);
    setNumber("wall_clock_ms", result.wallClockMilliseconds);
    setNumber("real_time_factor", result.realTimeFactor);
    setNumber("tire_evaluations_per_second", result.tireEvaluationsPerSecond);
    setNumber("microseconds_per_vehicle_step", result.microsecondsPerVehicleStep);
    setNumber("whole_tire_evaluations",
        static_cast<double>(result.wholeTireForceEvaluations));
    setNumber("distributed_brush_cells",
        static_cast<double>(result.distributedBrushCellEvaluations));
    setNumber("thermal_updates", static_cast<double>(result.thermalStateUpdates));
    setNumber("wear_updates", static_cast<double>(result.wearStateUpdates));
    setNumber("wet_updates", static_cast<double>(result.wetStateUpdates));
    setNumber("hydrology_cells", static_cast<double>(result.hydrologyCellCount));
    setNumber("hydrology_steps", static_cast<double>(result.hydrologySteps));
    setNumber("hydrology_tire_contacts",
        static_cast<double>(result.hydrologyTireContacts));
    setNumber("checksum", result.checksum);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleExportTireScenarioCsv(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string scenarioName = LuaModuleRuntime::stringArgument(
        *runtime, state, 3, "relaxation_step");
    const std::string requestedName = LuaModuleRuntime::stringArgument(
        *runtime, state, 4, scenarioName + ".csv");
    if (!runtime->m_physics || !runtime->m_context
        || !safeScenarioCsvName(requestedName))
    {
        const std::string message = !safeScenarioCsvName(requestedName)
            ? "Tire scenario export name must be a plain .csv filename."
            : "Tire scenario export requires Vehicle and Module services.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
        return 2;
    }
    TireScenarioResult result;
    const bool ran = runtime->m_physics->vehicles().wheelTireScenario(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
        scenarioWheelIndex(runtime->m_api, state, 2),
        scenarioName,
        result);
    const std::filesystem::path path = runtime->m_context->resolveSavePath(
        std::filesystem::path("TireCalibration") / requestedName);
    std::string exportError;
    const bool exported = ran && !path.empty()
        && heritage::vehicles::exportTireScenarioCsv(
            result, path, &exportError);
    const std::string message = exported
        ? path.string()
        : (ran ? exportError : runtime->m_physics->vehicles().lastError());
    runtime->m_api.lua_pushboolean(state, exported ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
}

} // namespace heritage::modules
