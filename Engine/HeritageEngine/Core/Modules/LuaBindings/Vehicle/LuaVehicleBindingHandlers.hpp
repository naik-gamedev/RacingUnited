#pragma once

struct lua_State;

namespace heritage::modules {

// CLEAN12: private Lua C-handler catalogue for the vehicle binding domain.
// Kept out of LuaModuleRuntime.hpp so ordinary runtime consumers do not
// parse hundreds of unrelated binding declarations.

struct LuaVehicleBindingHandlers
{

    static int luaVehicleSetInputs(lua_State* state);
    static int luaVehicleSetWheelBrakeFactors(lua_State* state);
    static int luaVehicleSetDriverAids(lua_State* state);
    static int luaVehicleGetDriverAidState(lua_State* state);
    static int luaVehicleSetTuning(lua_State* state);
    static int luaVehicleSetHighRateHertz(lua_State* state);
    static int luaVehicleSetSteeringGeometry(lua_State* state);
    static int luaVehicleGetSteeringState(lua_State* state);
    static int luaVehicleGetHighRateHertz(lua_State* state);
    static int luaVehicleIsAvailable(lua_State* state);
    static int luaVehicleCompileDefinitionV2(lua_State* state);
    static int luaVehicleCreateFromDefinitionV2(lua_State* state);
    static int luaVehicleCreate(lua_State* state);
    static int luaVehicleDestroy(lua_State* state);
    static int luaVehicleExists(lua_State* state);
    static int luaVehicleGetCount(lua_State* state);
    static int luaVehicleInspectAssetMetadata(lua_State* state);
    static int luaVehicleCheckTireWheelCompatibility(lua_State* state);
    static int luaVehicleAddWheel(lua_State* state);
    static int luaVehicleGetWheelCount(lua_State* state);
    static int luaVehicleSetPowertrain(lua_State* state);
    static int luaVehicleSetGearRatios(lua_State* state);
    static int luaVehicleSetDifferential(lua_State* state);
    static int luaVehicleSetGear(lua_State* state);
    static int luaVehicleShiftUp(lua_State* state);
    static int luaVehicleShiftDown(lua_State* state);
    static int luaVehicleGetDrivetrainState(lua_State* state);
    static int luaVehicleGetForwardGearCount(lua_State* state);
    static int luaVehicleStartDynamicsLab(lua_State* state);
    static int luaVehicleStopDynamicsLab(lua_State* state);
    static int luaVehicleClearDynamicsLab(lua_State* state);
    static int luaVehicleGetDynamicsLabSummary(lua_State* state);
    static int luaVehicleGetDynamicsLabSeries(lua_State* state);
    static int luaVehicleExportDynamicsLabCsv(lua_State* state);
    static int luaVehicleSetWheelFitment(lua_State* state);
    static int luaVehicleGetWheelFitment(lua_State* state);
    static int luaVehicleGetWheelFitmentGeometry(lua_State* state);
    static int luaVehicleSetWheelAlignment(lua_State* state);
    static int luaVehicleGetWheelAlignment(lua_State* state);
    static int luaVehicleEstimateMassProperties(lua_State* state);
    static int luaVehicleSetWheelSuspensionModel(lua_State* state);
    static int luaVehicleGetWheelSuspensionModel(lua_State* state);
    static int luaVehicleSetWheelSuspensionGeometry(lua_State* state);
    static int luaVehicleGetWheelSuspensionGeometry(lua_State* state);
    static int luaVehicleSetAntiRollBar(lua_State* state);
    static int luaVehicleGetAntiRollBar(lua_State* state);
    static int luaVehicleGetAntiRollBarCount(lua_State* state);
    static int luaVehicleSetChassisTorsionalCompliance(lua_State* state);
    static int luaVehicleEstimateChassisFlex(lua_State* state);
    static int luaVehicleEstimateMacPhersonHardpoints(lua_State* state);
    static int luaVehicleEstimateTrailingArmHardpoints(lua_State* state);
    static int luaVehicleSetWheelSuspensionHardpoints(lua_State* state);
    static int luaVehicleSetWheelUnsprungMassModel(lua_State* state);
    static int luaVehicleGetWheelUnsprungMassModel(lua_State* state);
    static int luaVehicleGetSpeed(lua_State* state);
    static int luaVehicleGetGroundedWheelCount(lua_State* state);
    static int luaVehicleGetLastHighRateStepCount(lua_State* state);
    static int luaVehicleGetTotalHighRateStepCount(lua_State* state);
    static int luaVehicleGetWheelState(lua_State* state);
    static int luaVehicleGetWheelTelemetry(lua_State* state);
    static int luaVehicleGetWheelContactDiagnostic(lua_State* state);
    static int luaVehicleGetWheelUprightPose(lua_State* state);
    static int luaVehicleGetChassisFlexState(lua_State* state);
    static int luaVehicleGetLastError(lua_State* state);
    static int luaVehicleSetTireModel(lua_State* state);
    static int luaVehicleSetWheelTireModel(lua_State* state);
    static int luaVehicleLoadWheelTirePropertyFile(lua_State* state);
    static int luaVehicleGetWheelTireModel(lua_State* state);
    static int luaVehicleGetWheelTireParameterInfo(lua_State* state);
    static int luaVehicleRunTireCalibrationSweep(lua_State* state);
    static int luaVehicleExportTireCalibrationSweepCsv(lua_State* state);
    static int luaVehicleRunTireScenario(lua_State* state);
    static int luaVehicleRunTireFleetBenchmark(lua_State* state);
    static int luaVehicleExportTireScenarioCsv(lua_State* state);
    static int luaVehicleSetTireColdInflationPressure(lua_State* state);
    static int luaVehicleTriggerWheelTireFailure(lua_State* state);
    static int luaVehicleTriggerTireFailure(lua_State* state);
    static int luaVehicleGetTireColdInflationPressureRange(lua_State* state);
    static int luaVehicleResetTirePhysicalState(lua_State* state);
    static int luaVehicleSetTireContactFidelity(lua_State* state);
    static int luaVehicleGetTireContactFidelity(lua_State* state);
    static int luaVehicleSetSurfacePreset(lua_State* state);
    static int luaVehicleGetSurfacePreset(lua_State* state);
};

} // namespace heritage::modules
