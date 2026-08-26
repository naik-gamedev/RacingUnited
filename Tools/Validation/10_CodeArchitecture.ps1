# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

# OPT00: profiling must stay asynchronous and structurally repeatable. Timestamp
# pairs may nest inside Heritage's existing frame-wide GL_TIME_ELAPSED query;
# current-frame blocking reads/glFinish are forbidden.
$opt00GpuTimerPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\AsyncGpuTimer.hpp"
$opt00GpuTimer = ReadText $opt00GpuTimerPath
Check (Test-Path $opt00GpuTimerPath) "OPT00 asynchronous GPU pass-timer utility exists"
Check ($opt00GpuTimer.Contains("GL_TIMESTAMP") -and $opt00GpuTimer.Contains("GL_QUERY_RESULT_AVAILABLE") -and -not $opt00GpuTimer.Contains("glFinish(")) "OPT00 GPU pass timing uses non-blocking timestamp pairs"
$opt00EngineRendering = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRendering.cpp")
Check ($opt00EngineRendering.Contains("GpuPerformanceSection::MeshRenderer") -and $opt00EngineRendering.Contains("postProcessGpuTimer")) "OPT00 durable top-level render passes publish asynchronous GPU timings"
$opt00SkyRenderer = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp")
Check ($opt00SkyRenderer.Contains("cloudRaymarchGpuTimer") -and $opt00SkyRenderer.Contains("cloudTemporalGpuTimer") -and $opt00SkyRenderer.Contains("cloudShadowGpuTimer")) "OPT00 sky/cloud GPU timing separates shadow, raymarch and temporal costs"
$opt00CodeHealthPath = Join-Path $Root "Tools\Diagnostics\CodeHealthAudit.ps1"
$opt00CodeHealth = ReadText $opt00CodeHealthPath
Check ((Test-Path $opt00CodeHealthPath) -and $opt00CodeHealth.Contains("UNCOMPILED .CPP INVENTORY") -and $opt00CodeHealth.Contains("UNREACHABLE LUA INVENTORY")) "OPT00 repeatable code-health snapshot tracks dead-code evidence"
$opt01CodeHealthReportPath = Join-Path $Root "Build\Reports\CodeHealthSnapshot.txt"
$opt01CodeHealthReport = ReadText $opt01CodeHealthReportPath
Check ($opt01CodeHealthReport.Contains("Project-tree .cpp not compiled by those active projects: 0") -and $opt01CodeHealthReport.Contains("Unreachable Racing United Lua files: 0")) "OPT01 active project tree has zero uncompiled cpp translation units and zero unreachable Racing United Lua files"
Check (Test-Path (Join-Path $Root "Docs\OPT01_PROVEN_DEAD_CODE_RETIREMENT.md")) "OPT01 dead-code retirement is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\OPT01_ProvenDeadCodeRetirement.txt")) "OPT01 milestone report is present"

# ARCH05: the Lua runtime is a lifecycle/registration coordinator. Binding
# implementation growth belongs in domain translation units, with another split
# before any individual binding file becomes a new monolith.
$runtimeLineCount = if (Test-Path $runtimeCppPath) { (Get-Content $runtimeCppPath).Count } else { 999999 }
Check ($runtimeLineCount -lt 3000) "LuaModuleRuntime.cpp remains a compact runtime coordinator"
Check ($luaBindingCppFiles.Count -ge 20) "Lua bindings are decomposed into domain translation units"
$oversizedLuaBindingFiles = @($luaBindingCppFiles | Where-Object { (Get-Content $_.FullName).Count -ge 1200 })
Check ($oversizedLuaBindingFiles.Count -eq 0) "no Lua binding implementation file has become a 1200+ line dumping ground"
$runtimeBindingDefinitions = @([regex]::Matches($runtimeCpp, 'LuaModuleRuntime::(?<handler>lua[A-Za-z0-9_]+)\s*\('))
Check ($runtimeBindingDefinitions.Count -eq 1 -and $runtimeBindingDefinitions[0].Groups['handler'].Value -eq 'luaPrint') "LuaModuleRuntime.cpp retains only the global print binding implementation"

# CLEAN12: the public/runtime declaration surface stays small; Lua C handlers live
# behind private domain catalogues and each binding file includes its actual service owner.
$runtimeHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.hpp"
$runtimeHeader = ReadText $runtimeHeaderPath
$runtimeHeaderLineCount = if (Test-Path $runtimeHeaderPath) { (Get-Content $runtimeHeaderPath).Count } else { 999999 }
$bindingHandlerHeaders = @(
    "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaCoreBindingHandlers.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsBindingHandlers.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Vehicle\LuaVehicleBindingHandlers.hpp",
    "Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityBindingHandlers.hpp"
)
foreach ($relativePath in $bindingHandlerHeaders) {
    Check (Test-Path (Join-Path $Root $relativePath)) "CLEAN12 private binding-handler catalogue exists: $relativePath"
}
Check ($runtimeHeaderLineCount -lt 300) "CLEAN12 LuaModuleRuntime.hpp remains a compact runtime declaration surface"
Check (-not $runtimeHeader.Contains("static int luaVehicle") -and -not $runtimeHeader.Contains("static int luaPhysics") -and -not $runtimeHeader.Contains("static int luaEntity") -and -not $runtimeHeader.Contains("static int luaUi")) "CLEAN12 domain Lua C handlers stay out of LuaModuleRuntime.hpp"
Check ($runtimeHeader.Contains("friend struct LuaCoreBindingHandlers") -and $runtimeHeader.Contains("friend struct LuaPhysicsBindingHandlers") -and $runtimeHeader.Contains("friend struct LuaVehicleBindingHandlers") -and $runtimeHeader.Contains("friend struct LuaEntityBindingHandlers")) "CLEAN12 runtime grants binding access only through explicit domain catalogues"
Check (-not $runtimeHeader.Contains("Audio/AudioSystem.hpp") -and -not $runtimeHeader.Contains("Physics/PhysicsWorld.hpp") -and -not $runtimeHeader.Contains("Input/InputSystem.hpp") -and -not $runtimeHeader.Contains("Graphics/EnvironmentSystem.hpp")) "CLEAN12 runtime header no longer drags service implementation headers into every binding translation unit"
$legacyDomainMemberDefinitions = @([regex]::Matches($luaBindingCpp, 'LuaModuleRuntime::lua[A-Za-z0-9_]+\s*\('))
Check ($legacyDomainMemberDefinitions.Count -eq 0) "CLEAN12 binding translation units use private domain handler owners instead of runtime member handlers"
$manifestGenerator = ReadText (Join-Path $Root "Tools\GenerateLuaApiManifest.ps1")
Check ($manifestGenerator.Contains("handlerOwnerPattern") -and $manifestGenerator.Contains("Lua(?:Core|Physics|Vehicle|Entity)BindingHandlers")) "CLEAN12 Lua API manifest resolves domain-owned handler implementations"
$validatorMain = ReadText (Join-Path $Root "Tools\ValidateProject.ps1")
$validatorModuleRoot = Join-Path $Root "Tools\Validation"
$validatorModules = if (Test-Path $validatorModuleRoot) { @(Get-ChildItem -Path $validatorModuleRoot -File -Filter "*.ps1") } else { @() }
Check ($validatorModules.Count -ge 5) "CLEAN12 validator responsibilities are split under Tools/Validation"
Check ($validatorMain.Contains("Validation\00_FoundationAndLuaApi.ps1") -and $validatorMain.Contains('. $modulePath')) "CLEAN12 top-level validator remains the single ordered validation entry point"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-059-Lua-Handler-Boundary-And-Modular-Validation.md")) "CLEAN12 architecture decision is documented"


# FIX01: vector-valued Lua bindings must consume three distinct, consecutive
# arguments. Historical copy/paste mistakes silently corrupted wheel-node
# rotations, prefab rotations and spring anchors while still compiling.
Check ([regex]::IsMatch($luaBindingCpp, '(?s)luaEntitySetMeshNodeWorldPose.*?const heritage::math::Vec3 rotation\{.*?state, 6,.*?state, 7,.*?state, 8,')) "mesh-node world-pose Lua rotation maps XYZ arguments 6/7/8"
Check ([regex]::IsMatch($luaBindingCpp, '(?s)luaEntitySetMeshNodeAnchoredWorldPose.*?const heritage::math::Vec3 rotation\{.*?state, 7,.*?state, 8,.*?state, 9,')) "anchored mesh-node pose Lua rotation maps XYZ arguments 7/8/9"
Check ([regex]::IsMatch($luaBindingCpp, '(?s)luaEntitySetMeshNodeAnchoredWorldDelta.*?const heritage::math::Vec3 rotationDelta\{.*?state, 7,.*?state, 8,.*?state, 9,')) "anchored mesh-node delta Lua rotation maps XYZ arguments 7/8/9"
Check ([regex]::IsMatch($luaBindingCpp, '(?s)luaPhysicsCreateSpringConstraint.*?description\.anchorB = \{.*?state, 6,.*?state, 7,.*?state, 8,')) "spring anchor-B Lua vector maps XYZ arguments 6/7/8"
Check ([regex]::IsMatch($luaBindingCpp, '(?s)luaPrefabInstantiate.*?options\.rotationDegrees = \{.*?state, 6,.*?state, 7,.*?state, 8,')) "prefab rotation Lua vector maps XYZ arguments 6/7/8"


# PERF06: do not put controller hardware enumeration back on a periodic gameplay
# timer. DirectInput EnumDevices is allowed at startup and explicit user refresh
# only; already-known devices are still polled every frame.
$directInputPath = Join-Path $Root "Engine\HeritageEngine\Input\WindowsDirectInputBackend.cpp"
$directInputCpp = if (Test-Path $directInputPath) { [IO.File]::ReadAllText($directInputPath) } else { "" }
Check (-not $directInputCpp.Contains("kDeviceRefreshSeconds")) "DirectInput has no periodic device-enumeration timer"
Check (-not $directInputCpp.Contains("refreshDevices(false)")) "DirectInput per-frame update does not enumerate hardware"
Check ($directInputCpp.Contains("void WindowsDirectInputBackend::refreshDevices()")) "DirectInput exposes explicit manual device refresh"
Check ($directInputCpp.Contains("void WindowsDirectInputBackend::beginCapture()") -and $directInputCpp.Contains("captureAxes") -and $directInputCpp.Contains("strongestMovement") -and $directInputCpp.Contains("device.axes[axisSlot] - baseline")) "INPUT G29 axis capture measures deliberate movement from a per-binding baseline and chooses the strongest moved axis"
Check ($directInputCpp.Contains("GetObjectInfo(") -and $directInputCpp.Contains("DIPH_BYOFFSET") -and $directInputCpp.Contains("kAxisStateOffsets") -and -not $directInputCpp.Contains("axisIndexFromOffset(object->dwOfs)")) "INPUT04 DirectInput maps wheel axes through the current c_dfDIJoystick2 data format instead of treating native EnumObjects offsets as DIJOYSTATE2 offsets"
Check ($directInputCpp.Contains("directInputButtonPressed") -and $directInputCpp.Contains("value & 0x80u") -and $directInputCpp.Contains("kNeutralSettleStableFrames") -and $directInputCpp.Contains("neutralCandidateAxes") -and $directInputCpp.Contains("neutralCalibrated") -and $directInputCpp.Contains("kEndpointNeutralThreshold") -and $directInputCpp.Contains("device.neutralAxes[axisSlot] = baseline") -and -not $directInputCpp.Contains("kNeutralNoiseDeadzone") -and -not $directInputCpp.Contains("device.neutralAxes[axis] = 0.0f")) "INPUT09 DirectInput keeps stable pedal rest calibration but never inserts a hidden live-axis deadzone"

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

$rigidBodyHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\RigidBodySystem.hpp"
$rigidBodyHeader = if (Test-Path $rigidBodyHeaderPath) { [IO.File]::ReadAllText($rigidBodyHeaderPath) } else { "" }

$vehicleHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystem.hpp"
$vehicleHeader = if (Test-Path $vehicleHeaderPath) { [IO.File]::ReadAllText($vehicleHeaderPath) } else { "" }
# CLEAN02/CLEAN03A: VehicleSystem implementation is split by stable responsibility, and
# the broad configuration bucket is further split into subsystem-owned translation units.
$vehicleConfigurationRelativePaths = @(
    "Engine\HeritageEngine\Vehicles\Core\VehicleRuntimeConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\VehicleSuspensionConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Suspension\Common\VehicleAntiRollBarConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Fitment\VehicleWheelFitmentConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Wheels\Alignment\VehicleWheelAlignmentConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\ChassisFlex\VehicleChassisComplianceConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Dynamics\MassProperties\VehicleUnsprungMassConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Steering\VehicleSteeringConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Brakes\VehicleBrakeConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\DriverAids\VehicleDriverAidConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Drivetrain\VehicleDrivetrainConfiguration.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\VehicleTireConfiguration.cpp"
)
$vehicleCoreSourceRelativePaths = @(
    "Engine\HeritageEngine\Vehicles\VehicleSystem.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleTelemetry.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleSimulation.cpp",
    "Engine\HeritageEngine\Vehicles\VehicleWheelSimulation.cpp"
)
$wheelSubstepPhaseRelativePaths = @(
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\00_PrepareWheelAndSupportQuery.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\01_TelemetryAndAirbornePolicy.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\02_SteeringBrakingAndFreeWheel.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\03_RoadEnvelopeAndFootprintSampling.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\04_TireStructureAndTerrainSupport.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\05_SuspensionAndContactResolution.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\06_ContactKinematicsAndPatchGeometry.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\07_SurfaceProvidersAndContactPatch.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\08_TireForcesAndSurfaceReactions.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\09_TirePhysicalStateUpdate.inl",
    "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\10_ApplyForcesAndIntegrateWheel.inl"
)
$vehicleSourceRelativePaths = @($vehicleCoreSourceRelativePaths + $vehicleConfigurationRelativePaths)
$vehicleSourceTextByPath = @{}
foreach ($relativePath in $vehicleSourceRelativePaths) {
    $sourcePath = Join-Path $Root $relativePath
    Check (Test-Path $sourcePath) "vehicle responsibility source exists: $relativePath"
    $vehicleSourceTextByPath[$relativePath] = if (Test-Path $sourcePath) { [IO.File]::ReadAllText($sourcePath) } else { "" }
}
$wheelSubstepPhaseTextByPath = @{}
foreach ($relativePath in $wheelSubstepPhaseRelativePaths) {
    $phasePath = Join-Path $Root $relativePath
    Check (Test-Path $phasePath) "CLEAN03B wheel-substep phase exists: $relativePath"
    $wheelSubstepPhaseTextByPath[$relativePath] = if (Test-Path $phasePath) { [IO.File]::ReadAllText($phasePath) } else { "" }
}
$wheelSubstepPhaseText = ($wheelSubstepPhaseRelativePaths | ForEach-Object { $wheelSubstepPhaseTextByPath[$_] }) -join "`n"
$tire26RemovedWheelSubstepStateAbsent =
    -not $wheelSubstepPhaseText.Contains("cachedTireColliderTriangles") -and
    -not $wheelSubstepPhaseText.Contains("cachedTirePrimitiveSurfaces") -and
    -not $wheelSubstepPhaseText.Contains("cachedTireCarcass3DPotential") -and
    -not $wheelSubstepPhaseText.Contains("carcassContactActiveLastSubstep") -and
    -not $wheelSubstepPhaseText.Contains("tireCarcass3DContactValid")
Check $tire26RemovedWheelSubstepStateAbsent "TIRE26 wheel substeps contain no stale TIRE18-TIRE21 carcass-state references"

$vehicleInternalHeaderPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystemInternal.hpp"
$vehicleInternalHeader = if (Test-Path $vehicleInternalHeaderPath) { [IO.File]::ReadAllText($vehicleInternalHeaderPath) } else { "" }
Check (Test-Path $vehicleInternalHeaderPath) "CLEAN02 shared VehicleSystem internal helper header exists"

$vehicleSystemCpp = $vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\VehicleSystem.cpp"]
$vehicleTelemetryCpp = $vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\VehicleTelemetry.cpp"]
$vehicleSimulationCpp = $vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\VehicleSimulation.cpp"]
$vehicleWheelSimulationCpp = $vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\VehicleWheelSimulation.cpp"]
$vehicleConfigurationCpp = ($vehicleConfigurationRelativePaths | ForEach-Object { $vehicleSourceTextByPath[$_] }) -join "`n"
# Include CLEAN03B function-scope phase fragments in the implementation aggregate so
# tire/surface guards follow the authoritative solver instead of assuming all text
# remains physically inside VehicleWheelSimulation.cpp.
$vehicleCpp = (($vehicleSourceRelativePaths | ForEach-Object { $vehicleSourceTextByPath[$_] }) -join "`n") + "`n" + $wheelSubstepPhaseText + "`n" + $vehicleInternalHeader

