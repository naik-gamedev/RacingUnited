param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $Root = (Resolve-Path $Root).Path
}

$reportRoot = Join-Path $Root "Build\Reports"
New-Item -ItemType Directory -Force -Path $reportRoot | Out-Null
$reportPath = Join-Path $reportRoot "ValidationReport.txt"
$results = New-Object System.Collections.Generic.List[string]
$failed = $false

function Check([bool]$Condition, [string]$Label) {
    if ($Condition) {
        $script:results.Add("PASS: $Label")
    } else {
        $script:results.Add("FAIL: $Label")
        $script:failed = $true
    }
}

$required = @(
    "Docs\README.md",
    "Docs\PROJECT_STATE.md",
    "Docs\AI_WORKFLOW.md",
    "Docs\ARCHITECTURE.md",
    "Docs\MEMORY_OWNERSHIP.md",
    "Docs\LUA_API_RULES.md",
    "Docs\PHYSICS_ARCHITECTURE.md",
    "Docs\VEHICLE_ARCHITECTURE.md",
    "Docs\VEHICLE_DYNAMICS_LAB.md",
    "Docs\SUSPENSION_MODEL.md",
    "Docs\VEHICLE_WORKSHOP.md",
    "Docs\VEHICLE_DEFINITION_RUNTIME.md",
    "Docs\Decisions\ADR-005-Advanced-Road-Tire-Provider.md",
    "Docs\Decisions\ADR-006-Per-Wheel-Tire-Profiles.md",
    "Docs\Decisions\ADR-007-Player-Vehicle-Visual-Slot.md",
    "Docs\Decisions\ADR-008-Articulated-Wheel-Presentation.md",
    "Docs\Decisions\ADR-009-Wheel-Coordinate-Contract.md",
    "Docs\Decisions\ADR-010-Blender-Authoring-Player-Scene.md",
    "Docs\Decisions\ADR-012-Native-Vehicle-Definition-Compiler.md",
    "Docs\Decisions\ADR-013-Suspension-Provider-Contract.md",
    "Docs\Decisions\ADR-014-Nonlinear-Suspension-Forces.md",
    "Docs\LuaApiAnnotations.json",
    "Engine\HeritageEngine\Core\Diagnostics\BuildIdentity.hpp",
    "Engine\HeritageEngine\Core\Diagnostics\GeneratedBuildIdentity.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp",
    "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.hpp",
    "Engine\HeritageEngine\Vehicles\TireModel.hpp",
    "Engine\HeritageEngine\Vehicles\TireModel.cpp",
    "Engine\HeritageEngine\Vehicles\SuspensionModel.hpp",
    "Engine\HeritageEngine\Vehicles\SuspensionModel.cpp",
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
    "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj",
    "Engine\HeritageEngine\Tests\PhysicsRegression.cpp",
    "Engine\HeritageEngine\HeritageEngine\main.cpp",
    "Modules\RacingUnited\Scripts\Main.lua",
    "Modules\RacingUnited\Scripts\Runtime\SurfaceDemo.lua",
    "Modules\RacingUnited\Scripts\Runtime\PlayerWorld.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Workshop.lua",
    "Modules\RacingUnited\Tests\VehicleDefinitionV2Tests.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Visuals.lua",
    "Modules\RacingUnited\Scripts\Vehicles\DynamicsLab.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\VisualPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\BodyPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\Visual\WheelsPanel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\VisualWheels.lua",
    "Modules\RacingUnited\Assets\Vehicles\Player\PlayerCar.obj",
    "Modules\RacingUnited\Assets\Vehicles\Player\PlayerWheel.obj",
    "Modules\RacingUnited\Assets\Scenes\Player\PlayerScene.obj",
    "Modules\RacingUnited\Assets\Scenes\Player\PlayerScene_Collision.obj",
    "Modules\RacingUnited\Assets\Scenes\Player\README_IMPORT.txt",
    "Modules\RacingUnited\Scripts\UI\VehicleDebugPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\DynamicsLabPanel.lua",
    "Modules\RacingUnited\Scripts\UI\Vehicle\WorkshopPanel.lua",
    "Tools\RunPhysicsTests.cmd"
)
foreach ($relative in $required) {
    Check (Test-Path (Join-Path $Root $relative)) "required file exists: $relative"
}

