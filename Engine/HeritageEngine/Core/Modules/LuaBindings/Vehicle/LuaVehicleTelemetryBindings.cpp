#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "../../../../Vehicles/Dynamics/ChassisFlex/ChassisFlexDiagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include "../../../../Physics/PhysicsWorld.hpp"
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

int LuaVehicleBindingHandlers::luaVehicleGetSpeed(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushnumber(state, runtime->m_physics
        ? runtime->m_physics->vehicles().speed(LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1)) : 0.0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetGroundedWheelCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().groundedWheelCount(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetLastHighRateStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? runtime->m_physics->vehicles().lastHighRateStepCount(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1)) : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetTotalHighRateStepCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    runtime->m_api.lua_pushinteger(state, runtime->m_physics
        ? static_cast<LuaInteger>(runtime->m_physics->vehicles().totalHighRateStepCount(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1))) : 0);
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    heritage::vehicles::WheelState value;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().wheelState(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);

    // GetWheelState currently returns 169 Lua values. Lua C functions only
    // have a small guaranteed amount of free stack space on entry; blindly
    // pushing this extended telemetry payload can overrun the Lua stack and
    // hard-crash the process. Reserve the full result capacity before either
    // the success or nil-filled failure path writes anything.
    if (!runtime->m_api.lua_checkstack
        || !runtime->m_api.lua_checkstack(state, 169))
    {
        return 0;
    }

    if (!result)
    {
        for (int index = 0; index < 169; ++index) runtime->m_api.lua_pushnil(state);
        return 169;
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
    runtime->m_api.lua_pushnumber(state, value.turnSlipPerM);
    runtime->m_api.lua_pushnumber(state, value.normalizedTurnSlip);
    runtime->m_api.lua_pushnumber(state, value.contactPatchTwistDegrees);
    runtime->m_api.lua_pushnumber(state, value.parkingTurnMoment);
    runtime->m_api.lua_pushnumber(state, value.turnSlipMoment);
    runtime->m_api.lua_pushnumber(state, value.turnSlipLongitudinalReduction);
    runtime->m_api.lua_pushnumber(state, value.turnSlipLateralReduction);
    runtime->m_api.lua_pushnumber(state, value.turnSlipCorneringReduction);
    runtime->m_api.lua_pushnumber(state, value.turnSlipTrailReduction);
    runtime->m_api.lua_pushnumber(state, value.tireFreeRollingRadius);
    runtime->m_api.lua_pushnumber(state, value.tireLoadedRadius);
    runtime->m_api.lua_pushnumber(state, value.tireEffectiveRollingRadius);
    runtime->m_api.lua_pushnumber(state, value.tireContactPatchLength);
    runtime->m_api.lua_pushnumber(state, value.tireContactPatchWidth);
    runtime->m_api.lua_pushnumber(state, value.tireContactPatchArea);
    runtime->m_api.lua_pushnumber(state, value.tireEnvelopeRoadOffset);
    runtime->m_api.lua_pushnumber(state, value.tireEnvelopeSlopeDegrees);
    runtime->m_api.lua_pushnumber(state, value.tireEnvelopeCrossSlopeDegrees);
    runtime->m_api.lua_pushnumber(state, value.tireEnvelopeValidSamples);
    runtime->m_api.lua_pushnumber(state, value.tireFootprintTotalSamples);
    runtime->m_api.lua_pushnumber(state, value.tireFootprintSupportedFraction);
    runtime->m_api.lua_pushnumber(state, value.tireFootprintRoughnessRange);
    runtime->m_api.lua_pushnumber(state, value.tireFootprintSurfaceFriction);
    runtime->m_api.lua_pushnumber(state, value.tireFootprintSurfaceSpread);
    runtime->m_api.lua_pushboolean(state, value.tireFootprintRefined ? 1 : 0);
    runtime->m_api.lua_pushnumber(state, value.tireRingRadialOffset);
    runtime->m_api.lua_pushnumber(state, value.tireRingRadialVelocity);
    runtime->m_api.lua_pushnumber(state, value.tireRingLongitudinalOffset);
    runtime->m_api.lua_pushnumber(state, value.tireRingLongitudinalVelocity);
    runtime->m_api.lua_pushnumber(state, value.tireRingLateralOffset);
    runtime->m_api.lua_pushnumber(state, value.tireRingLateralVelocity);
    runtime->m_api.lua_pushnumber(state, value.tireRingYawDegrees);
    runtime->m_api.lua_pushnumber(state, value.tireRingYawRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(state, value.tireRingWindupDegrees);
    runtime->m_api.lua_pushnumber(state, value.tireRingWindupRateDegreesPerSecond);
    runtime->m_api.lua_pushnumber(state, value.tireTreadTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireCarcassTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireGasTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireInflationPressurePa);
    runtime->m_api.lua_pushnumber(state, value.tireThermalFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireThermalStiffnessScale);
    runtime->m_api.lua_pushnumber(state, value.tireSlipDissipationWatts);
    runtime->m_api.lua_pushnumber(state, value.tireThermalLossDissipationWatts);
    runtime->m_api.lua_pushnumber(state, value.tireRoadHeatFlowWatts);
    runtime->m_api.lua_pushnumber(state, value.tireAirHeatFlowWatts);
    runtime->m_api.lua_pushnumber(state, value.tireTreadInsideSurfaceTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireTreadCenterSurfaceTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireTreadOutsideSurfaceTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireTreadHottestSurfaceTemperatureC);
    runtime->m_api.lua_pushnumber(state, value.tireTreadInsideDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTreadCenterDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTreadOutsideDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTreadMinimumDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTreadWearFraction);
    runtime->m_api.lua_pushnumber(state, value.tireFlatSpotDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireFlatSpotSector);
    runtime->m_api.lua_pushnumber(state, value.tireSpatialFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireTreadContactSector);
    runtime->m_api.lua_pushnumber(state, value.tireTreadHottestSector);
    // TIRE10 appends physical tread-radius coupling and the native road normal
    // without disturbing the historical 1..110 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireAverageTreadRadiusLossMm);
    runtime->m_api.lua_pushnumber(state, value.tireContactTreadRadiusLossMm);
    runtime->m_api.lua_pushnumber(state, value.tireContactRadiusVariationMm);
    runtime->m_api.lua_pushnumber(state, value.contactNormal.x);
    runtime->m_api.lua_pushnumber(state, value.contactNormal.y);
    runtime->m_api.lua_pushnumber(state, value.contactNormal.z);
    // TIRE11 appends contamination telemetry without disturbing the historical
    // 1..116 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireContaminationFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireContaminationTotal);
    runtime->m_api.lua_pushnumber(state, value.tireContaminationAverage);
    runtime->m_api.lua_pushnumber(state, value.tireOrganicContamination);
    runtime->m_api.lua_pushnumber(state, value.tireMineralContamination);
    runtime->m_api.lua_pushnumber(state, value.tireGravelFinesContamination);
    runtime->m_api.lua_pushnumber(state, value.tireRubberPickupContamination);
    runtime->m_api.lua_pushnumber(state, value.tireMudFilmContamination);
    runtime->m_api.lua_pushnumber(state, value.tireContaminationCleaningRate);
    // TIRE12 appends clean-room wet-surface/hydroplaning telemetry without
    // disturbing the historical 1..125 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireRoadWaterDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireRetainedWaterDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireDrainageDemandRatio);
    runtime->m_api.lua_pushnumber(state, value.tireWaterWedgeFraction);
    runtime->m_api.lua_pushnumber(state, value.tireHydroplaningFraction);
    runtime->m_api.lua_pushnumber(state, value.tirePavementContactFraction);
    runtime->m_api.lua_pushnumber(state, value.tireHydrodynamicLiftN);
    runtime->m_api.lua_pushnumber(state, value.tireHydrodynamicDragN);
    runtime->m_api.lua_pushnumber(state, value.tireWetFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireClassicalHydroplaningSpeedKph);
    // TIRE13 appends compacted-snow / hard-ice telemetry without disturbing
    // the historical 1..135 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireWinterSurfaceFraction);
    runtime->m_api.lua_pushnumber(state, value.tireSnowSurfaceFraction);
    runtime->m_api.lua_pushnumber(state, value.tireIceSurfaceFraction);
    runtime->m_api.lua_pushnumber(state, value.tireWinterFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireWinterStiffnessScale);
    runtime->m_api.lua_pushnumber(state, value.tirePackedSnowFraction);
    runtime->m_api.lua_pushnumber(state, value.tireIceMeltFilmMicrometers);
    runtime->m_api.lua_pushnumber(state, value.tireStudFrictionContribution);
    runtime->m_api.lua_pushnumber(state, value.tireSnowInterlockContribution);
    runtime->m_api.lua_pushnumber(state, value.tireWinterSurfaceTemperatureC);
    // TIRE14 appends shallow gravel / hard-dirt telemetry without disturbing
    // the historical 1..145 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireGranularSurfaceFraction);
    runtime->m_api.lua_pushnumber(state, value.tireGranularSinkageMm);
    runtime->m_api.lua_pushnumber(state, value.tireGranularContactPressureKPa);
    runtime->m_api.lua_pushnumber(state, value.tireGranularTreadEffectiveness);
    runtime->m_api.lua_pushnumber(state, value.tireGranularShearCapacityN);
    runtime->m_api.lua_pushnumber(state, value.tireGranularLongitudinalShearN);
    runtime->m_api.lua_pushnumber(state, value.tireGranularLateralShearN);
    runtime->m_api.lua_pushnumber(state, value.tireGranularBulldozingN);
    runtime->m_api.lua_pushnumber(state, value.tireGranularPlowingDragN);
    runtime->m_api.lua_pushnumber(state, value.tireGranularCompactionPowerW);
    runtime->m_api.lua_pushnumber(state, value.tireGranularFrictionScale);
    // TIRE15 appends persistent deformable-terrain telemetry without
    // disturbing the historical 1..156 unpack order.
    runtime->m_api.lua_pushnumber(state, value.tireTerrainSurfaceFraction);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainSinkageMm);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainRutDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainCompaction);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainMoisture);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainLooseDepthMm);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainShearCapacityN);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainLongitudinalShearN);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainLateralShearN);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainBulldozingN);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainPlowingDragN);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainMfFrictionScale);
    runtime->m_api.lua_pushnumber(state, value.tireTerrainPassCount);
    return 169;
}


