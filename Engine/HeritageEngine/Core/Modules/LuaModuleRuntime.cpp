#include "LuaModuleRuntime.hpp"
#include "../Diagnostics/BuildIdentity.hpp"
#include "../../Physics/StaticBoxSceneImporter.hpp"
#include "../../Physics/StaticTriangleSceneImporter.hpp"
#include "../../Vehicles/VehicleDefinitionCompiler.hpp"
#include "../../Vehicles/VehicleDefinitionLoader.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace heritage::modules {
namespace {

constexpr int kLuaOk = 0;
constexpr int kLuaTypeNil = 0;
constexpr int kLuaTypeBoolean = 1;
constexpr int kLuaTypeNumber = 3;
constexpr int kLuaTypeString = 4;
constexpr int kLuaTypeTable = 5;
constexpr int kLuaTypeFunction = 6;

std::unordered_map<lua_State*, LuaModuleRuntime*> g_runtimeByState;

float clampFloat(float value, float minimum, float maximum)
{
    return (std::max)(minimum, (std::min)(maximum, value));
}

enum class UiAlignment
{
    Left,
    Center,
    Right
};

UiAlignment parseAlignment(const std::string& text)
{
    std::string normalized = text;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });

    if (normalized == "center" || normalized == "centre")
        return UiAlignment::Center;
    if (normalized == "right")
        return UiAlignment::Right;
    return UiAlignment::Left;
}

ImVec2 calculateImageSize(
    int sourceWidth,
    int sourceHeight,
    float requestedWidth,
    float requestedHeight,
    float maximumWidth)
{
    const float safeSourceWidth = static_cast<float>((std::max)(sourceWidth, 1));
    const float safeSourceHeight = static_cast<float>((std::max)(sourceHeight, 1));
    const float aspect = safeSourceWidth / safeSourceHeight;

    float width = requestedWidth;
    float height = requestedHeight;

    if (width <= 0.0f && height <= 0.0f)
    {
        width = safeSourceWidth;
        height = safeSourceHeight;
    }
    else if (width > 0.0f && height <= 0.0f)
    {
        height = width / aspect;
    }
    else if (width <= 0.0f && height > 0.0f)
    {
        width = height * aspect;
    }

    width = (std::max)(1.0f, width);
    height = (std::max)(1.0f, height);

    const float safeMaximumWidth = (std::max)(1.0f, maximumWidth);
    if (width > safeMaximumWidth)
    {
        const float scale = safeMaximumWidth / width;
        width *= scale;
        height *= scale;
    }

    return ImVec2(width, height);
}

void alignNextItem(UiAlignment alignment, float itemWidth)
{
    if (alignment == UiAlignment::Left)
        return;

    const float available = ImGui::GetContentRegionAvail().x;
    const float offset = alignment == UiAlignment::Center
        ? (available - itemWidth) * 0.5f
        : available - itemWidth;

    if (offset > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
}

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20)
            {
                output << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<int>(character)
                    << std::dec << std::setfill(' ');
            }
            else
            {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

int absoluteLuaIndex(const LuaApi& api, lua_State* state, int index)
{
    return index > 0 ? index : api.lua_gettop(state) + index + 1;
}

void popLuaValues(const LuaApi& api, lua_State* state, int count = 1)
{
    if (count > 0)
        api.lua_settop(state, -count - 1);
}

std::string luaFieldString(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    const std::string& fallback = {})
{
    const int absoluteTable = absoluteLuaIndex(api, state, tableIndex);
    api.lua_getfield(state, absoluteTable, field);
    std::string value = fallback;
    if (api.lua_type(state, -1) == kLuaTypeString)
    {
        std::size_t length = 0;
        const char* text = api.lua_tolstring(state, -1, &length);
        if (text)
            value.assign(text, length);
    }
    popLuaValues(api, state);
    return value;
}

double luaFieldNumber(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    double fallback)
{
    const int absoluteTable = absoluteLuaIndex(api, state, tableIndex);
    api.lua_getfield(state, absoluteTable, field);
    int converted = 0;
    const LuaNumber candidate = api.lua_tonumberx(state, -1, &converted);
    popLuaValues(api, state);
    return converted ? static_cast<double>(candidate) : fallback;
}

bool luaFieldBoolean(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    bool fallback)
{
    const int absoluteTable = absoluteLuaIndex(api, state, tableIndex);
    api.lua_getfield(state, absoluteTable, field);
    const bool value = api.lua_type(state, -1) == kLuaTypeBoolean
        ? api.lua_toboolean(state, -1) != 0
        : fallback;
    popLuaValues(api, state);
    return value;
}

bool pushLuaTableField(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field)
{
    const int absoluteTable = absoluteLuaIndex(api, state, tableIndex);
    api.lua_getfield(state, absoluteTable, field);
    if (api.lua_type(state, -1) == kLuaTypeTable)
        return true;
    popLuaValues(api, state);
    return false;
}

heritage::math::Vec3 luaFieldVector3(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    const heritage::math::Vec3& fallback)
{
    if (!pushLuaTableField(api, state, tableIndex, field))
        return fallback;

    const int vectorIndex = api.lua_gettop(state);
    heritage::math::Vec3 value = fallback;
    float* components[] = { &value.x, &value.y, &value.z };
    for (LuaInteger component = 1; component <= 3; ++component)
    {
        api.lua_rawgeti(state, vectorIndex, component);
        int converted = 0;
        const LuaNumber candidate = api.lua_tonumberx(state, -1, &converted);
        if (converted)
            *components[static_cast<std::size_t>(component - 1)] =
                static_cast<float>(candidate);
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    return value;
}

bool luaArrayLength(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    std::size_t maximum,
    const char* label,
    std::size_t& length,
    std::string& errorMessage)
{
    length = api.lua_rawlen(state, tableIndex);
    if (length <= maximum)
        return true;

    errorMessage = std::string(label) + " exceeds the native bridge limit of "
        + std::to_string(maximum) + ".";
    return false;
}

std::vector<float> luaNumberArrayField(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    std::size_t maximum)
{
    std::vector<float> values;
    if (!pushLuaTableField(api, state, tableIndex, field))
        return values;

    const int arrayIndex = api.lua_gettop(state);
    const std::size_t count = (std::min)(api.lua_rawlen(state, arrayIndex), maximum);
    values.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        api.lua_rawgeti(
            state,
            arrayIndex,
            static_cast<LuaInteger>(index + 1));
        int converted = 0;
        const LuaNumber value = api.lua_tonumberx(state, -1, &converted);
        values.push_back(converted ? static_cast<float>(value) : 0.0f);
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    return values;
}

std::vector<std::string> luaStringArrayField(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    std::size_t maximum)
{
    std::vector<std::string> values;
    if (!pushLuaTableField(api, state, tableIndex, field))
        return values;

    const int arrayIndex = api.lua_gettop(state);
    const std::size_t count = (std::min)(api.lua_rawlen(state, arrayIndex), maximum);
    values.reserve(count);
    for (std::size_t index = 0; index < count; ++index)
    {
        api.lua_rawgeti(
            state,
            arrayIndex,
            static_cast<LuaInteger>(index + 1));
        std::string value;
        if (api.lua_type(state, -1) == kLuaTypeString)
        {
            std::size_t length = 0;
            const char* text = api.lua_tolstring(state, -1, &length);
            if (text)
                value.assign(text, length);
        }
        values.push_back(std::move(value));
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    return values;
}

bool parseLuaVehicleDefinitionV2(
    const LuaApi& api,
    lua_State* state,
    int definitionIndex,
    heritage::vehicles::VehicleDefinitionV2Source& source,
    std::string& errorMessage)
{
    errorMessage.clear();
    const int rootIndex = absoluteLuaIndex(api, state, definitionIndex);
    if (api.lua_type(state, rootIndex) != kLuaTypeTable)
    {
        errorMessage = "VehicleDefinitionV2 must be passed as a Lua table.";
        return false;
    }

    source.schemaVersion = static_cast<int>(luaFieldNumber(
        api, state, rootIndex, "schemaVersion", 0.0));
    source.id = luaFieldString(api, state, rootIndex, "id");
    source.displayName = luaFieldString(api, state, rootIndex, "displayName");
    source.classification = luaFieldString(
        api, state, rootIndex, "classification");

    if (pushLuaTableField(api, state, rootIndex, "presentation"))
    {
        source.bodyAsset = luaFieldString(api, state, -1, "bodyAsset");
        popLuaValues(api, state);
    }
    if (pushLuaTableField(api, state, rootIndex, "requirements"))
    {
        source.requirements.leanDynamics = luaFieldBoolean(
            api, state, -1, "leanDynamics", false);
        source.requirements.articulation = luaFieldBoolean(
            api, state, -1, "articulation", false);
        source.requirements.trackContacts = luaFieldBoolean(
            api, state, -1, "trackContacts", false);
        popLuaValues(api, state);
    }
    if (pushLuaTableField(api, state, rootIndex, "topologyIntent"))
    {
        source.driveLayoutIntent = luaFieldString(
            api, state, -1, "driveLayout");
        source.powerUnitPlacementIntent = luaFieldString(
            api, state, -1, "powerUnitPlacement");
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "bodies"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 64, "Body collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.bodies.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every body entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleBodyDefinition body;
            body.id = luaFieldString(api, state, -1, "id");
            body.role = luaFieldString(api, state, -1, "role");
            body.massKg = static_cast<float>(luaFieldNumber(
                api, state, -1, "massKg", 0.0));
            source.bodies.push_back(std::move(body));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "powerUnits"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 32, "Power-unit collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.powerUnits.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every power-unit entry must be a table.";
                return false;
            }
            heritage::vehicles::VehiclePowerUnitDefinition power;
            power.id = luaFieldString(api, state, -1, "id");
            power.kind = luaFieldString(api, state, -1, "kind");
            power.mountBody = luaFieldString(api, state, -1, "mountBody");
            power.location = luaFieldString(api, state, -1, "location");
            power.maximumTorqueNm = static_cast<float>(luaFieldNumber(
                api, state, -1, "maximumTorqueNm", 0.0));
            power.idleRpm = static_cast<float>(luaFieldNumber(
                api, state, -1, "idleRpm", 900.0));
            power.redlineRpm = static_cast<float>(luaFieldNumber(
                api, state, -1, "redlineRpm", 7000.0));
            power.engineBrakingTorqueNm = static_cast<float>(luaFieldNumber(
                api, state, -1, "engineBrakingTorqueNm", 70.0));
            source.powerUnits.push_back(std::move(power));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "transmissions"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 32, "Transmission collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.transmissions.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every transmission entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleTransmissionDefinition transmission;
            transmission.id = luaFieldString(api, state, -1, "id");
            transmission.kind = luaFieldString(api, state, -1, "kind");
            transmission.powerUnit = luaFieldString(
                api, state, -1, "powerUnit");
            transmission.reverseRatio = static_cast<float>(luaFieldNumber(
                api, state, -1, "reverseRatio", -3.20));
            transmission.forwardRatios = luaNumberArrayField(
                api, state, -1, "forwardRatios", 64);
            transmission.finalDriveRatio = static_cast<float>(luaFieldNumber(
                api, state, -1, "finalDriveRatio", 3.90));
            transmission.efficiency = static_cast<float>(luaFieldNumber(
                api, state, -1, "efficiency", 0.88));
            transmission.shiftDurationSeconds = static_cast<float>(luaFieldNumber(
                api, state, -1, "shiftDurationSeconds", 0.22));
            transmission.clutchEngagementRate = static_cast<float>(luaFieldNumber(
                api, state, -1, "clutchEngagementRate", 5.0));
            source.transmissions.push_back(std::move(transmission));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "suspensions"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 64, "Suspension collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.suspensions.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every suspension entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleSuspensionDefinition suspension;
            suspension.id = luaFieldString(api, state, -1, "id");
            suspension.provider = luaFieldString(api, state, -1, "provider");
            suspension.mountBody = luaFieldString(
                api, state, -1, "mountBody");
            suspension.restLengthM = static_cast<float>(luaFieldNumber(
                api, state, -1, "restLengthM", 0.50));
            suspension.maximumCompressionM = static_cast<float>(luaFieldNumber(
                api, state, -1, "maximumCompressionM", 0.18));
            suspension.maximumDroopM = static_cast<float>(luaFieldNumber(
                api, state, -1, "maximumDroopM", 0.15));
            suspension.springPreloadN = static_cast<float>(luaFieldNumber(
                api, state, -1, "springPreloadN", 0.0));
            suspension.springRateNPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "springRateNPerM", 35000.0));
            suspension.springProgressionNPerM2 = static_cast<float>(luaFieldNumber(
                api, state, -1, "springProgressionNPerM2", 0.0));
            suspension.bumpDampingNsPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "bumpDampingNsPerM", 3200.0));
            suspension.bumpHighSpeedDampingNsPerM = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "bumpHighSpeedDampingNsPerM",
                    suspension.bumpDampingNsPerM));
            suspension.bumpDampingKneeVelocityMps = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "bumpDampingKneeVelocityMps", 1.0));
            suspension.reboundDampingNsPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "reboundDampingNsPerM", 4200.0));
            suspension.reboundHighSpeedDampingNsPerM = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "reboundHighSpeedDampingNsPerM",
                    suspension.reboundDampingNsPerM));
            suspension.reboundDampingKneeVelocityMps = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "reboundDampingKneeVelocityMps", 1.0));
            suspension.bumpStopEngagementM = static_cast<float>(luaFieldNumber(
                api, state, -1, "bumpStopEngagementM",
                suspension.maximumCompressionM));
            suspension.bumpStopRateNPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "bumpStopRateNPerM", 0.0));
            suspension.bumpStopProgressionNPerM2 = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "bumpStopProgressionNPerM2", 0.0));
            suspension.droopStopEngagementM = static_cast<float>(luaFieldNumber(
                api, state, -1, "droopStopEngagementM",
                suspension.maximumDroopM));
            suspension.droopStopRateNPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "droopStopRateNPerM", 0.0));
            suspension.localSteeringAxis = luaFieldVector3(
                api, state, -1, "steeringAxis", { 0.0f, 1.0f, 0.0f });
            suspension.staticCamberDegrees = static_cast<float>(luaFieldNumber(
                api, state, -1, "staticCamberDegrees", 0.0));
            suspension.camberGainDegreesPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "camberGainDegreesPerM", 0.0));
            suspension.camberProgressionDegreesPerM2 = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "camberProgressionDegreesPerM2", 0.0));
            suspension.staticToeDegrees = static_cast<float>(luaFieldNumber(
                api, state, -1, "staticToeDegrees", 0.0));
            suspension.toeGainDegreesPerM = static_cast<float>(luaFieldNumber(
                api, state, -1, "toeGainDegreesPerM", 0.0));
            suspension.toeProgressionDegreesPerM2 = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "toeProgressionDegreesPerM2", 0.0));
            suspension.motionRatio = static_cast<float>(luaFieldNumber(
                api, state, -1, "motionRatio", 1.0));
            suspension.maximumForceN = static_cast<float>(luaFieldNumber(
                api, state, -1, "maximumForceN", 250000.0));
            source.suspensions.push_back(std::move(suspension));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "contactUnits"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 64, "Contact-unit collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.contactUnits.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every contact-unit entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleContactUnitDefinition contact;
            contact.id = luaFieldString(api, state, -1, "id");
            contact.kind = luaFieldString(api, state, -1, "kind");
            contact.mountBody = luaFieldString(api, state, -1, "mountBody");
            contact.axle = luaFieldString(api, state, -1, "axle");
            contact.localMount = luaFieldVector3(
                api, state, -1, "position", {});
            contact.suspensionDirection = luaFieldVector3(
                api, state, -1, "suspensionDirection", { 0.0f, -1.0f, 0.0f });
            contact.suspension = luaFieldString(
                api, state, -1, "suspension");
            contact.steering = luaFieldBoolean(
                api, state, -1, "steering", false);
            contact.serviceBrake = luaFieldBoolean(
                api, state, -1, "serviceBrake", true);
            contact.parkingBrake = luaFieldBoolean(
                api, state, -1, "parkingBrake", false);
            contact.tireProvider = luaFieldString(
                api, state, -1, "tireProvider");
            contact.radiusM = static_cast<float>(luaFieldNumber(
                api, state, -1, "radiusM", 0.35));
            contact.effectiveUnsprungMassKg = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "effectiveUnsprungMassKg", 0.0));
            contact.tireRadialStiffnessNPerM = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "tireRadialStiffnessNPerM", 220000.0));
            contact.tireRadialDampingNsPerM = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "tireRadialDampingNsPerM", 1800.0));
            contact.maximumTireDeflectionM = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "maximumTireDeflectionM", 0.08));
            contact.maximumTireNormalForceN = static_cast<float>(
                luaFieldNumber(
                    api, state, -1, "maximumTireNormalForceN", 250000.0));
            contact.serviceBrakeFactor = static_cast<float>(luaFieldNumber(
                api, state, -1, "serviceBrakeFactor", 0.25));
            contact.parkingBrakeFactor = static_cast<float>(luaFieldNumber(
                api, state, -1, "parkingBrakeFactor", 0.0));
            source.contactUnits.push_back(std::move(contact));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "driveConnections"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 32, "Drive-connection collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.driveConnections.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every drive-connection entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleDriveConnectionDefinition connection;
            connection.id = luaFieldString(api, state, -1, "id");
            connection.transmission = luaFieldString(
                api, state, -1, "transmission");
            connection.contactUnits = luaStringArrayField(
                api, state, -1, "contactUnits", 64);
            source.driveConnections.push_back(std::move(connection));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    return true;
}

} // namespace

bool LuaModuleRuntime::onLoad(
    GLFWwindow* window,
    const ModuleContext& context,
    const ModuleRuntimeServices& services,
    std::string& message)
{
    onShutdown();

    m_window = window;
    m_audio = services.audio;
    m_input = services.input;
    m_entities = services.entities;
    m_physics = services.physics;
    m_context.emplace(context);
    m_loaded = true;

    std::string saveWarning;
    if (!m_saveStore.initialize(context, saveWarning))
    {
        std::cerr << "[Save:" << context.module().id << "] "
            << saveWarning << '\n';
    }
    else if (!saveWarning.empty())
    {
        std::cerr << "[Save:" << context.module().id << "] "
            << saveWarning << " Starting with an empty save store.\n";
    }

    if (context.module().entryScript.empty())
    {
        setScriptError(
            "Module '" + context.module().id
            + "' requested runtime 'lua' but has no entry_script.");
        message = m_scriptError;
        return true;
    }

    m_scriptPath = context.resolveScriptPath(context.module().entryScript);
    if (m_scriptPath.empty())
    {
        setScriptError(
            "Module '" + context.module().id
            + "' has an unsafe entry_script path: "
            + context.module().entryScript);
        message = m_scriptError;
        return true;
    }

    std::string loadError;
    if (!createState(loadError) || !loadEntryScript(loadError))
    {
        setScriptError(loadError);
        message = m_scriptError;
        return true;
    }

    message.clear();
    return true;
}

void LuaModuleRuntime::onStart()
{
    if (!m_loaded)
        return;

    m_started = true;
    if (m_state && m_scriptError.empty())
        callOptionalNoArgs("OnStart");

    // Lua may choose a scene in OnStart. If it does not, the optional
    // manifest entry_scene becomes the initial scene.
    if (!m_pendingSceneId
        && m_sceneManager.activeSceneId().empty()
        && m_context
        && !m_context->module().scene.empty())
    {
        requestSceneLoad(m_context->module().scene, false);
    }

    processPendingSceneTransition();
}

void LuaModuleRuntime::onFixedUpdate(float fixedDeltaTime)
{
    if (m_started && m_state && m_scriptError.empty())
        callOptionalNumber("OnFixedUpdate", static_cast<LuaNumber>(fixedDeltaTime));
}

void LuaModuleRuntime::onUpdate(float deltaTime, bool allowInteraction)
{
    m_allowInteraction = allowInteraction;

    const bool f5Down = m_window
        && glfwGetKey(m_window, GLFW_KEY_F5) == GLFW_PRESS;
    const bool requestedManualReload = f5Down && !m_f5WasDown;
    m_f5WasDown = f5Down;

    const bool changedOnDisk = checkForScriptChange(deltaTime);
    if (requestedManualReload || changedOnDisk)
    {
        std::string reloadError;
        if (reloadScript(reloadError))
        {
            m_lastReloadMessage = "Lua module scripts reloaded from: " + m_scriptPath.parent_path().string();
            std::cout << m_lastReloadMessage << '\n';
        }
        else
        {
            setScriptError(reloadError);
            std::cerr << m_scriptError << '\n';
        }
    }

    processPendingSceneTransition();

    if (m_started)
        m_sceneManager.update(deltaTime, allowInteraction);

    if (m_started && m_state && m_scriptError.empty())
        callOptionalNumber("OnUpdate", static_cast<LuaNumber>(deltaTime));

    // Scene.Load() is queued. Applying it here prevents a script callback from
    // destroying the active scene while that scene is updating or drawing.
    processPendingSceneTransition();

    if (m_audio)
    {
        for (auto iterator = m_audioHandles.begin(); iterator != m_audioHandles.end(); )
        {
            if (!m_audio->isPlaying(*iterator))
                iterator = m_audioHandles.erase(iterator);
            else
                ++iterator;
        }
    }

    m_saveFlushTimer += (std::max)(0.0f, deltaTime);
    if (m_saveStore.isDirty() && m_saveFlushTimer >= 1.0f)
    {
        m_saveFlushTimer = 0.0f;
        if (!m_saveStore.flush())
        {
            std::cerr << "[Save:"
                << (m_context ? m_context->module().id : "?")
                << "] " << m_saveStore.lastError() << '\n';
        }
    }
}

heritage::math::Vec3 LuaModuleRuntime::clearColor() const
{
    return m_sceneManager.activeSceneId().empty()
        ? m_clearColor
        : m_sceneManager.clearColor();
}

void LuaModuleRuntime::onRender(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (m_started)
        m_sceneManager.draw(projection, videoSettings);
}

void LuaModuleRuntime::onDrawUI(
    int framebufferWidth,
    int framebufferHeight)
{
    m_framebufferWidth = (std::max)(framebufferWidth, 1);
    m_framebufferHeight = (std::max)(framebufferHeight, 1);

    if (!m_scriptError.empty() || !m_state)
    {
        drawRuntimeError(m_framebufferWidth, m_framebufferHeight);
        return;
    }

    // Scene overlays are drawn first so module Lua UI remains the top layer.
    m_sceneManager.drawOverlay(m_framebufferWidth, m_framebufferHeight);

    m_panelOpen = false;
    m_panelVisible = false;
    callOptionalTwoIntegers(
        "OnDrawUI",
        static_cast<LuaInteger>(m_framebufferWidth),
        static_cast<LuaInteger>(m_framebufferHeight));

    // A broken module script must not corrupt ImGui's Begin/End stack.
    closeOpenPanel();
}