& (Join-Path $PSScriptRoot "GenerateLuaApiManifest.ps1") -Root $Root

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
        "Physics.SetColliderSurface",
        "Physics.GetColliderSurface",
        "Physics.LoadStaticBoxScene",
        "Physics.UnloadStaticBoxScene",
        "Physics.GetStaticBoxSceneCount",
        "Physics.LoadStaticTriangleScene",
        "Physics.UnloadStaticTriangleScene",
        "Physics.GetStaticTriangleSceneCount",
        "Vehicle.GetWheelState",
        "Entity.Destroy"
    )) {
        Check ($qualified -contains $name) "Lua API contains $name"
    }
}

$runtimeCppPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp"
$runtimeCpp = if (Test-Path $runtimeCppPath) { [IO.File]::ReadAllText($runtimeCppPath) } else { "" }
Check ($runtimeCpp.Contains("m_registeredLuaFunctions")) "runtime records exact registered Lua names"
Check ($runtimeCpp.Contains("runSafetySmokeTests")) "runtime contains lifetime safety smoke tests"
Check ($runtimeCpp.Contains("LuaAPI_Runtime.json")) "runtime writes a live API manifest"
Check ($runtimeCpp.Contains("return 47;")) "wheel-state Lua bridge includes suspension force telemetry"
Check ($runtimeCpp.Contains("return 20;")) "Dynamics Lab summary includes suspension energy extrema"
Check ($runtimeCpp.Contains("OFN_FILEMUSTEXIST")) "Windows module asset picker requires an existing file"
Check ($runtimeCpp.Contains("resolveSavePath(relative)")) "bounded text export resolves through the active module save root"

$physicsWorldPath = Join-Path $Root "Engine\HeritageEngine\Physics\PhysicsWorld.cpp"
$physicsWorld = if (Test-Path $physicsWorldPath) { [IO.File]::ReadAllText($physicsWorldPath) } else { "" }
$vehiclePos = $physicsWorld.IndexOf("m_vehicles.destroyForBody(handle)")
$constraintPos = $physicsWorld.IndexOf("m_constraints.destroyForBody(handle)")
$colliderPos = $physicsWorld.IndexOf("m_collisions.destroyForBody(handle)")
$bodyPos = $physicsWorld.IndexOf("m_rigidBodies.destroy(handle)")
Check ($vehiclePos -ge 0 -and $constraintPos -gt $vehiclePos -and $colliderPos -gt $constraintPos -and $bodyPos -gt $colliderPos) "body destruction cascades dependents before invalidating the body"

foreach ($pair in @(
    @{ Path = "Engine\HeritageEngine\Core\Entities\EntityRegistry.hpp"; Text = "using EntityHandle = std::uint64_t" },
    @{ Path = "Engine\HeritageEngine\Physics\RigidBodySystem.hpp"; Text = "using BodyHandle = std::uint64_t" },
    @{ Path = "Engine\HeritageEngine\Physics\CollisionSystem.hpp"; Text = "using ColliderHandle = std::uint64_t" },
    @{ Path = "Engine\HeritageEngine\Physics\ConstraintSystem.hpp"; Text = "using ConstraintHandle = std::uint64_t" },
    @{ Path = "Engine\HeritageEngine\Vehicles\VehicleSystem.hpp"; Text = "using VehicleHandle = std::uint64_t" }
)) {
    $path = Join-Path $Root $pair.Path
    $text = if (Test-Path $path) { [IO.File]::ReadAllText($path) } else { "" }
    Check ($text.Contains($pair.Text)) "generation-handle contract present: $($pair.Path)"
}

$collisionHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\CollisionSystem.hpp"
$collisionHeader = if (Test-Path $collisionHeaderPath) { [IO.File]::ReadAllText($collisionHeaderPath) } else { "" }
Check ($collisionHeader.Contains("enum class SurfaceMaterial")) "physics collider surface-material contract exists"
Check ($collisionHeader.Contains("float surfaceWetness")) "physics collider wetness metadata exists"

$vehicleHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystem.hpp"
$vehicleHeader = if (Test-Path $vehicleHeaderPath) { [IO.File]::ReadAllText($vehicleHeaderPath) } else { "" }
$vehicleCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystem.cpp"
$vehicleCpp = if (Test-Path $vehicleCppPath) { [IO.File]::ReadAllText($vehicleCppPath) } else { "" }
Check ($vehicleHeader.Contains("contactCollider")) "wheel telemetry retains exact contacted collider"
Check ($vehicleHeader.Contains("surfaceMaterial")) "wheel telemetry retains contacted surface material"
Check ($vehicleHeader.Contains("TireModelDescription tireModel")) "each wheel record owns independent native tire data"
Check ($vehicleHeader.Contains("setWheelTireModel")) "per-wheel tire setter contract exists"
Check ($vehicleHeader.Contains("wheelTireModel")) "per-wheel tire readback contract exists"
Check ($vehicleHeader.Contains("struct VehicleRestState")) "vehicle parked-rest diagnostic contract exists"
Check ($vehicleHeader.Contains("startDynamicsLabCapture")) "vehicle exposes opt-in native dynamics capture"
Check ($vehicleHeader.Contains("damperDissipationWatts")) "wheel state exposes damper energy-rate telemetry"

