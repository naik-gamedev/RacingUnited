# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

$required = @(
    "Docs\README.md",
    "Docs\PROJECT_STATE.md",
    "Docs\AI_WORKFLOW.md",
    "Docs\ARCHITECTURE.md",
    "Docs\MEMORY_OWNERSHIP.md",
    "Docs\LUA_API_RULES.md",
    "Docs\LUA_BINDING_ARCHITECTURE.md",
    "Docs\PHYSICS_ARCHITECTURE.md",
    "Docs\NUMERICAL_PRECISION.md",
    "Docs\VEGETATION_ARCHITECTURE.md",
    "Docs\PERFORMANCE_MONITORING.md",
    "Docs\PERF06_INPUT_HOTPLUG_STUTTER.md",
    "Docs\TERRAIN_CONTACT_DIAGNOSTICS.md",
    "Docs\VEHICLE_ARCHITECTURE.md",
    "Docs\VEHICLE_DYNAMICS_LAB.md",
    "Docs\SUSPENSION_MODEL.md",
    "Docs\VEHICLE_WORKSHOP.md",
    "Docs\VEHICLE_DEFINITION_RUNTIME.md",
    "Docs\WHEEL_FITMENT_AND_ALIGNMENT.md",
    "Docs\TIRE_MODEL.md",
    "Docs\TIRE_SURFACE_ROADMAP.md",
    "Docs\Decisions\ADR-044-Spatial-Tread-Contamination-And-Cleaning.md",
    "Docs\Decisions\ADR-005-Advanced-Road-Tire-Provider.md",
    "Docs\Decisions\ADR-035-Public-MF62-Motorcycle-Tire-Branch.md",
    "Docs\Decisions\ADR-006-Per-Wheel-Tire-Profiles.md",
    "Docs\Decisions\ADR-007-Player-Vehicle-Visual-Slot.md",
    "Docs\Decisions\ADR-008-Articulated-Wheel-Presentation.md",
    "Docs\Decisions\ADR-009-Wheel-Coordinate-Contract.md",
    "Docs\Decisions\ADR-010-Blender-Authoring-Player-Scene.md",
    "Docs\Decisions\ADR-012-Native-Vehicle-Definition-Compiler.md",
    "Docs\Decisions\ADR-013-Suspension-Provider-Contract.md",
    "Docs\Decisions\ADR-014-Nonlinear-Suspension-Forces.md",
    "Docs\Decisions\ADR-015-Live-Per-Wheel-Suspension-Tuning.md",
    "Docs\Decisions\ADR-016-Scalar-Unsprung-Mass.md",
    "Docs\Decisions\ADR-017-Authoritative-Suspension-Upright-Pose.md",
    "Docs\Decisions\ADR-018-Observable-Wheel-Contact-Loss.md",
    "Docs\Decisions\ADR-022-Suspension-Hardpoint-Authoring-Contract.md",
    "Docs\Decisions\ADR-023-MacPherson-Hardpoint-Kinematics.md",
    "Docs\Decisions\ADR-024-Assisted-Suspension-Hardpoint-Estimation.md",
    "Docs\Decisions\ADR-025-Suspension-Geometry-vs-Wheel-Fitment.md",
    "Docs\Decisions\ADR-026-Trailing-Arm-Torsion-Bar-Kinematics.md",
    "Docs\Decisions\ADR-027-Reusable-Suspension-Anti-Roll-Bar.md",
    "Docs\SUSPENSION_AUTHORING.md",
    "Docs\UNSPRUNG_MASS_MODEL.md",
    "Docs\SUSPENSION_GEOMETRY.md",
    "Docs\SCENE_GLB_AUTHORING.md",
    "Docs\LuaApiAnnotations.json",
    "Engine\HeritageEngine\Core\Diagnostics\BuildIdentity.hpp",
    "Engine\HeritageEngine\Core\Diagnostics\PerformanceMonitor.hpp",
    "Engine\HeritageEngine\Core\Diagnostics\PerformanceMonitor.cpp",
    "Engine\HeritageEngine\Core\Diagnostics\GeneratedBuildIdentity.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaBindingInternals.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaUiBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsWorldBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsBodyBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsColliderBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsQueryBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsConstraintBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleDefinitionBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleDefinitionParser.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleSuspensionBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleDrivetrainBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityCoreBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityMeshBindings.cpp",
    "Engine\HeritageEngine\Core\Paths\Utf8Path.hpp",
    "Engine\HeritageEngine\Vehicles\VehiclePrecision.hpp",
    "Engine\HeritageEngine\Vehicles\TireModel.hpp",
    "Engine\HeritageEngine\Vehicles\TireModel.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\MagicFormula\MagicFormula62.hpp",
    "Engine\HeritageEngine\Vehicles\Tires\MagicFormula\MagicFormula62.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\MotorcycleTireProfile.hpp",
    "Engine\HeritageEngine\Vehicles\Tires\MotorcycleTireProfile.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\TireSlipDynamics.hpp",
    "Engine\HeritageEngine\Vehicles\Tires\TireSlipDynamics.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\TireSurfaceInteraction.hpp",
    "Engine\HeritageEngine\Vehicles\Tires\TireSurfaceInteraction.cpp",
    "Engine\HeritageEngine\Tests\TireModelRegression.cpp",
    "Engine\HeritageEngine\Vehicles\SuspensionModel.hpp",
    "Engine\HeritageEngine\Vehicles\SuspensionModel.cpp",
    "Engine\HeritageEngine\Vehicles\SuspensionGeometry.hpp",
    "Engine\HeritageEngine\Vehicles\SuspensionGeometry.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Springs\TorsionBar.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Springs\TorsionBar.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionAntiRollBar.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionAntiRollBar.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Authoring\TrailingArmHardpointEstimator.hpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Authoring\TrailingArmHardpointEstimator.cpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\WheelFitment.hpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\WheelFitment.cpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\HubReferenceGeometry.hpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\HubReferenceGeometry.cpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.hpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.cpp",
    "Engine\HeritageEngine\Vehicles\UnsprungMassModel.hpp",
    "Engine\HeritageEngine\Vehicles\UnsprungMassModel.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleDynamicsLab.hpp",
    "Engine\HeritageEngine\Vehicles\VehicleDynamicsLab.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleDefinition.hpp",
    "Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.hpp",
    "Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleDefinitionLoader.hpp",
    "Engine\HeritageEngine\Vehicles\VehicleDefinitionLoader.cpp",
    "Engine\HeritageEngine\Physics\StaticBoxSceneImporter.hpp",
    "Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp",
    "Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.hpp",
    "Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp",
    "Engine\HeritageEngine\Graphics\GltfSceneData.hpp",
    "Engine\HeritageEngine\Graphics\VegetationSystem.hpp",
    "Engine\HeritageEngine\Graphics\VegetationSystem.cpp",
    "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj",
    "Engine\HeritageEngine\Tests\PhysicsRegression.cpp",
    "Engine\HeritageEngine\Tests\PhysicsRegressionCommon.hpp",
    "Engine\HeritageEngine\Tests\PhysicsRegressionSupport.cpp",
    "Engine\HeritageEngine\Tests\VehicleDynamicsRegression.cpp",
    "Engine\HeritageEngine\Tests\CollisionTerrainRegression.cpp",
    "Engine\HeritageEngine\Tests\SuspensionRegression.cpp",
    "Engine\HeritageEngine\Tests\ChassisDynamicsRegression.cpp",
    "Engine\HeritageEngine\Tests\ChassisFlexRegression.cpp",
    "Engine\HeritageEngine\Tests\MassPropertiesRegression.cpp",
    "Engine\HeritageEngine\Tests\FitmentRegression.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisTorsionalCompliance.hpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisTorsionalCompliance.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisFlexEstimator.hpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisFlexEstimator.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisFlexDiagnostics.hpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\ChassisFlexDiagnostics.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesEstimator.hpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesEstimator.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesAccumulator.hpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesAccumulator.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesDiagnostics.cpp",
    "Docs\Decisions\ADR-028-Body-Reference-Origin-vs-Center-of-Mass.md",
    "Docs\Decisions\ADR-029-Combined-Pitch-Roll-Yaw-and-Four-Corner-Response.md",
    "Docs\Decisions\ADR-030-Chassis-Torsional-Compliance.md",
    "Docs\Decisions\ADR-031-Explicit-Vehicle-Mass-Properties.md",
    "Docs\Decisions\ADR-032-Reference-Assembly-vs-Vehicle-Setup.md",
    "Docs\Decisions\ADR-033-Factory-Alignment-Specification-vs-Setup.md",
    "Docs\Decisions\ADR-034-Wheel-Hub-Datums-and-Steering-Ground-Geometry.md",
    "Engine\HeritageEngine\Tests\VehicleDefinitionRegression.cpp",
    "Engine\HeritageEngine\HeritageEngine\main.cpp",
    "Engine\HeritageEngine\HeritageEngine\HeritageEngine.hpp",
    "Engine\HeritageEngine\HeritageEngine\HeritageEngine.cpp",
    "Engine\HeritageEngine\HeritageEngine\Runtime\EngineStartup.hpp",
    "Engine\HeritageEngine\HeritageEngine\Runtime\EngineStartup.cpp",
    "Engine\HeritageEngine\HeritageEngine\Runtime\EngineUiStyle.hpp",
    "Engine\HeritageEngine\HeritageEngine\Runtime\EngineUiStyle.cpp",
    "Engine\HeritageEngine\HeritageEngine\Display\DisplayModeController.hpp",
    "Engine\HeritageEngine\HeritageEngine\Display\DisplayModeController.cpp",
    "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.hpp",
    "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp",
    "Engine\HeritageEngine\Platform\Windows\BackbufferClipboard.hpp",
    "Engine\HeritageEngine\Platform\Windows\BackbufferClipboard.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityBindingRegistration.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsBindingRegistration.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleBindingRegistration.cpp",
    "Docs\Decisions\ADR-053-Heritage-Engine-Shell-And-Domain-Lua-Registration.md",
    "Modules\RacingUnited\Scripts\Main.lua",
    "Modules\RacingUnited\Scripts\Runtime\SurfaceDemo.lua",
    "Modules\RacingUnited\Scripts\Runtime\PlayerWorld.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\Peugeot206RC\AlignmentSpecification.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2Builder.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2Validation.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2DynamicsValidation.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2Compatibility.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2Serialization.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Workshop.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Fitment.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\FitmentPanel.lua",
    "Modules\RacingUnited\Tests\VehicleDefinitionV2Tests.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visuals.lua",
    "Modules\RacingUnited\Scripts\Vehicles\DynamicsLab.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Suspension.lua",
    "Modules\RacingUnited\Scripts\Vehicles\AntiRollBars.lua",
    "Modules\RacingUnited\Scripts\Vehicles\ChassisFlex.lua",
    "Modules\RacingUnited\Scripts\Vehicles\MassProperties.lua",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleMassBindings.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleFitmentBindings.cpp",
    "Modules\RacingUnited\Scripts\UI\Vehicle\VisualPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\BodyPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\WheelsPanel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\VisualWheels.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visual\TransformMath.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visual\ArticulatedWheels.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visual\EmbeddedWheelBinding.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visual\VisualWheels.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointSources.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointEstimation.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Suspension\SuspensionAuthoring.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointGizmos.lua",
    "Docs\Decisions\ADR-054-Lua-Responsibility-Owned-Module-Facades.md",
    "Modules\RacingUnited\Scripts\UI\VehicleDebugPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Prototype\VegetationPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\DynamicsLabPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\SuspensionPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\WorkshopPanel.lua"
)
foreach ($relative in $required) {
    Check (Test-Path (Join-Path $Root $relative)) "required file exists: $relative"
}

