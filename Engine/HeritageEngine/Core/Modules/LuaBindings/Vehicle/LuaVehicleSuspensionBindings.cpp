#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "../../../../Vehicles/Suspension/Authoring/MacPhersonHardpointEstimator.hpp"
#include "../../../../Vehicles/Suspension/Authoring/TrailingArmHardpointEstimator.hpp"
#include "../../../../Vehicles/Dynamics/ChassisFlex/ChassisFlexEstimator.hpp"

#include <algorithm>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cmath>
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

namespace heritage::modules {
using namespace lua_binding_detail;

namespace {

void popLuaValues(const LuaApi& api, lua_State* state, int count = 1)
{
    if (count > 0)
        api.lua_settop(state, -count - 1);
}

bool readNamedVector3(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    const char* field,
    heritage::math::Vec3& value)
{
    const int absoluteTable = tableIndex > 0
        ? tableIndex
        : api.lua_gettop(state) + tableIndex + 1;
    api.lua_getfield(state, absoluteTable, field);
    if (api.lua_type(state, -1) != kLuaTypeTable)
    {
        popLuaValues(api, state);
        return false;
    }
    const int pointIndex = api.lua_gettop(state);
    float* components[] = { &value.x, &value.y, &value.z };
    const char* names[] = { "x", "y", "z" };
    bool valid = true;
    for (std::size_t component = 0; component < 3; ++component)
    {
        api.lua_getfield(state, pointIndex, names[component]);
        int converted = 0;
        const LuaNumber number = api.lua_tonumberx(state, -1, &converted);
        if (!converted)
            valid = false;
        else
            *components[component] = static_cast<float>(number);
        popLuaValues(api, state);
    }
    popLuaValues(api, state);
    return valid;
}

void pushNamedVector3(
    const LuaApi& api,
    lua_State* state,
    const heritage::math::Vec3& value)
{
    api.lua_createtable(state, 0, 3);
    api.lua_pushnumber(state, value.x);
    api.lua_setfield(state, -2, "x");
    api.lua_pushnumber(state, value.y);
    api.lua_setfield(state, -2, "y");
    api.lua_pushnumber(state, value.z);
    api.lua_setfield(state, -2, "z");
}

void pushMacPhersonHardpoints(
    const LuaApi& api,
    lua_State* state,
    const heritage::vehicles::MacPhersonHardpoints& value)
{
    api.lua_createtable(state, 0, 8);
    const auto setPoint = [&](const char* id, const heritage::math::Vec3& point) {
        pushNamedVector3(api, state, point);
        api.lua_setfield(state, -2, id);
    };
    setPoint("strut_top_mount", value.strutTopMount);
    setPoint("strut_upright_mount", value.strutUprightMount);
    setPoint("lower_arm_inner_front", value.lowerArmInnerFront);
    setPoint("lower_arm_inner_rear", value.lowerArmInnerRear);
    setPoint("lower_ball_joint", value.lowerBallJoint);
    setPoint("tie_rod_inner", value.tieRodInner);
    setPoint("tie_rod_outer", value.tieRodOuter);
    setPoint("wheel_center", value.wheelCenter);
}

bool readMacPhersonHardpoints(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    heritage::vehicles::MacPhersonHardpoints& value)
{
    value = {};
    const bool complete =
        readNamedVector3(api, state, tableIndex, "strut_top_mount", value.strutTopMount)
        && readNamedVector3(api, state, tableIndex, "strut_upright_mount", value.strutUprightMount)
        && readNamedVector3(api, state, tableIndex, "lower_arm_inner_front", value.lowerArmInnerFront)
        && readNamedVector3(api, state, tableIndex, "lower_arm_inner_rear", value.lowerArmInnerRear)
        && readNamedVector3(api, state, tableIndex, "lower_ball_joint", value.lowerBallJoint)
        && readNamedVector3(api, state, tableIndex, "tie_rod_inner", value.tieRodInner)
        && readNamedVector3(api, state, tableIndex, "tie_rod_outer", value.tieRodOuter)
        && readNamedVector3(api, state, tableIndex, "wheel_center", value.wheelCenter);
    value.authored = complete;
    return complete && heritage::vehicles::validMacPhersonHardpoints(value);
}

void pushTrailingArmHardpoints(
    const LuaApi& api,
    lua_State* state,
    const heritage::vehicles::TrailingArmHardpoints& value)
{
    api.lua_createtable(state, 0, 5);
    const auto setPoint = [&](const char* id, const heritage::math::Vec3& point) {
        pushNamedVector3(api, state, point);
        api.lua_setfield(state, -2, id);
    };
    setPoint("arm_pivot_inner", value.armPivotInner);
    setPoint("arm_pivot_outer", value.armPivotOuter);
    setPoint("wheel_center", value.wheelCenter);
    setPoint("damper_upper_mount", value.damperUpperMount);
    setPoint("damper_lower_mount", value.damperLowerMount);
}

bool readTrailingArmHardpoints(
    const LuaApi& api,
    lua_State* state,
    int tableIndex,
    heritage::vehicles::TrailingArmHardpoints& value)
{
    value = {};
    const bool complete =
        readNamedVector3(api, state, tableIndex, "arm_pivot_inner", value.armPivotInner)
        && readNamedVector3(api, state, tableIndex, "arm_pivot_outer", value.armPivotOuter)
        && readNamedVector3(api, state, tableIndex, "wheel_center", value.wheelCenter)
        && readNamedVector3(api, state, tableIndex, "damper_upper_mount", value.damperUpperMount)
        && readNamedVector3(api, state, tableIndex, "damper_lower_mount", value.damperLowerMount);
    value.authored = complete;
    return complete && heritage::vehicles::validTrailingArmHardpoints(value);
}

} // namespace

int LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionModelDescription value;
    value.springPreloadN = LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0);
    value.springRateNPerM = LuaModuleRuntime::numberArgument(*runtime, state, 4, 35000.0);
    value.springProgressionNPerM2 = LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0);
    value.bumpDampingNsPerM = LuaModuleRuntime::numberArgument(*runtime, state, 6, 3200.0);
    value.bumpHighSpeedDampingNsPerM = LuaModuleRuntime::numberArgument(*runtime, state, 7, value.bumpDampingNsPerM);
    value.bumpDampingKneeVelocityMps = LuaModuleRuntime::numberArgument(*runtime, state, 8, 1.0);
    value.reboundDampingNsPerM = LuaModuleRuntime::numberArgument(*runtime, state, 9, 4200.0);
    value.reboundHighSpeedDampingNsPerM = LuaModuleRuntime::numberArgument(*runtime, state, 10, value.reboundDampingNsPerM);
    value.reboundDampingKneeVelocityMps = LuaModuleRuntime::numberArgument(*runtime, state, 11, 1.0);
    value.bumpStopEngagementM = LuaModuleRuntime::numberArgument(*runtime, state, 12, 0.18);
    value.bumpStopRateNPerM = LuaModuleRuntime::numberArgument(*runtime, state, 13, 0.0);
    value.bumpStopProgressionNPerM2 = LuaModuleRuntime::numberArgument(*runtime, state, 14, 0.0);
    value.droopStopEngagementM = LuaModuleRuntime::numberArgument(*runtime, state, 15, 0.15);
    value.droopStopRateNPerM = LuaModuleRuntime::numberArgument(*runtime, state, 16, 0.0);
    value.motionRatio = LuaModuleRuntime::numberArgument(*runtime, state, 17, 1.0);
    value.maximumForceN = LuaModuleRuntime::numberArgument(*runtime, state, 18, 250000.0);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelSuspensionModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelSuspensionModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
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
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
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

int LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::SuspensionGeometryDescription value;
    value.localSteeringAxis = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)) };
    value.staticCamberDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0));
    value.camberGainDegreesPerM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0));
    value.camberProgressionDegreesPerM2 = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.0));
    value.staticToeDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.0));
    value.toeGainDegreesPerM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 10, 0.0));
    value.toeProgressionDegreesPerM2 = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 11, 0.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelSuspensionGeometry(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelSuspensionGeometry(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
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
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
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

int LuaVehicleBindingHandlers::luaVehicleSetAntiRollBar(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    int barConverted = 0;
    int leftConverted = 0;
    int rightConverted = 0;
    const LuaInteger luaBarIndex = runtime->m_api.lua_tointegerx(
        state, 2, &barConverted);
    const LuaInteger luaLeftWheel = runtime->m_api.lua_tointegerx(
        state, 3, &leftConverted);
    const LuaInteger luaRightWheel = runtime->m_api.lua_tointegerx(
        state, 4, &rightConverted);
    const std::size_t barIndex = barConverted && luaBarIndex >= 1
        ? static_cast<std::size_t>(luaBarIndex - 1)
        : static_cast<std::size_t>(-1);

    heritage::vehicles::SuspensionAntiRollBarDescription value;
    value.leftWheelIndex = leftConverted && luaLeftWheel >= 1
        ? static_cast<std::size_t>(luaLeftWheel - 1)
        : static_cast<std::size_t>(-1);
    value.rightWheelIndex = rightConverted && luaRightWheel >= 1
        ? static_cast<std::size_t>(luaRightWheel - 1)
        : static_cast<std::size_t>(-1);
    value.torsionalStiffnessNmPerRad = LuaModuleRuntime::numberArgument(
        *runtime, state, 5, 900.0);
    value.torsionalDampingNmsPerRad = LuaModuleRuntime::numberArgument(
        *runtime, state, 6, 35.0);
    value.leftLeverArmM = LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.20);
    value.rightLeverArmM = LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.20);
    value.leftLinkMotionRatio = LuaModuleRuntime::numberArgument(*runtime, state, 9, 1.0);
    value.rightLinkMotionRatio = LuaModuleRuntime::numberArgument(*runtime, state, 10, 1.0);
    value.maximumWheelForceN = LuaModuleRuntime::numberArgument(*runtime, state, 11, 12000.0);
    value.enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 12, true);

    const bool result = runtime->m_physics
        && barIndex != static_cast<std::size_t>(-1)
        && runtime->m_physics->vehicles().setAntiRollBar(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), barIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetAntiRollBar(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t barIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);

    heritage::vehicles::SuspensionAntiRollBarDescription value;
    heritage::vehicles::SuspensionAntiRollBarOutput output;
    const bool result = runtime->m_physics
        && barIndex != static_cast<std::size_t>(-1)
        && runtime->m_physics->vehicles().antiRollBar(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            barIndex, value, output);
    if (!result)
    {
        for (int index = 0; index < 16; ++index)
            runtime->m_api.lua_pushnil(state);
        return 16;
    }

    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.leftWheelIndex + 1));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(value.rightWheelIndex + 1));
    runtime->m_api.lua_pushnumber(state, value.torsionalStiffnessNmPerRad);
    runtime->m_api.lua_pushnumber(state, value.torsionalDampingNmsPerRad);
    runtime->m_api.lua_pushnumber(state, value.leftLeverArmM);
    runtime->m_api.lua_pushnumber(state, value.rightLeverArmM);
    runtime->m_api.lua_pushnumber(state, value.leftLinkMotionRatio);
    runtime->m_api.lua_pushnumber(state, value.rightLinkMotionRatio);
    runtime->m_api.lua_pushnumber(state, value.maximumWheelForceN);
    runtime->m_api.lua_pushboolean(state, value.enabled ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, output.twistRadians);
    runtime->m_api.lua_pushnumber(state, output.twistRateRadiansPerSecond);
    runtime->m_api.lua_pushnumber(state, output.totalTorqueNm);
    runtime->m_api.lua_pushnumber(state, output.leftWheelForceN);
    runtime->m_api.lua_pushnumber(state, output.rightWheelForceN);
    runtime->m_api.lua_pushnumber(state,
        std::abs(output.leftWheelForceN) + std::abs(output.rightWheelForceN));
    return 16;
}