void LuaModuleRuntime::onShutdown()
{
    destroyState(true);
    clearImportedStaticBoxScene();
    if (m_physics)
        m_physics->collisions().clearStaticSceneTriangles();

    if (m_audio)
    {
        for (const heritage::audio::AudioHandle handle : m_audioHandles)
            m_audio->stop(handle);
    }
    m_audioHandles.clear();
    m_lastAudioError.clear();
    m_uiImages.clear();
    m_lastUiError.clear();
    m_lastPrefabError.clear();
    m_lastPhysicsError.clear();

    if (m_saveStore.isDirty() && !m_saveStore.flush())
    {
        std::cerr << "[Save:"
            << (m_context ? m_context->module().id : "?")
            << "] " << m_saveStore.lastError() << '\n';
    }

    m_sceneManager.shutdown();
    m_api.unload();

    m_actions.clear();
    m_pendingSceneId.reset();
    m_pendingSceneReload = false;
    m_lastSceneError.clear();
    m_saveStore.reset();
    m_context.reset();
    m_window = nullptr;
    m_audio = nullptr;
    m_input = nullptr;
    m_entities = nullptr;
    m_physics = nullptr;
    m_scriptPath.clear();
    m_scriptError.clear();
    m_lastReloadMessage.clear();
    m_registeredLuaFunctions.clear();
    m_lastSafetyReport.clear();
    m_lastSafetyReportPath.clear();
    m_clearColor = { 0.003f, 0.005f, 0.008f };
    m_reloadCheckTimer = 0.0f;
    m_saveFlushTimer = 0.0f;
    m_loaded = false;
    m_started = false;
    m_allowInteraction = true;
    m_f5WasDown = false;
    m_framebufferWidth = 1;
    m_framebufferHeight = 1;
    m_numericSliderInputLabel.clear();
    m_numericSliderFocusRequested = false;
}

void LuaModuleRuntime::clearImportedStaticBoxScene()
{
    if (m_physics)
    {
        for (const heritage::physics::BodyHandle body : m_importedStaticSceneBodies)
        {
            if (body != heritage::physics::InvalidBody
                && m_physics->rigidBodies().exists(body))
            {
                m_physics->destroyBody(body);
            }
        }
    }

    if (m_entities)
    {
        for (const heritage::entities::EntityHandle entity : m_importedStaticSceneEntities)
        {
            if (entity != heritage::entities::InvalidEntity
                && m_entities->exists(entity))
            {
                m_entities->destroy(entity);
            }
        }
    }

    m_importedStaticSceneBodies.clear();
    m_importedStaticSceneEntities.clear();
}

bool LuaModuleRuntime::pollAction(ModuleRuntimeAction& action)
{
    if (m_actions.empty())
        return false;

    action = std::move(m_actions.front());
    m_actions.pop_front();
    return true;
}

std::string LuaModuleRuntime::activeContentId() const
{
    const std::string script = m_scriptPath.empty()
        ? "<none>"
        : m_scriptPath.string();
    const std::string scene = m_sceneManager.activeSceneId().empty()
        ? "<none>"
        : m_sceneManager.activeSceneId();
    return script + " | scene=" + scene;
}

bool LuaModuleRuntime::createState(std::string& errorMessage)
{
    if (!m_context)
    {
        errorMessage = "Lua runtime has no ModuleContext.";
        return false;
    }

    if (!m_api.load(m_context->projectRoot(), errorMessage))
        return false;

    m_state = m_api.luaL_newstate();
    if (!m_state)
    {
        errorMessage = "Lua could not allocate a new interpreter state.";
        return false;
    }

    g_runtimeByState[m_state] = this;
    m_api.luaL_openlibs(m_state);
    sandboxStandardLibraries();
    m_registeredLuaFunctions.clear();
    registerBindings();
    writeLuaApiManifest();

    errorMessage.clear();
    return true;
}

bool LuaModuleRuntime::loadEntryScript(std::string& errorMessage)
{
    if (!m_state)
    {
        errorMessage = "Lua state is not initialized.";
        return false;
    }

    if (!std::filesystem::is_regular_file(m_scriptPath))
    {
        errorMessage = "Lua entry script was not found:\n" + m_scriptPath.string();
        return false;
    }

    const int loadStatus = m_api.luaL_loadfilex(
        m_state,
        m_scriptPath.string().c_str(),
        "t"); // Text only: module bytecode is deliberately not accepted.
    if (loadStatus != kLuaOk)
    {
        errorMessage = "Lua syntax/load error in " + m_scriptPath.string()
            + ":\n" + stackString(-1);
        pop(1);
        return false;
    }

    if (!protectedCall(0, 0, "loading the entry script"))
    {
        errorMessage = m_scriptError;
        return false;
    }

    refreshScriptWatchSnapshot();
    clearScriptError();
    errorMessage.clear();
    return true;
}

bool LuaModuleRuntime::reloadScript(std::string& errorMessage)
{
    const bool restartLifecycle = m_started;
    destroyState(true);

    // A reloaded script must not lose control of sounds created by the old
    // state. Stop every module-owned voice before creating the new state.
    if (m_audio)
    {
        for (const heritage::audio::AudioHandle handle : m_audioHandles)
            m_audio->stop(handle);
    }
    m_audioHandles.clear();
    m_uiImages.clear();
    m_lastUiError.clear();

    clearScriptError();

    if (!createState(errorMessage) || !loadEntryScript(errorMessage))
        return false;

    if (restartLifecycle)
        callOptionalNoArgs("OnStart");

    errorMessage = m_scriptError;
    return m_scriptError.empty();
}

void LuaModuleRuntime::destroyState(bool callShutdownFunction)
{
    closeOpenPanel();

    if (!m_state)
        return;

    if (callShutdownFunction && m_started && m_scriptError.empty())
        callOptionalNoArgs("OnShutdown");

    g_runtimeByState.erase(m_state);
    m_api.lua_close(m_state);
    m_state = nullptr;
}