# VALID01: PlayerCar.obj and PlayerWheel.obj were temporary creator-owned OBJ
# development slots. The modern Racing United path discovers Vehicle_*.glb assets
# anywhere below Assets/Vehicles, so deleting the legacy OBJ files must not make
# repository validation fail. Keep the old Lua fallback strings valid for older
# projects, but treat the physical OBJ files themselves as optional compatibility
# assets rather than required project files.
$legacyPlayerCar = Join-Path $Root "Modules\RacingUnited\Assets\Vehicles\Player\PlayerCar.obj"
$legacyPlayerWheel = Join-Path $Root "Modules\RacingUnited\Assets\Vehicles\Player\PlayerWheel.obj"
if (Test-Path $legacyPlayerCar) {
    $results.Add("PASS: optional legacy PlayerCar.obj compatibility asset is present")
} else {
    $results.Add("PASS: legacy PlayerCar.obj is absent (modern Vehicle_*.glb path is allowed)")
}
if (Test-Path $legacyPlayerWheel) {
    $results.Add("PASS: optional legacy PlayerWheel.obj compatibility asset is present")
} else {
    $results.Add("PASS: legacy PlayerWheel.obj is absent (embedded GLB wheel nodes are allowed)")
}

& (Join-Path $ToolsRoot "GenerateLuaApiManifest.ps1") -Root $Root

