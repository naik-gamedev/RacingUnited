#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"

#include "../../../../Physics/PhysicsWorld.hpp"
#include "../../../Diagnostics/BuildIdentity.hpp"
#include "../../../../Vehicles/Tires/TireCalibrationLab.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

namespace heritage::modules {
namespace {

using heritage::vehicles::TireCalibrationSample;
using heritage::vehicles::TireCalibrationSweepResult;
using heritage::vehicles::TireModelDescription;

std::size_t wheelIndexArgument(
    LuaApi& api,
    lua_State* state,
    int argument)
{
    int converted = 0;
    const LuaInteger value = api.lua_tointegerx(
        state, argument, &converted);
    return converted && value >= 1
        ? static_cast<std::size_t>(value - 1)
        : static_cast<std::size_t>(-1);
}

void pushNumberArray(
    LuaApi& api,
    lua_State* state,
    const char* field,
    const std::vector<const TireCalibrationSample*>& samples,
    heritage::vehicles::VehicleScalar (*value)(const TireCalibrationSample&))
{
    api.lua_createtable(
        state, static_cast<int>(samples.size()), 0);
    for (std::size_t index = 0; index < samples.size(); ++index)
    {
        api.lua_pushnumber(state, value(*samples[index]));
        api.lua_rawseti(
            state, -2, static_cast<LuaInteger>(index + 1));
    }
    api.lua_setfield(state, -2, field);
}

void pushCalibrationResult(
    LuaApi& api,
    lua_State* state,
    const TireCalibrationSweepResult& result,
    const TireModelDescription& tire,
    std::size_t requestedSecondarySlice,
    std::size_t maximumPoints)
{
    const std::size_t secondaryCount = result.secondary.sampleCount;
    const std::size_t secondaryIndex = (std::min)(
        secondaryCount - 1, requestedSecondarySlice);
    const std::size_t primaryCount = result.primary.sampleCount;
    const std::size_t outputCount = (std::min)(primaryCount, maximumPoints);

    std::vector<const TireCalibrationSample*> samples;
    samples.reserve(outputCount);
    for (std::size_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        const std::size_t primaryIndex = outputCount <= 1
            ? 0
            : static_cast<std::size_t>(std::llround(
                static_cast<double>(outputIndex)
                * static_cast<double>(primaryCount - 1)
                / static_cast<double>(outputCount - 1)));
        samples.push_back(&result.samples[
            secondaryIndex * primaryCount + primaryIndex]);
    }

    api.lua_createtable(state, 0, 34);
    const auto pushNumber = [&](const char* field, LuaNumber value) {
        api.lua_pushnumber(state, value);
        api.lua_setfield(state, -2, field);
    };
    const auto pushInteger = [&](const char* field, LuaInteger value) {
        api.lua_pushinteger(state, value);
        api.lua_setfield(state, -2, field);
    };
    const auto pushBoolean = [&](const char* field, bool value) {
        api.lua_pushboolean(state, value ? 1 : 0);
        api.lua_setfield(state, -2, field);
    };
    const auto pushString = [&](const char* field, const std::string& value) {
        api.lua_pushlstring(state, value.c_str(), value.size());
        api.lua_setfield(state, -2, field);
    };

    pushBoolean("valid", result.valid);
    pushString("name", result.name);
    pushString("primary_axis", heritage::vehicles::tireCalibrationAxisName(
        result.primary.axis));
    pushString("secondary_axis", heritage::vehicles::tireCalibrationAxisName(
        result.secondary.axis));
    pushInteger("primary_count", static_cast<LuaInteger>(primaryCount));
    pushInteger("secondary_count", static_cast<LuaInteger>(secondaryCount));
    pushInteger("secondary_slice", static_cast<LuaInteger>(secondaryIndex + 1));
    pushInteger("sample_count", static_cast<LuaInteger>(samples.size()));
    pushNumber("secondary_value", samples.front()->secondaryValue);
    pushBoolean("imported", tire.importedPropertyFile);
    pushInteger("fit_type", static_cast<LuaInteger>(tire.importedFitType));
    pushString("parameter_source", tire.parameterSource);
    pushString("parameter_provenance", tire.parameterProvenance);
    pushNumber("parameter_confidence", tire.parameterConfidence);
    pushBoolean("legacy_seed", tire.magicFormulaUsesLegacySeed);
    pushNumber("nominal_load_n", tire.nominalLoad);
    pushNumber("reference_pressure_pa", tire.referenceInflationPressurePa);
    pushNumber("minimum_load_n", tire.magicFormula.minimumLoadN);
    pushNumber("maximum_load_n", tire.magicFormula.maximumLoadN);
    pushNumber("minimum_pressure_pa", tire.magicFormula.minimumPressurePa);
    pushNumber("maximum_pressure_pa", tire.magicFormula.maximumPressurePa);
    pushNumber("maximum_abs_longitudinal_slip",
        tire.magicFormula.maximumAbsLongitudinalSlip);
    pushNumber("maximum_abs_slip_angle_rad",
        tire.magicFormula.maximumAbsSlipAngleRadians);
    pushNumber("maximum_abs_camber_rad",
        tire.magicFormula.maximumAbsCamberRadians);

    pushNumberArray(api, state, "primary", samples,
        [](const TireCalibrationSample& sample) { return sample.primaryValue; });
    pushNumberArray(api, state, "fx", samples,
        [](const TireCalibrationSample& sample) { return sample.force.longitudinalForce; });
    pushNumberArray(api, state, "fy", samples,
        [](const TireCalibrationSample& sample) { return sample.force.lateralForce; });
    pushNumberArray(api, state, "mz", samples,
        [](const TireCalibrationSample& sample) { return sample.force.aligningTorque; });
    pushNumberArray(api, state, "mx", samples,
        [](const TireCalibrationSample& sample) { return sample.force.overturningMoment; });
    pushNumberArray(api, state, "my", samples,
        [](const TireCalibrationSample& sample) { return sample.force.rollingResistanceMoment; });
    pushNumberArray(api, state, "trail_mm", samples,
        [](const TireCalibrationSample& sample) { return sample.force.pneumaticTrail * 1000.0; });
    pushNumberArray(api, state, "grip_percent", samples,
        [](const TireCalibrationSample& sample) { return sample.force.gripUtilization * 100.0; });
    pushNumberArray(api, state, "longitudinal_stiffness", samples,
        [](const TireCalibrationSample& sample) { return sample.force.longitudinalSlipStiffness; });
    pushNumberArray(api, state, "cornering_stiffness", samples,
        [](const TireCalibrationSample& sample) { return sample.force.corneringStiffness; });
    pushNumberArray(api, state, "camber_stiffness", samples,
        [](const TireCalibrationSample& sample) { return sample.force.camberStiffness; });
}

bool safeCsvName(const std::string& requestedName)
{
    const std::filesystem::path name(requestedName);
    return !requestedName.empty()
        && !name.has_parent_path()
        && name.filename() == name
        && name.extension() == ".csv";
}

std::string escapedJsonString(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += character; break;
        }
    }
    return result;
}

