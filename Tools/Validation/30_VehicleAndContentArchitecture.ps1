# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

# ARCH04 / BASE02 / OPT01: future vehicle mechanisms are architecture intent,
# not fake translation units. The project group now contains only real tracked
# implementation files; planned seams live in the manifest until implemented.
$vehicleArchitectureGroup = [regex]::Match(
    $vehicleProject,
    '<ItemGroup Label="VehicleArchitecture">(?<body>.*?)</ItemGroup>',
    [Text.RegularExpressions.RegexOptions]::Singleline)
Check $vehicleArchitectureGroup.Success "Visual Studio project exposes the real vehicle architecture implementation group"
$vehicleArchitectureBody = if ($vehicleArchitectureGroup.Success) { $vehicleArchitectureGroup.Groups['body'].Value } else { "" }
Check (-not $vehicleArchitectureBody.Contains('<None Include="..\Vehicles\')) "OPT01 vehicle architecture project group contains no fake source placeholders"
Check ($vehicleArchitectureBody.Contains('<ClCompile Include="..\Vehicles\Tires\TireContactPatch.cpp"') -and $vehicleArchitectureBody.Contains('<ClCompile Include="..\Vehicles\Tires\TireWear.cpp"')) "OPT01 vehicle architecture group tracks real compiled mechanisms"
$opt01VehicleManifestPath30 = Join-Path $Root "Docs\VEHICLE_SUBSYSTEM_ARCHITECTURE_MANIFEST.md"
$opt01VehicleManifest30 = if (Test-Path $opt01VehicleManifestPath30) { [IO.File]::ReadAllText($opt01VehicleManifestPath30) } else { "" }
Check ($opt01VehicleManifest30.Contains("Double wishbone") -and $opt01VehicleManifest30.Contains("Aerodynamics system") -and $opt01VehicleManifest30.Contains("Create source only when implementation exists")) "OPT01 future vehicle mechanisms remain explicitly documented without empty source files"
Check (-not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Vehicles\AerodynamicsSystem.cpp"))) "obsolete root-level AerodynamicsSystem scaffold is absent"
Check (-not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Vehicles\AeroSurface.cpp"))) "obsolete root-level AeroSurface scaffold is absent"
Check (-not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Vehicles\GroundEffect.cpp"))) "obsolete root-level GroundEffect scaffold is absent"

# FLEX01: chassis flex has graduated from scaffold to a compiled reusable vehicle mechanism.
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\ChassisFlex\ChassisTorsionalCompliance.cpp"')) "engine compiles chassis torsional compliance"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\ChassisFlex\ChassisFlexEstimator.cpp"')) "engine compiles chassis-flex estimator"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\ChassisFlex\ChassisFlexDiagnostics.cpp"')) "engine compiles chassis-flex diagnostics"
Check ($vehicleProject.Contains('<ClInclude Include="..\Vehicles\Dynamics\ChassisFlex\ChassisFlexDiagnostics.hpp"')) "engine tracks chassis-flex diagnostics contract"

# MASS01: physical mass properties are explicit and independent from collision proxies.
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesEstimator.cpp"')) "engine compiles vehicle mass-property estimator"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesAccumulator.cpp"')) "engine compiles vehicle mass-property accumulator"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesDiagnostics.cpp"')) "engine compiles mass-property diagnostics anchor"
Check ($vehicleProject.Contains('<ClCompile Include="..\Core\Modules\LuaBindings\Vehicle\LuaVehicleMassBindings.cpp"')) "engine compiles dedicated vehicle mass Lua bindings"
$luaAnnotationsPath = Join-Path $Root "Docs\LuaApiAnnotations.json"
$luaAnnotations = if (Test-Path $luaAnnotationsPath) { [IO.File]::ReadAllText($luaAnnotationsPath) } else { "" }
Check ($luaAnnotations.Contains('"Physics.GetBodyInertiaLocal"')) "MASS01 inertia readback Lua API is annotated"
Check ($luaAnnotations.Contains('"Physics.SetBodyInertiaLocal"')) "MASS01 inertia authoring Lua API is annotated"
Check ($luaAnnotations.Contains('"Physics.ClearBodyInertiaLocalOverride"')) "MASS01 inertia override-clear Lua API is annotated"
Check ($luaAnnotations.Contains('"Physics.IsBodyInertiaLocalOverridden"')) "MASS01 inertia override-state Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.EstimateMassProperties"')) "MASS01 estimator Lua API is annotated"

# FITMENT01: reference assembly remains immutable; installed fitment/alignment is
# per-corner setup layered downstream of suspension/steering reference geometry.
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Wheels\Fitment\WheelFitment.cpp"')) "engine compiles wheel-fitment geometry"
Check ($vehicleProject.Contains('<ClCompile Include="..\Core\Modules\LuaBindings\Vehicle\LuaVehicleFitmentBindings.cpp"')) "engine compiles dedicated vehicle fitment Lua bindings"
Check ($luaAnnotations.Contains('"Vehicle.SetWheelFitment"')) "FITMENT01 fitment setter Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.GetWheelFitment"')) "FITMENT01 fitment readback Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.SetWheelAlignment"')) "FITMENT01 alignment setter Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.GetWheelAlignment"')) "FITMENT01 alignment readback Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.GetWheelFitmentGeometry"')) "FITMENT02 hub/scrub geometry Lua API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.GetWheelState"') -and $luaAnnotations.Contains('inspect Vehicles/Telemetry.lua') -and $luaAnnotations.Contains('TIRE15')) "legacy positional wheel telemetry remains annotated through TIRE15"
Check ($luaAnnotations.Contains('"Vehicle.GetWheelTelemetry"') -and $luaAnnotations.Contains('"returns": "telemetry: table|nil"')) "preferred named wheel telemetry API is annotated"
Check ($luaAnnotations.Contains('"Vehicle.MeasureWheelGeometry"') -and $luaAnnotations.Contains('frontTrackM')) "live four-corner wheelbase/track measurement API is annotated"
Check ($luaAnnotations.Contains('"Entity.SetMeshNodeTireFlexibleRingFromWheel"')) "TIRE41 single-authority flexible-ring Entity API is annotated"
Check ($luaAnnotations.Contains('"UI.InputFloat"')) "ALIGN01 exact floating-point UI input API is annotated"

$wheelFitmentHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Wheels\Fitment\WheelFitment.hpp"
$wheelFitmentCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Wheels\Fitment\WheelFitment.cpp"
$wheelFitmentHeader = if (Test-Path $wheelFitmentHeaderPath) { [IO.File]::ReadAllText($wheelFitmentHeaderPath) } else { "" }
$wheelFitmentCpp = if (Test-Path $wheelFitmentCppPath) { [IO.File]::ReadAllText($wheelFitmentCppPath) } else { "" }
Check ($wheelFitmentHeader.Contains("WheelFitmentDescription")) "fitment owns explicit reference/installed wheel dimensions"
Check ($wheelFitmentHeader.Contains("WheelAlignmentSetup")) "fitment owns explicit per-corner alignment setup"
Check ($wheelFitmentCpp.Contains("wheelCenterlineOffsetLocal")) "fitment computes installed wheel centerline displacement downstream of upright"
Check ($vehicleCpp.Contains("wheelRayOriginWorld")) "vehicle contact query uses installed wheel centerline independently from suspension mount"
Check ($vehicleCpp.Contains("const heritage::math::Vec3 mountWorld")) "vehicle retains an explicit reference suspension mount"
Check ($vehicleCpp.Contains("description.localMount")) "steering/Ackermann reference geometry still consumes authored wheel mounts"

# FITMENT02: explicit wheel/hub datums and live steering-ground geometry graduate
# from scaffolds into compiled reusable mechanisms.
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Wheels\Fitment\HubReferenceGeometry.cpp"')) "engine compiles explicit hub-reference geometry"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.cpp"')) "engine compiles scrub-radius / trail geometry"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Wheels\Fitment\HubReferenceGeometry.cpp"')) "hub-reference geometry is no longer an unimplemented scaffold"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.cpp"')) "scrub-radius geometry is no longer an unimplemented scaffold"
Check ($physicsTestsProject.Contains('..\Vehicles\Wheels\Fitment\HubReferenceGeometry.cpp')) "physics regressions compile hub-reference geometry"
Check ($physicsTestsProject.Contains('..\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.cpp')) "physics regressions compile steering-ground geometry"
$hubGeometryPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Wheels\Fitment\HubReferenceGeometry.cpp"
$scrubGeometryPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Wheels\Fitment\ScrubRadiusGeometry.cpp"
$hubGeometry = if (Test-Path $hubGeometryPath) { [IO.File]::ReadAllText($hubGeometryPath) } else { "" }
$scrubGeometry = if (Test-Path $scrubGeometryPath) { [IO.File]::ReadAllText($scrubGeometryPath) } else { "" }
Check ($hubGeometry.Contains("referenceHubFaceCenterLocal")) "FITMENT02 distinguishes reference hub face from wheel centerline"
Check ($hubGeometry.Contains("installedMountFaceCenterLocal")) "FITMENT02 resolves spacer-shifted installed mounting face"
Check ($hubGeometry.Contains("installedInnerTirePlaneLocal") -and $hubGeometry.Contains("installedOuterTirePlaneLocal")) "FITMENT02 resolves nominal inboard/outboard tire envelope"
Check ($scrubGeometry.Contains("steeringAxisGroundPointWorld")) "FITMENT02 intersects steering axis with live road contact plane"
Check ($scrubGeometry.Contains("signedScrubRadiusM") -and $scrubGeometry.Contains("mechanicalTrailM")) "FITMENT02 reports scrub radius and mechanical trail"
Check ($vehicleCpp.Contains("evaluateSteeringGroundGeometry")) "vehicle high-rate state evaluates live steering-ground geometry"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Vehicle", "GetWheelFitmentGeometry"')) "Lua runtime registers FITMENT02 geometry readback"

$fitmentLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Fitment.lua"
$fitmentPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\FitmentPanel.lua"
$fitmentLua = if (Test-Path $fitmentLuaPath) { [IO.File]::ReadAllText($fitmentLuaPath) } else { "" }
$fitmentPanel = if (Test-Path $fitmentPanelPath) { [IO.File]::ReadAllText($fitmentPanelPath) } else { "" }
$alignmentSpecPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\Peugeot206RC\AlignmentSpecification.lua"
$alignmentSpec = if (Test-Path $alignmentSpecPath) { [IO.File]::ReadAllText($alignmentSpecPath) } else { "" }
$luaUiBindingsPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaUiBindings.cpp"
$luaUiBindings = if (Test-Path $luaUiBindingsPath) { [IO.File]::ReadAllText($luaUiBindingsPath) } else { "" }
Check ($fitmentLua.Contains("ApplyVehicleFitmentSetup")) "Racing United applies fitment/alignment as a setup layer"
Check ($fitmentLua.Contains("vehicle.setup.fitment.link_front")) "fitment saves optional front left/right linking"
Check ($fitmentLua.Contains("vehicle.setup.fitment.link_rear")) "fitment saves optional rear left/right linking"
Check ($fitmentLua.Contains("referenceCasterDegrees")) "fitment preserves reference caster separately from setup override"
Check ($fitmentLua.Contains('corner.referenceProvenance = "glb_custom_properties"')) "fitment can promote validated GLB wheel/tire metadata as reference evidence"
Check ($fitmentPanel.Contains("Link front L/R")) "fitment UI exposes front symmetric linking"
Check ($fitmentPanel.Contains("Link rear L/R")) "fitment UI exposes rear symmetric linking"
Check ($fitmentPanel.Contains("Disable linking for asymmetric oval/race setups")) "fitment UI explicitly supports independent oval/race alignment"
Check ($alignmentSpec.Contains("front = {") -and $alignmentSpec.Contains("rear = {")) "ALIGN01 stores Peugeot front/rear factory alignment ranges in a dedicated evidence file"
Check ($alignmentSpec.Contains("minimum = -0.50, maximum = 0.50")) "ALIGN01 preserves positive and negative front camber factory range"
Check ($alignmentSpec.Contains("minimum = -1.50, maximum = -0.50")) "ALIGN01 preserves rear camber factory range"
Check ($alignmentSpec.Contains("minimum = 2.70, maximum = 3.70")) "ALIGN01 preserves caster factory range"
Check ($alignmentSpec.Contains("minimum = 9.20, maximum = 10.20")) "ALIGN01 preserves SAI factory range"
Check ($fitmentPanel.Contains("UI.InputFloat")) "fitment UI exposes a visible exact numeric alignment editor"
Check ($fitmentPanel.Contains("0.01 deg")) "fitment UI documents 0.01 degree slider increments"
Check ($fitmentPanel.Contains("Advanced alignment slider range")) "fitment UI exposes broad historical/race alignment ranges"
Check ($fitmentPanel.Contains("CUSTOM / OUTSIDE SPEC")) "factory specification is advisory rather than a setup clamp"
Check ($fitmentLua.Contains("reference_data_version")) "fitment migrates untouched provisional defaults without overwriting custom saves"
Check ($luaUiBindings.Contains("luaUiInputFloat")) "native UI binding implements exact floating-point entry"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("UI", "InputFloat", &LuaCoreBindingHandlers::luaUiInputFloat)')) "Lua runtime registers exact floating-point UI input"
Check (-not $fitmentLua.Contains("SetWheelSuspensionHardpoints")) "fitment setup cannot rewrite suspension hardpoints"
Check ($fitmentLua.Contains("RefreshVehicleFitmentGeometry")) "Racing United reads live FITMENT02 geometry without rewriting setup"
Check ($fitmentPanel.Contains("Scrub radius")) "fitment UI exposes live scrub-radius diagnostics"
Check ($fitmentPanel.Contains("Mechanical trail")) "fitment UI exposes live mechanical-trail diagnostics"

# MASS01C: Vehicle.EstimateMassProperties returns numeric three-element Lua
# arrays. Heritage dynamically loads Lua, so every C API function used by a
# binding must exist in the LuaApi contract and be explicitly resolved.
$luaApiHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaApi.hpp"
$luaApiCppPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaApi.cpp"
$luaMassBindingsPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleMassBindings.cpp"
$luaApiHeader = if (Test-Path $luaApiHeaderPath) { [IO.File]::ReadAllText($luaApiHeaderPath) } else { "" }
$luaApiCpp = if (Test-Path $luaApiCppPath) { [IO.File]::ReadAllText($luaApiCppPath) } else { "" }
$luaMassBindings = if (Test-Path $luaMassBindingsPath) { [IO.File]::ReadAllText($luaMassBindingsPath) } else { "" }
Check ($luaMassBindings.Contains("lua_rawseti")) "MASS01 estimator emits numeric vector arrays"
Check ($luaApiHeader.Contains("lua_rawseti")) "LuaApi contract exposes lua_rawseti used by mass bindings"
Check ($luaApiCpp.Contains("HERITAGE_RESOLVE_LUA(lua_rawseti)")) "LuaApi dynamically resolves lua_rawseti"
Check ($luaApiCpp.Contains("lua_rawseti = nullptr")) "LuaApi clears lua_rawseti on unload"

$mainLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Main.lua"
if (Test-Path $mainLuaPath) {
    $mainLua = [IO.File]::ReadAllText($mainLuaPath)
    $lineCount = (Get-Content $mainLuaPath).Count
    Check ($lineCount -le 80) "Main.lua remains a small include coordinator"
    Check ($mainLua.Contains('Include("UI/Physics/Panels.lua")')) "Main.lua delegates physics UI loading to its subsystem coordinator"
    Check ($mainLua.Contains('Include("UI/Scene/Panels.lua")')) "Main.lua delegates scene weather/hydrology UI loading to its subsystem coordinator"
    Check (-not $mainLua.Contains("Runtime/VehicleDemo.lua")) "obsolete VehicleDemo.lua is not included"
    Check ($mainLua.Contains('Include("Vehicles/Definitions/Peugeot206RC/AlignmentSpecification.lua")')) "Racing United includes Peugeot 206 RC alignment evidence before prototype definition"
    $includeMatches = [regex]::Matches($mainLua, 'Include\("(?<path>[^"]+\.lua)"\)')
    foreach ($match in $includeMatches) {
        $relative = $match.Groups['path'].Value.Replace('/', [IO.Path]::DirectorySeparatorChar)
        Check (Test-Path (Join-Path (Split-Path $mainLuaPath) $relative)) "Main.lua include exists: $relative"
    }
    $definitionCoreIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2.lua")')
    $definitionBuilderIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2Builder.lua")')
    $definitionValidationIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2Validation.lua")')
    $definitionDynamicsValidationIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2DynamicsValidation.lua")')
    $definitionCompatibilityIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2Compatibility.lua")')
    $definitionSerializationIndex = $mainLua.IndexOf('Include("Vehicles/Definitions/VehicleDefinitionV2Serialization.lua")')
    Check ($definitionCoreIndex -ge 0 -and $definitionCoreIndex -lt $definitionBuilderIndex -and $definitionBuilderIndex -lt $definitionValidationIndex -and $definitionValidationIndex -lt $definitionDynamicsValidationIndex -and $definitionDynamicsValidationIndex -lt $definitionCompatibilityIndex -and $definitionCompatibilityIndex -lt $definitionSerializationIndex) "VehicleDefinitionV2 responsibility files load in schema-builder-core-dynamics-compatibility-serialization order"
}
$physicsPanelsCoordinatorPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Physics\Panels.lua"
$scenePanelsCoordinatorPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Scene\Panels.lua"
$physicsPanelsCoordinator = if (Test-Path $physicsPanelsCoordinatorPath) { [IO.File]::ReadAllText($physicsPanelsCoordinatorPath) } else { "" }
$scenePanelsCoordinator = if (Test-Path $scenePanelsCoordinatorPath) { [IO.File]::ReadAllText($scenePanelsCoordinatorPath) } else { "" }
Check (
    $physicsPanelsCoordinator.Contains('UI/Physics/WorldPanel.lua') -and
    $physicsPanelsCoordinator.Contains('UI/Physics/SuspensionPanel.lua') -and
    $physicsPanelsCoordinator.Contains('UI/Physics/QueriesPanel.lua') -and
    $physicsPanelsCoordinator.Contains('UI/Physics/BodyPanel.lua')
) "Physics UI coordinator owns the physics panel include set"
Check (
    $scenePanelsCoordinator.Contains('UI/Scene/WeatherPanel.lua') -and
    -not $scenePanelsCoordinator.Contains('WaterLaboratoryPanel.lua')
) "Scene UI coordinator owns weather without the retired Water Laboratory panel"
Check (-not (Test-Path (Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\VehicleDemo.lua"))) "obsolete Runtime/VehicleDemo.lua is absent"

# SUS03B/TIRE45D: hardpoint authoring distinguishes evidence quality. Estimated
# points remain creator scaffolding only; runtime hardpoint kinematics require a
# complete legacy-authored/asset-authored/measured set. Chassis suspension
# estimates must not be regenerated from current wheel/tire fitment.
$suspensionAuthoringRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\SuspensionAuthoring.lua"
$suspensionAuthoringPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\Suspension\AuthoringPanel.lua"
$suspensionAuthoringDocPath = Join-Path $Root "Docs\SUSPENSION_AUTHORING.md"
$suspensionAuthoringFiles = @(
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointSources.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointEstimation.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Suspension\SuspensionAuthoring.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Suspension\HardpointGizmos.lua")
)
Check (Test-Path $suspensionAuthoringRuntimePath) "suspension authoring compatibility coordinator exists"
Check (Test-Path $suspensionAuthoringPanelPath) "suspension authoring panel exists"
Check (Test-Path $suspensionAuthoringDocPath) "suspension authoring contract is documented"
$suspensionAuthoringRuntime = (($suspensionAuthoringFiles | Where-Object { Test-Path $_ } | ForEach-Object { [IO.File]::ReadAllText($_) }) -join "`n")
$suspensionAuthoringCoordinator = if (Test-Path $suspensionAuthoringRuntimePath) { [IO.File]::ReadAllText($suspensionAuthoringRuntimePath) } else { "" }
Check ($suspensionAuthoringCoordinator.Contains('Vehicles/Suspension/HardpointSources.lua') -and $suspensionAuthoringCoordinator.Contains('Vehicles/Suspension/HardpointEstimation.lua') -and $suspensionAuthoringCoordinator.Contains('Vehicles/Suspension/SuspensionAuthoring.lua') -and $suspensionAuthoringCoordinator.Contains('Vehicles/Suspension/HardpointGizmos.lua')) "CLEAN08 root suspension authoring file only coordinates responsibility-owned files"
Check ($suspensionAuthoringRuntime.Contains("EnsureSuspensionHardpointEstimates")) "Racing United can build explicit estimated hardpoints"
Check ($suspensionAuthoringRuntime.Contains("frontReferencePackageScaleM")) "front estimate uses chassis suspension-package scale"
Check ($suspensionAuthoringRuntime.Contains("rearReferencePackageScaleM")) "rear estimate uses chassis suspension-package scale"
Check (-not $suspensionAuthoringRuntime.Contains("wheel.radiusM or shared.radiusM")) "authoring runtime does not derive suspension geometry from installed tire radius"
Check ($suspensionAuthoringRuntime.Contains("EstimateTrailingArmHardpoints")) "Racing United can estimate trailing-arm rear hardpoints"
Check ($suspensionAuthoringRuntime.Contains('"estimated"')) "estimated suspension data is provenance-labeled"
Check ($suspensionAuthoringRuntime.Contains('assembly.minimumPhysicsProvenance or "legacy_authored"')) "generic estimated hardpoints cannot become runtime physics authority without a vehicle-local policy"
Check ($suspensionAuthoringRuntime.Contains('"reference_constrained_estimate"')) "source contract distinguishes explicitly authorized reference-constrained suspension estimates"
Check ($suspensionAuthoringRuntime.Contains("SuspensionAuthoringImportHardpointsFromMetadata")) "GLB hardpoints can supersede estimates"
Check ($suspensionAuthoringRuntime.Contains("Vehicle.SetWheelSuspensionHardpoints")) "authoring layer can activate native hardpoint kinematics"

$vehicleDefinitionHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDefinition.hpp"
$vehicleDefinitionHeader = if (Test-Path $vehicleDefinitionHeaderPath) { [IO.File]::ReadAllText($vehicleDefinitionHeaderPath) } else { "" }
Check ($vehicleDefinitionHeader.Contains("VehicleSuspensionHardpointDefinition")) "native VehicleDefinitionV2 stores suspension hardpoints"
Check ($vehicleDefinitionHeader.Contains("std::vector<VehicleSuspensionHardpointDefinition> hardpoints")) "suspension components own optional hardpoint arrays"
Check ($vehicleDefinitionHeader.Contains("std::string provenance")) "suspension hardpoints preserve provenance"
Check ($vehicleDefinitionHeader.Contains("float confidence")) "suspension hardpoints preserve confidence"
Check ($vehicleDefinitionHeader.Contains("VehicleAntiRollBarDefinition")) "VehicleDefinitionV2 stores independent anti-roll-bar components"
Check ($vehicleDefinitionHeader.Contains("VehicleChassisFlexDefinition")) "VehicleDefinitionV2 stores independent chassis-flex configuration"
Check ($vehicleDefinitionHeader.Contains("std::vector<VehicleAntiRollBarDefinition> antiRollBars")) "source definitions own anti-roll-bar arrays"
Check ($vehicleDefinitionHeader.Contains("hasInertiaLocalKgM2")) "VehicleDefinitionV2 can carry explicit local inertia"
Check ($vehicleDefinitionHeader.Contains("massPropertiesProvenance")) "VehicleDefinitionV2 preserves mass-property provenance"
Check ($vehicleDefinitionHeader.Contains("frontStaticLoadFraction")) "VehicleDefinitionV2 records static axle-load evidence"

$vehicleDefinitionParserPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleDefinitionParser.cpp"
$vehicleDefinitionParser = if (Test-Path $vehicleDefinitionParserPath) { [IO.File]::ReadAllText($vehicleDefinitionParserPath) } else { "" }
Check ($vehicleDefinitionParser.Contains('"hardpoints"')) "Lua VehicleDefinitionV2 bridge parses suspension hardpoints"
Check ($vehicleDefinitionParser.Contains('"provenance"')) "Lua VehicleDefinitionV2 bridge parses hardpoint provenance"
Check ($vehicleDefinitionParser.Contains('"confidence"')) "Lua VehicleDefinitionV2 bridge parses hardpoint confidence"
Check ($vehicleDefinitionParser.Contains('"antiRollBars"')) "Lua VehicleDefinitionV2 bridge parses anti-roll bars"
Check ($vehicleDefinitionParser.Contains('"chassisFlex"')) "Lua VehicleDefinitionV2 bridge parses chassis-flex configuration"
Check ($vehicleDefinitionParser.Contains('"torsionalStiffnessNmPerRad"')) "Lua VehicleDefinitionV2 bridge parses anti-roll stiffness"
Check ($vehicleDefinitionParser.Contains('"inertiaLocalKgM2"')) "Lua VehicleDefinitionV2 bridge parses explicit inertia"
Check ($vehicleDefinitionParser.Contains('"massPropertiesProvenance"')) "Lua VehicleDefinitionV2 bridge parses mass-property provenance"

$vehicleDefinitionCompilerPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.cpp"
$vehicleDefinitionCompiler = if (Test-Path $vehicleDefinitionCompilerPath) { [IO.File]::ReadAllText($vehicleDefinitionCompilerPath) } else { "" }
Check ($vehicleDefinitionCompiler.Contains("duplicate_suspension_hardpoint_id")) "native compiler rejects duplicate suspension hardpoint IDs"
Check ($vehicleDefinitionCompiler.Contains("suspension_hardpoint_position")) "native compiler validates suspension hardpoint positions"
Check ($vehicleDefinitionCompiler.Contains("suspension_hardpoint_provenance")) "native compiler validates suspension hardpoint provenance"
Check ($vehicleDefinitionCompiler.Contains("suspension_hardpoint_confidence")) "native compiler validates suspension hardpoint confidence"
Check ($vehicleDefinitionCompiler.Contains("body_inertia")) "native compiler validates explicit body inertia"
Check ($vehicleDefinitionCompiler.Contains("body_static_load_fraction")) "native compiler validates static-load evidence"
Check ($vehicleDefinitionCompiler.Contains("macpherson_required_hardpoints")) "native compiler requires the complete MacPherson hardpoint set"
Check ($vehicleDefinitionCompiler.Contains("trailing_arm_required_hardpoints")) "native compiler requires the complete trailing-arm hardpoint set"
Check ($vehicleDefinitionCompiler.Contains("anti_roll_bar_same_contact")) "native compiler rejects self-coupled anti-roll bars"
Check ($vehicleDefinitionCompiler.Contains("anti_roll_bar_parameters")) "native compiler validates anti-roll-bar parameters"
Check ($vehicleDefinitionCompiler.Contains("chassis_flex_parameters")) "native compiler validates chassis-flex parameters"
Check ($vehicleDefinitionCompiler.Contains("chassis_flex_provider")) "native compiler validates chassis-flex provider"
Check ($vehicleDefinitionCompiler.Contains('suspension.provider != "macpherson_strut_v1"')) "native road-wheel provider accepts MacPherson suspension components"
Check ($vehicleDefinitionCompiler.Contains('suspension.provider != "trailing_arm_torsion_bar_v1"')) "native road-wheel provider accepts trailing-arm torsion-bar suspension components"
Check ($vehicleDefinitionCompiler.Contains('suspension.provider != "double_wishbone_v1"')) "native road-wheel provider accepts double-wishbone suspension components"
Check ($vehicleDefinitionCompiler.Contains('suspension.provider != "pushrod_double_wishbone_v1"')) "native road-wheel provider accepts pushrod/rocker double-wishbone suspension components"

$vehicleDefinitionLua = $definitionV2
Check ($vehicleDefinitionLua.Contains("WorkshopSuspensionHardpoints")) "Workshop exports authored suspension hardpoints through VehicleDefinitionV2"
Check ($vehicleDefinitionLua.Contains("WorkshopSuspensionProvider")) "Workshop resolves suspension providers per axle instead of one vehicle-wide provider"
Check ($vehicleDefinitionLua.Contains("provenance")) "Workshop suspension hardpoints preserve provenance"
Check ($vehicleDefinitionLua.Contains("confidence")) "Workshop suspension hardpoints preserve confidence"
Check ($vehicleDefinitionLua.Contains("antiRollBars = {}")) "VehicleDefinitionV2 owns top-level anti-roll bars"
Check ($vehicleDefinitionLua.Contains("chassisFlex = { enabled = false }")) "VehicleDefinitionV2 owns top-level chassis-flex configuration"
Check ($vehicleDefinitionLua.Contains('chassisFlex.provider ~= "chassis_torsional_mode_v1"')) "VehicleDefinitionV2 validates the chassis-flex provider contract"
Check ($vehicleDefinitionLua.Contains("macpherson_strut_v1 = true") -and $vehicleDefinitionLua.Contains("double_wishbone_v1 = true") -and $vehicleDefinitionLua.Contains("trailing_arm_torsion_bar_v1 = true")) "VehicleDefinitionV2 runtime-readiness recognizes current native suspension providers"
Check ($vehicleDefinitionLua.Contains('for _, group in ipairs({ "front", "rear" }) do') -and $vehicleDefinitionLua.Contains('local bar = PrototypeCarDefinition.antiRollBars[group]')) "Racing United V2 definition emits front/rear anti-roll component groups"
Check ($vehicleDefinitionLua.Contains('id = group .. "_anti_roll_bar"')) "Racing United V2 anti-roll components receive stable group-derived IDs"

$visualDefinitionPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua"
$visualDefinition = if (Test-Path $visualDefinitionPath) { [IO.File]::ReadAllText($visualDefinitionPath) } else { "" }
Check ($visualDefinition.Contains('bodyAsset = "Vehicles/Player/PlayerCar.obj"')) "prototype definition points at the stable player-car OBJ slot"
Check ($visualDefinition.Contains("wheelbaseM = 2.442")) "prototype carries 2442 mm Peugeot reference wheelbase"
Check ($visualDefinition.Contains("frontTrackM = 1.425")) "prototype carries 1425 mm Peugeot reference front track"
Check ($visualDefinition.Contains("rearTrackM = 1.416")) "prototype carries 1416 mm Peugeot reference rear track"
Check ($visualDefinition.Contains("wheelRadiusM = 0.2979")) "prototype carries 205/40 ZR17 derived wheel radius"
Check ($visualDefinition.Contains('kinematics = "macpherson_strut"')) "Peugeot prototype declares front strut kinematics for authoring"
Check ($visualDefinition.Contains('preferredProvider = "macpherson_strut_v1"')) "Peugeot front architecture names the reusable MacPherson provider"
Check ($visualDefinition.Contains('preferredProvider = "trailing_arm_torsion_bar_v1"')) "Peugeot rear architecture names the reusable trailing-arm torsion-bar provider"
Check ([regex]::Matches($visualDefinition, 'minimumPhysicsProvenance = "reference_constrained_estimate"').Count -eq 2) "Peugeot explicitly authorizes reference-constrained estimates for both live suspension providers"
Check ($visualDefinition.Contains('rearStabilityTieRodCount = 2') -and $visualDefinition.Contains('stabilityTieRodModel = "fixed_alignment_constraint"')) "Peugeot rear stability tie rods are declared without fabricating compliance geometry"
Check ($visualDefinition.Contains("frontReferencePackageScaleM")) "prototype stores fitment-independent front suspension estimate scale"
Check ($visualDefinition.Contains("rearReferencePackageScaleM")) "prototype stores fitment-independent rear suspension estimate scale"
Check ($visualDefinition.Contains("antiRollBars")) "Peugeot prototype declares independent front/rear anti-roll bars"
Check ($visualDefinition.Contains("confidence = 0.20")) "prototype anti-roll data is explicitly low-confidence estimated data"
$macPhersonEstimatorPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.cpp"
$macPhersonEstimatorHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.hpp"
$macPhersonEstimator = if (Test-Path $macPhersonEstimatorPath) { [IO.File]::ReadAllText($macPhersonEstimatorPath) } else { "" }
$macPhersonEstimatorHeader = if (Test-Path $macPhersonEstimatorHeaderPath) { [IO.File]::ReadAllText($macPhersonEstimatorHeaderPath) } else { "" }
Check ($macPhersonEstimator.Contains("estimated_macpherson_road_v1")) "MacPherson estimator has a stable versioned profile ID"
Check ($macPhersonEstimatorHeader.Contains("referencePackageScaleM")) "MacPherson estimate scale is explicit chassis authoring data"
Check (-not $macPhersonEstimatorHeader.Contains("wheelRadiusM")) "MacPherson estimator is independent of installed wheel/tire radius"
Check ($macPhersonEstimator.Contains("result.confidence = 0.35f")) "MacPherson estimate is deliberately low-confidence"
Check ($vehicleProject.Contains('<ClCompile Include="..\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.cpp"')) "engine project compiles MacPherson hardpoint estimator"
Check ($physicsTestProject.Contains('<ClCompile Include="..\Vehicles\Suspension\Authoring\MacPhersonHardpointEstimator.cpp"')) "physics regression project compiles MacPherson estimator"
$trailingArmEstimatorPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Authoring\TrailingArmHardpointEstimator.cpp"
$trailingArmEstimator = if (Test-Path $trailingArmEstimatorPath) { [IO.File]::ReadAllText($trailingArmEstimatorPath) } else { "" }
Check ($trailingArmEstimator.Contains("estimated_trailing_arm_torsion_bar_road_v1")) "trailing-arm estimator has a stable versioned profile ID"
Check ($trailingArmEstimator.Contains("result.confidence = 0.30f")) "trailing-arm estimate is deliberately low-confidence"

$luaRegistration = $luaRuntimeAndBindingsCpp
Check ($luaRegistration.Contains('"EstimateMacPhersonHardpoints"')) "Lua API registers MacPherson hardpoint estimation"
Check ($luaRegistration.Contains('"EstimateTrailingArmHardpoints"')) "Lua API registers trailing-arm hardpoint estimation"
Check ($luaRegistration.Contains('"SetWheelSuspensionHardpoints"')) "Lua API registers native hardpoint activation"
Check ($luaRegistration.Contains('"SetAntiRollBar"')) "Lua API registers anti-roll-bar setter"
Check ($luaRegistration.Contains('"GetAntiRollBar"')) "Lua API registers anti-roll-bar readback"
Check ($luaRegistration.Contains('"GetAntiRollBarCount"')) "Lua API registers anti-roll-bar count"

$assetMetadataHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleAssetMetadata.hpp"
$assetMetadataHeader = if (Test-Path $assetMetadataHeaderPath) { [IO.File]::ReadAllText($assetMetadataHeaderPath) } else { "" }
$assetMetadataCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleAssetMetadata.cpp"
$assetMetadataCpp = if (Test-Path $assetMetadataCppPath) { [IO.File]::ReadAllText($assetMetadataCppPath) } else { "" }
Check ($assetMetadataHeader.Contains("VehicleAssetSuspensionHardpointMetadata")) "vehicle GLB metadata owns suspension hardpoints"
Check ($assetMetadataCpp.Contains('"SUS_FL_"')) "GLB hardpoint naming supports SUS_FL/SUS_FR/SUS_RL/SUS_RR"
Check ($assetMetadataCpp.Contains('"suspension_hardpoint"')) "GLB semantic extras can identify suspension hardpoints"
$suspensionRegressionPath = Join-Path $Root "Engine\HeritageEngine\Tests\SuspensionRegression.cpp"
$suspensionRegression = if (Test-Path $suspensionRegressionPath) { [IO.File]::ReadAllText($suspensionRegressionPath) } else { "" }
Check ($suspensionRegression.Contains("assistedMacPhersonEstimateIsPlausibleAndMirrored")) "regressions validate assisted MacPherson estimate geometry"
Check ($suspensionRegression.Contains("assistedFrontMacPhersonVehicleStaysStable")) "regressions validate mixed MacPherson-front vehicle stability"
Check ($suspensionRegression.Contains("trailingArmTorsionBarKinematicsAreDeterministic")) "regressions validate trailing-arm/torsion-bar kinematics"
Check ($suspensionRegression.Contains("assistedFrontRearSuspensionVehicleStaysStable")) "regressions validate mixed front/rear hardpoint suspension stability"
Check ($suspensionRegression.Contains("suspensionAntiRollBarCouplesWheelPairs")) "regressions validate reusable suspension anti-roll-bar coupling"
Check ($visualDefinition.Contains('kinematics = "trailing_arm"')) "Peugeot prototype declares rear trailing-arm kinematics for authoring"
Check ($visualDefinition.Contains('spring = "torsion_bar"')) "Peugeot prototype declares separate rear torsion-bar springing"
Check ($visualDefinition.Contains('provider = "chassis_torsional_mode_v1"')) "prototype enables reusable chassis torsional compliance"
Check ($visualDefinition.Contains('provenance = "estimated_chassis_flex_closed_unibody_v1"')) "prototype labels chassis-flex estimate provenance"
Check ($visualDefinition.Contains('confidence = 0.18')) "prototype labels chassis-flex estimate confidence"
Check ($visualDefinition.Contains('inertiaLocalKgM2 = {')) "prototype carries explicit pitch/yaw/roll inertia"
Check ($visualDefinition.Contains('massPropertiesProvenance = "estimated_mass_properties_road_car_v1"')) "prototype labels mass-property estimate provenance"
Check ($visualDefinition.Contains("mount = { -0.7125, 0.85, 1.221 }")) "front-left mount matches reference geometry"
Check ($visualDefinition.Contains("mount = { 0.7080, 0.85, -1.221 }")) "rear-right mount matches reference geometry"

$antiRollHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionAntiRollBar.hpp"
$antiRollCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Common\SuspensionAntiRollBar.cpp"
$antiRollHeader = if (Test-Path $antiRollHeaderPath) { [IO.File]::ReadAllText($antiRollHeaderPath) } else { "" }
$antiRollCpp = if (Test-Path $antiRollCppPath) { [IO.File]::ReadAllText($antiRollCppPath) } else { "" }
Check ($antiRollHeader.Contains("SuspensionAntiRollBarDescription")) "anti-roll bar has an explicit reusable description contract"
Check ($antiRollHeader.Contains("SuspensionAntiRollBarOutput")) "anti-roll bar exposes deterministic torque/force output"
Check ($antiRollCpp.Contains("twistRadians")) "anti-roll bar evaluates differential wheel travel as torsional twist"
$antiRollLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\AntiRollBars.lua"
$antiRollLua = if (Test-Path $antiRollLuaPath) { [IO.File]::ReadAllText($antiRollLuaPath) } else { "" }
Check ($antiRollLua.Contains("ApplyDefinitionAntiRollBars")) "Racing United can apply definition-driven anti-roll bars"
Check ($antiRollLua.Contains("RefreshAntiRollBarTelemetry")) "Racing United can read live anti-roll-bar telemetry"

$chassisFlexLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\ChassisFlex.lua"
$chassisFlexLua = if (Test-Path $chassisFlexLuaPath) { [IO.File]::ReadAllText($chassisFlexLuaPath) } else { "" }
Check ($chassisFlexLua.Contains("ApplyDefinitionChassisFlex")) "Racing United can apply definition-driven chassis flex"
Check ($chassisFlexLua.Contains("RebuildEstimatedChassisFlex")) "Racing United can rebuild low-confidence chassis-flex estimates"
Check ($chassisFlexLua.Contains("Vehicle.EstimateChassisFlex")) "Racing United estimator routes through native chassis-flex logic"

$massPropertiesLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\MassProperties.lua"
$massPropertiesLua = if (Test-Path $massPropertiesLuaPath) { [IO.File]::ReadAllText($massPropertiesLuaPath) } else { "" }
Check ($massPropertiesLua.Contains("Vehicle.EstimateMassProperties")) "Racing United mass authoring routes through native estimator"
Check ($massPropertiesLua.Contains("Physics.SetBodyCenterOfMassLocal")) "Racing United mass authoring applies authored chassis COM"
Check ($massPropertiesLua.Contains("Physics.SetBodyInertiaLocal")) "Racing United applies explicit rigid-body inertia"
Check ($massPropertiesLua.Contains("massPropertiesProvenance")) "Racing United keeps mass-property provenance visible"
Check ($massPropertiesLua.Contains("HasStrongerAuthoredMassProperties")) "Racing United does not overwrite stronger authored mass properties with estimates"

$visualRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Visuals.lua"
$visualRuntime = if (Test-Path $visualRuntimePath) { [IO.File]::ReadAllText($visualRuntimePath) } else { "" }
Check ($visualRuntime.Contains("ApplyVehicleVisualTransform")) "visual alignment is isolated in Vehicles/Visuals.lua"
Check ($visualRuntime.Contains("Entity.SetMesh")) "vehicle visual slot uses the Entity mesh API"

$visualPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\VisualPanel.lua"
$visualPanel = if (Test-Path $visualPanelPath) { [IO.File]::ReadAllText($visualPanelPath) } else { "" }
Check ($visualPanel.Contains('SetPrototypeScenePreset("visual")')) "visual tab selects the clean showroom preset"
Check ($visualPanel.Contains('DrawVehicleVisualBodyPanel')) "visual coordinator routes to the body subpanel"
Check ($visualPanel.Contains('DrawVehicleVisualWheelsPanel')) "visual coordinator routes to the articulated-wheel subpanel"

$visualBodyPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\BodyPanel.lua"
$visualBodyPanel = if (Test-Path $visualBodyPanelPath) { [IO.File]::ReadAllText($visualBodyPanelPath) } else { "" }
Check ($visualBodyPanel.Contains("Runtime transform: identity / 1:1")) "visual body tab treats creator transform as authored identity"
Check (-not $visualBodyPanel.Contains("Visual uniform scale")) "routine body scale slider was removed"

$visualWheelsPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\WheelsPanel.lua"
$visualWheelsPanel = if (Test-Path $visualWheelsPanelPath) { [IO.File]::ReadAllText($visualWheelsPanelPath) } else { "" }
Check ($visualWheelsPanel.Contains("ARTICULATED WHEELS - AUTHORED 1:1")) "visual wheels tab exposes authored 1:1 contract"
Check (-not $visualWheelsPanel.Contains("Wheel radius scale")) "routine wheel radius scaling UI was removed"

$visualWheelsRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\VisualWheels.lua"
$visualWheelFiles = @(
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Visual\TransformMath.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Visual\ArticulatedWheels.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Visual\EmbeddedWheelBinding.lua"),
    (Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Visual\VisualWheels.lua")
)
$visualWheelsRuntime = (($visualWheelFiles | Where-Object { Test-Path $_ } | ForEach-Object { [IO.File]::ReadAllText($_) }) -join "`n")
$visualWheelsCoordinator = if (Test-Path $visualWheelsRuntimePath) { [IO.File]::ReadAllText($visualWheelsRuntimePath) } else { "" }
Check ($visualWheelsCoordinator.Contains('Vehicles/Visual/TransformMath.lua') -and $visualWheelsCoordinator.Contains('Vehicles/Visual/ArticulatedWheels.lua') -and $visualWheelsCoordinator.Contains('Vehicles/Visual/EmbeddedWheelBinding.lua') -and $visualWheelsCoordinator.Contains('Vehicles/Visual/VisualWheels.lua')) "CLEAN08 root visual-wheel file only coordinates responsibility-owned files"
Check ($visualWheelsRuntime.Contains("UpdateVehicleWheelPresentation")) "articulated-wheel presentation runtime exists"
Check ($visualWheelsRuntime.Contains("Entity.SetWorldPosition")) "wheel presentation consumes authoritative native world centers"
Check ($visualWheelsRuntime.Contains("telemetry.centerX")) "wheel presentation reads native center telemetry"
Check ($visualWheelsRuntime.Contains("telemetry.uprightRotationX")) "articulated-wheel presentation consumes native upright telemetry"
Check ($visualWheelsRuntime.Contains("VehicleWheelVisualRotation")) "wheel presentation composes mesh orientation after native upright pose"
Check ($visualWheelsRuntime.Contains("telemetry.rotationDegrees")) "articulated-wheel presentation consumes native wheel rotation telemetry"
Check ($visualWheelsRuntime.Contains("visualFaceYawDegrees")) "wheel presentation supports independent left/right mesh facing"
Check ($visualWheelsRuntime.Contains("visualSpinSign")) "wheel presentation compensates spin for per-side mesh facing"
Check ($visualWheelsRuntime.Contains("EmbeddedWheelReferenceSuspensionLength") -and $visualWheelsRuntime.Contains("wheel.restLengthM") -and $visualWheelsRuntime.Contains("PrototypeCarDefinition.wheelPhysics")) "embedded GLB wheel bind position uses authored suspension rest datum"
Check ($vehicleTelemetry.Contains("Vehicle.GetWheelTelemetry(nativeVehicle, index)")) "Racing United consumes the extensible named wheel telemetry table"
Check ($vehicleTelemetry.Contains("RefreshVehicleGeometryMeasurement()")) "Racing United refreshes live wheelbase/track geometry with wheel telemetry"
Check (-not $vehicleTelemetry.Contains("Vehicle.GetWheelState(nativeVehicle, index)")) "first-party wheel telemetry no longer depends on the 169-value positional ABI"
Check (-not $vehicleTelemetry.Contains("local grounded, length, compression")) "wheel telemetry does not directly unpack an extended native state into locals"
Check ($wheelTelemetryCpp.Contains('pushNumberField("length", value.suspensionLength)')) "named wheel telemetry exposes native suspension length through the Lua `length` field"
Check ($visualWheelsRuntime.Contains("telemetry.length or baseline.suspensionLength")) "embedded GLB wheel presentation consumes the actual Lua suspension-length field"
Check (-not $visualWheelsRuntime.Contains("telemetry.suspensionLength")) "embedded GLB wheel presentation does not reference the nonexistent suspensionLength telemetry field"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Entity", "SetMeshNodeTireFlexibleRingFromWheel"') -and $luaBindingCpp.Contains("luaEntitySetMeshNodeTireFlexibleRingFromWheel")) "TIRE44 registers the copy-only physics-owned flexible-carcass Entity API"
Check ($visualWheelsRuntime.Contains("Entity.SetMeshNodeTireFlexibleRingFromWheel") -and $visualWheelsRuntime.Contains('"WH_" .. corner .. "_Tire"')) "Peugeot embedded tire nodes consume the physics-owned dynamic carcass field"
Check ($visualWheelsRuntime.Contains("local flexibleRingBridge = Entity.SetMeshNodeTireFlexibleRingFromWheel") -and $visualWheelsRuntime.Contains("if flexibleRingBridge ~= nil then")) "a stale native executable cannot abort Lua reload and erase the vehicle"
$tire44FieldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireFlexibleRingField.hpp"
$tire44FieldCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireFlexibleRingField.cpp"
$tire44VehicleHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystem.hpp"
$tire44Phase03Path = Join-Path $Root "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\03_RoadEnvelopeAndFootprintSampling.inl"
$tire44Phase06Path = Join-Path $Root "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\06_ContactKinematicsAndPatchGeometry.inl"
$tire44FieldHeader = if (Test-Path $tire44FieldHeaderPath) { [IO.File]::ReadAllText($tire44FieldHeaderPath) } else { "" }
$tire44FieldCpp = if (Test-Path $tire44FieldCppPath) { [IO.File]::ReadAllText($tire44FieldCppPath) } else { "" }
$tire44VehicleHeader = if (Test-Path $tire44VehicleHeaderPath) { [IO.File]::ReadAllText($tire44VehicleHeaderPath) } else { "" }
$tire44Phase03 = if (Test-Path $tire44Phase03Path) { [IO.File]::ReadAllText($tire44Phase03Path) } else { "" }
$tire44Phase06 = if (Test-Path $tire44Phase06Path) { [IO.File]::ReadAllText($tire44Phase06Path) } else { "" }
Check ($tire44FieldHeader.Contains("TireFlexibleRingFieldStations = 24") -and $tire44FieldHeader.Contains("TireFlexibleRingDynamicState") -and $tire44FieldHeader.Contains("TireFlexibleRingRoadSample")) "TIRE44 owns one bounded 24x13 stateful carcass lattice and explicit road-contact samples"
Check ($tire44FieldCpp.Contains("advanceTireFlexibleRingDynamics") -and $tire44FieldCpp.Contains("Implicit Jacobi solve") -and $tire44FieldCpp.Contains("implicitIterations = std::clamp") -and $tire44FieldCpp.Contains("roadSampleForNode") -and $tire44FieldCpp.Contains("solveRankOneContact") -and $tire44FieldCpp.Contains("Internal rim/flange collision")) "TIRE44 solves bounded inertia/carcass/contact dynamics with pre-associated physical road samples"
Check ($tire44VehicleHeader.Contains("flexibleRingState") -and $tire44VehicleHeader.Contains("carcassRoadSamples") -and $tire44VehicleHeader.Contains("wheelTireFlexibleRingField") -and $tire44VehicleHeader.Contains("flexibleRingDemandSeconds")) "VehicleSystem owns persistent per-wheel carcass displacement/velocity, collision samples and bounded simulation demand state"
Check ($tire44Phase03.Contains("carcassRoadSamples") -and $tire44Phase03.Contains("sampleHit.point") -and $tire44Phase03.Contains("sampleHit.normal")) "TIRE44 reuses tire-force road-envelope collision geometry instead of creating a render-side support approximation"
Check ($tire44Phase06.Contains("VehicleScalar{0.008} / flexibleRingRateScale") -and $tire44Phase06.Contains("flexibleRingDemandSeconds") -and $tire44Phase06.Contains("advanceTireFlexibleRingDynamics") -and -not $tire44Phase06.Contains("carcassInput.verticalDeflection")) "TIRE44 advances requested carcasses at 125 Hz without prescribing physical tireDeflection as a mesh-shape target"
Check ($tire44VehicleHeader.Contains("pointDeltaFromCenterContactWorld") -and -not $tire44VehicleHeader.Contains("heritage::math::Vec3 pointWorld{};")) "TIRE45B carcass road-envelope cache stores local point offsets instead of stale absolute world positions"
Check ($tire44Phase03.Contains("subtract(source.pointWorld, hit.point)") -and $tire44Phase06.Contains("add(") -and $tire44Phase06.Contains("state.contactPoint") -and $tire44Phase06.Contains("source.pointDeltaFromCenterContactWorld")) "TIRE45B re-anchors cached road-envelope shape to the live centre contact before each carcass solve"
Check (-not $tire44FieldCpp.Contains("transportCarcassStateThroughWheelRotation")) "TIRE45B keeps circumferential carcass stations Eulerian and does not advect the whole lattice with wheel spin"
Check ($luaBindingCpp.Contains("wheelTireFlexibleRingField") -and -not $luaBindingCpp.Contains("evaluateTireFlexibleRingField") -and -not $luaBindingCpp.Contains("solveTireFlexibleRingPresentationField")) "TIRE44 render bridge copies the already-simulated physics field and performs no carcass equilibrium solve"
Check (-not $vehicleProject.Contains("LuaEntityTireFlexibleRingBridge.cpp") -and -not $vehicleProject.Contains("LuaEntityTireFlexibleRingBridge.hpp")) "retired TIRE41 render-time carcass solver bridge is not compiled"
$tire44LauncherPath = Join-Path $Root "Tools\00_BuildAndRunCurrent.cmd"
$tire44StudioLauncherPath = Join-Path $Root "Tools\01_BuildAndRunHeritageStudio.cmd"
$tire44Launcher = if (Test-Path $tire44LauncherPath) { [IO.File]::ReadAllText($tire44LauncherPath) } else { "" }
$tire44StudioLauncher = if (Test-Path $tire44StudioLauncherPath) { [IO.File]::ReadAllText($tire44StudioLauncherPath) } else { "" }
Check ($tire44Launcher.Contains("LuaEntityTireFlexibleRingBridge.cpp") -and $tire44Launcher.Contains("LuaEntityTireFlexibleRingBridge.hpp") -and $tire44Launcher.Contains("del /f /q") -and $tire44StudioLauncher.Contains("LuaEntityTireFlexibleRingBridge.cpp") -and $tire44StudioLauncher.Contains("LuaEntityTireFlexibleRingBridge.hpp") -and $tire44StudioLauncher.Contains("del /f /q")) "TIRE44A runtime and Studio launchers delete the retired render-time carcass bridge before validation"
Check (-not $tire44FieldCpp.Contains("supportPlaneLock") -and -not $tire44Phase06.Contains("supportPlaneLock") -and -not $luaBindingCpp.Contains("supportPlaneLock")) "TIRE44 contains no support-plane lock or vertex snapping quick fix"
$tire44FieldUniforms = ([regex]::Matches($clean05Shaders, "uniform bool uTireVisualDeformationFieldValid;")).Count
$tire45cVisibleSamples = ([regex]::Matches($clean05Shaders, "vec3 fieldM = sampleTireFlexibleRingField\(theta, widthCoordinate\);")).Count
$tire45cVisibleWorldAdds = ([regex]::Matches($clean05Shaders, "world\.xyz \+= tireWorldDisplacement;")).Count
$tire45cShadowSamples = ([regex]::Matches($clean05Shaders, "vec3 fieldM = sampleTireShadowField\(theta, widthCoordinate\);")).Count
$tire45cShadowWorldApplies = ([regex]::Matches($clean05Shaders, "worldPosition\.xyz = deformTireShadowWorldPosition\(")).Count
Check ($tire44FieldUniforms -eq 2 -and $tire45cVisibleSamples -eq 1 -and $tire45cVisibleWorldAdds -eq 1 -and $tire45cShadowSamples -eq 1 -and $tire45cShadowWorldApplies -eq 1) "TIRE45C visible and shadow shaders each sample/apply the final simulated displacement field exactly once in world space"
Check (([regex]::Matches($clean05Shaders, "uniform vec3 uTireVisualDisplacementM\[HERITAGE_TIRE_FIELD_COUNT\];")).Count -eq 2 -and -not $clean05Shaders.Contains("uTireVisualForwardDisplacementM")) "TIRE44 final vector field remains GPU-register-safe and unified"
Check (-not $clean05Shaders.Contains("uTireVisualProbeCompressionM") -and -not $clean05Shaders.Contains("flattenAmount") -and -not $clean05Shaders.Contains("hardPlanePenetration") -and -not $clean05Shaders.Contains("tireCarcassProfileM")) "TIRE44 does not restore shader dents, plane clamps, heuristic bulges or stacked carcass authorities"

$tire45LabHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireCarcassDevelopmentLab.hpp"
$tire45LabCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireCarcassDevelopmentLab.cpp"
$tire45BindingPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleTireCarcassLabBindings.cpp"
$tire45UiPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\Tires\CarcassMegaLabPanel.lua"
$tire45LabHeader = ReadText $tire45LabHeaderPath
$tire45LabCpp = ReadText $tire45LabCppPath
$tire45Binding = ReadText $tire45BindingPath
$tire45Ui = ReadText $tire45UiPath
Check ($vehicleProject.Contains('..\Vehicles\Tires\TireCarcassDevelopmentLab.cpp') -and $vehicleProject.Contains('..\Core\Modules\LuaBindings\Vehicle\LuaVehicleTireCarcassLabBindings.cpp')) "TIRE45 compiles the dedicated carcass development lab and Lua binding translation units"
Check ($physicsTestsProject.Contains('..\Vehicles\Tires\TireCarcassDevelopmentLab.cpp')) "TIRE45 physics regressions compile the exact carcass development/search implementation"
Check ($tire44FieldHeader.Contains("TireFlexibleRingDevelopmentTuning") -and $tire44FieldHeader.Contains("stationFoundationScale") -and $tire44FieldHeader.Contains("bandLateralScale")) "TIRE45 flexible-ring dynamics accepts bounded development-only global/station/band structural tuning"
Check ($tire45LabCpp.Contains("kParameterCount") -and $tire45LabCpp.Contains("kGlobalParameterCount = 32") -and $tire45LabCpp.Contains("kStationBankCount = 5") -and $tire45LabCpp.Contains("kBandBankCount = 5")) "TIRE45 exposes 217 raw structural controls rather than another small guessed slider set"
Check ($tire45LabCpp.Contains("runTireCarcassSyntheticScenario") -and $tire45LabCpp.Contains("advanceTireFlexibleRingDynamics") -and -not $tire45LabCpp.Contains("evaluateTireFlexibleRingField(")) "TIRE45 isolated scenarios execute the exact stateful 24x13 dynamic carcass solver, not the legacy equilibrium proxy"
Check ($tire45LabCpp.Contains("structuralRestRadialScale") -and $tire45LabCpp.Contains("Metrics must use the exact same rounded structural rest carcass")) "TIRE45 synthetic diagnostics measure penetration against the same rounded structural rest carcass used by the exact solver"
Check ($tire45LabCpp.Contains("nearestRoad") -and $tire45LabCpp.Contains("nearestRoad->supported") -and $tire45LabCpp.Contains("signedDistance") -and $tire45LabCpp.Contains("PartialRoadEdge") -and $tire45LabCpp.Contains("BankedRoad")) "TIRE45 pathology scoring follows discrete supported road samples/normals instead of inventing a flat support plane"
Check ($tire45LabCpp.Contains("runTireCarcassSearchBatch") -and $tire45LabCpp.Contains("tireCarcassDevelopmentTrialTuning") -and $tire45Ui.Contains("1000000") -and $tire45Ui.Contains("searchBatch")) "TIRE45 supports deterministic bounded brute-force search budgets up to one million exact candidates"
Check ($tire45Binding.Contains("luaVehicleRunTireCarcassSyntheticScenario") -and $tire45Binding.Contains("luaVehicleRunTireCarcassSearchBatch") -and $tire45Binding.Contains("luaVehicleApplyTireCarcassSearchTrial") -and $tire45Ui.Contains("APPLY TRIAL INDEX")) "TIRE45 Lua API exposes exact scenarios, search batches and reproducible candidate application/recall"
Check ($tire45Ui.Contains("DrawCarcassParameterGroup()") -and $tire45Ui.Contains("Vehicle.ResetTireCarcassLabState") -and $tire45Ui.Contains("Physics.SetTimeScale(0.0)") -and $tire45Ui.Contains("Physics.SetTimeScale(0.10)") -and $tire45Ui.Contains("Vehicle.CopyTireCarcassLabToAllWheels") -and $tire45Ui.Contains("CaptureCarcassSnapshot()") -and $tire45Ui.Contains("RestoreCarcassSnapshot(")) "TIRE45 in-game Tire LAB provides grouped live tuning, dynamic-state reset, freeze/slow-motion, A/B snapshots and all-wheel copy workflow"
Check ($tire45Ui.Contains("static_flat") -and $tire45Ui.Contains("hard_acceleration") -and $tire45Ui.Contains("hard_braking") -and $tire45Ui.Contains("zero_pressure") -and $tire45Ui.Contains("partial_road_edge")) "TIRE45 Tire LAB exposes static, longitudinal, pressure and partial-support structural scenarios"
Check (-not $tire45LabCpp.Contains("supportPlaneLock") -and -not $tire45Ui.Contains("support plane lock") -and -not $tire45LabCpp.Contains("world-Z")) "TIRE45 search/tuning infrastructure does not reintroduce a support-plane or world-height shape correction"

$wheelContractPath = Join-Path $Root "Docs\Decisions\ADR-009-Wheel-Coordinate-Contract.md"
Check (Test-Path $wheelContractPath) "wheel coordinate/presentation ADR exists"
$tireSurfaceRoadmapPath = Join-Path $Root "Docs\TIRE_SURFACE_ROADMAP.md"
$tire10AdrPath = Join-Path $Root "Docs\Decisions\ADR-043-Physical-Flat-Spot-Radius-And-Contact-Plane.md"
$tire11AdrPath = Join-Path $Root "Docs\Decisions\ADR-044-Spatial-Tread-Contamination-And-Cleaning.md"
$tire12AdrPath = Join-Path $Root "Docs\Decisions\ADR-045-Wet-Surface-Water-Film-And-Hydroplaning.md"
$tire13AdrPath = Join-Path $Root "Docs\Decisions\ADR-046-Compacted-Snow-And-Hard-Ice.md"
$tire14AdrPath = Join-Path $Root "Docs\Decisions\ADR-048-Shallow-Granular-Gravel-And-Hard-Dirt.md"
$tire15AdrPath = Join-Path $Root "Docs\Decisions\ADR-049-Deformable-Terrain-SurfaceField-Terramechanics.md"
$tireAuthoringAdrPath = Join-Path $Root "Docs\Decisions\ADR-047-Tire-And-Surface-Authoring.md"
Check (Test-Path $tireSurfaceRoadmapPath) "tire + driven-surface completion roadmap exists"
Check (Test-Path $tire10AdrPath) "TIRE10 physical flat-spot/contact-plane ADR exists"
Check (Test-Path $tire11AdrPath) "TIRE11 spatial tread contamination/cleaning ADR exists"
Check (Test-Path $tire12AdrPath) "TIRE12 wet-surface/water-film/hydroplaning ADR exists"
Check (Test-Path $tire13AdrPath) "TIRE13 compacted-snow/hard-ice ADR exists"
Check (Test-Path $tire14AdrPath) "TIRE14 shallow-granular gravel/hard-dirt ADR exists"
Check (Test-Path $tire15AdrPath) "TIRE15 deformable-terrain/SurfaceField ADR exists"
Check (Test-Path $tireAuthoringAdrPath) "tire engineering metadata / SurfaceMaterial-SurfaceField authoring ADR exists"
$tireSurfaceRoadmap = if (Test-Path $tireSurfaceRoadmapPath) { [IO.File]::ReadAllText($tireSurfaceRoadmapPath) } else { "" }
Check ($tireSurfaceRoadmap.Contains("SurfaceMaterial") -and $tireSurfaceRoadmap.Contains("SurfaceField") -and $tireSurfaceRoadmap.Contains("drainage") -and $tireSurfaceRoadmap.Contains("siping") -and $tireSurfaceRoadmap.Contains("stud")) "tire/surface roadmap preserves mechanism-based tire metadata and shared scene surface authoring"
$tire14Validated = [regex]::IsMatch($tireSurfaceRoadmap, 'TIRE14[^\r\n]*USER VALIDATED')
$tire15Validated = [regex]::IsMatch($tireSurfaceRoadmap, 'TIRE15[^\r\n]*USER VALIDATED')
Check ($tire14Validated -and $tire15Validated -and $tireSurfaceRoadmap.Contains("TIRE15B") -and $tireSurfaceRoadmap.Contains("TIRE15C") -and $tireSurfaceRoadmap.Contains("SurfaceField") -and $tireSurfaceRoadmap.Contains("rut") -and $tireSurfaceRoadmap.Contains("compaction") -and $tireSurfaceRoadmap.Contains("marbles")) "tire/surface roadmap records validated TIRE14/TIRE15 plus SurfaceField and tire-marble follow-ups"

$playerWorldPath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\PlayerWorld.lua"
$playerWorld = if (Test-Path $playerWorldPath) { [IO.File]::ReadAllText($playerWorldPath) } else { "" }
Check ($playerWorld.Contains("Physics.LoadStaticTriangleScene")) "Player World loads exact static triangle drive-surface queries"
Check ($playerWorld.Contains("Scene_*.glb")) "Player World discovers Scene_*.glb creator worlds"
Check ($playerWorld.Contains("playerWorld.sceneAsset")) "Player World owns one GLB scene-container slot"
Check ($playerWorld.Contains("false)")) "Player World uses native glTF coordinates without legacy OBJ conversion"
Check ($playerWorld.Contains("spawnPosition")) "Player World stores authoritative imported spawn position"
Check ($playerWorld.Contains("ResetVehicleAtPlayerWorldSpawn")) "Player World resets vehicle at imported spawn"

$staticSceneImporterPath = Join-Path $Root "Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp"
$staticSceneImporter = if (Test-Path $staticSceneImporterPath) { [IO.File]::ReadAllText($staticSceneImporterPath) } else { "" }
Check ($staticSceneImporter.Contains("SurfaceMaterial::Mud") -and $staticSceneImporter.Contains("SurfaceMaterial::Sand") -and $staticSceneImporter.Contains("SurfaceMaterial::SoftSoil") -and $staticSceneImporter.Contains("SurfaceMaterial::DeepSnow")) "static scene importer preserves TIRE15 mud/sand/soft-soil/deep-snow surface identity"
Check ($collisionCppText.Contains('return "mud"') -and $collisionCppText.Contains('return "sand"') -and $collisionCppText.Contains('return "soft_soil"') -and $collisionCppText.Contains('return "deep_snow"') -and $collisionCppText.Contains('normalizedText == "soft_soil"')) "CollisionSystem names/parses TIRE15 deformable surface identities"
Check ($staticSceneImporter.Contains("loadStaticTriangleSceneFromGlb")) "static triangle bridge imports collision-marked GLB scene geometry"
Check ($staticSceneImporter.Contains("heritage.surface")) "GLB collision bridge consumes authored surface metadata"
Check ($staticSceneImporter.Contains("triangle-origin")) "static triangle bridge retains terrain-at-origin spawn fallback"
Check ($staticSceneImporter.Contains("snapStaticTriangleSceneSpawnToSurface")) "authored spawn markers snap to the actual drive surface"

$gltfSceneDataPath = Join-Path $Root "Engine\HeritageEngine\Graphics\GltfSceneData.hpp"
$gltfSceneData = if (Test-Path $gltfSceneDataPath) { [IO.File]::ReadAllText($gltfSceneDataPath) } else { "" }
Check ($gltfSceneData.Contains("extractGlbStaticCollisionScene")) "OpenGL-free GLB collision extraction contract exists"

$gltfCollisionImporterPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Gltf\GltfCollisionImporter.cpp"
$gltfCollisionImporter = if (Test-Path $gltfCollisionImporterPath) { [IO.File]::ReadAllText($gltfCollisionImporterPath) } else { "" }
Check ($gltfCollisionImporter.Contains("heritage.collision_type")) "GLB parser recognizes explicit collision metadata"
Check ($gltfCollisionImporter.Contains("spawn_player")) "GLB parser recognizes SPAWN_PLAYER metadata"

$vehicleAssetMetadataHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleAssetMetadata.hpp"
$vehicleAssetMetadataCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleAssetMetadata.cpp"
$vehicleAssetMetadataHeader = if (Test-Path $vehicleAssetMetadataHeaderPath) { [IO.File]::ReadAllText($vehicleAssetMetadataHeaderPath) } else { "" }
$vehicleAssetMetadataCpp = if (Test-Path $vehicleAssetMetadataCppPath) { [IO.File]::ReadAllText($vehicleAssetMetadataCppPath) } else { "" }
Check ($vehicleAssetMetadataHeader.Contains("VehicleAssetWheelFitmentDatumMetadata")) "vehicle asset metadata owns explicit wheel-fitment datum records"
Check ($vehicleAssetMetadataCpp.Contains('"wheel_fitment_datum"')) "GLB metadata recognizes explicit wheel-fitment datum part type"
Check ($vehicleAssetMetadataCpp.Contains('"hub_face_center"') -and $vehicleAssetMetadataCpp.Contains('"wheel_centerline"') -and $vehicleAssetMetadataCpp.Contains('"wheel_spin_axis"')) "GLB metadata recognizes hub face / centerline / spin-axis roles"
Check ($vehicleAssetMetadataCpp.Contains('"FIT_FL_"')) "GLB metadata recognizes stable FIT_* wheel-datum aliases"

$utf8PathPath = Join-Path $Root "Engine\HeritageEngine\Core\Paths\Utf8Path.hpp"
$utf8Path = if (Test-Path $utf8PathPath) { [IO.File]::ReadAllText($utf8PathPath) } else { "" }
Check ($utf8Path.Contains("generic_u8string")) "module asset paths preserve UTF-8 names"

$meshCppPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Mesh.cpp"
$meshCpp = if (Test-Path $meshCppPath) { [IO.File]::ReadAllText($meshCppPath) } else { "" }
Check ($meshCpp.Contains("blenderCoordinates")) "OBJ loader has explicit Blender-coordinate mode"
Check ($meshCpp.Contains("return { value[0], value[1], -value[2] };")) "Blender default OBJ axis conversion maps into engine coordinates"
Check ($meshCpp.Contains("face[triangle + 1], face[triangle]")) "Blender-coordinate reflection reverses triangle winding"
Check ($meshCpp.Contains("computeTireVisualGeometry") -and $meshCpp.Contains("tireVisualOuterRadius") -and $meshCpp.Contains("isTireVisualNodeName")) "TIRE09 auto-infers tire axis/centre/bead/outer geometry from authored tire nodes"

$prototypeDefinition = $visualDefinition
Check ($prototypeDefinition.Contains("radiusScale = 1.0")) "creator wheel radius scale defaults to 1:1"
Check ($prototypeDefinition.Contains("widthScale = 1.0")) "creator wheel width scale defaults to 1:1"
Check ($prototypeDefinition.Contains("offset = { 0.0, 0.0, 0.0 }")) "creator body visual offset defaults to identity"
Check ($prototypeDefinition.Contains("enabled = true")) "articulated player wheels are enabled by default"
Check ($prototypeDefinition.Contains("referenceAlignment = Peugeot206RCReferenceAlignment")) "prototype references Peugeot 206 RC alignment evidence"
Check ($prototypeDefinition.Contains("referenceFitment =")) "prototype records authoritative reference wheel/tire metadata"
Check ($prototypeDefinition.Contains("factoryAlignmentSpecification = Peugeot206RCAlignmentSpecification")) "prototype references Peugeot 206 RC factory alignment specification"
Check ($prototypeDefinition.Contains("factorySetup = Peugeot206RCWorkshopAlignmentDefault")) "prototype separates adjustable alignment from neutral reference geometry"

$alignmentSpecificationPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\Peugeot206RC\AlignmentSpecification.lua"
$alignmentSpecification = if (Test-Path $alignmentSpecificationPath) { [IO.File]::ReadAllText($alignmentSpecificationPath) } else { "" }
Check ($alignmentSpecification.Contains('provenance = "peugeot_workshop_manual_plus_rc_alignment_database"')) "Peugeot 206 RC alignment specification records researched provenance"
Check ($alignmentSpecification.Contains('provenance = "midpoint_of_published_peugeot_206_rc_spec_range"')) "Peugeot 206 RC reference alignment labels midpoint-derived provenance"
Check ($alignmentSpecification.Contains('provenance = "stock_midpoint_of_published_peugeot_206_rc_spec_range"')) "Peugeot 206 RC stock runtime setup is source-labelled"
Check ($alignmentSpecification.Contains('toeInDegrees = -0.060000') -and $alignmentSpecification.Contains('camberDegrees = -1.0') -and $alignmentSpecification.Contains('toeInDegrees = 0.260000')) "Peugeot stock runtime setup applies published RC midpoint alignment"
Check ($prototypeDefinition.Contains("staticCamberDegrees = 0.0")) "reference wheel geometry remains neutral for setup-owned camber"
Check ($prototypeDefinition.Contains("staticToeDegrees = 0.0")) "reference wheel geometry remains neutral for setup-owned toe"
$peugeotSuspensionDocPath = Join-Path $Root "Docs\PEUGEOT_206_RC_SUSPENSION.md"
Check (Test-Path $peugeotSuspensionDocPath) "Peugeot 206 RC suspension evidence and estimate boundary are documented"

$moduleManifestPath = Join-Path $Root "Modules\RacingUnited\module.ini"
$moduleManifest = if (Test-Path $moduleManifestPath) { [IO.File]::ReadAllText($moduleManifestPath) } else { "" }
Check ($moduleManifest.Contains("entry_scene = prototype")) "Racing United enters the driveable prototype scene"

# CAMLAB01 vehicle camera authoring: fixed creator cameras must remain separate
# from ChaseCamera, while Racing United owns names/defaults/save policy in Lua.
$cameraControllerHeaderPath = Join-Path $Root "Engine\HeritageEngine\Camera\VehicleCameraController.hpp"
$cameraControllerCppPath = Join-Path $Root "Engine\HeritageEngine\Camera\VehicleCameraController.cpp"
$cameraBindingsPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaCameraBindings.cpp"
$cameraViewsPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\CameraViews.lua"
$cameraLabPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\CameraLabPanel.lua"
$cameraAdrPath = Join-Path $Root "Docs\Decisions\ADR-085-Vehicle-Camera-Authoring-And-Persistent-Presets.md"
$cameraDocPath = Join-Path $Root "Docs\VEHICLE_CAMERA_AUTHORING.md"
$cameraControllerHeader = if (Test-Path $cameraControllerHeaderPath) { [IO.File]::ReadAllText($cameraControllerHeaderPath) } else { "" }
$cameraControllerCpp = if (Test-Path $cameraControllerCppPath) { [IO.File]::ReadAllText($cameraControllerCppPath) } else { "" }
$cameraBindings = if (Test-Path $cameraBindingsPath) { [IO.File]::ReadAllText($cameraBindingsPath) } else { "" }
$cameraViews = if (Test-Path $cameraViewsPath) { [IO.File]::ReadAllText($cameraViewsPath) } else { "" }
$cameraLabPanel = if (Test-Path $cameraLabPanelPath) { [IO.File]::ReadAllText($cameraLabPanelPath) } else { "" }
Check (Test-Path $cameraAdrPath) "CAMLAB01 vehicle-camera authoring ADR exists"
Check (Test-Path $cameraDocPath) "CAMLAB01 vehicle-camera authoring documentation exists"
Check ($cameraControllerHeader.Contains("class VehicleCameraController") -and $cameraControllerCpp.Contains("buildLocalFrame") -and $cameraControllerCpp.Contains("updateFly")) "CAMLAB01 native vehicle camera controller owns fixed pose + free-fly mechanics"
Check ($cameraBindings.Contains('registerFunction("Camera", "SetVehiclePose"') -and $cameraBindings.Contains('registerFunction("Camera", "SetFlyEnabled"')) "CAMLAB01 exposes a narrow Camera Lua authoring bridge"
$requiredCameraNames = @("Cockpit", "Nose", "Gearbox", "Roll Bar", "F Susp", "R Susp", "FL Wheel", "FR Wheel", "RL Wheel", "RR Wheel")
$allCameraNamesPresent = $true
foreach ($cameraName in $requiredCameraNames) {
    if (-not $cameraViews.Contains('name = "' + $cameraName + '"')) { $allCameraNamesPresent = $false }
}
Check $allCameraNamesPresent "CAMLAB01 Racing United defines all requested driving/suspension/wheel camera names"
Check ($cameraViews.Contains("SaveCurrentVehicleCamera") -and $cameraViews.Contains("SaveAllVehicleCameras") -and $cameraViews.Contains("CameraSavePrefix")) "CAMLAB01 camera presets persist independently per vehicle"
Check ($cameraLabPanel.Contains("Position X (m)") -and $cameraLabPanel.Contains("Position Y (m)") -and $cameraLabPanel.Contains("Position Z (m)") -and $cameraLabPanel.Contains("Pitch (deg)") -and $cameraLabPanel.Contains("Yaw (deg)") -and $cameraLabPanel.Contains("Roll / rotation angle (deg)")) "CAMLAB01 LAB exposes complete XYZ + pitch/yaw/roll authoring"
Check ($cameraLabPanel.Contains("UI.SliderFloat") -and $cameraLabPanel.Contains("UI.InputFloat") -and $cameraLabPanel.Contains("START FREE FLY")) "CAMLAB01 camera authoring supports sliders, exact numeric entry and free fly"


# UI04: revert the failed UI01/UI02/UI03 custom tab experiments. Racing United
# creator/debug pages use Heritage's original ImGui tab bars again. Buttons use
# natural label widths, left-to-right rows and a last-resort tiny-row wrap so a
# SameLine() chain can never leave a mysterious one-letter button off the panel.
$ui04LuaUiPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaUiBindings.cpp"
$ui04CommonPath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\Common.lua"
$ui04PrototypeTabsPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\PrototypeScreen.lua"
$ui04VehicleTabsPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\VehicleDebugPanel.lua"
$ui04LabTabsPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\LabPanel.lua"
$ui04LuaUi = if (Test-Path $ui04LuaUiPath) { [IO.File]::ReadAllText($ui04LuaUiPath) } else { "" }
$ui04Common = if (Test-Path $ui04CommonPath) { [IO.File]::ReadAllText($ui04CommonPath) } else { "" }
$ui04PrototypeTabs = if (Test-Path $ui04PrototypeTabsPath) { [IO.File]::ReadAllText($ui04PrototypeTabsPath) } else { "" }
$ui04VehicleTabs = if (Test-Path $ui04VehicleTabsPath) { [IO.File]::ReadAllText($ui04VehicleTabsPath) } else { "" }
$ui04LabTabs = if (Test-Path $ui04LabTabsPath) { [IO.File]::ReadAllText($ui04LabTabsPath) } else { "" }
Check ($ui04LuaUi.Contains("ImGuiTabBarFlags_FittingPolicyScroll") -and -not $ui04Common.Contains("DrawResponsiveTabs") -and $ui04PrototypeTabs.Contains('UI.BeginTabBar("PrototypeLabTabs")') -and $ui04VehicleTabs.Contains('UI.BeginTabBar("VehicleDebugTabs")') -and $ui04LabTabs.Contains('UI.BeginTabBar("VehicleLabSubTabs")')) "UI04 restores original ImGui tab bars across Racing United LAB pages"
Check ($ui04LuaUi.Contains("ImGui::CalcTextSize(label.c_str(), nullptr, true)") -and $ui04LuaUi.Contains("availableWidth < 120.0f") -and $ui04LuaUi.Contains("ImGui::NewLine()") -and $ui04LuaUi.Contains("state, 4, false")) "UI04 Lua buttons use natural widths and never collapse into off-panel one-letter controls"

# AUDIO01: Engine Simulator CE remains an external authoring source. Heritage
# owns capture, non-destructive acoustic shaping and bank metadata without
# introducing a second production vehicle-audio authority.
$audio01CaptureHeaderPath = Join-Path $Root "Engine\HeritageEngine\Audio\Lab\EngineSoundCaptureLab.hpp"
$audio01CaptureCppPath = Join-Path $Root "Engine\HeritageEngine\Audio\Lab\EngineSoundCaptureLab.cpp"
$audio01DspPath = Join-Path $Root "Engine\HeritageEngine\Audio\Lab\EngineSoundLabDsp.cpp"
$audio01BindingsPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaAudioBindings.cpp"
$audio01PanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\AudioLabPanel.lua"
$audio01DocPath = Join-Path $Root "Docs\Audio-Engine-Sound-Capture-Laboratory.md"
$audio01EnginePath = Join-Path $Root "Modules\RacingUnited\Assets\Audio\Authoring\EngineSimulator\Peugeot206RC\Peugeot_206_RC_EW10J4S_FINAL_STOCK.mr"
$audio01ResearchPath = Join-Path $Root "Modules\RacingUnited\Assets\Audio\Authoring\EngineSimulator\Peugeot206RC\Peugeot_206_RC_EW10J4S_FINAL_RESEARCH.txt"
$audio01Capture = if (Test-Path $audio01CaptureCppPath) { [IO.File]::ReadAllText($audio01CaptureCppPath) } else { "" }
$audio01Dsp = if (Test-Path $audio01DspPath) { [IO.File]::ReadAllText($audio01DspPath) } else { "" }
$audio01Bindings = if (Test-Path $audio01BindingsPath) { [IO.File]::ReadAllText($audio01BindingsPath) } else { "" }
$audio01Panel = if (Test-Path $audio01PanelPath) { [IO.File]::ReadAllText($audio01PanelPath) } else { "" }
Check ((Test-Path $audio01CaptureHeaderPath) -and (Test-Path $audio01CaptureCppPath) -and (Test-Path $audio01DspPath)) "AUDIO01 native Engine Sound Capture Laboratory is modular under Audio/Lab"
Check ($audio01Capture.Contains("AUDCLNT_STREAMFLAGS_LOOPBACK") -and $audio01Capture.Contains("WAVE_FORMAT_IEEE_FLOAT") -and $audio01Capture.Contains("capture_manifest.csv")) "AUDIO01 records default-output WASAPI loopback into float WAV and owns guided-bank manifest metadata"
Check ($audio01Dsp.Contains("EngineSoundPerspective::Raw") -and $audio01Dsp.Contains("EngineSoundPerspective::EngineBay") -and $audio01Dsp.Contains("EngineSoundPerspective::RearExhaust") -and $audio01Dsp.Contains("EngineSoundPerspective::DriverCabin")) "AUDIO01 non-destructive DSP preserves RAW plus engine-bay/exhaust/cabin audition perspectives"
Check ($audio01Bindings.Contains('registerFunction("Audio", "EngineLabStartCalibrationCapture"') -and $audio01Bindings.Contains('registerFunction("Audio", "EngineLabStartBankCapture"') -and $audio01Bindings.Contains('registerFunction("Audio", "EngineLabSetProfile"')) "AUDIO01 exposes a narrow Lua capture/profile authoring bridge"
Check ($audio01Panel.Contains("53 steady-state cells") -and $audio01Panel.Contains("SHAPE / FILTER") -and $audio01Panel.Contains("Stock exhaust muffling") -and $audio01Panel.Contains("Cabin damping / insulation")) "AUDIO01 Racing United LAB exposes guided capture plus Materialize-style acoustic shaping"
Check ((Test-Path $audio01DocPath) -and (Test-Path $audio01EnginePath) -and (Test-Path $audio01ResearchPath)) "AUDIO01 documentation and final calibrated EW10J4S authoring assets are present"