$manifestPath = Join-Path $reportRoot "LuaAPI.json"
Check (Test-Path $manifestPath) "generated LuaAPI.json"
if (Test-Path $manifestPath) {
    $manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
    Check ($manifest.binding_count -gt 150) "Lua API manifest contains a substantial binding set"
    $qualified = @($manifest.bindings | ForEach-Object qualified_name)
    Check (($qualified | Select-Object -Unique).Count -eq $qualified.Count) "Lua API names are unique"
    foreach ($name in @(
        "Engine.GetBuildIdentity",
        "Engine.DumpLuaAPI",
        "Engine.RunSafetySmokeTests",
        "Vehicle.SetInputs",
        "Vehicle.SetTireModel",
        "Vehicle.SetWheelTireModel",
        "Vehicle.GetWheelTireModel",
        "Vehicle.SetWheelSuspensionModel",
        "Vehicle.GetWheelSuspensionModel",
        "Vehicle.SetWheelSuspensionGeometry",
        "Vehicle.GetWheelSuspensionGeometry",
        "Vehicle.EstimateMacPhersonHardpoints",
        "Vehicle.EstimateTrailingArmHardpoints",
        "Vehicle.SetWheelSuspensionHardpoints",
        "Vehicle.SetChassisTorsionalCompliance",
        "Vehicle.EstimateChassisFlex",
        "Vehicle.GetChassisFlexState",
        "Vehicle.EstimateMassProperties",
        "Vehicle.SetWheelFitment",
        "Vehicle.GetWheelFitment",
        "Vehicle.GetWheelFitmentGeometry",
        "Vehicle.SetWheelAlignment",
        "Vehicle.GetWheelAlignment",
        "Vehicle.GetWheelUprightPose",
        "Vehicle.SetWheelUnsprungMassModel",
        "Vehicle.GetWheelUnsprungMassModel",
        "Vehicle.StartDynamicsLab",
        "Vehicle.GetDynamicsLabSummary",
        "Vehicle.GetDynamicsLabSeries",
        "Vehicle.ExportDynamicsLabCsv",
        "Vehicle.CompileDefinitionV2",
        "Vehicle.CreateFromDefinitionV2",
        "UI.InputText",
        "UI.GetAvailableWidth",
        "UI.PlotLines",
        "Module.AssetExists",
        "Module.SelectAssetFile",
        "Module.WriteSaveText",
        "Physics.DestroyBody",
        "Physics.GetBodyInertiaLocal",
        "Physics.SetBodyInertiaLocal",
        "Physics.ClearBodyInertiaLocalOverride",
        "Physics.IsBodyInertiaLocalOverridden",
        "Physics.SetColliderSurface",
        "Physics.GetColliderSurface",
        "Physics.LoadStaticBoxScene",
        "Physics.UnloadStaticBoxScene",
        "Physics.GetStaticBoxSceneCount",
        "Physics.LoadStaticTriangleScene",
        "Physics.UnloadStaticTriangleScene",
        "Physics.GetStaticTriangleSceneCount",
        "Vehicle.GetWheelState",
        "Vehicle.GetWheelTelemetry",
        "Vehicle.GetWheelContactDiagnostic",
        "Vegetation.IsAvailable",
        "Vegetation.RegisterSpecies",
        "Vegetation.AddInstance",
        "Vegetation.GetStats",
        "Vegetation.GetRepresentation",
        "Entity.Destroy"
    )) {
        Check ($qualified -contains $name) "Lua API contains $name"
    }
}

$runtimeCppPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp"
$runtimeCpp = if (Test-Path $runtimeCppPath) { [IO.File]::ReadAllText($runtimeCppPath) } else { "" }
$luaBindingRoot = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings"
$luaBindingCppFiles = if (Test-Path $luaBindingRoot) {
    @(Get-ChildItem -Path $luaBindingRoot -Recurse -File -Filter "*.cpp" | Sort-Object FullName)
} else {
    @()
}
$luaBindingCpp = ($luaBindingCppFiles | ForEach-Object { [IO.File]::ReadAllText($_.FullName) }) -join "`n"
$luaRuntimeAndBindingsCpp = $runtimeCpp + "`n" + $luaBindingCpp

Check ($runtimeCpp.Contains("m_registeredLuaFunctions")) "runtime records exact registered Lua names"
Check ($runtimeCpp.Contains("runSafetySmokeTests")) "runtime contains lifetime safety smoke tests"
Check ($runtimeCpp.Contains("LuaAPI_Runtime.json")) "runtime writes a live API manifest"
$wheelTelemetryCppPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleTelemetryBindings.cpp"
$wheelTelemetryCpp = if (Test-Path $wheelTelemetryCppPath) { [IO.File]::ReadAllText($wheelTelemetryCppPath) } else { "" }
$wheelStateHasUnsprungTelemetry =
    $wheelTelemetryCpp.Contains("value.unsprungVelocity") -and
    $wheelTelemetryCpp.Contains("value.tireDeflection") -and
    $wheelTelemetryCpp.Contains("value.tireDeflectionVelocity") -and
    $wheelTelemetryCpp.Contains("value.tireRadialDissipationWatts")