void LuaModuleRuntime::registerBindings()
{
    // Override base-library print with module-tagged engine logging.
    m_api.lua_pushcclosure(m_state, &LuaModuleRuntime::luaPrint, 0);
    m_api.lua_setglobal(m_state, "print");

    registerFunction("UI", "BeginPanel", &LuaModuleRuntime::luaUiBeginPanel);
    registerFunction("UI", "EndPanel", &LuaModuleRuntime::luaUiEndPanel);
    registerFunction("UI", "BeginTabBar", &LuaModuleRuntime::luaUiBeginTabBar);
    registerFunction("UI", "EndTabBar", &LuaModuleRuntime::luaUiEndTabBar);
    registerFunction("UI", "BeginTabItem", &LuaModuleRuntime::luaUiBeginTabItem);
    registerFunction("UI", "EndTabItem", &LuaModuleRuntime::luaUiEndTabItem);
    registerFunction("UI", "ModuleLabel", &LuaModuleRuntime::luaUiModuleLabel);
    registerFunction("UI", "Title", &LuaModuleRuntime::luaUiTitle);
    registerFunction("UI", "Subtitle", &LuaModuleRuntime::luaUiSubtitle);
    registerFunction("UI", "Text", &LuaModuleRuntime::luaUiText);
    registerFunction("UI", "TextWrapped", &LuaModuleRuntime::luaUiTextWrapped);
    registerFunction("UI", "TextDisabled", &LuaModuleRuntime::luaUiTextDisabled);
    registerFunction("UI", "Separator", &LuaModuleRuntime::luaUiSeparator);
    registerFunction("UI", "Spacing", &LuaModuleRuntime::luaUiSpacing);
    registerFunction("UI", "SameLine", &LuaModuleRuntime::luaUiSameLine);
    registerFunction("UI", "GetAvailableWidth", &LuaModuleRuntime::luaUiGetAvailableWidth);
    registerFunction("UI", "Button", &LuaModuleRuntime::luaUiButton);
    registerFunction("UI", "SliderFloat", &LuaModuleRuntime::luaUiSliderFloat);
    registerFunction("UI", "Checkbox", &LuaModuleRuntime::luaUiCheckbox);
    registerFunction("UI", "InputInt", &LuaModuleRuntime::luaUiInputInt);
    registerFunction("UI", "InputText", &LuaModuleRuntime::luaUiInputText);
    registerFunction("UI", "Image", &LuaModuleRuntime::luaUiImage);
    registerFunction("UI", "ImageButton", &LuaModuleRuntime::luaUiImageButton);
    registerFunction("UI", "GetImageSize", &LuaModuleRuntime::luaUiGetImageSize);
    registerFunction("UI", "UnloadImage", &LuaModuleRuntime::luaUiUnloadImage);
    registerFunction("UI", "GetLastError", &LuaModuleRuntime::luaUiGetLastError);
    registerFunction("UI", "SetCursorPos", &LuaModuleRuntime::luaUiSetCursorPos);
    registerFunction("UI", "GetCursorPos", &LuaModuleRuntime::luaUiGetCursorPos);
    registerFunction("UI", "Dummy", &LuaModuleRuntime::luaUiDummy);
    registerFunction("UI", "TextColored", &LuaModuleRuntime::luaUiTextColored);
    registerFunction("UI", "ProgressBar", &LuaModuleRuntime::luaUiProgressBar);
    registerFunction("UI", "PlotLines", &LuaModuleRuntime::luaUiPlotLines);

    registerFunction("Engine", "OpenSettings", &LuaModuleRuntime::luaEngineOpenSettings);
    registerFunction("Engine", "Exit", &LuaModuleRuntime::luaEngineExit);
    registerFunction("Engine", "SetClearColor", &LuaModuleRuntime::luaEngineSetClearColor);
    registerFunction("Engine", "Log", &LuaModuleRuntime::luaEngineLog);
    registerFunction("Engine", "GetBuildIdentity", &LuaModuleRuntime::luaEngineGetBuildIdentity);
    registerFunction("Engine", "GetBuildStep", &LuaModuleRuntime::luaEngineGetBuildStep);
    registerFunction("Engine", "GetGitCommit", &LuaModuleRuntime::luaEngineGetGitCommit);
    registerFunction("Engine", "GetBuildConfiguration", &LuaModuleRuntime::luaEngineGetBuildConfiguration);
    registerFunction("Engine", "GetLuaApiCount", &LuaModuleRuntime::luaEngineGetLuaApiCount);
    registerFunction("Engine", "GetLuaApiName", &LuaModuleRuntime::luaEngineGetLuaApiName);
    registerFunction("Engine", "DumpLuaAPI", &LuaModuleRuntime::luaEngineDumpLuaAPI);
    registerFunction("Engine", "RunSafetySmokeTests", &LuaModuleRuntime::luaEngineRunSafetySmokeTests);
    registerFunction("Engine", "GetLastSafetyReport", &LuaModuleRuntime::luaEngineGetLastSafetyReport);

    registerFunction("Script", "Include", &LuaModuleRuntime::luaScriptInclude);

    registerFunction("Scene", "Load", &LuaModuleRuntime::luaSceneLoad);
    registerFunction("Scene", "Reload", &LuaModuleRuntime::luaSceneReload);
    registerFunction("Scene", "GetCurrent", &LuaModuleRuntime::luaSceneGetCurrent);
    registerFunction("Scene", "Exists", &LuaModuleRuntime::luaSceneExists);
    registerFunction("Scene", "GetLastError", &LuaModuleRuntime::luaSceneGetLastError);

    registerFunction("Save", "GetString", &LuaModuleRuntime::luaSaveGetString);
    registerFunction("Save", "SetString", &LuaModuleRuntime::luaSaveSetString);
    registerFunction("Save", "GetInt", &LuaModuleRuntime::luaSaveGetInt);
    registerFunction("Save", "SetInt", &LuaModuleRuntime::luaSaveSetInt);
    registerFunction("Save", "GetNumber", &LuaModuleRuntime::luaSaveGetNumber);
    registerFunction("Save", "SetNumber", &LuaModuleRuntime::luaSaveSetNumber);
    registerFunction("Save", "GetBool", &LuaModuleRuntime::luaSaveGetBool);
    registerFunction("Save", "SetBool", &LuaModuleRuntime::luaSaveSetBool);
    registerFunction("Save", "Has", &LuaModuleRuntime::luaSaveHas);
    registerFunction("Save", "Remove", &LuaModuleRuntime::luaSaveRemove);
    registerFunction("Save", "Clear", &LuaModuleRuntime::luaSaveClear);
    registerFunction("Save", "Flush", &LuaModuleRuntime::luaSaveFlush);
    registerFunction("Save", "GetPath", &LuaModuleRuntime::luaSaveGetPath);
    registerFunction("Save", "GetLastError", &LuaModuleRuntime::luaSaveGetLastError);
    registerFunction("Save", "IsDirty", &LuaModuleRuntime::luaSaveIsDirty);

    registerFunction("Audio", "IsAvailable", &LuaModuleRuntime::luaAudioIsAvailable);
    registerFunction("Audio", "GetBackend", &LuaModuleRuntime::luaAudioGetBackend);
    registerFunction("Audio", "PlaySound", &LuaModuleRuntime::luaAudioPlaySound);
    registerFunction("Audio", "PlayLoop", &LuaModuleRuntime::luaAudioPlayLoop);
    registerFunction("Audio", "Stop", &LuaModuleRuntime::luaAudioStop);
    registerFunction("Audio", "StopAll", &LuaModuleRuntime::luaAudioStopAll);
    registerFunction("Audio", "IsPlaying", &LuaModuleRuntime::luaAudioIsPlaying);
    registerFunction("Audio", "SetVolume", &LuaModuleRuntime::luaAudioSetVolume);
    registerFunction("Audio", "SetPitch", &LuaModuleRuntime::luaAudioSetPitch);
    registerFunction("Audio", "SetMasterVolume", &LuaModuleRuntime::luaAudioSetMasterVolume);
    registerFunction("Audio", "GetMasterVolume", &LuaModuleRuntime::luaAudioGetMasterVolume);
    registerFunction("Audio", "SetBusVolume", &LuaModuleRuntime::luaAudioSetBusVolume);
    registerFunction("Audio", "GetBusVolume", &LuaModuleRuntime::luaAudioGetBusVolume);
    registerFunction("Audio", "GetLastError", &LuaModuleRuntime::luaAudioGetLastError);

    registerFunction("Input", "IsAvailable", &LuaModuleRuntime::luaInputIsAvailable);
    registerFunction("Input", "RegisterAction", &LuaModuleRuntime::luaInputRegisterAction);
    registerFunction("Input", "Down", &LuaModuleRuntime::luaInputDown);
    registerFunction("Input", "Pressed", &LuaModuleRuntime::luaInputPressed);
    registerFunction("Input", "Released", &LuaModuleRuntime::luaInputReleased);
    registerFunction("Input", "Value", &LuaModuleRuntime::luaInputValue);
    registerFunction("Input", "GetBinding", &LuaModuleRuntime::luaInputGetBinding);
    registerFunction("Input", "GetBindingCount", &LuaModuleRuntime::luaInputGetBindingCount);
    registerFunction("Input", "GetBindingAt", &LuaModuleRuntime::luaInputGetBindingAt);
    registerFunction("Input", "Bind", &LuaModuleRuntime::luaInputBind);
    registerFunction("Input", "AddBinding", &LuaModuleRuntime::luaInputAddBinding);
    registerFunction("Input", "RemoveBinding", &LuaModuleRuntime::luaInputRemoveBinding);
    registerFunction("Input", "ResetBinding", &LuaModuleRuntime::luaInputResetBinding);
    registerFunction("Input", "ResetBindings", &LuaModuleRuntime::luaInputResetBindings);
    registerFunction("Input", "KeyDown", &LuaModuleRuntime::luaInputKeyDown);
    registerFunction("Input", "KeyPressed", &LuaModuleRuntime::luaInputKeyPressed);
    registerFunction("Input", "KeyReleased", &LuaModuleRuntime::luaInputKeyReleased);
    registerFunction("Input", "MouseDown", &LuaModuleRuntime::luaInputMouseDown);
    registerFunction("Input", "MousePressed", &LuaModuleRuntime::luaInputMousePressed);
    registerFunction("Input", "MouseReleased", &LuaModuleRuntime::luaInputMouseReleased);
    registerFunction("Input", "MouseDelta", &LuaModuleRuntime::luaInputMouseDelta);
    registerFunction("Input", "GamepadConnected", &LuaModuleRuntime::luaInputGamepadConnected);
    registerFunction("Input", "GetGamepadName", &LuaModuleRuntime::luaInputGetGamepadName);
    registerFunction("Input", "GetLastError", &LuaModuleRuntime::luaInputGetLastError);

    registerFunction("Physics", "IsAvailable", &LuaModuleRuntime::luaPhysicsIsAvailable);
    registerFunction("Physics", "GetFixedDelta", &LuaModuleRuntime::luaPhysicsGetFixedDelta);
    registerFunction("Physics", "GetTickRate", &LuaModuleRuntime::luaPhysicsGetTickRate);
    registerFunction("Physics", "SetTickRate", &LuaModuleRuntime::luaPhysicsSetTickRate);
    registerFunction("Physics", "GetGravity", &LuaModuleRuntime::luaPhysicsGetGravity);
    registerFunction("Physics", "SetGravity", &LuaModuleRuntime::luaPhysicsSetGravity);
    registerFunction("Physics", "IsPaused", &LuaModuleRuntime::luaPhysicsIsPaused);
    registerFunction("Physics", "SetPaused", &LuaModuleRuntime::luaPhysicsSetPaused);
    registerFunction("Physics", "RequestSingleStep", &LuaModuleRuntime::luaPhysicsRequestSingleStep);
    registerFunction("Physics", "GetTimeScale", &LuaModuleRuntime::luaPhysicsGetTimeScale);
    registerFunction("Physics", "SetTimeScale", &LuaModuleRuntime::luaPhysicsSetTimeScale);
    registerFunction("Physics", "GetStepCount", &LuaModuleRuntime::luaPhysicsGetStepCount);
    registerFunction("Physics", "GetSimulationTime", &LuaModuleRuntime::luaPhysicsGetSimulationTime);
    registerFunction("Physics", "GetInterpolationAlpha", &LuaModuleRuntime::luaPhysicsGetInterpolationAlpha);
    registerFunction("Physics", "GetLastSubstepCount", &LuaModuleRuntime::luaPhysicsGetLastSubstepCount);
    registerFunction("Physics", "GetLastWorldStepCount", &LuaModuleRuntime::luaPhysicsGetLastSubstepCount);
    registerFunction("Physics", "GetMaximumWorldStepsPerFrame", &LuaModuleRuntime::luaPhysicsGetMaximumWorldStepsPerFrame);
    registerFunction("Physics", "GetPendingWorldStepCount", &LuaModuleRuntime::luaPhysicsGetPendingWorldStepCount);
    registerFunction("Physics", "GetBacklogTime", &LuaModuleRuntime::luaPhysicsGetBacklogTime);
    registerFunction("Physics", "GetPeakBacklogTime", &LuaModuleRuntime::luaPhysicsGetPeakBacklogTime);
    registerFunction("Physics", "WasOverloadedLastFrame", &LuaModuleRuntime::luaPhysicsWasOverloadedLastFrame);
    registerFunction("Physics", "GetOverloadFrameCount", &LuaModuleRuntime::luaPhysicsGetOverloadFrameCount);
    registerFunction("Physics", "GetDroppedTime", &LuaModuleRuntime::luaPhysicsGetDroppedTime);
    registerFunction("Physics", "GetClampedTime", &LuaModuleRuntime::luaPhysicsGetClampedTime);
    registerFunction("Physics", "ResetClock", &LuaModuleRuntime::luaPhysicsResetClock);

    registerFunction("Physics", "CreateBody", &LuaModuleRuntime::luaPhysicsCreateBody);
    registerFunction("Physics", "DestroyBody", &LuaModuleRuntime::luaPhysicsDestroyBody);
    registerFunction("Physics", "BodyExists", &LuaModuleRuntime::luaPhysicsBodyExists);
    registerFunction("Physics", "GetBodyCount", &LuaModuleRuntime::luaPhysicsGetBodyCount);
    registerFunction("Physics", "GetSleepingBodyCount", &LuaModuleRuntime::luaPhysicsGetSleepingBodyCount);
    registerFunction("Physics", "GetActiveDynamicBodyCount", &LuaModuleRuntime::luaPhysicsGetActiveDynamicBodyCount);
    registerFunction("Physics", "FindBodyByEntity", &LuaModuleRuntime::luaPhysicsFindBodyByEntity);
    registerFunction("Physics", "GetBodyEntity", &LuaModuleRuntime::luaPhysicsGetBodyEntity);
    registerFunction("Physics", "GetBodyMotionType", &LuaModuleRuntime::luaPhysicsGetBodyMotionType);
    registerFunction("Physics", "SetBodyMotionType", &LuaModuleRuntime::luaPhysicsSetBodyMotionType);
    registerFunction("Physics", "GetBodyMass", &LuaModuleRuntime::luaPhysicsGetBodyMass);
    registerFunction("Physics", "SetBodyMass", &LuaModuleRuntime::luaPhysicsSetBodyMass);
    registerFunction("Physics", "GetBodyGravityFactor", &LuaModuleRuntime::luaPhysicsGetBodyGravityFactor);
    registerFunction("Physics", "SetBodyGravityFactor", &LuaModuleRuntime::luaPhysicsSetBodyGravityFactor);
    registerFunction("Physics", "GetBodyLinearDamping", &LuaModuleRuntime::luaPhysicsGetBodyLinearDamping);
    registerFunction("Physics", "SetBodyLinearDamping", &LuaModuleRuntime::luaPhysicsSetBodyLinearDamping);
    registerFunction("Physics", "GetBodyAngularDamping", &LuaModuleRuntime::luaPhysicsGetBodyAngularDamping);
    registerFunction("Physics", "SetBodyAngularDamping", &LuaModuleRuntime::luaPhysicsSetBodyAngularDamping);
    registerFunction("Physics", "GetBodyContinuousCollision", &LuaModuleRuntime::luaPhysicsGetBodyContinuousCollision);
    registerFunction("Physics", "SetBodyContinuousCollision", &LuaModuleRuntime::luaPhysicsSetBodyContinuousCollision);
    registerFunction("Physics", "GetBodyPosition", &LuaModuleRuntime::luaPhysicsGetBodyPosition);
    registerFunction("Physics", "SetBodyPosition", &LuaModuleRuntime::luaPhysicsSetBodyPosition);
    registerFunction("Physics", "GetBodyRotation", &LuaModuleRuntime::luaPhysicsGetBodyRotation);
    registerFunction("Physics", "SetBodyRotation", &LuaModuleRuntime::luaPhysicsSetBodyRotation);
    registerFunction("Physics", "GetBodyLinearVelocity", &LuaModuleRuntime::luaPhysicsGetBodyLinearVelocity);
    registerFunction("Physics", "SetBodyLinearVelocity", &LuaModuleRuntime::luaPhysicsSetBodyLinearVelocity);
    registerFunction("Physics", "GetBodyAngularVelocity", &LuaModuleRuntime::luaPhysicsGetBodyAngularVelocity);
    registerFunction("Physics", "SetBodyAngularVelocity", &LuaModuleRuntime::luaPhysicsSetBodyAngularVelocity);
    registerFunction("Physics", "ApplyBodyForce", &LuaModuleRuntime::luaPhysicsApplyBodyForce);
    registerFunction("Physics", "ApplyBodyImpulse", &LuaModuleRuntime::luaPhysicsApplyBodyImpulse);
    registerFunction("Physics", "ApplyBodyImpulseAtPoint", &LuaModuleRuntime::luaPhysicsApplyBodyImpulseAtPoint);
    registerFunction("Physics", "ApplyBodyAngularImpulse", &LuaModuleRuntime::luaPhysicsApplyBodyAngularImpulse);
    registerFunction("Physics", "ClearBodyForces", &LuaModuleRuntime::luaPhysicsClearBodyForces);
    registerFunction("Physics", "IsBodySleeping", &LuaModuleRuntime::luaPhysicsIsBodySleeping);
    registerFunction("Physics", "SetBodySleeping", &LuaModuleRuntime::luaPhysicsSetBodySleeping);
    registerFunction("Physics", "GetBodyAllowSleep", &LuaModuleRuntime::luaPhysicsGetBodyAllowSleep);
    registerFunction("Physics", "SetBodyAllowSleep", &LuaModuleRuntime::luaPhysicsSetBodyAllowSleep);
    registerFunction("Physics", "WakeBody", &LuaModuleRuntime::luaPhysicsWakeBody);

    registerFunction("Physics", "CreateSphereCollider", &LuaModuleRuntime::luaPhysicsCreateSphereCollider);
    registerFunction("Physics", "CreateBoxCollider", &LuaModuleRuntime::luaPhysicsCreateBoxCollider);
    registerFunction("Physics", "LoadStaticBoxScene", &LuaModuleRuntime::luaPhysicsLoadStaticBoxScene);
    registerFunction("Physics", "UnloadStaticBoxScene", &LuaModuleRuntime::luaPhysicsUnloadStaticBoxScene);
    registerFunction("Physics", "LoadStaticTriangleScene", &LuaModuleRuntime::luaPhysicsLoadStaticTriangleScene);
    registerFunction("Physics", "UnloadStaticTriangleScene", &LuaModuleRuntime::luaPhysicsUnloadStaticTriangleScene);
    registerFunction("Physics", "GetStaticTriangleSceneCount", &LuaModuleRuntime::luaPhysicsGetStaticTriangleSceneCount);
    registerFunction("Physics", "GetStaticBoxSceneCount", &LuaModuleRuntime::luaPhysicsGetStaticBoxSceneCount);
    registerFunction("Physics", "DestroyCollider", &LuaModuleRuntime::luaPhysicsDestroyCollider);
    registerFunction("Physics", "ColliderExists", &LuaModuleRuntime::luaPhysicsColliderExists);
    registerFunction("Physics", "GetColliderCount", &LuaModuleRuntime::luaPhysicsGetColliderCount);
    registerFunction("Physics", "GetBodyColliderCount", &LuaModuleRuntime::luaPhysicsGetBodyColliderCount);
    registerFunction("Physics", "GetColliderBody", &LuaModuleRuntime::luaPhysicsGetColliderBody);
    registerFunction("Physics", "GetColliderShape", &LuaModuleRuntime::luaPhysicsGetColliderShape);
    registerFunction("Physics", "SetColliderMaterial", &LuaModuleRuntime::luaPhysicsSetColliderMaterial);
    registerFunction("Physics", "SetColliderSurface", &LuaModuleRuntime::luaPhysicsSetColliderSurface);
    registerFunction("Physics", "GetColliderSurface", &LuaModuleRuntime::luaPhysicsGetColliderSurface);
    registerFunction("Physics", "SetColliderTrigger", &LuaModuleRuntime::luaPhysicsSetColliderTrigger);
    registerFunction("Physics", "SetColliderFilter", &LuaModuleRuntime::luaPhysicsSetColliderFilter);
    registerFunction("Physics", "Raycast", &LuaModuleRuntime::luaPhysicsRaycast);
    registerFunction("Physics", "RaycastAny", &LuaModuleRuntime::luaPhysicsRaycastAny);
    registerFunction("Physics", "SphereCast", &LuaModuleRuntime::luaPhysicsSphereCast);
    registerFunction("Physics", "SphereCastAny", &LuaModuleRuntime::luaPhysicsSphereCastAny);
    registerFunction("Physics", "OverlapSphereCount", &LuaModuleRuntime::luaPhysicsOverlapSphereCount);
    registerFunction("Physics", "GetLastQueryCandidateCount", &LuaModuleRuntime::luaPhysicsGetLastQueryCandidateCount);
    registerFunction("Physics", "GetLastQueryExactTestCount", &LuaModuleRuntime::luaPhysicsGetLastQueryExactTestCount);
    registerFunction("Physics", "GetContactCount", &LuaModuleRuntime::luaPhysicsGetContactCount);
    registerFunction("Physics", "GetBodyContactCount", &LuaModuleRuntime::luaPhysicsGetBodyContactCount);
    registerFunction("Physics", "IsBodyTouching", &LuaModuleRuntime::luaPhysicsIsBodyTouching);
    registerFunction("Physics", "GetBroadphaseCandidateCount", &LuaModuleRuntime::luaPhysicsGetBroadphaseCandidateCount);
    registerFunction("Physics", "GetNarrowphaseTestCount", &LuaModuleRuntime::luaPhysicsGetNarrowphaseTestCount);
    registerFunction("Physics", "GetResolvedContactCount", &LuaModuleRuntime::luaPhysicsGetResolvedContactCount);
    registerFunction("Physics", "GetSimulationIslandCount", &LuaModuleRuntime::luaPhysicsGetSimulationIslandCount);
    registerFunction("Physics", "GetActiveIslandCount", &LuaModuleRuntime::luaPhysicsGetActiveIslandCount);
    registerFunction("Physics", "GetSleepingIslandCount", &LuaModuleRuntime::luaPhysicsGetSleepingIslandCount);
    registerFunction("Physics", "GetWarmStartedContactCount", &LuaModuleRuntime::luaPhysicsGetWarmStartedContactCount);
    registerFunction("Physics", "GetPersistentContactCount", &LuaModuleRuntime::luaPhysicsGetPersistentContactCount);
    registerFunction("Physics", "GetContinuousCollisionBodyCount", &LuaModuleRuntime::luaPhysicsGetContinuousCollisionBodyCount);
    registerFunction("Physics", "GetContinuousCollisionSweepCount", &LuaModuleRuntime::luaPhysicsGetContinuousCollisionSweepCount);
    registerFunction("Physics", "GetContinuousCollisionHitCount", &LuaModuleRuntime::luaPhysicsGetContinuousCollisionHitCount);
    registerFunction("Physics", "GetContinuousCollisionClampedBodyCount", &LuaModuleRuntime::luaPhysicsGetContinuousCollisionClampedBodyCount);
    registerFunction("Physics", "GetContinuousCollisionUnsupportedBodyCount", &LuaModuleRuntime::luaPhysicsGetContinuousCollisionUnsupportedBodyCount);

    registerFunction("Physics", "CreateSpringConstraint", &LuaModuleRuntime::luaPhysicsCreateSpringConstraint);
    registerFunction("Physics", "DestroyConstraint", &LuaModuleRuntime::luaPhysicsDestroyConstraint);
    registerFunction("Physics", "ConstraintExists", &LuaModuleRuntime::luaPhysicsConstraintExists);
    registerFunction("Physics", "GetConstraintCount", &LuaModuleRuntime::luaPhysicsGetConstraintCount);
    registerFunction("Physics", "GetEnabledConstraintCount", &LuaModuleRuntime::luaPhysicsGetEnabledConstraintCount);
    registerFunction("Physics", "GetActiveConstraintCount", &LuaModuleRuntime::luaPhysicsGetActiveConstraintCount);
    registerFunction("Physics", "SetConstraintEnabled", &LuaModuleRuntime::luaPhysicsSetConstraintEnabled);
    registerFunction("Physics", "GetConstraintEnabled", &LuaModuleRuntime::luaPhysicsGetConstraintEnabled);
    registerFunction("Physics", "SetSpringConstraintProperties", &LuaModuleRuntime::luaPhysicsSetSpringConstraintProperties);
    registerFunction("Physics", "GetSpringConstraintState", &LuaModuleRuntime::luaPhysicsGetSpringConstraintState);
    registerFunction("Physics", "GetConstraintBodyA", &LuaModuleRuntime::luaPhysicsGetConstraintBodyA);
    registerFunction("Physics", "GetConstraintBodyB", &LuaModuleRuntime::luaPhysicsGetConstraintBodyB);

    registerFunction("Physics", "GetLastError", &LuaModuleRuntime::luaPhysicsGetLastError);

    registerFunction("Vehicle", "IsAvailable", &LuaModuleRuntime::luaVehicleIsAvailable);
    registerFunction("Vehicle", "CompileDefinitionV2", &LuaModuleRuntime::luaVehicleCompileDefinitionV2);
    registerFunction("Vehicle", "CreateFromDefinitionV2", &LuaModuleRuntime::luaVehicleCreateFromDefinitionV2);
    registerFunction("Vehicle", "Create", &LuaModuleRuntime::luaVehicleCreate);
    registerFunction("Vehicle", "Destroy", &LuaModuleRuntime::luaVehicleDestroy);
    registerFunction("Vehicle", "Exists", &LuaModuleRuntime::luaVehicleExists);
    registerFunction("Vehicle", "GetCount", &LuaModuleRuntime::luaVehicleGetCount);
    registerFunction("Vehicle", "AddWheel", &LuaModuleRuntime::luaVehicleAddWheel);
    registerFunction("Vehicle", "GetWheelCount", &LuaModuleRuntime::luaVehicleGetWheelCount);
    registerFunction("Vehicle", "SetWheelSuspensionModel", &LuaModuleRuntime::luaVehicleSetWheelSuspensionModel);
    registerFunction("Vehicle", "GetWheelSuspensionModel", &LuaModuleRuntime::luaVehicleGetWheelSuspensionModel);
    registerFunction("Vehicle", "SetWheelSuspensionGeometry", &LuaModuleRuntime::luaVehicleSetWheelSuspensionGeometry);
    registerFunction("Vehicle", "GetWheelSuspensionGeometry", &LuaModuleRuntime::luaVehicleGetWheelSuspensionGeometry);
    registerFunction("Vehicle", "SetWheelUnsprungMassModel", &LuaModuleRuntime::luaVehicleSetWheelUnsprungMassModel);
    registerFunction("Vehicle", "GetWheelUnsprungMassModel", &LuaModuleRuntime::luaVehicleGetWheelUnsprungMassModel);
    registerFunction("Vehicle", "SetInputs", &LuaModuleRuntime::luaVehicleSetInputs);
    registerFunction("Vehicle", "SetWheelBrakeFactors", &LuaModuleRuntime::luaVehicleSetWheelBrakeFactors);
    registerFunction("Vehicle", "SetDriverAids", &LuaModuleRuntime::luaVehicleSetDriverAids);
    registerFunction("Vehicle", "GetDriverAidState", &LuaModuleRuntime::luaVehicleGetDriverAidState);
    registerFunction("Vehicle", "SetTuning", &LuaModuleRuntime::luaVehicleSetTuning);
    registerFunction("Vehicle", "SetTireModel", &LuaModuleRuntime::luaVehicleSetTireModel);
    registerFunction("Vehicle", "SetWheelTireModel", &LuaModuleRuntime::luaVehicleSetWheelTireModel);
    registerFunction("Vehicle", "GetWheelTireModel", &LuaModuleRuntime::luaVehicleGetWheelTireModel);
    registerFunction("Vehicle", "SetSurfacePreset", &LuaModuleRuntime::luaVehicleSetSurfacePreset);
    registerFunction("Vehicle", "GetSurfacePreset", &LuaModuleRuntime::luaVehicleGetSurfacePreset);
    registerFunction("Vehicle", "SetHighRateHertz", &LuaModuleRuntime::luaVehicleSetHighRateHertz);
    registerFunction("Vehicle", "SetSteeringGeometry", &LuaModuleRuntime::luaVehicleSetSteeringGeometry);
    registerFunction("Vehicle", "GetSteeringState", &LuaModuleRuntime::luaVehicleGetSteeringState);
    registerFunction("Vehicle", "SetPowertrain", &LuaModuleRuntime::luaVehicleSetPowertrain);
    registerFunction("Vehicle", "SetGearRatios", &LuaModuleRuntime::luaVehicleSetGearRatios);
    registerFunction("Vehicle", "SetDifferential", &LuaModuleRuntime::luaVehicleSetDifferential);
    registerFunction("Vehicle", "SetGear", &LuaModuleRuntime::luaVehicleSetGear);
    registerFunction("Vehicle", "ShiftUp", &LuaModuleRuntime::luaVehicleShiftUp);
    registerFunction("Vehicle", "ShiftDown", &LuaModuleRuntime::luaVehicleShiftDown);
    registerFunction("Vehicle", "GetDrivetrainState", &LuaModuleRuntime::luaVehicleGetDrivetrainState);
    registerFunction("Vehicle", "GetForwardGearCount", &LuaModuleRuntime::luaVehicleGetForwardGearCount);
    registerFunction("Vehicle", "GetHighRateHertz", &LuaModuleRuntime::luaVehicleGetHighRateHertz);
    registerFunction("Vehicle", "GetSpeed", &LuaModuleRuntime::luaVehicleGetSpeed);
    registerFunction("Vehicle", "GetGroundedWheelCount", &LuaModuleRuntime::luaVehicleGetGroundedWheelCount);
    registerFunction("Vehicle", "GetLastHighRateStepCount", &LuaModuleRuntime::luaVehicleGetLastHighRateStepCount);
    registerFunction("Vehicle", "GetTotalHighRateStepCount", &LuaModuleRuntime::luaVehicleGetTotalHighRateStepCount);
    registerFunction("Vehicle", "StartDynamicsLab", &LuaModuleRuntime::luaVehicleStartDynamicsLab);
    registerFunction("Vehicle", "StopDynamicsLab", &LuaModuleRuntime::luaVehicleStopDynamicsLab);
    registerFunction("Vehicle", "ClearDynamicsLab", &LuaModuleRuntime::luaVehicleClearDynamicsLab);
    registerFunction("Vehicle", "GetDynamicsLabSummary", &LuaModuleRuntime::luaVehicleGetDynamicsLabSummary);
    registerFunction("Vehicle", "GetDynamicsLabSeries", &LuaModuleRuntime::luaVehicleGetDynamicsLabSeries);
    registerFunction("Vehicle", "ExportDynamicsLabCsv", &LuaModuleRuntime::luaVehicleExportDynamicsLabCsv);
    registerFunction("Vehicle", "GetWheelState", &LuaModuleRuntime::luaVehicleGetWheelState);
    registerFunction(
        "Vehicle",
        "GetWheelContactDiagnostic",
        &LuaModuleRuntime::luaVehicleGetWheelContactDiagnostic);
    registerFunction("Vehicle", "GetWheelUprightPose", &LuaModuleRuntime::luaVehicleGetWheelUprightPose);
    registerFunction("Vehicle", "GetLastError", &LuaModuleRuntime::luaVehicleGetLastError);

    registerFunction("Entity", "IsAvailable", &LuaModuleRuntime::luaEntityIsAvailable);
    registerFunction("Entity", "Create", &LuaModuleRuntime::luaEntityCreate);
    registerFunction("Entity", "Destroy", &LuaModuleRuntime::luaEntityDestroy);
    registerFunction("Entity", "Exists", &LuaModuleRuntime::luaEntityExists);
    registerFunction("Entity", "Count", &LuaModuleRuntime::luaEntityCount);
    registerFunction("Entity", "GetPersistentId", &LuaModuleRuntime::luaEntityGetPersistentId);
    registerFunction("Entity", "FindByName", &LuaModuleRuntime::luaEntityFindByName);
    registerFunction("Entity", "SetName", &LuaModuleRuntime::luaEntitySetName);
    registerFunction("Entity", "GetName", &LuaModuleRuntime::luaEntityGetName);
    registerFunction("Entity", "AddTag", &LuaModuleRuntime::luaEntityAddTag);
    registerFunction("Entity", "RemoveTag", &LuaModuleRuntime::luaEntityRemoveTag);
    registerFunction("Entity", "HasTag", &LuaModuleRuntime::luaEntityHasTag);
    registerFunction("Entity", "FindFirstWithTag", &LuaModuleRuntime::luaEntityFindFirstWithTag);
    registerFunction("Entity", "SetParent", &LuaModuleRuntime::luaEntitySetParent);
    registerFunction("Entity", "ClearParent", &LuaModuleRuntime::luaEntityClearParent);
    registerFunction("Entity", "GetParent", &LuaModuleRuntime::luaEntityGetParent);
    registerFunction("Entity", "GetChildCount", &LuaModuleRuntime::luaEntityGetChildCount);
    registerFunction("Entity", "GetChildAt", &LuaModuleRuntime::luaEntityGetChildAt);
    registerFunction("Entity", "IsDescendantOf", &LuaModuleRuntime::luaEntityIsDescendantOf);
    registerFunction("Entity", "SetPosition", &LuaModuleRuntime::luaEntitySetPosition);
    registerFunction("Entity", "GetPosition", &LuaModuleRuntime::luaEntityGetPosition);
    registerFunction("Entity", "SetLocalPosition", &LuaModuleRuntime::luaEntitySetPosition);
    registerFunction("Entity", "GetLocalPosition", &LuaModuleRuntime::luaEntityGetPosition);
    registerFunction("Entity", "SetWorldPosition", &LuaModuleRuntime::luaEntitySetWorldPosition);
    registerFunction("Entity", "GetWorldPosition", &LuaModuleRuntime::luaEntityGetWorldPosition);
    registerFunction("Entity", "SetRotation", &LuaModuleRuntime::luaEntitySetRotation);
    registerFunction("Entity", "GetRotation", &LuaModuleRuntime::luaEntityGetRotation);
    registerFunction("Entity", "SetLocalRotation", &LuaModuleRuntime::luaEntitySetRotation);
    registerFunction("Entity", "GetLocalRotation", &LuaModuleRuntime::luaEntityGetRotation);
    registerFunction("Entity", "SetWorldRotation", &LuaModuleRuntime::luaEntitySetWorldRotation);
    registerFunction("Entity", "GetWorldRotation", &LuaModuleRuntime::luaEntityGetWorldRotation);
    registerFunction("Entity", "SetScale", &LuaModuleRuntime::luaEntitySetScale);
    registerFunction("Entity", "GetScale", &LuaModuleRuntime::luaEntityGetScale);
    registerFunction("Entity", "SetLocalScale", &LuaModuleRuntime::luaEntitySetScale);
    registerFunction("Entity", "GetLocalScale", &LuaModuleRuntime::luaEntityGetScale);
    registerFunction("Entity", "SetWorldScale", &LuaModuleRuntime::luaEntitySetWorldScale);
    registerFunction("Entity", "GetWorldScale", &LuaModuleRuntime::luaEntityGetWorldScale);
    registerFunction("Entity", "SetDebugPrimitive", &LuaModuleRuntime::luaEntitySetDebugPrimitive);
    registerFunction("Entity", "RemoveDebugPrimitive", &LuaModuleRuntime::luaEntityRemoveDebugPrimitive);
    registerFunction("Entity", "HasDebugPrimitive", &LuaModuleRuntime::luaEntityHasDebugPrimitive);
    registerFunction("Entity", "SetDebugVisible", &LuaModuleRuntime::luaEntitySetDebugVisible);
    registerFunction("Entity", "SetDebugColor", &LuaModuleRuntime::luaEntitySetDebugColor);
    registerFunction("Entity", "GetDebugPrimitive", &LuaModuleRuntime::luaEntityGetDebugPrimitive);
    registerFunction("Entity", "SetMesh", &LuaModuleRuntime::luaEntitySetMesh);
    registerFunction("Entity", "RemoveMesh", &LuaModuleRuntime::luaEntityRemoveMesh);
    registerFunction("Entity", "HasMesh", &LuaModuleRuntime::luaEntityHasMesh);
    registerFunction("Entity", "SetMeshVisible", &LuaModuleRuntime::luaEntitySetMeshVisible);
    registerFunction("Entity", "SetMeshColor", &LuaModuleRuntime::luaEntitySetMeshColor);
    registerFunction("Entity", "SetMeshNormalize", &LuaModuleRuntime::luaEntitySetMeshNormalize);
    registerFunction("Entity", "SetMeshDoubleSided", &LuaModuleRuntime::luaEntitySetMeshDoubleSided);
    registerFunction("Entity", "GetMesh", &LuaModuleRuntime::luaEntityGetMesh);
    registerFunction("Entity", "GetLastError", &LuaModuleRuntime::luaEntityGetLastError);

    registerFunction("Prefab", "IsAvailable", &LuaModuleRuntime::luaPrefabIsAvailable);
    registerFunction("Prefab", "Exists", &LuaModuleRuntime::luaPrefabExists);
    registerFunction("Prefab", "Instantiate", &LuaModuleRuntime::luaPrefabInstantiate);
    registerFunction("Prefab", "GetLastError", &LuaModuleRuntime::luaPrefabGetLastError);

    registerFunction("Module", "Id", &LuaModuleRuntime::luaModuleId);
    registerFunction("Module", "Name", &LuaModuleRuntime::luaModuleName);
    registerFunction("Module", "Version", &LuaModuleRuntime::luaModuleVersion);
    registerFunction("Module", "AssetPath", &LuaModuleRuntime::luaModuleAssetPath);
    registerFunction("Module", "AssetExists", &LuaModuleRuntime::luaModuleAssetExists);
    registerFunction("Module", "SelectAssetFile", &LuaModuleRuntime::luaModuleSelectAssetFile);
    registerFunction("Module", "DataPath", &LuaModuleRuntime::luaModuleDataPath);
    registerFunction("Module", "SavePath", &LuaModuleRuntime::luaModuleSavePath);
    registerFunction("Module", "WriteSaveText", &LuaModuleRuntime::luaModuleWriteSaveText);
}

void LuaModuleRuntime::sandboxStandardLibraries()
{
    // This is a practical mod sandbox foundation, not a hardened hostile-code
    // security boundary. Direct file/process/package/debug access is removed;
    // modules receive controlled engine services instead.
    const char* blockedGlobals[] = {
        "io", "os", "package", "debug",
        "dofile", "loadfile", "require"
    };

    for (const char* name : blockedGlobals)
        replaceGlobalWithNil(name);
}

void LuaModuleRuntime::registerFunction(
    const char* tableName,
    const char* functionName,
    LuaCFunction function)
{
    const std::string qualifiedName = std::string(tableName) + "." + functionName;
    if (std::find(
            m_registeredLuaFunctions.begin(),
            m_registeredLuaFunctions.end(),
            qualifiedName) == m_registeredLuaFunctions.end())
    {
        m_registeredLuaFunctions.push_back(qualifiedName);
    }
    else
    {
        std::cerr << "[LuaAPI] Duplicate binding registration: "
            << qualifiedName << '\n';
    }

    m_api.lua_getglobal(m_state, tableName);
    if (m_api.lua_type(m_state, -1) == kLuaTypeNil)
    {
        pop(1);
        m_api.lua_createtable(m_state, 0, 12);
    }

    m_api.lua_pushcclosure(m_state, function, 0);
    m_api.lua_setfield(m_state, -2, functionName);
    m_api.lua_setglobal(m_state, tableName);
}