int LuaVehicleBindingHandlers::luaVehicleGetAntiRollBarCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const std::size_t count = runtime->m_physics
        ? runtime->m_physics->vehicles().antiRollBarCount(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1))
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetChassisTorsionalCompliance(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::ChassisTorsionalComplianceDescription value;
    value.enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 2, true);
    value.torsionalRigidityNmPerDegree = LuaModuleRuntime::numberArgument(
        *runtime, state, 3, 10000.0);
    value.torsionalDampingNmsPerRad = LuaModuleRuntime::numberArgument(
        *runtime, state, 4, 12000.0);
    value.effectiveTorsionalInertiaKgM2 = LuaModuleRuntime::numberArgument(
        *runtime, state, 5, 500.0);
    value.torsionAxisLocalY = LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.45);
    value.frontReferenceLocalZ = LuaModuleRuntime::numberArgument(*runtime, state, 7, 1.20);
    value.rearReferenceLocalZ = LuaModuleRuntime::numberArgument(*runtime, state, 8, -1.20);
    value.maximumTwistDegrees = LuaModuleRuntime::numberArgument(*runtime, state, 9, 1.0);

    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setChassisTorsionalCompliance(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleEstimateChassisFlex(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::ChassisFlexEstimateInput input;
    input.massKg = LuaModuleRuntime::numberArgument(*runtime, state, 1, 1200.0);
    input.wheelbaseM = LuaModuleRuntime::numberArgument(*runtime, state, 2, 2.50);
    input.frontTrackM = LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.50);
    input.rearTrackM = LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.50);
    input.centerOfMassHeightM = LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.50);
    input.modelYear = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 2000.0));
    const std::string constructionId = LuaModuleRuntime::stringArgument(*runtime, state, 7);
    if (!heritage::vehicles::parseChassisConstructionKind(
            constructionId.empty() ? std::string("unknown") : constructionId,
            input.construction))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "Unknown chassis construction kind.");
        return 2;
    }

    const heritage::vehicles::ChassisFlexEstimate estimate =
        heritage::vehicles::estimateChassisFlex(input);
    if (!estimate.valid)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(
            state, "Could not derive a valid chassis-flex estimate.");
        return 2;
    }

    runtime->m_api.lua_createtable(state, 0, 11);
    runtime->m_api.lua_pushboolean(state, 1);
    runtime->m_api.lua_setfield(state, -2, "enabled");
    runtime->m_api.lua_pushstring(state, "chassis_torsional_mode_v1");
    runtime->m_api.lua_setfield(state, -2, "provider");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.torsionalRigidityNmPerDegree);
    runtime->m_api.lua_setfield(state, -2, "torsionalRigidityNmPerDegree");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.torsionalDampingNmsPerRad);
    runtime->m_api.lua_setfield(state, -2, "torsionalDampingNmsPerRad");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.effectiveTorsionalInertiaKgM2);
    runtime->m_api.lua_setfield(state, -2, "effectiveTorsionalInertiaKgM2");
    runtime->m_api.lua_pushnumber(state, estimate.description.torsionAxisLocalY);
    runtime->m_api.lua_setfield(state, -2, "torsionAxisLocalY");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.frontReferenceLocalZ);
    runtime->m_api.lua_setfield(state, -2, "frontReferenceLocalZ");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.rearReferenceLocalZ);
    runtime->m_api.lua_setfield(state, -2, "rearReferenceLocalZ");
    runtime->m_api.lua_pushnumber(
        state, estimate.description.maximumTwistDegrees);
    runtime->m_api.lua_setfield(state, -2, "maximumTwistDegrees");
    runtime->m_api.lua_pushlstring(
        state, estimate.provenance.c_str(), estimate.provenance.size());
    runtime->m_api.lua_setfield(state, -2, "provenance");
    runtime->m_api.lua_pushnumber(state, estimate.confidence);
    runtime->m_api.lua_setfield(state, -2, "confidence");
    runtime->m_api.lua_pushnil(state);
    return 2;
}