Check $wheelStateHasUnsprungTelemetry "wheel-state Lua bridge includes unsprung-mass telemetry"

# Do not use a historical return-count literal as a proxy for telemetry presence.
# Tire milestones intentionally extend GetWheelState as diagnostics grow. Keep the
# success/failure ABI count synchronized while validating milestone fields themselves
# so future telemetry additions fail for the right reason.
$wheelStateBindingMatch = [regex]::Match(
    $wheelTelemetryCpp,
    # Scope this check to GetWheelState itself rather than assuming which
    # telemetry binding happens to follow it in the translation unit. CLEAN01
    # inserted GetWheelTelemetry between the legacy positional ABI and the
    # diagnostic bindings; tying the terminator to a specific next function
    # made the validator accidentally count returns from both functions.
    'int LuaVehicleBindingHandlers::luaVehicleGetWheelState\(lua_State\* state\)(?<body>.*?)(?=\s*int LuaVehicleBindingHandlers::)',
    [Text.RegularExpressions.RegexOptions]::Singleline)
$wheelStateBinding = if ($wheelStateBindingMatch.Success) { $wheelStateBindingMatch.Groups['body'].Value } else { "" }
$wheelStateHasTire03Telemetry =
    $wheelStateBinding.Contains("value.turnSlipPerM") -and
    $wheelStateBinding.Contains("value.normalizedTurnSlip") -and
    $wheelStateBinding.Contains("value.contactPatchTwistDegrees") -and
    $wheelStateBinding.Contains("value.parkingTurnMoment") -and
    $wheelStateBinding.Contains("value.turnSlipMoment")
Check $wheelStateHasTire03Telemetry "wheel-state Lua bridge includes TIRE03 turn-slip telemetry"
$wheelStateHasTire04Telemetry =
    $wheelStateBinding.Contains("value.tireFreeRollingRadius") -and
    $wheelStateBinding.Contains("value.tireLoadedRadius") -and
    $wheelStateBinding.Contains("value.tireEffectiveRollingRadius") -and
    $wheelStateBinding.Contains("value.tireContactPatchLength") -and
    $wheelStateBinding.Contains("value.tireContactPatchWidth") -and
    $wheelStateBinding.Contains("value.tireContactPatchArea")
Check $wheelStateHasTire04Telemetry "wheel-state Lua bridge includes TIRE04 contact-geometry telemetry"
$wheelStateHasTire05Telemetry =
    $wheelStateBinding.Contains("value.tireEnvelopeRoadOffset") -and
    $wheelStateBinding.Contains("value.tireEnvelopeSlopeDegrees") -and
    $wheelStateBinding.Contains("value.tireEnvelopeValidSamples") -and
    $wheelStateBinding.Contains("value.tireRingRadialOffset") -and
    $wheelStateBinding.Contains("value.tireRingLongitudinalVelocity") -and
    $wheelStateBinding.Contains("value.tireRingLateralVelocity")