void LuaModuleRuntime::writeLuaApiManifest() const
{
    if (!m_context)
        return;

    try
    {
        std::vector<std::string> names = m_registeredLuaFunctions;
        std::sort(names.begin(), names.end());

        const std::filesystem::path reportRoot =
            m_context->projectRoot() / "Build" / "Reports";
        std::filesystem::create_directories(reportRoot);

        const std::filesystem::path jsonPath =
            reportRoot / "LuaAPI_Runtime.json";
        const std::filesystem::path markdownPath =
            reportRoot / "LuaAPI_Runtime.md";

        std::ofstream json(jsonPath, std::ios::trunc);
        if (json)
        {
            json << "{\n";
            json << "  \"build_identity\": \""
                << jsonEscape(heritage::diagnostics::buildIdentity()) << "\",\n";
            json << "  \"module\": \""
                << jsonEscape(m_context->module().id) << "\",\n";
            json << "  \"binding_count\": " << names.size() << ",\n";
            json << "  \"bindings\": [\n";
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                json << "    \"" << jsonEscape(names[index]) << "\"";
                if (index + 1 < names.size())
                    json << ',';
                json << '\n';
            }
            json << "  ]\n";
            json << "}\n";
        }

        std::ofstream markdown(markdownPath, std::ios::trunc);
        if (markdown)
        {
            markdown << "# Live Heritage Engine Lua API\n\n";
            markdown << "Build: `"
                << heritage::diagnostics::buildIdentity() << "`\n\n";
            markdown << "Module: `" << m_context->module().id << "`\n\n";
            markdown << "Registered functions: **" << names.size() << "**\n\n";
            markdown << "These names were captured from the running executable. "
                "Inspect `Build/Reports/LuaAPI.md` or the named C++ binding handler "
                "for arguments and return values; never guess a signature.\n\n";
            for (const std::string& name : names)
                markdown << "- `" << name << "`\n";
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[LuaAPI] Could not write runtime manifest: "
            << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "[LuaAPI] Could not write runtime manifest: unknown error\n";
    }
}

bool LuaModuleRuntime::runSafetySmokeTests(
    std::string& summary,
    std::filesystem::path& reportPath)
{
    if (!m_context || !m_entities || !m_physics)
    {
        summary = "FAIL: Engine services required by the safety tests are unavailable.";
        reportPath.clear();
        m_lastSafetyReport = summary;
        m_lastSafetyReportPath.clear();
        return false;
    }

    auto& bodies = m_physics->rigidBodies();
    auto& collisions = m_physics->collisions();
    auto& constraints = m_physics->constraints();
    auto& vehicles = m_physics->vehicles();

    const std::size_t entityCountBefore = m_entities->count();
    const std::size_t bodyCountBefore = bodies.count();
    const std::size_t colliderCountBefore = collisions.count();
    const std::size_t constraintCountBefore = constraints.count();
    const std::size_t vehicleCountBefore = vehicles.count();

    heritage::entities::EntityHandle entity = heritage::entities::InvalidEntity;
    heritage::entities::EntityHandle replacementEntity = heritage::entities::InvalidEntity;
    heritage::physics::BodyHandle body = heritage::physics::InvalidBody;
    heritage::physics::ColliderHandle collider = heritage::physics::InvalidCollider;
    heritage::physics::ConstraintHandle constraint = heritage::physics::InvalidConstraint;
    heritage::vehicles::VehicleHandle vehicle = heritage::vehicles::InvalidVehicle;

    bool passed = true;
    std::size_t passedChecks = 0;
    std::size_t totalChecks = 0;
    std::vector<std::string> checks;

    auto record = [&](bool condition, const std::string& label)
    {
        ++totalChecks;
        if (condition)
            ++passedChecks;
        else
            passed = false;
        checks.push_back(std::string(condition ? "PASS: " : "FAIL: ") + label);
    };

    entity = m_entities->create("__heritage_safety_smoke_entity__");
    record(entity != heritage::entities::InvalidEntity, "created temporary entity");
    record(m_entities->exists(entity), "temporary entity handle resolves");

    heritage::physics::RigidBodyDescription bodyDescription;
    bodyDescription.entity = entity;
    bodyDescription.motionType = heritage::physics::BodyMotionType::Dynamic;
    bodyDescription.position = { 0.0f, 10000.0f, 0.0f };
    bodyDescription.mass = 25.0f;
    bodyDescription.gravityFactor = 0.0f;
    body = bodies.create(bodyDescription);
    record(body != heritage::physics::InvalidBody, "created temporary rigid body");
    record(bodies.exists(body), "temporary body handle resolves");

    if (bodies.exists(body))
    {
        collider = collisions.createSphere(
            body,
            0.25f,
            { 0.0f, 0.0f, 0.0f },
            0.7f,
            0.0f,
            false,
            bodies);
    }
    record(collider != heritage::physics::InvalidCollider, "created dependent collider");
    record(collisions.exists(collider), "dependent collider handle resolves");

    if (bodies.exists(body))
    {
        heritage::physics::SpringConstraintDescription spring;
        spring.bodyA = body;
        spring.anchorB = { 0.0f, 10001.0f, 0.0f };
        spring.restLength = 1.0f;
        spring.stiffness = 100.0f;
        spring.damping = 10.0f;
        constraint = constraints.createSpring(spring, bodies);
    }
    record(constraint != heritage::physics::InvalidConstraint, "created dependent constraint");
    record(constraints.exists(constraint), "dependent constraint handle resolves");

    if (bodies.exists(body))
    {
        heritage::vehicles::VehicleDescription vehicleDescription;
        vehicleDescription.chassisBody = body;
        vehicleDescription.highRateHertz = 1000.0f;
        vehicle = vehicles.create(vehicleDescription, bodies);
    }
    record(vehicle != heritage::vehicles::InvalidVehicle, "created dependent vehicle");
    record(vehicles.exists(vehicle), "dependent vehicle handle resolves");

    const bool bodyDestroyed = bodies.exists(body)
        && m_physics->destroyBody(body);
    record(bodyDestroyed, "PhysicsWorld destroyed the temporary body");
    record(!bodies.exists(body), "destroyed body handle is stale");
    record(!collisions.exists(collider), "body destruction invalidated its collider");
    record(!constraints.exists(constraint), "body destruction invalidated its constraint");
    record(!vehicles.exists(vehicle), "body destruction invalidated its vehicle");

    const bool entityDestroyed = m_entities->exists(entity)
        && m_entities->destroy(entity);
    record(entityDestroyed, "destroyed temporary entity");
    record(!m_entities->exists(entity), "destroyed entity handle is stale");

    replacementEntity = m_entities->create("__heritage_safety_smoke_replacement__");
    record(replacementEntity != heritage::entities::InvalidEntity, "created replacement entity");
    record(replacementEntity != entity, "slot reuse changed the entity generation");
    record(!m_entities->exists(entity), "old entity handle remains invalid after slot reuse");

    if (m_entities->exists(replacementEntity))
        m_entities->destroy(replacementEntity);

    // Defensive cleanup if a preceding creation or cascade check failed.
    if (vehicles.exists(vehicle))
        vehicles.destroy(vehicle);
    if (constraints.exists(constraint))
        constraints.destroy(constraint);
    if (collisions.exists(collider))
        collisions.destroy(collider);
    if (bodies.exists(body))
        m_physics->destroyBody(body);
    if (m_entities->exists(entity))
        m_entities->destroy(entity);
    if (m_entities->exists(replacementEntity))
        m_entities->destroy(replacementEntity);

    record(m_entities->count() == entityCountBefore, "entity count returned to baseline");
    record(bodies.count() == bodyCountBefore, "body count returned to baseline");
    record(collisions.count() == colliderCountBefore, "collider count returned to baseline");
    record(constraints.count() == constraintCountBefore, "constraint count returned to baseline");
    record(vehicles.count() == vehicleCountBefore, "vehicle count returned to baseline");

    try
    {
        const std::filesystem::path diagnosticRoot =
            m_context->projectRoot() / "UserData" / "Diagnostics";
        std::filesystem::create_directories(diagnosticRoot);
        reportPath = diagnosticRoot / "safety_smoke_last.txt";

        std::ofstream report(reportPath, std::ios::trunc);
        if (report)
        {
            report << "build_identity="
                << heritage::diagnostics::buildIdentity() << '\n';
            report << "module=" << m_context->module().id << '\n';
            report << "result=" << (passed ? "PASS" : "FAIL") << '\n';
            report << "checks_passed=" << passedChecks << '\n';
            report << "checks_total=" << totalChecks << '\n';
            report << "\n";
            for (const std::string& check : checks)
                report << check << '\n';
        }
    }
    catch (...)
    {
        reportPath.clear();
    }

    std::ostringstream result;
    result << (passed ? "PASS: " : "FAIL: ")
        << passedChecks << "/" << totalChecks
        << " engine lifetime safety checks passed.";
    if (!reportPath.empty())
        result << " Report: " << reportPath.string();

    summary = result.str();
    m_lastSafetyReport = summary;
    m_lastSafetyReportPath = reportPath;
    return passed;
}

void LuaModuleRuntime::replaceGlobalWithNil(const char* name)
{
    m_api.lua_pushnil(m_state);
    m_api.lua_setglobal(m_state, name);
}

bool LuaModuleRuntime::callOptionalNoArgs(const char* functionName)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }
    return protectedCall(0, 0, functionName);
}

bool LuaModuleRuntime::callOptionalNumber(
    const char* functionName,
    LuaNumber value)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushnumber(m_state, value);
    return protectedCall(1, 0, functionName);
}

bool LuaModuleRuntime::callOptionalTwoIntegers(
    const char* functionName,
    LuaInteger first,
    LuaInteger second)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushinteger(m_state, first);
    m_api.lua_pushinteger(m_state, second);
    return protectedCall(2, 0, functionName);
}

bool LuaModuleRuntime::callOptionalString(
    const char* functionName,
    const std::string& value)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushlstring(m_state, value.c_str(), value.size());
    return protectedCall(1, 0, functionName);
}

bool LuaModuleRuntime::callOptionalTwoStrings(
    const char* functionName,
    const std::string& first,
    const std::string& second)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushlstring(m_state, first.c_str(), first.size());
    m_api.lua_pushlstring(m_state, second.c_str(), second.size());
    return protectedCall(2, 0, functionName);
}

bool LuaModuleRuntime::protectedCall(
    int argumentCount,
    int resultCount,
    const char* contextName)
{
    const int status = m_api.lua_pcallk(
        m_state,
        argumentCount,
        resultCount,
        0,
        0,
        nullptr);
    if (status == kLuaOk)
        return true;

    const std::string detail = stackString(-1);
    pop(1);
    setScriptError(
        "Lua error while " + std::string(contextName) + ":\n" + detail);
    return false;
}

void LuaModuleRuntime::setScriptError(const std::string& message)
{
    m_scriptError = message.empty()
        ? "Unknown Lua runtime error."
        : message;
}

void LuaModuleRuntime::clearScriptError()
{
    m_scriptError.clear();
}

std::string LuaModuleRuntime::stackString(int index) const
{
    if (!m_state || !m_api.lua_tolstring)
        return "<no Lua error text>";

    std::size_t length = 0;
    const char* text = m_api.lua_tolstring(m_state, index, &length);
    return text ? std::string(text, length) : "<non-string Lua error>";
}

void LuaModuleRuntime::pop(int count)
{
    if (m_state && count > 0)
        m_api.lua_settop(m_state, -count - 1);
}

void LuaModuleRuntime::drawRuntimeError(
    int framebufferWidth,
    int framebufferHeight)
{
    const float availableWidth = static_cast<float>((std::max)(framebufferWidth, 1));
    const float availableHeight = static_cast<float>((std::max)(framebufferHeight, 1));
    const float panelWidth = (std::max)(280.0f,
        (std::min)(720.0f, availableWidth - 32.0f));
    const float panelHeight = (std::max)(220.0f,
        (std::min)(460.0f, availableHeight - 48.0f));

    ImGui::SetNextWindowPos(
        ImVec2(availableWidth * 0.5f, availableHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.015f, 0.018f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##heritage_lua_error", nullptr, flags))
    {
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted("LUA MODULE ERROR");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        if (!m_scriptPath.empty())
            ImGui::TextDisabled("%s", m_scriptPath.string().c_str());

        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_scriptError.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Fix and save any module Lua file to reload automatically, or press F5. "
            "If lua54.dll is missing, run Tools\\SetupLua.ps1 and rebuild.");
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void LuaModuleRuntime::closeOpenPanel()
{
    while (!m_uiScopes.empty())
    {
        const UiScopeType scope = m_uiScopes.back();
        m_uiScopes.pop_back();
        if (scope == UiScopeType::TabItem)
            ImGui::EndTabItem();
        else
            ImGui::EndTabBar();
    }

    if (!m_panelOpen)
        return;

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    m_panelOpen = false;
    m_panelVisible = false;
}

void LuaModuleRuntime::queueAction(
    ModuleRuntimeActionType type,
    const std::string& payload)
{
    ModuleRuntimeAction action;
    action.type = type;
    action.payload = payload;
    m_actions.push_back(std::move(action));
}

bool LuaModuleRuntime::checkForScriptChange(float deltaTime)
{
    m_reloadCheckTimer += deltaTime;
    if (m_reloadCheckTimer < 0.5f || m_scriptPath.empty())
        return false;

    m_reloadCheckTimer = 0.0f;
    auto currentWriteTimes = captureScriptWriteTimes();
    if (currentWriteTimes == m_scriptWriteTimes)
        return false;

    m_scriptWriteTimes = std::move(currentWriteTimes);
    return true;
}

std::unordered_map<std::string, std::filesystem::file_time_type>
LuaModuleRuntime::captureScriptWriteTimes() const
{
    std::unordered_map<std::string, std::filesystem::file_time_type> result;
    if (!m_context)
        return result;

    const std::filesystem::path root = m_context->scriptsRoot();
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error)
        return result;

    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator endIterator;

    while (!error && iterator != endIterator)
    {
        const std::filesystem::directory_entry& entry = *iterator;
        std::error_code entryError;
        if (entry.is_regular_file(entryError)
            && !entryError
            && entry.path().extension() == ".lua")
        {
            const auto writeTime = entry.last_write_time(entryError);
            if (!entryError)
            {
                result.emplace(
                    entry.path().lexically_normal().generic_string(),
                    writeTime);
            }
        }

        iterator.increment(error);
    }

    return result;
}

void LuaModuleRuntime::refreshScriptWatchSnapshot()
{
    m_scriptWriteTimes = captureScriptWriteTimes();
}

void LuaModuleRuntime::requestSceneLoad(
    const std::string& sceneId,
    bool forceReload)
{
    m_pendingSceneId = sceneId;
    m_pendingSceneReload = forceReload;
}

void LuaModuleRuntime::processPendingSceneTransition()
{
    if (!m_pendingSceneId || !m_context || !m_window)
        return;

    const std::string requested = *m_pendingSceneId;
    const bool forceReload = m_pendingSceneReload;
    m_pendingSceneId.reset();
    m_pendingSceneReload = false;

    const std::string previous = m_sceneManager.activeSceneId();
    if (!forceReload && !previous.empty() && previous == requested)
        return;

    if (!previous.empty() && m_state && m_scriptError.empty())
        callOptionalString("OnSceneExit", previous);

    std::string sceneError;
    const bool loaded = m_sceneManager.load(
        requested,
        m_window,
        *m_context,
        m_entities,
        sceneError);

    m_lastSceneError = sceneError;
    if (loaded)
    {
        if (m_state && m_scriptError.empty())
            callOptionalString("OnSceneEnter", requested);
    }
    else
    {
        std::cerr << "[Scene:" << m_context->module().id << "] "
            << sceneError << '\n';
        if (m_state && m_scriptError.empty())
            callOptionalTwoStrings("OnSceneError", requested, sceneError);
    }
}

LuaModuleRuntime* LuaModuleRuntime::runtimeFrom(lua_State* state)
{
    const auto found = g_runtimeByState.find(state);
    return found == g_runtimeByState.end() ? nullptr : found->second;
}

std::string LuaModuleRuntime::stringArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    const std::string& fallback)
{
    std::size_t length = 0;
    const char* value = runtime.m_api.lua_tolstring(state, index, &length);
    return value ? std::string(value, length) : fallback;
}

double LuaModuleRuntime::numberArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    double fallback)
{
    int isNumber = 0;
    const LuaNumber value = runtime.m_api.lua_tonumberx(state, index, &isNumber);
    return isNumber ? static_cast<double>(value) : fallback;
}

bool LuaModuleRuntime::booleanArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    bool fallback)
{
    const int type = runtime.m_api.lua_type(state, index);
    return type == kLuaTypeBoolean
        ? runtime.m_api.lua_toboolean(state, index) != 0
        : fallback;
}

heritage::entities::EntityHandle LuaModuleRuntime::entityHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::entities::InvalidEntity;

    return static_cast<heritage::entities::EntityHandle>(value);
}

heritage::physics::BodyHandle LuaModuleRuntime::bodyHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidBody;

    return static_cast<heritage::physics::BodyHandle>(value);
}

heritage::physics::ColliderHandle LuaModuleRuntime::colliderHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidCollider;

    return static_cast<heritage::physics::ColliderHandle>(value);
}

heritage::physics::ConstraintHandle LuaModuleRuntime::constraintHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidConstraint;

    return static_cast<heritage::physics::ConstraintHandle>(value);
}

heritage::vehicles::VehicleHandle LuaModuleRuntime::vehicleHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::vehicles::InvalidVehicle;

    return static_cast<heritage::vehicles::VehicleHandle>(value);
}

int LuaModuleRuntime::luaPrint(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const int count = runtime->m_api.lua_gettop(state);
    std::ostringstream output;
    output << "[Lua:";
    output << (runtime->m_context ? runtime->m_context->module().id : "?");
    output << "] ";

    for (int index = 1; index <= count; ++index)
    {
        if (index > 1)
            output << '\t';

        const int type = runtime->m_api.lua_type(state, index);
        if (type == kLuaTypeBoolean)
            output << (runtime->m_api.lua_toboolean(state, index) ? "true" : "false");
        else if (type == kLuaTypeNil)
            output << "nil";
        else
            output << stringArgument(*runtime, state, index, "<value>");
    }

    std::cout << output.str() << '\n';
    return 0;
}

int LuaModuleRuntime::luaUiBeginPanel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->closeOpenPanel();

    const std::string panelId = stringArgument(
        *runtime, state, 1, "LuaModulePanel");
    const float availableWidth = static_cast<float>(runtime->m_framebufferWidth);
    const float availableHeight = static_cast<float>(runtime->m_framebufferHeight);
    const float requestedWidth = static_cast<float>(
        numberArgument(*runtime, state, 2, 620.0));
    const float requestedHeight = static_cast<float>(
        numberArgument(*runtime, state, 3, 520.0));

    const float maximumPanelWidth = (std::max)(120.0f, availableWidth - 32.0f);
    const float maximumPanelHeight = (std::max)(100.0f, availableHeight - 48.0f);
    const float panelWidth = (std::min)((std::max)(180.0f, requestedWidth), maximumPanelWidth);
    const float panelHeight = (std::min)((std::max)(140.0f, requestedHeight), maximumPanelHeight);

    ImGui::SetNextWindowPos(
        ImVec2(availableWidth * 0.5f, availableHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.040f, 0.048f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    const std::string windowName = "##lua_panel_" + panelId;
    runtime->m_panelVisible = ImGui::Begin(windowName.c_str(), nullptr, flags);
    runtime->m_panelOpen = true;
    runtime->m_api.lua_pushboolean(state, runtime->m_panelVisible ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaUiEndPanel(lua_State* state)
{
    if (LuaModuleRuntime* runtime = runtimeFrom(state))
        runtime->closeOpenPanel();
    return 0;
}

int LuaModuleRuntime::luaUiBeginTabBar(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string id = stringArgument(*runtime, state, 1, "LuaTabs");
    const bool open = ImGui::BeginTabBar(id.c_str());
    if (open)
        runtime->m_uiScopes.push_back(UiScopeType::TabBar);
    runtime->m_api.lua_pushboolean(state, open ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaUiEndTabBar(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_uiScopes.empty()
        && runtime->m_uiScopes.back() == UiScopeType::TabBar)
    {
        ImGui::EndTabBar();
        runtime->m_uiScopes.pop_back();
    }
    return 0;
}

int LuaModuleRuntime::luaUiBeginTabItem(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Tab");
    const bool open = ImGui::BeginTabItem(label.c_str());
    if (open)
        runtime->m_uiScopes.push_back(UiScopeType::TabItem);
    runtime->m_api.lua_pushboolean(state, open ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaUiEndTabItem(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_uiScopes.empty()
        && runtime->m_uiScopes.back() == UiScopeType::TabItem)
    {
        ImGui::EndTabItem();
        runtime->m_uiScopes.pop_back();
    }
    return 0;
}

int LuaModuleRuntime::luaUiModuleLabel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime && runtime->m_context)
    {
        const auto& module = runtime->m_context->module();
        const std::string label = module.name.empty() ? module.id : module.name;
        ImGui::TextDisabled("%s", label.c_str());
    }
    return 0;
}

int LuaModuleRuntime::luaUiTitle(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string text = stringArgument(*runtime, state, 1);
    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    return 0;
}

int LuaModuleRuntime::luaUiSubtitle(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime)
    {
        const std::string text = stringArgument(*runtime, state, 1);
        ImGui::TextDisabled("%s", text.c_str());
    }
    return 0;
}

int LuaModuleRuntime::luaUiText(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime)
        ImGui::TextUnformatted(stringArgument(*runtime, state, 1).c_str());
    return 0;
}

int LuaModuleRuntime::luaUiTextWrapped(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime)
    {
        const std::string text = stringArgument(*runtime, state, 1);
        ImGui::TextWrapped("%s", text.c_str());
    }
    return 0;
}

int LuaModuleRuntime::luaUiTextDisabled(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime)
    {
        const std::string text = stringArgument(*runtime, state, 1);
        ImGui::TextDisabled("%s", text.c_str());
    }
    return 0;
}

int LuaModuleRuntime::luaUiSeparator(lua_State*)
{
    ImGui::Separator();
    return 0;
}

int LuaModuleRuntime::luaUiSpacing(lua_State*)
{
    ImGui::Spacing();
    return 0;
}

int LuaModuleRuntime::luaUiSameLine(lua_State*)
{
    ImGui::SameLine();
    return 0;
}

int LuaModuleRuntime::luaUiGetAvailableWidth(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>((std::max)(0.0f, ImGui::GetContentRegionAvail().x)));
    return 1;
}