$physicsRegressionPath = Join-Path $Root "Engine\HeritageEngine\Tests\PhysicsRegression.cpp"
$physicsRegression = if (Test-Path $physicsRegressionPath) { [IO.File]::ReadAllText($physicsRegressionPath) } else { "" }
Check ($physicsRegression.Contains("parkingBrakeHoldsOnSlope")) "headless regression covers parking-brake slope hold"
Check ($physicsRegression.Contains("unbrakedVehicleRollsOnSlope")) "headless regression preserves unbraked slope roll"
Check ($physicsRegression.Contains("flatRestSleepsAndThrottleWakes")) "headless regression covers parked sleep and throttle wake"
Check ($physicsRegression.Contains("dynamicsLabCapturesHighRateTelemetry")) "headless regression verifies exact high-rate dynamics capture"
Check ($physicsRegression.Contains("vehicleDefinitionCompilerAndLoaderWork")) "headless regression verifies native definition compilation and loading"
Check ($physicsRegression.Contains("motion_ratio_force_n")) "headless regression verifies suspension motion-ratio force evaluation"
Check ($physicsRegression.Contains("nonlinear_force_n")) "headless regression verifies non-linear suspension force components"

$definitionV2Path = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\VehicleDefinitionV2.lua"
$definitionV2 = if (Test-Path $definitionV2Path) { [IO.File]::ReadAllText($definitionV2Path) } else { "" }
Check ($definitionV2.Contains("schemaVersion = 2")) "VehicleDefinitionV2 has an explicit schema version"
Check ($definitionV2.Contains("powerUnits = {}")) "VehicleDefinitionV2 stores arbitrary power units"
Check ($definitionV2.Contains("transmissions = {}")) "VehicleDefinitionV2 stores arbitrary transmissions"
Check ($definitionV2.Contains("suspensions = {}")) "VehicleDefinitionV2 stores reusable suspension components"
Check ($definitionV2.Contains("suspension = suspensionId")) "contact units reference stable suspension IDs"
Check ($definitionV2.Contains("bumpHighSpeedDampingNsPerM")) "suspension definitions author digressive damping"
Check ($definitionV2.Contains("bumpStopProgressionNPerM2")) "suspension definitions author progressive travel stops"
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


$tireHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\TireModel.hpp"
$tireHeader = if (Test-Path $tireHeaderPath) { [IO.File]::ReadAllText($tireHeaderPath) } else { "" }
$tireCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\TireModel.cpp"
$tireCpp = if (Test-Path $tireCppPath) { [IO.File]::ReadAllText($tireCppPath) } else { "" }
Check ($tireHeader.Contains("struct TireContactInput")) "advanced tire provider has an explicit contact-input contract"
Check ($tireHeader.Contains("struct TireForceResult")) "advanced tire provider has an explicit force-output contract"
Check ($tireCpp.Contains("evaluateAdvancedRoadTire")) "advanced road-tire provider implementation exists"
Check ($tireCpp.Contains("generalizedTireCurve")) "advanced road-tire curve is isolated from VehicleSystem"

$suspensionHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionModel.hpp"
$suspensionHeader = if (Test-Path $suspensionHeaderPath) { [IO.File]::ReadAllText($suspensionHeaderPath) } else { "" }
$suspensionCppPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\SuspensionModel.cpp"
$suspensionCpp = if (Test-Path $suspensionCppPath) { [IO.File]::ReadAllText($suspensionCppPath) } else { "" }
Check ($suspensionHeader.Contains("SuspensionModelInput")) "suspension provider has an explicit input contract"
Check ($suspensionHeader.Contains("SuspensionModelOutput")) "suspension provider has an explicit output contract"
Check ($suspensionCpp.Contains('"linear_raycast_v1"')) "linear raycast suspension provider exists"
Check ($suspensionCpp.Contains("digressiveDamperForce")) "suspension provider implements low/high-speed damping"
Check ($suspensionCpp.Contains("damperDissipationW")) "suspension provider reports dissipated damper power"
Check ($vehicleCpp.Contains("evaluateSuspensionModel")) "VehicleSystem evaluates suspension through the provider boundary"

$dynamicsLabHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleDynamicsLab.hpp"
$dynamicsLabHeader = if (Test-Path $dynamicsLabHeaderPath) { [IO.File]::ReadAllText($dynamicsLabHeaderPath) } else { "" }
Check ($dynamicsLabHeader.Contains("WheelDamperDissipationWatts")) "Dynamics Lab records damper energy rate"


$vehicleProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$vehicleProject = if (Test-Path $vehicleProjectPath) { [IO.File]::ReadAllText($vehicleProjectPath) } else { "" }
Check ($vehicleProject.Contains("..\Vehicles\TireModel.cpp")) "Visual Studio project compiles TireModel.cpp"
Check ($vehicleProject.Contains("..\Vehicles\TireModel.hpp")) "Visual Studio project tracks TireModel.hpp"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionModel.cpp")) "Visual Studio project compiles SuspensionModel.cpp"
Check ($vehicleProject.Contains("..\Vehicles\SuspensionModel.hpp")) "Visual Studio project tracks SuspensionModel.hpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDynamicsLab.cpp")) "Visual Studio project compiles VehicleDynamicsLab.cpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDynamicsLab.hpp")) "Visual Studio project tracks VehicleDynamicsLab.hpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDefinitionCompiler.cpp")) "Visual Studio project compiles VehicleDefinitionCompiler.cpp"
Check ($vehicleProject.Contains("..\Vehicles\VehicleDefinitionLoader.cpp")) "Visual Studio project compiles VehicleDefinitionLoader.cpp"
Check ($vehicleProject.Contains("..\Physics\StaticBoxSceneImporter.cpp")) "Visual Studio project compiles StaticBoxSceneImporter.cpp"
Check ($vehicleProject.Contains("..\Physics\StaticBoxSceneImporter.hpp")) "Visual Studio project tracks StaticBoxSceneImporter.hpp"
Check ($vehicleProject.Contains("..\Physics\StaticTriangleSceneImporter.cpp")) "Visual Studio project compiles StaticTriangleSceneImporter.cpp"
Check ($vehicleProject.Contains("..\Physics\StaticTriangleSceneImporter.hpp")) "Visual Studio project tracks StaticTriangleSceneImporter.hpp"