Check $wheelStateHasTire05Telemetry "wheel-state Lua bridge includes TIRE05 rigid-ring/enveloping telemetry"
$wheelStateHasTire06Telemetry =
    $wheelStateBinding.Contains("value.tireEnvelopeCrossSlopeDegrees") -and
    $wheelStateBinding.Contains("value.tireFootprintTotalSamples") -and
    $wheelStateBinding.Contains("value.tireFootprintSupportedFraction") -and
    $wheelStateBinding.Contains("value.tireFootprintRoughnessRange") -and
    $wheelStateBinding.Contains("value.tireFootprintSurfaceFriction") -and
    $wheelStateBinding.Contains("value.tireFootprintSurfaceSpread") -and
    $wheelStateBinding.Contains("value.tireFootprintRefined") -and
    $wheelStateBinding.Contains("value.tireRingYawDegrees") -and
    $wheelStateBinding.Contains("value.tireRingWindupDegrees")
Check $wheelStateHasTire06Telemetry "wheel-state Lua bridge includes TIRE06 adaptive-footprint/ring-rotation telemetry"
$wheelStateHasTire07Telemetry =
    $wheelStateBinding.Contains("value.tireTreadTemperatureC") -and
    $wheelStateBinding.Contains("value.tireCarcassTemperatureC") -and
    $wheelStateBinding.Contains("value.tireGasTemperatureC") -and
    $wheelStateBinding.Contains("value.tireInflationPressurePa") -and
    $wheelStateBinding.Contains("value.tireThermalFrictionScale") -and
    $wheelStateBinding.Contains("value.tireThermalStiffnessScale") -and
    $wheelStateBinding.Contains("value.tireSlipDissipationWatts") -and
    $wheelStateBinding.Contains("value.tireThermalLossDissipationWatts") -and
    $wheelStateBinding.Contains("value.tireRoadHeatFlowWatts") -and
    $wheelStateBinding.Contains("value.tireAirHeatFlowWatts")
Check $wheelStateHasTire07Telemetry "wheel-state Lua bridge includes TIRE07 thermal/pressure/energy telemetry"
$wheelStateHasTire08Telemetry =
    $wheelStateBinding.Contains("value.tireTreadInsideSurfaceTemperatureC") -and
    $wheelStateBinding.Contains("value.tireTreadCenterSurfaceTemperatureC") -and
    $wheelStateBinding.Contains("value.tireTreadOutsideSurfaceTemperatureC") -and
    $wheelStateBinding.Contains("value.tireTreadMinimumDepthMm") -and
    $wheelStateBinding.Contains("value.tireTreadWearFraction") -and
    $wheelStateBinding.Contains("value.tireFlatSpotDepthMm") -and
    $wheelStateBinding.Contains("value.tireSpatialFrictionScale")
Check $wheelStateHasTire08Telemetry "wheel-state Lua bridge includes TIRE08 spatial tread temperature/wear telemetry"
$wheelStateHasTire09VisualTelemetry =
    $wheelStateBinding.Contains("value.tireFlatSpotSector")
Check $wheelStateHasTire09VisualTelemetry "wheel-state Lua bridge exposes TIRE09 material-fixed flat-spot sector for visual deformation"
$wheelStateHasTire10Telemetry =
    $wheelStateBinding.Contains("value.tireAverageTreadRadiusLossMm") -and
    $wheelStateBinding.Contains("value.tireContactTreadRadiusLossMm") -and
    $wheelStateBinding.Contains("value.tireContactRadiusVariationMm") -and
    $wheelStateBinding.Contains("value.contactNormal.x") -and
    $wheelStateBinding.Contains("value.contactNormal.y") -and
    $wheelStateBinding.Contains("value.contactNormal.z")
Check $wheelStateHasTire10Telemetry "wheel-state Lua bridge includes TIRE10 physical tread-radius and contact-normal telemetry"
$wheelStateHasTire11Telemetry =
    $wheelStateBinding.Contains("value.tireContaminationFrictionScale") -and
    $wheelStateBinding.Contains("value.tireContaminationTotal") -and
    $wheelStateBinding.Contains("value.tireContaminationAverage") -and
    $wheelStateBinding.Contains("value.tireOrganicContamination") -and
    $wheelStateBinding.Contains("value.tireMineralContamination") -and
    $wheelStateBinding.Contains("value.tireGravelFinesContamination") -and
    $wheelStateBinding.Contains("value.tireRubberPickupContamination") -and
    $wheelStateBinding.Contains("value.tireMudFilmContamination") -and
    $wheelStateBinding.Contains("value.tireContaminationCleaningRate")