Check ($vehicleSystemCpp.Contains("VehicleSystem::create") -and -not $vehicleSystemCpp.Contains("simulateWheelSubstep")) "CLEAN02 VehicleSystem.cpp owns lifetime/plumbing, not the high-rate wheel solver"
Check ($vehicleConfigurationCpp.Contains("setWheelSuspensionModel") -and $vehicleConfigurationCpp.Contains("setWheelTireProvider")) "CLEAN03A subsystem configuration files collectively retain the complete configuration API"
Check ($vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\Suspension\VehicleSuspensionConfiguration.cpp"].Contains("setWheelSuspensionModel")) "CLEAN03A suspension configuration has explicit ownership"
Check ($vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\Wheels\Alignment\VehicleWheelAlignmentConfiguration.cpp"].Contains("setWheelAlignment")) "CLEAN03A alignment configuration has explicit ownership"
Check ($vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\Suspension\Common\VehicleAntiRollBarConfiguration.cpp"].Contains("setAntiRollBar")) "CLEAN03A anti-roll-bar configuration has explicit ownership"
Check ($vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\Drivetrain\VehicleDrivetrainConfiguration.cpp"].Contains("setPowertrain")) "CLEAN03A drivetrain configuration has explicit ownership"
Check ($vehicleSourceTextByPath["Engine\HeritageEngine\Vehicles\Tires\VehicleTireConfiguration.cpp"].Contains("setWheelTireProvider")) "CLEAN03A tire configuration has explicit ownership"
Check ($vehicleTelemetryCpp.Contains("startDynamicsLabCapture") -and $vehicleTelemetryCpp.Contains("captureDynamicsLabFrame")) "CLEAN02 VehicleTelemetry.cpp owns Dynamics Lab capture/readback"
Check ($vehicleSimulationCpp.Contains("VehicleSystem::simulate") -and $vehicleSimulationCpp.Contains("prepareAntiRollBarForces") -and -not $vehicleSimulationCpp.Contains("void VehicleSystem::simulateWheelSubstep")) "CLEAN02 VehicleSimulation.cpp owns vehicle-level stepping/orchestration"
Check ($vehicleWheelSimulationCpp.Contains("void VehicleSystem::simulateWheelSubstep")) "CLEAN02 VehicleWheelSimulation.cpp owns the authoritative high-rate wheel solver"
$wheelPhaseIncludeOrderValid = $true
$wheelPhaseCursor = -1
foreach ($relativePath in $wheelSubstepPhaseRelativePaths) {
    $phaseName = Split-Path $relativePath -Leaf
    $needle = '#include "Simulation/WheelSubstep/' + $phaseName + '"'
    $nextIndex = $vehicleWheelSimulationCpp.IndexOf($needle)
    if ($nextIndex -lt 0 -or $nextIndex -le $wheelPhaseCursor) {
        $wheelPhaseIncludeOrderValid = $false
        break
    }
    $wheelPhaseCursor = $nextIndex
}
Check $wheelPhaseIncludeOrderValid "CLEAN03B wheel solver orchestrates all named phases in authoritative order"
Check ($vehicleWheelSimulationCpp.Split("`n").Count -lt 120) "CLEAN03B VehicleWheelSimulation.cpp is an orchestrator instead of a multi-thousand-line procedure"
Check ($wheelSubstepPhaseText.Contains("integrateTireSlipDynamics") -and $wheelSubstepPhaseText.Contains("integrateTireContactPatch") -and $wheelSubstepPhaseText.Contains("advanceTireThermal") -and ($wheelSubstepPhaseText.Contains("surfaces.applyDeformable") -or $wheelSubstepPhaseText.Contains("m_surfaceField.apply"))) "CLEAN03B phase aggregate retains core tire/contact/thermal/terrain mechanisms"
$wheelSubstepReadmePath = Join-Path $Root "Engine\HeritageEngine\Vehicles\Simulation\WheelSubstep\README.md"
$wheelSubstepReadme = if (Test-Path $wheelSubstepReadmePath) { [IO.File]::ReadAllText($wheelSubstepReadmePath) } else { "" }
Check ($wheelSubstepReadme.Contains("exactly-four-wheel assumption") -and $wheelSubstepReadme.Contains("graduate")) "CLEAN03B phase contract documents arbitrary-wheel topology and future helper graduation"


# CLEAN04: shared quaternion algebra remains centralized and CLEAN04B now
# compiles the collision implementation from stable responsibility units.
$quaternionMathPath = Join-Path $Root "Engine\HeritageEngine\Core\Math\Quaternion.hpp"
$transformMathPath = Join-Path $Root "Engine\HeritageEngine\Core\Math\TransformMath.hpp"
$quaternionMath = if (Test-Path $quaternionMathPath) { [IO.File]::ReadAllText($quaternionMathPath) } else { "" }
$transformMath = if (Test-Path $transformMathPath) { [IO.File]::ReadAllText($transformMathPath) } else { "" }
Check (Test-Path $quaternionMathPath) "CLEAN04A shared Quaternion.hpp exists"
Check (Test-Path $transformMathPath) "CLEAN04A TransformMath.hpp ownership scaffold exists"
Check ($quaternionMath.Contains("struct Quaternion") -and $quaternionMath.Contains("makeQuaternionFromEulerDegrees") -and $quaternionMath.Contains("rotateVectorUnit") -and $quaternionMath.Contains("rotateVectorGeneral")) "CLEAN04A shared quaternion primitives own conversion and rotation algebra"
Check ($transformMath.Contains("Intentionally no policy-bearing helpers yet")) "CLEAN04A transform scaffold resists generic-math dumping-ground growth"
Check ($rigidBodyHeader.Contains("using Quaternion = heritage::math::Quaternion") -and -not $rigidBodyHeader.Contains("struct Quaternion")) "CLEAN04A rigid bodies use shared quaternion representation"
$entityRegistryHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Entities\EntityRegistry.hpp"
$entityRegistryHeader = if (Test-Path $entityRegistryHeaderPath) { [IO.File]::ReadAllText($entityRegistryHeaderPath) } else { "" }
Check ($entityRegistryHeader.Contains("using Quaternion = heritage::math::Quaternion") -and -not $entityRegistryHeader.Contains("struct Quaternion")) "CLEAN04A entity hierarchy uses shared quaternion representation"
$collisionSourcePath = Join-Path $Root "Engine\HeritageEngine\Physics\CollisionSystem.cpp"
$collisionSource = if (Test-Path $collisionSourcePath) { [IO.File]::ReadAllText($collisionSourcePath) } else { "" }
Check ($collisionSource.Contains("heritage::math::rotateVectorUnit") -and $collisionSource.Contains("heritage::math::conjugate")) "CLEAN04A collision rotation wrappers use shared quaternion algebra"
Check ($vehicleInternalHeader.Contains("using Quaternion = heritage::math::Quaternion") -and $vehicleInternalHeader.Contains("heritage::math::rotateVectorUnit")) "CLEAN04A vehicle simulation uses shared quaternion representation/algebra"
$collisionResponsibilityRelativePaths = @(
    "Engine\HeritageEngine\Physics\Collision\Broadphase\CollisionBroadphase.cpp",
    "Engine\HeritageEngine\Physics\Collision\Queries\CollisionQueries.cpp",
    "Engine\HeritageEngine\Physics\Collision\Narrowphase\CollisionNarrowphase.cpp",
    "Engine\HeritageEngine\Physics\Collision\Solver\CollisionSolver.cpp",
    "Engine\HeritageEngine\Physics\Collision\CCD\CollisionCCD.cpp",
    "Engine\HeritageEngine\Physics\Collision\Islands\CollisionIslands.cpp"
)
$clean04EngineProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$clean04EngineProject = if (Test-Path $clean04EngineProjectPath) { [IO.File]::ReadAllText($clean04EngineProjectPath) } else { "" }
$clean04TestProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$clean04TestProject = if (Test-Path $clean04TestProjectPath) { [IO.File]::ReadAllText($clean04TestProjectPath) } else { "" }
foreach ($relativePath in $collisionResponsibilityRelativePaths) {
    $implementationPath = Join-Path $Root $relativePath
    $projectRelative = $relativePath.Replace("Engine\HeritageEngine\", "..\")
    Check (Test-Path $implementationPath) "CLEAN04B collision responsibility implementation exists: $relativePath"
    Check ($clean04EngineProject.Contains('ClCompile Include="' + $projectRelative + '"')) "CLEAN04B engine compiles collision responsibility unit: $relativePath"
    Check (-not $clean04EngineProject.Contains('None Include="' + $projectRelative + '"')) "CLEAN04B collision responsibility unit is no longer a non-compiled scaffold: $relativePath"
    Check ($clean04TestProject.Contains('ClCompile Include="' + $projectRelative + '"')) "CLEAN04B physics regression project compiles collision responsibility unit: $relativePath"
}
$collisionInternalPath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\CollisionInternal.hpp"
$collisionInternal = if (Test-Path $collisionInternalPath) { [IO.File]::ReadAllText($collisionInternalPath) } else { "" }
Check ($collisionInternal.Contains("namespace heritage::physics::collision_detail") -and $collisionInternal.Contains("kContactEpsilon")) "CLEAN04B shared private collision numerical vocabulary is explicit"
$collisionBroadphasePath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\Broadphase\CollisionBroadphase.cpp"
$collisionBroadphase = if (Test-Path $collisionBroadphasePath) { [IO.File]::ReadAllText($collisionBroadphasePath) } else { "" }
$collisionQueriesPath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\Queries\CollisionQueries.cpp"
$collisionQueries = if (Test-Path $collisionQueriesPath) { [IO.File]::ReadAllText($collisionQueriesPath) } else { "" }
$collisionNarrowphasePath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\Narrowphase\CollisionNarrowphase.cpp"
$collisionNarrowphase = if (Test-Path $collisionNarrowphasePath) { [IO.File]::ReadAllText($collisionNarrowphasePath) } else { "" }
$collisionSolverPath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\Solver\CollisionSolver.cpp"
$collisionSolver = if (Test-Path $collisionSolverPath) { [IO.File]::ReadAllText($collisionSolverPath) } else { "" }
$collisionCcdPath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\CCD\CollisionCCD.cpp"
$collisionCcd = if (Test-Path $collisionCcdPath) { [IO.File]::ReadAllText($collisionCcdPath) } else { "" }
$collisionIslandsPath = Join-Path $Root "Engine\HeritageEngine\Physics\Collision\Islands\CollisionIslands.cpp"
$collisionIslands = if (Test-Path $collisionIslandsPath) { [IO.File]::ReadAllText($collisionIslandsPath) } else { "" }
Check ($collisionBroadphase.Contains("CollisionSystem::collectBroadphaseContacts") -and $collisionBroadphase.Contains("std::vector<BroadphaseProxy>") -and $collisionBroadphase.Contains("m_broadphaseCandidateCount")) "CLEAN13 broadphase unit owns candidate/contact-set collection"
Check ($collisionQueries.Contains("CollisionSystem::raycast") -and $collisionQueries.Contains("CollisionSystem::sphereCast") -and $collisionQueries.Contains("CollisionSystem::overlapSphereCount")) "CLEAN04B query unit owns ray/sphere/overlap scene queries"
Check ($collisionNarrowphase.Contains("CollisionSystem::generateContact") -and $collisionNarrowphase.Contains("CollisionSystem::boxBoxContact") -and $collisionNarrowphase.Contains("CollisionSystem::generateStaticTriangleContact")) "CLEAN04B narrowphase unit owns primitive/static-triangle contact generation"
Check ($collisionSolver.Contains("CollisionSystem::warmStartContact") -and $collisionSolver.Contains("CollisionSystem::resolveVelocity") -and $collisionSolver.Contains("CollisionSystem::persistContactCache")) "CLEAN04B solver unit owns warm start/cache/constraint response"
Check ($collisionCcd.Contains("CollisionSystem::applyContinuousCollisionDetection")) "CLEAN04B CCD unit owns continuous collision protection"
Check ($collisionIslands.Contains("CollisionSystem::updateSimulationIslandsAndSleeping")) "CLEAN04B islands unit owns island construction and sleep/wake"
Check (-not $collisionSource.Contains("CollisionSystem::raycast(") -and -not $collisionSource.Contains("CollisionSystem::generateContact(") -and -not $collisionSource.Contains("CollisionSystem::resolveVelocity(") -and -not $collisionSource.Contains("CollisionSystem::applyContinuousCollisionDetection(") -and -not $collisionSource.Contains("CollisionSystem::updateSimulationIslandsAndSleeping(")) "CLEAN04B root CollisionSystem.cpp is coordinator/lifecycle rather than subsystem dumping ground"
Check ($collisionSource.Split("`n").Count -lt 1500) "CLEAN04B root CollisionSystem.cpp stays below coordinator size guard"

# CLEAN05: EntityMeshRenderer implementation ownership is physically split while
# the public facade stays stable. Validate project inclusion and durable ownership,
# not comment formatting or incidental line positions.
$clean05RendererRoot = "Engine\HeritageEngine\Graphics\Renderer"
$clean05RendererProject = $clean04EngineProject
$clean05SourceNames = @(
    "EntityMeshRenderer.cpp",
    "EntityMeshAssetCache.cpp",
    "EntityMeshAnimation.cpp",
    "EntityMeshShadows.cpp",
    "EntityMeshRenderMath.cpp",
    "EntityMeshProfiling.cpp"
)
$clean05HeaderNames = @(
    "EntityMeshRendererInternal.hpp",
    "EntityMeshShaders.hpp",
    "EntityMeshShadowConfig.hpp"
)
$clean05Text = @{}
foreach ($name in $clean05SourceNames + $clean05HeaderNames) {
    $relativePath = "$clean05RendererRoot\$name"
    $absolutePath = Join-Path $Root $relativePath
    Check (Test-Path $absolutePath) "CLEAN05 renderer ownership file exists: $relativePath"
    $clean05Text[$name] = if (Test-Path $absolutePath) { [IO.File]::ReadAllText($absolutePath) } else { "" }
}
foreach ($name in $clean05SourceNames) {
    $projectRelative = "..\Graphics\Renderer\$name"
    Check ($clean05RendererProject.Contains('ClCompile Include="' + $projectRelative + '"')) "CLEAN05 engine compiles renderer responsibility unit: $name"
}
foreach ($name in $clean05HeaderNames) {
    $projectRelative = "..\Graphics\Renderer\$name"
    Check ($clean05RendererProject.Contains('ClInclude Include="' + $projectRelative + '"')) "CLEAN05 engine tracks renderer private/header ownership: $name"
}
$clean05RootRenderer = $clean05Text["EntityMeshRenderer.cpp"]
$clean05AssetCache = $clean05Text["EntityMeshAssetCache.cpp"]
$clean05Animation = $clean05Text["EntityMeshAnimation.cpp"]
$clean05Shadows = $clean05Text["EntityMeshShadows.cpp"]
$clean05Shaders = $clean05Text["EntityMeshShaders.hpp"]
$clean05ShadowConfig = $clean05Text["EntityMeshShadowConfig.hpp"]
Check ($clean05RootRenderer.Contains("EntityMeshRenderer::draw(") -and $clean05RootRenderer.Contains("EntityMeshRenderer::initialize(") -and -not $clean05RootRenderer.Contains("EntityMeshRenderer::acquireMesh(") -and -not $clean05RootRenderer.Contains("EntityMeshRenderer::initializeShadowResources(") -and -not $clean05RootRenderer.Contains("EntityMeshRenderer::animationTransformsForInstance(")) "CLEAN05 root EntityMeshRenderer.cpp owns lifecycle/draw orchestration only"
$clean05RootRendererLineCount = if (Test-Path (Join-Path $Root "$clean05RendererRoot\EntityMeshRenderer.cpp")) { [IO.File]::ReadAllLines((Join-Path $Root "$clean05RendererRoot\EntityMeshRenderer.cpp")).Count } else { 999999 }
Check ($clean05RootRendererLineCount -lt 1200) "CLEAN05 root EntityMeshRenderer.cpp stays below orchestrator size guard"
$perf01Profiling = $clean05Text["EntityMeshProfiling.cpp"]
Check ($perf01Profiling.Contains("copySkyPerformanceStats(") -and $perf01Profiling.Contains("drawElementsProfiled(")) "PERF01A mesh/cloud attribution helpers have a dedicated compiled owner instead of bloating the renderer orchestrator"

# OPT04A: large renderers are orchestration owners, while persistent GPU-cache
# responsibilities and embedded GLSL live in dedicated translation units.
$opt04ProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$opt04Project = ReadText $opt04ProjectPath
$opt04SurfaceRootPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.cpp"
$opt04SurfaceTireMarksPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationTireMarks.cpp"
$opt04SurfaceRubberPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRubber.cpp"
$opt04SurfaceShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationShaders.cpp"
$opt04SkyRootPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp"
$opt04SkyAtmosphereShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererAtmosphereShaders.cpp"
$opt04SkyCloudShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererCloudShaders.cpp"
$opt04SkyPbrAtmospherePath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererPbrAtmosphere.cpp"
$opt04SkyPbrShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererPbrAtmosphereShaders.cpp"
foreach ($rendererOwnerPath in @(
    $opt04SurfaceRootPath,
    $opt04SurfaceTireMarksPath,
    $opt04SurfaceRubberPath,
    $opt04SurfaceShadersPath,
    $opt04SkyRootPath,
    $opt04SkyAtmosphereShadersPath,
    $opt04SkyCloudShadersPath,
    $opt04SkyPbrAtmospherePath,
    $opt04SkyPbrShadersPath
)) {
    Check (Test-Path $rendererOwnerPath) ("OPT04A renderer ownership file exists: " + [IO.Path]::GetFileName($rendererOwnerPath))
}
foreach ($rendererOwnerName in @(
    "SurfacePresentationRenderer.cpp",
    "SurfacePresentationTireMarks.cpp",
    "SurfacePresentationRubber.cpp",
    "SurfacePresentationShaders.cpp",
    "SkyRenderer.cpp",
    "SkyRendererAtmosphereShaders.cpp",
    "SkyRendererCloudShaders.cpp",
    "SkyRendererPbrAtmosphere.cpp",
    "SkyRendererPbrAtmosphereShaders.cpp"
)) {
    Check ($opt04Project.Contains('ClCompile Include="..\Graphics\Renderer\' + $rendererOwnerName + '"')) ("OPT04A engine compiles renderer owner: " + $rendererOwnerName)
}
$opt04SurfaceRootLines = (Get-Content $opt04SurfaceRootPath).Count
$opt04SkyRootLines = (Get-Content $opt04SkyRootPath).Count
Check ($opt04SurfaceRootLines -lt 1000 -and $opt04SkyRootLines -lt 1000) "OPT04A SurfacePresentationRenderer and SkyRenderer roots remain sub-1000-line orchestration owners"
Check (
    (ReadText $opt04SurfaceTireMarksPath).Contains("syncTireMarkGpuCache") -and
    (ReadText $opt04SurfaceRubberPath).Contains("syncMarbleGpuCache") -and
    (ReadText $opt04SurfaceShadersPath).Contains("kTireMarkGeometryShader") -and
    (ReadText $opt04SkyAtmosphereShadersPath).Contains("kSkyVertexShader") -and
    (ReadText $opt04SkyPbrShadersPath).Contains("kPbrSkyFragmentShader") -and
    (ReadText $opt04SkyCloudShadersPath).Contains("kCloudRaymarchFragmentShader")
) "OPT04A renderer cache and GLSL responsibilities remain physically split across their current owners"


# OPT04B: hot renderer paths do not synchronously query driver state that the
# frame orchestrator already owns, and simple fullscreen/logo shaders cache
# uniform locations once instead of resolving names on every draw.
$opt04bEntityPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$opt04bEntityHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.hpp"
$opt04bWeatherPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WeatherPresentationRenderer.cpp"
$opt04bWeatherHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WeatherPresentationRenderer.hpp"
$opt04bSkyPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp"
$opt04bSkyHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.hpp"
$opt04bPostPath = Join-Path $Root "Engine\HeritageEngine\Graphics\PostProcessing\PostProcessor.cpp"
$opt04bPostHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\PostProcessing\PostProcessor.hpp"
$opt04bScenePath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SceneRenderer.cpp"
$opt04bSceneHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SceneRenderer.hpp"
$opt04bEngineRenderingPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRendering.cpp"
$opt04bEntity = ReadText $opt04bEntityPath
$opt04bEntityHeader = ReadText $opt04bEntityHeaderPath
$opt04bWeather = ReadText $opt04bWeatherPath
$opt04bWeatherHeader = ReadText $opt04bWeatherHeaderPath
$opt04bSky = ReadText $opt04bSkyPath
$opt04bSkyHeader = ReadText $opt04bSkyHeaderPath
$opt04bPost = ReadText $opt04bPostPath
$opt04bPostHeader = ReadText $opt04bPostHeaderPath
$opt04bScene = ReadText $opt04bScenePath
$opt04bSceneHeader = ReadText $opt04bSceneHeaderPath
$opt04bEngineRendering = ReadText $opt04bEngineRenderingPath
Check (
    -not $opt04bWeather.Contains("glIsEnabled(GL_DEPTH_TEST)") -and
    -not $opt04bWeather.Contains("glGetBooleanv(GL_DEPTH_WRITEMASK") -and
    -not $opt04bWeather.Contains("glGetIntegerv(GL_ACTIVE_TEXTURE") -and
    -not $opt04bWeather.Contains("glGetIntegerv(GL_VIEWPORT") -and
    $opt04bWeatherHeader.Contains("GLsizei viewportWidth") -and
    $opt04bWeatherHeader.Contains("GLsizei viewportHeight") -and
    $opt04bEngineRendering.Contains("entityMeshRenderer.environmentMap(),`n                    fboW,`n                    fboH") -and
    $opt04bEngineRendering.Contains("entityMeshRenderer.environmentMap(),`n                rW,`n                rH")
) "OPT04B precipitation renderer consumes orchestrator viewport state without per-frame GL state capture"
Check (
    -not $opt04bEntity.Contains("glIsEnabled(GL_BLEND)") -and
    -not $opt04bEntity.Contains("for (int unit = 0; unit < 9; ++unit)") -and
    $opt04bEntity.Contains("glBindSampler(10, 0)") -and
    $opt04bEntity.Contains("glBindSampler(11, 0)") -and
    $opt04bEntity.Contains("glDisable(GL_BLEND)")
) "OPT04B entity renderer avoids driver blend-state query and blanket texture-unit zeroing while releasing shadow samplers"
Check (
    -not $opt04bSky.Contains("glGetIntegerv(GL_SAMPLES") -and
    -not $opt04bSky.Contains("for(int unit=7;unit>=0;--unit)") -and
    $opt04bSkyHeader.Contains("GLsizei samples = 1") -and
    $opt04bEntityHeader.Contains("GLsizei samples = 1") -and
    $opt04bEntity.Contains("renderTargetState.samples") -and
    $opt04bEngineRendering.Contains("needMSAA ? antiAliasing.msaaSamples : 1")
) "OPT04B volumetric clouds receive framebuffer sample count from render orchestration without GL_SAMPLES query or blanket texture cleanup"
Check (
    $opt04bPostHeader.Contains("m_fxaaUniformScene") -and
    $opt04bPostHeader.Contains("m_blitUniformNearestNeighbour") -and
    -not $opt04bPost.Contains('glUniform1i(glGetUniformLocation(m_fxaaProgram') -and
    -not $opt04bPost.Contains('glUniform1i(glGetUniformLocation(m_blitProgram')
) "OPT04B post-processing uniform locations are cached at initialization rather than resolved per draw"
Check (
    $opt04bSceneHeader.Contains("m_uniformModel") -and
    $opt04bSceneHeader.Contains("m_uniformSaturation") -and
    -not $opt04bScene.Contains('glUniformMatrix4fv(glGetUniformLocation(m_program') -and
    -not $opt04bScene.Contains('glUniform1f(glGetUniformLocation(m_program')
) "OPT04B scene renderer uniform locations are cached at initialization rather than resolved per draw"

# OPT04C: frame-local mesh/node preparation is evaluated once and shared by the
# layered shadow pass plus normal material pass. Shadow submission also avoids
# redundant per-instance state calls and uploads only live skin joints.
$opt04cAnimation = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshAnimation.cpp")
$opt04cRenderer = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp")
$opt04cHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.hpp")
$opt04cShadows = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshShadows.cpp")
Check (
    $opt04cHeader.Contains("struct PreparedFrameInstance") -and
    $opt04cHeader.Contains("m_preparedFrameInstanceScratch") -and
    $opt04cAnimation.Contains("EntityMeshRenderer::prepareFrameInstances(") -and
    $opt04cRenderer.Contains("prepareFrameInstances(instances, eye, elapsedSeconds)") -and
    $opt04cRenderer.Contains("drawShadowMaps(m_preparedFrameInstanceScratch, renderTargetState)")
) "OPT04C prepares mesh/animation/node state once per frame and shares it with shadow rendering"
Check (
    -not $opt04cShadows.Contains("animationTransformsForInstance(") -and
    -not $opt04cShadows.Contains("applyMeshNodeOverrides(") -and
    -not $opt04cShadows.Contains("acquireMesh(") -and
    $opt04cShadows.Contains("preparedInstance.nodeGlobals") -and
    $opt04cShadows.Contains("preparedInstance.tireVisualOverrides")
) "OPT04C shadow pass consumes shared prepared frame data instead of duplicating mesh/animation/tire setup"
Check (
    $opt04cShadows.Contains("bool cullFaceEnabled = true") -and
    $opt04cShadows.Contains("GLuint activeVao = 0") -and
    $opt04cShadows.Contains("requestedCullFace != cullFaceEnabled") -and
    $opt04cShadows.Contains("mesh.vao != activeVao")
) "OPT04C shadow submission caches per-instance cull and VAO state"
Check (
    $opt04cRenderer.Contains("static_cast<GLsizei>(palette.size())") -and
    $opt04cShadows.Contains("static_cast<GLsizei>(palette.size())") -and
    $opt04cRenderer.Contains("std::array<float, 16 * kMaxSkinJoints> jointData;") -and
    $opt04cShadows.Contains("std::array<float, 16 * kMaxSkinJoints> jointData;")
) "OPT04C visible and shadow skinning upload only actual palette matrices without 64-joint identity padding"

# OPT05: retain the bandwidth-safe history ownership that is compatible with the
# authoritative cloud renderer. CLOUDURP15E6 intentionally retires OPT05's fused
# low-resolution temporal shortcut because the upstream temporal denoiser expects
# a full-resolution scene+cloud accumulation buffer before Pass 3. The old
# shortcut must not silently reappear; ping-pong history remains mandatory.
$opt05Sky = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp")
$opt05SkyHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.hpp")
$opt05CloudShaders = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererCloudShaders.cpp")
Check (
    $opt05SkyHeader.Contains("m_cloudHistoryFbo") -and
    $opt05SkyHeader.Contains("m_cloudHistorySampler") -and
    $opt05Sky.Contains("std::swap(m_cloudTemporalTexture,m_cloudHistoryTexture)") -and
    $opt05Sky.Contains("std::swap(m_cloudTemporalFbo,m_cloudHistoryFbo)") -and
    -not $opt05Sky.Contains("glCopyImageSubData(m_cloudTemporalTexture")
) "OPT05 cloud temporal history still ping-pongs instead of copying a full RGBA16F frame"
Check (
    $opt05Sky.Contains("setup2D(GL_RGBA8,w,h,m_cloudSceneTexture)") -and
    $opt05Sky.Contains("glBindFramebuffer(GL_FRAMEBUFFER,m_cloudCombinedFbo)") -and
    $opt05Sky.Contains("glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,m_cloudSceneTexture)") -and
    $opt05Sky.Contains("glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudCombinedTexture)") -and
    $opt05CloudShaders.Contains("uniform sampler2D uCloudTexture;uniform sampler2D uSceneTexture;uniform bool uBilateral") -and
    -not $opt05CloudShaders.Contains("uniform bool uCurrentLowResolution") -and
    -not $opt05Sky.Contains("temporalCurrentLowResolution=true")
) "CLOUDURP15E6 supersedes OPT05 pass fusion with the upstream full-resolution temporal accumulation path"
Check (
    $opt05Sky.Contains("GL_COLOR_BUFFER_BIT,GL_NEAREST") -and
    $opt05Sky.Contains("GL_DEPTH_BUFFER_BIT,GL_NEAREST") -and
    $opt05Sky.Contains("glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ZERO,GL_ONE)") -and
    $opt05CloudShaders.Contains("FragColor=vec4(cloud.rgb+scene*cloud.a,cloud.a)")
) "CLOUDURP15E6 keeps legal scene/depth resolves and presents the already-resolved temporal camera colour"

# PBSKY01: replace the retired artistic sky fragment with one physical atmosphere
# authority. The Earth preset follows UnityPhysicallyBasedSkyURP's Rayleigh,
# aerosol and ozone model; Heritage owns the OpenGL LUT orchestration and its
# astronomical sun/moon/star integration. Existing volumetric clouds are frozen
# until the separately testable VCLOUD01 migration.
$pbskyRoot = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp")
$pbskyHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.hpp")
$pbskyBaseShaders = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererAtmosphereShaders.cpp")
$pbskyRuntimePath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererPbrAtmosphere.cpp"
$pbskyShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererPbrAtmosphereShaders.cpp"
$pbskyRuntime = ReadText $pbskyRuntimePath
$pbskyShaders = ReadText $pbskyShadersPath
$pbskyNoticePath = Join-Path $Root "Docs\ThirdParty\UnityPhysicallyBasedSkyURP_NOTICE.txt"
Check ((Test-Path $pbskyRuntimePath) -and (Test-Path $pbskyShadersPath) -and (Test-Path $pbskyNoticePath)) "PBSKY01 physical-atmosphere owners and MIT attribution exist"
Check (
    $opt04Project.Contains('ClCompile Include="..\Graphics\Renderer\SkyRendererPbrAtmosphere.cpp"') -and
    $opt04Project.Contains('ClCompile Include="..\Graphics\Renderer\SkyRendererPbrAtmosphereShaders.cpp"')
) "PBSKY01 physical-atmosphere runtime and GLSL owners are compiled"
Check (
    $pbskyRoot.Contains("buildShaderProgram(kSkyVertexShader,kPbrSkyFragmentShader)") -and
    $pbskyRoot.Contains("updatePhysicallyBasedAtmosphereLuts(lighting, w)") -and
    $pbskyHeader.Contains("m_pbrSkyViewTexture") -and
    $pbskyHeader.Contains("m_pbrAtmosphereProgramsLinked")
) "PBSKY01 sky draw consumes the physical sky-view LUT with cached program validity"
Check (
    -not $pbskyBaseShaders.Contains("kSkyFragmentShader") -and
    $pbskyBaseShaders.Contains("kSkyVertexShader")
) "PBSKY01 retires the legacy artistic sky fragment rather than stacking a second sky authority"
Check (
    $pbskyShaders.Contains("6378100.0") -and
    $pbskyShaders.Contains("8000.0") -and
    $pbskyShaders.Contains("1200.0") -and
    $pbskyShaders.Contains("vec3(5.8e-6, 13.5e-6, 33.1e-6)") -and
    $pbskyShaders.Contains("30000.0") -and
    $pbskyShaders.Contains("10000.0")
) "PBSKY01 Earth atmosphere retains physical planet, Rayleigh, aerosol and ozone coefficients"
Check (
    $pbskyRuntime.Contains("kMultiScatteringWidth = 32") -and
    $pbskyRuntime.Contains("kMultiScatteringHeight = 32") -and
    $pbskyRuntime.Contains("kSkyViewWidth = 256") -and
    $pbskyRuntime.Contains("kSkyViewHeight = 144") -and
    $pbskyShaders.Contains("const int DIRS=64") -and
    $pbskyShaders.Contains("const int N=16")
) "PBSKY01 camera-space LUT sizes and sampling budgets track the upstream physical-sky architecture"
$pbskyUpdateIndex = $pbskyRuntime.IndexOf("void SkyRenderer::updatePhysicallyBasedAtmosphereLuts(", [System.StringComparison]::Ordinal)
$pbskyUpdateText = if ($pbskyUpdateIndex -ge 0) { $pbskyRuntime.Substring($pbskyUpdateIndex) } else { "" }
Check (
    $pbskyRuntime.Contains("glGetProgramiv(program, GL_LINK_STATUS") -and
    -not $pbskyUpdateText.Contains("glGetProgramiv")
) "PBSKY01 preserves PERF05: shader link status is cached at initialization and never queried in the frame hot path"
Check (
    (ReadText $pbskyNoticePath).Contains("Copyright (c) 2025 jiaozi158") -and
    (ReadText $pbskyNoticePath).Contains("MIT License")
) "PBSKY01 retains the upstream MIT attribution"

# OPT06: stabilization/freeze pass. Keep debug rendering from reintroducing
# per-draw driver name lookups, and pin the major production optimizations that
# should now be treated as architectural invariants rather than experiments.
$opt06DebugPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityDebugRenderer.cpp"
$opt06DebugHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityDebugRenderer.hpp"
$opt06Debug = ReadText $opt06DebugPath
$opt06DebugHeader = ReadText $opt06DebugHeaderPath
$opt06DebugDrawIndex = $opt06Debug.IndexOf("void EntityDebugRenderer::draw(", [System.StringComparison]::Ordinal)
$opt06DebugDraw = if ($opt06DebugDrawIndex -ge 0) { $opt06Debug.Substring($opt06DebugDrawIndex) } else { $opt06Debug }
Check (
    $opt06DebugHeader.Contains("m_uniformModel") -and
    $opt06DebugHeader.Contains("m_uniformSaturation") -and
    $opt06Debug.Contains('m_uniformModel = glGetUniformLocation(m_program, "uModel")') -and
    $opt06Debug.Contains('m_uniformSaturation = glGetUniformLocation(m_program, "uSaturation")') -and
    -not $opt06DebugDraw.Contains("glGetUniformLocation(")
) "OPT06 debug renderer caches uniform locations at initialization instead of resolving names in the live draw path"
Check (
    $opt06DebugDraw.Contains("GLuint activeVao = m_box.vao") -and
    $opt06DebugDraw.Contains("if (mesh.vao != activeVao)") -and
    $opt06DebugDraw.Contains("glBindVertexArray(activeVao)")
) "OPT06 debug renderer avoids redundant consecutive VAO binds"
$opt06CodeHealth = ReadText (Join-Path $Root "Build\Reports\CodeHealthSnapshot.txt")
Check (
    $opt06CodeHealth.Contains("OPT06 OPTIMIZATION-FREEZE INVENTORY") -and
    $opt06CodeHealth.Contains("Debug renderer draw-time glGetUniformLocation calls: 0") -and
    $opt06CodeHealth.Contains("Known synchronous hot renderer state queries: 0") -and
    $opt06CodeHealth.Contains("Cloud temporal full-image copy calls: 0") -and
    $opt06CodeHealth.Contains("Blocking graphics readback/finish calls (glFinish/glGetTextureSubImage/glReadPixels): 0") -and
    $opt06CodeHealth.Contains("Production DynamicSurfaceHydrology files present: 0")
) "OPT06 code-health audit freezes the optimized production ownership and nonblocking renderer invariants"
Check (
    (Test-Path (Join-Path $Root "Docs\OPT06_OPTIMIZATION_FREEZE.md")) -and
    (Test-Path (Join-Path $Root "Build\Reports\OPT06_OptimizationFreeze.txt"))
) "OPT06 final optimization-freeze documentation and milestone report are present"
$opt06ProjectState = ReadText (Join-Path $Root "Docs\PROJECT_STATE.md")
Check (
    $opt06ProjectState.Contains("Current Dynamic Surface candidate:** OPT03C4/OPT06") -and
    $opt06ProjectState.Contains("single production spatial-water authority") -and
    $opt06ProjectState.Contains('historical CPU `DynamicSurfaceHydrology` implementation exists only under `Tests/Reference`') -and
    -not $opt06ProjectState.Contains("CPU Hydro is still compiled only as the GPU-unavailable/regression fallback")
) "OPT06 current-state documentation matches the single GPU-water authority instead of preserving the superseded CPU-Hydro fallback narrative"
Check ($clean05AssetCache.Contains("EntityMeshRenderer::acquireMesh(") -and $clean05AssetCache.Contains("EntityMeshRenderer::resolveAsset(") -and $clean05AssetCache.Contains("EntityMeshRenderer::dependenciesChanged(")) "CLEAN05 asset/cache unit owns mesh loading and hot-reload dependency checks"
Check ($clean05Animation.Contains("EntityMeshRenderer::animationTransformsForInstance(") -and $clean05Animation.Contains("applyMeshNodeOverrides(") -and $clean05Animation.Contains("buildSkinPalette(")) "CLEAN05 animation unit owns clip/node/skin evaluation"
Check ($clean05Shadows.Contains("EntityMeshRenderer::initializeShadowResources(") -and $clean05Shadows.Contains("EntityMeshRenderer::buildShadowCascades(") -and $clean05Shadows.Contains("EntityMeshRenderer::drawShadowMaps(")) "CLEAN05 shadow unit owns cascaded shadow resources and pass"
Check ($clean05Shaders.Contains("kVertexShader") -and $clean05Shaders.Contains("kFragmentShader") -and $clean05Shaders.Contains("kShadowVertexShader") -and $clean05Shaders.Contains("kShadowFragmentShader") -and -not $clean05RootRenderer.Contains('R"glsl(')) "CLEAN05 embedded GLSL has one explicit header owner"
Check ($clean05ShadowConfig.Contains("kMediumResolution = 1536") -and $clean05ShadowConfig.Contains("kHighResolution = 2048") -and $clean05ShadowConfig.Contains("kUltraResolution = 3072") -and $clean05ShadowConfig.Contains("kDefaultQuality = Quality::Ultra")) "CLOUDURP15BK shadow quality presets centralize Low/Medium/High/Ultra with performance-tuned 3072 Ultra default"
Check ($clean05Shadows.Contains("GL_MAX_TEXTURE_SIZE") -and $clean05Shadows.Contains("kDefaultMapResolution") -and $clean05Shadows.Contains("m_shadowResolution")) "CLEAN05 shadow allocation uses centralized default and GPU dimension clamp"
Check ($clean05Shaders.Contains("kShadowGeometryShader") -and $clean05Shaders.Contains("gl_Layer = cascade") -and $clean05Shadows.Contains("glFramebufferTexture(") -and $clean05Shadows.Contains("uCascadeMask") -and $clean05Shadows.Contains("batchIndexCount") -and $clean05Shadows.Contains("sameTransformState") -and -not $clean05Shadows.Contains("glFramebufferTextureLayer(")) "SHADOW02 uses one layered CSM framebuffer, GPU cascade fan-out and material-agnostic contiguous range batching"
$shadow03VideoSettingsPath = Join-Path $Root "Engine\HeritageEngine\Core\Settings\VideoSettings.hpp"
$shadow03VideoStoragePath = Join-Path $Root "Engine\HeritageEngine\Core\Settings\VideoSettingsStorage.cpp"
$shadow03VideoPagePath = Join-Path $Root "Engine\HeritageEngine\UI\Settings\VideoSettingsPage.cpp"
$shadow03LauncherPagePath = Join-Path $Root "Engine\Launcher\LauncherSettingsPage.cpp"
$shadow03VideoSettings = if (Test-Path $shadow03VideoSettingsPath) { [IO.File]::ReadAllText($shadow03VideoSettingsPath) } else { "" }
$shadow03VideoStorage = if (Test-Path $shadow03VideoStoragePath) { [IO.File]::ReadAllText($shadow03VideoStoragePath) } else { "" }
$shadow03VideoPage = if (Test-Path $shadow03VideoPagePath) { [IO.File]::ReadAllText($shadow03VideoPagePath) } else { "" }
$shadow03LauncherPage = if (Test-Path $shadow03LauncherPagePath) { [IO.File]::ReadAllText($shadow03LauncherPagePath) } else { "" }
Check ($shadow03VideoSettings.Contains("shadowQualityIndex = 3") -and $shadow03VideoSettings.Contains("shadowFilterIndex = 2") -and $shadow03VideoSettings.Contains('"Poisson PCF"') -and $shadow03VideoSettings.Contains('"PCSS + Poisson"') -and $shadow03VideoStorage.Contains('shadowQualityIndex=') -and $shadow03VideoStorage.Contains('shadowFilterIndex=')) "CLOUDURP15BK persists Ultra and PCSS+Poisson shadow selections with performance-tuned implementation"
Check ($shadow03VideoPage.Contains('ImGui::TextDisabled("Shadows")') -and $shadow03VideoPage.Contains('"Shadow Quality"') -and $shadow03VideoPage.Contains('"Shadow Filtering"') -and $shadow03LauncherPage.Contains('ImGui::TextDisabled("Shadows")') -and $shadow03LauncherPage.Contains('"Shadow Quality"') -and $shadow03LauncherPage.Contains('"Shadow Filtering"')) "SHADOW04 groups live shadow quality/filter controls under a Shadows divider in engine and launcher Video Settings"
Check ($clean05Shaders.Contains("sampler2DArrayShadow uShadowMap") -and $clean05Shaders.Contains("sampler2DArray uShadowDepthMap") -and $clean05Shaders.Contains("kShadowPoissonDisk") -and $clean05Shaders.Contains("sampleShadowPoissonPcf") -and $clean05Shaders.Contains("sampleShadowPcssPoisson") -and $clean05Shaders.Contains("uShadowFilterMode") -and $clean05Shadows.Contains("glGenSamplers") -and $clean05Shadows.Contains("GL_COMPARE_REF_TO_TEXTURE") -and $clean05Shadows.Contains("GL_LINEAR") -and $clean05Shadows.Contains("GL_NEAREST") -and $clean05RootRenderer.Contains("glBindSampler(10, m_shadowCompareSampler)") -and $clean05RootRenderer.Contains("glBindSampler(11, m_shadowRawSampler)")) "SHADOW04 provides Nearest, Poisson PCF and PCSS+Poisson using shared raw/linear-comparison shadow samplers"

# CLEAN06: InputSystem and glTF importer implementation ownership is split while
# caller-facing headers remain stable.
$clean06EngineProject = $clean04EngineProject
$clean06InputRoot = "Engine\HeritageEngine\Input"
$clean06InputSources = @(
    "InputSystem.cpp",
    "InputBindings.cpp",
    "InputAnalog.cpp",
    "InputCapture.cpp",
    "InputBindingEvaluation.cpp",
    "InputBindingParser.cpp",
    "InputDevices.cpp",
    "InputProfiles.cpp",
    "InputPersistence.cpp"
)
foreach ($name in $clean06InputSources) {
    $relativePath = "$clean06InputRoot\$name"
    $absolutePath = Join-Path $Root $relativePath
    Check (Test-Path $absolutePath) "CLEAN06 input ownership file exists: $relativePath"
    Check ($clean06EngineProject.Contains('ClCompile Include="$(ProjectDir)..\Input\' + $name + '"')) "CLEAN06 engine compiles input responsibility unit: $name"
}
$clean06InputInternalPath = Join-Path $Root "$clean06InputRoot\InputSystemInternal.hpp"
$clean06InputInternal = if (Test-Path $clean06InputInternalPath) { [IO.File]::ReadAllText($clean06InputInternalPath) } else { "" }
Check ($clean06InputInternal.Contains("namespace heritage::input::input_internal") -and $clean06InputInternal.Contains("supportedKeyCodes") -and $clean06InputInternal.Contains("profileNamesEqual")) "CLEAN06 input private shared helpers have one explicit internal owner"
$clean06InputSystem = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputSystem.cpp"))
$clean06InputBindings = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputBindings.cpp"))
$clean13InputAnalog = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputAnalog.cpp"))
$clean13InputCapture = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputCapture.cpp"))
$clean13InputEvaluation = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputBindingEvaluation.cpp"))
$clean13InputParser = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputBindingParser.cpp"))
$clean06InputDevices = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputDevices.cpp"))
$clean06InputProfiles = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputProfiles.cpp"))
$clean06InputPersistence = [IO.File]::ReadAllText((Join-Path $Root "$clean06InputRoot\InputPersistence.cpp"))
Check ($clean06InputSystem.Contains("InputSystem::initialize(") -and $clean06InputSystem.Contains("InputSystem::updateActions(") -and -not $clean06InputSystem.Contains("InputSystem::createProfile(")) "CLEAN06 root InputSystem owns lifecycle/action orchestration rather than profile persistence"
Check ($clean06InputSystem.Split("`n").Count -lt 500) "CLEAN06 root InputSystem.cpp stays below coordinator size guard"
Check ($clean06InputBindings.Contains("InputSystem::registerAction(") -and $clean13InputCapture.Contains("InputSystem::beginBindingCapture(") -and $clean13InputParser.Contains("InputSystem::parseBinding(")) "CLEAN06/CLEAN13 input ownership covers action editing, capture and parsing"
$input03DefinitionsPath = Join-Path $Root "Modules\RacingUnited\Data\InputActions.ini"
$input03VehicleInputPath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Input.lua"
$input03VehicleLifecyclePath = Join-Path $Root "Modules\RacingUnited\Scripts\Vehicles\Lifecycle.lua"
$input03SettingsCommonPath = Join-Path $Root "Engine\HeritageEngine\UI\Settings\InputSettingsCommon.cpp"
$input03Definitions = ReadText $input03DefinitionsPath
$input03VehicleInput = ReadText $input03VehicleInputPath
$input03VehicleLifecycle = ReadText $input03VehicleLifecyclePath
$input03SettingsCommon = ReadText $input03SettingsCommonPath
Check ($clean06InputBindings.Contains("!trim(defaultBinding).empty()") -and $clean06InputSystem.Contains("empty right-hand side is a valid deliberately-unbound")) "INPUT03 module actions may be declared bindable without stealing a factory-default control"
Check ($input03Definitions.Contains("[Gears]") -and $input03Definitions.Contains("Clutch =") -and $input03Definitions.Contains("Gear 1 =") -and $input03Definitions.Contains("Gear 24 =") -and $input03VehicleInput.Contains('Input.RegisterAction("Clutch", "", "Gears")') -and $input03VehicleInput.Contains('for gear = 1, 24 do')) "INPUT03 Racing United exposes Clutch, Neutral, Reverse and direct Gear 1-24 actions in a dedicated Gears category"
Check ($input03VehicleLifecycle.Contains("ReadVehicleDirectGearSelection()") -and $input03VehicleLifecycle.Contains("Vehicle.SetGear(nativeVehicle, directGear)")) "INPUT03 direct gear bindings drive the authoritative vehicle gearbox"
Check ($input03SettingsCommon.Contains('if (group == "Gears")') -and $input03SettingsCommon.Contains('if (name == "Shift Up") return 0;') -and $input03SettingsCommon.Contains('if (name == "Shift Down") return 1;') -and $input03SettingsCommon.Contains('if (name == "Clutch") return 2;') -and $input03SettingsCommon.Contains('if (name == "Select Neutral") return 3;') -and $input03SettingsCommon.Contains('if (name == "Select Reverse") return 4;') -and $input03SettingsCommon.Contains('return 4 + gear;')) "INPUT03B Gears settings preserve Shift Up, Shift Down, Clutch, Neutral, Reverse, then natural Gear 1-24 order"
Check ($clean06InputDevices.Contains("InputSystem::updateHardwareState(") -and $clean06InputDevices.Contains("InputSystem::gamepads(") -and $clean06InputDevices.Contains("InputSystem::readGamepadState(")) "CLEAN06 device unit owns hardware polling and device discovery"
Check ($clean06InputProfiles.Contains("InputSystem::createProfile(") -and $clean06InputProfiles.Contains("InputSystem::applyProfile(") -and $clean06InputProfiles.Contains("InputSystem::deleteProfile(")) "CLEAN06 profile unit owns named profile snapshots and CRUD"
Check ($clean06InputPersistence.Contains("InputSystem::save(") -and $clean06InputPersistence.Contains("InputSystem::load(")) "CLEAN06 persistence unit owns live input settings save/load"

$clean06GltfRoot = "Engine\HeritageEngine\Graphics\Gltf"
$clean06GltfSources = @(
    "GltfBinary.cpp",
    "GltfJson.cpp",
    "GltfDocument.cpp",
    "GltfMeshImporter.cpp",
    "GltfMetadata.cpp",
    "GltfCollisionImporter.cpp"
)
$clean06GltfText = @{}
foreach ($name in $clean06GltfSources) {
    $relativePath = "$clean06GltfRoot\$name"
    $absolutePath = Join-Path $Root $relativePath
    Check (Test-Path $absolutePath) "CLEAN06 glTF ownership file exists: $relativePath"
    $clean06GltfText[$name] = if (Test-Path $absolutePath) { [IO.File]::ReadAllText($absolutePath) } else { "" }
    Check ($clean06EngineProject.Contains('ClCompile Include="..\Graphics\Gltf\' + $name + '"')) "CLEAN06 engine compiles glTF responsibility unit: $name"
}
$clean06GltfInternalPath = Join-Path $Root "$clean06GltfRoot\GltfInternal.hpp"
$clean06GltfInternal = if (Test-Path $clean06GltfInternalPath) { [IO.File]::ReadAllText($clean06GltfInternalPath) } else { "" }
Check ($clean06GltfInternal.Contains("namespace heritage::graphics::gltf_internal") -and $clean06GltfInternal.Contains("struct JsonValue") -and $clean06GltfInternal.Contains("struct AccessorInfo")) "CLEAN06 glTF private importer vocabulary has one explicit owner"
Check (-not $clean06EngineProject.Contains('ClCompile Include="..\Graphics\GltfBinary.cpp"')) "CLEAN06 legacy root GltfBinary.cpp is not compiled"
Check ($clean06GltfText["GltfJson.cpp"].Contains("class JsonParser") -and $clean06GltfText["GltfJson.cpp"].Contains("parseJsonDocument")) "CLEAN06 JSON unit owns glTF JSON parsing"
Check ($clean06GltfText["GltfDocument.cpp"].Contains("parseGlb(") -and $clean06GltfText["GltfDocument.cpp"].Contains("getAccessorInfo(") -and $clean06GltfText["GltfDocument.cpp"].Contains("readFloatAccessor(")) "CLEAN06 document unit owns GLB container/accessor decoding"
Check ($clean06GltfText["GltfMeshImporter.cpp"].Contains("buildMaterialDefinition(") -and $clean06GltfText["GltfMeshImporter.cpp"].Contains("appendPrimitive(") -and $clean06GltfText["GltfMeshImporter.cpp"].Contains("buildAnimations(")) "CLEAN06 mesh importer owns materials/primitives/skins/animations"
Check ($clean06GltfText["GltfMetadata.cpp"].Contains("appendExtrasMetadata(") -and $clean06GltfText["GltfMetadata.cpp"].Contains("buildNodeHierarchy(")) "CLEAN06 metadata unit owns extras and node hierarchy metadata"
Check ($clean06GltfText["GltfCollisionImporter.cpp"].Contains("isCollisionAuthoringNode(") -and $clean06GltfText["GltfCollisionImporter.cpp"].Contains("extractCollisionNodeRecursive(")) "CLEAN06 collision importer owns collision/spawn authoring extraction"
Check ($clean06GltfText["GltfBinary.cpp"].Contains("loadGlbMesh(") -and $clean06GltfText["GltfBinary.cpp"].Contains("inspectGlbMetadata(") -and $clean06GltfText["GltfBinary.cpp"].Contains("extractGlbStaticCollisionScene(")) "CLEAN06 glTF facade retains public entry-point implementations"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-052-Input-And-Gltf-Ownership.md")) "CLEAN06 input/glTF ownership ADR exists"


# CLEAN07: main.cpp is only the executable entry point; engine orchestration and
# stable helper responsibilities have explicit owners. Lua API tables register
# from their domains instead of one ever-growing master catalogue.
$clean07Main = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\main.cpp")
$clean07Engine = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.cpp")
$clean07Display = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Display\DisplayModeController.cpp")
$clean07Overlay = ReadText (Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp")
$clean07Clipboard = ReadText (Join-Path $Root "Engine\HeritageEngine\Platform\Windows\BackbufferClipboard.cpp")
$clean07Project = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj")
$clean09Frame = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineFrame.cpp")
$clean09Simulation = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineSimulation.cpp")
$clean09Rendering = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRendering.cpp")
$clean09Hotkeys = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineHotkeys.cpp")
$clean09RuntimeState = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRuntimeState.hpp")
$clean09EngineHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.hpp")
$clean09BuildHelper = ReadText (Join-Path $Root "Tools\00_BuildAndRunCurrent.cmd")
Check ($clean07Main.Split("`n").Count -lt 80 -and $clean07Main.Contains("HeritageEngine engine") -and $clean07Main.Contains("engine.run(argc, argv)")) "CLEAN07 main.cpp is a boring executable entry point"
Check ($clean07Engine.Contains("HeritageEngine::run") -and $clean07Engine.Contains("ModuleRuntimeManager moduleRuntime") -and $clean09Simulation.Contains("physics.advance")) "CLEAN09 HeritageEngine orchestrates process state while simulation stepping is phase-owned"
Check ($clean07Display.Contains("DisplayModeController::initiateChange") -and $clean07Display.Contains("DisplayModeController::restorePendingChange") -and $clean07Display.Contains("drawChangeConfirmationPopup")) "CLEAN07 display mode policy has one dedicated owner"
Check ($clean07Overlay.Contains("HERITAGE PERFORMANCE [F8]") -and -not $clean07Engine.Contains("HERITAGE PERFORMANCE [F8]")) "CLEAN07 performance overlay UI is outside HeritageEngine.cpp"
Check ($clean07Clipboard.Contains("copyBackbufferToClipboard") -and -not $clean07Engine.Contains("SetClipboardData")) "CLEAN07 Windows backbuffer clipboard code is platform-owned"
foreach ($name in @("EngineFrame.cpp", "EngineSimulation.cpp", "EngineRendering.cpp", "EngineHotkeys.cpp")) {
    $relative = "Runtime\$name"
    Check (Test-Path (Join-Path $Root "Engine\HeritageEngine\HeritageEngine\$relative")) "CLEAN09 runtime phase owner exists: $relative"
    Check ($clean07Project.Contains('<ClCompile Include="' + $relative + '"')) "CLEAN09 runtime phase owner is compiled: $relative"
}
Check ($clean09EngineHeader.Contains("std::unique_ptr<EngineRuntimeState> m_state") -and $clean09RuntimeState.Contains("VideoSettings videoSettings") -and $clean09RuntimeState.Contains("PhysicsWorld physics")) "CLEAN09 persistent engine services/settings are explicit HeritageEngine-owned state"
Check (-not $clean07Engine.Contains("VideoSettings g_videoSettings") -and -not $clean07Engine.Contains("PhysicsWorld g_physics") -and -not $clean07Engine.Contains("InputSystem g_input")) "CLEAN09 HeritageEngine.cpp no longer stores process services as anonymous globals"
Check ($clean09Frame.Contains("beginEngineFrame") -and $clean09Frame.Contains("completeEngineFrameSetup") -and $clean09Frame.Contains("presentEngineFrame")) "CLEAN09 frame timing/input/present responsibilities are phase-owned"
Check ($clean09Hotkeys.Contains("processEngineHotkeys") -and $clean09Hotkeys.Contains("GLFW_KEY_F12") -and $clean09Hotkeys.Contains("GLFW_KEY_F11")) "CLEAN09 developer/application hotkeys are phase-owned"

# CLEAN09B: GLFW can include the platform OpenGL header unless GLFW_INCLUDE_NONE
# is defined first. That conflicts with GLAD when a later engine header includes
# glad/glad.h. Keep every extracted runtime phase translation unit explicit.
foreach ($runtimeSourceName in @("EngineFrame.cpp", "EngineSimulation.cpp", "EngineRendering.cpp", "EngineHotkeys.cpp")) {
    $runtimeSourceText = ReadText (Join-Path $Root ("Engine\HeritageEngine\HeritageEngine\Runtime\" + $runtimeSourceName))
    $glfwIncludeIndex = $runtimeSourceText.IndexOf("#include <GLFW/glfw3.h>")
    $glfwNoneIndex = $runtimeSourceText.IndexOf("#define GLFW_INCLUDE_NONE")
    Check (($glfwIncludeIndex -lt 0) -or (($glfwNoneIndex -ge 0) -and ($glfwNoneIndex -lt $glfwIncludeIndex))) ("CLEAN09 runtime phase prevents GLFW from pre-including OpenGL before GLAD: " + $runtimeSourceName)
}
Check ($clean09Rendering.Contains("prepareEngineRendering") -and $clean09Rendering.Contains("renderEngineScene") -and $clean09Rendering.Contains("glBeginQuery")) "CLEAN09 render targets/GPU timing/render orchestration are phase-owned"
Check ($clean09Simulation.Contains("updateEngineSimulation") -and $clean09Simulation.Contains("synchronizeEntityTransforms") -and $clean09Simulation.Contains("chaseCamera.update")) "CLEAN09 simulation/camera frame update is phase-owned"
Check ($clean09BuildHelper.Contains('set "MSBUILD_TARGET=Build"') -and $clean09BuildHelper.Contains('if /I "%~1"=="full"') -and $clean09BuildHelper.Contains('/t:%MSBUILD_TARGET%')) "CLEAN09 normal helper is incremental with an explicit FULL rebuild mode"


# PERF10: remove avoidable render-thread heap/driver metadata work and expose
# enough sub-buckets to distinguish real engine work from OpenGL pacing waits.
$perf10MonitorHeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceMonitor.hpp"
$perf10RenderingPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRendering.cpp"
$perf10SurfaceHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.hpp"
$perf10SurfacePath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.cpp"
$perf10OverlayPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp"
$perf10MonitorHeader = ReadText $perf10MonitorHeaderPath
$perf10Rendering = ReadText $perf10RenderingPath
$perf10SurfaceHeader = ReadText $perf10SurfaceHeaderPath
$perf10Surface = ReadText $perf10SurfacePath
$perf10Overlay = ReadText $perf10OverlayPath
$perf10DrawIndex = $perf10Surface.IndexOf("void SurfacePresentationRenderer::draw(")
$perf10DrawText = if ($perf10DrawIndex -ge 0) { $perf10Surface.Substring($perf10DrawIndex) } else { $perf10Surface }
Check ($perf10MonitorHeader.Contains("SurfacePresentation,") -and $perf10MonitorHeader.Contains("WeatherPresentation,") -and $perf10MonitorHeader.Contains("FramebufferSetup,") -and $perf10MonitorHeader.Contains("kCpuHitchThresholdMs = 20.0")) "PERF10 render forensics owns surface/weather/FBO buckets and captures >=20ms CPU-active hitches"
Check ($perf10Rendering.Contains("RenderPerformanceSection::SurfacePresentation") -and $perf10Rendering.Contains("RenderPerformanceSection::WeatherPresentation") -and $perf10Rendering.Contains("RenderPerformanceSection::FramebufferSetup") -and $perf10Rendering.Contains("renderSurfaceMs +=") -and $perf10Rendering.Contains("renderWeatherMs +=")) "PERF10 render orchestration times surface, weather and framebuffer setup without GPU synchronization"
Check ($perf10SurfaceHeader.Contains("m_trackVertexScratch") -and $perf10SurfaceHeader.Contains("m_particleVertexScratch") -and $perf10SurfaceHeader.Contains("m_marbleCellScratch") -and $perf10SurfaceHeader.Contains("m_movingRubberPacketScratch") -and $perf10Surface.Contains("m_trackVertexScratch.reserve(240000)") -and -not $perf10Surface.Contains("std::vector<TrackVertex> trackVertices;")) "PERF10 surface presentation reuses transient CPU staging instead of reserving a multi-megabyte track vector every render frame"
Check ($perf10SurfaceHeader.Contains("m_tireMarkUniformView") -and $perf10SurfaceHeader.Contains("m_marbleUniformView") -and $perf10SurfaceHeader.Contains("m_particleUniformView") -and $perf10Surface.Contains('m_tireMarkUniformView = glGetUniformLocation') -and -not $perf10DrawText.Contains("glGetUniformLocation")) "PERF10 surface presentation resolves OpenGL uniform locations at initialization rather than in draw()"
Check ($perf10Overlay.Contains("Pacing: VSync") -and $perf10Overlay.Contains("surface %.2f  weather %.2f") -and $perf10Overlay.Contains("Mesh submit residual/driver") -and $perf10Overlay.Contains("LAST CPU HITCH (>= 20 ms active)")) "PERF10 F8 overlay exposes pacing state, expanded render buckets and mesh driver/residual time"
Check (Test-Path (Join-Path $Root "Docs\PERF10_FRAME_PACING_AND_SURFACE_SUBMIT.md")) "PERF10 frame-pacing/surface-submit optimization is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF10_FramePacingSurfaceSubmit.txt")) "PERF10 milestone report is present"
foreach ($registration in @(
    "..\Core\Modules\LuaBindings\Entity\LuaEntityBindingRegistration.cpp",
    "..\Core\Modules\LuaBindings\Physics\LuaPhysicsBindingRegistration.cpp",
    "..\Core\Modules\LuaBindings\Vehicle\LuaVehicleBindingRegistration.cpp"
)) {
    Check ($clean07Project.Contains('<ClCompile Include="' + $registration + '"')) "CLEAN07 compiles domain registration unit: $registration"
}
Check ($runtimeCpp.Contains("registerUiBindings();") -and $runtimeCpp.Contains("registerPhysicsBindings();") -and $runtimeCpp.Contains("registerVehicleBindings();") -and $runtimeCpp.Contains("registerEntityBindings();")) "CLEAN07 LuaModuleRuntime registration is an ordered domain orchestrator"
Check (-not $runtimeCpp.Contains('registerFunction("Physics"') -and -not $runtimeCpp.Contains('registerFunction("Vehicle"') -and -not $runtimeCpp.Contains('registerFunction("Entity"')) "CLEAN07 large Lua API tables no longer live in LuaModuleRuntime.cpp"
Check ($luaBindingCpp.Contains('void LuaModuleRuntime::registerPhysicsBindings()') -and $luaBindingCpp.Contains('void LuaModuleRuntime::registerVehicleBindings()') -and $luaBindingCpp.Contains('void LuaModuleRuntime::registerEntityBindings()')) "CLEAN07 large Lua API domains own registration implementations"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-053-Heritage-Engine-Shell-And-Domain-Lua-Registration.md")) "CLEAN07 engine-shell/Lua-registration ADR exists"


$opt01DeadFiles = @(
    "Engine\HeritageEngine\Vehicles\Tires\TireCarcass3D.cpp",
    "Engine\HeritageEngine\Vehicles\Tires\TireCarcass3D.hpp",
    "Engine\HeritageEngine\Graphics\GltfBinary.cpp",
    "Engine\HeritageEngine\Physics\SurfaceField.cpp",
    "Modules\RacingUnited\Scripts\UI\Prototype\CloudLabPanel.lua"
)
foreach ($relativePath in $opt01DeadFiles) {
    Check (-not (Test-Path (Join-Path $Root $relativePath))) "OPT01 proven dead/signpost file is retired: $relativePath"
}


$retiredVehicleConfigurationPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleConfiguration.cpp"
$opt01VehicleManifestPath = Join-Path $Root "Docs\VEHICLE_SUBSYSTEM_ARCHITECTURE_MANIFEST.md"
$opt01VehicleManifest = ReadText $opt01VehicleManifestPath
Check (-not (Test-Path $retiredVehicleConfigurationPath)) "OPT01 retired VehicleConfiguration.cpp signpost is removed instead of masquerading as a source unit"
Check ($vehicleHeader.Contains("std::vector<WheelRecord> wheels")) "common native vehicle storage remains arbitrary-wheel-count"
$retiredTopologyScaffolds = @(
    "Engine\HeritageEngine\Vehicles\Topology\Common\VehicleTopologyCoordinator.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\TwoWheel\TwoWheelVehicleDynamics.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\ThreeWheel\ThreeWheelVehicleDynamics.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\FourPlusWheel\FourPlusWheelVehicleDynamics.cpp",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\Common.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\TwoWheel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\ThreeWheel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\FourPlusWheel.lua"
)
foreach ($relativePath in $retiredTopologyScaffolds) {
    Check (-not (Test-Path (Join-Path $Root $relativePath))) "OPT01 empty vehicle-topology scaffold is retired: $relativePath"
}
$opt01ScaffoldCpp = @(Get-ChildItem -LiteralPath (Join-Path $Root "Engine\HeritageEngine\Vehicles") -Recurse -File -Filter '*.cpp' | Where-Object {
    $sourceText = [IO.File]::ReadAllText($_.FullName)
    $sourceText.Contains("vehicle-architecture scaffold") -or $sourceText.Contains("vehicle-topology scaffold")
})
Check ($opt01ScaffoldCpp.Count -eq 0) "OPT01 vehicle source tree contains no fake noncompiled architecture-scaffold translation units"
Check (-not [regex]::IsMatch($clean07Project, '<None Include="\.\.\\Vehicles\\[^"]+\.cpp"')) "OPT01 Visual Studio project no longer lists fake Vehicle .cpp files as None items"
Check ((Test-Path $opt01VehicleManifestPath) -and $opt01VehicleManifest.Contains("Create source only when implementation exists") -and $opt01VehicleManifest.Contains("Two-wheel topology") -and $opt01VehicleManifest.Contains("Ground effect")) "OPT01 planned vehicle seams live in an explicit architecture manifest instead of fake translation units"
Check (Test-Path (Join-Path $Root "Docs\VEHICLE_TOPOLOGY_ARCHITECTURE.md")) "CLEAN03A vehicle-topology architecture contract is documented"
$vehicleInternalPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleSystemInternal.hpp"
$vehicleInternal = if (Test-Path $vehicleInternalPath) { [IO.File]::ReadAllText($vehicleInternalPath) } else { "" }
Check ($vehicleInternal.Contains("namespace heritage::vehicles::vehicle_system_detail")) "CLEAN02 private cross-translation-unit vehicle helpers are explicit"
Check ($vehicleHeader.Contains("contactCollider")) "wheel telemetry retains exact contacted collider"
Check ($vehicleHeader.Contains("surfaceMaterial")) "wheel telemetry retains contacted surface material"
Check ($vehicleHeader.Contains("TireModelDescription tireModel")) "each wheel record owns independent native tire data"
Check ($vehicleHeader.Contains("setWheelTireModel")) "per-wheel tire setter contract exists"
Check ($vehicleHeader.Contains("wheelTireModel")) "per-wheel tire readback contract exists"
Check ($vehicleHeader.Contains("setWheelSuspensionModel")) "per-wheel suspension setter contract exists"
Check ($vehicleHeader.Contains("wheelSuspensionModel")) "per-wheel suspension readback contract exists"
Check ($vehicleHeader.Contains("setWheelSuspensionGeometry")) "per-wheel suspension-geometry setter contract exists"
Check ($vehicleHeader.Contains("wheelSuspensionGeometry")) "per-wheel suspension-geometry readback contract exists"
Check ($vehicleHeader.Contains("setWheelFitment")) "per-wheel fitment setter contract exists"
Check ($vehicleHeader.Contains("wheelFitment")) "per-wheel fitment readback contract exists"
Check ($vehicleHeader.Contains("setWheelAlignment")) "per-wheel alignment setter contract exists"
Check ($vehicleHeader.Contains("wheelAlignment")) "per-wheel alignment readback contract exists"
Check ($vehicleHeader.Contains("setAntiRollBar")) "vehicle exposes reusable anti-roll-bar setter"
Check ($vehicleHeader.Contains("antiRollBarCount")) "vehicle exposes anti-roll-bar count/readback contract"
Check ($vehicleHeader.Contains("setChassisTorsionalCompliance")) "vehicle exposes chassis torsional-compliance setter"
Check ($vehicleHeader.Contains("chassisTorsionalCompliance")) "vehicle exposes chassis torsional-compliance telemetry/readback"
Check ($vehicleCpp.Contains("prepareAntiRollBarForces")) "vehicle evaluates anti-roll coupling before per-wheel solve"
Check ($vehicleHeader.Contains("localUprightRotationDegrees")) "wheel state exposes authoritative upright pose"
Check ($vehicleHeader.Contains("setWheelUnsprungMassModel")) "per-wheel unsprung-mass setter contract exists"
Check ($vehicleHeader.Contains("wheelUnsprungMassModel")) "per-wheel unsprung-mass readback contract exists"
Check ($vehicleHeader.Contains("struct VehicleRestState")) "vehicle parked-rest diagnostic contract exists"
Check ($vehicleHeader.Contains("startDynamicsLabCapture")) "vehicle exposes opt-in native dynamics capture"
Check ($vehicleHeader.Contains("damperDissipationWatts")) "wheel state exposes damper energy-rate telemetry"
Check ($vehicleHeader.Contains("enum class WheelContactStatus")) "wheel state classifies support and contact-loss outcomes"
Check ($vehicleHeader.Contains("contactLossTransitionCount")) "wheel state counts grounded-to-airborne transitions"
Check ($collisionHeader.Contains("RaycastQueryDiagnostics")) "raycasts expose static-scene diagnostic evidence"
Check ($rigidBodyHeader.Contains("setInertiaLocal")) "rigid bodies expose explicit local inertia override"
Check ($rigidBodyHeader.Contains("inertiaLocalOverridden")) "rigid bodies expose inertia provenance state to tooling"



# CLEAN13: final high-value ownership pass. These checks protect durable owners
# without encouraging arbitrary micro-splitting after the cleanup stop rule.
$clean13EngineProject = $clean04EngineProject
$clean13TestProject = $clean04TestProject

$entityRoot = "Engine\HeritageEngine\Core\Entities"
$entitySources = @(
    "EntityRegistry.cpp",
    "EntityHierarchy.cpp",
    "EntityTransforms.cpp",
    "EntityDebugComponents.cpp",
    "EntityMeshComponents.cpp"
)
$entityAggregate = ""
foreach ($name in $entitySources) {
    $path = Join-Path $Root "$entityRoot\$name"
    Check (Test-Path $path) "CLEAN13 entity ownership file exists: $name"
    $entityAggregate += if (Test-Path $path) { [IO.File]::ReadAllText($path) } else { "" }
    $projectRelative = "..\Core\Entities\$name"
    Check ($clean13EngineProject.Contains('ClCompile Include="' + $projectRelative + '"')) "CLEAN13 engine compiles entity owner: $name"
    Check ($clean13TestProject.Contains('ClCompile Include="' + $projectRelative + '"')) "CLEAN13 native regression project compiles entity owner: $name"
}
$entityRegistryRoot = ReadText (Join-Path $Root "$entityRoot\EntityRegistry.cpp")
$entityHierarchy = ReadText (Join-Path $Root "$entityRoot\EntityHierarchy.cpp")
$entityTransforms = ReadText (Join-Path $Root "$entityRoot\EntityTransforms.cpp")
$entityDebug = ReadText (Join-Path $Root "$entityRoot\EntityDebugComponents.cpp")
$entityMesh = ReadText (Join-Path $Root "$entityRoot\EntityMeshComponents.cpp")
Check ($entityRegistryRoot.Contains("EntityRegistry::create(") -and $entityRegistryRoot.Contains("EntityRegistry::destroy(") -and -not $entityRegistryRoot.Contains("EntityRegistry::setParent(") -and -not $entityRegistryRoot.Contains("EntityRegistry::setMesh(")) "CLEAN13 root EntityRegistry owns lifetime/query plumbing only"
Check ($entityHierarchy.Contains("EntityRegistry::setParent(") -and $entityHierarchy.Contains("EntityRegistry::isDescendantOf(")) "CLEAN13 entity hierarchy has a dedicated owner"
Check ($entityTransforms.Contains("EntityRegistry::setWorldPosition(") -and $entityTransforms.Contains("EntityRegistry::computeWorldTransform(")) "CLEAN13 entity transforms have a dedicated owner"
Check ($entityDebug.Contains("EntityRegistry::setDebugPrimitive(") -and $entityDebug.Contains("EntityRegistry::debugPrimitiveInstances(")) "CLEAN13 entity debug components have a dedicated owner"
Check ($entityMesh.Contains("EntityRegistry::setMesh(") -and $entityMesh.Contains("EntityRegistry::meshInstances(")) "CLEAN13 entity mesh components have a dedicated owner"
Check ($entityRegistryRoot.Split("`n").Count -lt 700) "CLEAN13 root EntityRegistry.cpp stays compact"

$inputOwnerNames = @("InputBindings.cpp", "InputAnalog.cpp", "InputCapture.cpp", "InputBindingEvaluation.cpp", "InputBindingParser.cpp")
foreach ($name in $inputOwnerNames) {
    $path = Join-Path $Root "Engine\HeritageEngine\Input\$name"
    Check (Test-Path $path) "CLEAN13 input ownership file exists: $name"
    Check ($clean13EngineProject.Contains('ClCompile Include="$(ProjectDir)..\Input\' + $name + '"')) "CLEAN13 engine compiles input owner: $name"
}
Check ($clean06InputBindings.Contains("InputSystem::registerAction(") -and -not $clean06InputBindings.Contains("InputSystem::parseBinding(") -and -not $clean06InputBindings.Contains("InputSystem::beginBindingCapture(")) "CLEAN13 InputBindings.cpp is action/binding ownership rather than a parser/capture dumping ground"
Check ($clean13InputAnalog.Contains("InputSystem::applyAnalogProcessing(") -and $clean13InputAnalog.Contains("InputSystem::sanitizeAnalogSettings(")) "CLEAN13 analogue processing/settings have a dedicated owner"
Check ($clean13InputCapture.Contains("InputSystem::beginBindingCapture(") -and $clean13InputCapture.Contains("InputSystem::updateBindingCapture(")) "CLEAN13 binding capture has a dedicated owner"
Check ($clean13InputEvaluation.Contains("InputSystem::evaluateBindingRaw(") -and $clean13InputEvaluation.Contains("InputSystem::matchingGamepads(")) "CLEAN13 binding evaluation has a dedicated owner"
Check ($clean13InputParser.Contains("InputSystem::parseBinding(") -and $clean13InputParser.Contains("InputSystem::bindingDisplayName(") -and $clean13InputParser.Contains("InputSystem::keyCodeFromName(")) "CLEAN13 binding parser/name conversion has a dedicated owner"

$inputSettingsRoot = "Engine\HeritageEngine\UI\Settings"
$inputSettingsSourceNames = @("InputSettingsPage.cpp", "InputSettingsCommon.cpp", "InputSettingsBindings.cpp", "InputSettingsAnalogue.cpp", "InputSettingsProfiles.cpp")
foreach ($name in $inputSettingsSourceNames) {
    $path = Join-Path $Root "$inputSettingsRoot\$name"
    Check (Test-Path $path) "CLEAN13 input-settings owner exists: $name"
    Check ($clean13EngineProject.Contains('ClCompile Include="..\UI\Settings\' + $name + '"')) "CLEAN13 engine compiles input-settings owner: $name"
}
$inputSettingsPage = ReadText (Join-Path $Root "$inputSettingsRoot\InputSettingsPage.cpp")
$inputSettingsBindings = ReadText (Join-Path $Root "$inputSettingsRoot\InputSettingsBindings.cpp")
$inputSettingsAnalogue = ReadText (Join-Path $Root "$inputSettingsRoot\InputSettingsAnalogue.cpp")
$inputSettingsProfiles = ReadText (Join-Path $Root "$inputSettingsRoot\InputSettingsProfiles.cpp")
Check ($inputSettingsPage.Contains("drawBindingsTab(input)") -and $inputSettingsPage.Contains("drawAnalogueTab(input)") -and $inputSettingsPage.Contains("drawProfilesTab(input)") -and $inputSettingsPage.Split("`n").Count -lt 120) "CLEAN13 InputSettingsPage is a small tab coordinator"
Check ($inputSettingsBindings.Contains("drawBindingsTab") -and $inputSettingsAnalogue.Contains("drawAnalogueTab") -and $inputSettingsProfiles.Contains("drawProfilesTab")) "CLEAN13 Input Settings tab ownership is explicit"

Check ($collisionSource.Contains("collectBroadphaseContacts(bodies)") -and -not $collisionSource.Contains("std::vector<BroadphaseProxy> proxies")) "CLEAN13 collision root delegates broadphase/contact candidate collection"
# TIRE22R1: the exact local-space tire-wrap candidate ranking added a small amount
# of coordinator plumbing to CollisionSystem.cpp. Keep a guard, but do not make a
# historical line count from CLEAN13 block a correct rebase. The real architecture
# checks above still enforce ownership boundaries.
Check ($collisionSource.Split("`n").Count -lt 1250) "CLEAN13/TIRE22R root CollisionSystem.cpp stays under coordinator guard"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-060-Final-Ownership-Pass-And-Cleanup-Stop-Rule.md")) "CLEAN13 final cleanup decision and stop rule are documented"

# JOB01: Heritage owns one reusable process-wide CPU worker pool. Hydrology is
# the first deterministic production migration; protect ownership and race-free
# phase structure without pinning implementation to incidental line counts.
$job01HeaderPath = Join-Path $Root "Engine\HeritageEngine\Core\Jobs\JobSystem.hpp"
$job01SourcePath = Join-Path $Root "Engine\HeritageEngine\Core\Jobs\JobSystem.cpp"
$job01RuntimeStatePath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineRuntimeState.hpp"
$job01HydrologyPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrology.cpp"
$job01HydrologyHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrology.hpp"
$job01HydrologyTopologyPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyTopology.cpp"
$job01HydrologyTilesPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyTiles.cpp"
$job01HydrologyCoverPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyCover.cpp"
$job01HydrologyCachePath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\SurfaceHydrologyCache.cpp"
$job01OverlayPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp"
$job01EngineProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$job01TestProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$job01Header = ReadText $job01HeaderPath
$job01Source = ReadText $job01SourcePath
$job01RuntimeState = ReadText $job01RuntimeStatePath
$job01Hydrology = ReadText $job01HydrologyPath
$job01HydrologyHeader = ReadText $job01HydrologyHeaderPath
$job01HydrologyTopology = ReadText $job01HydrologyTopologyPath
$job01HydrologyTiles = ReadText $job01HydrologyTilesPath
$job01HydrologyCover = ReadText $job01HydrologyCoverPath
$job01HydrologyCache = ReadText $job01HydrologyCachePath
$job01Overlay = ReadText $job01OverlayPath
$job01EngineProject = ReadText $job01EngineProjectPath
$job01TestProject = ReadText $job01TestProjectPath
Check ((Test-Path $job01HeaderPath) -and (Test-Path $job01SourcePath) -and $job01Header.Contains("class JobSystem final") -and $job01Header.Contains("parallelFor(")) "JOB01 shared Core/Jobs worker-pool contract exists"
Check ($job01Source.Contains("std::thread::hardware_concurrency()") -and $job01Source.Contains("m_workers.emplace_back") -and $job01Source.Contains("executeRanges(batch, true)") -and $job01Source.Contains("executingOnThisSystem()")) "JOB01 scheduler uses persistent hardware-aware workers, caller participation and nested-call safety"
Check ($job01RuntimeState.Contains("heritage::jobs::JobSystem jobs;") -and $job01RuntimeState.Contains("physics.setJobSystem(&jobs)")) "JOB01 EngineRuntimeState owns the process-wide scheduler and wires physics to it"
# OPT02: immutable .hhyd v15 topology is the only responsibility left in
# SurfaceHydrology. The retired WATER14-WATER17 adaptive CPU solver, signed
# virtual pipes, cadence scheduler and presentation-cell gathers are deleted.
$opt02VirtualPipePath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\VirtualPipeFlow.hpp"
$opt02SurfaceWorld = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.cpp")
$opt02SurfaceWorldHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.hpp")
$opt02FleetBenchmark = ReadText (Join-Path $Root "Engine\HeritageEngine\Vehicles\Tires\TireFleetBenchmark.cpp")
Check (
    (Test-Path $job01HydrologyTopologyPath) -and
    (Test-Path $job01HydrologyTilesPath) -and
    (Test-Path $job01HydrologyCoverPath) -and
    (Test-Path $job01HydrologyCachePath) -and
    $job01HydrologyHeader.Contains('immutable scene-hydrology') -and
    -not $job01HydrologyHeader.Contains('struct AdaptiveCell') -and
    -not $job01HydrologyHeader.Contains('struct AdaptivePipe') -and
    -not $job01HydrologyHeader.Contains('collectVisualCells(') -and
    -not $job01HydrologyHeader.Contains('void advance(') -and
    -not $job01HydrologyHeader.Contains('simulateStep(')
) "OPT02 SurfaceHydrology is a small immutable topology service with no adaptive live-water solver API"
Check (
    -not (Test-Path $opt02VirtualPipePath) -and
    -not $job01Hydrology.Contains('rebuildAdaptiveSimulationTopology') -and
    -not $job01HydrologyTopology.Contains('AdaptiveCell') -and
    -not $job01HydrologyTiles.Contains('AdaptivePipe')
) "OPT02 retires WATER14-WATER17 adaptive cells, virtual pipes and CPU water-simulation implementation"
Check (
    $job01HydrologyTopology.Contains('priority flood from true open boundaries') -and
    $job01HydrologyTopology.Contains('accumulatedAreaM2[target] += accumulatedAreaM2[i] * targetInfo.fraction;') -and
    $job01HydrologyTiles.Contains('rasterPrebakedPuddleResponseTileUncached') -and
    $job01HydrologyTiles.Contains('m_jobSystem->parallelFor') -and
    $job01HydrologyTiles.Contains('kEncodeBatchTiles = 4096u') -and
    $job01HydrologyCover.Contains('hasPrecipitationCoverAbove') -and
    $job01HydrologyCover.Contains('prebakedTriangleTileSpan')
) "OPT02 preserves welded-mesh priority-flood/MFD topology, bounded far-tile encoding and exact triangle-space precipitation cover"
Check (
    $job01HydrologyCache.Contains('constexpr std::uint32_t kCacheVersion = 15;') -and
    $job01HydrologyCache.Contains('CachePrebakedTriangle') -and
    $job01HydrologyCache.Contains('CachePrebakedFarTile') -and
    $job01HydrologyCache.Contains('header.prebakedFarPayloadBytes')
) "OPT02 keeps byte-compatible .hhyd v15 cache loading/writing in a dedicated source owner"
Check (
    $job01EngineProject.Contains('SurfaceHydrologyTopology.cpp') -and
    $job01EngineProject.Contains('SurfaceHydrologyTiles.cpp') -and
    $job01EngineProject.Contains('SurfaceHydrologyCover.cpp') -and
    $job01EngineProject.Contains('SurfaceHydrologyCache.cpp') -and
    $job01TestProject.Contains('SurfaceHydrologyTopology.cpp') -and
    $job01TestProject.Contains('SurfaceHydrologyTiles.cpp') -and
    $job01TestProject.Contains('SurfaceHydrologyCover.cpp') -and
    $job01TestProject.Contains('SurfaceHydrologyCache.cpp')
) "OPT02 engine and native tests compile every split SurfaceHydrology implementation unit"
Check (
    -not $opt02SurfaceWorld.Contains('m_hydrology.advance(') -and
    -not $opt02SurfaceWorld.Contains('m_hydrology.resetWater(') -and
    -not $opt02SurfaceWorldHeader.Contains('m_hydrology.setInterestSource(') -and
    -not $opt02SurfaceWorld.Contains('m_dynamicSurface.advanceHydro(')
) "OPT03C SurfaceWorld cannot advance immutable .hhyd topology or a second CPU Dynamic Surface water solver"
Check (
    $opt02FleetBenchmark.Contains('CPU tire-stack benchmark only') -and
    -not $opt02FleetBenchmark.Contains('DynamicSurfaceSystem hydrology') -and
    -not $opt02FleetBenchmark.Contains('hydrology.advanceHydro(') -and
    -not $opt02FleetBenchmark.Contains('hydrology.applyHydroTireContact(')
) "OPT03C tire fleet benchmark no longer creates a synthetic CPU spatial-water authority"
Check ((Test-Path (Join-Path $Root "Docs\OPT02_PREBAKED_HYDROLOGY_EXTRACTION.md")) -and (Test-Path (Join-Path $Root "Build\Reports\OPT02_PrebakedHydrologyExtraction.txt")) -and (Test-Path (Join-Path $Root "Tools\Diagnostics\ApplyOPT02Retirement.ps1"))) "OPT02 extraction, retirement convergence and compatibility gates are documented"
Check ((($job01Overlay.Contains('ImGui::Text("JOB SYSTEM")')) -or ($job01Overlay.Contains('appendf("JOB SYSTEM")') -and $job01Overlay.Contains('ImGui::TextUnformatted(reportCache.text.c_str()'))) -and $job01Overlay.Contains("workerRangeCount") -and $job01Overlay.Contains("callerRangeCount")) "JOB01 F8 performance overlay exposes scheduler activity"
Check ($job01EngineProject.Contains('Core\Jobs\JobSystem.cpp') -and $job01EngineProject.Contains('Core\Jobs\JobSystem.hpp') -and $job01TestProject.Contains('JobSystemRegression.cpp')) "JOB01 engine/test projects compile scheduler and its native regression"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-076-Engine-Job-System-And-Deterministic-Parallel-Phases.md")) "JOB01 deterministic multicore ownership decision is documented"
Check ((Test-Path (Join-Path $Root "Docs\PERFORMANCE_MULTICORE_ROADMAP.md")) -and (Test-Path (Join-Path $Root "Docs\WEATHER_ROADMAP.md"))) "JOB01 multicore roadmap and future weather handoff use the normal Docs safety-net structure"

# WATER15: iRacing publicly documents Dynamic Track as persistent water/moisture
# surface state with puddles, drying, tire/aero movement, drainage and wetness
# shaders. The proprietary implementation is not public; Heritage therefore
# protects behavior/architecture parity without claiming source-code identity.
# Settled water no longer owns a duplicate tessellated mesh.
$job02SurfaceRendererPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.cpp"
$job02SurfaceRendererHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SurfacePresentationRenderer.hpp"
$job02SurfaceRenderer = ReadText $job02SurfaceRendererPath
$job02SurfaceRendererHeader = ReadText $job02SurfaceRendererHeaderPath
$waterLegacyContourPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WaterContourMesher.hpp"
$waterLegacyParcelSourcePath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.cpp"
$waterLegacyParcelHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.hpp"
$waterLegacyStitcherPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WaterSurfaceStitcher.hpp"
Check (-not (Test-Path $waterLegacyContourPath) -and -not (Test-Path $waterLegacyParcelSourcePath) -and -not (Test-Path $waterLegacyParcelHeaderPath) -and -not (Test-Path $waterLegacyStitcherPath)) "WATER15 removes legacy contour, settled parcel and adaptive water-stitch presentation sources"
Check (-not $job02SurfaceRendererHeader.Contains("WaterRingGpuCache") -and -not $job02SurfaceRendererHeader.Contains("WaterMeshVertex") -and -not $job02SurfaceRendererHeader.Contains("m_waterRings") -and -not $job02SurfaceRenderer.Contains("buildStitchedAdaptiveWaterSurface") -and -not $job02SurfaceRenderer.Contains("glPolygonOffset(0.0f, 1.0f)") -and $job02SurfaceRenderer.Contains("WATER15: no second settled-water geometry pass exists here")) "WATER15 settled water has no renderer-owned ring mesh, seam stitcher, polygon bias or duplicate depth owner"
Check (-not $job01EngineProject.Contains("WaterSurfaceStitcher") -and -not $job01EngineProject.Contains("WaterParcelRenderer") -and -not $job01EngineProject.Contains("WaterContourMesher")) "WATER15 retired settled-water presentation code is absent from the engine project"
Check ((Test-Path (Join-Path $Root "Docs\WATER15_DYNAMIC_TRACK_SURFACE_STATE.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-121-Dynamic-Track-Surface-State-Water-Presentation.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15A_DynamicTrackSurfaceState.txt")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15B_ExactSceneSurfaceWater.txt")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15C_IntegratedMaterialWater.txt")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15E_HighResolutionClipmaps.txt")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15F_HydraulicHeadReconstruction.txt")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-122-Hydraulic-Head-Water-Reconstruction.md")) -and (Test-Path (Join-Path $Root "Docs\WATER15G_COMPACT_DYNAMIC_TRACK_REOPTIMIZATION.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-123-Compact-Hydrology-Presentation-Clipmaps.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15G_CompactDynamicTrackReoptimization.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER15I_FILM_PUDDLE_DECOUPLING.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-124-Decouple-Thin-Film-From-Puddle-Hydrology-Presentation.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER15I_FilmPuddleDecoupling.txt"))) "WATER15 Dynamic Track surface-state architecture through WATER15I film/puddle decoupling is documented"

# JOB03 / DSURF04C: authoritative hydrology uses fixed-2Hz 100m tiles around
# the UNION of actual simulation-interest sources. Never average split-screen /
# multiplayer positions into a midpoint. Stable tile phases prevent the shared
# 2Hz cadence from synchronizing into periodic spikes; >1000m is dormant.
$job03EngineSimulationPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\Runtime\EngineSimulation.cpp"
$job03SurfaceWorldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.hpp"
$job03SurfaceWorldTestPath = Join-Path $Root "Engine\HeritageEngine\Tests\SurfaceWorldRegression.cpp"
$job03EngineSimulation = ReadText $job03EngineSimulationPath
$job03SurfaceWorldHeader = ReadText $job03SurfaceWorldHeaderPath
$job03SurfaceWorldTest = ReadText $job03SurfaceWorldTestPath
$dynamicSurfaceRegression = ReadText (Join-Path $Root "Engine\HeritageEngine\Tests\DynamicSurfaceRegression.cpp")
Check (
    $job03SurfaceWorldTest.Contains('rasterPrebakedPuddleResponseTile(') -and
    $job03SurfaceWorldTest.Contains('prebakedFarPuddleResponseTile(') -and
    $job03SurfaceWorldTest.Contains('gutterRunoffAt') -and
    $job03SurfaceWorldTest.Contains('hasPrecipitationCoverAbove(')
) "OPT02 native regression preserves .hhyd v15 standing-depth, runoff, far-payload and exact shelter behavior"
Check (
    $job03SurfaceWorldTest.Contains('OPT02: the retired WATER14-WATER17 adaptive SurfaceHydrology solver') -and
    -not $job03SurfaceWorldTest.Contains('adaptiveFlatStats') -and
    -not $job03SurfaceWorldTest.Contains('collectVisualCellsBand(') -and
    -not $job03SurfaceWorldTest.Contains('setUniformWaterDepthForLab(')
) "OPT02 native regression no longer keeps the deleted adaptive CPU hydrology alive as a test-only dependency"
Check (
    $job03SurfaceWorldTest.Contains('multiMidpoint.valid') -and
    $job03SurfaceWorldTest.Contains('multiSourceHydrologyWorld.dynamicSurface().interestSources().size() != 2u')
) "JOB03 multi-source water regression remains owned by current Dynamic Surface Hydro"
Check (
    $job03SurfaceWorldTest.Contains('WATER15 presentation regression boundary') -and
    -not $job03SurfaceWorldTest.Contains('buildStitchedAdaptiveWaterSurface(') -and
    -not $job03SurfaceWorldTest.Contains('water_stitch')
) "WATER15 native regression remains decoupled from retired water tessellation"
$perf19EntityMeshPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$perf19EntityMeshHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.hpp"
$perf19EntityMeshShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshShaders.hpp"
$perf19WetnessAtlasPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshSurfaceWetness.cpp"
$perf19EntityMesh = ReadText $perf19EntityMeshPath
$perf19EntityMeshHeader = ReadText $perf19EntityMeshHeaderPath
$perf19EntityMeshShaders = ReadText $perf19EntityMeshShadersPath
$perf19WetnessAtlas = ReadText $perf19WetnessAtlasPath
$dsurfThermalAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceThermal.cpp")
$dsurfTypes = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceTypes.hpp")
$dsurfPagePoolHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfacePagePool.hpp")
$dsurfSurfaceWorld = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.cpp")
$dsurfSystem = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceSystem.cpp")
$dsurfChunkAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceChunk.cpp")
$dsurfGpuWaterAuthority = @(
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuRuntime.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuResources.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuResidency.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTopology.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuGeometry.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuDispatch.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTireEvents.cpp")
    ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTimers.cpp")
) -join "`n"
$dsurfGpuWaterAuthorityHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuRuntime.hpp")
$opt03GpuRuntimeRoot = Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface"
$opt03RuntimeFiles = @(
    'DynamicSurfaceGpuRuntime.cpp',
    'DynamicSurfaceGpuResources.cpp',
    'DynamicSurfaceGpuResidency.cpp',
    'DynamicSurfaceGpuTopology.cpp',
    'DynamicSurfaceGpuGeometry.cpp',
    'DynamicSurfaceGpuDispatch.cpp',
    'DynamicSurfaceGpuTireEvents.cpp',
    'DynamicSurfaceGpuTimers.cpp',
    'DynamicSurfaceGpuRuntime.hpp',
    'DynamicSurfaceGpuShaders.hpp'
)
$opt03AllRuntimeFilesPresent = $true
foreach ($file in $opt03RuntimeFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $opt03GpuRuntimeRoot $file))) {
        $opt03AllRuntimeFilesPresent = $false
    }
}
$opt03CoordinatorLines = @(
    Get-Content -LiteralPath (Join-Path $opt03GpuRuntimeRoot 'DynamicSurfaceGpuRuntime.cpp')
).Count
Check (
    $opt03AllRuntimeFilesPresent -and
    $opt03CoordinatorLines -lt 500 -and
    -not (Test-Path -LiteralPath (Join-Path $opt03GpuRuntimeRoot 'DynamicSurfaceGpuLodPrototype.cpp')) -and
    -not (Test-Path -LiteralPath (Join-Path $opt03GpuRuntimeRoot 'DynamicSurfaceGpuLodPrototype.hpp')) -and
    $dsurfGpuWaterAuthorityHeader.Contains('class DynamicSurfaceGpuRuntime') -and
    -not $dsurfGpuWaterAuthorityHeader.Contains('Prototype')
) "OPT03 production Dynamic Surface GPU runtime is explicitly named, split by responsibility and no longer carries prototype files"
$opt03bTireWaterBridge = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuTireEvents.cpp")
$opt03bMeshDynamicSurface = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshDynamicSurface.cpp")
Check (
    $dsurfGpuWaterAuthorityHeader.Contains('DynamicSurfaceGpuTireWaterSampleRequest') -and
    $dsurfGpuWaterAuthorityHeader.Contains('std::array<TireWaterSampleReadbackSlot, 3>') -and
    $opt03bTireWaterBridge.Contains('glClientWaitSync(slot.fence, 0, 0)') -and
    $opt03bTireWaterBridge.Contains('glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bytes, GL_MAP_READ_BIT)') -and
    $opt03bTireWaterBridge.Contains('Never wait for the GPU') -and
    -not $opt03bTireWaterBridge.Contains('glGetTextureSubImage') -and
    -not $opt03bTireWaterBridge.Contains('glFinish(') -and
    $opt03bMeshDynamicSurface.Contains('consumeGpuDynamicSurfaceWaterSampleRequests(') -and
    $opt03bMeshDynamicSurface.Contains('consumeCompletedTireWaterSamples(') -and
    $opt03bMeshDynamicSurface.Contains('publishGpuDynamicSurfaceWaterSamples(') -and
    $dsurfSurfaceWorld.Contains('m_gpuDynamicSurfaceWaterSamples.find(key)') -and
    $dsurfSurfaceWorld.Contains('m_gpuDynamicSurfaceWaterSampleRequests.push_back') -and
    -not $dsurfThermalAuthority.Contains('m_hydrology.sample(')
) "OPT03B tire physics samples the filtered production GPU water field through a nonblocking three-slot SSBO bridge"
Check (
    (Test-Path (Join-Path $Root "Docs\OPT03B_GPU_TIRE_WATER_SAMPLE_BRIDGE.md")) -and
    (Test-Path (Join-Path $Root "Build\Reports\OPT03B_GpuTireWaterSampleBridge.txt"))
) "OPT03B GPU tire-water bridge documentation and milestone report are present"
$waterLaboratoryHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\WaterLaboratory.hpp"
$waterLaboratoryScenePanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Scene\WaterLaboratoryPanel.lua"
# LIVETRACK21: the strategy matrix stays deleted. Production water has one path:
# immutable .hhyd v15 runoff/standing-depth/flow, priority-flood standing-water storage,
# MFD catchment kinematic runoff, 256x256 near topology through 100m plus the
# complete 32x32 far presentation set through 500m.
# All desired far tiles are admitted in one 20Hz poll; there is no progressive streamer.
Check (
    -not (Test-Path $waterLaboratoryHeaderPath) -and
    -not (Test-Path $waterLaboratoryScenePanelPath) -and
    -not $dsurfGpuWaterAuthority.Contains('m_waterLaboratorySettings') -and
    -not $perf19EntityMeshShaders.Contains('uWaterLab') -and
    -not $perf19EntityMesh.Contains('waterLaboratorySettings')
) "LIVETRACK15 keeps Water Laboratory deleted from production code and UI"
Check (
    $dsurfGpuWaterAuthorityHeader.Contains('kTileWorldSizeM = 10.0f') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kTileResolution = 256u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kSimulationRadiusM = 100.0f') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kTopologyPrefetchRadiusM = 105.0f') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kPresentationRadiusM = 500.0f') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kFarTileResolution = 32u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kFarAtlasTilesPerAxis = 128u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kPresentationPollIntervalSeconds = 1.0 / 20.0') -and
    $dsurfGpuWaterAuthority.Contains('streamFarTopology(') -and
    $dsurfGpuWaterAuthority.Contains('GL_RGB8') -and
    $dsurfGpuWaterAuthority.Contains('prebakedFarPuddleResponseTile(') -and
    -not $dsurfGpuWaterAuthority.Contains('kFarTopologyUploadBudgetPerPoll') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kFarBulkUploadThreshold = 512u') -and
    $dsurfGpuWaterAuthority.Contains('m_farAtlasCpuMirror') -and
    $dsurfGpuWaterAuthority.Contains('m_prebakedRgbaScratch.assign(texelCount * 4u, 0u);') -and
    $dsurfGpuWaterAuthority.Contains('releaseTileSlot(it->second.slot);') -and
    $dsurfGpuWaterAuthority.Contains('m_tiles.erase(it);')
) "LIVETRACK15 keeps 256x256 near topology and admits the complete world-prebaked 32x32 far set through 500m at 20Hz"
Check (
    $perf19EntityMeshShaders.Contains('uniform float uPrebakedWaterExposureM;') -and
    $perf19EntityMeshShaders.Contains('uniform float uRainWettingExposureM;') -and
    $perf19EntityMeshShaders.Contains('uniform float uRainRateMmPerHour;') -and
    $perf19EntityMeshShaders.Contains('float capacityM = decodeGpuWaterDepth(state.b);') -and
    $perf19EntityMeshShaders.Contains('const float kStandingWaterMaxDepthM = 0.00070;') -and
    $perf19EntityMeshShaders.Contains('float retainedHeadDriverM = clamp(') -and
    $perf19EntityMeshShaders.Contains('float headDeficitM = kStandingWaterMaxDepthM') -and
    $perf19EntityMeshShaders.Contains('float equilibriumDepthM = max(capacityM - headDeficitM, 0.0);') -and
    $perf19EntityMeshShaders.Contains('decoded.depthM = min(equilibriumDepthM, capacityM)') -and
    $perf19EntityMeshShaders.Contains('dynamicSurfacePuddleDepthM = standingDepthM;') -and
    $perf19EntityMeshShaders.Contains('float kinematicRunoffDepthM(GpuWaterDecoded state)') -and
    $perf19EntityMeshShaders.Contains('float dischargeM3ps = rainfallMps * max(state.runoffAreaM2, 0.0);') -and
    $perf19EntityMeshShaders.Contains('const float manningN = 0.014;') -and
    $perf19WetnessAtlas.Contains('m_dynamicSurfaceGpuRuntime.stats().backgroundSeedDepthM') -and
    $perf19WetnessAtlas.Contains('m_dynamicSurfaceGpuRuntime.stats().surfaceWettingExposureM') -and
    $perf19WetnessAtlas.Contains('m_dynamicSurfaceGpuRuntime.stats().runoffDriverMmPerHour') -and
    $dsurfGpuWaterAuthority.Contains('m_runoffDriverMmPerHour * std::exp(-dt / 45.0f)') -and
    $dsurfGpuWaterAuthority.Contains('const float retainedRainM = rainM * captureFraction;') -and
    $dsurfGpuWaterAuthority.Contains('LIVETRACK19 persistence contract: retained basin water NEVER') -and
    -not $perf19EntityMeshShaders.Contains('rainImpactWetting(') -and
    -not $dsurfGpuWaterAuthority.Contains('spatialRainMultiplier')
) "LIVETRACK21I uses the 0..0.70mm baked standing-depth authority plus live MFD/Manning runoff with a 45s runoff tail and no circular rain splats"
Check (
    $dsurfGpuWaterAuthority.Contains('Rain itself dispatches no full-field compute') -and
    -not $dsurfGpuWaterAuthority.Contains('dispatchWaterBatch(cohort, elapsedSeconds') -and
    -not $dsurfGpuWaterAuthority.Contains('glGenTextures(1, &m_waterBatchScratch)') -and
    $dsurfGpuWaterAuthority.Contains('4u, false, errorMessage') -and
    -not $dsurfGpuWaterAuthority.Contains('glGetTextureSubImage') -and
    $dsurfGpuWaterAuthority.Contains('Water has no full-field compute program anymore')
) "LIVETRACK15 forbids periodic all-tile water compute, water scratch textures and synchronous Hydro readback"
Check (
    $dsurfGpuWaterAuthorityHeader.Contains('kOptionalTileBudgetPerFrame = 12u') -and
    $dsurfGpuWaterAuthority.Contains('processed >= kOptionalTileBudgetPerFrame') -and
    $dsurfGpuWaterAuthority.Contains('m_stats.dispatchBacklogTiles = due - processed;') -and
    $dsurfGpuWaterAuthority.Contains('m_appliedHydrologyResetSerial != hydrologyResetSerial') -and
    $dsurfSurfaceWorld.Contains('++m_hydrologyResetSerial;')
) "LIVETRACK15 staggers optional snow/mud work and propagates water resets into renderer-owned exposure/dry-line state"
Check (
    $job01HydrologyCache.Contains('constexpr std::uint32_t kCacheVersion = 15;') -and
    $job01HydrologyCache.Contains('CachePrebakedFarTile') -and
    $job01HydrologyTiles.Contains('prebakedFarPuddleResponseTile(') -and
    $job01HydrologyTiles.Contains('rebuildPrebakedFarTileCache()') -and
    $job01HydrologyCache.Contains('CachePrebakedTriangle') -and
    $job01HydrologyTopology.Contains('rebuildPrebakedTriangleTopology(') -and
    $job01HydrologyHeader.Contains('m_prebakedTriangleTileSpans') -and
    $job01HydrologyHeader.Contains('m_prebakedFarPayload') -and
    $job01HydrologyTiles.Contains('kEncodeBatchTiles = 4096u') -and
    $job01HydrologyTiles.Contains('encoded[localIndex].encoding = 2u;') -and
    $job01HydrologyTiles.Contains('cf[texel] = static_cast<std::uint8_t>((capacityCode << 4u) | flowCode);') -and
    $job01HydrologyTiles.Contains('spillElevationM = wa * triangle.spillElevationA') -and
    $job01HydrologyTopology.Contains('runoffAccumulationAM2') -and
    $job01HydrologyTopology.Contains('accumulatedAreaM2[target] += accumulatedAreaM2[i] * targetInfo.fraction;') -and
    $job01HydrologyTopology.Contains('priority flood from true open boundaries') -and
    $job01HydrologyTiles.Contains('0.0, 0.00001, 0.00005, 0.00010, 0.00015, 0.00020, 0.00025, 0.00030,') -and
    $job01HydrologyTiles.Contains('0.00035, 0.00040, 0.00045, 0.00050, 0.00055, 0.00060, 0.00065, 0.00070') -and
    $job01HydrologyTiles.Contains('capacityM = std::clamp(capacityM, 0.0, 0.00070);') -and
    (-not $job01HydrologyTiles.Contains('capacityDither01')) -and
    $job01HydrologyTiles.Contains('const auto encodeCapacity = [&](double capacityM) -> std::uint8_t') -and
    $perf19EntityMeshShaders.Contains('float waterDepthFromLadderCode(int code)') -and
    $perf19EntityMeshShaders.Contains('float quantizeStandingWaterDepth(float depthM)') -and
    $perf19EntityMeshShaders.Contains('return waterDepthFromLadderCode(code);') -and
    $perf19EntityMeshShaders.Contains('combined.depthM = quantizeStandingWaterDepth(combined.depthM);') -and
    $perf19EntityMeshShaders.Contains('if (distanceM < 85.0)') -and
    $perf19EntityMeshShaders.Contains('else if (distanceM > 100.0)') -and
    $perf19EntityMeshShaders.Contains('const vec2 offsets[4] = vec2[4](') -and
    -not $job01HydrologyTiles.Contains('supportConstantCapacityCodes') -and
    $perf19EntityMeshShaders.Contains('const float visibleWaterOnsetM = 0.000010;') -and
    -not $perf19EntityMeshShaders.Contains('rainImpactWetting(') -and
    $perf19EntityMeshShaders.Contains('LIVETRACK21I performance: retain hardware GL_LINEAR filtering but reduce') -and
    $perf19EntityMeshShaders.Contains('state = texture(uGpuWaterAtlas, atlasUv);') -and
    $perf19EntityMeshShaders.Contains('bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)') -and
    $perf19EntityMeshShaders.IndexOf('bool gpuWaterNearestState(vec3 positionRelative, out vec4 state)') -lt $perf19EntityMeshShaders.IndexOf('bool gpuNearWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)') -and
    $perf19EntityMeshShaders.Contains('float kinematicRunoffDepthM(GpuWaterDecoded state)') -and
    $perf19EntityMeshShaders.Contains('float runoffSheet = runoff * max(film, 0.12) * runoffPatch') -and
    $perf19EntityMeshShaders.Contains('vec3 rainImpactRippleNormal(') -and
    $perf19EntityMeshShaders.Contains('float puddleGate = smoothstep(0.00001, 0.00008, standingDepthM) * standing;') -and
    $perf19EntityMeshShaders.Contains('float runoffGate = smoothstep(0.00005, 0.00030, runoffDepthM) * runoff * 0.10;') -and
    $perf19EntityMeshShaders.Contains('float connected = smoothstep(0.000010, 0.000180, uRainWettingExposureM);') -and
    $perf19EntityMeshShaders.Contains('smoothstep(0.02, 0.25, rainIntensity)') -and
    -not $perf19EntityMeshShaders.Contains('float ring = sin((n1 + uSurfacePresentationTime') -and
    $perf19EntityMeshShaders.Contains('dynamicSurfaceFlowDirection') -and
    $perf19EntityMeshShaders.Contains('uSurfaceWetnessBreakupMask') -and
    $perf19EntityMeshShaders.Contains('dynamicSurfacePuddleDepthM = standingDepthM;') -and
    $perf19EntityMeshShaders.Contains('float runningDepthM = gpuValid ? kinematicRunoffDepthM(gpuState) : 0.0;') -and
    $perf19EntityMeshShaders.Contains('float headDeficitM = kStandingWaterMaxDepthM') -and
    $perf19EntityMeshShaders.Contains('float freeSurfaceDepthM = max(puddleDepthM, 0.0);') -and
    $perf19EntityMeshShaders.Contains('float runoffDepthM = max(presentationDepthM, 0.0);') -and
    $perf19EntityMeshShaders.Contains('smoothstep(visibleWaterOnsetM, kStandingWaterMaxDepthM,') -and
    (-not $perf19EntityMeshShaders.Contains('mix(0.028, 0.0')) -and
    (-not $perf19EntityMeshShaders.Contains('vWorldPosition, max(standingDepthM, runningDepthM)'))
) "LIVETRACK21I keeps compressed v15 topology, makes the literal 0..0.70mm ladder the effective standing-water authority, and keeps range-gated fast filtering"
Check (
    $perf19EntityMeshShaders.Contains('const vec2 offsets[4] = vec2[4](') -and
    $perf19EntityMeshShaders.Contains('decoded.depthM = depthSum * inverseWeight;') -and
    $perf19EntityMeshShaders.Contains('decoded.runoffPotential = runoffSum * inverseWeight;') -and
    $perf19EntityMeshShaders.Contains('normalize(flowSum)') -and
    $perf19EntityMeshShaders.Contains('bool gpuFarWaterDecoded(vec3 positionRelative, out GpuWaterDecoded decoded)') -and
    $perf19EntityMeshShaders.Contains('lodDetail = 1.0 - smoothstep(450.0, 500.0, distanceM);') -and
    $perf19EntityMeshShaders.Contains('if (uHasEnvironmentMap)')
) "LIVETRACK15 keeps seam-safe near sampling, lower-resolution far topology and a 450-500m optical fade"
Check (
    -not $dsurfSurfaceWorld.Contains('m_dynamicSurface.advanceHydro(') -and
    -not $dsurfSurfaceWorld.Contains('m_dynamicSurface.sampleHydro(') -and
    $dsurfSurfaceWorld.Contains('if (m_gpuDynamicSurfaceAuthorityEnabled)') -and
    $dsurfSurfaceWorld.Contains('m_gpuDynamicSurfaceTireEvents') -and
    $dsurfSurfaceWorld.Contains('no CPU spatial-water fallback') -and
    -not (Test-Path (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.cpp")) -and
    (Test-Path (Join-Path $Root "Engine\HeritageEngine\Tests\Reference\DynamicSurfaceHydrologyReference.cpp")) -and
    -not $perf19EntityMeshShaders.Contains('uDynamicSurfaceHydroPages') -and
    -not $perf19WetnessAtlas.Contains('dynamicSurface.rasterHydroPage(') -and
    -not $perf19WetnessAtlas.Contains('uploadHydroMip(')
) "OPT03C retires CPU Hydro from production while preserving a test-only regression reference"
Check (
    -not $job01EngineProject.Contains('DynamicSurfaceHydrology.cpp') -and
    -not $job01EngineProject.Contains('DynamicSurfaceHydrology.hpp') -and
    $job01TestProject.Contains('Reference\DynamicSurfaceHydrologyReference.cpp') -and
    $job01TestProject.Contains('Reference\DynamicSurfaceHydrologyReference.hpp') -and
    (Test-Path (Join-Path $Root "Docs\OPT03C_SINGLE_GPU_WATER_AUTHORITY.md")) -and
    (Test-Path (Join-Path $Root "Build\Reports\OPT03C_SingleGpuWaterAuthority.txt"))
) "OPT03C production/test project ownership and milestone documentation enforce single GPU water authority"
Check (
    -not $perf19EntityMesh.Contains('drawWetFilmPass(') -and
    -not $perf19WetnessAtlas.Contains('glDrawArrays(GL_TRIANGLES, 0, 3)')
) "LIVETRACK06 keeps water optics in the ordinary authored material draw with no duplicate puddle mesh"
Check (
    $job01Overlay.Contains('LIVETRACK21 STANDING + RUNNING WATER <=500m / 20Hz TOPOLOGY POLL') -and
    $job01Overlay.Contains('Near topology: %u/%u resident | %u visible <=100m | %u prefetch') -and
    $job01Overlay.Contains('Far topology <=500m: %u/%u resident | %u admitted this poll | unavailable %u') -and
    $job01Overlay.Contains('static .hhyd v15: priority-flood 4-bit standing-depth ceiling + total-contributing MFD catchment + flow direction; terminal minima retain catchment area') -and
    $job01Overlay.Contains('100-500m: complete lower-resolution prebaked runoff/standing-depth/flow set; every desired tile admitted together') -and
    $job01Overlay.Contains('tile membership poll: fixed 20Hz; no progressive far streamer; optical shading remains per-frame') -and
    $job01Overlay.Contains('rain water full-field CFD: OFF') -and
    $job01Overlay.Contains('synchronous atlas readback: OFF')
) "LIVETRACK15 F8 exposes compressed near/far topology residency, complete 500m admission, 20Hz polling and stutter guards"
Check (
    -not $job01Overlay.Contains('Virtual pipes: active') -and
    -not $job01Overlay.Contains('waterMaximumVirtualPipeFluxLps')
) "LIVETRACK04 keeps retired virtual-pipe water presentation dead"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-078-Distance-Adaptive-Multi-Source-Hydrology-Cadence.md")) "JOB03 distance-adaptive multi-source hydrology decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\JOB03_DistanceAdaptiveHydrology.txt")) "JOB03 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF11_ADAPTIVE_WATER_PRESENTATION.md")) "PERF11 adaptive water-presentation architecture is documented"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-096-Adaptive-Cadence-Aligned-Water-Presentation.md")) "PERF11 adaptive water-presentation ADR is present"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF11_AdaptiveWaterPresentation.txt")) "PERF11 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF12_ADAPTIVE_WATER_MESH_V2.md")) "PERF12 adaptive water mesh v2 architecture is documented"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-097-Adaptive-Water-Mesh-Across-All-Visual-Rings.md")) "PERF12 adaptive water mesh v2 ADR is present"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF12_AdaptiveWaterMeshV2.txt")) "PERF12 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF13_SEAMLESS_WATER_MATERIAL.md")) "PERF13 seamless water material architecture is documented"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-098-Seamless-World-Space-Water-Material.md")) "PERF13 seamless water material ADR is present"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF13_SeamlessWaterMaterial.txt")) "PERF13 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF14_ADAPTIVE_CONTOUR_WATER_TESSELLATION.md")) "PERF14 adaptive contour water tessellation architecture is documented"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-099-Adaptive-Contour-Water-Tessellation.md")) "PERF14 adaptive contour water tessellation ADR is present"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF14_AdaptiveContourWaterTessellation.txt")) "PERF14 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF16_CONNECTED_WATER_SURFACE.md")) "PERF16 connected-water surface architecture is documented"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-100-Connected-Hydrology-Water-Surface.md")) "PERF16 connected-water surface ADR is present"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF16_ConnectedWaterSurface.txt")) "PERF16 milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\PERF16A_DRIVABLE_CONNECTED_WATER.md")) "PERF16A drivable connected-water hotfix is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\PERF16A_DrivableConnectedWater.txt")) "PERF16A milestone report is present"

# WEATHER06A: integrated storm presentation stays modular and bounded. Weather
# authority/hydrology remain separate from view-specific OpenGL representation.
$weather06RendererPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WeatherPresentationRenderer.cpp"
$weather06RendererHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\WeatherPresentationRenderer.hpp"
$weather06SkyPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.cpp"
$weather06SkyAtmosphereShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererAtmosphereShaders.cpp"
$weather06SkyCloudShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererCloudShaders.cpp"
$weather06SkyPbrAtmosphereShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRendererPbrAtmosphereShaders.cpp"
$weather06EntityPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$weather06EntityRegionalWeatherPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRegionalWeather.cpp"
$weather06EnvironmentPath = Join-Path $Root "Engine\HeritageEngine\Graphics\EnvironmentMap.cpp"
$weather06ProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$weather06Renderer = ReadText $weather06RendererPath
$weather06RendererHeader = ReadText $weather06RendererHeaderPath
$weather06Sky = (ReadText $weather06SkyPath) + "`n" + (ReadText $weather06SkyAtmosphereShadersPath) + "`n" + (ReadText $weather06SkyCloudShadersPath) + "`n" + (ReadText $weather06SkyPbrAtmosphereShadersPath)
$weather06Entity = ReadText $weather06EntityPath
$weather06EntityRegionalWeather = ReadText $weather06EntityRegionalWeatherPath
$weather06Environment = ReadText $weather06EnvironmentPath
$weather06Project = ReadText $weather06ProjectPath
Check ((Test-Path $weather06RendererPath) -and (Test-Path $weather06RendererHeaderPath) -and $weather06Project.Contains('Graphics\Renderer\WeatherPresentationRenderer.cpp') -and $weather06Project.Contains('Graphics\Renderer\WeatherPresentationRenderer.hpp')) "WEATHER06A modular weather presentation renderer is compiled by the engine"
Check (($weather06Renderer.Contains("kRainGridX = 32") -and $weather06Renderer.Contains("glDrawArraysInstanced")) -or ($weather06Renderer.Contains("kRainComputeShader") -and $weather06Renderer.Contains("glDrawArraysIndirect") -and $weather06Renderer.Contains("absoluteCell = uBaseCell + localCell"))) "WEATHER06A+ falling rain remains a bounded world-space GPU population rather than CPU particles"
# VCLOUD01 restores the actual HDRP-derived UnityVolumetricCloudsURP density
# and lighting model after the long CLOUDURP15 tuning chain drifted into a
# Heritage-specific artistic marcher. Keep a strict safety-net around the
# upstream-derived four-preset LUT, bounded 32-step/empty-space ray march,
# two-light-step dual-HG multiple scattering, PBSKY transmittance coupling,
# cloud-only temporal/depth semantics and the upstream 16-segment shadow trace.
$cloudUrp15DocPath = Join-Path $Root "Docs\CLOUDURP15_UNITY_VOLUMETRIC_CLOUDS_URP_PORT.md"
$vcloud01DocPath = Join-Path $Root "Docs\VCLOUD01_HDRP_DERIVED_VOLUMETRIC_CLOUDS.md"
$cloudUrp15NoticePath = Join-Path $Root "Docs\ThirdParty\UnityVolumetricCloudsURP_NOTICE.txt"
$cloudUrp15ShapePath = Join-Path $Root "Modules\RacingUnited\Assets\Weather\Clouds\UnityVolumetricCloudsURP\WorleyNoise128R.hvol"
$cloudUrp15ErosionPath = Join-Path $Root "Modules\RacingUnited\Assets\Weather\Clouds\UnityVolumetricCloudsURP\PerlinNoise32R.hvol"
$cloudUrp15Header = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\SkyRenderer.hpp")
$cloudUrp15StarMapPath = Join-Path $Root "Modules\RacingUnited\Assets\Scenes\Sky\Scene_NightSky.ktx2"
$cloudUrp15EnvironmentSystem = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\EnvironmentSystem.cpp")
$cloudUrp15Texture2D = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Texture2D.cpp")
$cloudUrp15LuaEnvironment = ReadText (Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\LuaEnvironmentBindings.cpp")
Check (
    (Test-Path $cloudUrp15DocPath) -and
    (Test-Path $vcloud01DocPath) -and
    (Test-Path $cloudUrp15NoticePath) -and
    (Test-Path $cloudUrp15ShapePath) -and
    (Test-Path $cloudUrp15ErosionPath) -and
    $weather06Sky.Contains("EARTH_RADIUS=6378100.0") -and
    $weather06Sky.Contains("CLOUD_SHELL_MIN_ALTITUDE_M=900.0") -and
    $weather06Sky.Contains("CLOUD_SHELL_MAX_ALTITUDE_M=6500.0") -and
    $weather06Sky.Contains("MAX_SKYBOX_VOLUMETRIC_CLOUDS_DISTANCE=200000.0") -and
    $weather06Sky.Contains("PRIMARY_STEPS=32") -and
    $weather06Sky.Contains("NUM_LIGHT_STEPS=2") -and
    $weather06Sky.Contains("EMPTY_STEPS_BEFORE_LARGE_STEPS=8") -and
    $weather06Sky.Contains("NUM_MULTI_SCATTERING_OCTAVES=2") -and
    $weather06Sky.Contains("FORWARD_ECCENTRICITY=0.7") -and
    $weather06Sky.Contains("BACKWARD_ECCENTRICITY=0.7") -and
    $weather06Sky.Contains("POWDER_EFFECT_INTENSITY=0.25") -and
    $weather06Sky.Contains("const float shapeScale=5.0") -and
    $weather06Sky.Contains("vec4(0.32,0.32,0.18,0.245)") -and
    $weather06Sky.Contains("cloudCoverageData.coverage*cloudCoverageData.coverage") -and
    $weather06Sky.Contains("properties.sigmaT=mix(0.04,0.12,cloudCoverageData.rainClouds)") -and
    $weather06Sky.Contains("evaluateSunTransmittance") -and
    $weather06Sky.Contains("henyeyGreenstein") -and
    $weather06Sky.Contains("uPbrTransmittanceLut") -and
    $weather06Sky.Contains("uHistoryTexture") -and
    $weather06Sky.Contains("const float accumulationFactor=0.95") -and
    $weather06Sky.Contains("boxMin=min(boxMin") -and
    $weather06Sky.Contains("boxMax=max(boxMax") -and
    $weather06Sky.Contains("vec3 prevColor=clamp(historyPoint(prevUv),boxMin,boxMax)") -and
    $weather06Sky.Contains("float intensity=clamp(min(accumulationFactor-abs(velocity.x)*accumulationFactor") -and
    $weather06Sky.Contains("FragColor=vec4(mix(cur.rgb,prevColor,intensity),cur.a)") -and
    $weather06Sky.Contains("glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST)") -and
    $weather06Sky.Contains("glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MAG_FILTER,GL_NEAREST)") -and
    $weather06Sky.Contains("float depth=uLocalClouds?sceneDepthAt(vUv):0.0") -and
    $weather06Sky.Contains("FragColor=vec4(cloud.rgb+scene*cloud.a,cloud.a)") -and
    $weather06Sky.Contains('C(m_combine.scene,"uSceneTexture")') -and
    $weather06Sky.Contains("glBlendFuncSeparate(GL_ONE,GL_ZERO,GL_ZERO,GL_ONE)") -and
    $weather06Sky.Contains("float integrationJitter=integrationNoise()") -and
    $weather06Sky.Contains("float currentDistance=integrationJitter") -and
    $weather06Sky.Contains("float relativeStepSize=mix(integrationJitter,1.0") -and
    $weather06Sky.Contains('R(m_ray.temporalFrameIndex,"uTemporalFrameIndex")') -and
    $weather06Sky.Contains("glUniform1ui(m_ray.temporalFrameIndex,m_cloudTemporalFrameIndex++)") -and
    $weather06Sky.Contains("for(int i=1;i<16;++i)") -and
    $weather06Sky.Contains("kCloudShadowFilterFragmentShader") -and
    $cloudUrp15Header.Contains("outputCloudDepthToScene") -and
    $cloudUrp15Header.Contains("localClouds") -and
    $cloudUrp15Header.Contains("authoredCloudCover") -and
    $cloudUrp15Header.Contains("bool physicallyBasedSun = true") -and
    $cloudUrp15Header.Contains("int m_cloudShadowResolution = 256") -and
    -not $weather06Sky.Contains("visibleFormationStrength()") -and
    -not $weather06Sky.Contains("cloudBodyBlend()")
) "VCLOUD01 volumetric clouds preserve the HDRP-derived Unity URP density/march/lighting/temporal/depth/shadow architecture"

# CELESTIAL01 keeps astronomy/weather/cloud ownership singular while allowing
# both physical celestial sources to illuminate the cloud volume. Ground cloud
# attenuation reuses the one existing optical-depth cookie and follows the
# EnvironmentSystem continuous key direction, so Sun/Moon transitions do not
# create duplicate shadow textures or hard dawn/dusk ownership switches.
$celestial01DocPath = Join-Path $Root "Docs\CELESTIAL01_SUN_MOON_CLOUD_LIGHTING_SHADOWS.md"
$celestial01MeshShader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshShaders.hpp")
$celestial01RegionalWeather = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRegionalWeather.cpp")
Check (
    (Test-Path $celestial01DocPath) -and
    $weather06Sky.Contains('evaluateMoonTransmittance') -and
    $weather06Sky.Contains('result.moonScattering') -and
    $weather06Sky.Contains('moonRadiance=uMoonColor*PI*max(uMoonIntensity,0.0)') -and
    $weather06Sky.Contains('physicalCelestialTransmission(meanPosition,moonDirection)') -and
    $weather06Sky.Contains('uCelestialLightDirection') -and
    $weather06Sky.Contains('lighting.keyLightDirection.y<=0.01f') -and
    $weather06Sky.Contains('m_shadow.sunDirection,lighting.keyLightDirection.x') -and
    $celestial01MeshShader.Contains('vec3 volumetricCloudSunTransmission') -and
    $celestial01MeshShader.Contains('spectralShape * combinedTransmission') -and
    $celestial01RegionalWeather.Contains('const float moonTransmission = std::clamp') -and
    $celestial01RegionalWeather.Contains('1.0f - overcast * 0.20f - rain * 0.06f - storm * 0.04f')
) "CELESTIAL01 Sun/Moon cloud lighting shares VCLOUD density/PBSKY transmission and the ground cookie follows the continuous celestial key"


# CELESTIAL02 responds to visual validation: Moon-lit cloud interiors retain a
# broad higher-order fill while the one shared ground cookie carries stronger
# optical depth and cannot normalize away broad regional attenuation.
$celestial02DocPath = Join-Path $Root "Docs\CELESTIAL02_STRONGER_CLOUD_LIGHT_AND_GROUND_SHADOWS.md"
Check (
    (Test-Path $celestial02DocPath) -and
    $weather06Sky.Contains('MOON_INTERIOR_SCATTERING_STRENGTH') -and
    $weather06Sky.Contains('MOON_INTERIOR_DENSITY_SCALE') -and
    $weather06Sky.Contains('moonInteriorFill') -and
    $weather06Sky.Contains('shadowBase=clamp(transmittance-0.02') -and
    $celestial01MeshShader.Contains('float combinedTransmission=min(regionalLuminance,detailedTransmission)') -and
    -not $celestial01MeshShader.Contains('spectralShape * detailedTransmission')
) "CELESTIAL02+ retains lunar cloud interior fill and stronger direct-light cloud attenuation without adding a second cloud/shadow authority"

# CELESTIAL03 preserves one cloud/shadow authority while making the physically
# expected lunar aureole and receiver-visible ambient response explicit.
$celestial03DocPath = Join-Path $Root "Docs\CELESTIAL03_LUNAR_AUREOLE_AND_RECEIVER_CLOUD_SHADOWS.md"
Check (
    (Test-Path $celestial03DocPath) -and
    $weather06Sky.Contains('MOON_FORWARD_ECCENTRICITY=0.90') -and
    $weather06Sky.Contains('moonForwardAureole') -and
    $weather06Sky.Contains('float regionalFloor=mix(1.0,mix(0.52,0.26,receiverCoverage.rainClouds),coverageOcclusion)') -and
    $weather06Sky.Contains('float shadow=min(densityShadow,regionalFloor)') -and
    $celestial01MeshShader.Contains('celestialCloudTransmission') -and
    $celestial01MeshShader.Contains('cloudAmbientVisibility') -and
    $celestial01MeshShader.Contains('ambientLighting*=cloudAmbientVisibility*cloudAmbientTint')
) "CELESTIAL03 adds lunar droplet aureole and receiver-visible Sun/Moon cloud shadows without a second shadow system"


# CELESTIAL04 fixes the receiver wiring rather than further tuning an optional
# material branch. One fullscreen post-opaque receiver reconstructs opaque
# world positions from scene depth, samples the same single cloud optical-depth
# cookie, and multiplies the destination before cloud composition. Later cloud
# temporal-denoiser milestones may tune TAA independently of this receiver contract.
$celestial04DocPath = Join-Path $Root "Docs\CELESTIAL04_DEDICATED_CLOUD_SHADOW_RECEIVER_AND_WEAK_TAA.md"
Check (
    (Test-Path $celestial04DocPath) -and
    $weather06Sky.Contains('kCloudGroundShadowFragmentShader') -and
    $weather06Sky.Contains('relativeAtDepth(vec2 uv,float depth)') -and
    $weather06Sky.Contains('glBlendFuncSeparate(GL_ZERO,GL_SRC_COLOR,GL_ZERO,GL_ONE)') -and
    $weather06Sky.Contains('m_cloudGroundShadowProgram') -and
    $weather06Sky.Contains('MOON_CLOUD_SCATTER_EXPOSURE=5.0')
) "CELESTIAL04 routes one Sun/Moon cloud cookie through a dedicated opaque receiver pass without adding another shadow authority"

# CLOUDURP15E7 keeps CLOUDURP15E6's upstream temporal denoiser as the sole
# cloud-TAA path and selectively lengthens temporal integration only where the
# existing 5-pixel current neighbourhood exhibits stochastic RGB/transmittance
# disagreement. Dense coherent interiors stay on the upstream 0.95 baseline.
$cloudUrp15E7DocPath = Join-Path $Root "Docs\CLOUDURP15E7_SELECTIVE_STOCHASTIC_ACCUMULATION.md"
Check (
    (Test-Path $cloudUrp15E7DocPath) -and
    $weather06Sky.Contains('uniform sampler2D uCurrentTexture;uniform sampler2D uHistoryTexture;uniform bool uHistoryValid') -and
    $weather06Sky.Contains('const float accumulationFactor=0.95') -and
    $weather06Sky.Contains('float minTransmittance=min(cur.a') -and
    $weather06Sky.Contains('if(minTransmittance>=0.999999)') -and
    $weather06Sky.Contains('float stochasticGrain=max(rgbGrain,alphaGrain)') -and
    $weather06Sky.Contains('float exposedSample=smoothstep(0.035,0.32,transmittanceMean)') -and
    $weather06Sky.Contains('float adaptiveAccumulation=mix(accumulationFactor,0.985,mildGrain)') -and
    $weather06Sky.Contains('adaptiveAccumulation=mix(adaptiveAccumulation,0.9975,strongGrain)') -and
    $weather06Sky.Contains('intensity=max(intensity,adaptiveIntensity)') -and
    $weather06Sky.Contains('vec3 prevColor=clamp(historyPoint(prevUv),boxMin,boxMax)') -and
    $weather06Sky.Contains('float depth=uLocalClouds?sceneDepthAt(vUv):0.0') -and
    $weather06Sky.Contains('glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST)') -and
    $weather06Sky.Contains('glSamplerParameteri(m_cloudHistorySampler,GL_TEXTURE_MAG_FILTER,GL_NEAREST)') -and
    $weather06Sky.Contains('glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,m_cloudCombinedTexture)') -and
    $weather06Sky.Contains('float integrationJitter=integrationNoise()') -and
    -not $weather06Sky.Contains('const float stableHistory=0.20') -and
    -not $weather06Sky.Contains('const float mildHistory=0.60') -and
    -not $weather06Sky.Contains('const float strongTaaIntensity=50.0') -and
    -not $weather06Sky.Contains('nativeCurrentNeighbor') -and
    -not $weather06Sky.Contains('float cloudDepth=cloudDepthAt(vUv)') -and
    -not $weather06Sky.Contains('GL_TEXTURE_MIN_FILTER,GL_LINEAR);glSamplerParameteri(m_cloudHistorySampler')
) "CLOUDURP15E7 keeps one upstream point-history TAA and selectively increases accumulation only for stochastic partial cloud samples"


Check (
    (Test-Path $weather06SkyPbrAtmosphereShadersPath) -and
    $weather06Sky.Contains("jiaozi158/UnityPhysicallyBasedSkyURP (MIT)") -and
    $weather06Sky.Contains("rayleighPhase") -and
    $weather06Sky.Contains("miePhase") -and
    $weather06Sky.Contains("uPbrSkyViewLut") -and
    $weather06Sky.Contains("uPbrTransmittanceLut") -and
    $weather06Sky.Contains("moonTransmission") -and
    $weather06Sky.Contains("moonAngle") -and
    $weather06Sky.Contains("bottomAltitude=presetValue(vec4(3000.0,1200.0,1500.0,1000.0),cloudType)") -and
    $weather06Sky.Contains("altitudeRange=presetValue(vec4(1000.0,2000.0,2500.0,5000.0),cloudType)") -and
    $weather06Sky.Contains("positionPS.xz-uCameraGlobal.xz")
) "PBSKY01 physical sky remains paired with world-anchored VCLOUD01 upstream preset altitude structure"

Check (
    $weather06Sky.Contains("emissiveStarRadiance") -and
    $weather06Sky.Contains("starFootprint=min(fwidth(starUv),starTexel*6.0)") -and
    $weather06Sky.Contains("stellarBloom*=0.25*0.026") -and
    $weather06Sky.Contains("sourceScale=min(1.0,1.80/max(sampleLuminance,0.0001))") -and
    $weather06Sky.Contains("starTransmission=atmosphereTransmission(direction)")
) "PBSKY01A preserves CLOUDURP15P peak-only HDR stellar micro-bloom under physical atmospheric extinction"

Check (
    $weather06EntityRegionalWeather.Contains("CLOUDURP15J7: weather is no longer allowed to act as a second hidden") -and
    $weather06EntityRegionalWeather.Contains("weatherAdjustedLighting") -and
    $weather06Sky.Contains("uPbrSkyViewLut") -and
    $weather06Sky.Contains("uPbrTransmittanceLut") -and
    $weather06Sky.Contains("Full moon is composited AFTER tone mapping") -and
    $weather06Sky.Contains("uEnvironmentMap") -and
    $weather06Sky.Contains("uMoonTexture") -and
    $weather06Sky.Contains("atmosphereTransmission") -and
    -not $weather06Sky.Contains("referenceCloudLuma") -and
    -not $weather06Sky.Contains("fxaaCloud") -and
    -not $weather06Sky.Contains("nightSkyBlend")
) "PBSKY01 physical atmosphere owns visible sky/night while regional weather remains a lighting/cloud modulation authority"
Check (
    (Test-Path $cloudUrp15StarMapPath) -and
    $weather06Sky.Contains("uStarMapTexture") -and
    $weather06Sky.Contains("uWorldToCelestialRow0") -and
    ($weather06Sky.Contains("0.5-ra/6.28318530718") -or $weather06Sky.Contains("0.5-ra/(2.0*PI)")) -and
    -not $weather06Sky.Contains("hash13(vec3 p)") -and
    $cloudUrp15Texture2D.Contains("KTX2 texture") -and
    $cloudUrp15Texture2D.Contains("GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT") -and
    $cloudUrp15EnvironmentSystem.Contains("greenwichMeanSiderealRadians") -and
    $cloudUrp15EnvironmentSystem.Contains("solarEquatorial") -and
    $cloudUrp15EnvironmentSystem.Contains("lunarEquatorial") -and
    $cloudUrp15EnvironmentSystem.Contains("worldToCelestialRow0") -and
    $cloudUrp15LuaEnvironment.Contains("heritage.latitude_deg") -and
    $cloudUrp15LuaEnvironment.Contains("heritage.longitude_deg") -and
    $cloudUrp15LuaEnvironment.Contains('registerFunction("Environment", "ApplySceneMetadata"')
) "PBSKY01 keeps the BC6H NASA celestial map with geographic/calendar sidereal rotation and GLB scene metadata"
Check ((Test-Path $weather06EntityRegionalWeatherPath) -and $weather06EntityRegionalWeather.Contains("weatherAdjustedLighting") -and $weather06EntityRegionalWeather.Contains("weatherFogDensity") -and -not $weather06Entity.Contains("kRainVertexShader") -and -not $weather06EntityRegionalWeather.Contains("kRainVertexShader") -and $weather06Renderer.Contains("kRainVertexShader")) "WEATHER06A entity renderer consumes regional storm lighting/fog while rain particles remain owned by WeatherPresentationRenderer"
Check ($weather06Environment.Contains("environmentLightingDifference") -and $weather06Environment.Contains("lightingChanged")) "WEATHER06A environment cubemap refresh responds to weather lighting changes"
$weather06ALegacyWetFilm = $job02SurfaceRenderer.Contains("wetSpotLayer")
$perf13WorldSpaceWetFilm = (
    $job02SurfaceRenderer.Contains("rippleCellSize") -and
    $job02SurfaceRenderer.Contains("rippleBand") -and
    $job02SurfaceRenderer.Contains("gSurfaceCoord")
)
$weather06LegacyHydrologyWetFilm = (
    ($weather06ALegacyWetFilm -or $perf13WorldSpaceWetFilm) -and
    $job02SurfaceRenderer.Contains("uPrecipitationRateMmPerHour") -and
    -not $job02SurfaceRenderer.Contains("for (int neighbor")
)
# LIVETRACK21/PERF09 supersedes the old CPU Hydro material-page presentation
# and the obsolete renderer-side DynamicSurface GPU page uploader. The smooth
# weather film is read directly from SurfaceWorld weather output in the entity
# draw coordinator, while EntityMeshSurfaceWetness only binds that scalar plus
# the fixed prebaked GPU puddle atlases. OPT03 keeps this ownership split.
$liveTrack21PrebakedWetFilm = (
    $perf19EntityMesh.Contains("m_surfaceWeatherFilmWetness = static_cast<float>(std::clamp(") -and
    $perf19EntityMesh.Contains("weatherOutput.effectiveWetness") -and
    $perf19WetnessAtlas.Contains("glUniform1f(m_uniforms.surfaceWeatherFilmWetness, m_surfaceWeatherFilmWetness);") -and
    $perf19WetnessAtlas.Contains("m_dynamicSurfaceGpuRuntime.stats().backgroundSeedDepthM") -and
    -not $perf19WetnessAtlas.Contains("dynamicSurface.rasterHydroPage(") -and
    -not $perf19WetnessAtlas.Contains("uploadHydroMip(") -and
    $perf19EntityMeshShaders.Contains("uSurfaceWeatherFilmWetness") -and
    $perf19EntityMeshShaders.Contains("uPrebakedWaterExposureM") -and
    $perf19EntityMeshShaders.Contains("uGpuWaterAtlas") -and
    -not $perf19EntityMeshShaders.Contains("uDynamicSurfaceHydroPages") -and
    $perf19EntityMeshShaders.Contains("applyDynamicSurfaceWater") -and
    -not $perf19EntityMesh.Contains("drawWetFilmPass(") -and
    -not $perf19WetnessAtlas.Contains("for (int neighbor") -and
    -not $perf19EntityMeshShaders.Contains("for (int neighbor") -and
    -not $dsurfGpuWaterAuthority.Contains("for (int neighbor")
)
Check ($weather06LegacyHydrologyWetFilm -or $liveTrack21PrebakedWetFilm) "WEATHER06A+ wet-surface presentation uses scene wetness plus fixed prebaked GPU puddles without runtime neighbour loops"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-079-Integrated-OpenGL-Weather-Presentation.md")) "WEATHER06A OpenGL weather presentation decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER06A_IntegratedRainClouds.txt")) "WEATHER06A milestone report is present"

# WEATHER06D: the live WEATHER06C fullscreen streak veil visibly travelled with
# the camera. Keep rain existence/trajectory in world cells and derive direct-
# precipitation cover from the already-baked layered hydrology surface field.
Check (-not $weather06Renderer.Contains("kRainOverlayVertexShader") -and -not $weather06Renderer.Contains("kRainOverlayFragmentShader")) "WEATHER06D removes camera-attached fullscreen streak rain from the live renderer"
Check ($weather06Renderer.Contains("hasPrecipitationCoverAbove(") -and $job01HydrologyCover.Contains("bool SurfaceHydrology::hasPrecipitationCoverAbove(") -and -not $weather06Renderer.Contains("raycast")) "WEATHER06H precipitation-cover diagnostics use an exact hydrology-column query instead of per-drop CPU raycasts"
$weather07b7SupersedesCpuFallback = $weather06Renderer.Contains("kRainComputeShader") -and $weather06Renderer.Contains("glDispatchCompute") -and $weather06Renderer.Contains("intentionally has no per-drop CPU rain fallback")
Check (($weather06Renderer.Contains("kRainFallbackVertexShader") -and $weather06Renderer.Contains("RainFallbackVertex")) -or $weather07b7SupersedesCpuFallback) "WEATHER06I fallback history remains documented, or the modern GPU rain path explicitly supersedes executable per-drop CPU fallback"
Check ($weather07b7SupersedesCpuFallback -and -not $weather06Renderer.Contains("kRainFallbackVertexShader") -and -not $weather06Renderer.Contains("m_rainFallbackVbo")) "WEATHER07C1 removes dead legacy CPU fallback shader/VBO ownership from the modern-only rain renderer"
Check ($weather06Renderer.Contains("never let shelter classification erase the entire rain pass") -and -not $weather06Renderer.Contains("if (cameraUnderPrecipitationCover)
        return;")) "WEATHER06I shelter classification cannot early-return the complete rain renderer"
Check (($weather06Renderer.Contains("worldCell = uBaseCell + localCell") -or $weather06Renderer.Contains("absoluteCell = uBaseCell + localCell")) -and $weather06Renderer.Contains("terminalVelocityMps") -and -not $weather06Renderer.Contains("fallSpeed = mix(22.0, 38.0")) "WEATHER07A visible rain keeps world-cell identity and physical terminal velocity rather than inventing 22-38 m/s fall speeds"
Check ($job01Hydrology.Contains("precipitationExposed") -and $job01Hydrology.Contains("highestSurfaceByColumn") -and $job01Hydrology.Contains("cell.precipitationExposed")) "WEATHER06D authoritative hydrology suppresses direct rainfall beneath higher layered cover"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-081-World-Anchored-Rain-And-Precipitation-Coverage.md")) "WEATHER06D rain anchoring and precipitation-cover decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER06D_WorldAnchoredRainCover.txt")) "WEATHER06D milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-082-World-Space-Visible-Rain-And-Natural-Gear-Ordering.md")) "WEATHER06F world-space visible rain and natural gear-order decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER06F_WorldVolumeRainNaturalGears.txt")) "WEATHER06F milestone report is present"


# WEATHER07A: replace presentation-authored rain motion with a reusable physical
# precipitation authority. Rainfall mass stays in mm/h for hydrology while the
# statistical drop population supplies size, mass and terminal-speed structure.
$weather07MicrophysicsPath = Join-Path $Root "Engine\HeritageEngine\Physics\Weather\RainMicrophysics.cpp"
$weather07MicrophysicsHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Weather\RainMicrophysics.hpp"
$weather07FieldPath = Join-Path $Root "Engine\HeritageEngine\Physics\Weather\PrecipitationField.cpp"
$weather07FieldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Weather\PrecipitationField.hpp"
$weather07SurfaceWorldHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.hpp"
$weather07SurfaceWeatherHeaderPath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWeather.hpp"
$weather07LuaWeatherPath = Join-Path $Root "Engine\HeritageEngine\Core\Modules\LuaBindings\Physics\LuaPhysicsWorldBindings.cpp"
$weather07SurfacePanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Scene\WeatherPanel.lua"
$weather07VehicleSurfacePanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\SurfacesPanel.lua"
$weather07ScenePanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Prototype\ScenePanel.lua"
$weather07RegressionPath = Join-Path $Root "Engine\HeritageEngine\Tests\WeatherRegression.cpp"
$weather07Microphysics = ReadText $weather07MicrophysicsPath
$weather07Field = ReadText $weather07FieldPath
$weather07FieldHeader = ReadText $weather07FieldHeaderPath
$weather07SurfaceWorldHeader = ReadText $weather07SurfaceWorldHeaderPath
$weather07SurfaceWeatherHeader = ReadText $weather07SurfaceWeatherHeaderPath
$weather07LuaWeather = ReadText $weather07LuaWeatherPath
$weather07SurfacePanel = ReadText $weather07SurfacePanelPath
$weather07VehicleSurfacePanel = ReadText $weather07VehicleSurfacePanelPath
$weather07ScenePanel = ReadText $weather07ScenePanelPath
$weather07Regression = ReadText $weather07RegressionPath
Check ((Test-Path $weather07MicrophysicsHeaderPath) -and (Test-Path $weather07FieldHeaderPath) -and $weather06Project.Contains('Physics\Weather\RainMicrophysics.cpp') -and $weather06Project.Contains('Physics\Weather\PrecipitationField.cpp')) "WEATHER07A physical rain microphysics and precipitation field are compiled as reusable engine subsystems"
Check ($weather07Microphysics.Contains("kMarshallPalmerLambdaCoefficient = 4.1") -and $weather07Microphysics.Contains("kMarshallPalmerLambdaRainExponent = -0.21") -and $weather07Microphysics.Contains("9.65 - 10.3 * std::exp(-0.6 * dMm)")) "WEATHER07A rain population uses Marshall-Palmer size structure and bounded Atlas terminal velocity"
Check ($weather07Microphysics.Contains("requestedVolumeFluxMps") -and $weather07Microphysics.Contains("populationScale = requestedVolumeFluxMps / baseVolumeFlux") -and $weather07Microphysics.Contains("massFluxKgPerM2PerSecond")) "WEATHER07A statistical drop population is mass-normalized to authoritative mm/h rainfall"
Check ($weather07FieldHeader.Contains("class PrecipitationField") -and $weather07Field.Contains("hashCell(") -and $weather07Field.Contains("sampleRainRepresentative") -and $weather07Field.Contains("m_elapsedSeconds * drop.terminalVelocityMps")) "WEATHER07A precipitation representatives are deterministic world-cell trajectories rather than camera-owned particles"
Check ($weather07SurfaceWorldHeader.Contains("weather::PrecipitationField m_precipitation") -and $weather07SurfaceWorldHeader.Contains("const weather::PrecipitationField& precipitation() const")) "WEATHER07A SurfaceWorld exposes one shared physical precipitation field to all views"
Check ($weather07SurfaceWeatherHeader.Contains("windDirectionDegrees") -and $weather07LuaWeather.Contains('"wind_direction_deg"') -and $weather07SurfacePanel.Contains('"Wind direction"') -and $weather06Sky.Contains("uWindVelocityXZ") -and -not $weather06Sky.Contains("vec2(0.72, 0.69)")) "WEATHER07A weather has explicit world wind heading shared by precipitation and cloud advection rather than a hard-coded renderer direction"
Check (
    -not (Test-Path (Join-Path $Root "Modules\RacingUnited\Scripts\UI\Scene\WaterLaboratoryPanel.lua")) -and
    -not (Test-Path (Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\WaterLaboratoryPanel.lua")) -and
    $weather07ScenePanel.Contains('UI.BeginTabItem("WEATHER")') -and
    $weather07ScenePanel.Contains('DrawSceneWeatherPanel()') -and
    $weather07SurfacePanel.Contains('SCENE WEATHER / PRECIPITATION') -and
    $weather07SurfacePanel.Contains('PREBAKED WATER RUNTIME') -and
    -not $weather07SurfacePanel.Contains('DrawWaterLaboratoryPanel()') -and
    -not $weather07VehicleSurfacePanel.Contains('Physics.GetSurfaceWeather()')
) "LIVETRACK11 keeps scene weather under SCENE -> WEATHER and deletes Water Laboratory UI"
Check ($weather06Renderer.Contains("surfaces.precipitation()") -and $weather06Renderer.Contains("fluxWeightedMeanTerminalVelocityMps") -and $weather06Renderer.Contains("uDropLambdaPerMm") -and $weather06Renderer.Contains("terminalVelocityMps")) "WEATHER07A current OpenGL rain presentation consumes the physical precipitation population, terminal-speed law and shared wind/time authority"
Check ($weather07Regression.Contains("massFluxMatches") -and $weather07Regression.Contains("deterministicIdentity") -and $weather07Regression.Contains("terminalVelocityCurve")) "WEATHER07A native regression covers rainfall mass, realistic terminal speed and deterministic world identity"
Check ($job01Overlay.Contains("physical mean %.2f mm") -and $job01Overlay.Contains("flux fall %.2f m/s")) "WEATHER07A F8 diagnostics expose physical rain size and fall-speed authority"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-091-Physical-Rain-Microphysics-And-World-Precipitation-Field.md")) "WEATHER07A physical precipitation decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER07A_PhysicalPrecipitationFoundation.txt")) "WEATHER07A milestone report is present"

# WEATHER07C1: the settled rain material intentionally uses one tiny base-colour
# + alpha texture plus live environment reflection. Normal/thickness maps are no
# longer required for fast-moving airborne rain.
$weather07bOpacityPath = Join-Path $Root "Modules\RacingUnited\Assets\Weather\Rain\RainDrop_BC.png"
Check (Test-Path $weather07bOpacityPath) "WEATHER07C1 Racing United ships the base-colour/alpha rain optical map"
Check ($weather06RendererHeader.Contains("initialize(const std::filesystem::path& moduleAssetRoot)") -and $weather06Renderer.Contains('moduleAssetRoot / "Weather" / "Rain"') -and $weather06Renderer.Contains('RainDrop_BC.png') -and -not $weather06Renderer.Contains('rain normal texture unavailable') -and -not $weather06Renderer.Contains('rain thickness texture unavailable')) "WEATHER07C1 rain optics use only the engine-generic Weather/Rain base-colour/alpha contract"
$weather07bUsesPhysicalOpticalSizing = $weather06Renderer.Contains("sampleDiameterMm") -and $weather06Renderer.Contains("terminalVelocityMps") -and $weather06Renderer.Contains("exposureSeconds") -and $weather06Renderer.Contains("uDropLambdaPerMm")
$weather07b2ScaleDiagnostic = $weather06Renderer.Contains("WEATHER07B2 DIAGNOSTIC") -and $weather06Renderer.Contains("float streakLength = 2.0") -and $weather06Renderer.Contains("float opticalWidth = 1.0") -and (Test-Path (Join-Path $Root "Build\Reports\WEATHER07B2_2mRainDiagnostic.txt"))
Check ($weather07bUsesPhysicalOpticalSizing -or $weather07b2ScaleDiagnostic) "WEATHER07B textured streak geometry uses physical optical sizing, except the explicit bounded WEATHER07B2 2 m visibility diagnostic"
Check ($weather06Renderer.Contains("uRainOpacityTexture") -and $weather06Renderer.Contains("uEnvironmentMap") -and $weather06Renderer.Contains("fresnel") -and $weather06Renderer.Contains("procedural bulge") -and -not $weather06Renderer.Contains("texture(uRainNormalTexture") -and -not $weather06Renderer.Contains("texture(uRainThicknessTexture")) "WEATHER07C1 rain shader uses base/alpha plus environment reflection without normal/thickness texture sampling"
$weather07b5ScientificSizing = $weather06Renderer.Contains("0.00020") -and $weather06Renderer.Contains("0.00600") -and ($weather06Renderer.Contains("presentationExposureSeconds") -or $weather06Renderer.Contains("exposureSeconds")) -and $weather06Renderer.Contains("opticalAreaCompensation") -and $weather06Renderer.Contains("pixelWorldWidth") -and $weather06Renderer.Contains("terminalVelocityMps")
Check $weather07b5ScientificSizing "WEATHER07B5 uses millimetre physical drop diameters, exposure-derived streak length and sub-pixel area compensation"
Check (-not $weather06Renderer.Contains("WEATHER07B2 DIAGNOSTIC")) "WEATHER07B5 removes the 2 metre rain visibility diagnostic from executable rain geometry"
$weather07bModernGpuRain = $weather06Renderer.Contains("kRainComputeShader") -and $weather06Renderer.Contains("glDispatchCompute") -and $weather06Renderer.Contains("GL_SHADER_STORAGE_BUFFER") -and $weather06Renderer.Contains("GL_COMMAND_BARRIER_BIT") -and $weather06Renderer.Contains("atomicAdd(drawCommand.instanceCount") -and $weather06Renderer.Contains("glDrawArraysIndirect")
Check $weather07bModernGpuRain "WEATHER07C1 high-density rain uses OpenGL compute, GPU compaction and indirect drawing without per-drop CPU loops or visible-count readback"
Check (
    $weather06Renderer.Contains('WEATHER09A: fixed-function depth remains disabled THROUGH the indirect') -and
    $weather06Renderer.Contains('glDrawArraysIndirect(GL_TRIANGLES, nullptr);') -and
    -not $weather06Renderer.Contains('glDepthFunc(GL_GREATER);')
) "WEATHER09A airborne rain remains depth-test disabled through the indirect draw on the reversed-Z MSAA path"
Check ($weather06Renderer.Contains("kRainGpuNearCandidates = 10000u") -and $weather06Renderer.Contains("kRainGpuMidCandidates = 100000u") -and $weather06Renderer.Contains("kRainGpuFarCandidates = 10000u") -and $weather06Renderer.Contains("10, 8, 10") -and $weather06Renderer.Contains("0.50, 1.25") -and $weather06Renderer.Contains("0.0f, 2.0f") -and $weather06Renderer.Contains("2.0f, 10.0f") -and $weather06Renderer.Contains("10.0f, 100.0f") -and $weather06Renderer.Contains("uTierSalt") -and -not $weather06Renderer.Contains("diagnosticDisableRainCulling = true")) "WEATHER07C6 authored rain LOD uses 10k at 0-2m, 100k at 2-10m and 10k at 10-100m"
Check ($weather06Renderer.Contains("constexpr float corners[]") -and $weather06Renderer.Contains("0.0f, 0.0f") -and $weather06Renderer.Contains("glDrawArraysIndirect(GL_TRIANGLES")) "WEATHER07C1 textured rain uses one UV-unwrapped triangle per compacted representative"
Check (-not $weather06Renderer.Contains("sampleRainRepresentative(gx, gy, gz")) "WEATHER07B7 removes executable per-drop CPU precipitation sampling from normal rain presentation"
Check (-not $weather06Renderer.Contains("m_rainVolumeProgram") -and $weather06Renderer.Contains("The module-authored rain texture owns the complete 0-100 m presentation")) "WEATHER07C6 authored textured rain is the sole airborne presentation path without a procedural fullscreen curtain layered over it"
Check ($job01Overlay.Contains("optical rain %s  material %dx%d")) "WEATHER07B F8 exposes rain optical-material readiness and dimensions"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-092-Textured-Optical-Rain-Material.md")) "WEATHER07B textured optical rain decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER07B_TexturedOpticalRain.txt")) "WEATHER07B milestone report is present"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-094-GPU-Compute-Precipitation-Presentation.md")) "WEATHER07B7 GPU-compute precipitation decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER07B7_GpuComputeRain.txt")) "WEATHER07B7 GPU-compute rain report is present"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-095-Compacted-Rain-LOD-And-Indirect-Draw.md")) "WEATHER07C1 compacted rain LOD decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER07C1_CompactedRainLOD.txt")) "WEATHER07C1 compacted rain LOD report is present"