int LuaModuleRuntime::luaUiButton(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "BUTTON");
    const float defaultWidth = (std::min)(
        360.0f,
        (std::max)(100.0f, ImGui::GetContentRegionAvail().x));
    const float availableWidth = (std::max)(80.0f, ImGui::GetContentRegionAvail().x);
    const float requestedWidth = static_cast<float>(
        numberArgument(*runtime, state, 2, defaultWidth));
    const float width = (std::min)((std::max)(80.0f, requestedWidth), availableWidth);
    const float height = (std::max)(22.0f, static_cast<float>(
        numberArgument(*runtime, state, 3, 38.0)));
    const bool centered = booleanArgument(*runtime, state, 4, true);

    if (centered)
    {
        const float cursorX = ImGui::GetCursorPosX()
            + (ImGui::GetContentRegionAvail().x - width) * 0.5f;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), cursorX));
    }

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool clicked = ImGui::Button(label.c_str(), ImVec2(width, height));
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, clicked ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaUiSliderFloat(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Value");
    float value = static_cast<float>(numberArgument(*runtime, state, 2, 0.0));
    const float minimum = static_cast<float>(numberArgument(*runtime, state, 3, 0.0));
    const float maximum = static_cast<float>(numberArgument(*runtime, state, 4, 1.0));
    const std::string format = stringArgument(*runtime, state, 5, "%.2f");

    const float originalValue = value;
    bool changed = false;

    ImGui::BeginDisabled(!runtime->m_allowInteraction);

    const bool numericInputMode = runtime->m_numericSliderInputLabel == label;
    if (numericInputMode)
    {
        if (runtime->m_numericSliderFocusRequested)
        {
            ImGui::SetKeyboardFocusHere();
            runtime->m_numericSliderFocusRequested = false;
        }

        const bool submitted = ImGui::InputFloat(
            label.c_str(),
            &value,
            0.0f,
            0.0f,
            format.c_str(),
            ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_AutoSelectAll);
        value = clampFloat(value, minimum, maximum);
        changed = std::abs(value - originalValue) > 0.0000001f;

        // Enter commits immediately. Clicking elsewhere leaves text-input mode
        // after the field loses focus. A double-click on any slider activates
        // this mode without requiring Ctrl or another modifier key.
        if (submitted || (!ImGui::IsItemActive() && !ImGui::IsItemHovered()))
            runtime->m_numericSliderInputLabel.clear();
    }
    else
    {
        changed = ImGui::SliderFloat(
            label.c_str(), &value, minimum, maximum, format.c_str());

        if (runtime->m_allowInteraction
            && ImGui::IsItemHovered()
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            runtime->m_numericSliderInputLabel = label;
            runtime->m_numericSliderFocusRequested = true;
        }
    }

    ImGui::EndDisabled();

    runtime->m_api.lua_pushnumber(state, value);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaModuleRuntime::luaUiCheckbox(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Enabled");
    bool value = booleanArgument(*runtime, state, 2, false);

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::Checkbox(label.c_str(), &value);
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaModuleRuntime::luaUiInputInt(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Value");
    int validInteger = 0;
    int value = static_cast<int>(runtime->m_api.lua_tointegerx(state, 2, &validInteger));
    if (!validInteger)
        value = 0;
    const int step = static_cast<int>(numberArgument(*runtime, state, 3, 1.0));

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::InputInt(label.c_str(), &value, step);
    ImGui::EndDisabled();

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(value));
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaModuleRuntime::luaUiInputText(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Text");
    const std::string current = stringArgument(*runtime, state, 2);
    const std::size_t requestedCapacity = static_cast<std::size_t>((std::max)(
        32.0,
        (std::min)(4096.0, numberArgument(*runtime, state, 3, 512.0))));
    std::vector<char> buffer(requestedCapacity + 1, '\0');
    const std::size_t copied = (std::min)(current.size(), requestedCapacity);
    std::memcpy(buffer.data(), current.data(), copied);

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::InputText(
        label.c_str(), buffer.data(), buffer.size());
    ImGui::EndDisabled();

    const std::size_t length = std::strlen(buffer.data());
    runtime->m_api.lua_pushlstring(state, buffer.data(), length);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaModuleRuntime::luaUiImage(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const float requestedWidth = static_cast<float>(
        numberArgument(*runtime, state, 2, 0.0));
    const float requestedHeight = static_cast<float>(
        numberArgument(*runtime, state, 3, 0.0));
    const UiAlignment alignment = parseAlignment(
        stringArgument(*runtime, state, 4, "left"));
    const float alpha = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 5, 1.0)), 0.0f, 1.0f);

    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(relativePath);
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
            "UI IMAGE ERROR: %s",
            runtime->m_lastUiError.c_str());
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastUiError.clear();
    const ImVec2 size = calculateImageSize(
        image->width,
        image->height,
        requestedWidth,
        requestedHeight,
        ImGui::GetContentRegionAvail().x);
    alignNextItem(alignment, size.x);

    const ImTextureRef texture(
        static_cast<ImTextureID>(image->textureId));
    ImGui::ImageWithBg(
        texture,
        size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f),
        ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
        ImVec4(1.0f, 1.0f, 1.0f, alpha));

    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaUiImageButton(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string itemId = stringArgument(
        *runtime, state, 1, "LuaImageButton");
    const std::string relativePath = stringArgument(*runtime, state, 2);
    const float requestedWidth = static_cast<float>(
        numberArgument(*runtime, state, 3, 0.0));
    const float requestedHeight = static_cast<float>(
        numberArgument(*runtime, state, 4, 0.0));
    const UiAlignment alignment = parseAlignment(
        stringArgument(*runtime, state, 5, "left"));

    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(relativePath);
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
            "UI IMAGE ERROR: %s",
            runtime->m_lastUiError.c_str());
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        return 2;
    }

    runtime->m_lastUiError.clear();
    const ImVec2 size = calculateImageSize(
        image->width,
        image->height,
        requestedWidth,
        requestedHeight,
        ImGui::GetContentRegionAvail().x);
    alignNextItem(alignment, size.x);

    const ImTextureRef texture(
        static_cast<ImTextureID>(image->textureId));
    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool clicked = ImGui::ImageButton(
        itemId.c_str(),
        texture,
        size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, clicked ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, 1);
    return 2;
}

int LuaModuleRuntime::luaUiGetImageSize(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(relativePath);
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        return 3;
    }

    runtime->m_lastUiError.clear();
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(image->width));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(image->height));
    runtime->m_api.lua_pushboolean(state, 1);
    return 3;
}

int LuaModuleRuntime::luaUiUnloadImage(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(relativePath);
    const bool unloaded = runtime->m_uiImages.unload(absolutePath);
    runtime->m_api.lua_pushboolean(state, unloaded ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaUiGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastUiError.c_str(),
        runtime->m_lastUiError.size());
    return 1;
}

int LuaModuleRuntime::luaUiSetCursorPos(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const float x = static_cast<float>(numberArgument(*runtime, state, 1, 0.0));
    const float y = static_cast<float>(numberArgument(*runtime, state, 2, 0.0));
    ImGui::SetCursorPos(ImVec2((std::max)(0.0f, x), (std::max)(0.0f, y)));
    return 0;
}

int LuaModuleRuntime::luaUiGetCursorPos(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const ImVec2 position = ImGui::GetCursorPos();
    runtime->m_api.lua_pushnumber(state, position.x);
    runtime->m_api.lua_pushnumber(state, position.y);
    return 2;
}

int LuaModuleRuntime::luaUiDummy(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const float width = (std::max)(0.0f, static_cast<float>(
        numberArgument(*runtime, state, 1, 0.0)));
    const float height = (std::max)(0.0f, static_cast<float>(
        numberArgument(*runtime, state, 2, 0.0)));
    ImGui::Dummy(ImVec2(width, height));
    return 0;
}

int LuaModuleRuntime::luaUiTextColored(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string text = stringArgument(*runtime, state, 1);
    const float red = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 2, 1.0)), 0.0f, 1.0f);
    const float green = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 3, 1.0)), 0.0f, 1.0f);
    const float blue = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 4, 1.0)), 0.0f, 1.0f);
    const float alpha = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 5, 1.0)), 0.0f, 1.0f);

    ImGui::TextColored(
        ImVec4(red, green, blue, alpha),
        "%s",
        text.c_str());
    return 0;
}

int LuaModuleRuntime::luaUiProgressBar(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const float fraction = clampFloat(static_cast<float>(
        numberArgument(*runtime, state, 1, 0.0)), 0.0f, 1.0f);
    const float requestedWidth = static_cast<float>(
        numberArgument(*runtime, state, 2, -1.0));
    const float height = (std::max)(1.0f, static_cast<float>(
        numberArgument(*runtime, state, 3, 0.0)));
    const std::string overlay = stringArgument(*runtime, state, 4);

    const float width = requestedWidth <= 0.0f
        ? ImGui::GetContentRegionAvail().x
        : (std::min)(requestedWidth, ImGui::GetContentRegionAvail().x);
    ImGui::ProgressBar(
        fraction,
        ImVec2(width, height),
        overlay.empty() ? nullptr : overlay.c_str());
    return 0;
}

int LuaModuleRuntime::luaUiPlotLines(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = stringArgument(*runtime, state, 1, "Plot");
    const float height = (std::max)(40.0f, static_cast<float>(
        numberArgument(*runtime, state, 2, 90.0)));
    const int argumentCount = runtime->m_api.lua_gettop(state);
    const int valueCount = (std::min)(512, (std::max)(0, argumentCount - 2));
    if (valueCount == 0)
    {
        ImGui::TextDisabled("%s: no captured samples", label.c_str());
        return 0;
    }

    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(valueCount));
    for (int argument = 3; argument < 3 + valueCount; ++argument)
    {
        values.push_back(static_cast<float>(
            numberArgument(*runtime, state, argument, 0.0)));
    }

    ImGui::PlotLines(
        label.c_str(),
        values.data(),
        valueCount,
        0,
        nullptr,
        FLT_MAX,
        FLT_MAX,
        ImVec2(-1.0f, height));
    return 0;
}

int LuaModuleRuntime::luaEngineOpenSettings(lua_State* state)
{
    if (LuaModuleRuntime* runtime = runtimeFrom(state))
        runtime->queueAction(ModuleRuntimeActionType::OpenEngineSettings);
    return 0;
}

int LuaModuleRuntime::luaEngineExit(lua_State* state)
{
    if (LuaModuleRuntime* runtime = runtimeFrom(state))
        runtime->queueAction(ModuleRuntimeActionType::ExitApplication);
    return 0;
}

int LuaModuleRuntime::luaEngineSetClearColor(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_clearColor = {
        clampFloat(static_cast<float>(numberArgument(*runtime, state, 1, 0.0)), 0.0f, 1.0f),
        clampFloat(static_cast<float>(numberArgument(*runtime, state, 2, 0.0)), 0.0f, 1.0f),
        clampFloat(static_cast<float>(numberArgument(*runtime, state, 3, 0.0)), 0.0f, 1.0f)
    };
    return 0;
}

int LuaModuleRuntime::luaEngineLog(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (runtime)
    {
        std::cout << "[Lua:";
        std::cout << (runtime->m_context ? runtime->m_context->module().id : "?");
        std::cout << "] " << stringArgument(*runtime, state, 1) << '\n';
    }
    return 0;
}

int LuaModuleRuntime::luaEngineGetBuildIdentity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string identity = heritage::diagnostics::buildIdentity();
    runtime->m_api.lua_pushlstring(state, identity.data(), identity.size());
    return 1;
}

int LuaModuleRuntime::luaEngineGetBuildStep(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::generated::kMilestone);
    return 1;
}

int LuaModuleRuntime::luaEngineGetGitCommit(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::generated::kGitCommit);
    return 1;
}

int LuaModuleRuntime::luaEngineGetBuildConfiguration(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushstring(
        state,
        heritage::diagnostics::compiledConfiguration());
    return 1;
}

int LuaModuleRuntime::luaEngineGetLuaApiCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_registeredLuaFunctions.size()));
    return 1;
}

int LuaModuleRuntime::luaEngineGetLuaApiName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const double rawIndex = numberArgument(*runtime, state, 1, 0.0);
    if (rawIndex < 1.0)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const std::size_t index = static_cast<std::size_t>(rawIndex - 1.0);
    if (index >= runtime->m_registeredLuaFunctions.size())
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const std::string& name = runtime->m_registeredLuaFunctions[index];
    runtime->m_api.lua_pushlstring(state, name.data(), name.size());
    return 1;
}

int LuaModuleRuntime::luaEngineDumpLuaAPI(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    std::vector<std::string> names = runtime->m_registeredLuaFunctions;
    std::sort(names.begin(), names.end());

    std::cout << "[LuaAPI] " << heritage::diagnostics::buildIdentity() << '\n';
    std::cout << "[LuaAPI] Registered functions: " << names.size() << '\n';
    for (const std::string& name : names)
        std::cout << "[LuaAPI] " << name << '\n';

    runtime->writeLuaApiManifest();
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(names.size()));
    return 1;
}

int LuaModuleRuntime::luaEngineRunSafetySmokeTests(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    std::string summary;
    std::filesystem::path reportPath;
    const bool passed = runtime->runSafetySmokeTests(summary, reportPath);

    runtime->m_api.lua_pushboolean(state, passed ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, summary.data(), summary.size());
    const std::string pathString = reportPath.string();
    runtime->m_api.lua_pushlstring(
        state,
        pathString.data(),
        pathString.size());
    return 3;
}

int LuaModuleRuntime::luaEngineGetLastSafetyReport(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastSafetyReport.data(),
        runtime->m_lastSafetyReport.size());
    const std::string pathString = runtime->m_lastSafetyReportPath.string();
    runtime->m_api.lua_pushlstring(
        state,
        pathString.data(),
        pathString.size());
    return 2;
}