Check $wheelStateHasTire11Telemetry "wheel-state Lua bridge includes TIRE11 tread-contamination/cleaning telemetry"
$wheelStateHasTire12Telemetry =
    $wheelStateBinding.Contains("value.tireRoadWaterDepthMm") -and
    $wheelStateBinding.Contains("value.tireRetainedWaterDepthMm") -and
    $wheelStateBinding.Contains("value.tireDrainageDemandRatio") -and
    $wheelStateBinding.Contains("value.tireWaterWedgeFraction") -and
    $wheelStateBinding.Contains("value.tireHydroplaningFraction") -and
    $wheelStateBinding.Contains("value.tirePavementContactFraction") -and
    $wheelStateBinding.Contains("value.tireHydrodynamicLiftN") -and
    $wheelStateBinding.Contains("value.tireHydrodynamicDragN") -and
    $wheelStateBinding.Contains("value.tireWetFrictionScale") -and
    $wheelStateBinding.Contains("value.tireClassicalHydroplaningSpeedKph")
Check $wheelStateHasTire12Telemetry "wheel-state Lua bridge includes TIRE12 water-film/drainage/hydroplaning telemetry"
$wheelStateHasTire13Telemetry =
    $wheelStateBinding.Contains("value.tireWinterSurfaceFraction") -and
    $wheelStateBinding.Contains("value.tireSnowSurfaceFraction") -and
    $wheelStateBinding.Contains("value.tireIceSurfaceFraction") -and
    $wheelStateBinding.Contains("value.tireWinterFrictionScale") -and
    $wheelStateBinding.Contains("value.tireWinterStiffnessScale") -and
    $wheelStateBinding.Contains("value.tirePackedSnowFraction") -and
    $wheelStateBinding.Contains("value.tireIceMeltFilmMicrometers") -and
    $wheelStateBinding.Contains("value.tireStudFrictionContribution") -and
    $wheelStateBinding.Contains("value.tireSnowInterlockContribution") -and
    $wheelStateBinding.Contains("value.tireWinterSurfaceTemperatureC")
Check $wheelStateHasTire13Telemetry "wheel-state Lua bridge includes TIRE13 compacted-snow/hard-ice telemetry"
$wheelStateHasTire14Telemetry =
    $wheelStateBinding.Contains("value.tireGranularSurfaceFraction") -and
    $wheelStateBinding.Contains("value.tireGranularSinkageMm") -and
    $wheelStateBinding.Contains("value.tireGranularContactPressureKPa") -and
    $wheelStateBinding.Contains("value.tireGranularTreadEffectiveness") -and
    $wheelStateBinding.Contains("value.tireGranularShearCapacityN") -and
    $wheelStateBinding.Contains("value.tireGranularLongitudinalShearN") -and
    $wheelStateBinding.Contains("value.tireGranularLateralShearN") -and
    $wheelStateBinding.Contains("value.tireGranularBulldozingN") -and
    $wheelStateBinding.Contains("value.tireGranularPlowingDragN") -and
    $wheelStateBinding.Contains("value.tireGranularCompactionPowerW") -and
    $wheelStateBinding.Contains("value.tireGranularFrictionScale")
Check $wheelStateHasTire14Telemetry "wheel-state Lua bridge includes TIRE14 shallow-granular gravel/dirt telemetry"
$wheelStateHasTire15Telemetry =
    $wheelStateBinding.Contains("value.tireTerrainSurfaceFraction") -and
    $wheelStateBinding.Contains("value.tireTerrainSinkageMm") -and
    $wheelStateBinding.Contains("value.tireTerrainRutDepthMm") -and
    $wheelStateBinding.Contains("value.tireTerrainCompaction") -and
    $wheelStateBinding.Contains("value.tireTerrainMoisture") -and
    $wheelStateBinding.Contains("value.tireTerrainLooseDepthMm") -and
    $wheelStateBinding.Contains("value.tireTerrainShearCapacityN") -and
    $wheelStateBinding.Contains("value.tireTerrainLongitudinalShearN") -and
    $wheelStateBinding.Contains("value.tireTerrainLateralShearN") -and
    $wheelStateBinding.Contains("value.tireTerrainBulldozingN") -and
    $wheelStateBinding.Contains("value.tireTerrainPlowingDragN") -and
    $wheelStateBinding.Contains("value.tireTerrainMfFrictionScale") -and
    $wheelStateBinding.Contains("value.tireTerrainPassCount")
