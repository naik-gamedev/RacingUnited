# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

# ARCH03: regressions are deliberately split by domain. Treat the suite as one
# safety contract instead of assuming every test lives in the runner translation unit.
$physicsRegressionRelativePaths = @(
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
    "Engine\HeritageEngine\Tests\VehicleDefinitionRegression.cpp"
)
$physicsRegression = ($physicsRegressionRelativePaths | ForEach-Object {
    $path = Join-Path $Root $_
    if (Test-Path $path) { [IO.File]::ReadAllText($path) } else { "" }
}) -join "`n"

$physicsTestProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$physicsTestProject = if (Test-Path $physicsTestProjectPath) { [IO.File]::ReadAllText($physicsTestProjectPath) } else { "" }
Check ($physicsTestProject.Contains('ClCompile Include="PhysicsRegressionSupport.cpp"')) "physics test project compiles shared regression support"
Check ($physicsTestProject.Contains('ClCompile Include="VehicleDynamicsRegression.cpp"')) "physics test project compiles vehicle dynamics regressions"
Check ($physicsTestProject.Contains('ClCompile Include="CollisionTerrainRegression.cpp"')) "physics test project compiles collision/terrain regressions"
Check ($physicsTestProject.Contains('ClCompile Include="SuspensionRegression.cpp"')) "physics test project compiles suspension regressions"
Check ($physicsTestProject.Contains('ClCompile Include="ChassisDynamicsRegression.cpp"')) "physics test project compiles chassis-dynamics regressions"
Check ($physicsTestProject.Contains('ClCompile Include="ChassisFlexRegression.cpp"')) "physics test project compiles chassis-flex regressions"
Check ($physicsTestProject.Contains('ClCompile Include="MassPropertiesRegression.cpp"')) "physics test project compiles mass-property regressions"
Check ($physicsTestProject.Contains('ClCompile Include="FitmentRegression.cpp"')) "physics test project compiles wheel-fitment/alignment regressions"
Check ($physicsTestProject.Contains('ClCompile Include="VehicleDefinitionRegression.cpp"')) "physics test project compiles definition/compiler regressions"
Check ($physicsTestProject.Contains('ClCompile Include="..\Vehicles\Tires\Authoring\TirePartResolver.cpp"')) "physics test project compiles TIRE17C reusable tire-part resolver"
foreach ($configSource in $vehicleConfigurationRelativePaths) {
    $projectRelative = $configSource.Replace("Engine\HeritageEngine\", "..\")
    Check ($physicsTestProject.Contains('ClCompile Include="' + $projectRelative + '"')) "physics test project compiles CLEAN03A configuration source: $projectRelative"
}
Check (-not $physicsTestProject.Contains('ClCompile Include="..\Vehicles\VehicleConfiguration.cpp"')) "physics test project does not compile retired VehicleConfiguration umbrella"
Check ($physicsTestProject.Contains('ClCompile Include="..\Vehicles\VehicleTelemetry.cpp"')) "physics test project compiles CLEAN02 vehicle telemetry"
Check ($physicsTestProject.Contains('ClCompile Include="..\Vehicles\VehicleSimulation.cpp"')) "physics test project compiles CLEAN02 vehicle simulation"
Check ($physicsTestProject.Contains('ClCompile Include="..\Vehicles\VehicleWheelSimulation.cpp"')) "physics test project compiles CLEAN02 wheel simulation"
foreach ($phasePath in $wheelSubstepPhaseRelativePaths) {
    $projectRelative = $phasePath.Replace("Engine\HeritageEngine\", "..\")
    Check ($physicsTestProject.Contains('ClInclude Include="' + $projectRelative + '"')) "physics test project tracks CLEAN03B wheel phase: $projectRelative"
}
Check ($physicsRegression.Contains("parkingBrakeHoldsOnSlope")) "headless regression covers parking-brake slope hold"
Check ($physicsRegression.Contains("unbrakedVehicleRollsOnSlope")) "headless regression preserves unbraked slope roll"
Check ($physicsRegression.Contains("flatRestSleepsAndThrottleWakes")) "headless regression covers parked sleep and throttle wake"
Check ($physicsRegression.Contains("brakeHeldSteeringWakesAndTracks")) "headless regression covers brake-held parked steering wake/tracking"
Check ($vehicleCpp.Contains("steeringMotionRequested") -and $vehicleCpp.Contains("steeringSettled")) "parked-rest logic keeps steering active while the brake is held"
Check ($vehicleCpp.Contains('requestedSteerCenterDegrees != vehicle.currentSteerCenterDegrees') -and $vehicleCpp.Contains('requestedSteerCenterDegrees == vehicle.currentSteerCenterDegrees') -and $vehicleCpp.Contains('vehicle.steering == 0.0f')) "INPUT08 steering has no hidden wake/settle/return-to-center deadband"
Check ($physicsRegression.Contains('0.0001f') -and $physicsRegression.Contains('afterMicroSteer')) "INPUT08 regression covers sub-0.01-degree parked steering onset"
Check ($physicsRegression.Contains("dynamicsLabCapturesHighRateTelemetry")) "headless regression verifies exact high-rate dynamics capture"
Check ($physicsRegression.Contains("vehicleDefinitionCompilerAndLoaderWork")) "headless regression verifies native definition compilation and loading"
Check ($physicsRegression.Contains("motion_ratio_force_n")) "headless regression verifies suspension motion-ratio force evaluation"
Check ($physicsRegression.Contains("nonlinear_force_n")) "headless regression verifies non-linear suspension force components"
Check ($physicsRegression.Contains("live_suspension_roundtrip")) "headless regression verifies live suspension tuning roundtrip"
Check ($physicsRegression.Contains("scalar unsprung-mass wheel-hop response")) "headless regression verifies scalar wheel-hop response"
Check ($physicsRegression.Contains("live_unsprung_roundtrip")) "headless regression verifies live unsprung-mass tuning roundtrip"
Check ($physicsRegression.Contains("authoritative suspension upright pose")) "headless regression verifies authoritative suspension geometry"
Check ($physicsRegression.Contains("live_geometry_roundtrip")) "headless regression verifies live suspension-geometry tuning roundtrip"
Check ($physicsRegression.Contains("terrainContactDiagnosticsClassifyFailureModes")) "headless regression diagnoses terrain seams, gaps, bottom-out, tunnelling and bounds"
Check ($physicsRegression.Contains("rigidBodyCenterOfMassOffsetGeneratesTorque")) "headless regression verifies offset COM generates physical torque"
Check ($physicsRegression.Contains("vehicleChassisRollRespondsToCornering")) "headless regression verifies chassis body roll and left/right load transfer"
Check ($physicsRegression.Contains("chassisTorsionalComplianceRespondsToDiagonalLoad")) "headless regression verifies structural torsion under diagonal loading"
Check ($physicsRegression.Contains("chassisFlexIntegratesWithHighRateVehicleDynamics")) "headless regression verifies chassis flex inside high-rate vehicle dynamics"
Check ($physicsRegression.Contains("vehicleMassPropertiesEstimatorProducesBoundedEstimate")) "headless regression verifies epistemic vehicle mass-property estimation"
Check ($physicsRegression.Contains("vehicleMassComponentAccumulationUsesParallelAxisTheorem")) "headless regression verifies installed-component mass accumulation"
Check ($physicsRegression.Contains("rigidBodyExplicitInertiaIsAuthoritative")) "headless regression verifies explicit inertia survives collider mass-property rebuilds"
Check ($physicsRegression.Contains("wheelFitmentAndAlignmentAreReferenceSafe")) "headless regression verifies fitment moves installed centerline without moving suspension/Ackermann reference geometry"
Check ($physicsRegression.Contains("TIRE17C reusable tire parts resolve and assign with per-wheel cold pressure")) "headless regression verifies TIRE17C runtime tire-part reuse and fitment pressure ownership"
Check ($physicsRegression.Contains("vehicleCombinedPitchRollYawRespondsToBrakingTurn")) "headless regression verifies simultaneous pitch/roll/yaw and four-corner response"
Check ($physicsRegression.Contains("max_corner_load_spread_n")) "combined chassis regression exposes asymmetric corner loading"

$definitionV2Directory = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions"
$definitionV2Files = @(Get-ChildItem -Path $definitionV2Directory -Filter "VehicleDefinitionV2*.lua" -File -ErrorAction SilentlyContinue | Sort-Object Name)
$definitionV2 = ReadTextSet $definitionV2Files
Check ($definitionV2Files.Count -eq 6) "VehicleDefinitionV2 is decomposed into schema, builder, core validation, dynamics validation, compatibility and serialization files"
$oversizedDefinitionV2Files = @($definitionV2Files | Where-Object { (Get-Content $_.FullName).Count -ge 700 })
Check ($oversizedDefinitionV2Files.Count -eq 0) "no VehicleDefinitionV2 Lua responsibility file has become a 700+ line dumping ground"
$definitionValidationRoot = [IO.File]::ReadAllText((Join-Path $definitionV2Directory "VehicleDefinitionV2Validation.lua"))
$definitionDynamicsValidation = [IO.File]::ReadAllText((Join-Path $definitionV2Directory "VehicleDefinitionV2DynamicsValidation.lua"))
$definitionCompatibility = [IO.File]::ReadAllText((Join-Path $definitionV2Directory "VehicleDefinitionV2Compatibility.lua"))
Check ($definitionValidationRoot.Contains("ValidateVehicleDefinitionV2Dynamics") -and $definitionValidationRoot.Contains("ValidateVehicleDefinitionV2Compatibility")) "CLEAN08 root definition validator orchestrates dynamics and compatibility phases"
Check ($definitionDynamicsValidation.Contains("suspension_motion_ratio") -and $definitionDynamicsValidation.Contains("anti_roll_bar_parameter") -and $definitionDynamicsValidation.Contains("chassis_flex_parameter")) "CLEAN08 dynamics validator owns physical component validity"
Check ($definitionCompatibility.Contains("future_solver_components") -and $definitionCompatibility.Contains("motorcycle lean/camber dynamics")) "CLEAN08 compatibility validator distinguishes valid future topology from current preview support"
# ROLL01: the authored rigid-body/entity origin is a reference datum, not an
# implicit physical centre of mass. Vehicle content must opt into a physical COM
# explicitly and preserve provenance/confidence for estimated values.
$collisionCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\CollisionSystem.cpp"
$collisionCppText = if (Test-Path $collisionCppPath) { [IO.File]::ReadAllText($collisionCppPath) } else { "" }
$prototypeCarPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua"
$prototypeCar = if (Test-Path $prototypeCarPath) { [IO.File]::ReadAllText($prototypeCarPath) } else { "" }
$vehicleFactoryPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Factory.lua"
$vehicleFactory = if (Test-Path $vehicleFactoryPath) { [IO.File]::ReadAllText($vehicleFactoryPath) } else { "" }
Check ($rigidBodyHeader.Contains("centerOfMassLocal")) "rigid-body contract separates local physical COM from authored origin"
Check ($rigidBodyHeader.Contains("centerOfMassWorld")) "rigid-body contract exposes physical world COM"
Check ($collisionCppText.Contains("worldCenterOfMass")) "collision/contact torque and inertia paths reference physical COM"
Check ($prototypeCar.Contains("centerOfMassLocal")) "Racing United prototype authors a chassis COM explicitly"
Check ($prototypeCar.Contains("centerOfMassProvenance")) "Racing United prototype labels COM provenance"
Check ($prototypeCar.Contains("centerOfMassConfidence")) "Racing United prototype labels COM confidence"
Check ($vehicleFactory.Contains("ApplyPrototypeRigidBodyMassProperties(nativeVehicleBody)")) "Racing United vehicle factory routes chassis mass properties through shared authoring helper"

$vehicleTelemetryPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Telemetry.lua"
$vehicleTelemetry = if (Test-Path $vehicleTelemetryPath) { [IO.File]::ReadAllText($vehicleTelemetryPath) } else { "" }
$vehicleTelemetryPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\TelemetryPanel.lua"
$vehicleTelemetryPanel = if (Test-Path $vehicleTelemetryPanelPath) { [IO.File]::ReadAllText($vehicleTelemetryPanelPath) } else { "" }
Check ($vehicleTelemetry.Contains("RefreshAntiRollBarTelemetry")) "vehicle telemetry refreshes anti-roll state with wheel/chassis state"
Check ($vehicleTelemetryPanel.Contains("Body pitch / yaw / roll")) "vehicle telemetry names chassis rotational axes physically"
Check ($vehicleTelemetryPanel.Contains("Diagonal load FL+RR / FR+RL")) "vehicle telemetry exposes diagonal four-corner loading"
Check ($vehicleTelemetryPanel.Contains("ARB torque front/rear")) "vehicle telemetry exposes independent front/rear anti-roll torque"

Check ($definitionV2.Contains("schemaVersion = 2")) "VehicleDefinitionV2 has an explicit schema version"
Check ($definitionV2.Contains("powerUnits = {}")) "VehicleDefinitionV2 stores arbitrary power units"
Check ($definitionV2.Contains("transmissions = {}")) "VehicleDefinitionV2 stores arbitrary transmissions"
Check ($definitionV2.Contains("suspensions = {}")) "VehicleDefinitionV2 stores reusable suspension components"
Check ($definitionV2.Contains("suspension = suspensionId")) "contact units reference stable suspension IDs"
Check ($definitionV2.Contains("bumpHighSpeedDampingNsPerM")) "suspension definitions author digressive damping"
Check ($definitionV2.Contains("bumpStopProgressionNPerM2")) "suspension definitions author progressive travel stops"
Check ($definitionV2.Contains("effectiveUnsprungMassKg")) "contact definitions author effective unsprung mass"
Check ($definitionV2.Contains("tireRadialStiffnessNPerM")) "contact definitions author radial tire stiffness"
Check ($definitionV2.Contains("steeringAxis =")) "suspension definitions author a local steering axis"
Check ($definitionV2.Contains("camberGainDegreesPerM")) "suspension definitions author camber travel curves"
Check ($definitionV2.Contains("toeGainDegreesPerM")) "suspension definitions author toe/bump-steer curves"
Check ($definitionV2.Contains("driveConnections = {}")) "VehicleDefinitionV2 stores explicit drive connections"
Check ($definitionV2.Contains("ValidateVehicleDefinitionV2")) "VehicleDefinitionV2 has structural validation"
Check ($definitionV2.Contains("currentSolverReady")) "definition validity remains separate from current solver support"
Check ($definitionV2.Contains('twin_engine = {')) "Workshop includes an independent-powertrain template"

$workshopPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Workshop.lua"
$workshop = if (Test-Path $workshopPath) { [IO.File]::ReadAllText($workshopPath) } else { "" }
Check ($workshop.Contains("Module.SelectAssetFile")) "Workshop uses the module-isolated native asset picker"
Check ($workshop.Contains("Module.WriteSaveText")) "Workshop exports through the bounded module save API"
Check ($workshop.Contains("ApplyVehicleWorkshopPreview")) "Workshop has an explicit current-solver preview bridge"
Check ($workshop.Contains("Vehicle.CompileDefinitionV2")) "Workshop uses authoritative native definition compilation"
Check (-not $workshop.Contains("ApplyWorkshopDriveLayout")) "Workshop no longer reconstructs drivetrain routing in Lua"

$workshopPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\WorkshopPanel.lua"
$workshopPanel = if (Test-Path $workshopPanelPath) { [IO.File]::ReadAllText($workshopPanelPath) } else { "" }
Check ($workshopPanel.Contains("UI.GetAvailableWidth()")) "Workshop sizes controls from the available panel width"
Check ($workshopPanel.Contains("WorkshopTemplateRow")) "Workshop wraps topology templates into bounded rows"
Check ($workshopPanel.Contains("WorkshopTemplateThreeColumnRow")) "Workshop supports the wider three-column topology grid"
Check ($workshopPanel.Contains("availableWidth >= 540.0")) "Workshop retains an explicit narrow-panel fallback"


$precisionHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehiclePrecision.hpp"
$precisionHeader = if (Test-Path $precisionHeaderPath) { [IO.File]::ReadAllText($precisionHeaderPath) } else { "" }
Check ($precisionHeader.Contains("using VehicleScalar = double")) "vehicle high-rate scalar policy is FP64"
Check ($vehicleHeader.Contains("VehicleScalar wheelAngularVelocity")) "wheel angular state retains FP64 precision"
Check ($vehicleHeader.Contains("VehicleScalar slipRatio")) "wheel slip state retains FP64 precision"
Check ($vehicleHeader.Contains("VehicleScalar normalForce")) "wheel contact-force state retains FP64 precision"
Check (-not $luaRuntimeAndBindingsCpp.Contains("value.springRateNPerM = static_cast<float>")) "Lua suspension provider avoids FP32 truncation"
Check (-not $luaRuntimeAndBindingsCpp.Contains("value.tireRadialStiffnessNPerM = static_cast<float>")) "Lua unsprung-mass provider avoids FP32 truncation"

$mathHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Math\Math.hpp"
$mathHeader = if (Test-Path $mathHeaderPath) { [IO.File]::ReadAllText($mathHeaderPath) } else { "" }
$physicsWorldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\PhysicsWorld.hpp"
$physicsWorldHeader = if (Test-Path $physicsWorldHeaderPath) { [IO.File]::ReadAllText($physicsWorldHeaderPath) } else { "" }
$physicsWorldCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\PhysicsWorld.cpp"
$physicsWorldCpp = if (Test-Path $physicsWorldCppPath) { [IO.File]::ReadAllText($physicsWorldCppPath) } else { "" }
$meshRendererPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$meshRenderer = if (Test-Path $meshRendererPath) { [IO.File]::ReadAllText($meshRendererPath) } else { "" }
$meshRendererFramePrepPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshAnimation.cpp"
$meshRendererFramePrep = if (Test-Path $meshRendererFramePrepPath) { [IO.File]::ReadAllText($meshRendererFramePrepPath) } else { "" }
$vehicleFactoryPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Factory.lua"
$vehicleFactory = if (Test-Path $vehicleFactoryPath) { [IO.File]::ReadAllText($vehicleFactoryPath) } else { "" }
Check ($mathHeader.Contains("struct DVec3")) "large-world absolute coordinate type is FP64"
Check ($physicsWorldHeader.Contains("heritage::math::DVec3 m_globalOrigin")) "physics world owns an FP64 global origin"
Check ($physicsWorldCpp.Contains("updateFloatingOrigin")) "physics world performs bounded floating-origin rebases"
Check ($physicsWorldCpp.Contains("m_collisions.rebaseLocalOrigin")) "static collision follows physics-origin rebases"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetBodyGlobalPosition"')) "Lua exposes FP64 body-global coordinates"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetBodyCenterOfMassLocal"')) "Lua exposes body-local physical COM readback"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "SetBodyCenterOfMassLocal"')) "Lua exposes body-local physical COM authoring"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetBodyCenterOfMassWorld"')) "Lua exposes world physical COM readback"
Check ($meshRenderer.Contains("prepareFrameInstances(instances, eye, elapsedSeconds)") -and $meshRendererFramePrep.Contains("cameraRelativeInstance.position") -and $meshRendererFramePrep.Contains("instance.position.x - eye.x") -and $meshRendererFramePrep.Contains("instance.position.y - eye.y") -and $meshRendererFramePrep.Contains("instance.position.z - eye.z") -and $meshRendererFramePrep.Contains("prepared.instanceModel = modelMatrix(cameraRelativeInstance)")) "mesh renderer submits camera-relative FP32 positions"
Check ($vehicleCpp.Contains("pointVelocityFromOffset")) "vehicle solver uses chassis-local point-velocity offsets"
Check ($vehicleFactory.Contains("Physics.SetFloatingOriginAnchor(nativeVehicleBody, 4096.0)")) "Racing United anchors floating origin to the player chassis"

$vegetationHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\VegetationSystem.hpp"
$vegetationHeader = if (Test-Path $vegetationHeaderPath) { [IO.File]::ReadAllText($vegetationHeaderPath) } else { "" }
$vegetationCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\VegetationSystem.cpp")
$lodTransitionHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\LodTransitionPolicy.hpp")
$presentationPrecisionHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\PresentationPrecision.hpp")
$performanceHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceMonitor.hpp"
$performanceHeader = if (Test-Path $performanceHeaderPath) { [IO.File]::ReadAllText($performanceHeaderPath) } else { "" }
$mainCppPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\main.cpp"
$mainCpp = if (Test-Path $mainCppPath) { [IO.File]::ReadAllText($mainCppPath) } else { "" }
$heritageEngineCppPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.cpp"
$heritageEngineCpp = if (Test-Path $heritageEngineCppPath) { [IO.File]::ReadAllText($heritageEngineCppPath) } else { "" }
$performanceOverlayPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp"
$performanceOverlayCpp = if (Test-Path $performanceOverlayPath) { [IO.File]::ReadAllText($performanceOverlayPath) } else { "" }
Check ($vegetationHeader.Contains("QuantizedVegetationInstance")) "vegetation uses chunk-local quantized placement storage"
Check ($vegetationHeader.Contains("VegetationKind::") -or $vegetationHeader.Contains("enum class VegetationKind")) "vegetation supports multiple plant families"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Vegetation", "RegisterSpecies"')) "Lua exposes vegetation species registration"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Vegetation", "GetStats"')) "Lua exposes vegetation diagnostics"
Check ($lodTransitionHeader.Contains("MasterTransitionPolicy") -and $lodTransitionHeader.Contains("crossfadeAtBoundary") -and $lodTransitionHeader.Contains("crossfadeAfterBoundary") -and $lodTransitionHeader.Contains("visibilityWeight")) "LOD01 Heritage owns one reusable master LOD blend/fade policy"
Check ($vegetationHeader.Contains("VegetationRepresentationBlend") -and $vegetationCpp.Contains("representationBlendForDistance") -and $vegetationCpp.Contains("lod::crossfadeAtBoundary")) "LOD01 vegetation exposes master-policy representation blending instead of only hard LOD selection"
Check ($presentationPrecisionHeader.Contains("cameraRelativeFp32") -and $presentationPrecisionHeader.Contains("Persistent/authoritative state may stay FP64")) "LOD01 presentation precision keeps authoritative world state precise and emits camera-relative FP32 render coordinates"
Check ($performanceHeader.Contains("struct PerformanceSnapshot")) "performance monitor exposes rolling timing snapshot"
Check ($clean09Rendering.Contains("GL_TIME_ELAPSED") -and $clean09Rendering.Contains("glBeginQuery") -and $clean09Rendering.Contains("glGetQueryObject")) "performance monitor uses asynchronous GPU timer queries"
Check ($performanceOverlayCpp.Contains("HERITAGE PERFORMANCE [F8]")) "performance overlay is available through F8"
Check ($meshRenderer.Contains("m_frameStats.drawCalls")) "mesh renderer records draw-call diagnostics"

$tireHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\TireModel.hpp"
$tireHeader = if (Test-Path $tireHeaderPath) { [IO.File]::ReadAllText($tireHeaderPath) } else { "" }
$tireCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\TireModel.cpp"
$tireCpp = if (Test-Path $tireCppPath) { [IO.File]::ReadAllText($tireCppPath) } else { "" }
Check ($tireHeader.Contains("struct TireContactInput")) "advanced tire provider has an explicit contact-input contract"
Check ($tireHeader.Contains("struct TireForceResult")) "advanced tire provider has an explicit force-output contract"
Check ($tireCpp.Contains("evaluateAdvancedRoadTire")) "advanced road-tire provider implementation exists"
Check ($tireHeader.Contains("TireProviderKind::MagicFormula62") -or $tireHeader.Contains("MagicFormula62")) "TIRE01 MF6.2 provider is part of the tire contract"
Check ($tireCpp.Contains("evaluateMagicFormula62")) "TIRE01 routes road tire evaluation through the MF6.2 core"
Check ($tireCpp.Contains("generalizedTireCurve")) "legacy generalized road-tire fallback remains isolated"
$mf62CppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\MagicFormula\MagicFormula62.cpp"
$mf62Cpp = if (Test-Path $mf62CppPath) { [IO.File]::ReadAllText($mf62CppPath) } else { "" }
$motorcycleTireCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\MotorcycleTireProfile.cpp"
$motorcycleTireCpp = if (Test-Path $motorcycleTireCppPath) { [IO.File]::ReadAllText($motorcycleTireCppPath) } else { "" }
$tireSlipCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireSlipDynamics.cpp"
$tireSlipCpp = if (Test-Path $tireSlipCppPath) { [IO.File]::ReadAllText($tireSlipCppPath) } else { "" }
$tireContactPatchCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireContactPatch.cpp"
$tireContactPatchCpp = if (Test-Path $tireContactPatchCppPath) { [IO.File]::ReadAllText($tireContactPatchCppPath) } else { "" }
$tirePropertyCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\MagicFormula\TirePropertyFile.cpp"
$tirePropertyCpp = if (Test-Path $tirePropertyCppPath) { [IO.File]::ReadAllText($tirePropertyCppPath) } else { "" }
$tirePropertyAuthoringDir = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\Authoring"
if (Test-Path $tirePropertyAuthoringDir) {
    foreach ($authoringSource in (Get-ChildItem -LiteralPath $tirePropertyAuthoringDir -Filter '*.cpp' -File | Sort-Object Name)) {
        $tirePropertyCpp += "`n" + [IO.File]::ReadAllText($authoringSource.FullName)
    }
}
$tirePropertyHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\MagicFormula\TirePropertyFile.hpp"
$tirePropertyHeader = if (Test-Path $tirePropertyHeaderPath) { [IO.File]::ReadAllText($tirePropertyHeaderPath) } else { "" }
Check ($mf62Cpp.Contains("evaluateMagicFormula62")) "TIRE01 public MF6.2 force/moment evaluator exists"
Check ($mf62Cpp.Contains("camberStiffnessNPerRad")) "TIRE01 MF6.2 evaluator exposes camber stiffness"
Check ($mf62Cpp.Contains("rollingResistanceMomentNm") -and $mf62Cpp.Contains("aligningMomentNm")) "TIRE01 MF6.2 evaluator exposes My and Mz"
Check ($motorcycleTireCpp.Contains("mcContourA") -and $motorcycleTireCpp.Contains("lateralContactOffsetM")) "TIRE01 motorcycle contour geometry is implemented"
Check ($tireSlipCpp.Contains("integrateTireSlipDynamics")) "TIRE01 transient tire relaxation is a compiled mechanism"
Check ($vehicleCpp.Contains("integrateTireSlipDynamics")) "VehicleSystem consumes modular TIRE01 slip dynamics"
Check ($tirePropertyHeader.Contains("struct TirePropertyFileData")) "TIRE02 tire property-file data contract exists"
Check ($tirePropertyCpp.Contains("parseTirePropertyFileText") -and $tirePropertyCpp.Contains("FITTYP")) "TIRE02 human-readable .tir parser exists"
Check ($tirePropertyCpp.Contains("MC_CONTOUR_A") -and $tirePropertyCpp.Contains("MC_CONTOUR_B")) "TIRE02 .tir importer recognizes motorcycle contour data"
Check ($tirePropertyCpp.Contains("unsupportedAssignmentCount")) "TIRE02 keeps unsupported future .tir parameters explicit"
Check ($tirePropertyCpp.Contains("resetImportedCoefficientDefaults")) "TIRE02 imported .tir files do not inherit synthetic MF seed coefficients"
Check ($tirePropertyCpp.Contains("hasCoreForceCoefficients") -and $tirePropertyCpp.Contains("PKY4")) "TIRE02 rejects incomplete core force datasets instead of silently hybridizing them"
$tireAuthoringFiles = @(
    "TirePropertyParsing.cpp",
    "TirePropertyMapping.cpp",
    "TirePropertyMagicFormula.cpp",
    "TirePropertyCommonMetadata.cpp",
    "TirePropertyHeritageExtensions.cpp",
    "TirePropertyStructuralMetadata.cpp",
    "TirePropertyDiagnostics.cpp",
    "TirePropertyImportInternal.hpp",
    "TirePartDefinition.cpp",
    "TirePartDefinition.hpp",
    "TireFamilyBaseline.cpp",
    "TireFamilyBaseline.hpp",
    "TirePerformanceBiasMapping.cpp",
    "TirePerformanceBiasMapping.hpp",
    "TirePartResolver.cpp",
    "TirePartResolver.hpp"
)
$allTireAuthoringFilesExist = $true
foreach ($file in $tireAuthoringFiles) {
    if (-not (Test-Path (Join-Path $tirePropertyAuthoringDir $file))) { $allTireAuthoringFilesExist = $false }
}
Check $allTireAuthoringFilesExist "CLEAN11 tire authoring responsibility files exist"
$tirePartHeaderPath = Join-Path $tirePropertyAuthoringDir "TirePartDefinition.hpp"
$tirePartHeader = if (Test-Path $tirePartHeaderPath) { [IO.File]::ReadAllText($tirePartHeaderPath) } else { "" }
Check ($tirePartHeader.Contains("struct TirePartDefinition") -and $tirePartHeader.Contains("struct TirePerformanceBias")) "CLEAN11 reusable tire-part authoring contract exists"
Check ($tirePartHeader.Contains("snowIce") -and $tirePartHeader.Contains("mud") -and $tirePartHeader.Contains("sand") -and $tirePartHeader.Contains("gravel") -and $tirePartHeader.Contains("wearEndurance")) "CLEAN11 tire-part bias contract covers road, winter and loose/deformable surfaces"
Check ($tirePartHeader.Contains("NOT final-force multipliers")) "CLEAN11 creator biases are documented as parameter-generation inputs rather than force multipliers"
$tireFamilyBaselineHeaderPath = Join-Path $tirePropertyAuthoringDir "TireFamilyBaseline.hpp"
$tireFamilyBaselineCppPath = Join-Path $tirePropertyAuthoringDir "TireFamilyBaseline.cpp"
$tireFamilyBaselineHeader = if (Test-Path $tireFamilyBaselineHeaderPath) { [IO.File]::ReadAllText($tireFamilyBaselineHeaderPath) } else { "" }
$tireFamilyBaselineCpp = if (Test-Path $tireFamilyBaselineCppPath) { [IO.File]::ReadAllText($tireFamilyBaselineCppPath) } else { "" }
Check ($tirePartHeader.Contains("enum class TireFamily") -and $tirePartHeader.Contains("RoadSummerPerformance") -and $tirePartHeader.Contains("RacingSlick") -and $tirePartHeader.Contains("RacingWet") -and $tirePartHeader.Contains("StuddedIce") -and $tirePartHeader.Contains("RallyGravel") -and $tirePartHeader.Contains("Motorcycle") -and $tirePartHeader.Contains("CommercialTruck") -and $tirePartHeader.Contains("LowPressureOffRoad")) "TIRE17A reusable tire-part taxonomy covers road, racing, winter, gravel, motorcycle, kart, truck and low-pressure families"
Check ($tireFamilyBaselineHeader.Contains("TireFamilyBaselineInput") -and $tireFamilyBaselineHeader.Contains("buildTireFamilyBaseline") -and $tireFamilyBaselineCpp.Contains("heritage_estimated_family_baseline") -and $tireFamilyBaselineCpp.Contains("parameterConfidence = 0.15") -and $tireFamilyBaselineCpp.Contains("treadVoidRatio") -and $tireFamilyBaselineCpp.Contains("winterCompoundEffectiveness") -and $tireFamilyBaselineCpp.Contains("flotationCoupling")) "TIRE17A generates explicit low-confidence family baselines from dimensions/load/pressure and mechanism traits rather than brand assumptions"
$tireBiasMappingHeaderPath = Join-Path $tirePropertyAuthoringDir "TirePerformanceBiasMapping.hpp"
$tireBiasMappingCppPath = Join-Path $tirePropertyAuthoringDir "TirePerformanceBiasMapping.cpp"
$tireBiasMappingHeader = if (Test-Path $tireBiasMappingHeaderPath) { [IO.File]::ReadAllText($tireBiasMappingHeaderPath) } else { "" }
$tireBiasMappingCpp = if (Test-Path $tireBiasMappingCppPath) { [IO.File]::ReadAllText($tireBiasMappingCppPath) } else { "" }
Check ($tireBiasMappingHeader.Contains("kTirePerformanceBiasMappingVersion") -and $tireBiasMappingHeader.Contains("applyTirePerformanceBias") -and $tireBiasMappingCpp.Contains("applyWetBias") -and $tireBiasMappingCpp.Contains("applySnowIceBias") -and $tireBiasMappingCpp.Contains("applyMudBias") -and $tireBiasMappingCpp.Contains("applySandBias") -and $tireBiasMappingCpp.Contains("applyGravelBias") -and $tireBiasMappingCpp.Contains("applyWearEnduranceBias")) "TIRE17B maps creator biases into coherent dry/wet/winter/terrain/wear mechanisms rather than final-force multipliers"
Check ($tireBiasMappingCpp.Contains("baseline.importedPropertyFile") -and $tireBiasMappingCpp.Contains("authoritativeDataPreserved") -and $tireBiasMappingCpp.Contains("never invents physical studs")) "TIRE17B preserves fitted tire authority and does not synthesize explicit stud hardware from a simple bias"
$tirePartResolverHeaderPath = Join-Path $tirePropertyAuthoringDir "TirePartResolver.hpp"
$tirePartResolverCppPath = Join-Path $tirePropertyAuthoringDir "TirePartResolver.cpp"
$tirePartResolverHeader = if (Test-Path $tirePartResolverHeaderPath) { [IO.File]::ReadAllText($tirePartResolverHeaderPath) } else { "" }
$tirePartResolverCpp = if (Test-Path $tirePartResolverCppPath) { [IO.File]::ReadAllText($tirePartResolverCppPath) } else { "" }
Check ($tirePartHeader.Contains("struct TirePartEngineeringData") -and $tirePartHeader.Contains("referenceInflationPressurePa") -and $tirePartResolverHeader.Contains("TirePartFitment") -and $tirePartResolverHeader.Contains("TirePartResolutionSource") -and $tirePartResolverHeader.Contains("resolveTirePart")) "TIRE17C reusable tire parts carry engineering inputs and resolve through an explicit runtime fitment contract"
Check ($tirePartResolverCpp.Contains("AuthoritativePropertyFile") -and $tirePartResolverCpp.Contains("EstimatedFamilyBaseline") -and $tirePartResolverCpp.Contains("coldInflationPressurePa") -and $tirePartResolverCpp.Contains("heritage_estimated_tire_part")) "TIRE17C resolves authoritative property files or family estimates and keeps cold inflation pressure vehicle-fitment owned"
Check ($vehicleCpp.Contains("assignWheelTirePart") -and $vehicleCpp.Contains("wheelTirePartAssignment") -and $vehicleCpp.Contains("tirePartAssignment = {}")) "TIRE17C VehicleSystem assigns reusable tire parts per wheel and clears stale identity after low-level tire overrides"
Check ($vehicleCpp.Contains("loadWheelTirePropertyFile")) "VehicleSystem can apply imported tire property data per wheel"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Vehicle", "LoadWheelTirePropertyFile"')) "Lua exposes TIRE02 per-wheel property-file loading"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Vehicle", "GetWheelTireParameterInfo"')) "Lua exposes TIRE02 imported tire provenance readback"
$tireFrontDataPath = Join-Path $Root "Modules\RacingUnited\Data\Tires\PrototypeRoadFront_MF62.tir"
$tireRearDataPath = Join-Path $Root "Modules\RacingUnited\Data\Tires\PrototypeRoadRear_MF62.tir"
Check ((Test-Path $tireFrontDataPath) -and (Test-Path $tireRearDataPath)) "Racing United carries explicit TIRE02 prototype .tir datasets"
$tireFrontData = if (Test-Path $tireFrontDataPath) { [IO.File]::ReadAllText($tireFrontDataPath) } else { "" }
$tireRearData = if (Test-Path $tireRearDataPath) { [IO.File]::ReadAllText($tireRearDataPath) } else { "" }
Check ($mf62Cpp.Contains("pDxP1") -and $mf62Cpp.Contains("normalizedTurnSlip")) "TIRE03 MF6.2 evaluator consumes turn-slip coefficients"
Check ($tirePropertyCpp.Contains('"TURNSLIP_COEFFICIENTS"') -and $tirePropertyCpp.Contains('"QCRP1"')) "TIRE03 .tir importer maps turn-slip coefficients"
Check ($tireSlipCpp.Contains("magicFormulaLongitudinalRelaxationLengthM") -and $tireSlipCpp.Contains("magicFormulaLateralRelaxationLengthM")) "TIRE03 transient path derives MF relaxation lengths"
Check ($tireContactPatchCpp.Contains("integrateTireContactPatch") -and $tireContactPatchCpp.Contains("parkingTurnMomentNm")) "TIRE03 standstill contact-patch torsion mechanism exists"
Check ($vehicleCpp.Contains("integrateTireContactPatch") -and $vehicleCpp.Contains("turnSlipPerM")) "VehicleSystem integrates TIRE03 turn slip at the wheel contact"
Check ($luaRuntimeAndBindingsCpp.Contains("contactPatchTwistDegrees") -and $luaRuntimeAndBindingsCpp.Contains("turnSlipTrailReduction")) "Lua telemetry exposes TIRE03 turn-slip/contact-patch state"
$tireContactGeometryHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireContactGeometry.hpp"
$tireContactGeometryCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireContactGeometry.cpp"
$tireContactGeometryHeader = if (Test-Path $tireContactGeometryHeaderPath) { [IO.File]::ReadAllText($tireContactGeometryHeaderPath) } else { "" }
$tireContactGeometryCpp = if (Test-Path $tireContactGeometryCppPath) { [IO.File]::ReadAllText($tireContactGeometryCppPath) } else { "" }
Check ($tireContactGeometryHeader.Contains("TireContactGeometryDescription") -and $tireContactGeometryHeader.Contains("effectiveRollingRadiusM")) "TIRE04 contact-geometry provider contract exists"
Check ($tireContactGeometryCpp.Contains("qRe0") -and $tireContactGeometryCpp.Contains("std::atan") -and $tireContactGeometryCpp.Contains("effectiveRollingRadiusM")) "TIRE04 load/velocity effective-radius evaluator exists"
Check ($tireContactGeometryCpp.Contains("qRa1") -and $tireContactGeometryCpp.Contains("contactPatchLengthM") -and $tireContactGeometryCpp.Contains("contactPatchAreaM2")) "TIRE04 finite-footprint geometry evaluator exists"
Check ($tirePropertyCpp.Contains('"CONTACT_PATCH"') -and $tirePropertyCpp.Contains('"BREFF"')) "TIRE04 .tir importer maps rolling-radius/contact-patch vocabulary"
Check ($vehicleCpp.Contains("evaluateTireContactGeometry") -and $vehicleCpp.Contains("effectiveRollingRadius")) "VehicleSystem consumes TIRE04 effective radius in the high-rate wheel path"
Check ($luaRuntimeAndBindingsCpp.Contains("tireEffectiveRollingRadius") -and $luaRuntimeAndBindingsCpp.Contains("tireContactPatchArea")) "Lua telemetry exposes TIRE04 radius/footprint state"
$tireRigidRingHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireRigidRing.hpp"
$tireRigidRingCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireRigidRing.cpp"
$tireRoadEnvelopingHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireRoadEnveloping.hpp"
$tireRoadEnvelopingCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireRoadEnveloping.cpp"
$tireRigidRingHeader = if (Test-Path $tireRigidRingHeaderPath) { [IO.File]::ReadAllText($tireRigidRingHeaderPath) } else { "" }
$tireRigidRingCpp = if (Test-Path $tireRigidRingCppPath) { [IO.File]::ReadAllText($tireRigidRingCppPath) } else { "" }
$tireRoadEnvelopingHeader = if (Test-Path $tireRoadEnvelopingHeaderPath) { [IO.File]::ReadAllText($tireRoadEnvelopingHeaderPath) } else { "" }
$tireRoadEnvelopingCpp = if (Test-Path $tireRoadEnvelopingCppPath) { [IO.File]::ReadAllText($tireRoadEnvelopingCppPath) } else { "" }
Check ($tireRigidRingHeader.Contains("TireRigidRingDescription") -and $tireRigidRingHeader.Contains("TireRigidRingState")) "TIRE05 rigid-ring provider owns explicit description/state"
Check ($tireRigidRingCpp.Contains("advanceTireRigidRing") -and $tireRigidRingCpp.Contains("advanceSecondOrder")) "TIRE05 rigid-ring provider advances damped structural modes"
Check ($tireRoadEnvelopingHeader.Contains("TireRoadEnvelopingDescription") -and $tireRoadEnvelopingHeader.Contains("TireRoadEnvelopeSample")) "TIRE05 road-enveloping provider owns explicit sample/result contracts"
Check ($tireRoadEnvelopingCpp.Contains("evaluateTireRoadEnvelope") -and $tireRoadEnvelopingCpp.Contains("superEllipseSag")) "TIRE05 tandem-cam-inspired road-enveloping evaluator exists"
Check ($tirePropertyCpp.Contains('"STRUCTURAL"') -and $tirePropertyCpp.Contains('"ELLIPS_LENGTH"') -and $tirePropertyCpp.Contains('"BELT_MASS"')) "TIRE05 .tir importer maps public structural/enveloping vocabulary"
Check ($vehicleCpp.Contains("advanceTireRigidRing") -and $vehicleCpp.Contains("evaluateTireRoadEnvelope") -and $vehicleCpp.Contains("expectedPlaneHeight")) "VehicleSystem integrates TIRE05 structure/enveloping and removes smooth local road plane"
Check ($luaRuntimeAndBindingsCpp.Contains("tireEnvelopeRoadOffset") -and $luaRuntimeAndBindingsCpp.Contains("tireRingRadialVelocity")) "Lua telemetry exposes TIRE05 road-envelope/rigid-ring state"
Check ($tireRoadEnvelopingHeader.Contains("adaptive2D") -and $tireRoadEnvelopingHeader.Contains("maximumAxisSamples") -and $tireRoadEnvelopingHeader.Contains("effectiveCrossSlopeRadians")) "TIRE06 road-enveloping contract supports bounded adaptive 2D footprint state"
Check ($tireRoadEnvelopingCpp.Contains("buildTireRoadEnvelopeSamplePattern") -and $tireRoadEnvelopingCpp.Contains("tireRoadEnvelopeNeedsHeightRefinement") -and $tireRoadEnvelopingCpp.Contains("tireRoadEnvelopeHasPartialSupport") -and $tireRoadEnvelopingCpp.Contains("roadEnvelopeLateralHalfSpanM")) "TIRE06 builds/refines the bounded 2D footprint sample pattern"
Check ($tireRigidRingHeader.Contains("yawAngleRadians") -and $tireRigidRingHeader.Contains("windupAngleRadians") -and $tireRigidRingHeader.Contains("beltPolarInertiaKgM2")) "TIRE06 rigid-ring contract owns yaw/wind-up state and belt rotational inertia"
Check ($tireRigidRingCpp.Contains("input.aligningMomentNm") -and $tireRigidRingCpp.Contains("input.longitudinalReactionMomentNm") -and $tireRigidRingCpp.Contains("state.windupAngleRadians")) "TIRE06 rigid-ring provider advances yaw/wind-up modes from tire moments"
Check ($vehicleCpp.Contains("cachedFootprintSurfaceValid") -and $vehicleCpp.Contains("cachedFootprintFrictionMultiplier") -and $vehicleCpp.Contains("cachedFootprintRefined") -and $vehicleCpp.Contains("rigidRingOutput.windupAngularVelocityRadPerS")) "VehicleSystem integrates TIRE06 adaptive footprint surface blending and rotational ring state"
Check ($luaRuntimeAndBindingsCpp.Contains("tireEnvelopeCrossSlopeDegrees") -and $luaRuntimeAndBindingsCpp.Contains("tireFootprintSupportedFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireFootprintRefined") -and $luaRuntimeAndBindingsCpp.Contains("tireRingYawDegrees") -and $luaRuntimeAndBindingsCpp.Contains("tireRingWindupDegrees")) "Lua telemetry exposes TIRE06 adaptive-footprint/ring-rotation state"
$tireThermalHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireThermal.hpp"
$tireThermalCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireThermal.cpp"
$tireThermalHeader = if (Test-Path $tireThermalHeaderPath) { [IO.File]::ReadAllText($tireThermalHeaderPath) } else { "" }
$tireThermalCpp = if (Test-Path $tireThermalCppPath) { [IO.File]::ReadAllText($tireThermalCppPath) } else { "" }
Check ($tireThermalHeader.Contains("TireThermalDescription") -and $tireThermalHeader.Contains("TireThermalState") -and $tireThermalHeader.Contains("TireThermalOutput")) "TIRE07 thermal provider owns explicit description/state/output contracts"
Check ($tireThermalCpp.Contains("advanceTireThermal") -and $tireThermalCpp.Contains("idealGasGaugePressurePa") -and $tireThermalCpp.Contains("slipDissipationWatts")) "TIRE07 thermal provider advances energy state and ideal-gas pressure"
Check ($tirePropertyCpp.Contains('"HERITAGE_THERMAL"') -and $tirePropertyHeader.Contains("hasHeritageThermalModel")) "TIRE07 .tir data layer recognizes explicit Heritage thermal metadata without claiming proprietary T&V parity"
Check ($vehicleCpp.Contains("dynamicInflationPressurePa") -and $vehicleCpp.Contains("thermalBefore.frictionScale") -and $vehicleCpp.Contains("advanceTireThermal")) "VehicleSystem integrates TIRE07 pressure/grip feedback and high-rate thermal state"
Check ($luaRuntimeAndBindingsCpp.Contains("tireTreadTemperatureC") -and $luaRuntimeAndBindingsCpp.Contains("tireInflationPressurePa") -and $luaRuntimeAndBindingsCpp.Contains("tireSlipDissipationWatts")) "Lua telemetry exposes TIRE07 thermal/pressure/energy state"
$tireWearHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWear.hpp"
$tireWearCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWear.cpp"
$tireWearHeader = if (Test-Path $tireWearHeaderPath) { [IO.File]::ReadAllText($tireWearHeaderPath) } else { "" }
$tireWearCpp = if (Test-Path $tireWearCppPath) { [IO.File]::ReadAllText($tireWearCppPath) } else { "" }
Check ($tireWearHeader.Contains("kTireTreadSectorCount = 16") -and $tireWearHeader.Contains("kTireTreadBandCount = 3") -and $tireWearHeader.Contains("TireWearState")) "TIRE08 owns a bounded 16x3 spatial tread state"
Check ($tireWearCpp.Contains("advanceTireWear") -and $tireWearCpp.Contains("surfaceTemperatureOffsetC") -and $tireWearCpp.Contains("flatSpotDepthM")) "TIRE08 advances local surface temperature, tread wear and flat-spot history"
Check ($tirePropertyCpp.Contains('"HERITAGE_TREAD_STATE"') -and $tirePropertyHeader.Contains("hasHeritageTreadState")) "TIRE08 .tir data layer recognizes explicit Heritage spatial tread metadata"
Check ($vehicleCpp.Contains("wearBefore.contactFrictionScale") -and $vehicleCpp.Contains("advanceTireWear") -and $vehicleCpp.Contains("tireTreadContactSector")) "VehicleSystem integrates TIRE08 spatial tread feedback and telemetry"
Check ($luaRuntimeAndBindingsCpp.Contains("tireTreadInsideSurfaceTemperatureC") -and $luaRuntimeAndBindingsCpp.Contains("tireTreadWearFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireFlatSpotDepthMm")) "Lua telemetry exposes TIRE08 spatial tread temperature/wear state"
Check ($tireWearHeader.Contains("flatSpotSector") -and $tireWearCpp.Contains("out.flatSpotSector") -and $vehicleCpp.Contains("tireFlatSpotSector")) "TIRE09 preserves the deepest circumferential wear sector for material-fixed visual flat spots"
Check ($tireWearHeader.Contains("contactTreadRadiusLossM") -and $tireWearCpp.Contains("out.contactRadiusVariationM") -and $vehicleCpp.Contains("physicalSupportRadiusM") -and $vehicleCpp.Contains("hit.distance - physicalSupportRadiusM") -and $vehicleCpp.Contains("nominalUnloadedRadiusM - currentTreadRadiusLossM")) "TIRE10 couples spatial tread loss into physical support/contact rolling radius"
$tireSurfaceHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireSurfaceInteraction.hpp"
$tireSurfaceCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireSurfaceInteraction.cpp"
$tireSurfaceHeader = if (Test-Path $tireSurfaceHeaderPath) { [IO.File]::ReadAllText($tireSurfaceHeaderPath) } else { "" }
$tireSurfaceCpp = if (Test-Path $tireSurfaceCppPath) { [IO.File]::ReadAllText($tireSurfaceCppPath) } else { "" }
Check ($tireWearHeader.Contains("organicContamination") -and $tireWearHeader.Contains("mineralContamination") -and $tireWearHeader.Contains("gravelFinesContamination") -and $tireWearHeader.Contains("rubberPickupContamination") -and $tireWearHeader.Contains("mudFilmContamination")) "TIRE11 stores five persistent contamination channels in each 16x3 tread cell"
Check ($tireWearHeader.Contains("TireTreadContactWeights") -and $tireWearHeader.Contains("tireTreadContactWeights")) "TIRE11 exports one shared 48-cell contact weighting contract for wear/surface state"
Check ($tireSurfaceHeader.Contains("TireContaminationDescription") -and $tireSurfaceHeader.Contains("TireContaminationInput") -and $tireSurfaceHeader.Contains("TireContaminationOutput") -and $tireSurfaceHeader.Contains("footprintSurfaceBlendValid")) "TIRE11 contamination provider owns explicit description/input/output contracts and TIRE06 footprint blending"
Check ($tireSurfaceCpp.Contains("advanceTireContamination") -and $tireSurfaceCpp.Contains("SurfaceMaterial::Grass") -and $tireSurfaceCpp.Contains("SurfaceMaterial::Dirt") -and $tireSurfaceCpp.Contains("SurfaceMaterial::Gravel") -and $tireSurfaceCpp.Contains("cleanHardFraction") -and $tireSurfaceCpp.Contains("cleaningRate(") -and $tireSurfaceCpp.Contains("cleanChannel(")) "TIRE11 implements material-specific pickup and progressive clean-hard-surface release"
Check ($tirePropertyCpp.Contains('"HERITAGE_CONTAMINATION"') -and $tirePropertyHeader.Contains("hasHeritageContaminationModel")) "TIRE11 .tir data layer recognizes explicit Heritage contamination metadata"
Check ($vehicleCpp.Contains("contaminationBefore.contactFrictionScale") -and $vehicleCpp.Contains("contaminationBefore.rollingResistanceScale") -and $vehicleCpp.Contains("thermalInput.roadHeatTransferScale") -and $vehicleCpp.Contains("advanceTireContamination") -and $vehicleCpp.Contains("cachedFootprintGrassFraction") -and $vehicleCpp.Contains("footprintSurfaceBlendValid")) "VehicleSystem integrates TIRE11 adaptive-footprint pickup plus grip/rolling/thermal feedback and persistent tread state"
Check ($luaRuntimeAndBindingsCpp.Contains("tireContaminationFrictionScale") -and $luaRuntimeAndBindingsCpp.Contains("tireRubberPickupContamination") -and $luaRuntimeAndBindingsCpp.Contains("tireContaminationCleaningRate")) "Lua telemetry exposes TIRE11 contamination and cleaning state"
$tireWetHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWetSurfaceInteraction.hpp"
$tireWetCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWetSurfaceInteraction.cpp"
$tireWetHeader = if (Test-Path $tireWetHeaderPath) { [IO.File]::ReadAllText($tireWetHeaderPath) } else { "" }
$tireWetCpp = if (Test-Path $tireWetCppPath) { [IO.File]::ReadAllText($tireWetCppPath) } else { "" }
Check ($tireWetHeader.Contains("TireWetSurfaceDescription") -and $tireWetHeader.Contains("TireWetSurfaceInput") -and $tireWetHeader.Contains("TireWetSurfaceOutput")) "TIRE12 wet-surface provider owns explicit description/input/output contracts"
Check ($tireWetCpp.Contains("evaluateTireWetSurface") -and $tireWetCpp.Contains("advanceTireWetSurface") -and $tireWetCpp.Contains("hydrodynamicLiftN") -and $tireWetCpp.Contains("drainageDemandRatio")) "TIRE12 implements continuous drainage, retained-water and hydrodynamic-load behavior"
Check ($tireWearHeader.Contains("retainedWaterFilmM")) "TIRE12 stores retained water in each existing 16x3 tread cell"
Check ($tirePropertyCpp.Contains('"HERITAGE_WET_SURFACE"') -and $tirePropertyHeader.Contains("hasHeritageWetSurfaceModel") -and $tirePropertyCpp.Contains("WETNESS_ONE_WATER_DEPTH_M") -and $tirePropertyCpp.Contains("TREAD_VOID_RATIO")) "TIRE12 .tir data layer recognizes explicit Heritage wet-surface/drainage metadata"
Check ($vehicleCpp.Contains("wetBefore.frictionScale") -and $vehicleCpp.Contains("wetBefore.hydrodynamicDragN") -and $vehicleCpp.Contains("cachedFootprintWetBaseFrictionMultiplier") -and $vehicleCpp.Contains("advanceTireWetSurface")) "VehicleSystem composes TIRE12 hard-surface wet physics around the one-MF tire path"
Check ($luaRuntimeAndBindingsCpp.Contains("tireRoadWaterDepthMm") -and $luaRuntimeAndBindingsCpp.Contains("tireHydroplaningFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireHydrodynamicLiftN") -and $luaRuntimeAndBindingsCpp.Contains("tireClassicalHydroplaningSpeedKph")) "Lua telemetry exposes TIRE12 wet-surface state"
$tireWinterHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWinterSurfaceInteraction.hpp"
$tireWinterCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireWinterSurfaceInteraction.cpp"
$tireWinterHeader = if (Test-Path $tireWinterHeaderPath) { [IO.File]::ReadAllText($tireWinterHeaderPath) } else { "" }
$tireWinterCpp = if (Test-Path $tireWinterCppPath) { [IO.File]::ReadAllText($tireWinterCppPath) } else { "" }
Check ($tireWinterHeader.Contains("TireWinterSurfaceDescription") -and $tireWinterHeader.Contains("TireWinterSurfaceInput") -and $tireWinterHeader.Contains("TireWinterSurfaceOutput")) "TIRE13 winter-surface provider owns explicit description/input/output contracts"
Check ($tireWinterCpp.Contains("evaluateTireWinterSurface") -and $tireWinterCpp.Contains("advanceTireWinterSurface") -and $tireWinterCpp.Contains("iceMeltFilmDepthM") -and $tireWinterCpp.Contains("studFrictionContribution") -and $tireWinterCpp.Contains("snowInterlockContribution")) "TIRE13 implements temperature/slip ice response, optional studs and compacted-snow interlock"
Check ($tireWearHeader.Contains("packedSnowFraction")) "TIRE13 stores packed snow in each existing 16x3 tread cell"
Check ($tirePropertyCpp.Contains('"HERITAGE_WINTER_SURFACE"') -and $tirePropertyHeader.Contains("hasHeritageWinterSurfaceModel") -and $tirePropertyCpp.Contains("WINTER_COMPOUND_EFFECTIVENESS") -and $tirePropertyCpp.Contains("STUD_COUNT")) "TIRE13 .tir data layer recognizes explicit winter/siping/stud metadata"
Check ($vehicleCpp.Contains("cachedFootprintSnowFraction") -and $vehicleCpp.Contains("cachedFootprintIceFraction") -and $vehicleCpp.Contains("evaluateTireWinterSurface") -and $vehicleCpp.Contains("advanceTireWinterSurface") -and $vehicleCpp.Contains("combineDedicatedSurfaceScale")) "VehicleSystem composes TIRE13 snow/ice provider around the one-MF tire path"
Check ($luaRuntimeAndBindingsCpp.Contains("tireWinterSurfaceFraction") -and $luaRuntimeAndBindingsCpp.Contains("tirePackedSnowFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireStudFrictionContribution") -and $luaRuntimeAndBindingsCpp.Contains("tireWinterSurfaceTemperatureC")) "Lua telemetry exposes TIRE13 winter surface state"
$tireShallowGranularHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireShallowGranularInteraction.hpp"
$tireShallowGranularCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireShallowGranularInteraction.cpp"
$tireShallowGranularHeader = if (Test-Path $tireShallowGranularHeaderPath) { [IO.File]::ReadAllText($tireShallowGranularHeaderPath) } else { "" }
$tireShallowGranularCpp = if (Test-Path $tireShallowGranularCppPath) { [IO.File]::ReadAllText($tireShallowGranularCppPath) } else { "" }
Check ($tireShallowGranularHeader.Contains("TireShallowGranularDescription") -and $tireShallowGranularHeader.Contains("TireShallowGranularInput") -and $tireShallowGranularHeader.Contains("TireShallowGranularOutput")) "TIRE14 shallow-granular provider owns explicit description/input/output contracts"
Check ($tireShallowGranularCpp.Contains("evaluateTireShallowGranular") -and $tireShallowGranularCpp.Contains("sinkageModulusPaPerMExponent") -and $tireShallowGranularCpp.Contains("maximumShearStress") -and $tireShallowGranularCpp.Contains("longitudinalShearMobilization") -and $tireShallowGranularCpp.Contains("lateralBulldozingForceN") -and $tireShallowGranularCpp.Contains("plowingDragN")) "TIRE14 implements bounded sinkage, granular shear mobilization, bulldozing and plowing/compaction"
Check ($tirePropertyCpp.Contains('"HERITAGE_SHALLOW_GRANULAR"') -and $tirePropertyHeader.Contains("hasHeritageShallowGranularModel") -and $tirePropertyCpp.Contains("TREAD_AGGRESSIVENESS") -and $tirePropertyCpp.Contains("OPEN_VOID_RATIO") -and $tirePropertyCpp.Contains("GRANULAR_SHEAR_COUPLING")) "TIRE14 .tir data layer recognizes tire-side shallow-granular tread traits"
Check ($vehicleCpp.Contains("shallowGranularSurfaceMaterial") -and $vehicleCpp.Contains("cachedFootprintGravelFraction") -and $vehicleCpp.Contains("cachedFootprintDirtFraction") -and $vehicleCpp.Contains("granularSinkageM") -and $vehicleCpp.Contains("evaluateTireShallowGranular") -and $vehicleCpp.Contains("shallowGranularBefore.longitudinalShearForceN") -and $vehicleCpp.Contains("shallowGranularBefore.lateralBulldozingForceN") -and $vehicleCpp.Contains("shallowGranularBefore.plowingDragN")) "VehicleSystem composes TIRE14 physical support/sinkage and granular forces around the one-MF tire path"
Check ($luaRuntimeAndBindingsCpp.Contains("tireGranularSurfaceFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireGranularSinkageMm") -and $luaRuntimeAndBindingsCpp.Contains("tireGranularShearCapacityN") -and $luaRuntimeAndBindingsCpp.Contains("tireGranularBulldozingN") -and $luaRuntimeAndBindingsCpp.Contains("tireGranularCompactionPowerW")) "Lua telemetry exposes TIRE14 shallow-granular state"
$tireDeformableTerrainHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireDeformableTerrainInteraction.hpp"
$tireDeformableTerrainCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireDeformableTerrainInteraction.cpp"
$tireDeformableTerrainHeader = if (Test-Path $tireDeformableTerrainHeaderPath) { [IO.File]::ReadAllText($tireDeformableTerrainHeaderPath) } else { "" }
$tireDeformableTerrainCpp = if (Test-Path $tireDeformableTerrainCppPath) { [IO.File]::ReadAllText($tireDeformableTerrainCppPath) } else { "" }
$surfaceFieldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceField.hpp"
$surfaceFieldCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceField.cpp"
$surfaceWorldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.hpp"
$surfaceWorldCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.cpp"
$surfaceMaterialPropertiesHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceMaterialProperties.hpp"
$surfaceMaterialPropertiesCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceMaterialProperties.cpp"
$staticTriangleSceneImporterPath = Join-Path $Root "Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp"
$surfaceFieldCompatHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\SurfaceField.hpp"
$surfaceFieldCompatCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\SurfaceField.cpp"
$trackRubberHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Rubber\TrackRubberState.hpp"
$trackRubberCppPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Rubber\TrackRubberState.cpp"
$trackRubberRegressionPath = Join-Path $Root "Engine\HeritageEngine\Tests\TrackRubberRegression.cpp"
$surfaceFieldHeader = if (Test-Path $surfaceFieldHeaderPath) { [IO.File]::ReadAllText($surfaceFieldHeaderPath) } else { "" }
$surfaceFieldCpp = if (Test-Path $surfaceFieldCppPath) { [IO.File]::ReadAllText($surfaceFieldCppPath) } else { "" }
$surfaceWorldHeader = if (Test-Path $surfaceWorldHeaderPath) { [IO.File]::ReadAllText($surfaceWorldHeaderPath) } else { "" }
$surfaceWorldCpp = if (Test-Path $surfaceWorldCppPath) { [IO.File]::ReadAllText($surfaceWorldCppPath) } else { "" }
$surfaceMaterialPropertiesHeader = if (Test-Path $surfaceMaterialPropertiesHeaderPath) { [IO.File]::ReadAllText($surfaceMaterialPropertiesHeaderPath) } else { "" }
$surfaceMaterialPropertiesCpp = if (Test-Path $surfaceMaterialPropertiesCppPath) { [IO.File]::ReadAllText($surfaceMaterialPropertiesCppPath) } else { "" }
$staticTriangleSceneImporter = if (Test-Path $staticTriangleSceneImporterPath) { [IO.File]::ReadAllText($staticTriangleSceneImporterPath) } else { "" }
$surfaceFieldCompatHeader = if (Test-Path $surfaceFieldCompatHeaderPath) { [IO.File]::ReadAllText($surfaceFieldCompatHeaderPath) } else { "" }
$trackRubberHeader = if (Test-Path $trackRubberHeaderPath) { [IO.File]::ReadAllText($trackRubberHeaderPath) } else { "" }
$trackRubberCpp = if (Test-Path $trackRubberCppPath) { [IO.File]::ReadAllText($trackRubberCppPath) } else { "" }
$trackRubberRegression = if (Test-Path $trackRubberRegressionPath) { [IO.File]::ReadAllText($trackRubberRegressionPath) } else { "" }
Check ($tireDeformableTerrainHeader.Contains("TireDeformableTerrainDescription") -and $tireDeformableTerrainHeader.Contains("TireDeformableTerrainInput") -and $tireDeformableTerrainHeader.Contains("TireDeformableTerrainOutput")) "TIRE15 deformable-terrain provider owns explicit description/input/output contracts"
Check ($tireDeformableTerrainCpp.Contains("evaluateTireDeformableTerrain") -and $tireDeformableTerrainCpp.Contains("pressureModulus") -and $tireDeformableTerrainCpp.Contains("maximumShearStress") -and $tireDeformableTerrainCpp.Contains("longitudinalShearMobilization") -and $tireDeformableTerrainCpp.Contains("passiveWedge") -and $tireDeformableTerrainCpp.Contains("tireDeformableTerrainFieldUpdate")) "TIRE15 implements pressure-sinkage, shear mobilization, bulldozing/plowing and persistent field updates"
Check ($surfaceFieldHeader.Contains("class SurfaceField") -and $surfaceFieldHeader.Contains("rutDepthM") -and $surfaceFieldHeader.Contains("compaction") -and $surfaceFieldHeader.Contains("moisture") -and $surfaceFieldHeader.Contains("passCount")) "TIRE15 SurfaceField owns bounded shared rut/compaction/moisture/shear state"
Check ($surfaceFieldHeader.Contains("chunkSizeCells") -and $surfaceFieldHeader.Contains("maximumResidentChunkCount") -and $surfaceFieldHeader.Contains("verticalLayerSizeM") -and $surfaceFieldHeader.Contains("SurfaceFieldChunkSnapshot") -and $surfaceFieldHeader.Contains("ChunkEvictionCallback")) "CLEAN10 SurfaceField exposes chunked bounded 3D-layer-aware storage plus streaming/persistence seams"
Check ($surfaceFieldCpp.Contains("m_chunkLru") -and $surfaceFieldCpp.Contains("evictLeastRecentlyUsedChunk") -and $surfaceFieldCpp.Contains("passProgress") -and -not $surfaceFieldCpp.Contains("evictOldestIfNeeded")) "CLEAN10 SurfaceField uses bounded chunk LRU eviction without full-field oldest-cell scans"
Check ($surfaceFieldHeader.Contains("heritage::math::DVec3") -and $surfaceFieldHeader.Contains("verticalLayer") -and $surfaceFieldCpp.Contains("quantizedCellCoordinate") -and $surfaceFieldCpp.Contains("globalPosition.y")) "CLEAN10 SurfaceField keys dynamic state from FP64 global coordinates and separates stacked surfaces"
Check ($tirePropertyCpp.Contains('"HERITAGE_DEFORMABLE_TERRAIN"') -and $tirePropertyHeader.Contains("hasHeritageDeformableTerrainModel") -and $tirePropertyCpp.Contains("FLOTATION_COUPLING") -and $tirePropertyCpp.Contains("MAX_TERRAIN_FORCE_RATIO")) "TIRE15 .tir data layer recognizes tire-side deformable-terrain traits"
Check ($tireFrontData.Contains("[HERITAGE_DEFORMABLE_TERRAIN]") -and $tireRearData.Contains("[HERITAGE_DEFORMABLE_TERRAIN]") -and $tireFrontData.Contains("SOIL_SHEAR_COUPLING") -and $tireRearData.Contains("FLOTATION_COUPLING")) "Racing United prototype tires carry explicit TIRE15 deformable-terrain tire-side metadata"
Check ($vehicleCpp.Contains("deformableTerrainSurfaceMaterial") -and $vehicleCpp.Contains("cachedFootprintMudFraction") -and $vehicleCpp.Contains("cachedFootprintSandFraction") -and $vehicleCpp.Contains("cachedFootprintSoftSoilFraction") -and $vehicleCpp.Contains("cachedFootprintDeepSnowFraction") -and $vehicleCpp.Contains("surfaces.sampleDeformable") -and $vehicleCpp.Contains("surfaces.applyDeformable") -and $vehicleCpp.Contains("deformableTerrainBefore.longitudinalTerrainForceN") -and $vehicleCpp.Contains("deformableTerrainBefore.lateralBulldozingForceN")) "VehicleSystem composes TIRE15 terrain-primary forces through world-owned SurfaceWorld state"
Check (-not $vehicleHeader.Contains("m_surfaceField") -and $vehicleHeader.Contains("SurfaceWorld& surfaces")) "CLEAN10 VehicleSystem consumes world surface state instead of owning a private SurfaceField"
Check ($surfaceWorldHeader.Contains("class SurfaceWorld") -and $surfaceWorldHeader.Contains("sampleDeformable") -and $surfaceWorldHeader.Contains("applyDeformable") -and $surfaceWorldCpp.Contains("localToGlobal")) "CLEAN10 SurfaceWorld owns the local-FP32 to global-FP64 driven-surface boundary"
Check ($physicsWorldHeader.Contains("SurfaceWorld m_surfaces") -and $physicsWorldHeader.Contains("SurfaceWorld& surfaces()") -and $physicsWorldCpp.Contains("m_surfaces.setGlobalOrigin(m_globalOrigin)")) "CLEAN10 PhysicsWorld owns SurfaceWorld and keeps it synchronized with floating-origin changes"
Check ($surfaceFieldCompatHeader.Contains("Surfaces/SurfaceField.hpp") -and -not (Test-Path $surfaceFieldCompatCppPath)) "OPT01 legacy Physics/SurfaceField keeps only the compatibility include header; dead cpp signpost is retired"
Check ($trackRubberHeader.Contains("class TrackRubberState") -and $trackRubberHeader.Contains("TrackRubberContactInput") -and $trackRubberHeader.Contains("TrackRubberVisualCell") -and $trackRubberCpp.Contains("applyContact") -and $trackRubberCpp.Contains("collectPresentationCells")) "TIRE15C dedicated TrackRubberState owns bounded deposited-rubber/marble state instead of deformable terrain"
Check ($surfaceMaterialPropertiesHeader.Contains("SurfaceDeformableProperties") -and $surfaceMaterialPropertiesHeader.Contains("SurfaceMaterialProperties") -and $surfaceMaterialPropertiesCpp.Contains("defaultSurfaceMaterialProperties") -and $surfaceMaterialPropertiesCpp.Contains("blendSurfaceDeformableProperties")) "TIRE15B1 world surface subsystem owns authored deformable material properties and finite-footprint blending"
Check ($staticTriangleSceneImporter.Contains('heritage.surface.bekker_kc') -and $staticTriangleSceneImporter.Contains('heritage.surface.cohesion_pa') -and $staticTriangleSceneImporter.Contains('heritage.surface.temperature_c') -and $staticTriangleSceneImporter.Contains("applySurfacePropertyMetadata")) "TIRE15B1 GLB collision authoring imports physical surface parameters and local road temperature"
Check ($surfaceWorldHeader.Contains("SurfaceWorldEnvironment") -and $surfaceWorldHeader.Contains("SurfaceLocalConditions") -and $surfaceWorldHeader.Contains("setEnvironment") -and $surfaceWorldCpp.Contains("localConditions")) "TIRE15B1 SurfaceWorld resolves live weather plus authored local surface conditions"
Check ($vehicleCpp.Contains("hitSurfaceConditions") -and $vehicleCpp.Contains("surfacePropertiesValid") -and $vehicleCpp.Contains("cachedFootprintAverageSurfaceTemperatureC") -and $vehicleCpp.Contains("supportTerrainProperties")) "TIRE15B1 wheel simulation consumes effective wetness, road temperature and authored terrain mechanics"
Check ($tireThermalHeader.Contains("environmentTemperatureOverride") -and $tireThermalCpp.Contains("input.ambientTemperatureC") -and $tireThermalCpp.Contains("input.roadTemperatureC")) "TIRE15B1 tire thermal state accepts live world air/road temperature without removing tire-file fallbacks"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetSurfaceEnvironment"') -and $luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "SetSurfaceEnvironment"')) "TIRE15B1 Lua API exposes bounded live world-surface weather controls"

$surfacePresentationHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Presentation\SurfacePresentation.hpp")
$surfacePresentationCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Presentation\SurfacePresentation.cpp")
$surfacePresentationRendererHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.hpp")
$surfacePresentationRendererCpp = (@(
    "SurfacePresentationRenderer.cpp",
    "SurfacePresentationTireMarks.cpp",
    "SurfacePresentationRubber.cpp",
    "SurfacePresentationShaders.cpp",
    "SurfacePresentationRendererInternal.hpp"
) | ForEach-Object { ReadText (Join-Path $Root ("Engine\HeritageEngine\Graphics\Renderer\" + $_)) }) -join "`n"
$tireMarkChunkingHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\TireMarkChunking.hpp")
$shaderProgramHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\ShaderProgram.hpp")
$shaderProgramCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\ShaderProgram.cpp")
$engineSimulationCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineSimulation.cpp")
$engineRenderingCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRendering.cpp")
$wheelPhysicalStatePhase = ReadText (Join-Path $Root "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\09_TirePhysicalStateUpdate.inl")
$surfacePresentationRegression = ReadText (Join-Path $Root "Engine\HeritageEngine\Tests\SurfacePresentationRegression.cpp")
Check ($surfacePresentationHeader.Contains("class SurfacePresentation") -and $surfacePresentationHeader.Contains("kMaximumTrackMarks = 8192") -and $surfacePresentationHeader.Contains("kMaximumParticles = 2048") -and $surfacePresentationHeader.Contains("SurfacePresentationAudioMix")) "TIRE15B2 surface presentation owns bounded track-mark, particle and audio-mechanism state"
Check ($surfacePresentationCpp.Contains("recordContact") -and $surfacePresentationCpp.Contains("trackKey") -and $surfacePresentationCpp.Contains("globalPosition") -and $surfacePresentationCpp.Contains("SurfaceParticleKind::WaterSpray") -and $surfacePresentationCpp.Contains("SurfaceParticleKind::Dust")) "TIRE15B2 presentation derives deterministic driven-surface visuals from global-addressed contact state"
Check ($surfaceWorldHeader.Contains("SurfacePresentation m_presentation") -and $surfaceWorldHeader.Contains("recordContactPresentation") -and $surfaceWorldCpp.Contains("localToGlobal(localPosition)")) "TIRE15B2 SurfaceWorld owns presentation and keeps it floating-origin safe"
Check ($wheelPhysicalStatePhase.Contains("recordContactPresentation") -and $wheelPhysicalStatePhase.Contains("presentationContact.rutDepthDeltaM") -and $wheelPhysicalStatePhase.Contains("presentationContact.displacedVolumeDeltaM3")) "TIRE15B2 wheel phase drives presentation one-way from authoritative post-physics state"
Check ($surfaceWorldHeader.Contains("rubber::TrackRubberState m_trackRubber") -and $surfaceWorldHeader.Contains("sampleTrackRubber") -and $surfaceWorldHeader.Contains("applyTrackRubberContact") -and $surfaceWorldCpp.Contains("m_trackRubber.advance")) "TIRE15C SurfaceWorld owns floating-origin-safe shared track rubber and weather ageing"
Check ($vehicleCpp.Contains("sampleTrackRubber") -and $vehicleCpp.Contains("surfaceRubberDebrisFraction") -and $vehicleCpp.Contains("trackRubberBefore.contactFrictionScale") -and $vehicleCpp.Contains("applyTrackRubberContact")) "TIRE15C wheel simulation samples rubber before forces, feeds marble pickup, then deposits shared rubber after tire-state update"
Check ($surfacePresentationRendererCpp.Contains("kMarbleGeometryShader") -and $surfacePresentationRendererCpp.Contains("MarbleCellGpuRecord") -and $surfacePresentationRendererCpp.Contains("persistentPiecePopulation")) "TIRE15C renderer procedurally presents visible tire marbles without authored marble meshes"
Check (($trackRubberCpp.Contains("Speed alone never tears off visible marbles") -or $trackRubberCpp.Contains("speed alone must never act as a marble generator")) -and $trackRubberCpp.Contains("slipTearSignal") -and $trackRubberCpp.Contains("wearSusceptibility")) "TIRE15C1/C2 loose-rubber generation is stress/wear driven rather than vehicle speed alone"
Check ($surfacePresentationRendererCpp.Contains("kMarbleGpuMaximumCellsPerPage = 8192") -and ($surfacePresentationRendererCpp -match [regex]::Escape("Keep visual slots stable as rubber state updates")) -and $surfacePresentationRendererCpp.Contains("stableSurfaceBasis") -and -not $surfacePresentationRendererCpp.Contains("cell.depositedRubber * 0.34f")) "TIRE15C1-C4 rubber presentation keeps bounded stable marbles and retires the old deposited-rubber track-mark overlay"

Check ($trackRubberHeader.Contains("marbleMaturity") -and $trackRubberCpp.Contains("Marble maturity is physical track state") -and $trackRubberCpp.Contains("compoundShedding") -and $trackRubberCpp.Contains("maturationMultiplier")) "TIRE15C2 rubber state models traffic-driven shred-to-marble maturity and explicit compound shedding propensity"
Check ($surfaceWorldHeader.Contains("SurfaceWorldDevelopmentControls") -and $surfaceWorldHeader.Contains("tireWearRateMultiplier") -and $surfaceWorldHeader.Contains("rubberGenerationMultiplier") -and $surfaceWorldHeader.Contains("marbleMaturationMultiplier")) "TIRE15C2 SurfaceWorld owns non-authored tire/rubber lab acceleration controls"
Check ($surfacePresentationRendererCpp.Contains("kMarbleGpuDrawDistanceM = 220.0") -and $surfacePresentationRendererCpp.Contains("kMarbleGpuDetailedDistanceM = 55.0") -and $surfacePresentationRendererCpp.Contains("freshLengthM") -and $surfacePresentationRendererCpp.Contains("maturity")) "TIRE15C2 renderer presents distant marble concentration and maturity-dependent procedural shapes"
Check ($surfacePresentationRendererCpp.Contains("stableSurfaceBasis") -and $surfacePresentationRendererCpp.Contains("chunkAddress(") -and $surfacePresentationRendererCpp.Contains("localFp32(") -and -not $surfacePresentationRendererCpp.Contains("cell.depositedRubber * 0.34f")) "TIRE15C3 marble geometry is world-anchored and per-cell rubber rectangles are not rendered"
Check ($trackRubberHeader.Contains("maximumPersistentPieceCount = 500000") -and $trackRubberHeader.Contains("persistentPiecePopulation") -and $trackRubberCpp.Contains("Anchor presentation geometry to the first physical support frame") -and $trackRubberCpp.Contains("freshFragmentSeverity")) "TIRE15C4 rubber state owns a 500k logical persistent piece budget, stable support anchors and event-severity fragment state"
Check ($surfacePresentationHeader.Contains("RubberShred") -and $surfacePresentationCpp.Contains("rubber flight is no longer a lossy generic presentation") -and $wheelPhysicalStatePhase.Contains("freshRubberShed")) "TIRE15C5 retires the C4 presentation-only rubber shred path while preserving fresh-shed telemetry compatibility"
Check ($trackRubberHeader.Contains("TrackRubberTransientPhase") -and $trackRubberHeader.Contains("maximumTransientPacketCount = 8192") -and $trackRubberHeader.Contains("TrackRubberWakeInput") -and $trackRubberHeader.Contains("collectTransientPresentation") -and $trackRubberCpp.Contains("enqueueFreshTransient") -and $trackRubberCpp.Contains("applyWake") -and $trackRubberCpp.Contains("transferLooseRubber")) "TIRE15C5 TrackRubberState owns bounded authoritative airborne/mobile rubber and conservative aerodynamic migration"
Check ($surfaceWorldHeader.Contains("applyTrackRubberWake") -and $surfaceWorldCpp.Contains("m_trackRubber.applyWake") -and $vehicleCpp.Contains("TrackRubberWakeInput wake") -and $vehicleCpp.Contains("applyTrackRubberWake")) "TIRE15C5 vehicle/world integration applies one analytical aggregate marble wake outside the high-rate wheel loop"
Check ($surfacePresentationRendererHeader.Contains("visibleMovingRubber") -and $surfacePresentationRendererCpp.Contains("kMovingRubberGeometryShader") -and $surfacePresentationRendererCpp.Contains("bendVertex1M") -and $surfacePresentationRendererCpp.Contains("bendVertex3M") -and $surfacePresentationRendererCpp.Contains("glDisable(GL_CULL_FACE)")) "TIRE15C5 renderer draws authoritative moving rubber as bounded two-triangle two-sided deformable flakes"
Check ($trackRubberCpp.Contains("kLooseRubberProductionCalibration = 0.360f") -and $surfacePresentationRendererCpp.Contains("representativeCount") -and $surfacePresentationRendererCpp.Contains("cameraRelativeFp32") -and $surfacePresentationRendererCpp.Contains("anchorLongitudinal") -and $surfacePresentationRendererCpp.Contains("emitFlake")) "TIRE16G keeps the C5A flake presentation and raises production 1x shedding exactly 3x over TIRE16D"
Check ($trackRubberRegression.Contains("TIRE16D") -and $trackRubberRegression.Contains("workedStats.looseGeneration < 0.135") -and $trackRubberRegression.Contains("workedStats.looseGeneration > 0.36") -and $trackRubberRegression.Contains("30000")) "TIRE16G native regression locks the 3x production marble baseline"
Check ($surfacePresentationRendererCpp.Contains("persistentPiecePopulation") -and $surfacePresentationRendererCpp.Contains("stackLayer") -and $surfacePresentationRendererCpp.Contains("severityLengthScale") -and $surfacePresentationRendererCpp.Contains("kMarbleGpuMaximumCellsPerPage = 8192")) "TIRE15C4 renderer reconstructs persistent piece populations with severity-dependent size and local pile stacking"
Check ($surfacePresentationHeader.Contains("SurfaceTireMarkSegment") -and $surfacePresentationHeader.Contains("kMaximumTireMarkSegments = 1000000") -and $surfacePresentationHeader.Contains("kTireMarkRetirementSeconds = 1200.0") -and -not $surfacePresentationHeader.Contains("kTireMarkDetailedLifetimeSeconds") -and -not $surfacePresentationHeader.Contains("kTireMarkSimplifiedLifetimeSeconds") -and $surfacePresentationHeader.Contains("std::deque<SurfaceTireMarkSegment>") -and $surfacePresentationCpp.Contains("m_tireMarkSegments.pop_front()") -and $surfacePresentationCpp.Contains("sample.widthM = std::clamp(contact.tireWidthM") -and -not $surfacePresentationCpp.Contains("contact.tireWidthM * 0.97f") -and $surfacePresentationCpp.Contains("recordTireMark") -and $surfacePresentationCpp.Contains("slipDissipationWatts") -and $surfacePresentationCpp.Contains("gripActivation") -and $surfacePresentationCpp.Contains("kinematicSlideActivation") -and $surfacePresentationCpp.Contains("preSlideTraceIntensity") -and $surfacePresentationCpp.Contains("kTargetSegmentLengthM")) "TIRE16J keeps one-million FP64 history, full authored tire-mark width and a single twenty-minute time-fade lifetime"
Check ($wheelPhysicalStatePhase.Contains("tireMarkStreamId") -and $wheelPhysicalStatePhase.Contains("insideLoadFraction") -and $wheelPhysicalStatePhase.Contains("markInsideIsPositiveRight") -and $wheelPhysicalStatePhase.Contains("slipDissipationWatts") -and $wheelPhysicalStatePhase.Contains("gripUtilization") -and $wheelPhysicalStatePhase.Contains("slipRatio") -and $wheelPhysicalStatePhase.Contains("slipAngleDegrees")) "TIRE16D wheel phase passes stable stream identity plus pressure-band, slip-work, grip and explicit kinematic-slip state to presentation"
Check ($tireMarkChunkingHeader.Contains("kChunkSizeM = 100.0") -and $tireMarkChunkingHeader.Contains("localFp32") -and $tireMarkChunkingHeader.Contains("reconstructGlobal") -and $surfacePresentationHeader.Contains("firstTireMarkSerial") -and $surfacePresentationHeader.Contains("tireMarkSegmentBySerial")) "TIRE16K uses invisible 100 m FP64-origin / FP32-local tire-mark chunks with O(1) serial access to newly appended history"
Check ($shaderProgramHeader.Contains("geometrySource") -and $shaderProgramCpp.Contains("GL_GEOMETRY_SHADER") -and $surfacePresentationRendererCpp.Contains("kTireMarkGeometryShader") -and $surfacePresentationRendererCpp.Contains("TireMarkGpuRecord") -and $surfacePresentationRendererCpp.Contains("kTireMarkGpuMaximumSegmentsPerPage = 8192") -and $surfacePresentationRendererCpp.Contains("tireMarkGpuPageCapacity") -and $surfacePresentationRendererCpp.Contains("glBufferSubData") -and $surfacePresentationRendererCpp.Contains("glDrawArrays(") -and $surfacePresentationRendererCpp.Contains("GL_POINTS") -and $surfacePresentationRendererCpp.Contains("syncTireMarkGpuCache") -and -not $surfacePresentationRendererCpp.Contains("tireMarkVertices.reserve")) "TIRE16K uploads compact tire-mark records once into persistent GPU pages and expands ribbons in a geometry shader instead of CPU retessellation"

Check ($surfacePresentationHeader.Contains("previousSegmentSerial") -and $surfacePresentationRendererCpp.Contains("clearTireMarkGpuEndFeather") -and $surfacePresentationRendererCpp.Contains("updatedFlags = location.flags & ~2u") -and $surfacePresentationRendererCpp.Contains("previousSegmentSerial != 0") -and $surfacePresentationRegression.Contains("TIRE16K1")) "TIRE16K1 persistent GPU tire-mark joins clear only predecessor end feathers so longitudinal ribbons stay continuous without CPU retessellation"
Check (($surfacePresentationCpp -match "if\s*\(segment\.serial\s*==\s*0\)\s*segment\.serial\s*=\s*m_nextTireMarkSerial\+\+") -and $surfacePresentationCpp.Contains("segment.previousSegmentSerial = previousSegmentSerial") -and $surfacePresentationRegression.Contains("TIRE16K1B")) "TIRE16K1B keeps tire-mark serials contiguous so O(1) GPU-cache lookup cannot drop every later segment"
Check ($trackRubberHeader.Contains("collectPresentationCellsUnsorted") -and $trackRubberHeader.Contains("collectTransientPresentationUnsorted") -and $surfacePresentationRendererCpp.Contains("kMarbleGeometryShader") -and $surfacePresentationRendererCpp.Contains("syncMarbleGpuCache") -and $surfacePresentationRendererCpp.Contains("drawMarbleGpuCache") -and $surfacePresentationRendererCpp.Contains("drawMovingRubberGpu") -and $surfacePresentationRendererCpp.Contains("MarbleCellGpuRecord") -and $surfacePresentationRendererCpp.Contains("MovingRubberGpuRecord") -and $surfacePresentationRendererCpp.Contains("100 m") -and $trackRubberRegression.Contains("TIRE16L")) "TIRE16L caches resting marble cells in invisible 100 m GPU pages and expands resting/moving two-triangle flakes on the GPU without per-flake CPU tessellation"
Check ($surfacePresentationRendererCpp.Contains("kTireMarkGpuDrawDistanceM = 500.0") -and $surfacePresentationRendererCpp.Contains("kTireMarkGpuDetailedDistanceM = 200.0") -and ($surfacePresentationRendererCpp.Contains("GL_POLYGON_OFFSET_FILL") -and $surfacePresentationRendererCpp.Contains("glPolygonOffset(1.0f, 4.0f)")) -and $surfacePresentationRendererCpp.Contains("float ageOpacity = 1.0 - smooth01(ageT)") -and $surfacePresentationRendererCpp.Contains("float detailMix = nearWeight") -and $surfacePresentationRendererCpp.Contains("sampleCount = detailed ? 6 : 2") -and $surfacePresentationRendererCpp.Contains("visibilityWeight(distanceMeters)")) "TIRE16K GPU shader preserves six-control twenty-minute time fade and master-faded 200 m detailed / 500 m uniform distance LOD"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetTireDevelopmentControls"') -and $luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "SetTireDevelopmentControls"') -and $luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "ResetTrackRubber"') -and $luaRuntimeAndBindingsCpp.Contains('registerFunction("Vehicle", "ResetTirePhysicalState"')) "TIRE15C2 Lua API exposes bounded development acceleration and reset controls"
Check ($engineSimulationCpp.Contains("advancePresentation") -and $engineSimulationCpp.Contains("lastWorldStepCount")) "TIRE15B2 engine advances driven-surface presentation on simulation time"
Check ($surfacePresentationRendererHeader.Contains("class SurfacePresentationRenderer") -and $surfacePresentationRendererCpp.Contains("SurfaceTrackMark") -and $surfacePresentationRendererCpp.Contains("SurfacePresentationParticle") -and $engineRenderingCpp.Contains("surfacePresentationRenderer.draw")) "TIRE15B2 renderer consumes world presentation state for bounded ruts/sinkage and spray/dust/debris visuals"
Check ($luaRuntimeAndBindingsCpp.Contains('registerFunction("Physics", "GetSurfacePresentation"') -and $luaRuntimeAndBindingsCpp.Contains("rolling_audio") -and $luaRuntimeAndBindingsCpp.Contains("spray_audio") -and $luaRuntimeAndBindingsCpp.Contains("dust_audio") -and $luaRuntimeAndBindingsCpp.Contains("debris_audio")) "TIRE15B2 Lua API exposes read-only rolling/spray/dust/debris audio mechanism hooks"
Check ($luaRuntimeAndBindingsCpp.Contains("active_rubber_cells") -and $luaRuntimeAndBindingsCpp.Contains("resident_rubber_chunks") -and $luaRuntimeAndBindingsCpp.Contains("rubber_deposited_generation") -and $luaRuntimeAndBindingsCpp.Contains("rubber_loose_generation")) "TIRE15C Lua surface telemetry exposes authoritative shared rubber-state accounting"
Check ($surfacePresentationRegression.Contains("surfacePresentationIsBoundedAndWorldAddressed") -and $surfacePresentationRegression.Contains("SurfaceMaterial::Gravel")) "TIRE15B2 native regression covers bounded floating-origin-safe presentation plus non-deformable loose-surface emission"
Check ($surfacePresentationRegression.Contains("TIRE16J") -and $surfacePresentationRegression.Contains("slowLateralDrag") -and $surfacePresentationRegression.Contains("curbSkid") -and $surfacePresentationRegression.Contains("ordinaryRoundabout") -and $surfacePresentationRegression.Contains("shoulderTrace") -and $surfacePresentationRegression.Contains("kTireMarkRetirementSeconds") -and $surfacePresentationRegression.Contains("firstSkid.startWidthM - skid.tireWidthM")) "TIRE16J native regression covers clean cornering, drag suppression, curb discontinuities, pressure traces, authored mark width and twenty-minute retirement"
Check ($surfacePresentationRegression.Contains("TIRE16K") -and $surfacePresentationRegression.Contains("chunkProbeA") -and $surfacePresentationRegression.Contains("reconstructGlobal") -and $surfacePresentationRegression.Contains("firstTireMarkSerial") -and $surfacePresentationRegression.Contains("tireMarkSegmentBySerial")) "TIRE16K native regression covers sub-millimetre chunk-local FP32 reconstruction across a 100 m boundary and serial-indexed incremental history access"
Check ($surfacePresentationRegression.Contains("TIRE16I/LOD01") -and $surfacePresentationRegression.Contains("crossfadeAfterBoundary") -and $surfacePresentationRegression.Contains("visibilityWeight") -and $surfacePresentationRegression.Contains("cameraRelativeFp32")) "LOD01 native regression covers master LOD morph/fade boundaries and camera-relative FP32 presentation"
Check ($luaRuntimeAndBindingsCpp.Contains("tireTerrainSurfaceFraction") -and $luaRuntimeAndBindingsCpp.Contains("tireTerrainRutDepthMm") -and $luaRuntimeAndBindingsCpp.Contains("tireTerrainCompaction") -and $luaRuntimeAndBindingsCpp.Contains("tireTerrainPassCount")) "Lua telemetry exposes TIRE15 persistent deformable-terrain state"

$suspensionHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionModel.hpp"
$suspensionHeader = if (Test-Path $suspensionHeaderPath) { [IO.File]::ReadAllText($suspensionHeaderPath) } else { "" }
$suspensionCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionModel.cpp"
$suspensionCpp = if (Test-Path $suspensionCppPath) { [IO.File]::ReadAllText($suspensionCppPath) } else { "" }
Check ($suspensionHeader.Contains("SuspensionModelInput")) "suspension provider has an explicit input contract"
Check ($suspensionHeader.Contains("SuspensionModelOutput")) "suspension provider has an explicit output contract"
Check ($suspensionCpp.Contains('"linear_raycast_v1"')) "linear raycast suspension provider exists"
Check ($suspensionCpp.Contains('"macpherson_strut_v1"')) "MacPherson strut suspension provider ID exists"
Check ($suspensionCpp.Contains('"trailing_arm_torsion_bar_v1"')) "trailing-arm torsion-bar suspension provider ID exists"
Check ($suspensionCpp.Contains("evaluateEquivalentTorsionBar")) "trailing-arm suspension evaluates rotational torsion-bar springing"
Check ($suspensionCpp.Contains("digressiveDamperForce")) "suspension provider implements low/high-speed damping"
Check ($suspensionCpp.Contains("damperDissipationW")) "suspension provider reports dissipated damper power"
Check ($vehicleCpp.Contains("evaluateSuspensionModel")) "VehicleSystem evaluates suspension through the provider boundary"
Check ($luaRuntimeAndBindingsCpp.Contains("luaVehicleGetWheelSuspensionModel")) "Lua exposes exact native suspension readback"

$unsprungHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\UnsprungMassModel.hpp"
$unsprungHeader = if (Test-Path $unsprungHeaderPath) { [IO.File]::ReadAllText($unsprungHeaderPath) } else { "" }
$unsprungCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\UnsprungMassModel.cpp"
$unsprungCpp = if (Test-Path $unsprungCppPath) { [IO.File]::ReadAllText($unsprungCppPath) } else { "" }
Check ($unsprungHeader.Contains("struct UnsprungMassInput")) "unsprung-mass provider has an explicit input contract"
Check ($unsprungHeader.Contains("struct UnsprungMassOutput")) "unsprung-mass provider has an explicit output contract"
Check ($unsprungCpp.Contains("advanceUnsprungMassModel")) "scalar unsprung-mass provider implementation exists"
Check ($luaRuntimeAndBindingsCpp.Contains("luaVehicleGetWheelUnsprungMassModel")) "Lua exposes exact unsprung-mass readback"

$geometryHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionGeometry.hpp"
$geometryHeader = if (Test-Path $geometryHeaderPath) { [IO.File]::ReadAllText($geometryHeaderPath) } else { "" }
$geometryCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionGeometry.cpp"
$geometryCpp = if (Test-Path $geometryCppPath) { [IO.File]::ReadAllText($geometryCppPath) } else { "" }
Check ($geometryHeader.Contains("struct SuspensionGeometryInput")) "suspension geometry has an explicit input contract"
Check ($geometryHeader.Contains("struct SuspensionGeometryOutput")) "suspension geometry has an authoritative output contract"
Check ($geometryCpp.Contains("evaluateSuspensionGeometry")) "suspension geometry provider implementation exists"
$macPhersonHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.hpp"
$macPhersonCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.cpp"
$macPhersonHeader = if (Test-Path $macPhersonHeaderPath) { [IO.File]::ReadAllText($macPhersonHeaderPath) } else { "" }
$macPhersonCpp = if (Test-Path $macPhersonCppPath) { [IO.File]::ReadAllText($macPhersonCppPath) } else { "" }
Check ($macPhersonHeader.Contains("struct MacPhersonHardpoints")) "MacPherson provider owns a fixed hardpoint contract"
Check ($macPhersonCpp.Contains("lowerBallJointAtCompression")) "MacPherson provider solves lower-arm travel from hardpoints"
Check ($macPhersonCpp.Contains("solveBumpSteerRadians")) "MacPherson provider derives passive bump steer from tie-rod geometry"
Check ($macPhersonCpp.Contains("springMotionRatio")) "MacPherson provider derives instantaneous spring motion ratio"
$trailingArmHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.hpp"
$trailingArmCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.cpp"
$trailingArmHeader = if (Test-Path $trailingArmHeaderPath) { [IO.File]::ReadAllText($trailingArmHeaderPath) } else { "" }
$trailingArmCpp = if (Test-Path $trailingArmCppPath) { [IO.File]::ReadAllText($trailingArmCppPath) } else { "" }
Check ($trailingArmHeader.Contains("struct TrailingArmHardpoints")) "trailing-arm provider owns a fixed hardpoint contract"
Check ($trailingArmCpp.Contains("rotatePointAroundLine")) "trailing-arm provider moves the wheel on its pivot arc"
Check ($trailingArmCpp.Contains("torsionBarTwistRadians")) "trailing-arm provider derives torsion-bar angular travel"
Check ($trailingArmCpp.Contains("damperMotionRatio")) "trailing-arm provider derives separate-damper motion ratio"
Check ($vehicleCpp.Contains("state.worldWheelForward")) "VehicleSystem consumes the authoritative upright basis"
Check ($luaRuntimeAndBindingsCpp.Contains("luaVehicleGetWheelUprightPose")) "Lua exposes authoritative upright telemetry"

$suspensionRuntimePath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Suspension.lua"
$suspensionRuntime = if (Test-Path $suspensionRuntimePath) { [IO.File]::ReadAllText($suspensionRuntimePath) } else { "" }
Check ($suspensionRuntime.Contains("ApplyVehicleSuspensionModelToAllWheels")) "module can explicitly copy a selected suspension tune"
$suspensionPanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\SuspensionPanel.lua"
$suspensionPanel = if (Test-Path $suspensionPanelPath) { [IO.File]::ReadAllText($suspensionPanelPath) } else { "" }
Check ($suspensionPanel.Contains("DrawSuspensionLiveTelemetry")) "vehicle suspension panel exposes live force telemetry"
Check ($suspensionPanel.Contains("DrawSuspensionUnsprungControls")) "vehicle suspension panel exposes unsprung-mass tuning"
Check ($suspensionPanel.Contains("DrawSuspensionGeometryControls")) "vehicle suspension panel exposes upright-geometry tuning"
Check ($suspensionPanel.Contains("contactLossTransitions")) "vehicle suspension panel exposes live contact-loss diagnostics"

$dynamicsLabHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDynamicsLab.hpp"
$dynamicsLabHeader = if (Test-Path $dynamicsLabHeaderPath) { [IO.File]::ReadAllText($dynamicsLabHeaderPath) } else { "" }
Check ($dynamicsLabHeader.Contains("WheelDamperDissipationWatts")) "Dynamics Lab records damper energy rate"
Check ($dynamicsLabHeader.Contains("WheelUnsprungVelocityMps")) "Dynamics Lab records wheel-hop velocity"
Check ($dynamicsLabHeader.Contains("WheelCamberDegrees")) "Dynamics Lab records authoritative camber"
Check ($dynamicsLabHeader.Contains("WheelToeDegrees")) "Dynamics Lab records authoritative toe"


$vehicleProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$vehicleProject = if (Test-Path $vehicleProjectPath) { [IO.File]::ReadAllText($vehicleProjectPath) } else { "" }
foreach ($splitSource in @(
    "..\Vehicles\VehicleSystem.cpp",
    "..\Vehicles\VehicleTelemetry.cpp",
    "..\Vehicles\VehicleSimulation.cpp",
    "..\Vehicles\VehicleWheelSimulation.cpp"
)) {
    Check ($vehicleProject.Contains('ClCompile Include="' + $splitSource + '"')) "Visual Studio project compiles CLEAN02 source: $splitSource"
}
foreach ($configSource in $vehicleConfigurationRelativePaths) {
    $projectRelative = $configSource.Replace("Engine\HeritageEngine\", "..\")
    Check ($vehicleProject.Contains('ClCompile Include="' + $projectRelative + '"')) "Visual Studio project compiles CLEAN03A configuration source: $projectRelative"
}
Check (-not $vehicleProject.Contains('ClCompile Include="..\Vehicles\VehicleConfiguration.cpp"')) "Visual Studio project does not compile retired VehicleConfiguration umbrella"
Check ($vehicleProject.Contains('ClInclude Include="..\Vehicles\VehicleSystemInternal.hpp"')) "Visual Studio project tracks CLEAN02 private vehicle helper header"
foreach ($phasePath in $wheelSubstepPhaseRelativePaths) {
    $projectRelative = $phasePath.Replace("Engine\HeritageEngine\", "..\")
    Check ($vehicleProject.Contains('ClInclude Include="' + $projectRelative + '"')) "Visual Studio project tracks CLEAN03B wheel phase: $projectRelative"
}
$engineSourceRoot = Join-Path $Root "Engine\HeritageEngine"
$allLuaBindingSourcesCompiled = $true
foreach ($file in $luaBindingCppFiles) {
    $relativeToEngine = $file.FullName.Substring($engineSourceRoot.Length) -replace '^[\\/]+', ''
    $projectRelative = "..\" + $relativeToEngine
    if (-not $vehicleProject.Contains('ClCompile Include="' + $projectRelative + '"')) {
        $allLuaBindingSourcesCompiled = $false
    }
}
Check $allLuaBindingSourcesCompiled "Visual Studio project compiles every Lua binding implementation translation unit"
Check ($vehicleProject.Contains("..\Core\Modules\LuaBindings\LuaUiBindings.cpp")) "Visual Studio project compiles split UI Lua bindings"
Check ($vehicleProject.Contains("..\Core\Modules\LuaBindings\Physics\LuaPhysicsBodyBindings.cpp")) "Visual Studio project compiles split physics Lua bindings"
Check ($vehicleProject.Contains("..\Core\Modules\LuaBindings\Vehicle\LuaVehicleDefinitionBindings.cpp")) "Visual Studio project compiles split vehicle Lua bindings"
Check ($vehicleProject.Contains("..\Core\Modules\LuaBindings\Entity\LuaEntityCoreBindings.cpp")) "Visual Studio project compiles split entity Lua bindings"
Check ($vehicleProject.Contains("..\Core\Modules\LuaBindings\Vehicle\LuaVehicleDefinitionParser.cpp")) "Visual Studio project compiles the vehicle-definition Lua parser helper"
Check ($vehicleProject.Contains("..\Vehicles\TireModel.cpp")) "Visual Studio project compiles TireModel.cpp"
Check ($vehicleProject.Contains("..\Vehicles\TireModel.hpp")) "Visual Studio project tracks TireModel.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\MagicFormula\MagicFormula62.cpp")) "Visual Studio project compiles TIRE01 MagicFormula62.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\MagicFormula\TirePropertyFile.cpp")) "Visual Studio project compiles TIRE02 TirePropertyFile.cpp"
$clean11CompileFiles = @(
    "TirePropertyParsing.cpp", "TirePropertyMapping.cpp", "TirePropertyMagicFormula.cpp",
    "TirePropertyCommonMetadata.cpp", "TirePropertyHeritageExtensions.cpp",
    "TirePropertyStructuralMetadata.cpp", "TirePropertyDiagnostics.cpp", "TirePartDefinition.cpp"
)
$clean11EngineProjectComplete = $true
foreach ($file in $clean11CompileFiles) {
    if (-not $vehicleProject.Contains("..\Vehicles\Tires\Authoring\" + $file)) { $clean11EngineProjectComplete = $false }
}
Check $clean11EngineProjectComplete "Visual Studio project compiles all CLEAN11 tire-authoring translation units"
Check ($vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePropertyImportInternal.hpp") -and $vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePartDefinition.hpp")) "Visual Studio project tracks CLEAN11 tire-authoring private/public contracts"
Check ($vehicleProject.Contains("..\Vehicles\Tires\Authoring\TireFamilyBaseline.cpp") -and $vehicleProject.Contains("..\Vehicles\Tires\Authoring\TireFamilyBaseline.hpp")) "Visual Studio project compiles and tracks TIRE17A tire-family baseline generation"
Check ($vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePerformanceBiasMapping.cpp") -and $vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePerformanceBiasMapping.hpp")) "Visual Studio project compiles and tracks TIRE17B creator-bias mapping"
Check ($vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePartResolver.cpp") -and $vehicleProject.Contains("..\Vehicles\Tires\Authoring\TirePartResolver.hpp")) "Visual Studio project compiles and tracks TIRE17C runtime tire-part resolution"
Check ($vehicleProject.Contains("..\Vehicles\Tires\MagicFormula\TirePropertyFile.hpp")) "Visual Studio project tracks TIRE02 TirePropertyFile.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\MotorcycleTireProfile.cpp")) "Visual Studio project compiles TIRE01 MotorcycleTireProfile.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireSlipDynamics.cpp")) "Visual Studio project compiles TIRE01 TireSlipDynamics.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireContactPatch.cpp")) "Visual Studio project compiles TIRE03 TireContactPatch.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireContactPatch.hpp")) "Visual Studio project tracks TIRE03 TireContactPatch.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireContactGeometry.cpp")) "Visual Studio project compiles TIRE04 TireContactGeometry.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireContactGeometry.hpp")) "Visual Studio project tracks TIRE04 TireContactGeometry.hpp"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireContactGeometry.cpp"')) "TIRE04 TireContactGeometry.cpp is a compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireRigidRing.cpp")) "Visual Studio project compiles TIRE05 TireRigidRing.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireRigidRing.hpp")) "Visual Studio project tracks TIRE05 TireRigidRing.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireRoadEnveloping.cpp")) "Visual Studio project compiles TIRE05 TireRoadEnveloping.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireRoadEnveloping.hpp")) "Visual Studio project tracks TIRE05 TireRoadEnveloping.hpp"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireRigidRing.cpp"')) "TIRE05 TireRigidRing.cpp is a compiled mechanism"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireRoadEnveloping.cpp"')) "TIRE05 TireRoadEnveloping.cpp is a compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireThermal.cpp")) "Visual Studio project compiles TIRE07 TireThermal.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireThermal.hpp")) "Visual Studio project tracks TIRE07 TireThermal.hpp"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireThermal.cpp"')) "TIRE07 TireThermal.cpp is a compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWear.cpp")) "Visual Studio project compiles TIRE08 TireWear.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWear.hpp")) "Visual Studio project tracks TIRE08 TireWear.hpp"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireWear.cpp"')) "TIRE08 TireWear.cpp graduated from scaffold to compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireSurfaceInteraction.cpp")) "Visual Studio project compiles TIRE11 TireSurfaceInteraction.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireSurfaceInteraction.hpp")) "Visual Studio project tracks TIRE11 TireSurfaceInteraction.hpp"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireSurfaceInteraction.cpp"')) "TIRE11 TireSurfaceInteraction.cpp graduated from scaffold to compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWetSurfaceInteraction.cpp")) "Visual Studio project compiles TIRE12 TireWetSurfaceInteraction.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWetSurfaceInteraction.hpp")) "Visual Studio project tracks TIRE12 TireWetSurfaceInteraction.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWinterSurfaceInteraction.cpp")) "Visual Studio project compiles TIRE13 TireWinterSurfaceInteraction.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireWinterSurfaceInteraction.hpp")) "Visual Studio project tracks TIRE13 TireWinterSurfaceInteraction.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireShallowGranularInteraction.cpp")) "Visual Studio project compiles TIRE14 TireShallowGranularInteraction.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireShallowGranularInteraction.hpp")) "Visual Studio project tracks TIRE14 TireShallowGranularInteraction.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireDeformableTerrainInteraction.cpp")) "Visual Studio project compiles TIRE15 TireDeformableTerrainInteraction.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Tires\TireDeformableTerrainInteraction.hpp")) "Visual Studio project tracks TIRE15 TireDeformableTerrainInteraction.hpp"
Check ($vehicleProject.Contains("..\Physics\Surfaces\SurfaceField.cpp") -and $vehicleProject.Contains("..\Physics\Surfaces\SurfaceMaterialProperties.cpp") -and $vehicleProject.Contains("..\Physics\Surfaces\SurfaceWorld.cpp") -and $vehicleProject.Contains("..\Physics\Surfaces\SurfaceField.hpp") -and $vehicleProject.Contains("..\Physics\Surfaces\SurfaceMaterialProperties.hpp") -and $vehicleProject.Contains("..\Physics\Surfaces\SurfaceWorld.hpp")) "Visual Studio project compiles/tracks CLEAN10/TIRE15B1 world surface subsystem"
Check ($vehicleProject.Contains("..\Physics\Surfaces\Presentation\SurfacePresentation.cpp") -and $vehicleProject.Contains("..\Physics\Surfaces\Presentation\SurfacePresentation.hpp") -and $vehicleProject.Contains("..\Graphics\Renderer\SurfacePresentationRenderer.cpp") -and $vehicleProject.Contains("..\Graphics\Renderer\SurfacePresentationRenderer.hpp")) "Visual Studio project compiles/tracks TIRE15B2 driven-surface presentation and renderer"
Check ($vehicleProject.Contains("..\Graphics\TireMarkChunking.hpp") -and $vehicleProject.Contains("..\Graphics\ShaderProgram.cpp") -and $vehicleProject.Contains("..\Graphics\ShaderProgram.hpp")) "Visual Studio project tracks TIRE16K tire-mark chunking and geometry-shader program support"
Check (-not $vehicleProject.Contains('<ClCompile Include="..\Physics\SurfaceField.cpp"')) "legacy Physics/SurfaceField.cpp is not compiled after CLEAN10"
Check ($vehicleProject.Contains('<ClCompile Include="..\Physics\Surfaces\Rubber\TrackRubberState.cpp"') -and $vehicleProject.Contains('<ClInclude Include="..\Physics\Surfaces\Rubber\TrackRubberState.hpp"') -and -not $vehicleProject.Contains('<None Include="..\Physics\Surfaces\Rubber\TrackRubberState.cpp"')) "TIRE15C TrackRubberState graduated from scaffold to compiled specialized rubber subsystem"
Check ($physicsTestProject.Contains('TrackRubberRegression.cpp') -and $physicsTestProject.Contains('..\Physics\Surfaces\Rubber\TrackRubberState.cpp')) "native regression target compiles TIRE15C track-rubber mechanism and dedicated regression"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireContactPatch.cpp"')) "TIRE03 TireContactPatch.cpp graduated from scaffold to compiled mechanism"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\MotorcycleTireProfile.cpp"')) "TIRE01 MotorcycleTireProfile.cpp graduated from scaffold to compiled mechanism"
Check (-not $vehicleProject.Contains('<None Include="..\Vehicles\Tires\TireSlipDynamics.cpp"')) "TIRE01 TireSlipDynamics.cpp graduated from scaffold to compiled mechanism"
Check ($vehicleProject.Contains("..\Vehicles\VehiclePrecision.hpp")) "Visual Studio project tracks VehiclePrecision.hpp"
$definitionCompilerPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDefinitionCompiler.cpp"
$definitionCompiler = if (Test-Path $definitionCompilerPath) { [IO.File]::ReadAllText($definitionCompilerPath) } else { "" }
$definitionLoaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDefinitionLoader.cpp"
$definitionLoader = if (Test-Path $definitionLoaderPath) { [IO.File]::ReadAllText($definitionLoaderPath) } else { "" }
Check ($definitionCompiler.Contains('"mf62_road"') -and $definitionCompiler.Contains('"mf62_motorcycle"')) "VehicleDefinition compiler accepts TIRE01 provider IDs"
Check ($definitionLoader.Contains("MagicFormula62Motorcycle")) "VehicleDefinition loader maps the motorcycle tire provider into runtime"
$physicsTestProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$physicsTestProject = if (Test-Path $physicsTestProjectPath) { [IO.File]::ReadAllText($physicsTestProjectPath) } else { "" }
Check ($physicsTestProject.Contains("TireModelRegression.cpp")) "native test project compiles TIRE01 tire regressions"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\MagicFormula\TirePropertyFile.cpp")) "native test project compiles TIRE02 property-file importer"
$clean11TestProjectComplete = $true
foreach ($file in $clean11CompileFiles) {
    if (-not $physicsTestProject.Contains("..\Vehicles\Tires\Authoring\" + $file)) { $clean11TestProjectComplete = $false }
}
Check $clean11TestProjectComplete "native test project compiles all CLEAN11 tire-authoring translation units"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireContactPatch.cpp")) "native test project compiles TIRE03 contact-patch mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireContactGeometry.cpp")) "native test project compiles TIRE04 contact-geometry mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireRigidRing.cpp")) "native test project compiles TIRE05 rigid-ring mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireRoadEnveloping.cpp")) "native test project compiles TIRE05 road-enveloping mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireThermal.cpp")) "native test project compiles TIRE07 thermal/pressure mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireWear.cpp")) "native test project compiles TIRE08 spatial tread/wear mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireSurfaceInteraction.cpp")) "native test project compiles TIRE11 tread contamination/surface mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireWetSurfaceInteraction.cpp")) "native test project compiles TIRE12 wet-surface/hydroplaning mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireWinterSurfaceInteraction.cpp")) "native test project compiles TIRE13 compacted-snow/hard-ice mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireShallowGranularInteraction.cpp")) "native test project compiles TIRE14 shallow-granular mechanism"
Check ($physicsTestProject.Contains("..\Vehicles\Tires\TireDeformableTerrainInteraction.cpp")) "native test project compiles TIRE15 deformable-terrain mechanism"
Check ($physicsTestProject.Contains("..\Physics\Surfaces\SurfaceField.cpp") -and $physicsTestProject.Contains("..\Physics\Surfaces\SurfaceMaterialProperties.cpp") -and $physicsTestProject.Contains("..\Physics\Surfaces\SurfaceWorld.cpp")) "native test project compiles CLEAN10/TIRE15B1 world surface subsystem"
Check ($physicsTestProject.Contains("..\Physics\Surfaces\Presentation\SurfacePresentation.cpp") -and $physicsTestProject.Contains("SurfacePresentationRegression.cpp")) "native test project compiles TIRE15B2 surface presentation regression and implementation"
Check ($physicsTestProject.Contains("SurfaceWorldRegression.cpp")) "native test project compiles CLEAN10 floating-origin/chunk-cache regression"
Check ($tireHeader.Contains("turnSlipPerM") -and $tireHeader.Contains("contactPatchTurnMomentNm")) "TIRE03 tire provider contract carries turn-slip/contact-patch inputs"
Check ($tireHeader.Contains("parameterProvenance") -and $tireHeader.Contains("parameterConfidence")) "TIRE02 tire descriptions retain provenance/confidence"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionModel.cpp")) "Visual Studio project compiles SuspensionModel.cpp"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionModel.hpp")) "Visual Studio project tracks SuspensionModel.hpp"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionGeometry.cpp")) "Visual Studio project compiles SuspensionGeometry.cpp"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionGeometry.hpp")) "Visual Studio project tracks SuspensionGeometry.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.cpp")) "Visual Studio project compiles MacPhersonKinematics.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.hpp")) "Visual Studio project tracks MacPhersonKinematics.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.cpp")) "Visual Studio project compiles TrailingArmKinematics.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.hpp")) "Visual Studio project tracks TrailingArmKinematics.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Springs\TorsionBar.cpp")) "Visual Studio project compiles TorsionBar.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Springs\TorsionBar.hpp")) "Visual Studio project tracks TorsionBar.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Common\SuspensionAntiRollBar.cpp")) "Visual Studio project compiles SuspensionAntiRollBar.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Common\SuspensionAntiRollBar.hpp")) "Visual Studio project tracks SuspensionAntiRollBar.hpp"
Check ($vehicleProject.Contains("..\Vehicles\Suspension\Authoring\TrailingArmHardpointEstimator.cpp")) "Visual Studio project compiles trailing-arm hardpoint estimator"
Check ($vehicleProject.Contains("..\Vehicles\UnsprungMassModel.cpp")) "Visual Studio project compiles UnsprungMassModel.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Wheels\Fitment\WheelFitment.cpp")) "Visual Studio project compiles WheelFitment.cpp"
Check ($vehicleProject.Contains("..\Vehicles\Wheels\Fitment\WheelFitment.hpp")) "Visual Studio project tracks WheelFitment.hpp"
Check ($vehicleProject.Contains("..\Vehicles\UnsprungMassModel.hpp")) "Visual Studio project tracks UnsprungMassModel.hpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDynamicsLab.cpp")) "Visual Studio project compiles VehicleDynamicsLab.cpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDynamicsLab.hpp")) "Visual Studio project tracks VehicleDynamicsLab.hpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDefinitionCompiler.cpp")) "Visual Studio project compiles VehicleDefinitionCompiler.cpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDefinitionLoader.cpp")) "Visual Studio project compiles VehicleDefinitionLoader.cpp"
$physicsTestsProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$physicsTestsProject = if (Test-Path $physicsTestsProjectPath) { [IO.File]::ReadAllText($physicsTestsProjectPath) } else { "" }
Check ($physicsTestsProject.Contains("..\Vehicles\Suspension\Geometry\MacPherson\MacPhersonKinematics.cpp")) "physics regression project compiles MacPherson kinematics"
Check ($physicsTestsProject.Contains("..\Vehicles\Suspension\Geometry\TrailingArm\TrailingArmKinematics.cpp")) "physics regression project compiles trailing-arm kinematics"
Check ($physicsTestsProject.Contains("..\Vehicles\Suspension\Springs\TorsionBar.cpp")) "physics regression project compiles torsion-bar springing"
Check ($physicsTestsProject.Contains("..\Vehicles\Suspension\Common\SuspensionAntiRollBar.cpp")) "physics regression project compiles suspension anti-roll bars"
Check ($physicsTestsProject.Contains("..\Vehicles\Suspension\Authoring\TrailingArmHardpointEstimator.cpp")) "physics regression project compiles trailing-arm estimator"
Check ($physicsTestsProject.Contains("..\Vehicles\Dynamics\ChassisFlex\ChassisTorsionalCompliance.cpp")) "physics regression project compiles chassis torsional compliance"
Check ($physicsTestsProject.Contains("..\Vehicles\Dynamics\ChassisFlex\ChassisFlexEstimator.cpp")) "physics regression project compiles chassis-flex estimator"
Check ($physicsTestsProject.Contains("..\Vehicles\Dynamics\ChassisFlex\ChassisFlexDiagnostics.cpp")) "physics regression project compiles chassis-flex diagnostics"
Check ($physicsTestsProject.Contains("..\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesEstimator.cpp")) "physics regression project compiles vehicle mass-property estimator"
Check ($physicsTestsProject.Contains("..\Vehicles\Dynamics\MassProperties\VehicleMassPropertiesAccumulator.cpp")) "physics regression project compiles vehicle mass-property accumulator"
Check ($physicsTestsProject.Contains("..\Vehicles\Wheels\Fitment\WheelFitment.cpp")) "physics regression project compiles wheel fitment geometry"
Check ($vehicleProject.Contains("..\Physics\StaticBoxSceneImporter.cpp")) "Visual Studio project compiles StaticBoxSceneImporter.cpp"
Check ($vehicleProject.Contains("..\Physics\StaticBoxSceneImporter.hpp")) "Visual Studio project tracks StaticBoxSceneImporter.hpp"
Check ($vehicleProject.Contains("..\Physics\StaticTriangleSceneImporter.cpp")) "Visual Studio project compiles StaticTriangleSceneImporter.cpp"
Check ($vehicleProject.Contains("..\Physics\StaticTriangleSceneImporter.hpp")) "Visual Studio project tracks StaticTriangleSceneImporter.hpp"



# DSURF01: persistent static-scene bake must partition authoritative collision
# geometry into world chunks/sheets, retain curb/bridge topology and cache the
# result. Dynamic state migration is intentionally later (DSURF03+).
$dsurfSystemHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceSystem.hpp")
$dsurfBake = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceBake.cpp")
$dsurfStaticData = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceStaticData.hpp")
$dsurfChunk = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceChunk.hpp")
$dsurfSceneLoad = ReadText (Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsColliderBindings.cpp")
$dsurfRegression = ReadText (Join-Path $Root "Engine\HeritageEngine\Tests\DynamicSurfaceRegression.cpp")
Check ($vehicleProject.Contains('DynamicSurface\DynamicSurfaceBake.cpp') -and $vehicleProject.Contains('DynamicSurface\DynamicSurfaceStaticData.hpp')) "DSURF01 engine project compiles/tracks static Dynamic Surface bake and metadata"
Check ($physicsTestProject.Contains('DynamicSurfaceRegression.cpp') -and $physicsTestProject.Contains('DynamicSurface\DynamicSurfaceBake.cpp') -and $physicsTestProject.Contains('DynamicSurface\DynamicSurfaceSystem.cpp')) "DSURF01 native test target compiles Dynamic Surface bake implementation and regression"
Check ($dsurfSystemHeader.Contains('loadOrBakeStaticScene(') -and $dsurfBake.Contains('clipToChunk(') -and $dsurfBake.Contains('buildChunkSheetsAndBoundaries(') -and $dsurfBake.Contains('StaticSurfaceSheetLink')) "DSURF01 collision geometry is clipped into persistent 100m chunks and connected surface sheets"
Check ($dsurfBake.Contains('quantizedEdge(') -and $dsurfBake.Contains('surfaceSheetId') -and $dsurfStaticData.Contains('StaticSurfaceBarrierSegment') -and $dsurfStaticData.Contains('StaticSurfaceDrainRegion')) "DSURF01 bakes 3D manifold sheet identity, hard boundaries and engineered drain metadata"
Check ($dsurfBake.Contains('writeStaticBakeCache(') -and $dsurfBake.Contains('loadStaticBakeCache(') -and $dsurfSceneLoad.Contains('.hdsurf') -and $dsurfSceneLoad.Contains('loadOrBakeDynamicSurface(')) "DSURF01 scene loading owns deterministic .hdsurf static-surface cache"
Check ($dsurfChunk.Contains('staticTriangles()') -and $dsurfChunk.Contains('staticSheets()') -and $dsurfChunk.Contains('staticBarriers()') -and $dsurfChunk.Contains('staticDrains()')) "DSURF01 chunk identity exposes immutable scene-derived metadata without allocating 4096-square CPU state"
Check ($dsurfRegression.Contains('bridge') -and $dsurfRegression.Contains('0.15f') -and $dsurfRegression.Contains('crossChunkSheetLinkCount') -and $dsurfRegression.Contains('loadedFromCache')) "DSURF01 regression protects bridge/road separation, 15cm curb boundary, chunk seam links and cache reload"
Check ((Test-Path (Join-Path $Root "Docs\DSURF01_STATIC_SCENE_SURFACE_BAKE.md")) -and (Test-Path (Join-Path $Root "Build\Reports\DSURF01_StaticSceneSurfaceBake.txt"))) "DSURF01 static-scene surface bake documentation and milestone report are present"

# DSURF02 CPU page residency remains the persistent physics/Track authority, but
# OPT03 retires the renderer-side DynamicSurfaceGpuPagePool duplicate entirely.
# The live renderer now has one explicit production GPU owner: DynamicSurfaceGpuRuntime.
$dsurfTypes = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceTypes.hpp")
$dsurfPagePoolHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfacePagePool.hpp")
$dsurfPagePoolCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfacePagePool.cpp")
$dsurfMeshRendererHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.hpp")
$dsurfMeshRendererCpp = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp")
$dsurfMeshDynamicSurface = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshDynamicSurface.cpp")
$dsurfPerfOverlay = ReadText (Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp")
Check ($vehicleProject.Contains('DynamicSurface\DynamicSurfacePagePool.cpp') -and $vehicleProject.Contains('Graphics\Renderer\EntityMeshDynamicSurface.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuRuntime.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuResources.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuResidency.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuTopology.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuGeometry.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuDispatch.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuTireEvents.cpp') -and $vehicleProject.Contains('Graphics\DynamicSurface\DynamicSurfaceGpuTimers.cpp')) "OPT03 engine project compiles CPU residency authority plus the split production DynamicSurfaceGpuRuntime"
Check ($physicsTestProject.Contains('DynamicSurface\DynamicSurfacePagePool.cpp') -and $physicsTestProject.Contains('DynamicSurfaceRegression.cpp')) "DSURF02 native regression target compiles software virtual page residency"
Check ($dsurfTypes.Contains('kLogicalResolution = 64') -and $dsurfTypes.Contains('kPhysicalPageResolution = 64') -and $dsurfTypes.Contains('kPagesPerAxis = 1') -and $dsurfTypes.Contains('kPhysicalPageMipLevels = 7u')) "LIVETRACK04 keeps the persistent page identity for CPU Track/rubber/temperature state"
Check ($dsurfPagePoolHeader.Contains('VirtualPageAddress') -and $dsurfPagePoolHeader.Contains('kDefaultBudgetBytes = 96ull * 1024ull * 1024ull') -and $dsurfPagePoolHeader.Contains('PagePlaneMask') -and $dsurfPagePoolCpp.Contains('findEvictionCandidate()') -and $dsurfPagePoolCpp.Contains('assignment.pinned') -and $dsurfPagePoolCpp.Contains('any(slot.assignment.dirtyPlanes)')) "DSURF02 CPU page pool remains budgeted, persistent and dirty/pinned-safe"
Check (
    -not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuPagePool.cpp")) -and
    -not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuPagePool.hpp")) -and
    -not $vehicleProject.Contains('DynamicSurfaceGpuPagePool') -and
    -not $dsurfMeshRendererHeader.Contains('DynamicSurfaceGpuPagePool') -and
    -not $dsurfMeshDynamicSurface.Contains('m_dynamicSurfaceGpuPagePool') -and
    $dsurfMeshRendererCpp.Contains('obsolete renderer-side DynamicSurfaceGpuPagePool has been') -and
    $dsurfPerfOverlay.Contains('Legacy renderer page mirror: RETIRED (OPT03)')
) "OPT03 removes the proven-unused renderer GPU page mirror instead of retaining dead regression-only graphics code"
Check ($dsurfRegression.Contains('dynamicSurfacePagePoolIsPersistentBudgetedAndLruSafe') -and $dsurfRegression.Contains('Dirty state is never silently evicted') -and $dsurfRegression.Contains('Pinning independently protects a page') -and $dsurfRegression.Contains('evictionCount != 1u')) "DSURF02 native regression protects stable CPU page identity, strict budget, clean LRU and dirty/pinned eviction safety"
# LIVETRACK04: GPU texture authority supersedes the CPU 100m/256x256 Hydro
# experiment. The old CPU DynamicSurface Hydro remains compiled as a fallback
# and regression oracle, but runtime per-texel water advancement is skipped once
# the GPU authority is ready.
$liveTrack04GpuHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuRuntime.hpp")
$liveTrack04GpuCpp = @(
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuRuntime.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuResources.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuResidency.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTopology.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuGeometry.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuDispatch.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTireEvents.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTimers.cpp")
) -join "`n"
$liveTrack04SurfaceWorld = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.cpp")
$liveTrack10CpuHydro = ReadText (Join-Path $Root "Engine\HeritageEngine\Tests\Reference\DynamicSurfaceHydrologyReference.cpp")
$liveTrack13WorldHydro = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrology.cpp")
$liveTrack13WorldHydroTopology = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyTopology.cpp")
$liveTrack13WorldHydroTiles = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyTiles.cpp")
$liveTrack13WorldHydroCache = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyCache.cpp")
$surfaceWorldRegression = ReadText (Join-Path $Root "Engine\HeritageEngine\Tests\SurfaceWorldRegression.cpp")
$liveTrack04Wetness = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshSurfaceWetness.cpp")
$liveTrack04Shaders = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshShaders.hpp")
$liveTrack04Renderer = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp")
$waterLaboratoryHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\WaterLaboratory.hpp"
Check (
    $liveTrack04GpuHeader.Contains('kTileWorldSizeM = 10.0f') -and
    $liveTrack04GpuHeader.Contains('kTileResolution = 256u') -and
    $liveTrack04GpuHeader.Contains('kSimulationRadiusM = 100.0f') -and
    $liveTrack04GpuHeader.Contains('kTopologyPrefetchRadiusM = 105.0f') -and
    $liveTrack04GpuHeader.Contains('kPresentationRadiusM = 500.0f') -and
    $liveTrack04GpuHeader.Contains('kFarTileResolution = 32u') -and
    $liveTrack04GpuHeader.Contains('kPresentationPollIntervalSeconds = 1.0 / 20.0') -and
    $liveTrack13WorldHydroCache.Contains('constexpr std::uint32_t kCacheVersion = 15;') -and
    $liveTrack13WorldHydroCache.Contains('CachePrebakedFarTile') -and
    $liveTrack13WorldHydroTiles.Contains('rebuildPrebakedFarTileCache()') -and
    $liveTrack13WorldHydroTiles.Contains('encoded[localIndex].encoding = 2u;') -and
    $liveTrack13WorldHydroTiles.Contains('cf[texel] = static_cast<std::uint8_t>((capacityCode << 4u) | flowCode);') -and
    $liveTrack13WorldHydroTopology.Contains('runoffAccumulationAM2') -and
    $liveTrack13WorldHydroTopology.Contains('accumulatedAreaM2[target] += accumulatedAreaM2[i] * targetInfo.fraction;') -and
    $liveTrack13WorldHydroTopology.Contains('priority flood from true open boundaries') -and
    $liveTrack13WorldHydroTiles.Contains('0.0, 0.00001, 0.00005, 0.00010, 0.00015, 0.00020, 0.00025, 0.00030,') -and
    $liveTrack13WorldHydroTiles.Contains('0.00035, 0.00040, 0.00045, 0.00050, 0.00055, 0.00060, 0.00065, 0.00070') -and
    (-not $liveTrack13WorldHydroTiles.Contains('capacityDither01')) -and
    $liveTrack13WorldHydroTiles.Contains('const auto encodeCapacity = [&](double capacityM) -> std::uint8_t') -and
    $liveTrack04Shaders.Contains('float waterDepthFromLadderCode(int code)') -and
    $liveTrack04Shaders.Contains('float quantizeStandingWaterDepth(float depthM)') -and
    $liveTrack04Shaders.Contains('return waterDepthFromLadderCode(code);') -and
    $liveTrack04Shaders.Contains('combined.depthM = quantizeStandingWaterDepth(combined.depthM);')
) "LIVETRACK21I defines compressed v15 world-baked topology with the exact literal 0..0.70mm 4-bit standing-depth ladder and no dithering"
Check (
    -not (Test-Path $waterLaboratoryHeaderPath) -and
    -not $liveTrack04GpuCpp.Contains('m_waterLaboratorySettings') -and
    -not $liveTrack04Shaders.Contains('uWaterLab') -and
    -not $liveTrack04Wetness.Contains('waterLaboratorySettings')
) "LIVETRACK15 keeps runtime Water Laboratory strategy mutation removed"
Check (
    $liveTrack04GpuCpp.Contains('prebakedFarPuddleResponseTile(') -and
    $liveTrack04GpuCpp.Contains('m_prebakedRgbaScratch.assign(texelCount * 4u, 0u);') -and
    $liveTrack04GpuCpp.Contains('releaseTileSlot(it->second.slot);') -and
    $liveTrack04GpuCpp.Contains('m_tiles.erase(it);') -and
    $liveTrack04GpuCpp.Contains('topologyBecameReady') -and
    $liveTrack04GpuCpp.Contains('streamFarTopology(') -and
    -not $liveTrack04GpuCpp.Contains('kFarTopologyUploadBudgetPerPoll') -and
    $liveTrack04GpuHeader.Contains('kFarBulkUploadThreshold = 512u') -and
    $liveTrack04GpuCpp.Contains('m_farAtlasCpuMirror')
) "LIVETRACK15 admits complete near topology and the complete lower-resolution 500m far topology set in one poll"
Check (
    $liveTrack04Shaders.Contains('uniform float uPrebakedWaterExposureM;') -and
    $liveTrack04Shaders.Contains('uniform float uRainWettingExposureM;') -and
    $liveTrack04Shaders.Contains('uniform float uRainRateMmPerHour;') -and
    $liveTrack04Shaders.Contains('float capacityM = decodeGpuWaterDepth(state.b);') -and
    $liveTrack04Shaders.Contains('const float kStandingWaterMaxDepthM = 0.00070;') -and
    $liveTrack04Shaders.Contains('float retainedHeadDriverM = clamp(') -and
    $liveTrack04Shaders.Contains('float headDeficitM = kStandingWaterMaxDepthM') -and
    $liveTrack04Shaders.Contains('float equilibriumDepthM = max(capacityM - headDeficitM, 0.0);') -and
    $liveTrack04Shaders.Contains('decoded.depthM = min(equilibriumDepthM, capacityM)') -and
    $liveTrack04Shaders.Contains('dynamicSurfacePuddleDepthM = standingDepthM;') -and
    $liveTrack04Shaders.Contains('float kinematicRunoffDepthM(GpuWaterDecoded state)') -and
    $liveTrack04Shaders.Contains('const float manningN = 0.014;') -and
    $liveTrack04Wetness.Contains('stats().backgroundSeedDepthM') -and
    $liveTrack04Wetness.Contains('stats().surfaceWettingExposureM') -and
    $liveTrack04Wetness.Contains('stats().runoffDriverMmPerHour') -and
    $liveTrack04GpuCpp.Contains('m_runoffDriverMmPerHour * std::exp(-dt / 45.0f)') -and
    $liveTrack04GpuCpp.Contains('const float retainedRainM = rainM * captureFraction;') -and
    $liveTrack04GpuCpp.Contains('LIVETRACK19 persistence contract: retained basin water NEVER') -and
    -not $liveTrack04Shaders.Contains('rainImpactWetting(') -and
    -not $liveTrack04GpuCpp.Contains('spatialRainMultiplier')
) "LIVETRACK21I uses persistent wet film + 0..0.70mm baked standing-depth authority + a 45s live kinematic runoff driver with no circular impact puddles"
Check (
    $liveTrack04GpuCpp.Contains('Rain itself dispatches no full-field compute') -and
    -not $liveTrack04GpuCpp.Contains('dispatchWaterBatch(cohort, elapsedSeconds') -and
    -not $liveTrack04GpuCpp.Contains('glGenTextures(1, &m_waterBatchScratch)') -and
    $liveTrack04GpuCpp.Contains('4u, false, errorMessage') -and
    -not $liveTrack04GpuCpp.Contains('glGetTextureSubImage')
) "LIVETRACK15 removes the periodic full-field water pulse, water scratch texture and blocking atlas readback"
Check (
    $liveTrack04GpuHeader.Contains('kOptionalTileBudgetPerFrame = 12u') -and
    $liveTrack04GpuCpp.Contains('processed >= kOptionalTileBudgetPerFrame') -and
    $liveTrack04GpuCpp.Contains('m_appliedHydrologyResetSerial != hydrologyResetSerial') -and
    $liveTrack04SurfaceWorld.Contains('++m_hydrologyResetSerial;')
) "LIVETRACK15 prevents optional-state cohort pulses and keeps Reset Surface Water synchronized with GPU presentation state"
Check (
    $liveTrack04Shaders.Contains('const vec2 offsets[4] = vec2[4](') -and
    $liveTrack04Shaders.Contains('decoded.depthM = depthSum * inverseWeight;') -and
    $liveTrack04Shaders.Contains('decoded.runoffPotential = runoffSum * inverseWeight;') -and
    $liveTrack04Shaders.Contains('decoded.runoffAreaM2 = runoffAreaSum * inverseWeight;') -and
    $liveTrack04Shaders.Contains('normalize(flowSum)') -and
    $liveTrack04Shaders.Contains('bool gpuFarWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)') -and
    $liveTrack04Shaders.Contains('bool nearValid = false;') -and
    $liveTrack04Shaders.Contains('bool farValid = false;') -and
    $liveTrack04Shaders.Contains('if (distanceM < 85.0)') -and
    $liveTrack04Shaders.Contains('else if (distanceM > 100.0)') -and
    $liveTrack04Shaders.Contains('combined.depthM = quantizeStandingWaterDepth(combined.depthM);') -and
    $liveTrack04Shaders.Contains('lodDetail = 1.0 - smoothstep(450.0, 500.0, distanceM);') -and
    $liveTrack04Shaders.Contains('const float visibleWaterOnsetM = 0.000010;') -and
    -not $liveTrack04Shaders.Contains('rainImpactWetting(') -and
    $liveTrack04Shaders.Contains('LIVETRACK21I performance: retain hardware GL_LINEAR filtering but reduce') -and
    $liveTrack04Shaders.Contains('state = texture(uGpuWaterAtlas, atlasUv);') -and
    $liveTrack04Shaders.Contains('bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)') -and
    $liveTrack04Shaders.IndexOf('bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)') -lt $liveTrack04Shaders.IndexOf('bool gpuNearWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)') -and
    $liveTrack04Shaders.Contains('float kinematicRunoffDepthM(GpuWaterDecoded state)') -and
    $liveTrack04Shaders.Contains('float runoffSheet = runoff * max(film, 0.12) * runoffPatch') -and
    $liveTrack04Shaders.Contains('vec3 rainImpactRippleNormal(') -and
    $liveTrack04Shaders.Contains('float puddleGate = smoothstep(0.00001, 0.00008, standingDepthM) * standing;') -and
    $liveTrack04Shaders.Contains('float runoffGate = smoothstep(0.00005, 0.00030, runoffDepthM) * runoff * 0.10;') -and
    $liveTrack04Shaders.Contains('float connected = smoothstep(0.000010, 0.000180, uRainWettingExposureM);') -and
    $liveTrack04Shaders.Contains('smoothstep(0.02, 0.25, rainIntensity)') -and
    -not $liveTrack04Shaders.Contains('float ring = sin((n1 + uSurfacePresentationTime') -and
    $liveTrack04Shaders.Contains('dynamicSurfaceFlowDirection') -and
    $liveTrack04Shaders.Contains('uSurfaceWetnessBreakupMask') -and
    $liveTrack04Shaders.Contains('dynamicSurfacePuddleDepthM = standingDepthM;') -and
    $liveTrack04Shaders.Contains('float runningDepthM = gpuValid ? kinematicRunoffDepthM(gpuState) : 0.0;') -and
    $liveTrack04Shaders.Contains('float headDeficitM = kStandingWaterMaxDepthM') -and
    $liveTrack04Shaders.Contains('float freeSurfaceDepthM = max(puddleDepthM, 0.0);') -and
    $liveTrack04Shaders.Contains('float runoffDepthM = max(presentationDepthM, 0.0);') -and
    (-not $liveTrack04Shaders.Contains('mix(0.028, 0.0')) -and
    (-not $liveTrack04Shaders.Contains('vWorldPosition, max(standingDepthM, runningDepthM)')) -and
    $liveTrack04Shaders.Contains('0.045')
) "LIVETRACK21I keeps the fast single-snap range-gated near/far water path, 500m topology, 0.01mm standing-water onset, catchment kinematic runoff and puddle-gated rain ripples"
Check (
    -not $liveTrack04SurfaceWorld.Contains('m_dynamicSurface.advanceHydro(') -and
    -not $liveTrack04SurfaceWorld.Contains('m_dynamicSurface.sampleHydro(') -and
    $liveTrack04SurfaceWorld.Contains('if (m_gpuDynamicSurfaceAuthorityEnabled)') -and
    $liveTrack04SurfaceWorld.Contains('m_gpuDynamicSurfaceTireEvents') -and
    $liveTrack04SurfaceWorld.Contains('no CPU spatial-water fallback') -and
    -not $liveTrack04Wetness.Contains('dynamicSurface.rasterHydroPage(') -and
    -not $liveTrack04Wetness.Contains('uploadHydroMip(') -and
    -not $liveTrack04Shaders.Contains('uDynamicSurfaceHydroPages')
) "OPT03C production SurfaceWorld has one GPU spatial-water authority and no CPU Hydro fallback"

Check (
    $dsurfTypes.Contains('static constexpr double hydroDistantHz = 0.0;') -and
    $liveTrack10CpuHydro.Contains('if (nearest > nearRadiusSquared)') -and
    $liveTrack10CpuHydro.Contains('it = m_pages.erase(it);') -and
    $surfaceWorldRegression.Contains('multiMidpoint.valid') -and
    $surfaceWorldRegression.Contains('multiStats.cadenceDistantPages != 0u')
) "OPT03C test-only CPU Hydro reference remains bounded to the <=100m interest union for historical regression coverage"
Check (
    $dsurfMeshDynamicSurface.Contains('m_dynamicSurfaceGpuRuntime.initialize(errorMessage)') -and
    $dsurfMeshDynamicSurface.Contains('m_dynamicSurfaceGpuRuntime.update(') -and
    $dsurfMeshDynamicSurface.Contains('setGpuDynamicSurfaceAuthorityEnabled(')
) "LIVETRACK06 renderer initializes, updates and promotes the GPU Hydro field to runtime authority"
Check (
    $dsurfPerfOverlay.Contains('LIVETRACK21 STANDING + RUNNING WATER <=500m / 20Hz TOPOLOGY POLL') -and
    $dsurfPerfOverlay.Contains('Near source: %u prebaked | %u fallback | %u uploads this frame') -and
    $dsurfPerfOverlay.Contains('Far topology <=500m: %u/%u resident | %u admitted this poll | unavailable %u') -and
    $dsurfPerfOverlay.Contains('static .hhyd v15: priority-flood 4-bit standing-depth ceiling + total-contributing MFD catchment + flow direction; terminal minima retain catchment area') -and
    $dsurfPerfOverlay.Contains('100-500m: complete lower-resolution prebaked runoff/standing-depth/flow set; every desired tile admitted together') -and
    $dsurfPerfOverlay.Contains('tile membership poll: fixed 20Hz; no progressive far streamer; optical shading remains per-frame') -and
    $dsurfPerfOverlay.Contains('rain water full-field CFD: OFF') -and
    $dsurfPerfOverlay.Contains('localized tire dry-line remains compute-shader driven') -and
    $dsurfPerfOverlay.Contains('synchronous atlas readback: OFF')
) "LIVETRACK15 F8 telemetry exposes compressed near/far topology coverage, complete 500m admission, GPU reconstruction and 20Hz stutter guards"
Check (
    -not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.cpp")) -and
    (Test-Path (Join-Path $Root "Engine\HeritageEngine\Tests\Reference\DynamicSurfaceHydrologyReference.cpp")) -and
    (Test-Path (Join-Path $Root "Engine\HeritageEngine\Tests\DynamicSurfaceRegression.cpp")) -and
    -not $liveTrack04SurfaceWorld.Contains('m_dynamicSurface.advanceHydro(') -and
    $dsurfRegression.Contains('DynamicSurfaceHydrologyReference')
) "OPT03C quarantines the retired CPU Hydro implementation under native tests only"
Check ($dsurfRegression.Contains('dynamicSurfaceHydrologyConservesCappedVolume') -and $dsurfRegression.Contains('dynamicSurfaceThermalIsSheetAwareAndTireHeated')) "LIVETRACK04 retains legacy Hydro conservation and Track thermal regressions as safety oracles"
Check (
    (Test-Path (Join-Path $Root "Docs\LIVETRACK01_PERSISTENT_SENSOR_SURFACE.md")) -and
    (Test-Path (Join-Path $Root "Docs\Decisions\ADR-137-Persistent-Sensor-LiveTrack-Surface.md")) -and
    (Test-Path (Join-Path $Root "Build\Reports\LIVETRACK01_PersistentSensorSurface.txt"))
) "LIVETRACK01 architecture, decision record and milestone report are present"
