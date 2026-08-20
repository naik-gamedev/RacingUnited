#include "../../LuaModuleRuntime.hpp"
#include "LuaVehicleBindingHandlers.hpp"

namespace heritage::modules {

void LuaModuleRuntime::registerVehicleBindings()
{
    registerFunction("Vehicle", "IsAvailable", &LuaVehicleBindingHandlers::luaVehicleIsAvailable);
    registerFunction("Vehicle", "CompileDefinitionV2", &LuaVehicleBindingHandlers::luaVehicleCompileDefinitionV2);
    registerFunction("Vehicle", "CreateFromDefinitionV2", &LuaVehicleBindingHandlers::luaVehicleCreateFromDefinitionV2);
    registerFunction("Vehicle", "Create", &LuaVehicleBindingHandlers::luaVehicleCreate);
    registerFunction("Vehicle", "Destroy", &LuaVehicleBindingHandlers::luaVehicleDestroy);
    registerFunction("Vehicle", "Exists", &LuaVehicleBindingHandlers::luaVehicleExists);
    registerFunction("Vehicle", "GetCount", &LuaVehicleBindingHandlers::luaVehicleGetCount);
    registerFunction("Vehicle", "AddWheel", &LuaVehicleBindingHandlers::luaVehicleAddWheel);
    registerFunction("Vehicle", "GetWheelCount", &LuaVehicleBindingHandlers::luaVehicleGetWheelCount);
    registerFunction("Vehicle", "InspectAssetMetadata", &LuaVehicleBindingHandlers::luaVehicleInspectAssetMetadata);
    registerFunction("Vehicle", "CheckTireWheelCompatibility", &LuaVehicleBindingHandlers::luaVehicleCheckTireWheelCompatibility);
    registerFunction("Vehicle", "SetWheelSuspensionModel", &LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionModel);
    registerFunction("Vehicle", "GetWheelSuspensionModel", &LuaVehicleBindingHandlers::luaVehicleGetWheelSuspensionModel);
    registerFunction("Vehicle", "SetWheelSuspensionGeometry", &LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionGeometry);
    registerFunction("Vehicle", "GetWheelSuspensionGeometry", &LuaVehicleBindingHandlers::luaVehicleGetWheelSuspensionGeometry);
    registerFunction("Vehicle", "SetWheelFitment", &LuaVehicleBindingHandlers::luaVehicleSetWheelFitment);
    registerFunction("Vehicle", "GetWheelFitment", &LuaVehicleBindingHandlers::luaVehicleGetWheelFitment);
    registerFunction("Vehicle", "GetWheelFitmentGeometry", &LuaVehicleBindingHandlers::luaVehicleGetWheelFitmentGeometry);
    registerFunction("Vehicle", "SetWheelAlignment", &LuaVehicleBindingHandlers::luaVehicleSetWheelAlignment);
    registerFunction("Vehicle", "GetWheelAlignment", &LuaVehicleBindingHandlers::luaVehicleGetWheelAlignment);
    registerFunction("Vehicle", "SetAntiRollBar", &LuaVehicleBindingHandlers::luaVehicleSetAntiRollBar);
    registerFunction("Vehicle", "GetAntiRollBar", &LuaVehicleBindingHandlers::luaVehicleGetAntiRollBar);
    registerFunction("Vehicle", "GetAntiRollBarCount", &LuaVehicleBindingHandlers::luaVehicleGetAntiRollBarCount);
    registerFunction("Vehicle", "SetChassisTorsionalCompliance", &LuaVehicleBindingHandlers::luaVehicleSetChassisTorsionalCompliance);
    registerFunction("Vehicle", "EstimateChassisFlex", &LuaVehicleBindingHandlers::luaVehicleEstimateChassisFlex);
    registerFunction("Vehicle", "GetChassisFlexState", &LuaVehicleBindingHandlers::luaVehicleGetChassisFlexState);
    registerFunction("Vehicle", "EstimateMassProperties", &LuaVehicleBindingHandlers::luaVehicleEstimateMassProperties);
    registerFunction("Vehicle", "EstimateMacPhersonHardpoints", &LuaVehicleBindingHandlers::luaVehicleEstimateMacPhersonHardpoints);
    registerFunction("Vehicle", "EstimateTrailingArmHardpoints", &LuaVehicleBindingHandlers::luaVehicleEstimateTrailingArmHardpoints);
    registerFunction("Vehicle", "SetWheelSuspensionHardpoints", &LuaVehicleBindingHandlers::luaVehicleSetWheelSuspensionHardpoints);
    registerFunction("Vehicle", "SetWheelUnsprungMassModel", &LuaVehicleBindingHandlers::luaVehicleSetWheelUnsprungMassModel);
    registerFunction("Vehicle", "GetWheelUnsprungMassModel", &LuaVehicleBindingHandlers::luaVehicleGetWheelUnsprungMassModel);
    registerFunction("Vehicle", "SetInputs", &LuaVehicleBindingHandlers::luaVehicleSetInputs);
    registerFunction("Vehicle", "SetWheelBrakeFactors", &LuaVehicleBindingHandlers::luaVehicleSetWheelBrakeFactors);
    registerFunction("Vehicle", "SetDriverAids", &LuaVehicleBindingHandlers::luaVehicleSetDriverAids);
    registerFunction("Vehicle", "GetDriverAidState", &LuaVehicleBindingHandlers::luaVehicleGetDriverAidState);
    registerFunction("Vehicle", "SetTuning", &LuaVehicleBindingHandlers::luaVehicleSetTuning);
    registerFunction("Vehicle", "SetTireModel", &LuaVehicleBindingHandlers::luaVehicleSetTireModel);
    registerFunction("Vehicle", "SetWheelTireModel", &LuaVehicleBindingHandlers::luaVehicleSetWheelTireModel);
    registerFunction("Vehicle", "LoadWheelTirePropertyFile", &LuaVehicleBindingHandlers::luaVehicleLoadWheelTirePropertyFile);
    registerFunction("Vehicle", "GetWheelTireModel", &LuaVehicleBindingHandlers::luaVehicleGetWheelTireModel);
    registerFunction("Vehicle", "GetWheelTireParameterInfo", &LuaVehicleBindingHandlers::luaVehicleGetWheelTireParameterInfo);
    registerFunction("Vehicle", "RunTireCalibrationSweep", &LuaVehicleBindingHandlers::luaVehicleRunTireCalibrationSweep);
    registerFunction("Vehicle", "ExportTireCalibrationSweepCsv", &LuaVehicleBindingHandlers::luaVehicleExportTireCalibrationSweepCsv);
    registerFunction("Vehicle", "RunTireScenario", &LuaVehicleBindingHandlers::luaVehicleRunTireScenario);
    registerFunction("Vehicle", "RunTireFleetBenchmark", &LuaVehicleBindingHandlers::luaVehicleRunTireFleetBenchmark);
    registerFunction("Vehicle", "ExportTireScenarioCsv", &LuaVehicleBindingHandlers::luaVehicleExportTireScenarioCsv);
    registerFunction("Vehicle", "SetTireColdInflationPressure", &LuaVehicleBindingHandlers::luaVehicleSetTireColdInflationPressure);
    registerFunction("Vehicle", "TriggerWheelTireFailure", &LuaVehicleBindingHandlers::luaVehicleTriggerWheelTireFailure);
    registerFunction("Vehicle", "TriggerTireFailure", &LuaVehicleBindingHandlers::luaVehicleTriggerTireFailure);
    registerFunction("Vehicle", "GetTireColdInflationPressureRange", &LuaVehicleBindingHandlers::luaVehicleGetTireColdInflationPressureRange);
    registerFunction("Vehicle", "ResetTirePhysicalState", &LuaVehicleBindingHandlers::luaVehicleResetTirePhysicalState);
    registerFunction("Vehicle", "SetTireContactFidelity", &LuaVehicleBindingHandlers::luaVehicleSetTireContactFidelity);
    registerFunction("Vehicle", "GetTireContactFidelity", &LuaVehicleBindingHandlers::luaVehicleGetTireContactFidelity);
    registerFunction("Vehicle", "SetSurfacePreset", &LuaVehicleBindingHandlers::luaVehicleSetSurfacePreset);
    registerFunction("Vehicle", "GetSurfacePreset", &LuaVehicleBindingHandlers::luaVehicleGetSurfacePreset);
    registerFunction("Vehicle", "SetHighRateHertz", &LuaVehicleBindingHandlers::luaVehicleSetHighRateHertz);
    registerFunction("Vehicle", "SetSteeringGeometry", &LuaVehicleBindingHandlers::luaVehicleSetSteeringGeometry);
    registerFunction("Vehicle", "GetSteeringState", &LuaVehicleBindingHandlers::luaVehicleGetSteeringState);
    registerFunction("Vehicle", "SetPowertrain", &LuaVehicleBindingHandlers::luaVehicleSetPowertrain);
    registerFunction("Vehicle", "SetGearRatios", &LuaVehicleBindingHandlers::luaVehicleSetGearRatios);
    registerFunction("Vehicle", "SetDifferential", &LuaVehicleBindingHandlers::luaVehicleSetDifferential);
    registerFunction("Vehicle", "SetGear", &LuaVehicleBindingHandlers::luaVehicleSetGear);
    registerFunction("Vehicle", "ShiftUp", &LuaVehicleBindingHandlers::luaVehicleShiftUp);
    registerFunction("Vehicle", "ShiftDown", &LuaVehicleBindingHandlers::luaVehicleShiftDown);
    registerFunction("Vehicle", "GetDrivetrainState", &LuaVehicleBindingHandlers::luaVehicleGetDrivetrainState);
    registerFunction("Vehicle", "GetForwardGearCount", &LuaVehicleBindingHandlers::luaVehicleGetForwardGearCount);
    registerFunction("Vehicle", "GetHighRateHertz", &LuaVehicleBindingHandlers::luaVehicleGetHighRateHertz);
    registerFunction("Vehicle", "GetSpeed", &LuaVehicleBindingHandlers::luaVehicleGetSpeed);
    registerFunction("Vehicle", "GetGroundedWheelCount", &LuaVehicleBindingHandlers::luaVehicleGetGroundedWheelCount);
    registerFunction("Vehicle", "GetLastHighRateStepCount", &LuaVehicleBindingHandlers::luaVehicleGetLastHighRateStepCount);
    registerFunction("Vehicle", "GetTotalHighRateStepCount", &LuaVehicleBindingHandlers::luaVehicleGetTotalHighRateStepCount);
    registerFunction("Vehicle", "StartDynamicsLab", &LuaVehicleBindingHandlers::luaVehicleStartDynamicsLab);
    registerFunction("Vehicle", "StopDynamicsLab", &LuaVehicleBindingHandlers::luaVehicleStopDynamicsLab);
    registerFunction("Vehicle", "ClearDynamicsLab", &LuaVehicleBindingHandlers::luaVehicleClearDynamicsLab);
    registerFunction("Vehicle", "GetDynamicsLabSummary", &LuaVehicleBindingHandlers::luaVehicleGetDynamicsLabSummary);
    registerFunction("Vehicle", "GetDynamicsLabSeries", &LuaVehicleBindingHandlers::luaVehicleGetDynamicsLabSeries);
    registerFunction("Vehicle", "ExportDynamicsLabCsv", &LuaVehicleBindingHandlers::luaVehicleExportDynamicsLabCsv);
    registerFunction("Vehicle", "GetWheelState", &LuaVehicleBindingHandlers::luaVehicleGetWheelState);
    registerFunction("Vehicle", "GetWheelTelemetry", &LuaVehicleBindingHandlers::luaVehicleGetWheelTelemetry);
    registerFunction("Vehicle", "GetWheelContactDiagnostic", &LuaVehicleBindingHandlers::luaVehicleGetWheelContactDiagnostic);
    registerFunction("Vehicle", "GetWheelUprightPose", &LuaVehicleBindingHandlers::luaVehicleGetWheelUprightPose);
    registerFunction("Vehicle", "GetLastError", &LuaVehicleBindingHandlers::luaVehicleGetLastError);
}

} // namespace heritage::modules