int LuaVehicleBindingHandlers::luaVehicleGetWheelTelemetry(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::WheelState value;
    int converted = 0;
    const LuaInteger luaIndex = runtime->m_api.lua_tointegerx(state, 2, &converted);
    const std::size_t wheelIndex = converted && luaIndex >= 1
        ? static_cast<std::size_t>(luaIndex - 1)
        : static_cast<std::size_t>(-1);
    const bool result = runtime->m_physics && runtime->m_physics->vehicles().wheelState(
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    // CLEAN01: named wheel telemetry supersedes the ever-growing positional
    // GetWheelState ABI for first-party Racing United scripts. A Lua table
    // consumes only one return slot, so adding future tire/surface diagnostics
    // cannot exhaust Lua's local-variable or C-stack limits. The legacy
    // GetWheelState entry point remains registered for compatibility.
    runtime->m_api.lua_createtable(state, 0, 205);
    const auto pushNumberField = [&](const char* name, LuaNumber fieldValue) {
        runtime->m_api.lua_pushnumber(state, fieldValue);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto pushBooleanField = [&](const char* name, int fieldValue) {
        runtime->m_api.lua_pushboolean(state, fieldValue);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto pushIntegerField = [&](const char* name, LuaInteger fieldValue) {
        runtime->m_api.lua_pushinteger(state, fieldValue);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto pushStringField = [&](const char* name, const char* fieldValue) {
        runtime->m_api.lua_pushstring(state, fieldValue);
        runtime->m_api.lua_setfield(state, -2, name);
    };

    pushBooleanField("grounded", value.grounded ? 1 : 0);
    pushNumberField("length", value.suspensionLength);
    pushNumberField("compression", value.compression);
    pushNumberField("compressionVelocity", value.compressionVelocity);
    pushNumberField("normalForce", value.normalForce);
    pushNumberField("longitudinalForce", value.longitudinalForce);
    pushNumberField("lateralForce", value.lateralForce);
    pushNumberField("steerAngle", value.steerAngleDegrees);
    pushNumberField("angularVelocity", value.wheelAngularVelocity);
    pushNumberField("rotationDegrees", value.wheelRotationDegrees);
    pushNumberField("centerX", value.worldCenter.x);
    pushNumberField("centerY", value.worldCenter.y);
    pushNumberField("centerZ", value.worldCenter.z);
    pushNumberField("contactX", value.contactPoint.x);
    pushNumberField("contactY", value.contactPoint.y);
    pushNumberField("contactZ", value.contactPoint.z);
    pushNumberField("longitudinalSpeed", value.longitudinalSpeed);
    pushNumberField("lateralSpeed", value.lateralSpeed);
    pushNumberField("slipRatio", value.slipRatio);
    pushNumberField("slipAngleDegrees", value.slipAngleDegrees);
    pushNumberField("relaxedSlipRatio", value.relaxedSlipRatio);
    pushNumberField("relaxedSlipAngleDegrees", value.relaxedSlipAngleDegrees);
    pushNumberField("effectiveFriction", value.effectiveFriction);
    pushNumberField("gripUtilization", value.gripUtilization);
    pushNumberField("pureLongitudinalForce", value.pureLongitudinalForce);
    pushNumberField("pureLateralForce", value.pureLateralForce);
    pushNumberField("combinedSlipScale", value.combinedSlipScale);
    pushNumberField("pneumaticTrail", value.pneumaticTrail);
    pushNumberField("aligningTorque", value.aligningTorque);
    pushNumberField("driveTorque", value.appliedDriveTorque);
    pushNumberField("brakeTorque", value.appliedBrakeTorque);
    pushNumberField("serviceBrakeTorque", value.serviceBrakeTorque);
    pushNumberField("handbrakeTorque", value.handbrakeTorque);
    pushNumberField("antiLockModulation", value.antiLockModulation);
    pushNumberField("tractionControlModulation", value.tractionControlModulation);
    pushBooleanField("antiLockActive", value.antiLockActive ? 1 : 0);
    pushBooleanField("tractionControlActive", value.tractionControlActive ? 1 : 0);
    pushIntegerField("contactCollider", static_cast<LuaInteger>(value.contactCollider));
    pushStringField("surfaceName", heritage::physics::surfaceMaterialName(value.surfaceMaterial));
    pushIntegerField("surfaceId", static_cast<LuaInteger>(value.surfaceMaterial));
    pushNumberField("surfaceWetness", static_cast<LuaNumber>(value.surfaceWetness));
    pushNumberField("surfaceTemperatureC", static_cast<LuaNumber>(value.surfaceTemperatureC));
    pushNumberField("suspensionSpringForce", value.suspensionSpringForce);
    pushNumberField("suspensionDampingForce", value.suspensionDampingForce);
    pushNumberField("suspensionBumpStopForce", value.suspensionBumpStopForce);
    pushNumberField("suspensionDroopStopForce", value.suspensionDroopStopForce);
    pushNumberField("suspensionUnclampedForce", value.suspensionUnclampedForce);
    pushNumberField("damperDissipationWatts", value.damperDissipationWatts);
    pushNumberField("unsprungVelocity", value.unsprungVelocity);
    pushNumberField("tireDeflection", value.tireDeflection);
    pushNumberField("tireDeflectionVelocity", value.tireDeflectionVelocity);
    pushNumberField("tireRadialDissipationWatts", value.tireRadialDissipationWatts);
    pushNumberField("turnSlipPerM", value.turnSlipPerM);
    pushNumberField("normalizedTurnSlip", value.normalizedTurnSlip);
    pushNumberField("contactPatchTwistDegrees", value.contactPatchTwistDegrees);
    pushNumberField("parkingTurnMoment", value.parkingTurnMoment);
    pushNumberField("turnSlipMoment", value.turnSlipMoment);
    pushNumberField("turnSlipLongitudinalReduction", value.turnSlipLongitudinalReduction);
    pushNumberField("turnSlipLateralReduction", value.turnSlipLateralReduction);
    pushNumberField("turnSlipCorneringReduction", value.turnSlipCorneringReduction);
    pushNumberField("turnSlipTrailReduction", value.turnSlipTrailReduction);
    pushNumberField("tireFreeRollingRadius", value.tireFreeRollingRadius);
    pushNumberField("tireLoadedRadius", value.tireLoadedRadius);
    pushNumberField("tireEffectiveRollingRadius", value.tireEffectiveRollingRadius);
    pushNumberField("tireContactPatchLength", value.tireContactPatchLength);
    pushNumberField("tireContactPatchWidth", value.tireContactPatchWidth);
    pushNumberField("tireContactPatchArea", value.tireContactPatchArea);
    pushNumberField("tireEnvelopeRoadOffset", value.tireEnvelopeRoadOffset);
    pushNumberField("tireEnvelopeSlopeDegrees", value.tireEnvelopeSlopeDegrees);
    pushNumberField("tireEnvelopeCrossSlopeDegrees", value.tireEnvelopeCrossSlopeDegrees);
    pushNumberField("tireEnvelopeValidSamples", value.tireEnvelopeValidSamples);
    pushNumberField("tireFootprintTotalSamples", value.tireFootprintTotalSamples);
    pushNumberField("tireFootprintSupportedFraction", value.tireFootprintSupportedFraction);
    pushNumberField("tireFootprintRoughnessRange", value.tireFootprintRoughnessRange);
    pushNumberField("tireFootprintSurfaceFriction", value.tireFootprintSurfaceFriction);
    pushNumberField("tireFootprintSurfaceSpread", value.tireFootprintSurfaceSpread);
    pushBooleanField("tireFootprintRefined", value.tireFootprintRefined ? 1 : 0);
    pushNumberField("tireRingRadialOffset", value.tireRingRadialOffset);
    pushNumberField("tireRingRadialVelocity", value.tireRingRadialVelocity);
    pushNumberField("tireRingLongitudinalOffset", value.tireRingLongitudinalOffset);
    pushNumberField("tireRingLongitudinalVelocity", value.tireRingLongitudinalVelocity);
    pushNumberField("tireRingLateralOffset", value.tireRingLateralOffset);
    pushNumberField("tireRingLateralVelocity", value.tireRingLateralVelocity);
    pushNumberField("tireRingYawDegrees", value.tireRingYawDegrees);
    pushNumberField("tireRingYawRateDegreesPerSecond", value.tireRingYawRateDegreesPerSecond);
    pushNumberField("tireRingWindupDegrees", value.tireRingWindupDegrees);
    pushNumberField("tireRingWindupRateDegreesPerSecond", value.tireRingWindupRateDegreesPerSecond);
    pushNumberField("tireTreadTemperatureC", value.tireTreadTemperatureC);
    pushNumberField("tireCarcassTemperatureC", value.tireCarcassTemperatureC);
    pushNumberField("tireGasTemperatureC", value.tireGasTemperatureC);
    pushNumberField("tireInflationPressurePa", value.tireInflationPressurePa);
    pushStringField("tireFailureStage", heritage::vehicles::tires::tireFailureStageName(value.tireFailureStage));
    pushIntegerField("tireFailureStageId", static_cast<LuaInteger>(value.tireFailureStage));
    pushIntegerField("tireFailureEventSerial", static_cast<LuaInteger>(value.tireFailureEventSerial));
    pushNumberField("tireContainedGasMassRatio", value.tireContainedGasMassRatio);
    pushNumberField("tirePressurizedGasFraction", value.tirePressurizedGasFraction);
    pushNumberField("tirePunctureAreaMm2", value.tirePunctureAreaMm2);
    pushNumberField("tireEffectiveLeakAreaMm2", value.tireEffectiveLeakAreaMm2);
    pushNumberField("tireLeakMassFlowGramsPerSecond", value.tireLeakMassFlowGramsPerSecond);
    pushNumberField("tireStructuralIntegrity", value.tireStructuralIntegrity);
    pushNumberField("tireTreadAttachment", value.tireTreadAttachment);
    pushNumberField("tireRimContactFraction", value.tireRimContactFraction);
    pushNumberField("tireFailureEventElapsedSeconds", value.tireFailureEventElapsedSeconds);
    pushNumberField("tireThermalFrictionScale", value.tireThermalFrictionScale);
    pushNumberField("tireThermalStiffnessScale", value.tireThermalStiffnessScale);
    pushNumberField("tireSlipDissipationWatts", value.tireSlipDissipationWatts);
    pushNumberField("tireThermalLossDissipationWatts", value.tireThermalLossDissipationWatts);
    pushNumberField("tireRoadHeatFlowWatts", value.tireRoadHeatFlowWatts);
    pushNumberField("tireAirHeatFlowWatts", value.tireAirHeatFlowWatts);
    pushNumberField("tireTreadInsideSurfaceTemperatureC", value.tireTreadInsideSurfaceTemperatureC);
    pushNumberField("tireTreadCenterSurfaceTemperatureC", value.tireTreadCenterSurfaceTemperatureC);
    pushNumberField("tireTreadOutsideSurfaceTemperatureC", value.tireTreadOutsideSurfaceTemperatureC);
    pushNumberField("tireTreadHottestSurfaceTemperatureC", value.tireTreadHottestSurfaceTemperatureC);
    pushNumberField("tireTreadInsideDepthMm", value.tireTreadInsideDepthMm);
    pushNumberField("tireTreadCenterDepthMm", value.tireTreadCenterDepthMm);
    pushNumberField("tireTreadOutsideDepthMm", value.tireTreadOutsideDepthMm);
    pushNumberField("tireTreadMinimumDepthMm", value.tireTreadMinimumDepthMm);
    pushNumberField("tireTreadWearFraction", value.tireTreadWearFraction);
    pushNumberField("tireFlatSpotDepthMm", value.tireFlatSpotDepthMm);
    pushNumberField("tireFlatSpotSector", value.tireFlatSpotSector);
    pushNumberField("tireSpatialFrictionScale", value.tireSpatialFrictionScale);
    pushNumberField("tireTreadContactSector", value.tireTreadContactSector);
    pushNumberField("tireTreadHottestSector", value.tireTreadHottestSector);
    pushNumberField("tireAverageTreadRadiusLossMm", value.tireAverageTreadRadiusLossMm);
    pushNumberField("tireContactTreadRadiusLossMm", value.tireContactTreadRadiusLossMm);
    pushNumberField("tireContactRadiusVariationMm", value.tireContactRadiusVariationMm);
    pushNumberField("contactNormalX", value.contactNormal.x);
    pushNumberField("contactNormalY", value.contactNormal.y);
    pushNumberField("contactNormalZ", value.contactNormal.z);
    pushNumberField("tireContaminationFrictionScale", value.tireContaminationFrictionScale);
    pushNumberField("tireContaminationTotal", value.tireContaminationTotal);
    pushNumberField("tireContaminationAverage", value.tireContaminationAverage);
    pushNumberField("tireOrganicContamination", value.tireOrganicContamination);
    pushNumberField("tireMineralContamination", value.tireMineralContamination);
    pushNumberField("tireGravelFinesContamination", value.tireGravelFinesContamination);
    pushNumberField("tireRubberPickupContamination", value.tireRubberPickupContamination);
    pushNumberField("tireMudFilmContamination", value.tireMudFilmContamination);
    pushNumberField("tireContaminationCleaningRate", value.tireContaminationCleaningRate);
    pushNumberField("tireRoadWaterDepthMm", value.tireRoadWaterDepthMm);
    pushNumberField("tireRetainedWaterDepthMm", value.tireRetainedWaterDepthMm);
    pushNumberField("tireDrainageDemandRatio", value.tireDrainageDemandRatio);
    pushNumberField("tireWaterWedgeFraction", value.tireWaterWedgeFraction);
    pushNumberField("tireHydroplaningFraction", value.tireHydroplaningFraction);
    pushNumberField("tirePavementContactFraction", value.tirePavementContactFraction);
    pushNumberField("tireHydrodynamicLiftN", value.tireHydrodynamicLiftN);
    pushNumberField("tireHydrodynamicDragN", value.tireHydrodynamicDragN);
    pushNumberField("tireWetFrictionScale", value.tireWetFrictionScale);
    pushNumberField("tireClassicalHydroplaningSpeedKph", value.tireClassicalHydroplaningSpeedKph);
    pushNumberField("tireWinterSurfaceFraction", value.tireWinterSurfaceFraction);
    pushNumberField("tireSnowSurfaceFraction", value.tireSnowSurfaceFraction);
    pushNumberField("tireIceSurfaceFraction", value.tireIceSurfaceFraction);
    pushNumberField("tireWinterFrictionScale", value.tireWinterFrictionScale);
    pushNumberField("tireWinterStiffnessScale", value.tireWinterStiffnessScale);
    pushNumberField("tirePackedSnowFraction", value.tirePackedSnowFraction);
    pushNumberField("tireIceMeltFilmMicrometers", value.tireIceMeltFilmMicrometers);
    pushNumberField("tireStudFrictionContribution", value.tireStudFrictionContribution);
    pushNumberField("tireSnowInterlockContribution", value.tireSnowInterlockContribution);
    pushNumberField("tireWinterSurfaceTemperatureC", value.tireWinterSurfaceTemperatureC);
    pushNumberField("tireGranularSurfaceFraction", value.tireGranularSurfaceFraction);
    pushNumberField("tireGranularSinkageMm", value.tireGranularSinkageMm);
    pushNumberField("tireGranularContactPressureKPa", value.tireGranularContactPressureKPa);
    pushNumberField("tireGranularTreadEffectiveness", value.tireGranularTreadEffectiveness);
    pushNumberField("tireGranularShearCapacityN", value.tireGranularShearCapacityN);
    pushNumberField("tireGranularLongitudinalShearN", value.tireGranularLongitudinalShearN);
    pushNumberField("tireGranularLateralShearN", value.tireGranularLateralShearN);
    pushNumberField("tireGranularBulldozingN", value.tireGranularBulldozingN);
    pushNumberField("tireGranularPlowingDragN", value.tireGranularPlowingDragN);
    pushNumberField("tireGranularCompactionPowerW", value.tireGranularCompactionPowerW);
    pushNumberField("tireGranularFrictionScale", value.tireGranularFrictionScale);
    pushNumberField("tireTerrainSurfaceFraction", value.tireTerrainSurfaceFraction);
    pushNumberField("tireTerrainSinkageMm", value.tireTerrainSinkageMm);
    pushNumberField("tireTerrainRutDepthMm", value.tireTerrainRutDepthMm);
    pushNumberField("tireTerrainCompaction", value.tireTerrainCompaction);
    pushNumberField("tireTerrainMoisture", value.tireTerrainMoisture);
    pushNumberField("tireTerrainLooseDepthMm", value.tireTerrainLooseDepthMm);
    pushNumberField("tireTerrainShearCapacityN", value.tireTerrainShearCapacityN);
    pushNumberField("tireTerrainLongitudinalShearN", value.tireTerrainLongitudinalShearN);
    pushNumberField("tireTerrainLateralShearN", value.tireTerrainLateralShearN);
    pushNumberField("tireTerrainBulldozingN", value.tireTerrainBulldozingN);
    pushNumberField("tireTerrainPlowingDragN", value.tireTerrainPlowingDragN);
    pushNumberField("tireTerrainMfFrictionScale", value.tireTerrainMfFrictionScale);
    pushNumberField("tireTerrainPassCount", value.tireTerrainPassCount);
    pushNumberField("tireTrackDepositedRubber", value.tireTrackDepositedRubber);
    pushNumberField("tireTrackLooseRubber", value.tireTrackLooseRubber);
    pushNumberField("tireTrackMarbleMaturity", value.tireTrackMarbleMaturity);
    pushNumberField("tireTrackRubberFrictionScale", value.tireTrackRubberFrictionScale);
    pushNumberField("tireTrackRubberPassCount", value.tireTrackRubberPassCount);
    pushStringField("contactStatus", heritage::vehicles::wheelContactStatusName(value.contactStatus));
    pushIntegerField("contactStatusId", static_cast<LuaInteger>(value.contactStatus));
    pushIntegerField("contactLossTransitions", static_cast<LuaInteger>(value.contactLossTransitionCount));
    pushIntegerField("rayCandidates", static_cast<LuaInteger>(value.rayCandidateCount));
    pushIntegerField("rayExactTests", static_cast<LuaInteger>(value.rayExactTestCount));
    pushIntegerField("staticTriangleCandidates", static_cast<LuaInteger>(value.staticTriangleCandidateCount));
    pushBooleanField("staticSceneLoaded", value.staticSceneLoaded ? 1 : 0);
    pushBooleanField("originInsideStaticSceneBounds", value.originInsideStaticSceneBounds ? 1 : 0);
    pushBooleanField("rayBoundsOverlapStaticScene", value.rayBoundsOverlapStaticScene ? 1 : 0);
    pushBooleanField("selectedHitWasStaticTriangle", value.selectedHitWasStaticTriangle ? 1 : 0);
    pushNumberField("rawSupportDistance", value.rawSupportDistance);
    pushBooleanField("suspensionBottomed", value.suspensionBottomed ? 1 : 0);
    pushNumberField("bottomOutPenetration", value.bottomOutPenetration);
    pushNumberField("camberAngleDegrees", value.camberAngleDegrees);
    pushNumberField("toeAngleDegrees", value.toeAngleDegrees);
    pushNumberField("uprightRotationX", value.localUprightRotationDegrees.x);
    pushNumberField("uprightRotationY", value.localUprightRotationDegrees.y);
    pushNumberField("uprightRotationZ", value.localUprightRotationDegrees.z);
    pushNumberField("steeringAxisX", value.worldSteeringAxis.x);
    pushNumberField("steeringAxisY", value.worldSteeringAxis.y);
    pushNumberField("steeringAxisZ", value.worldSteeringAxis.z);
    pushNumberField("wheelForwardX", value.worldWheelForward.x);
    pushNumberField("wheelForwardY", value.worldWheelForward.y);
    pushNumberField("wheelForwardZ", value.worldWheelForward.z);
    pushNumberField("wheelRightX", value.worldWheelRight.x);
    pushNumberField("wheelRightY", value.worldWheelRight.y);
    pushNumberField("wheelRightZ", value.worldWheelRight.z);
    pushNumberField("wheelUpX", value.worldWheelUp.x);
    pushNumberField("wheelUpY", value.worldWheelUp.y);
    pushNumberField("wheelUpZ", value.worldWheelUp.z);

    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetWheelContactDiagnostic(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
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
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
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

int LuaVehicleBindingHandlers::luaVehicleGetWheelUprightPose(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
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
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1), wheelIndex, value);
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

int LuaVehicleBindingHandlers::luaVehicleGetChassisFlexState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;

    heritage::vehicles::ChassisTorsionalComplianceDescription description;
    heritage::vehicles::ChassisTorsionalComplianceState flexState;
    const bool result = runtime->m_physics
        && runtime->m_physics->vehicles().chassisTorsionalCompliance(
            LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1),
            description,
            flexState);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const heritage::vehicles::ChassisFlexDiagnostics diagnostics =
        heritage::vehicles::evaluateChassisFlexDiagnostics(
            description, flexState);
    runtime->m_api.lua_createtable(state, 0, 20);
    const auto pushNumberField = [&](const char* name, double value) {
        runtime->m_api.lua_pushnumber(state, value);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    runtime->m_api.lua_pushboolean(state, description.enabled ? 1 : 0);
    runtime->m_api.lua_setfield(state, -2, "enabled");
    pushNumberField(
        "torsionalRigidityNmPerDegree",
        description.torsionalRigidityNmPerDegree);
    pushNumberField(
        "torsionalDampingNmsPerRad",
        description.torsionalDampingNmsPerRad);
    pushNumberField(
        "effectiveTorsionalInertiaKgM2",
        description.effectiveTorsionalInertiaKgM2);
    pushNumberField("torsionAxisLocalY", description.torsionAxisLocalY);
    pushNumberField("frontReferenceLocalZ", description.frontReferenceLocalZ);
    pushNumberField("rearReferenceLocalZ", description.rearReferenceLocalZ);
    pushNumberField("maximumTwistDegrees", description.maximumTwistDegrees);
    pushNumberField("twistDegrees", diagnostics.twistDegrees);
    pushNumberField(
        "twistRateDegreesPerSecond",
        diagnostics.twistRateDegreesPerSecond);
    pushNumberField("frontRollMomentNm", flexState.frontRollMomentNm);
    pushNumberField("rearRollMomentNm", flexState.rearRollMomentNm);
    pushNumberField("driveTorqueNm", flexState.driveTorqueNm);
    pushNumberField("springTorqueNm", flexState.springTorqueNm);
    pushNumberField("dampingTorqueNm", flexState.dampingTorqueNm);
    pushNumberField("elasticEnergyJ", diagnostics.elasticEnergyJ);
    pushNumberField("kineticEnergyJ", diagnostics.kineticEnergyJ);
    pushNumberField(
        "frontSectionTwistDegrees",
        diagnostics.frontSectionTwistDegrees);
    pushNumberField(
        "rearSectionTwistDegrees",
        diagnostics.rearSectionTwistDegrees);
    runtime->m_api.lua_pushboolean(state, flexState.saturated ? 1 : 0);
    runtime->m_api.lua_setfield(state, -2, "saturated");
    return 1;
}

int LuaVehicleBindingHandlers::luaVehicleGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime) return 0;
    const std::string error = runtime->m_physics
        ? runtime->m_physics->vehicles().lastError()
        : std::string("Vehicle system is unavailable.");
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

} // namespace heritage::modules
