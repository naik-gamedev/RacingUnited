#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "LuaVehicleDefinitionParser.hpp"
#include "../../../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cmath>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../../../../Vehicles/VehicleDefinitionCompiler.hpp"
#include "../../../../Vehicles/VehicleDefinitionLoader.hpp"
#include "../../../../Vehicles/VehicleAssetMetadata.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaVehicleBindingHandlers::luaVehicleIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleCompileDefinitionV2(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::VehicleDefinitionV2Source source;
    std::string bridgeError;
    if (!parseLuaVehicleDefinitionV2(
            runtime->m_api, state, 1, source, bridgeError))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(state, "unresolved");
        runtime->m_api.lua_pushstring(state, "Native definition bridge rejected the input.");
        runtime->m_api.lua_pushlstring(
            state, bridgeError.c_str(), bridgeError.size());
        return 5;
    }

    const heritage::vehicles::VehicleDefinitionCompileResult result =
        heritage::vehicles::VehicleDefinitionCompiler::compile(source);
    const std::string issues = result.issueSummary();
    runtime->m_api.lua_pushboolean(state, result.valid ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, result.currentSolverReady ? 1 : 0);
    runtime->m_api.lua_pushlstring(
        state,
        result.definition.runtimeProvider.c_str(),
        result.definition.runtimeProvider.size());
    runtime->m_api.lua_pushlstring(
        state, result.summary.c_str(), result.summary.size());
    runtime->m_api.lua_pushlstring(state, issues.c_str(), issues.size());
    return 5;
}

int LuaVehicleBindingHandlers::luaVehicleCreateFromDefinitionV2(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushstring(state, "unresolved");
        runtime->m_api.lua_pushstring(state, "Vehicle system is unavailable.");
        return 3;
    }

    heritage::vehicles::VehicleDefinitionV2Source source;
    std::string bridgeError;
    if (!parseLuaVehicleDefinitionV2(
            runtime->m_api, state, 1, source, bridgeError))
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushstring(state, "unresolved");
        runtime->m_api.lua_pushlstring(
            state, bridgeError.c_str(), bridgeError.size());
        return 3;
    }

    const heritage::vehicles::VehicleDefinitionCompileResult compiled =
        heritage::vehicles::VehicleDefinitionCompiler::compile(source);
    if (!compiled.valid || !compiled.currentSolverReady)
    {
        std::string message = compiled.issueSummary();
        if (message.empty())
            message = compiled.summary;
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushlstring(
            state,
            compiled.definition.runtimeProvider.c_str(),
            compiled.definition.runtimeProvider.size());
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 3;
    }

    heritage::vehicles::VehicleDefinitionLoadSettings settings;
    if (runtime->m_context)
        settings.moduleRoot = runtime->m_context->moduleRoot();
    settings.vehicle.chassisBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 2);
    settings.vehicle.highRateHertz = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 1000.0));
    settings.vehicle.maximumDriveForce = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 7000.0));
    settings.vehicle.maximumBrakeForce = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, 12000.0));
    settings.vehicle.maximumSteerAngleDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 38.0));
    settings.vehicle.tireFriction = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 7, 1.15));
    settings.vehicle.lateralStiffness = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 8, 11000.0));
    settings.vehicle.rollingResistance = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 90.0));

    std::string loadMessage;
    const heritage::vehicles::VehicleHandle handle =
        heritage::vehicles::VehicleDefinitionLoader::create(
            compiled.definition,
            settings,
            runtime->m_physics->rigidBodies(),
            runtime->m_physics->vehicles(),
            loadMessage);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    runtime->m_api.lua_pushlstring(
        state,
        compiled.definition.runtimeProvider.c_str(),
        compiled.definition.runtimeProvider.size());
    runtime->m_api.lua_pushlstring(
        state, loadMessage.c_str(), loadMessage.size());
    return 3;
}