int LuaVehicleBindingHandlers::luaVehicleEstimateMacPhersonHardpoints(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::MacPhersonHardpointEstimateInput input;
    input.wheelCenter = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)) };
    input.referencePackageScaleM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.30));
    input.casterDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, 3.0));
    input.steeringAxisInclinationDegrees = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 6, 10.0));

    const auto estimate = heritage::vehicles::estimateMacPhersonHardpointsV1(
        input);
    if (!estimate.valid)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(
            state,
            "Could not derive a valid MacPherson package from the supplied wheel/alignment data.");
        return 2;
    }

    runtime->m_api.lua_createtable(state, 0, 3);
    runtime->m_api.lua_pushlstring(
        state, estimate.profileId.c_str(), estimate.profileId.size());
    runtime->m_api.lua_setfield(state, -2, "profile_id");
    runtime->m_api.lua_pushnumber(state, estimate.confidence);
    runtime->m_api.lua_setfield(state, -2, "confidence");
    pushMacPhersonHardpoints(runtime->m_api, state, estimate.hardpoints);
    runtime->m_api.lua_setfield(state, -2, "hardpoints");
    runtime->m_api.lua_pushnil(state);
    return 2;
}

int LuaVehicleBindingHandlers::luaVehicleEstimateTrailingArmHardpoints(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    heritage::vehicles::TrailingArmHardpointEstimateInput input;
    input.wheelCenter = {
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)) };
    input.referencePackageScaleM = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.30));

    const auto estimate = heritage::vehicles::estimateTrailingArmHardpointsV1(
        input);
    if (!estimate.valid)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(
            state,
            "Could not derive a valid trailing-arm package from the supplied chassis reference data.");
        return 2;
    }

    runtime->m_api.lua_createtable(state, 0, 3);
    runtime->m_api.lua_pushlstring(
        state, estimate.profileId.c_str(), estimate.profileId.size());
    runtime->m_api.lua_setfield(state, -2, "profile_id");
    runtime->m_api.lua_pushnumber(state, estimate.confidence);
    runtime->m_api.lua_setfield(state, -2, "confidence");
    pushTrailingArmHardpoints(runtime->m_api, state, estimate.hardpoints);
    runtime->m_api.lua_setfield(state, -2, "hardpoints");
    runtime->m_api.lua_pushnil(state);
    return 2;
}

int LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionHardpoints(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_physics)
        return 0;

    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const std::string providerId = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    heritage::vehicles::SuspensionProviderKind provider;
    if (!heritage::vehicles::parseSuspensionProvider(providerId, provider))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    heritage::vehicles::SuspensionGeometryDescription geometry;
    if (!runtime->m_physics->vehicles().wheelSuspensionGeometry(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, geometry))
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    if (provider == heritage::vehicles::SuspensionProviderKind::MacPhersonStrutV1)
    {
        if (runtime->m_api.lua_type(state, 4) != kLuaTypeTable
            || !readMacPhersonHardpoints(
                runtime->m_api, state, 4, geometry.macPherson))
        {
            runtime->m_api.lua_pushboolean(state, 0);
            return 1;
        }
        geometry.trailingArm = {};
    }
    else if (provider
        == heritage::vehicles::SuspensionProviderKind::TrailingArmTorsionBarV1)
    {
        if (runtime->m_api.lua_type(state, 4) != kLuaTypeTable
            || !readTrailingArmHardpoints(
                runtime->m_api, state, 4, geometry.trailingArm))
        {
            runtime->m_api.lua_pushboolean(state, 0);
            return 1;
        }
        geometry.macPherson = {};
    }
    else if (provider != heritage::vehicles::SuspensionProviderKind::LinearRaycastV1)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }
    else
    {
        geometry.macPherson = {};
        geometry.trailingArm = {};
    }

    geometry.provider = provider;
    const bool result = runtime->m_physics->vehicles().setWheelSuspensionGeometry(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, geometry);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleSetWheelUnsprungMassModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(
        state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    heritage::vehicles::UnsprungMassDescription value;
    value.effectiveMassKg = LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0);
    value.tireRadialStiffnessNPerM = LuaModuleRuntime::numberArgument(*runtime, state, 4, 220000.0);
    value.tireRadialDampingNsPerM = LuaModuleRuntime::numberArgument(*runtime, state, 5, 1800.0);
    value.maximumTireDeflectionM = LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.08);
    value.maximumNormalForceN = LuaModuleRuntime::numberArgument(*runtime, state, 7, 250000.0);
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().setWheelUnsprungMassModel(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelUnsprungMassModel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
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
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
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

} // namespace heritage::modules