Check $wheelStateHasTire15Telemetry "wheel-state Lua bridge includes TIRE15 persistent deformable-terrain telemetry"
$wheelStateFailureCountMatch = [regex]::Match($wheelStateBinding, 'index < (?<count>\d+)')
$wheelStateReturnCounts = @([regex]::Matches($wheelStateBinding, 'return\s+(?<count>\d+)\s*;') | ForEach-Object { [int]$_.Groups['count'].Value } | Where-Object { $_ -gt 0 })
$wheelStateFailureCount = if ($wheelStateFailureCountMatch.Success) { [int]$wheelStateFailureCountMatch.Groups['count'].Value } else { -1 }
$wheelStateAbiSynchronized =
    $wheelStateFailureCount -gt 0 -and
    $wheelStateReturnCounts.Count -ge 2 -and
    (@($wheelStateReturnCounts | Where-Object { $_ -ne $wheelStateFailureCount }).Count -eq 0)
Check $wheelStateAbiSynchronized "Vehicle.GetWheelState nil/success return counts remain synchronized"
$wheelStateReservesLuaStack = $wheelStateFailureCount -gt 0 -and [regex]::IsMatch(
    $wheelStateBinding,
    ('lua_checkstack\(\s*state\s*,\s*' + [regex]::Escape([string]$wheelStateFailureCount) + '\s*\)'))
Check $wheelStateReservesLuaStack "Vehicle.GetWheelState reserves Lua C-stack capacity for its full telemetry return payload"
Check ($luaRuntimeAndBindingsCpp.Contains("luaVehicleGetWheelTelemetry")) "Lua registration table exposes named wheel telemetry"
$wheelTelemetryHasNamedTable =
    $wheelTelemetryCpp.Contains("luaVehicleGetWheelTelemetry") -and
    [regex]::IsMatch($wheelTelemetryCpp, 'lua_createtable\(\s*state\s*,\s*0\s*,\s*\d+\s*\)') -and
    $wheelTelemetryCpp.Contains('pushNumberField("surfaceTemperatureC"') -and
    $wheelTelemetryCpp.Contains('pushNumberField("tireTerrainPassCount"') -and
    $wheelTelemetryCpp.Contains('pushNumberField("tireTrackDepositedRubber"') -and
    $wheelTelemetryCpp.Contains('pushNumberField("tireTrackLooseRubber"') -and
    $wheelTelemetryCpp.Contains('pushNumberField("tireTrackMarbleMaturity"') -and
    $wheelTelemetryCpp.Contains('pushStringField("contactStatus"') -and
    $wheelTelemetryCpp.Contains('pushNumberField("uprightRotationX"')
Check $wheelTelemetryHasNamedTable "named wheel telemetry returns tire, contact-diagnostic and upright state in one table"
$tire26RemovedCarcassTelemetryAbsent =
    -not $wheelTelemetryCpp.Contains("tireCarcass3DContactValid") -and
    -not $wheelTelemetryCpp.Contains("tireCarcass3DContactCount") -and
    -not $wheelTelemetryCpp.Contains("tireCarcass3DTotalNormalForceN") -and
    -not $wheelTelemetryCpp.Contains("tireCarcass3DMaximumCompressionM")
Check $tire26RemovedCarcassTelemetryAbsent "TIRE26 visual branch telemetry does not reference removed TIRE18-TIRE21 carcass WheelState fields"
Check ($luaRuntimeAndBindingsCpp.Contains("luaVehicleGetWheelContactDiagnostic")) "Lua registration table exposes wheel contact-loss diagnostics"
Check ($luaBindingCpp.Contains("return 25;")) "Dynamics Lab summary includes geometry extrema"
Check ($luaBindingCpp.Contains("OFN_FILEMUSTEXIST")) "Windows module asset picker requires an existing file"
Check ($luaBindingCpp.Contains("resolveSavePath(relative)")) "bounded text export resolves through the active module save root"