bool exportCalibrationManifest(
    const TireCalibrationSweepResult& result,
    const TireModelDescription& tire,
    const heritage::vehicles::tires::TirePartAssignmentInfo& assignment,
    std::size_t wheelIndex,
    const std::filesystem::path& path,
    std::string& error)
{
    if (!result.valid || result.samples.empty() || path.empty())
    {
        error = "Tire calibration manifest requires a valid non-empty sweep and path.";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        error = "Could not open the tire calibration manifest.";
        return false;
    }

    const TireCalibrationSample& baseline = result.samples.front();
    output << std::fixed << std::setprecision(9)
        << "{\n"
        << "  \"schema\": \"heritage_tire_calibration_manifest_v1\",\n"
        << "  \"engine_build_identity\": \""
        << escapedJsonString(heritage::diagnostics::buildIdentity()) << "\",\n"
        << "  \"sweep\": {\n"
        << "    \"name\": \"" << escapedJsonString(result.name) << "\",\n"
        << "    \"primary_axis\": \""
        << heritage::vehicles::tireCalibrationAxisName(result.primary.axis)
        << "\",\n"
        << "    \"primary_minimum\": " << result.primary.minimum << ",\n"
        << "    \"primary_maximum\": " << result.primary.maximum << ",\n"
        << "    \"primary_samples\": " << result.primary.sampleCount << ",\n"
        << "    \"secondary_axis\": \""
        << heritage::vehicles::tireCalibrationAxisName(result.secondary.axis)
        << "\",\n"
        << "    \"secondary_minimum\": " << result.secondary.minimum << ",\n"
        << "    \"secondary_maximum\": " << result.secondary.maximum << ",\n"
        << "    \"secondary_samples\": " << result.secondary.sampleCount << "\n"
        << "  },\n"
        << "  \"installed_wheel\": {\n"
        << "    \"index_one_based\": " << (wheelIndex + 1) << ",\n"
        << "    \"radius_m\": " << baseline.input.wheelRadiusM << ",\n"
        << "    \"part_assigned\": " << (assignment.assigned ? "true" : "false") << ",\n"
        << "    \"part_id\": \"" << escapedJsonString(assignment.partId) << "\",\n"
        << "    \"part_display_name\": \""
        << escapedJsonString(assignment.displayName) << "\",\n"
        << "    \"family\": \""
        << heritage::vehicles::tires::tireFamilyName(assignment.family) << "\",\n"
        << "    \"resolution_source\": "
        << static_cast<unsigned int>(assignment.source) << ",\n"
        << "    \"cold_pressure_pa\": "
        << assignment.coldInflationPressurePa << "\n"
        << "  },\n"
        << "  \"parameters\": {\n"
        << "    \"source\": \"" << escapedJsonString(tire.parameterSource) << "\",\n"
        << "    \"provenance\": \""
        << escapedJsonString(tire.parameterProvenance) << "\",\n"
        << "    \"confidence\": " << tire.parameterConfidence << ",\n"
        << "    \"imported_property_file\": "
        << (tire.importedPropertyFile ? "true" : "false") << ",\n"
        << "    \"fit_type\": " << tire.importedFitType << ",\n"
        << "    \"legacy_seed\": "
        << (tire.magicFormulaUsesLegacySeed ? "true" : "false") << "\n"
        << "  },\n"
        << "  \"validity\": {\n"
        << "    \"minimum_load_n\": " << tire.magicFormula.minimumLoadN << ",\n"
        << "    \"maximum_load_n\": " << tire.magicFormula.maximumLoadN << ",\n"
        << "    \"minimum_pressure_pa\": "
        << tire.magicFormula.minimumPressurePa << ",\n"
        << "    \"maximum_pressure_pa\": "
        << tire.magicFormula.maximumPressurePa << ",\n"
        << "    \"maximum_abs_longitudinal_slip\": "
        << tire.magicFormula.maximumAbsLongitudinalSlip << ",\n"
        << "    \"maximum_abs_slip_angle_rad\": "
        << tire.magicFormula.maximumAbsSlipAngleRadians << ",\n"
        << "    \"maximum_abs_camber_rad\": "
        << tire.magicFormula.maximumAbsCamberRadians << "\n"
        << "  },\n"
        << "  \"operating_point\": {\n"
        << "    \"normal_load_n\": " << baseline.input.normalLoad << ",\n"
        << "    \"inflation_pressure_pa\": "
        << baseline.input.inflationPressurePa << ",\n"
        << "    \"forward_speed_mps\": " << baseline.input.forwardSpeedMps << "\n"
        << "  }\n"
        << "}\n";
    if (!output)
    {
        error = "Writing the tire calibration manifest failed.";
        return false;
    }
    error.clear();
    return true;
}

} // namespace