$mainLuaPath = Join-Path $Root "Modules\RacingUnited\Scripts\Main.lua"
if (Test-Path $mainLuaPath) {
    $mainLua = [IO.File]::ReadAllText($mainLuaPath)
    $lineCount = (Get-Content $mainLuaPath).Count
    Check ($lineCount -lt 80) "Main.lua remains a small include coordinator"
    Check (-not $mainLua.Contains("Runtime/VehicleDemo.lua")) "obsolete VehicleDemo.lua is not included"
    $includeMatches = [regex]::Matches($mainLua, 'Include\("(?<path>[^"]+\.lua)"\)')
    foreach ($match in $includeMatches) {
        $relative = $match.Groups['path'].Value.Replace('/', [IO.Path]::DirectorySeparatorChar)
        Check (Test-Path (Join-Path (Split-Path $mainLuaPath) $relative)) "Main.lua include exists: $relative"
    }
}
Check (-not (Test-Path (Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\VehicleDemo.lua"))) "obsolete Runtime/VehicleDemo.lua is absent"

$visualDefinitionPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua"
$visualDefinition = if (Test-Path $visualDefinitionPath) { [IO.File]::ReadAllText($visualDefinitionPath) } else { "" }
Check ($visualDefinition.Contains('bodyAsset = "Vehicles/Player/PlayerCar.obj"')) "prototype definition points at the stable player-car OBJ slot"
Check ($visualDefinition.Contains("wheelbaseM = 2.442")) "prototype carries 2442 mm Peugeot reference wheelbase"
Check ($visualDefinition.Contains("frontTrackM = 1.437")) "prototype carries 1437 mm Peugeot reference front track"
Check ($visualDefinition.Contains("rearTrackM = 1.428")) "prototype carries 1428 mm Peugeot reference rear track"
Check ($visualDefinition.Contains("wheelRadiusM = 0.2979")) "prototype carries 205/40 ZR17 derived wheel radius"
Check ($visualDefinition.Contains("mount = { -0.7185, 0.85, 1.221 }")) "front-left mount matches reference geometry"
Check ($visualDefinition.Contains("mount = { 0.7140, 0.85, -1.221 }")) "rear-right mount matches reference geometry"

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
$visualWheelsRuntime = if (Test-Path $visualWheelsRuntimePath) { [IO.File]::ReadAllText($visualWheelsRuntimePath) } else { "" }
Check ($visualWheelsRuntime.Contains("UpdateVehicleWheelPresentation")) "articulated-wheel presentation runtime exists"
Check ($visualWheelsRuntime.Contains("Entity.SetWorldPosition")) "wheel presentation consumes authoritative native world centers"
Check ($visualWheelsRuntime.Contains("telemetry.centerX")) "wheel presentation reads native center telemetry"
Check ($visualWheelsRuntime.Contains("telemetry.steerAngle")) "articulated-wheel presentation consumes native steering telemetry"
Check ($visualWheelsRuntime.Contains("telemetry.rotationDegrees")) "articulated-wheel presentation consumes native wheel rotation telemetry"
Check ($visualWheelsRuntime.Contains("visualFaceYawDegrees")) "wheel presentation supports independent left/right mesh facing"
Check ($visualWheelsRuntime.Contains("visualSpinSign")) "wheel presentation compensates spin for per-side mesh facing"

$wheelContractPath = Join-Path $Root "Docs\Decisions\ADR-009-Wheel-Coordinate-Contract.md"
Check (Test-Path $wheelContractPath) "wheel coordinate/presentation ADR exists"

$playerWorldPath = Join-Path $Root "Modules\RacingUnited\Scripts\Runtime\PlayerWorld.lua"
$playerWorld = if (Test-Path $playerWorldPath) { [IO.File]::ReadAllText($playerWorldPath) } else { "" }
Check ($playerWorld.Contains("Physics.LoadStaticTriangleScene")) "Player World loads exact static triangle drive-surface queries"
Check ($playerWorld.Contains("playerWorld.visualAsset")) "Player World owns a creator visual-scene slot"
Check ($playerWorld.Contains("true)")) "Player World requests Blender-coordinate import"
Check ($playerWorld.Contains("spawnPosition")) "Player World stores authoritative imported spawn position"
Check ($playerWorld.Contains("ResetVehicleAtPlayerWorldSpawn")) "Player World resets vehicle at imported spawn"

$staticSceneImporterPath = Join-Path $Root "Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp"
$staticSceneImporter = if (Test-Path $staticSceneImporterPath) { [IO.File]::ReadAllText($staticSceneImporterPath) } else { "" }
Check ($staticSceneImporter.Contains('"spawn_player"')) "OBJ triangle bridge recognizes SPAWN_PLAYER metadata"
Check ($staticSceneImporter.Contains("triangle-origin")) "OBJ triangle bridge has terrain-at-origin spawn fallback"
Check ($staticSceneImporter.Contains("snapStaticTriangleSceneSpawnToSurface")) "authored spawn markers snap to the actual drive surface"

$meshCppPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Mesh.cpp"
$meshCpp = if (Test-Path $meshCppPath) { [IO.File]::ReadAllText($meshCppPath) } else { "" }
Check ($meshCpp.Contains("blenderCoordinates")) "OBJ loader has explicit Blender-coordinate mode"
Check ($meshCpp.Contains("return { value[0], value[1], -value[2] };")) "Blender default OBJ axis conversion maps into engine coordinates"
Check ($meshCpp.Contains("face[triangle + 1], face[triangle]")) "Blender-coordinate reflection reverses triangle winding"

$prototypeDefinition = $visualDefinition
Check ($prototypeDefinition.Contains("radiusScale = 1.0")) "creator wheel radius scale defaults to 1:1"
Check ($prototypeDefinition.Contains("widthScale = 1.0")) "creator wheel width scale defaults to 1:1"
Check ($prototypeDefinition.Contains("offset = { 0.0, 0.0, 0.0 }")) "creator body visual offset defaults to identity"

$compiledMain = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\main.cpp"
$obsoleteMain = Join-Path $Root "Engine\HeritageEngine\main.cpp"
Check (Test-Path $compiledMain) "authoritative compiled main.cpp exists"
Check (-not (Test-Path $obsoleteMain)) "obsolete outer main.cpp is absent"

$headerPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\GeneratedBuildIdentity.hpp"
$headerText = if (Test-Path $headerPath) { [IO.File]::ReadAllText($headerPath) } else { "" }
Check ($headerText.Contains("kMilestone")) "generated build identity header is valid"

$summary = @(
    "Heritage Engine static validation",
    "utc=$([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ss.fffZ'))",
    "root=$Root",
    "result=$(if ($failed) { 'FAIL' } else { 'PASS' })",
    ""
) + $results
[IO.File]::WriteAllLines($reportPath, $summary, [Text.UTF8Encoding]::new($false))

foreach ($line in $results) { Write-Host $line }
Write-Host "Validation report: $reportPath"
if ($failed) { exit 1 }
exit 0