int LuaVehicleBindingHandlers::luaVehicleCreate(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::vehicles::VehicleDescription description;
    description.chassisBody = LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1);
    description.highRateHertz = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1000.0));
    description.maximumDriveForce = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 7000.0));
    description.maximumBrakeForce = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 12000.0));
    description.maximumSteerAngleDegrees = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 38.0));
    description.tireFriction = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 1.15));
    description.lateralStiffness = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 11000.0));
    description.rollingResistance = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 90.0));
    const heritage::vehicles::VehicleHandle handle = runtime->m_physics->vehicles().create(
        description, runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleDestroy(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().destroy(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().exists(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().count()) : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleInspectAssetMetadata(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativeText = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path relative = heritage::paths::fromUtf8(relativeText);
    std::string extension = relative.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const std::filesystem::path absolute = runtime->m_context->resolveAssetPath(relative);
    std::error_code filesystemError;
    if (absolute.empty()
        || extension != ".glb"
        || !std::filesystem::is_regular_file(absolute, filesystemError)
        || filesystemError)
    {
        runtime->m_api.lua_pushnil(state);
        const std::string message =
            "Vehicle.InspectAssetMetadata requires an existing module-asset-relative .glb file.";
        runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
        return 2;
    }

    heritage::vehicles::VehicleAssetMetadata metadata;
    std::string error;
    if (!heritage::vehicles::inspectVehicleAssetMetadata(absolute, metadata, error))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
        return 2;
    }

    auto setString = [&](const char* name, const std::string& value) {
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
        runtime->m_api.lua_setfield(state, -2, name);
    };
    auto setNumber = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
        runtime->m_api.lua_setfield(state, -2, name);
    };
    auto setBool = [&](const char* name, bool value) {
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
        runtime->m_api.lua_setfield(state, -2, name);
    };

    runtime->m_api.lua_createtable(state, 0, 8);
    setString("asset_path", heritage::paths::toUtf8(relative.lexically_normal()));
    setNumber("part_count", static_cast<double>(metadata.parts.size()));
    setNumber(
        "suspension_hardpoint_count",
        static_cast<double>(metadata.suspensionHardpoints.size()));
    setNumber(
        "wheel_fitment_datum_count",
        static_cast<double>(metadata.wheelFitmentDatums.size()));
    setNumber("warning_count", static_cast<double>(metadata.warnings.size()));
    std::string warningSummary;
    for (const std::string& warning : metadata.warnings)
    {
        if (!warningSummary.empty())
            warningSummary += "\n";
        warningSummary += warning;
    }
    setString("warning_summary", warningSummary);

    runtime->m_api.lua_createtable(state, 0, static_cast<int>(metadata.parts.size()));
    for (const auto& part : metadata.parts)
    {
        runtime->m_api.lua_createtable(state, 0, 12);
        setNumber("node_index", static_cast<double>(part.nodeIndex));
        setString("node_name", part.nodeName);
        setString("slot", part.slot);
        setString("role", part.role);
        setString("part_type", part.partType);
        setString("part_id", part.partId);
        setString("corner", part.corner);
        setBool("replaceable", part.replaceable);
        setBool("rotates_with_wheel", part.rotatesWithWheel);
        setNumber(
            "tire_nominal_diameter_m",
            heritage::vehicles::tireNominalDiameterMeters(part));

        runtime->m_api.lua_createtable(
            state, 0, static_cast<int>(part.properties.size()));
        for (const auto& [key, value] : part.properties)
        {
            switch (value.type)
            {
            case heritage::graphics::AssetMetadataValueType::String:
                runtime->m_api.lua_pushlstring(
                    state, value.stringValue.c_str(), value.stringValue.size());
                break;
            case heritage::graphics::AssetMetadataValueType::Number:
                runtime->m_api.lua_pushnumber(
                    state, static_cast<LuaNumber>(value.numberValue));
                break;
            case heritage::graphics::AssetMetadataValueType::Boolean:
                runtime->m_api.lua_pushboolean(state, value.boolValue ? 1 : 0);
                break;
            }
            runtime->m_api.lua_setfield(state, -2, key.c_str());
        }
        runtime->m_api.lua_setfield(state, -2, "properties");

        const std::string key = part.slot.empty() ? part.nodeName : part.slot;
        runtime->m_api.lua_setfield(state, -2, key.c_str());
    }
    runtime->m_api.lua_setfield(state, -2, "parts");

    runtime->m_api.lua_createtable(
        state, 0, static_cast<int>(metadata.wheelFitmentDatums.size()));
    for (const auto& datum : metadata.wheelFitmentDatums)
    {
        runtime->m_api.lua_createtable(state, 0, 13);
        setNumber("node_index", static_cast<double>(datum.nodeIndex));
        setString("node_name", datum.nodeName);
        setString("corner", datum.corner);
        setString("role", datum.role);
        setNumber("x", datum.localPosition.x);
        setNumber("y", datum.localPosition.y);
        setNumber("z", datum.localPosition.z);
        setNumber("axis_x", datum.localAxis.x);
        setNumber("axis_y", datum.localAxis.y);
        setNumber("axis_z", datum.localAxis.z);
        setString("provenance", datum.provenance);
        setNumber("confidence", datum.confidence);
        const std::string key = datum.corner + ":" + datum.role;
        runtime->m_api.lua_setfield(state, -2, key.c_str());
    }
    runtime->m_api.lua_setfield(state, -2, "wheel_fitment_datums");

    runtime->m_api.lua_createtable(
        state, 0, static_cast<int>(metadata.suspensionHardpoints.size()));
    for (const auto& hardpoint : metadata.suspensionHardpoints)
    {
        runtime->m_api.lua_createtable(state, 0, 9);
        setNumber("node_index", static_cast<double>(hardpoint.nodeIndex));
        setString("node_name", hardpoint.nodeName);
        setString("corner", hardpoint.corner);
        setString("id", hardpoint.id);
        setNumber("x", hardpoint.localPosition.x);
        setNumber("y", hardpoint.localPosition.y);
        setNumber("z", hardpoint.localPosition.z);
        setString("provenance", hardpoint.provenance);
        setNumber("confidence", hardpoint.confidence);
        const std::string key = hardpoint.corner + ":" + hardpoint.id;
        runtime->m_api.lua_setfield(state, -2, key.c_str());
    }
    runtime->m_api.lua_setfield(state, -2, "suspension_hardpoints");

    runtime->m_api.lua_createtable(state, 0, static_cast<int>(metadata.warnings.size()));
    for (std::size_t index = 0; index < metadata.warnings.size(); ++index)
    {
        const std::string key = std::to_string(index + 1);
        runtime->m_api.lua_pushlstring(
            state,
            metadata.warnings[index].c_str(),
            metadata.warnings[index].size());
        runtime->m_api.lua_setfield(state, -2, key.c_str());
    }
    runtime->m_api.lua_setfield(state, -2, "warnings");

    runtime->m_api.lua_pushnil(state);
    return 2;
}

int LuaVehicleBindingHandlers::luaVehicleCheckTireWheelCompatibility(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::filesystem::path relative(LuaModuleRuntime::stringArgument(*runtime, state, 1));
    const std::string wheelSlot = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string tireSlot = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    const std::filesystem::path absolute = runtime->m_context->resolveAssetPath(relative);

    std::string extension = relative.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    std::error_code filesystemError;
    if (absolute.empty()
        || extension != ".glb"
        || !std::filesystem::is_regular_file(absolute, filesystemError)
        || filesystemError)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(state, "invalid GLB asset path");
        return 3;
    }

    heritage::vehicles::VehicleAssetMetadata metadata;
    std::string error;
    if (!heritage::vehicles::inspectVehicleAssetMetadata(absolute, metadata, error))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
        return 3;
    }

    const auto* wheel = metadata.findBySlot(wheelSlot);
    const auto* tire = metadata.findBySlot(tireSlot);
    if (!wheel || !tire)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        const std::string message = !wheel
            ? "wheel slot not found: " + wheelSlot
            : "tire slot not found: " + tireSlot;
        runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
        return 3;
    }

    const heritage::vehicles::VehiclePartCompatibility compatibility =
        heritage::vehicles::checkTireWheelCompatibility(*wheel, *tire);
    const std::string summary = compatibility.summary();
    runtime->m_api.lua_pushboolean(state, compatibility.compatible ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, compatibility.complete ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, summary.c_str(), summary.size());
    return 3;
}

