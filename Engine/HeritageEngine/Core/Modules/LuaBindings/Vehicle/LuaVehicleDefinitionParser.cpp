#include "LuaVehicleDefinitionParser.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules::lua_binding_detail {
namespace {


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

bool luaRequiredFieldVector3(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    heritage::math::Vec3& value)
{
    if (!pushLuaTableField(api, state, tableIndex, field))
        return false;

    const int vectorIndex = api.lua_gettop(state);
    heritage::math::Vec3 parsed{};
    float* components[] = { &parsed.x, &parsed.y, &parsed.z };
    bool valid = api.lua_rawlen(state, vectorIndex) >= 3;
    for (LuaInteger component = 1; component <= 3; ++component)
    {
        api.lua_rawgeti(state, vectorIndex, component);
        int converted = 0;
        const LuaNumber candidate = api.lua_tonumberx(state, -1, &converted);
        if (converted)
        {
            *components[static_cast<std::size_t>(component - 1)] =
                static_cast<float>(candidate);
        }
        else
        {
            valid = false;
        }
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    if (valid)
        value = parsed;
    return valid;
}

bool luaOptionalFieldVector3(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    bool& present,
    heritage::math::Vec3& value)
{
    const int absoluteTable = absoluteLuaIndex(api, state, tableIndex);
    api.lua_getfield(state, absoluteTable, field);
    const int type = api.lua_type(state, -1);
    if (type == kLuaTypeNil)
    {
        present = false;
        popLuaValues(api, state);
        return true;
    }
    if (type != kLuaTypeTable)
    {
        present = true;
        popLuaValues(api, state);
        return false;
    }

    present = true;
    const int vectorIndex = api.lua_gettop(state);
    heritage::math::Vec3 parsed{};
    float* components[] = { &parsed.x, &parsed.y, &parsed.z };
    bool valid = api.lua_rawlen(state, vectorIndex) >= 3;
    for (LuaInteger component = 1; component <= 3; ++component)
    {
        api.lua_rawgeti(state, vectorIndex, component);
        int converted = 0;
        const LuaNumber candidate = api.lua_tonumberx(state, -1, &converted);
        if (converted)
            *components[static_cast<std::size_t>(component - 1)] =
                static_cast<float>(candidate);
        else
            valid = false;
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    if (valid)
        value = parsed;
    return valid;
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

} // namespace

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
            if (!luaOptionalFieldVector3(
                    api, state, -1, "centerOfMassLocal",
                    body.hasCenterOfMassLocal, body.centerOfMassLocal))
            {
                popLuaValues(api, state, 2);
                errorMessage = "Body centerOfMassLocal must be a three-number array when present.";
                return false;
            }
            if (!luaOptionalFieldVector3(
                    api, state, -1, "inertiaLocalKgM2",
                    body.hasInertiaLocalKgM2, body.inertiaLocalKgM2))
            {
                popLuaValues(api, state, 2);
                errorMessage = "Body inertiaLocalKgM2 must be a three-number array when present.";
                return false;
            }
            body.frontStaticLoadFraction = static_cast<float>(luaFieldNumber(
                api, state, -1, "frontStaticLoadFraction", 0.50));
            body.leftStaticLoadFraction = static_cast<float>(luaFieldNumber(
                api, state, -1, "leftStaticLoadFraction", 0.50));
            body.massPropertiesProvenance = luaFieldString(
                api, state, -1, "massPropertiesProvenance");
            body.massPropertiesConfidence = static_cast<float>(luaFieldNumber(
                api, state, -1, "massPropertiesConfidence", 0.0));
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

            // SUS01: preserve optional creator-authored suspension linkage
            // anchors even while linear_raycast_v1 remains the runtime solver.
            // Hardpoint providers can later consume the same V2 definition
            // without replacing the authoring contract.
            if (pushLuaTableField(api, state, -1, "hardpoints"))
            {
                const int hardpointsIndex = api.lua_gettop(state);
                std::size_t hardpointCount = 0;
                if (!luaArrayLength(
                        api, state, hardpointsIndex, 32,
                        "Suspension hardpoint collection",
                        hardpointCount, errorMessage))
                {
                    popLuaValues(api, state, 3);
                    return false;
                }
                suspension.hardpoints.reserve(hardpointCount);
                for (std::size_t hardpointIndex = 0;
                     hardpointIndex < hardpointCount; ++hardpointIndex)
                {
                    api.lua_rawgeti(
                        state, hardpointsIndex,
                        static_cast<LuaInteger>(hardpointIndex + 1));
                    if (api.lua_type(state, -1) != kLuaTypeTable)
                    {
                        popLuaValues(api, state, 4);
                        errorMessage =
                            "Every suspension hardpoint entry must be a table.";
                        return false;
                    }
                    heritage::vehicles::VehicleSuspensionHardpointDefinition
                        hardpoint;
                    hardpoint.id = luaFieldString(
                        api, state, -1, "id");
                    hardpoint.provenance = luaFieldString(
                        api, state, -1, "provenance");
                    hardpoint.confidence = static_cast<float>(luaFieldNumber(
                        api, state, -1, "confidence",
                        hardpoint.provenance.empty() ? 0.0 : 1.0));
                    if (!luaRequiredFieldVector3(
                            api, state, -1, "position",
                            hardpoint.localPosition))
                    {
                        popLuaValues(api, state, 4);
                        errorMessage =
                            "Suspension hardpoint position must contain three numbers.";
                        return false;
                    }
                    suspension.hardpoints.push_back(std::move(hardpoint));
                    popLuaValues(api, state);
                }
                popLuaValues(api, state);
            }

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
            contact.tireParameterFile = luaFieldString(
                api, state, -1, "tireParameterFile");
            contact.tireParameterProvenance = luaFieldString(
                api, state, -1, "tireParameterProvenance");
            contact.tireParameterConfidence = static_cast<float>(luaFieldNumber(
                api, state, -1, "tireParameterConfidence", 0.0));
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

    if (pushLuaTableField(api, state, rootIndex, "antiRollBars"))
    {
        const int collectionIndex = api.lua_gettop(state);
        std::size_t count = 0;
        if (!luaArrayLength(
                api, state, collectionIndex, 32, "Anti-roll-bar collection",
                count, errorMessage))
        {
            popLuaValues(api, state);
            return false;
        }
        source.antiRollBars.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            api.lua_rawgeti(
                state, collectionIndex, static_cast<LuaInteger>(index + 1));
            if (api.lua_type(state, -1) != kLuaTypeTable)
            {
                popLuaValues(api, state, 2);
                errorMessage = "Every anti-roll-bar entry must be a table.";
                return false;
            }
            heritage::vehicles::VehicleAntiRollBarDefinition bar;
            bar.id = luaFieldString(api, state, -1, "id");
            bar.leftContactUnit = luaFieldString(
                api, state, -1, "leftContactUnit");
            bar.rightContactUnit = luaFieldString(
                api, state, -1, "rightContactUnit");
            bar.enabled = luaFieldBoolean(api, state, -1, "enabled", true);
            bar.torsionalStiffnessNmPerRad = static_cast<float>(luaFieldNumber(
                api, state, -1, "torsionalStiffnessNmPerRad", 0.0));
            bar.torsionalDampingNmsPerRad = static_cast<float>(luaFieldNumber(
                api, state, -1, "torsionalDampingNmsPerRad", 0.0));
            bar.leftLeverArmM = static_cast<float>(luaFieldNumber(
                api, state, -1, "leftLeverArmM", 0.20));
            bar.rightLeverArmM = static_cast<float>(luaFieldNumber(
                api, state, -1, "rightLeverArmM", 0.20));
            bar.leftLinkMotionRatio = static_cast<float>(luaFieldNumber(
                api, state, -1, "leftLinkMotionRatio", 1.0));
            bar.rightLinkMotionRatio = static_cast<float>(luaFieldNumber(
                api, state, -1, "rightLinkMotionRatio", 1.0));
            bar.maximumWheelForceN = static_cast<float>(luaFieldNumber(
                api, state, -1, "maximumWheelForceN", 12000.0));
            bar.provenance = luaFieldString(api, state, -1, "provenance");
            bar.confidence = static_cast<float>(luaFieldNumber(
                api, state, -1, "confidence",
                bar.provenance.empty() ? 0.0 : 1.0));
            source.antiRollBars.push_back(std::move(bar));
            popLuaValues(api, state);
        }
        popLuaValues(api, state);
    }

    if (pushLuaTableField(api, state, rootIndex, "chassisFlex"))
    {
        const int flexIndex = api.lua_gettop(state);
        source.chassisFlex.enabled = luaFieldBoolean(
            api, state, flexIndex, "enabled", false);
        source.chassisFlex.provider = luaFieldString(
            api, state, flexIndex, "provider", "chassis_torsional_mode_v1");
        source.chassisFlex.mountBody = luaFieldString(
            api, state, flexIndex, "mountBody", "chassis");
        source.chassisFlex.torsionalRigidityNmPerDegree = static_cast<float>(
            luaFieldNumber(
                api, state, flexIndex, "torsionalRigidityNmPerDegree", 10000.0));
        source.chassisFlex.torsionalDampingNmsPerRad = static_cast<float>(
            luaFieldNumber(
                api, state, flexIndex, "torsionalDampingNmsPerRad", 12000.0));
        source.chassisFlex.effectiveTorsionalInertiaKgM2 = static_cast<float>(
            luaFieldNumber(
                api, state, flexIndex, "effectiveTorsionalInertiaKgM2", 500.0));
        source.chassisFlex.torsionAxisLocalY = static_cast<float>(luaFieldNumber(
            api, state, flexIndex, "torsionAxisLocalY", 0.45));
        source.chassisFlex.frontReferenceLocalZ = static_cast<float>(
            luaFieldNumber(
                api, state, flexIndex, "frontReferenceLocalZ", 1.20));
        source.chassisFlex.rearReferenceLocalZ = static_cast<float>(
            luaFieldNumber(
                api, state, flexIndex, "rearReferenceLocalZ", -1.20));
        source.chassisFlex.maximumTwistDegrees = static_cast<float>(
            luaFieldNumber(api, state, flexIndex, "maximumTwistDegrees", 1.0));
        source.chassisFlex.provenance = luaFieldString(
            api, state, flexIndex, "provenance");
        source.chassisFlex.confidence = static_cast<float>(luaFieldNumber(
            api, state, flexIndex, "confidence",
            source.chassisFlex.provenance.empty() ? 0.0 : 1.0));
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

} // namespace heritage::modules::lua_binding_detail