int LuaModuleRuntime::luaScriptInclude(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::filesystem::path resolved =
        runtime->m_context->resolveScriptPath(relativePath);

    if (relativePath.empty() || resolved.empty())
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(
            state,
            "Script.Include requires a safe path relative to the module Scripts folder.");
        return 2;
    }

    if (resolved.extension() != ".lua")
    {
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushstring(
            state,
            "Script.Include accepts text .lua files only.");
        return 2;
    }

    if (!std::filesystem::is_regular_file(resolved))
    {
        const std::string message =
            "Included Lua file was not found: " + resolved.string();
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    const int loadStatus = runtime->m_api.luaL_loadfilex(
        state,
        resolved.string().c_str(),
        "t");
    if (loadStatus != kLuaOk)
    {
        const std::string message =
            "Lua syntax/load error in included file " + resolved.string()
            + ":\n" + runtime->stackString(-1);
        runtime->pop(1);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    const int callStatus = runtime->m_api.lua_pcallk(
        state,
        0,
        0,
        0,
        0,
        nullptr);
    if (callStatus != kLuaOk)
    {
        const std::string message =
            "Lua runtime error in included file " + resolved.string()
            + ":\n" + runtime->stackString(-1);
        runtime->pop(1);
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(state, message.data(), message.size());
        return 2;
    }

    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaSceneLoad(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string sceneId = stringArgument(*runtime, state, 1);
    if (sceneId.empty())
    {
        runtime->m_lastSceneError = "Scene.Load requires a non-empty scene ID.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    if (!runtime->m_sceneManager.exists(sceneId, *runtime->m_context))
    {
        runtime->m_lastSceneError = "Scene '" + sceneId
            + "' does not exist in this module and is not a registered built-in scene.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastSceneError.clear();
    runtime->requestSceneLoad(sceneId, false);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaSceneReload(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string current = runtime->m_sceneManager.activeSceneId();
    if (current.empty())
    {
        runtime->m_lastSceneError = "Scene.Reload cannot run because no scene is active.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastSceneError.clear();
    runtime->requestSceneLoad(current, true);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaSceneGetCurrent(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string current = runtime->m_sceneManager.activeSceneId();
    runtime->m_api.lua_pushlstring(state, current.c_str(), current.size());
    return 1;
}

int LuaModuleRuntime::luaSceneExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string sceneId = stringArgument(*runtime, state, 1);
    const bool exists = !sceneId.empty()
        && runtime->m_sceneManager.exists(sceneId, *runtime->m_context);
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSceneGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastSceneError.c_str(),
        runtime->m_lastSceneError.size());
    return 1;
}

int LuaModuleRuntime::luaSaveGetString(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string key = stringArgument(*runtime, state, 1);
    const std::string fallback = stringArgument(*runtime, state, 2);
    const std::string value = runtime->m_saveStore.getString(key, fallback);
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaModuleRuntime::luaSaveSetString(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool saved = runtime->m_saveStore.setString(
        stringArgument(*runtime, state, 1),
        stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveGetInt(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    int validFallback = 0;
    const LuaInteger fallback = runtime->m_api.lua_tointegerx(
        state, 2, &validFallback);
    const std::int64_t value = runtime->m_saveStore.getInteger(
        stringArgument(*runtime, state, 1),
        validFallback ? static_cast<std::int64_t>(fallback) : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(value));
    return 1;
}

int LuaModuleRuntime::luaSaveSetInt(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    int validValue = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(
        state, 2, &validValue);
    const bool saved = validValue
        && runtime->m_saveStore.setInteger(
            stringArgument(*runtime, state, 1),
            static_cast<std::int64_t>(value));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveGetNumber(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const double fallback = numberArgument(*runtime, state, 2, 0.0);
    const double value = runtime->m_saveStore.getNumber(
        stringArgument(*runtime, state, 1), fallback);
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaModuleRuntime::luaSaveSetNumber(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    int validValue = 0;
    const LuaNumber value = runtime->m_api.lua_tonumberx(
        state, 2, &validValue);
    const bool saved = validValue
        && runtime->m_saveStore.setNumber(
            stringArgument(*runtime, state, 1),
            static_cast<double>(value));
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveGetBool(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool fallback = booleanArgument(*runtime, state, 2, false);
    const bool value = runtime->m_saveStore.getBoolean(
        stringArgument(*runtime, state, 1), fallback);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveSetBool(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool value = booleanArgument(*runtime, state, 2, false);
    const bool saved = runtime->m_saveStore.setBoolean(
        stringArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveHas(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool exists = runtime->m_saveStore.has(
        stringArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveRemove(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool removed = runtime->m_saveStore.remove(
        stringArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, removed ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveClear(lua_State* state)
{
    if (LuaModuleRuntime* runtime = runtimeFrom(state))
    {
        runtime->m_saveStore.clear();
        runtime->m_api.lua_pushboolean(state, 1);
        return 1;
    }
    return 0;
}

int LuaModuleRuntime::luaSaveFlush(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool flushed = runtime->m_saveStore.flush();
    runtime->m_saveFlushTimer = 0.0f;
    runtime->m_api.lua_pushboolean(state, flushed ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaSaveGetPath(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string value = runtime->m_saveStore.path().string();
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaModuleRuntime::luaSaveGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string& error = runtime->m_saveStore.lastError();
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaModuleRuntime::luaSaveIsDirty(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state, runtime->m_saveStore.isDirty() ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool available = runtime->m_audio && runtime->m_audio->isAvailable();
    runtime->m_api.lua_pushboolean(state, available ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioGetBackend(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string backend = runtime->m_audio
        ? runtime->m_audio->backendName()
        : "No audio service";
    runtime->m_api.lua_pushlstring(state, backend.c_str(), backend.size());
    return 1;
}

int LuaModuleRuntime::luaAudioPlaySound(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_context || !runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::string busName = stringArgument(*runtime, state, 2, "Effects");
    const auto bus = heritage::audio::parseAudioBus(busName);
    const float volume = static_cast<float>(numberArgument(*runtime, state, 3, 1.0));
    const float pitch = static_cast<float>(numberArgument(*runtime, state, 4, 1.0));
    const std::filesystem::path path = runtime->m_context->resolveAssetPath(relativePath);

    if (path.empty())
    {
        runtime->m_lastAudioError = "Unsafe module audio path: " + relativePath;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::audio::AudioHandle handle = runtime->m_audio->playOneShot(
        path, *bus, volume, pitch);
    if (handle == heritage::audio::kInvalidAudioHandle)
    {
        runtime->m_lastAudioError = runtime->m_audio->lastError();
    }
    else
    {
        runtime->m_audioHandles.insert(handle);
        runtime->m_lastAudioError.clear();
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaAudioPlayLoop(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_context || !runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::string busName = stringArgument(*runtime, state, 2, "Ambience");
    const auto bus = heritage::audio::parseAudioBus(busName);
    const float volume = static_cast<float>(numberArgument(*runtime, state, 3, 1.0));
    const float pitch = static_cast<float>(numberArgument(*runtime, state, 4, 1.0));
    const std::filesystem::path path = runtime->m_context->resolveAssetPath(relativePath);

    if (path.empty())
    {
        runtime->m_lastAudioError = "Unsafe module audio path: " + relativePath;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::audio::AudioHandle handle = runtime->m_audio->playLoop(
        path, *bus, volume, pitch);
    if (handle == heritage::audio::kInvalidAudioHandle)
    {
        runtime->m_lastAudioError = runtime->m_audio->lastError();
    }
    else
    {
        runtime->m_audioHandles.insert(handle);
        runtime->m_lastAudioError.clear();
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaAudioStop(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;

    const bool owned = runtime->m_audioHandles.contains(handle);
    const bool stopped = owned && runtime->m_audio->stop(handle);
    if (owned)
        runtime->m_audioHandles.erase(handle);

    runtime->m_api.lua_pushboolean(state, stopped ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioStopAll(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    for (const heritage::audio::AudioHandle handle : runtime->m_audioHandles)
        runtime->m_audio->stop(handle);
    runtime->m_audioHandles.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaAudioIsPlaying(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const bool playing = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->isPlaying(handle);
    runtime->m_api.lua_pushboolean(state, playing ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioSetVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const float volume = static_cast<float>(numberArgument(*runtime, state, 2, 1.0));
    const bool changed = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->setHandleVolume(handle, volume);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioSetPitch(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const float pitch = static_cast<float>(numberArgument(*runtime, state, 2, 1.0));
    const bool changed = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->setHandlePitch(handle, pitch);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaAudioSetMasterVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_audio->setMasterVolume(
        static_cast<float>(numberArgument(*runtime, state, 1, 1.0)));
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaAudioGetMasterVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(runtime->m_audio->masterVolume()));
    return 1;
}

int LuaModuleRuntime::luaAudioSetBusVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const std::string busName = stringArgument(*runtime, state, 1);
    const auto bus = heritage::audio::parseAudioBus(busName);
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_audio->setBusVolume(
        *bus,
        static_cast<float>(numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_lastAudioError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaAudioGetBusVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    const std::string busName = stringArgument(*runtime, state, 1);
    const auto bus = heritage::audio::parseAudioBus(busName);
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(runtime->m_audio->busVolume(*bus)));
    return 1;
}

int LuaModuleRuntime::luaAudioGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    std::string error = runtime->m_lastAudioError;
    if (error.empty() && runtime->m_audio)
        error = runtime->m_audio->lastError();
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaModuleRuntime::luaInputIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_input && runtime->m_input->isAvailable() ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputRegisterAction(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string action = stringArgument(*runtime, state, 1);
    const std::string binding = stringArgument(*runtime, state, 2);
    const std::string group = stringArgument(*runtime, state, 3, "Common");
    const bool result = runtime->m_input
        && runtime->m_input->registerAction(action, binding, group);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputDown(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionDown(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputPressed(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionPressed(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->actionReleased(action);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputValue(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const float value = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->actionValue(action)
        : 0.0f;
    runtime->m_api.lua_pushnumber(state, value);
    return 1;
}

int LuaModuleRuntime::luaInputGetBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const std::string binding = runtime->m_input
        ? runtime->m_input->actionBinding(action)
        : std::string{};
    runtime->m_api.lua_pushlstring(state, binding.c_str(), binding.size());
    return 1;
}

int LuaModuleRuntime::luaInputGetBindingCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const std::size_t count = runtime->m_input
        ? runtime->m_input->actionBindingCount(action)
        : 0;
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<heritage::modules::LuaInteger>(count));
    return 1;
}

int LuaModuleRuntime::luaInputGetBindingAt(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const int oneBasedIndex = static_cast<int>(
        numberArgument(*runtime, state, 2, 1.0));
    const std::string binding = runtime->m_input && oneBasedIndex > 0
        ? runtime->m_input->actionBinding(
            action,
            static_cast<std::size_t>(oneBasedIndex - 1))
        : std::string{};
    runtime->m_api.lua_pushlstring(state, binding.c_str(), binding.size());
    return 1;
}

int LuaModuleRuntime::luaInputBind(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const std::string binding = stringArgument(*runtime, state, 2);
    const int argumentCount = runtime->m_api.lua_gettop(state);
    bool result = false;
    if (runtime->m_input)
    {
        if (argumentCount >= 3)
        {
            const int oneBasedIndex = static_cast<int>(
                numberArgument(*runtime, state, 3, 1.0));
            result = oneBasedIndex > 0
                && runtime->m_input->setBinding(
                    action,
                    static_cast<std::size_t>(oneBasedIndex - 1),
                    binding);
        }
        else
        {
            result = runtime->m_input->setBinding(action, binding);
        }
    }
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputAddBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const std::string binding = stringArgument(*runtime, state, 2);
    const bool result = runtime->m_input
        && runtime->m_input->addBinding(action, binding);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputRemoveBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const int oneBasedIndex = static_cast<int>(
        numberArgument(*runtime, state, 2, 1.0));
    const bool result = runtime->m_input
        && oneBasedIndex > 0
        && runtime->m_input->removeBinding(
            action,
            static_cast<std::size_t>(oneBasedIndex - 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputResetBinding(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string action = stringArgument(*runtime, state, 1);
    const bool result = runtime->m_input
        && runtime->m_input->resetBindings(action);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputResetBindings(lua_State* state)
{
    return luaInputResetBinding(state);
}

int LuaModuleRuntime::luaInputKeyDown(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyDown(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputKeyPressed(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyPressed(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputKeyReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string key = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->keyReleased(key);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputMouseDown(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mouseDown(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputMousePressed(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mousePressed(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputMouseReleased(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string button = stringArgument(*runtime, state, 1);
    const bool value = runtime->m_allowInteraction
        && runtime->m_input
        && runtime->m_input->mouseReleased(button);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputMouseDelta(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const double x = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->mouseDeltaX()
        : 0.0;
    const double y = runtime->m_allowInteraction && runtime->m_input
        ? runtime->m_input->mouseDeltaY()
        : 0.0;
    runtime->m_api.lua_pushnumber(state, x);
    runtime->m_api.lua_pushnumber(state, y);
    return 2;
}

int LuaModuleRuntime::luaInputGamepadConnected(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const int ordinal = static_cast<int>(numberArgument(*runtime, state, 1, 0.0));
    const bool value = runtime->m_input
        && runtime->m_input->gamepadConnected(ordinal);
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaInputGetGamepadName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const int ordinal = static_cast<int>(numberArgument(*runtime, state, 1, 0.0));
    const std::string name = runtime->m_input
        ? runtime->m_input->gamepadName(ordinal)
        : std::string{};
    runtime->m_api.lua_pushlstring(state, name.c_str(), name.size());
    return 1;
}

int LuaModuleRuntime::luaInputGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string error = runtime->m_input
        ? runtime->m_input->lastError()
        : std::string("InputSystem is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaModuleRuntime::luaPhysicsIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetFixedDelta(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->fixedDeltaTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetTickRate(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->tickRate())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetTickRate(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const float hertz = static_cast<float>(numberArgument(*runtime, state, 1, 120.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->setTickRate(hertz);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetGravity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 gravity = runtime->m_physics
        ? runtime->m_physics->gravity()
        : heritage::math::Vec3{ 0.0f, 0.0f, 0.0f };
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(gravity.z));
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetGravity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_physics->setGravity({
        static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 2, -9.80665)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
    });
    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics->lastError().empty() ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsIsPaused(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics && runtime->m_physics->paused() ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetPaused(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->setPaused(booleanArgument(*runtime, state, 1, false));
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsRequestSingleStep(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->requestSingleStep();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->timeScale())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetTimeScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const float scale = static_cast<float>(numberArgument(*runtime, state, 1, 1.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->setTimeScale(scale);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->stepCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetSimulationTime(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->simulationTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetInterpolationAlpha(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->interpolationAlpha())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetLastSubstepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->lastSubstepCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetMaximumWorldStepsPerFrame(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->maximumWorldStepsPerFrame())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetPendingWorldStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->pendingWorldStepCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBacklogTime(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->backlogTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetPeakBacklogTime(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->peakBacklogTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsWasOverloadedLastFrame(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(
        state,
        runtime->m_physics
            && runtime->m_physics->overloadedLastFrame()
            ? 1
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetOverloadFrameCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->overloadFrameCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetDroppedTime(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->droppedSimulationTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetClampedTime(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        runtime->m_physics
            ? static_cast<LuaNumber>(runtime->m_physics->clampedSimulationTime())
            : 0.0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsResetClock(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    if (runtime->m_physics)
        runtime->m_physics->resetClock();
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsCreateBody(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_entities)
    {
        runtime->m_lastPhysicsError = "Physics or Entity service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::entities::EntityHandle entity = entityHandleArgument(
        *runtime, state, 1);
    if (!runtime->m_entities->exists(entity))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody requires a valid entity handle.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::BodyMotionType motionType;
    const std::string motionText = stringArgument(
        *runtime, state, 2, "dynamic");
    if (!heritage::physics::parseBodyMotionType(motionText, motionType))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody motion type must be static, kinematic, or dynamic.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::math::Vec3 position{};
    heritage::math::Vec3 rotationDegrees{};
    if (!runtime->m_entities->worldPosition(entity, position)
        || !runtime->m_entities->worldRotationDegrees(entity, rotationDegrees))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody could not read the entity world transform.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::RigidBodyDescription description;
    description.entity = entity;
    description.motionType = motionType;
    description.position = position;
    description.rotationDegrees = rotationDegrees;
    description.mass = static_cast<float>(numberArgument(*runtime, state, 3, 1.0));

    const heritage::physics::BodyHandle handle =
        runtime->m_physics->rigidBodies().create(description);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsDestroyBody(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->destroyBody(
            bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsBodyExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().exists(
            bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const std::size_t count = runtime->m_physics
        ? runtime->m_physics->rigidBodies().count()
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetSleepingBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->rigidBodies().sleepingCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetActiveDynamicBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->rigidBodies().activeDynamicCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsFindBodyByEntity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle handle = runtime->m_physics
        ? runtime->m_physics->rigidBodies().bodyForEntity(
            entityHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyEntity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::entities::EntityHandle entity = runtime->m_physics
        ? runtime->m_physics->rigidBodies().entityForBody(
            bodyHandleArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(entity));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyMotionType(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::BodyMotionType value;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().motionType(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const char* name = heritage::physics::bodyMotionTypeName(value);
    runtime->m_api.lua_pushlstring(state, name, std::char_traits<char>::length(name));
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyMotionType(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::BodyMotionType value;
    const std::string text = stringArgument(*runtime, state, 2);
    if (!heritage::physics::parseBodyMotionType(text, value))
    {
        runtime->m_lastPhysicsError =
            "Physics.SetBodyMotionType requires static, kinematic, or dynamic.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setMotionType(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyMass(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().mass(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyMass(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setMass(
            bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyGravityFactor(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().gravityFactor(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyGravityFactor(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setGravityFactor(
            bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyLinearDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().linearDamping(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyLinearDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setLinearDamping(
            bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 0.02)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyAngularDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().angularDamping(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyAngularDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAngularDamping(
            bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 0.05)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyContinuousCollision(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().continuousCollision(
            bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyContinuousCollision(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setContinuousCollision(
            bodyHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}


int LuaModuleRuntime::luaPhysicsGetBodyPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RigidBodyPose pose;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().pose(
            bodyHandleArgument(*runtime, state, 1), pose))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, pose.position.x);
    runtime->m_api.lua_pushnumber(state, pose.position.y);
    runtime->m_api.lua_pushnumber(state, pose.position.z);
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetBodyPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setPosition(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RigidBodyPose pose;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().pose(
            bodyHandleArgument(*runtime, state, 1), pose))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.x);
    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.y);
    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.z);
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetBodyRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setRotationDegrees(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyLinearVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().linearVelocity(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetBodyLinearVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setLinearVelocity(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyAngularVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().angularVelocityDegrees(
            bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetBodyAngularVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAngularVelocityDegrees(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsApplyBodyForce(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyForce(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsApplyBodyImpulse(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyLinearImpulse(
            bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsApplyBodyImpulseAtPoint(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 impulse{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const heritage::math::Vec3 worldPoint{
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 7, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyImpulseAtPoint(
            bodyHandleArgument(*runtime, state, 1),
            impulse,
            worldPoint);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsApplyBodyAngularImpulse(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 angularImpulse{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyAngularImpulse(
            bodyHandleArgument(*runtime, state, 1),
            angularImpulse);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsClearBodyForces(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().clearForces(
            bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}



int LuaModuleRuntime::luaPhysicsIsBodySleeping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().sleeping(
            bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodySleeping(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setSleeping(
            bodyHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyAllowSleep(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().allowSleep(
            bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetBodyAllowSleep(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAllowSleep(
            bodyHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsWakeBody(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().wake(
            bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsCreateSphereCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::physics::ColliderHandle handle =
        runtime->m_physics->collisions().createSphere(
            bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 0.5)),
            {
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 5, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 6, 0.75)),
            static_cast<float>(numberArgument(*runtime, state, 7, 0.15)),
            booleanArgument(*runtime, state, 8, false),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsCreateBoxCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::physics::ColliderHandle handle =
        runtime->m_physics->collisions().createBox(
            bodyHandleArgument(*runtime, state, 1),
            {
                static_cast<float>(numberArgument(*runtime, state, 2, 0.5)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.5)),
                static_cast<float>(numberArgument(*runtime, state, 4, 0.5))
            },
            {
                static_cast<float>(numberArgument(*runtime, state, 5, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 8, 0.75)),
            static_cast<float>(numberArgument(*runtime, state, 9, 0.15)),
            booleanArgument(*runtime, state, 10, false),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsLoadStaticBoxScene(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_entities || !runtime->m_context)
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene requires Physics, Entity, and Module services.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const bool blenderCoordinates = booleanArgument(*runtime, state, 2, true);
    const std::filesystem::path resolved =
        runtime->m_context->resolveAssetPath(relativePath);
    if (resolved.empty())
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene requires a safe module-asset-relative path.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::string extension = resolved.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".obj")
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene currently supports OBJ proxy documents only.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::vector<heritage::physics::StaticBoxSceneDescriptor> descriptors;
    heritage::physics::StaticBoxSceneSpawn importedSpawn;
    std::string importError;
    if (!heritage::physics::loadStaticBoxSceneFromObj(
            resolved,
            blenderCoordinates,
            descriptors,
            &importedSpawn,
            importError))
    {
        runtime->m_lastPhysicsError = importError;
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    runtime->clearImportedStaticBoxScene();

    for (const auto& descriptor : descriptors)
    {
        const heritage::entities::EntityHandle entity =
            runtime->m_entities->create("Player Scene Collision: " + descriptor.name);
        if (entity == heritage::entities::InvalidEntity
            || !runtime->m_entities->setPosition(entity, descriptor.center))
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create a collision-proxy entity.";
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        heritage::physics::RigidBodyDescription bodyDescription;
        bodyDescription.entity = entity;
        bodyDescription.motionType = heritage::physics::BodyMotionType::Static;
        bodyDescription.position = descriptor.center;
        bodyDescription.mass = 1.0f;
        const heritage::physics::BodyHandle body =
            runtime->m_physics->rigidBodies().create(bodyDescription);
        if (body == heritage::physics::InvalidBody)
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create a static body: "
                + runtime->m_physics->rigidBodies().lastError();
            runtime->m_entities->destroy(entity);
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        const heritage::physics::ColliderHandle collider =
            runtime->m_physics->collisions().createBox(
                body,
                descriptor.halfExtents,
                { 0.0f, 0.0f, 0.0f },
                descriptor.friction,
                descriptor.restitution,
                false,
                runtime->m_physics->rigidBodies());
        if (collider == heritage::physics::InvalidCollider
            || !runtime->m_physics->collisions().setSurface(
                collider,
                descriptor.surfaceMaterial,
                descriptor.surfaceWetness))
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create/configure collider '"
                + descriptor.name + "': "
                + runtime->m_physics->collisions().lastError();
            runtime->m_physics->destroyBody(body);
            runtime->m_entities->destroy(entity);
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        runtime->m_importedStaticSceneEntities.push_back(entity);
        runtime->m_importedStaticSceneBodies.push_back(body);
    }

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_importedStaticSceneBodies.size()));
    if (importedSpawn.found)
    {
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.x);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.y);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.z);
        runtime->m_api.lua_pushstring(
            state,
            importedSpawn.explicitMarker ? "marker" : "auto-ground");
    }
    else
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "origin-fallback");
    }
    return 5;
}

int LuaModuleRuntime::luaPhysicsLoadStaticTriangleScene(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_context)
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene requires Physics and Module services.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::string spawnRelativePath = stringArgument(*runtime, state, 2);
    const bool blenderDefaultObjCoordinates = booleanArgument(*runtime, state, 3, true);
    const std::filesystem::path resolved =
        runtime->m_context->resolveAssetPath(relativePath);
    if (resolved.empty())
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene requires a safe module-asset-relative collision path.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::string extension = resolved.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".obj")
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene currently supports OBJ scene documents only.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::vector<heritage::physics::StaticSceneTriangle> triangles;
    heritage::physics::StaticTriangleSceneSpawn importedSpawn;
    std::string importError;
    if (!heritage::physics::loadStaticTriangleSceneFromObj(
            resolved,
            blenderDefaultObjCoordinates,
            triangles,
            &importedSpawn,
            importError))
    {
        runtime->m_lastPhysicsError = importError;
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    // Some Blender workflows keep SPAWN_PLAYER in the visual OBJ rather than
    // the collision export. If so, use that exact authored marker instead of
    // forcing the creator to duplicate/re-export it just for this bridge.
    if (!spawnRelativePath.empty())
    {
        const std::filesystem::path spawnResolved =
            runtime->m_context->resolveAssetPath(spawnRelativePath);
        if (!spawnResolved.empty())
        {
            heritage::physics::StaticTriangleSceneSpawn visualSpawn;
            std::string spawnError;
            if (heritage::physics::loadStaticTriangleSceneSpawnFromObj(
                    spawnResolved,
                    blenderDefaultObjCoordinates,
                    visualSpawn,
                    spawnError)
                && visualSpawn.found)
            {
                importedSpawn = visualSpawn;
            }
        }
    }

    if (importedSpawn.found)
        heritage::physics::snapStaticTriangleSceneSpawnToSurface(
            triangles, importedSpawn);

    runtime->m_physics->collisions().setStaticSceneTriangles(std::move(triangles));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(
            runtime->m_physics->collisions().staticSceneTriangleCount()));
    if (importedSpawn.found)
    {
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.x);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.y);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.z);
        runtime->m_api.lua_pushstring(
            state,
            importedSpawn.explicitMarker ? "marker" : importedSpawn.sourceName.c_str());
    }
    else
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "origin-fallback");
    }
    return 5;
}

int LuaModuleRuntime::luaPhysicsUnloadStaticTriangleScene(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
        runtime->m_physics->collisions().clearStaticSceneTriangles();
    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetStaticTriangleSceneCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::size_t count = runtime->m_physics
        ? runtime->m_physics->collisions().staticSceneTriangleCount()
        : 0u;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaModuleRuntime::luaPhysicsUnloadStaticBoxScene(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->clearImportedStaticBoxScene();
    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetStaticBoxSceneCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_importedStaticSceneBodies.size()));
    return 1;
}

int LuaModuleRuntime::luaPhysicsDestroyCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().destroy(
            colliderHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsColliderExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().exists(
            colliderHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetColliderCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().count())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyColliderCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().countForBody(
                bodyHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetColliderBody(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle handle = runtime->m_physics
        ? runtime->m_physics->collisions().body(
            colliderHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetColliderShape(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    heritage::physics::ColliderShapeType value;
    if (!runtime->m_physics
        || !runtime->m_physics->collisions().shapeType(
            colliderHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    const char* name = heritage::physics::colliderShapeTypeName(value);
    runtime->m_api.lua_pushstring(state, name);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetColliderMaterial(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setMaterial(
            colliderHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 0.75)),
            static_cast<float>(numberArgument(*runtime, state, 3, 0.15)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetColliderSurface(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SurfaceMaterial material;
    const std::string materialText = stringArgument(
        *runtime, state, 2, "default");
    if (!heritage::physics::parseSurfaceMaterial(materialText, material))
    {
        runtime->m_lastPhysicsError =
            "Physics.SetColliderSurface material must be default, asphalt, "
            "gravel, dirt, grass, snow, ice, kerb/curb, or painted_line.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setSurface(
            colliderHandleArgument(*runtime, state, 1),
            material,
            static_cast<float>(numberArgument(*runtime, state, 3, 0.0)));
    if (!result && runtime->m_physics && runtime->m_lastPhysicsError.empty())
        runtime->m_lastPhysicsError = runtime->m_physics->collisions().lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetColliderSurface(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SurfaceMaterial material =
        heritage::physics::SurfaceMaterial::Default;
    float wetness = 0.0f;
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().surface(
            colliderHandleArgument(*runtime, state, 1),
            material,
            wetness);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(material));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(material));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(wetness));
    return 3;
}

int LuaModuleRuntime::luaPhysicsSetColliderTrigger(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setTrigger(
            colliderHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetColliderFilter(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const std::uint32_t layer = static_cast<std::uint32_t>(
        numberArgument(*runtime, state, 2, 1.0));
    const std::uint32_t mask = static_cast<std::uint32_t>(
        numberArgument(*runtime, state, 3, 4294967295.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setFilter(
            colliderHandleArgument(*runtime, state, 1), layer, mask);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsRaycast(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RaycastHit hit;
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = numberArgument(
            *runtime, state, 8, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = booleanArgument(*runtime, state, 9, false);
        filter.ignoredBody = bodyHandleArgument(*runtime, state, 10);

        result = runtime->m_physics->collisions().raycast(
            {
                static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
            },
            {
                static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 5, -1.0)),
                static_cast<float>(numberArgument(*runtime, state, 6, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 7, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies(),
            hit);
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.collider));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.body));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.distance));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.z));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.z));
    runtime->m_api.lua_pushboolean(state, hit.trigger ? 1 : 0);
    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(hit.surfaceMaterial));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(hit.surfaceMaterial));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(hit.surfaceWetness));
    return 14;
}

int LuaModuleRuntime::luaPhysicsRaycastAny(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = numberArgument(
            *runtime, state, 8, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = booleanArgument(*runtime, state, 9, false);
        filter.ignoredBody = bodyHandleArgument(*runtime, state, 10);

        result = runtime->m_physics->collisions().raycastAny(
            {
                static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
            },
            {
                static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 5, -1.0)),
                static_cast<float>(numberArgument(*runtime, state, 6, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 7, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSphereCast(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SphereCastHit hit;
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = numberArgument(
            *runtime, state, 9, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = booleanArgument(*runtime, state, 10, false);
        filter.ignoredBody = bodyHandleArgument(*runtime, state, 11);

        result = runtime->m_physics->collisions().sphereCast(
            {
                static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 4, 0.5)),
            {
                static_cast<float>(numberArgument(*runtime, state, 5, 1.0)),
                static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 8, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies(),
            hit);
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.collider));
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(hit.body));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.distance));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.point.z));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.x));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.y));
    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(hit.normal.z));
    runtime->m_api.lua_pushboolean(state, hit.trigger ? 1 : 0);
    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(hit.surfaceMaterial));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(hit.surfaceMaterial));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(hit.surfaceWetness));
    return 14;
}

int LuaModuleRuntime::luaPhysicsSphereCastAny(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool result = false;
    if (runtime->m_physics)
    {
        const double rawMask = numberArgument(
            *runtime, state, 9, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = booleanArgument(*runtime, state, 10, false);
        filter.ignoredBody = bodyHandleArgument(*runtime, state, 11);

        result = runtime->m_physics->collisions().sphereCastAny(
            {
                static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 4, 0.5)),
            {
                static_cast<float>(numberArgument(*runtime, state, 5, 1.0)),
                static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 8, 1000.0)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}


int LuaModuleRuntime::luaPhysicsOverlapSphereCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    std::size_t result = 0;
    if (runtime->m_physics)
    {
        const double rawMask = numberArgument(
            *runtime, state, 5, 4294967295.0);
        heritage::physics::CollisionQueryFilter filter;
        filter.layerMask = rawMask <= 0.0
            ? 0u
            : (rawMask >= 4294967295.0
                ? 0xffffffffu
                : static_cast<std::uint32_t>(rawMask));
        filter.includeTriggers = booleanArgument(*runtime, state, 6, false);
        filter.ignoredBody = bodyHandleArgument(*runtime, state, 7);

        result = runtime->m_physics->collisions().overlapSphereCount(
            {
                static_cast<float>(numberArgument(*runtime, state, 1, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
                static_cast<float>(numberArgument(*runtime, state, 3, 0.0))
            },
            static_cast<float>(numberArgument(*runtime, state, 4, 0.5)),
            filter,
            runtime->m_physics->rigidBodies());
    }
    else
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(result));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetLastQueryCandidateCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().lastQueryCandidateCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetLastQueryExactTestCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().lastQueryExactTestCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().contactCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBodyContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().contactCountForBody(
                bodyHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsIsBodyTouching(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().bodyTouching(
            bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetBroadphaseCandidateCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().broadphaseCandidateCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetNarrowphaseTestCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().narrowphaseTestCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetResolvedContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().resolvedContactCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetSimulationIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().simulationIslandCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetActiveIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().activeIslandCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetSleepingIslandCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().sleepingIslandCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetWarmStartedContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().warmStartedContactCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetPersistentContactCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().persistentContactCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContinuousCollisionBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionBodyCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContinuousCollisionSweepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionSweepCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContinuousCollisionHitCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionHitCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContinuousCollisionClampedBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionClampedBodyCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetContinuousCollisionUnsupportedBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->collisions().continuousCollisionUnsupportedBodyCount())
            : 0);
    return 1;
}


int LuaModuleRuntime::luaPhysicsCreateSpringConstraint(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::SpringConstraintDescription description;
    description.bodyA = bodyHandleArgument(*runtime, state, 1);
    description.bodyB = bodyHandleArgument(*runtime, state, 2);
    description.localAnchorA = {
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0))
    };
    description.anchorB = {
        static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 8, 0.0))
    };
    description.restLength = static_cast<float>(
        numberArgument(*runtime, state, 9, 1.0));
    description.stiffness = static_cast<float>(
        numberArgument(*runtime, state, 10, 1000.0));
    description.damping = static_cast<float>(
        numberArgument(*runtime, state, 11, 100.0));
    description.maximumForce = static_cast<float>(
        numberArgument(*runtime, state, 12, 1000000.0));
    description.enabled = booleanArgument(*runtime, state, 13, true);

    const heritage::physics::ConstraintHandle handle =
        runtime->m_physics->constraints().createSpring(
            description,
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaPhysicsDestroyConstraint(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().destroy(
            constraintHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsConstraintExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().exists(
            constraintHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->constraints().count())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetEnabledConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->constraints().enabledCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetActiveConstraintCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->constraints().activeCount())
            : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetConstraintEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().setEnabled(
            constraintHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetConstraintEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().enabled(
            constraintHandleArgument(*runtime, state, 1),
            value);
    if (result)
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    else
        runtime->m_api.lua_pushnil(state);
    return 1;
}

int LuaModuleRuntime::luaPhysicsSetSpringConstraintProperties(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().setSpringProperties(
            constraintHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 1.0)),
            static_cast<float>(numberArgument(*runtime, state, 3, 1000.0)),
            static_cast<float>(numberArgument(*runtime, state, 4, 100.0)),
            static_cast<float>(numberArgument(*runtime, state, 5, 1000000.0)),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetSpringConstraintState(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    heritage::physics::SpringConstraintState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->constraints().state(
            constraintHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 4; ++index)
            runtime->m_api.lua_pushnil(state);
        return 4;
    }
    runtime->m_api.lua_pushnumber(state, value.currentLength);
    runtime->m_api.lua_pushnumber(state, value.extension);
    runtime->m_api.lua_pushnumber(state, value.relativeSpeed);
    runtime->m_api.lua_pushnumber(state, value.appliedForce);
    return 4;
}

int LuaModuleRuntime::luaPhysicsGetConstraintBodyA(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle body = runtime->m_physics
        ? runtime->m_physics->constraints().bodyA(
            constraintHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(body));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetConstraintBodyB(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle body = runtime->m_physics
        ? runtime->m_physics->constraints().bodyB(
            constraintHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(body));
    return 1;
}

int LuaModuleRuntime::luaPhysicsGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string error = !runtime->m_lastPhysicsError.empty()
        ? runtime->m_lastPhysicsError
        : (runtime->m_physics
            ? runtime->m_physics->lastError()
            : std::string("PhysicsWorld is unavailable."));
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}


int LuaModuleRuntime::luaVehicleIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushboolean(state, runtime->m_physics ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleCompileDefinitionV2(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
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

int LuaModuleRuntime::luaVehicleCreateFromDefinitionV2(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
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
    settings.vehicle.chassisBody = bodyHandleArgument(*runtime, state, 2);
    settings.vehicle.highRateHertz = static_cast<float>(
        numberArgument(*runtime, state, 3, 1000.0));
    settings.vehicle.maximumDriveForce = static_cast<float>(
        numberArgument(*runtime, state, 4, 7000.0));
    settings.vehicle.maximumBrakeForce = static_cast<float>(
        numberArgument(*runtime, state, 5, 12000.0));
    settings.vehicle.maximumSteerAngleDegrees = static_cast<float>(
        numberArgument(*runtime, state, 6, 38.0));
    settings.vehicle.tireFriction = static_cast<float>(
        numberArgument(*runtime, state, 7, 1.15));
    settings.vehicle.lateralStiffness = static_cast<float>(
        numberArgument(*runtime, state, 8, 11000.0));
    settings.vehicle.rollingResistance = static_cast<float>(
        numberArgument(*runtime, state, 9, 90.0));

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

int LuaModuleRuntime::luaVehicleCreate(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_physics)
    {
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::vehicles::VehicleDescription description;
    description.chassisBody = bodyHandleArgument(*runtime, state, 1);
    description.highRateHertz = static_cast<float>(numberArgument(*runtime, state, 2, 1000.0));
    description.maximumDriveForce = static_cast<float>(numberArgument(*runtime, state, 3, 7000.0));
    description.maximumBrakeForce = static_cast<float>(numberArgument(*runtime, state, 4, 12000.0));
    description.maximumSteerAngleDegrees = static_cast<float>(numberArgument(*runtime, state, 5, 38.0));
    description.tireFriction = static_cast<float>(numberArgument(*runtime, state, 6, 1.15));
    description.lateralStiffness = static_cast<float>(numberArgument(*runtime, state, 7, 11000.0));
    description.rollingResistance = static_cast<float>(numberArgument(*runtime, state, 8, 90.0));
    const heritage::vehicles::VehicleHandle handle = runtime->m_physics->vehicles().create(
        description, runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaVehicleDestroy(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().destroy(
        vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().exists(
        vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().count()) : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleAddWheel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelDescription description;
    description.localMount = {
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.8)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0)) };
    description.localSuspensionDirection = {
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 6, -1.0)),
        static_cast<float>(numberArgument(*runtime, state, 7, 0.0)) };
    description.radius = static_cast<float>(numberArgument(*runtime, state, 8, 0.35));
    description.restLength = static_cast<float>(numberArgument(*runtime, state, 9, 0.50));
    description.maximumCompression = static_cast<float>(numberArgument(*runtime, state, 10, 0.18));
    description.maximumDroop = static_cast<float>(numberArgument(*runtime, state, 11, 0.15));
    description.springRate = static_cast<float>(numberArgument(*runtime, state, 12, 35000.0));
    description.bumpDamping = static_cast<float>(numberArgument(*runtime, state, 13, 3200.0));
    description.reboundDamping = static_cast<float>(numberArgument(*runtime, state, 14, 4200.0));
    description.driveFactor = static_cast<float>(numberArgument(*runtime, state, 15, 0.0));
    description.steerFactor = static_cast<float>(numberArgument(*runtime, state, 16, 0.0));
    description.brakeFactor = static_cast<float>(numberArgument(*runtime, state, 17, 1.0));
    description.handbrakeFactor = static_cast<float>(numberArgument(*runtime, state, 18, 0.0));
    description.springPreload = static_cast<float>(
        numberArgument(*runtime, state, 19, 0.0));
    description.springProgression = static_cast<float>(
        numberArgument(*runtime, state, 20, 0.0));
    description.bumpHighSpeedDamping = static_cast<float>(
        numberArgument(*runtime, state, 21, description.bumpDamping));
    description.bumpDampingKneeVelocity = static_cast<float>(
        numberArgument(*runtime, state, 22, 1.0));
    description.reboundHighSpeedDamping = static_cast<float>(
        numberArgument(*runtime, state, 23, description.reboundDamping));
    description.reboundDampingKneeVelocity = static_cast<float>(
        numberArgument(*runtime, state, 24, 1.0));
    description.bumpStopEngagement = static_cast<float>(
        numberArgument(*runtime, state, 25, description.maximumCompression));
    description.bumpStopRate = static_cast<float>(
        numberArgument(*runtime, state, 26, 0.0));
    description.bumpStopProgression = static_cast<float>(
        numberArgument(*runtime, state, 27, 0.0));
    description.droopStopEngagement = static_cast<float>(
        numberArgument(*runtime, state, 28, description.maximumDroop));
    description.droopStopRate = static_cast<float>(
        numberArgument(*runtime, state, 29, 0.0));
    description.suspensionMotionRatio = static_cast<float>(
        numberArgument(*runtime, state, 30, 1.0));
    description.maximumSuspensionForce = static_cast<float>(
        numberArgument(*runtime, state, 31, 250000.0));
    description.effectiveUnsprungMass = static_cast<float>(
        numberArgument(*runtime, state, 32, 0.0));
    description.tireRadialStiffness = static_cast<float>(
        numberArgument(*runtime, state, 33, 220000.0));
    description.tireRadialDamping = static_cast<float>(
        numberArgument(*runtime, state, 34, 1800.0));
    description.maximumTireDeflection = static_cast<float>(
        numberArgument(*runtime, state, 35, 0.08));
    description.maximumTireNormalForce = static_cast<float>(
        numberArgument(*runtime, state, 36, 250000.0));
    description.localSteeringAxis = {
        static_cast<float>(numberArgument(*runtime, state, 37, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 38, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 39, 0.0)) };
    description.staticCamberDegrees = static_cast<float>(
        numberArgument(*runtime, state, 40, 0.0));
    description.camberGainDegreesPerM = static_cast<float>(
        numberArgument(*runtime, state, 41, 0.0));
    description.camberProgressionDegreesPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 42, 0.0));
    description.staticToeDegrees = static_cast<float>(
        numberArgument(*runtime, state, 43, 0.0));
    description.toeGainDegreesPerM = static_cast<float>(
        numberArgument(*runtime, state, 44, 0.0));
    description.toeProgressionDegreesPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 45, 0.0));
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().addWheel(
        vehicleHandleArgument(*runtime, state, 1), description);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetWheelCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().wheelCount(
            vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetWheelSuspensionModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionModelDescription value;
    value.springPreloadN = static_cast<float>(
        numberArgument(*runtime, state, 3, 0.0));
    value.springRateNPerM = static_cast<float>(
        numberArgument(*runtime, state, 4, 35000.0));
    value.springProgressionNPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 5, 0.0));
    value.bumpDampingNsPerM = static_cast<float>(
        numberArgument(*runtime, state, 6, 3200.0));
    value.bumpHighSpeedDampingNsPerM = static_cast<float>(
        numberArgument(*runtime, state, 7, value.bumpDampingNsPerM));
    value.bumpDampingKneeVelocityMps = static_cast<float>(
        numberArgument(*runtime, state, 8, 1.0));
    value.reboundDampingNsPerM = static_cast<float>(
        numberArgument(*runtime, state, 9, 4200.0));
    value.reboundHighSpeedDampingNsPerM = static_cast<float>(
        numberArgument(*runtime, state, 10, value.reboundDampingNsPerM));
    value.reboundDampingKneeVelocityMps = static_cast<float>(
        numberArgument(*runtime, state, 11, 1.0));
    value.bumpStopEngagementM = static_cast<float>(
        numberArgument(*runtime, state, 12, 0.18));
    value.bumpStopRateNPerM = static_cast<float>(
        numberArgument(*runtime, state, 13, 0.0));
    value.bumpStopProgressionNPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 14, 0.0));
    value.droopStopEngagementM = static_cast<float>(
        numberArgument(*runtime, state, 15, 0.15));
    value.droopStopRateNPerM = static_cast<float>(
        numberArgument(*runtime, state, 16, 0.0));
    value.motionRatio = static_cast<float>(
        numberArgument(*runtime, state, 17, 1.0));
    value.maximumForceN = static_cast<float>(
        numberArgument(*runtime, state, 18, 250000.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelSuspensionModel(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetWheelSuspensionModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionModelDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelSuspensionModel(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 17; ++index)
            runtime->m_api.lua_pushnil(state);
        return 17;
    }

    runtime->m_api.lua_pushstring(state, heritage::vehicles::suspensionProviderId(
        value.provider));
    runtime->m_api.lua_pushnumber(state, value.springPreloadN);
    runtime->m_api.lua_pushnumber(state, value.springRateNPerM);
    runtime->m_api.lua_pushnumber(state, value.springProgressionNPerM2);
    runtime->m_api.lua_pushnumber(state, value.bumpDampingNsPerM);
    runtime->m_api.lua_pushnumber(state, value.bumpHighSpeedDampingNsPerM);
    runtime->m_api.lua_pushnumber(state, value.bumpDampingKneeVelocityMps);
    runtime->m_api.lua_pushnumber(state, value.reboundDampingNsPerM);
    runtime->m_api.lua_pushnumber(state, value.reboundHighSpeedDampingNsPerM);
    runtime->m_api.lua_pushnumber(state, value.reboundDampingKneeVelocityMps);
    runtime->m_api.lua_pushnumber(state, value.bumpStopEngagementM);
    runtime->m_api.lua_pushnumber(state, value.bumpStopRateNPerM);
    runtime->m_api.lua_pushnumber(state, value.bumpStopProgressionNPerM2);
    runtime->m_api.lua_pushnumber(state, value.droopStopEngagementM);
    runtime->m_api.lua_pushnumber(state, value.droopStopRateNPerM);
    runtime->m_api.lua_pushnumber(state, value.motionRatio);
    runtime->m_api.lua_pushnumber(state, value.maximumForceN);
    return 17;
}

int LuaModuleRuntime::luaVehicleSetWheelSuspensionGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionGeometryDescription value;
    value.localSteeringAxis = {
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0)) };
    value.staticCamberDegrees = static_cast<float>(
        numberArgument(*runtime, state, 6, 0.0));
    value.camberGainDegreesPerM = static_cast<float>(
        numberArgument(*runtime, state, 7, 0.0));
    value.camberProgressionDegreesPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 8, 0.0));
    value.staticToeDegrees = static_cast<float>(
        numberArgument(*runtime, state, 9, 0.0));
    value.toeGainDegreesPerM = static_cast<float>(
        numberArgument(*runtime, state, 10, 0.0));
    value.toeProgressionDegreesPerM2 = static_cast<float>(
        numberArgument(*runtime, state, 11, 0.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelSuspensionGeometry(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetWheelSuspensionGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionGeometryDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelSuspensionGeometry(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 10; ++index)
            runtime->m_api.lua_pushnil(state);
        return 10;
    }
    runtime->m_api.lua_pushstring(state, heritage::vehicles::suspensionProviderId(
        value.provider));
    runtime->m_api.lua_pushnumber(state, value.localSteeringAxis.x);
    runtime->m_api.lua_pushnumber(state, value.localSteeringAxis.y);
    runtime->m_api.lua_pushnumber(state, value.localSteeringAxis.z);
    runtime->m_api.lua_pushnumber(state, value.staticCamberDegrees);
    runtime->m_api.lua_pushnumber(state, value.camberGainDegreesPerM);
    runtime->m_api.lua_pushnumber(state, value.camberProgressionDegreesPerM2);
    runtime->m_api.lua_pushnumber(state, value.staticToeDegrees);
    runtime->m_api.lua_pushnumber(state, value.toeGainDegreesPerM);
    runtime->m_api.lua_pushnumber(state, value.toeProgressionDegreesPerM2);
    return 10;
}

int LuaModuleRuntime::luaVehicleSetWheelUnsprungMassModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::UnsprungMassDescription value;
    value.effectiveMassKg = static_cast<float>(
        numberArgument(*runtime, state, 3, 0.0));
    value.tireRadialStiffnessNPerM = static_cast<float>(
        numberArgument(*runtime, state, 4, 220000.0));
    value.tireRadialDampingNsPerM = static_cast<float>(
        numberArgument(*runtime, state, 5, 1800.0));
    value.maximumTireDeflectionM = static_cast<float>(
        numberArgument(*runtime, state, 6, 0.08));
    value.maximumNormalForceN = static_cast<float>(
        numberArgument(*runtime, state, 7, 250000.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelUnsprungMassModel(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetWheelUnsprungMassModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::UnsprungMassDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelUnsprungMassModel(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 5; ++index)
            runtime->m_api.lua_pushnil(state);
        return 5;
    }
    runtime->m_api.lua_pushnumber(state, value.effectiveMassKg);
    runtime->m_api.lua_pushnumber(state, value.tireRadialStiffnessNPerM);
    runtime->m_api.lua_pushnumber(state, value.tireRadialDampingNsPerM);
    runtime->m_api.lua_pushnumber(state, value.maximumTireDeflectionM);
    runtime->m_api.lua_pushnumber(state, value.maximumNormalForceN);
    return 5;
}

int LuaModuleRuntime::luaVehicleSetInputs(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setInputs(
        vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetWheelBrakeFactors(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelBrakeFactors(
            vehicleHandleArgument(*runtime, state, 1),
            wheelIndex,
            static_cast<float>(numberArgument(*runtime, state, 3, 1.0)),
            static_cast<float>(numberArgument(*runtime, state, 4, 0.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetDriverAids(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setDriverAids(
            vehicleHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true),
            booleanArgument(*runtime, state, 3, true),
            static_cast<float>(numberArgument(*runtime, state, 4, 0.16)),
            static_cast<float>(numberArgument(*runtime, state, 5, 0.12)),
            static_cast<float>(numberArgument(*runtime, state, 6, 2.5)),
            static_cast<float>(numberArgument(*runtime, state, 7, 18.0)),
            static_cast<float>(numberArgument(*runtime, state, 8, 3500.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetDriverAidState(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::DriverAidState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().driverAidState(
            vehicleHandleArgument(*runtime, state, 1),
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

int LuaModuleRuntime::luaVehicleSetTuning(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setTuning(
        vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(numberArgument(*runtime, state, 2, 7000.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 12000.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 38.0)),
        static_cast<float>(numberArgument(*runtime, state, 5, 1.15)),
        static_cast<float>(numberArgument(*runtime, state, 6, 11000.0)),
        static_cast<float>(numberArgument(*runtime, state, 7, 90.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setTireModel(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 3500.0)),
            static_cast<float>(numberArgument(*runtime, state, 3, 1.15)),
            static_cast<float>(numberArgument(*runtime, state, 4, 90000.0)),
            static_cast<float>(numberArgument(*runtime, state, 5, 80000.0)),
            static_cast<float>(numberArgument(*runtime, state, 6, 0.12)),
            static_cast<float>(numberArgument(*runtime, state, 7, 0.35)),
            static_cast<float>(numberArgument(*runtime, state, 8, 0.45)),
            static_cast<float>(numberArgument(*runtime, state, 9, 1.55)),
            static_cast<float>(numberArgument(*runtime, state, 10, 0.075)),
            static_cast<float>(numberArgument(*runtime, state, 11, 0.85)),
            static_cast<float>(numberArgument(*runtime, state, 12, 1.65)),
            static_cast<float>(numberArgument(*runtime, state, 13, 1.30)),
            static_cast<float>(numberArgument(*runtime, state, 14, 0.20)),
            static_cast<float>(numberArgument(*runtime, state, 15, 0.15)),
            static_cast<float>(numberArgument(*runtime, state, 16, 2.0)),
            static_cast<float>(numberArgument(*runtime, state, 17, 0.70)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetWheelTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelTireModel(
            vehicleHandleArgument(*runtime, state, 1),
            wheelIndex,
            static_cast<float>(numberArgument(*runtime, state, 3, 3500.0)),
            static_cast<float>(numberArgument(*runtime, state, 4, 1.15)),
            static_cast<float>(numberArgument(*runtime, state, 5, 90000.0)),
            static_cast<float>(numberArgument(*runtime, state, 6, 80000.0)),
            static_cast<float>(numberArgument(*runtime, state, 7, 0.12)),
            static_cast<float>(numberArgument(*runtime, state, 8, 0.35)),
            static_cast<float>(numberArgument(*runtime, state, 9, 0.45)),
            static_cast<float>(numberArgument(*runtime, state, 10, 1.55)),
            static_cast<float>(numberArgument(*runtime, state, 11, 0.075)),
            static_cast<float>(numberArgument(*runtime, state, 12, 0.85)),
            static_cast<float>(numberArgument(*runtime, state, 13, 1.65)),
            static_cast<float>(numberArgument(*runtime, state, 14, 1.30)),
            static_cast<float>(numberArgument(*runtime, state, 15, 0.20)),
            static_cast<float>(numberArgument(*runtime, state, 16, 0.15)),
            static_cast<float>(numberArgument(*runtime, state, 17, 2.0)),
            static_cast<float>(numberArgument(*runtime, state, 18, 0.70)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetWheelTireModel(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::TireModelDescription value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelTireModel(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 16; ++index)
            runtime->m_api.lua_pushnil(state);
        return 16;
    }

    runtime->m_api.lua_pushnumber(state, value.nominalLoad);
    runtime->m_api.lua_pushnumber(state, value.peakFriction);
    runtime->m_api.lua_pushnumber(state, value.longitudinalStiffness);
    runtime->m_api.lua_pushnumber(state, value.corneringStiffness);
    runtime->m_api.lua_pushnumber(state, value.loadSensitivity);
    runtime->m_api.lua_pushnumber(state, value.longitudinalRelaxationLength);
    runtime->m_api.lua_pushnumber(state, value.lateralRelaxationLength);
    runtime->m_api.lua_pushnumber(state, value.wheelInertia);
    runtime->m_api.lua_pushnumber(state, value.pneumaticTrail);
    runtime->m_api.lua_pushnumber(state, value.stiffnessLoadExponent);
    runtime->m_api.lua_pushnumber(state, value.longitudinalShapeFactor);
    runtime->m_api.lua_pushnumber(state, value.lateralShapeFactor);
    runtime->m_api.lua_pushnumber(state, value.longitudinalCurvatureFactor);
    runtime->m_api.lua_pushnumber(state, value.lateralCurvatureFactor);
    runtime->m_api.lua_pushnumber(state, value.combinedSlipExponent);
    runtime->m_api.lua_pushnumber(state, value.pneumaticTrailFalloff);
    return 16;
}

int LuaModuleRuntime::luaVehicleSetSurfacePreset(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger rawSurface = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const bool result = converted
        && runtime->m_physics
        && runtime->m_physics->vehicles().setSurfacePreset(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<heritage::vehicles::TireSurface>(
                static_cast<int>(rawSurface)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetSurfacePreset(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->vehicles().surfacePreset(
                vehicleHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetHighRateHertz(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().setHighRateHertz(
        vehicleHandleArgument(*runtime, state, 1),
        static_cast<float>(numberArgument(*runtime, state, 2, 1000.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetSteeringGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setSteeringGeometry(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 1.0)),
            static_cast<float>(numberArgument(*runtime, state, 3, 260.0)),
            static_cast<float>(numberArgument(*runtime, state, 4, 360.0)),
            static_cast<float>(numberArgument(*runtime, state, 5, 0.35)),
            static_cast<float>(numberArgument(*runtime, state, 6, 40.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetSteeringState(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::SteeringState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().steeringState(
            vehicleHandleArgument(*runtime, state, 1),
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

int LuaModuleRuntime::luaVehicleSetPowertrain(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setPowertrain(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 900.0)),
            static_cast<float>(numberArgument(*runtime, state, 3, 7000.0)),
            static_cast<float>(numberArgument(*runtime, state, 4, 250.0)),
            static_cast<float>(numberArgument(*runtime, state, 5, 70.0)),
            static_cast<float>(numberArgument(*runtime, state, 6, 3.90)),
            static_cast<float>(numberArgument(*runtime, state, 7, 0.88)),
            static_cast<float>(numberArgument(*runtime, state, 8, 0.22)),
            static_cast<float>(numberArgument(*runtime, state, 9, 5.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetGearRatios(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;

    std::vector<float> forwardRatios;
    const int argumentCount = runtime->m_api.lua_gettop(state);
    for (int argument = 3; argument <= argumentCount; ++argument)
    {
        forwardRatios.push_back(static_cast<float>(
            numberArgument(*runtime, state, argument, 0.0)));
    }
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setGearRatios(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, -3.20)),
            forwardRatios);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetDifferential(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
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
            vehicleHandleArgument(*runtime, state, 1),
            mode,
            static_cast<float>(numberArgument(*runtime, state, 3, 2.25)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleSetGear(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger gear = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const bool result = converted
        && runtime->m_physics
        && runtime->m_physics->vehicles().setGear(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<int>(gear));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleShiftUp(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().shiftUp(
            vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleShiftDown(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().shiftDown(
            vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetDrivetrainState(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::DrivetrainState value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().drivetrainState(
            vehicleHandleArgument(*runtime, state, 1),
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

int LuaModuleRuntime::luaVehicleGetForwardGearCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->vehicles().forwardGearCount(
                    vehicleHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetHighRateHertz(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushnumber(state, runtime->m_physics
        ? runtime->m_physics->vehicles().highRateHertz(vehicleHandleArgument(*runtime, state, 1)) : 0.0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetSpeed(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushnumber(state, runtime->m_physics
        ? runtime->m_physics->vehicles().speed(vehicleHandleArgument(*runtime, state, 1)) : 0.0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetGroundedWheelCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().groundedWheelCount(
            vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetLastHighRateStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? runtime->m_physics->vehicles().lastHighRateStepCount(
            vehicleHandleArgument(*runtime, state, 1)) : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetTotalHighRateStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().totalHighRateStepCount(
            vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleStartDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().startDynamicsLabCapture(
            vehicleHandleArgument(*runtime, state, 1),
            static_cast<float>(numberArgument(*runtime, state, 2, 20.0)),
            static_cast<float>(numberArgument(*runtime, state, 3, 1000.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleStopDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().stopDynamicsLabCapture(
            vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleClearDynamicsLab(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().clearDynamicsLabCapture(
            vehicleHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaVehicleGetDynamicsLabSummary(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::DynamicsLabSummary value;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().dynamicsLabSummary(
            vehicleHandleArgument(*runtime, state, 1),
            value);
    if (!result)
    {
        for (int index = 0; index < 25; ++index)
            runtime->m_api.lua_pushnil(state);
        return 25;
    }

    runtime->m_api.lua_pushboolean(state, value.recording ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, value.captureComplete ? 1 : 0);
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.sampleCount));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.sampleCapacity));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.wheelCount));
    runtime->m_api.lua_pushnumber(state, value.durationSeconds);
    runtime->m_api.lua_pushnumber(state, value.requestedCaptureHertz);
    runtime->m_api.lua_pushnumber(state, value.peakSpeedKph);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteRollRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsolutePitchRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteYawRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteSuspensionVelocityMps);
    runtime->m_api.lua_pushnumber(state, value.peakAbsoluteSlipRatio);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteSlipAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.peakGripUtilizationPercent);
    runtime->m_api.lua_pushnumber(
        state, value.minimumGroundedNormalForceNewtons);
    runtime->m_api.lua_pushnumber(
        state, value.maximumGroundedNormalForceNewtons);
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.groundContactLossEvents));
    runtime->m_api.lua_pushnumber(
        state, value.peakSuspensionTravelStopForceNewtons);
    runtime->m_api.lua_pushnumber(
        state, value.peakDamperDissipationWatts);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteUnsprungVelocityMps);
    runtime->m_api.lua_pushnumber(
        state, value.peakTireDeflectionMillimeters);
    runtime->m_api.lua_pushnumber(
        state, value.peakTireRadialDissipationWatts);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteCamberDegrees);
    runtime->m_api.lua_pushnumber(
        state, value.peakAbsoluteToeDegrees);
    return 25;
}

int LuaModuleRuntime::luaVehicleGetDynamicsLabSeries(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;

    heritage::vehicles::DynamicsLabMetric metric;
    if (!heritage::vehicles::parseDynamicsLabMetric(
            stringArgument(*runtime, state, 2),
            metric))
    {
        return 0;
    }

    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 3, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : 0u;
    const std::size_t maximumPoints = static_cast<std::size_t>(
        (std::max)(16.0, (std::min)(240.0,
            numberArgument(*runtime, state, 4, 180.0))));

    std::vector<float> values;
    if (!runtime->m_physics->vehicles().dynamicsLabMetricSeries(
            vehicleHandleArgument(*runtime, state, 1),
            metric,
            wheelIndex,
            maximumPoints,
            values))
    {
        return 0;
    }
    if (!runtime->m_api.lua_checkstack(
            state,
            static_cast<int>(values.size())))
    {
        return 0;
    }
    for (const float value : values)
        runtime->m_api.lua_pushnumber(state, value);
    return static_cast<int>(values.size());
}

int LuaModuleRuntime::luaVehicleExportDynamicsLabCsv(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string requestedName = stringArgument(
        *runtime, state, 2, "latest_vehicle_dynamics.csv");
    const std::filesystem::path name(requestedName);
    const bool safeName = !requestedName.empty()
        && !name.has_parent_path()
        && name.filename() == name
        && name.extension() == ".csv";
    if (!runtime->m_physics || !runtime->m_context || !safeName)
    {
        const std::string message = !safeName
            ? "Dynamics lab export name must be a plain .csv filename."
            : "Dynamics lab export requires Vehicle and Module services.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::filesystem::path path = runtime->m_context->resolveSavePath(
        std::filesystem::path("DynamicsLab") / name);
    const bool result = !path.empty()
        && runtime->m_physics->vehicles().exportDynamicsLabCsv(
            vehicleHandleArgument(*runtime, state, 1),
            path);
    const std::string message = result
        ? path.string()
        : runtime->m_physics->vehicles().lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
}

int LuaModuleRuntime::luaVehicleGetWheelState(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelState value;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().wheelState(
        vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 51; ++index) runtime->m_api.lua_pushnil(state);
        return 51;
    }
    runtime->m_api.lua_pushboolean(state, value.grounded ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.suspensionLength);
    runtime->m_api.lua_pushnumber(state, value.compression);
    runtime->m_api.lua_pushnumber(state, value.compressionVelocity);
    runtime->m_api.lua_pushnumber(state, value.normalForce);
    runtime->m_api.lua_pushnumber(state, value.longitudinalForce);
    runtime->m_api.lua_pushnumber(state, value.lateralForce);
    runtime->m_api.lua_pushnumber(state, value.steerAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.wheelAngularVelocity);
    runtime->m_api.lua_pushnumber(state, value.wheelRotationDegrees);
    runtime->m_api.lua_pushnumber(state, value.worldCenter.x);
    runtime->m_api.lua_pushnumber(state, value.worldCenter.y);
    runtime->m_api.lua_pushnumber(state, value.worldCenter.z);
    runtime->m_api.lua_pushnumber(state, value.contactPoint.x);
    runtime->m_api.lua_pushnumber(state, value.contactPoint.y);
    runtime->m_api.lua_pushnumber(state, value.contactPoint.z);
    runtime->m_api.lua_pushnumber(state, value.longitudinalSpeed);
    runtime->m_api.lua_pushnumber(state, value.lateralSpeed);
    runtime->m_api.lua_pushnumber(state, value.slipRatio);
    runtime->m_api.lua_pushnumber(state, value.slipAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.relaxedSlipRatio);
    runtime->m_api.lua_pushnumber(state, value.relaxedSlipAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.effectiveFriction);
    runtime->m_api.lua_pushnumber(state, value.gripUtilization);
    runtime->m_api.lua_pushnumber(state, value.pureLongitudinalForce);
    runtime->m_api.lua_pushnumber(state, value.pureLateralForce);
    runtime->m_api.lua_pushnumber(state, value.combinedSlipScale);
    runtime->m_api.lua_pushnumber(state, value.pneumaticTrail);
    runtime->m_api.lua_pushnumber(state, value.aligningTorque);
    runtime->m_api.lua_pushnumber(state, value.appliedDriveTorque);
    runtime->m_api.lua_pushnumber(state, value.appliedBrakeTorque);
    runtime->m_api.lua_pushnumber(state, value.serviceBrakeTorque);
    runtime->m_api.lua_pushnumber(state, value.handbrakeTorque);
    runtime->m_api.lua_pushnumber(state, value.antiLockModulation);
    runtime->m_api.lua_pushnumber(state, value.tractionControlModulation);
    runtime->m_api.lua_pushboolean(state, value.antiLockActive ? 1 : 0);
    runtime->m_api.lua_pushboolean(
        state, value.tractionControlActive ? 1 : 0);
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(value.contactCollider));
    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(value.surfaceMaterial));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(value.surfaceMaterial));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(value.surfaceWetness));
    runtime->m_api.lua_pushnumber(state, value.suspensionSpringForce);
    runtime->m_api.lua_pushnumber(state, value.suspensionDampingForce);
    runtime->m_api.lua_pushnumber(state, value.suspensionBumpStopForce);
    runtime->m_api.lua_pushnumber(state, value.suspensionDroopStopForce);
    runtime->m_api.lua_pushnumber(state, value.suspensionUnclampedForce);
    runtime->m_api.lua_pushnumber(state, value.damperDissipationWatts);
    runtime->m_api.lua_pushnumber(state, value.unsprungVelocity);
    runtime->m_api.lua_pushnumber(state, value.tireDeflection);
    runtime->m_api.lua_pushnumber(state, value.tireDeflectionVelocity);
    runtime->m_api.lua_pushnumber(state, value.tireRadialDissipationWatts);
    return 51;
}

int LuaModuleRuntime::luaVehicleGetWheelContactDiagnostic(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelState value;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelState(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 13; ++index)
            runtime->m_api.lua_pushnil(state);
        return 13;
    }

    runtime->m_api.lua_pushstring(
        state,
        heritage::vehicles::wheelContactStatusName(value.contactStatus));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.contactStatus));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(value.contactLossTransitionCount));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.rayCandidateCount));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.rayExactTestCount));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(value.staticTriangleCandidateCount));
    runtime->m_api.lua_pushboolean(
        state, value.staticSceneLoaded ? 1 : 0);
    runtime->m_api.lua_pushboolean(
        state, value.originInsideStaticSceneBounds ? 1 : 0);
    runtime->m_api.lua_pushboolean(
        state, value.rayBoundsOverlapStaticScene ? 1 : 0);
    runtime->m_api.lua_pushboolean(
        state, value.selectedHitWasStaticTriangle ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.rawSupportDistance);
    runtime->m_api.lua_pushboolean(
        state, value.suspensionBottomed ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.bottomOutPenetration);
    return 13;
}

int LuaModuleRuntime::luaVehicleGetWheelUprightPose(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelState value;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().wheelState(
            vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        for (int index = 0; index < 17; ++index)
            runtime->m_api.lua_pushnil(state);
        return 17;
    }
    runtime->m_api.lua_pushnumber(state, value.camberAngleDegrees);
    runtime->m_api.lua_pushnumber(state, value.toeAngleDegrees);
    runtime->m_api.lua_pushnumber(
        state, value.localUprightRotationDegrees.x);
    runtime->m_api.lua_pushnumber(
        state, value.localUprightRotationDegrees.y);
    runtime->m_api.lua_pushnumber(
        state, value.localUprightRotationDegrees.z);
    runtime->m_api.lua_pushnumber(state, value.worldSteeringAxis.x);
    runtime->m_api.lua_pushnumber(state, value.worldSteeringAxis.y);
    runtime->m_api.lua_pushnumber(state, value.worldSteeringAxis.z);
    runtime->m_api.lua_pushnumber(state, value.worldWheelForward.x);
    runtime->m_api.lua_pushnumber(state, value.worldWheelForward.y);
    runtime->m_api.lua_pushnumber(state, value.worldWheelForward.z);
    runtime->m_api.lua_pushnumber(state, value.worldWheelRight.x);
    runtime->m_api.lua_pushnumber(state, value.worldWheelRight.y);
    runtime->m_api.lua_pushnumber(state, value.worldWheelRight.z);
    runtime->m_api.lua_pushnumber(state, value.worldWheelUp.x);
    runtime->m_api.lua_pushnumber(state, value.worldWheelUp.y);
    runtime->m_api.lua_pushnumber(state, value.worldWheelUp.z);
    return 17;
}

int LuaModuleRuntime::luaVehicleGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime) return 0;
    const std::string error = runtime->m_physics
        ? runtime->m_physics->vehicles().lastError()
        : std::string("Vehicle system is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaModuleRuntime::luaEntityIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushboolean(state, runtime->m_entities ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityCreate(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string name = stringArgument(*runtime, state, 1);
    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->create(name)
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaEntityDestroy(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->destroy(entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->exists(entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const LuaInteger count = runtime->m_entities
        ? static_cast<LuaInteger>(runtime->m_entities->count())
        : 0;
    runtime->m_api.lua_pushinteger(state, count);
    return 1;
}

int LuaModuleRuntime::luaEntityGetPersistentId(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::uint64_t id = runtime->m_entities
        ? runtime->m_entities->persistentId(entityHandleArgument(*runtime, state, 1))
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(id));
    return 1;
}

int LuaModuleRuntime::luaEntityFindByName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->findByName(stringArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaEntitySetName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->setName(
            entityHandleArgument(*runtime, state, 1),
            stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string value = runtime->m_entities
        ? runtime->m_entities->name(entityHandleArgument(*runtime, state, 1))
        : std::string{};
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return 1;
}

int LuaModuleRuntime::luaEntityAddTag(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->addTag(
            entityHandleArgument(*runtime, state, 1),
            stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityRemoveTag(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->removeTag(
            entityHandleArgument(*runtime, state, 1),
            stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityHasTag(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->hasTag(
            entityHandleArgument(*runtime, state, 1),
            stringArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityFindFirstWithTag(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->findFirstWithTag(stringArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaEntitySetParent(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool keepWorld = booleanArgument(*runtime, state, 3, false);
    const bool result = runtime->m_entities
        && runtime->m_entities->setParent(
            entityHandleArgument(*runtime, state, 1),
            entityHandleArgument(*runtime, state, 2),
            keepWorld);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityClearParent(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool keepWorld = booleanArgument(*runtime, state, 2, true);
    const bool result = runtime->m_entities
        && runtime->m_entities->clearParent(
            entityHandleArgument(*runtime, state, 1),
            keepWorld);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetParent(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->parent(entityHandleArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaEntityGetChildCount(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::size_t count = runtime->m_entities
        ? runtime->m_entities->childCount(entityHandleArgument(*runtime, state, 1))
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaModuleRuntime::luaEntityGetChildAt(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const double requestedIndex = numberArgument(*runtime, state, 2, 1.0);
    const std::size_t zeroBasedIndex = requestedIndex >= 1.0
        ? static_cast<std::size_t>(requestedIndex - 1.0)
        : static_cast<std::size_t>(-1);
    const heritage::entities::EntityHandle handle = runtime->m_entities
        ? runtime->m_entities->childAt(
            entityHandleArgument(*runtime, state, 1),
            zeroBasedIndex)
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaModuleRuntime::luaEntityIsDescendantOf(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool result = runtime->m_entities
        && runtime->m_entities->isDescendantOf(
            entityHandleArgument(*runtime, state, 1),
            entityHandleArgument(*runtime, state, 2));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setPosition(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->position(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setRotationDegrees(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->rotationDegrees(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 1.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setScale(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->scale(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetWorldPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldPosition(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetWorldPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldPosition(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetWorldRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldRotationDegrees(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetWorldRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldRotationDegrees(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetWorldScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 value{
        static_cast<float>(numberArgument(*runtime, state, 2, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 3, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 1.0))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setWorldScale(
            entityHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetWorldScale(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::math::Vec3 value{};
    if (!runtime->m_entities
        || !runtime->m_entities->worldScale(
            entityHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaModuleRuntime::luaEntitySetDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string requestedType = stringArgument(*runtime, state, 2, "box");
    std::string normalizedType = requestedType;
    std::transform(normalizedType.begin(), normalizedType.end(), normalizedType.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    heritage::entities::DebugPrimitiveType type = heritage::entities::DebugPrimitiveType::Box;
    bool validType = true;
    if (normalizedType == "box" || normalizedType == "cube")
        type = heritage::entities::DebugPrimitiveType::Box;
    else if (normalizedType == "cylinder" || normalizedType == "wheel")
        type = heritage::entities::DebugPrimitiveType::Cylinder;
    else if (normalizedType == "sphere")
        type = heritage::entities::DebugPrimitiveType::Sphere;
    else
        validType = false;

    const heritage::math::Vec3 color{
        static_cast<float>(numberArgument(*runtime, state, 3, 0.65)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.72)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.82))
    };

    const bool result = validType
        && runtime->m_entities
        && runtime->m_entities->setDebugPrimitive(
            entityHandleArgument(*runtime, state, 1), type, color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityRemoveDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->removeDebugPrimitive(
            entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityHasDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->hasDebugPrimitive(
            entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetDebugVisible(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setDebugPrimitiveVisible(
            entityHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetDebugColor(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 color{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.65)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.72)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.82))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setDebugPrimitiveColor(
            entityHandleArgument(*runtime, state, 1), color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetDebugPrimitive(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::entities::DebugPrimitiveComponent component;
    if (!runtime->m_entities
        || !runtime->m_entities->debugPrimitive(
            entityHandleArgument(*runtime, state, 1), component))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const char* typeName = "box";
    if (component.type == heritage::entities::DebugPrimitiveType::Cylinder)
        typeName = "cylinder";
    else if (component.type == heritage::entities::DebugPrimitiveType::Sphere)
        typeName = "sphere";

    runtime->m_api.lua_pushstring(state, typeName);
    runtime->m_api.lua_pushnumber(state, component.color.x);
    runtime->m_api.lua_pushnumber(state, component.color.y);
    runtime->m_api.lua_pushnumber(state, component.color.z);
    runtime->m_api.lua_pushboolean(state, component.visible ? 1 : 0);
    return 5;
}

int LuaModuleRuntime::luaEntitySetMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const heritage::math::Vec3 color{
        static_cast<float>(numberArgument(*runtime, state, 3, 0.72)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.78)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.88))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMesh(
            entityHandleArgument(*runtime, state, 1),
            stringArgument(*runtime, state, 2),
            color,
            booleanArgument(*runtime, state, 6, false),
            booleanArgument(*runtime, state, 7, false),
            booleanArgument(*runtime, state, 8, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityRemoveMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->removeMesh(entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityHasMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->hasMesh(entityHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetMeshVisible(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshVisible(
            entityHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetMeshColor(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const heritage::math::Vec3 color{
        static_cast<float>(numberArgument(*runtime, state, 2, 0.72)),
        static_cast<float>(numberArgument(*runtime, state, 3, 0.78)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.88))
    };
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshColor(
            entityHandleArgument(*runtime, state, 1), color);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetMeshNormalize(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshNormalize(
            entityHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntitySetMeshDoubleSided(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;
    const bool result = runtime->m_entities
        && runtime->m_entities->setMeshDoubleSided(
            entityHandleArgument(*runtime, state, 1),
            booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaEntityGetMesh(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::entities::MeshComponent component;
    if (!runtime->m_entities
        || !runtime->m_entities->mesh(
            entityHandleArgument(*runtime, state, 1), component))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushlstring(
        state, component.assetPath.c_str(), component.assetPath.size());
    runtime->m_api.lua_pushnumber(state, component.color.x);
    runtime->m_api.lua_pushnumber(state, component.color.y);
    runtime->m_api.lua_pushnumber(state, component.color.z);
    runtime->m_api.lua_pushboolean(state, component.visible ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.normalize ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.doubleSided ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, component.blenderCoordinates ? 1 : 0);
    return 8;
}

int LuaModuleRuntime::luaEntityGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string error = runtime->m_entities
        ? runtime->m_entities->lastError()
        : std::string("EntityRegistry is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

int LuaModuleRuntime::luaPrefabIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool available = runtime->m_entities && runtime->m_context.has_value();
    runtime->m_api.lua_pushboolean(state, available ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPrefabExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::filesystem::path path = runtime->m_context
        ? runtime->m_context->resolvePrefabPath(relativePath)
        : std::filesystem::path{};

    const bool exists = !path.empty()
        && path.extension() == ".hprefab"
        && std::filesystem::is_regular_file(path);
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaPrefabInstantiate(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPrefabError.clear();
    if (!runtime->m_entities || !runtime->m_context)
    {
        runtime->m_lastPrefabError = "Prefab service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    const std::string relativePath = stringArgument(*runtime, state, 1);
    const std::filesystem::path path =
        runtime->m_context->resolvePrefabPath(relativePath);
    if (path.empty() || path.extension() != ".hprefab")
    {
        runtime->m_lastPrefabError =
            "Prefab.Instantiate requires a safe module-Prefabs-relative .hprefab path.";
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    heritage::entities::PrefabInstantiationOptions options;
    options.rootName = stringArgument(*runtime, state, 2);
    options.position = {
        static_cast<float>(numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 4, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 5, 0.0))
    };
    options.rotationDegrees = {
        static_cast<float>(numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 7, 0.0)),
        static_cast<float>(numberArgument(*runtime, state, 8, 0.0))
    };
    options.scale = {
        static_cast<float>(numberArgument(*runtime, state, 9, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 10, 1.0)),
        static_cast<float>(numberArgument(*runtime, state, 11, 1.0))
    };
    options.namePrefix = stringArgument(*runtime, state, 12);

    heritage::entities::PrefabInstantiationResult result;
    if (!heritage::entities::EntityPrefabDocument::instantiate(
            path,
            *runtime->m_entities,
            options,
            result,
            runtime->m_lastPrefabError))
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        return 2;
    }

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(result.root));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(result.entities.size()));
    return 2;
}

int LuaModuleRuntime::luaPrefabGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastPrefabError.c_str(),
        runtime->m_lastPrefabError.size());
    return 1;
}

int LuaModuleRuntime::luaModuleId(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().id : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaModuleRuntime::luaModuleName(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().name : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaModuleRuntime::luaModuleVersion(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().version : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaModuleRuntime::luaModuleAssetPath(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveAssetPath(
        stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaModuleRuntime::luaModuleAssetExists(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::filesystem::path path = runtime->m_context->resolveAssetPath(
        stringArgument(*runtime, state, 1));
    std::error_code error;
    const bool exists = !path.empty()
        && std::filesystem::is_regular_file(path, error)
        && !error;
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaModuleRuntime::luaModuleSelectAssetFile(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

#if defined(_WIN32)
    std::vector<wchar_t> selected(32768, L'\0');
    const std::wstring initialDirectory =
        runtime->m_context->assetRoot().wstring();
    const wchar_t filter[] =
        L"Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.lpstrTitle = L"Select a module-owned vehicle OBJ";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
        | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;

    if (!GetOpenFileNameW(&dialog))
    {
        runtime->m_api.lua_pushnil(state);
        const std::string message = CommDlgExtendedError() == 0
            ? "Asset selection cancelled."
            : "The Windows asset picker could not complete.";
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        runtime->m_context->assetRoot(), error);
    if (error)
    {
        const std::string message = "The module Assets directory is unavailable.";
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }
    const std::filesystem::path absolute = std::filesystem::weakly_canonical(
        std::filesystem::path(selected.data()), error);
    const std::filesystem::path relative = error
        ? std::filesystem::path{}
        : absolute.lexically_relative(root);
    bool safe = !relative.empty() && !relative.is_absolute();
    for (const std::filesystem::path& component : relative)
    {
        if (component == "..")
            safe = false;
    }
    std::string extension = absolute.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    safe = safe && extension == ".obj"
        && std::filesystem::is_regular_file(absolute, error) && !error;
    if (!safe)
    {
        const std::string message =
            "Vehicle assets must be .obj files already inside this module's Assets folder.";
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::string value = relative.generic_string();
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    runtime->m_api.lua_pushnil(state);
    return 2;
#else
    const std::string message =
        "The native asset picker is currently implemented for Windows only.";
    runtime->m_api.lua_pushnil(state);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
#endif
}

int LuaModuleRuntime::luaModuleDataPath(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveDataPath(
        stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaModuleRuntime::luaModuleSavePath(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveSavePath(
        stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaModuleRuntime::luaModuleWriteSaveText(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativeText = stringArgument(*runtime, state, 1);
    const std::string contents = stringArgument(*runtime, state, 2);
    const std::filesystem::path relative(relativeText);
    std::string extension = relative.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    const bool supportedExtension = extension == ".lua"
        || extension == ".json" || extension == ".txt";
    const std::filesystem::path path = runtime->m_context->resolveSavePath(relative);
    if (path.empty() || !supportedExtension || contents.size() > 2u * 1024u * 1024u)
    {
        const std::string message =
            "Module.WriteSaveText requires a safe .lua, .json or .txt save-relative path and at most 2 MiB of text.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        const std::string message =
            "Could not create the module save directory: " + error.message();
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output)
    {
        const std::string message = "Could not write the module save text file.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::string result = path.string();
    runtime->m_api.lua_pushboolean(state, 1);
    runtime->m_api.lua_pushlstring(state, result.c_str(), result.size());
    return 2;
}

} // namespace heritage::modules