int LuaVehicleBindingHandlers::luaVehicleAddWheel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelDescription description;
    description.localMount = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.8)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)) };
    description.localSuspensionDirection = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, -1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0)) };
    description.radius = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.35));
    description.restLength = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.50));
    description.maximumCompression = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 10, 0.18));
    description.maximumDroop = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 11, 0.15));
    description.springRate = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 12, 35000.0));
    description.bumpDamping = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 13, 3200.0));
    description.reboundDamping = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 14, 4200.0));
    description.driveFactor = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 15, 0.0));
    description.steerFactor = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 16, 0.0));
    description.brakeFactor = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 17, 1.0));
    description.handbrakeFactor = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 18, 0.0));
    description.springPreload = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 19, 0.0));
    description.springProgression = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 20, 0.0));
    description.bumpHighSpeedDamping = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 21, description.bumpDamping));
    description.bumpDampingKneeVelocity = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 22, 1.0));
    description.reboundHighSpeedDamping = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 23, description.reboundDamping));
    description.reboundDampingKneeVelocity = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 24, 1.0));
    description.bumpStopEngagement = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 25, description.maximumCompression));
    description.bumpStopRate = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 26, 0.0));
    description.bumpStopProgression = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 27, 0.0));
    description.droopStopEngagement = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 28, description.maximumDroop));
    description.droopStopRate = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 29, 0.0));
    description.suspensionMotionRatio = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 30, 1.0));
    description.maximumSuspensionForce = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 31, 250000.0));
    description.effectiveUnsprungMass = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 32, 0.0));
    description.tireRadialStiffness = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 33, 220000.0));
    description.tireRadialDamping = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 34, 1800.0));
    description.maximumTireDeflection = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 35, 0.08));
    description.maximumTireNormalForce = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 36, 250000.0));
    description.localSteeringAxis = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 37, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 38, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 39, 0.0)) };
    description.staticCamberDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 40, 0.0));
    description.camberGainDegreesPerM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 41, 0.0));
    description.camberProgressionDegreesPerM2 = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 42, 0.0));
    description.staticToeDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 43, 0.0));
    description.toeGainDegreesPerM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 44, 0.0));
    description.toeProgressionDegreesPerM2 = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 45, 0.0));
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().addWheel(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), description);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().wheelCount(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

} // namespace heritage::modules