int LuaVehicleBindingHandlers::luaVehicleRunTireCalibrationSweep(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;

    const auto handle = LuaModuleRuntime::vehicleHandleArgument(
        *runtime, state, 1);
    const std::size_t wheelIndex = wheelIndexArgument(runtime->m_api, state, 2);
    const std::string sweepName = LuaModuleRuntime::stringArgument(
        *runtime, state, 3, "pure_longitudinal");
    const std::size_t secondarySlice = static_cast<std::size_t>((std::max)(
        1.0, LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0))) - 1;
    const std::size_t maximumPoints = static_cast<std::size_t>((std::max)(
        16.0, (std::min)(512.0,
            LuaModuleRuntime::numberArgument(*runtime, state, 5, 180.0))));

    TireCalibrationSweepResult result;
    TireModelDescription tire;
    if (!runtime->m_physics->vehicles().wheelTireCalibrationSweep(
            handle, wheelIndex, sweepName, result)
        || !runtime->m_physics->vehicles().wheelTireModel(
            handle, wheelIndex, tire))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    pushCalibrationResult(
        runtime->m_api, state, result, tire, secondarySlice, maximumPoints);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleExportTireCalibrationSweepCsv(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string sweepName = LuaModuleRuntime::stringArgument(
        *runtime, state, 3, "pure_longitudinal");
    const std::string requestedName = LuaModuleRuntime::stringArgument(
        *runtime, state, 4, sweepName + ".csv");
    if (!runtime->m_physics || !runtime->m_context
        || !safeCsvName(requestedName))
    {
        const std::string message = !safeCsvName(requestedName)
            ? "Tire calibration export name must be a plain .csv filename."
            : "Tire calibration export requires Vehicle and Module services.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    TireCalibrationSweepResult result;
    TireModelDescription tire;
    heritage::vehicles::tires::TirePartAssignmentInfo assignment;
    const auto handle = LuaModuleRuntime::vehicleHandleArgument(
        *runtime, state, 1);
    const std::size_t wheelIndex = wheelIndexArgument(
        runtime->m_api, state, 2);
    const bool ran = runtime->m_physics->vehicles().wheelTireCalibrationSweep(
        handle,
        wheelIndex,
        sweepName,
        result)
        && runtime->m_physics->vehicles().wheelTireModel(
            handle, wheelIndex, tire)
        && runtime->m_physics->vehicles().wheelTirePartAssignment(
            handle, wheelIndex, assignment);
    const std::filesystem::path path = runtime->m_context->resolveSavePath(
        std::filesystem::path("TireCalibration") / requestedName);
    std::filesystem::path manifestPath = path;
    manifestPath.replace_extension(".manifest.json");
    std::string exportError;
    const bool exported = ran && !path.empty()
        && heritage::vehicles::exportTireCalibrationSweepCsv(
            result, path, &exportError)
        && exportCalibrationManifest(
            result, tire, assignment, wheelIndex, manifestPath, exportError);
    const std::string message = exported
        ? path.string() + " + " + manifestPath.string()
        : (ran ? exportError : runtime->m_physics->vehicles().lastError());
    runtime->m_api.lua_pushboolean(state, exported ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
}

} // namespace heritage::modules
