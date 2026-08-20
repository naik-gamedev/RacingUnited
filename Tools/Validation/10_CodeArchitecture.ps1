# CLEAN12 validation module. Dot-sourced by Tools/ValidateProject.ps1.
# It intentionally shares the caller scope so existing checks keep the same
# variables and Check()/ReadText() helpers while ownership is physically split.

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
    "EntityMeshRenderMath.cpp"
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
Check ($clean05RootRenderer.Split("`n").Count -lt 1200) "CLEAN05 root EntityMeshRenderer.cpp stays below orchestrator size guard"
Check ($clean05AssetCache.Contains("EntityMeshRenderer::acquireMesh(") -and $clean05AssetCache.Contains("EntityMeshRenderer::resolveAsset(") -and $clean05AssetCache.Contains("EntityMeshRenderer::dependenciesChanged(")) "CLEAN05 asset/cache unit owns mesh loading and hot-reload dependency checks"
Check ($clean05Animation.Contains("EntityMeshRenderer::animationTransformsForInstance(") -and $clean05Animation.Contains("applyMeshNodeOverrides(") -and $clean05Animation.Contains("buildSkinPalette(")) "CLEAN05 animation unit owns clip/node/skin evaluation"
Check ($clean05Shadows.Contains("EntityMeshRenderer::initializeShadowResources(") -and $clean05Shadows.Contains("EntityMeshRenderer::buildShadowCascades(") -and $clean05Shadows.Contains("EntityMeshRenderer::drawShadowMaps(")) "CLEAN05 shadow unit owns cascaded shadow resources and pass"
Check ($clean05Shaders.Contains("kVertexShader") -and $clean05Shaders.Contains("kFragmentShader") -and $clean05Shaders.Contains("kShadowVertexShader") -and $clean05Shaders.Contains("kShadowFragmentShader") -and -not $clean05RootRenderer.Contains('R"glsl(')) "CLEAN05 embedded GLSL has one explicit header owner"
Check ($clean05ShadowConfig.Contains("kMediumResolution = 2048") -and $clean05ShadowConfig.Contains("kHighResolution = 3072") -and $clean05ShadowConfig.Contains("kUltraResolution = 4096") -and $clean05ShadowConfig.Contains("kDefaultQuality = Quality::Ultra")) "SHADOW03 shadow quality presets centralize Low/Medium/High/Ultra with 4096 Ultra default"
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
Check ($shadow03VideoSettings.Contains("shadowQualityIndex = 3") -and $shadow03VideoSettings.Contains("shadowFilterIndex = 2") -and $shadow03VideoSettings.Contains('"Poisson PCF"') -and $shadow03VideoSettings.Contains('"PCSS + Poisson"') -and $shadow03VideoStorage.Contains('shadowQualityIndex=') -and $shadow03VideoStorage.Contains('shadowFilterIndex=')) "SHADOW04 persists Ultra/4096 and PCSS+Poisson shadow defaults in per-module VideoSettings"
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


$retiredVehicleConfigurationPath = Join-Path $Root "Engine\HeritageEngine\Vehicles\VehicleConfiguration.cpp"
$retiredVehicleConfiguration = if (Test-Path $retiredVehicleConfigurationPath) { [IO.File]::ReadAllText($retiredVehicleConfigurationPath) } else { "" }
Check ($retiredVehicleConfiguration.Contains("retired umbrella translation unit") -and -not $retiredVehicleConfiguration.Contains("VehicleSystem::set")) "CLEAN03A root VehicleConfiguration.cpp is a non-implementation signpost"
Check ($vehicleHeader.Contains("std::vector<WheelRecord> wheels")) "common native vehicle storage remains arbitrary-wheel-count"
$topologyScaffolds = @(
    "Engine\HeritageEngine\Vehicles\Topology\Common\VehicleTopologyCoordinator.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\TwoWheel\TwoWheelVehicleDynamics.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\ThreeWheel\ThreeWheelVehicleDynamics.cpp",
    "Engine\HeritageEngine\Vehicles\Topology\FourPlusWheel\FourPlusWheelVehicleDynamics.cpp",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\Common.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\TwoWheel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\ThreeWheel.lua",
    "Modules\RacingUnited\Scripts\Vehicles\Topology\FourPlusWheel.lua"
)
foreach ($relativePath in $topologyScaffolds) {
    Check (Test-Path (Join-Path $Root $relativePath)) "CLEAN03A vehicle-topology scaffold exists: $relativePath"
}
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
$job01OverlayPath = Join-Path $Root "Engine\HeritageEngine\Core\Diagnostics\PerformanceOverlay.cpp"
$job01EngineProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$job01TestProjectPath = Join-Path $Root "Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
$job01Header = ReadText $job01HeaderPath
$job01Source = ReadText $job01SourcePath
$job01RuntimeState = ReadText $job01RuntimeStatePath
$job01Hydrology = ReadText $job01HydrologyPath
$job01HydrologyHeader = ReadText $job01HydrologyHeaderPath
$job01Overlay = ReadText $job01OverlayPath
$job01EngineProject = ReadText $job01EngineProjectPath
$job01TestProject = ReadText $job01TestProjectPath
Check ((Test-Path $job01HeaderPath) -and (Test-Path $job01SourcePath) -and $job01Header.Contains("class JobSystem final") -and $job01Header.Contains("parallelFor(")) "JOB01 shared Core/Jobs worker-pool contract exists"
Check ($job01Source.Contains("std::thread::hardware_concurrency()") -and $job01Source.Contains("m_workers.emplace_back") -and $job01Source.Contains("executeRanges(batch, true)") -and $job01Source.Contains("executingOnThisSystem()")) "JOB01 scheduler uses persistent hardware-aware workers, caller participation and nested-call safety"
Check ($job01RuntimeState.Contains("heritage::jobs::JobSystem jobs;") -and $job01RuntimeState.Contains("physics.setJobSystem(&jobs)")) "JOB01 EngineRuntimeState owns the process-wide scheduler and wires physics to it"
Check ($job01Hydrology.Contains("m_jobSystem->parallelFor") -and $job01HydrologyHeader.Contains("m_dueDeltaTimeByCell") -and -not $job01HydrologyHeader.Contains("m_flowColorBuckets")) "WATER14 hydrology keeps shared JobSystem parallel work without the retired fixed-grid 27-colour neighbour scheduler"
$water14VirtualPipePath = Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\Water\VirtualPipeFlow.hpp"
$water14VirtualPipe = ReadText $water14VirtualPipePath
Check ((Test-Path $water14VirtualPipePath) -and $water14VirtualPipe.Contains("AdaptiveVirtualPipeFluxInput") -and $water14VirtualPipe.Contains("integrateAdaptiveVirtualPipeFlux") -and $water14VirtualPipe.Contains("edgeLengthM") -and $water14VirtualPipe.Contains("centerDistanceM") -and -not $water14VirtualPipe.Contains("integrateVirtualPipeFlux") -and -not $water14VirtualPipe.Contains("kVirtualPipeNeighbourSlots")) "WATER14 variable-face signed virtual-pipe kernel replaces the retired fixed N/E/S/W 0.5m pipe helper"
Check ($job01HydrologyHeader.Contains("adaptiveMinimumCellSizeM = 0.10") -and $job01HydrologyHeader.Contains("adaptiveMaximumCellSizeM = 20.0") -and $job01HydrologyHeader.Contains("struct AdaptiveCell") -and $job01HydrologyHeader.Contains("double waterVolumeM3") -and $job01HydrologyHeader.Contains("struct AdaptivePipe") -and $job01HydrologyHeader.Contains("edgeLengthM") -and $job01HydrologyHeader.Contains("centerDistanceM")) "WATER14 authoritative hydrology owns conserved water volume in adaptive 0.10m-to-20m control volumes connected by variable-size faces"
Check ($job01Hydrology.Contains("rebuildAdaptiveSimulationTopology()") -and $job01Hydrology.Contains("kSupportSpanCandidates") -and $job01Hydrology.Contains("40, 32, 16, 8, 4, 2") -and $job01Hydrology.Contains("aggressiveSlopeNormalY") -and $job01Hydrology.Contains("aggressiveNormalBreakCosine") -and $job01HydrologyHeader.Contains("adaptiveMinimumCellSlopeDegrees = 55.0") -and $job01HydrologyHeader.Contains("adaptiveMinimumCellNormalBreakDegrees = 30.0") -and $job01Hydrology.Contains("Surface-fit error is still part of coarse-cell merge")) "WATER14A/J 0.10m authoritative cells remain reserved for aggressive angles while coarse packing uses restricted hierarchy spans"
Check ($job01Hydrology.Contains("struct SupportPlaneFit") -and $job01Hydrology.Contains("fitSupportPlane") -and $job01Hydrology.Contains("maximumResidualM") -and $job01Hydrology.Contains("do not require a global span-aligned origin") -and $job01HydrologyHeader.Contains("adaptiveNormalErrorDegrees = 10.0")) "WATER14F coarse adaptive hydrology uses best-fit-plane residuals and unaligned greedy packing so broad planar roads/parking lots stay coarse beside local curbs"
Check ($job01Hydrology.Contains("angularFineSupport") -and $job01Hydrology.Contains("fineBoundaryMaskBySupport") -and $job01Hydrology.Contains("fineDetailDistance") -and $job01Hydrology.Contains("kMaximumGradingDistanceSupports = 32u") -and $job01Hydrology.Contains("kCardinalDirections") -and $job01Hydrology.Contains("maximumSpanNearFineDetail") -and $job01Hydrology.Contains("supportSpan > maximumSpanNearFineDetail") -and $job01Hydrology.Contains("explicit 2:1 balancing pass") -and $job01Hydrology.Contains("cell.cellSizeM > neighbour.cellSizeM * 2.0")) "WATER14J adaptive hydrology uses widening feature-distance bands and explicit shared-face 2:1 balancing"
Check ($job01Hydrology.Contains("fineEdgeSurfaceBreakM") -and $job01Hydrology.Contains("expectedContinuousDeltaY") -and $job01Hydrology.Contains("unexplainedStepM") -and $job01Hydrology.Contains("positiveDirections") -and $job01Hydrology.Contains("FineBoundaryRight") -and $job01Hydrology.Contains("FineBoundaryTop") -and $job01Hydrology.Contains("a detected curb/step is a hard topology boundary")) "WATER14I traces continuous curb/sidewalk height discontinuities into directional edge masks without refining ordinary continuous slopes or allowing coarse cells to cross the step"
Check ($job01Hydrology.Contains("rebuildAdaptivePipes()") -and $job01Hydrology.Contains("overlapM") -and $job01Hydrology.Contains("pipe.sillElevationM") -and $job01Hydrology.Contains("detail::integrateAdaptiveVirtualPipeFlux") -and -not $job01HydrologyHeader.Contains("virtualPipeOutflowM3ps") -and -not $job01HydrologyHeader.Contains("virtualPipeSillOffsetM") -and -not $job01HydrologyHeader.Contains("std::array<std::int32_t, 8> neighbours")) "WATER14 adaptive cells use one signed persistent pipe per shared face and fixed-grid per-support-cell pipe/neighbour state is gone"
Check ($job01Hydrology.Contains("rainVolume") -and $job01Hydrology.Contains("cell.areaM2") -and $job01Hydrology.Contains("cell.waterVolumeM3 += rainVolume") -and $job01Hydrology.Contains("m_outflowScaleByCell") -and $job01Hydrology.Contains("transferredVolumeM3")) "WATER14 rain, loss and transport operate on physical cell area/volume with conservative adaptive-cell outflow limiting"
Check ($job01HydrologyHeader.Contains("supportCellCount") -and $job01Hydrology.Contains("m_stats.supportCellCount = m_cells.size()") -and $job01Hydrology.Contains("m_stats.cellCount = m_adaptiveCells.size()") -and $job01HydrologyHeader.Contains("Immutable support raster")) "WATER14 clearly separates immutable 0.5m terrain support samples from authoritative adaptive simulation cells"
Check ((Test-Path (Join-Path $Root "Docs\WATER14_ADAPTIVE_CONTROL_VOLUME_HYDROLOGY.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-113-Adaptive-Control-Volume-Water-Authority.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14_AdaptiveControlVolumeHydrology.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14A_AGGRESSIVE_ANGLE_MINIMUM_CELL_GATE.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-114-Aggressive-Angle-Minimum-Hydrology-Cell-Gate.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14A_AggressiveAngleMinimumCellGate.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14B_SHARED_BOUNDARY_WATER_STITCHING.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-115-Shared-Boundary-Adaptive-Water-Stitching.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14B_SharedBoundaryWaterStitching.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14C_3CM_COLLIDER_NORMAL_WATER_OFFSET.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14C_3cmColliderNormalWaterOffset.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14D_DISTANCE_ADAPTIVE_COLLIDER_NORMAL_OFFSET.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14D_DistanceAdaptiveColliderNormalOffset.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14E_WIDER_WELD_TRIANGLE_COVERAGE.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14E_WiderWeldTriangleCoverage.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14F_PLANAR_COARSENING.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-116-Planarity-Driven-Adaptive-Hydrology-Coarsening.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14F_PlanarCoarsening.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14G_GRADED_FINE_TRANSITIONS.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-117-Graded-Fine-Water-Transitions.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14G_GradedFineTransitions.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14H_CONTINUOUS_CURB_EDGE_REFINEMENT.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-118-Continuous-Curb-Edge-Water-Refinement.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14H_ContinuousCurbEdgeRefinement.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14I_EDGE_ONLY_ADAPTIVE_SUBDIVISION.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-119-Edge-Only-Adaptive-Water-Subdivision.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14I_EdgeOnlyAdaptiveSubdivision.txt")) -and (Test-Path (Join-Path $Root "Docs\WATER14J_BALANCED_FEATURE_QUADTREE.md")) -and (Test-Path (Join-Path $Root "Docs\Decisions\ADR-120-Balanced-Feature-Quadtree-Water-Topology.md")) -and (Test-Path (Join-Path $Root "Build\Reports\WATER14J_BalancedFeatureQuadtree.txt"))) "WATER14 through WATER14J adaptive hydrology, stitching, offsets, coverage and balanced feature topology are documented"
Check ($job01Overlay.Contains('ImGui::Text("JOB SYSTEM")') -and $job01Overlay.Contains("workerRangeCount") -and $job01Overlay.Contains("callerRangeCount")) "JOB01 F8 performance overlay exposes scheduler activity"
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
Check ($job03SurfaceWorldTest.Contains("adaptiveFlatStats.supportCellCount <= adaptiveFlatStats.cellCount") -and $job03SurfaceWorldTest.Contains("adaptiveMaximumCellSizeM < 8.0") -and $job03SurfaceWorldTest.Contains("largestAdaptiveSimulationCellM") -and $job03SurfaceWorldTest.Contains("materialBoundaryStats.adaptiveSubDecimetreCellCount != 0u") -and $job03SurfaceWorldTest.Contains("aggressiveSlopeStats.adaptiveSubDecimetreCellCount == 0u")) "WATER14A native regression proves flat terrain coarsens, material boundaries do not trigger 0.10m cells, and aggressive ~60deg support does"
Check ($job03SurfaceWorldTest.Contains("planarSlopeStats.adaptiveMaximumCellSizeM < 7.9") -and $job03SurfaceWorldTest.Contains("curbLocalStats.adaptiveMaximumCellSizeM < 1.9") -and $job03SurfaceWorldTest.Contains("adaptiveMinimumCellSizeM < 0.4999") -and $job03SurfaceWorldTest.Contains("curbBoundaryCellCount") -and $job03SurfaceWorldTest.Contains("curbUnexpectedTinyCellCount != 0u") -and $job03SurfaceWorldTest.Contains("WATER14J restricted-quadtree regression") -and $job03SurfaceWorldTest.Contains("larger > smaller * 2.0")) "WATER14J native regression keeps curb detail edge-only and proves actual 0.50m+ shared faces obey the 2:1 hierarchy"
Check ($job03SurfaceWorldTest.Contains("WATER15 presentation regression boundary") -and -not $job03SurfaceWorldTest.Contains("buildStitchedAdaptiveWaterSurface(") -and -not $job03SurfaceWorldTest.Contains("water_stitch")) "WATER15 native regression no longer couples authoritative hydrology tests to retired renderer tessellation"
Check ($job03SurfaceWorldTest.Contains("0.0080") -and $job03SurfaceWorldTest.Contains("adaptiveWaterVolumes.empty()") -and $job01Hydrology.Contains("if (!includeDryCells && depth <= visibleThreshold)")) "WATER15 preserves hydrology query thresholds only as collection behavior; presentation ownership no longer hands off to a 3D water mesh"
$perf19EntityMeshPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$perf19EntityMeshHeaderPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.hpp"
$perf19EntityMeshShadersPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshShaders.hpp"
$perf19WetnessAtlasPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshSurfaceWetness.cpp"
$perf19EntityMesh = ReadText $perf19EntityMeshPath
$perf19EntityMeshHeader = ReadText $perf19EntityMeshHeaderPath
$perf19EntityMeshShaders = ReadText $perf19EntityMeshShadersPath
$perf19WetnessAtlas = ReadText $perf19WetnessAtlasPath
$dsurfHydrologyAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceHydrology.cpp")
$dsurfThermalAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceThermal.cpp")
$dsurfTypes = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceTypes.hpp")
$dsurfPagePoolHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfacePagePool.hpp")
$dsurfSurfaceWorld = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\SurfaceWorld.cpp")
$dsurfSystem = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceSystem.cpp")
$dsurfChunkAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Physics\Surfaces\DynamicSurface\DynamicSurfaceChunk.cpp")
$dsurfGpuWaterAuthority = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuLodPrototype.cpp")
$dsurfGpuWaterAuthorityHeader = ReadText (Join-Path $Root "Engine\HeritageEngine\Graphics\DynamicSurface\DynamicSurfaceGpuLodPrototype.hpp")
# LIVETRACK07: detailed Hydro is presented inside 100m. The bounded atlas keeps
# a small exact recent-tile history without allocating duplicate all-scene state.
Check (
    $dsurfGpuWaterAuthorityHeader.Contains('kTileWorldSizeM = 10.0f') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kTileResolution = 256u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kSimulationRadiusM = 100.0f') -and
    $dsurfGpuWaterAuthority.Contains('if (distanceM <= kSimulationRadiusM) return 2.0f;') -and
    $dsurfGpuWaterAuthority.Contains('return 0.0f;')
) "LIVETRACK07 keeps 256x256/10m detailed Hydro inside one synchronized 100m/2Hz cohort"
Check (
    -not $dsurfGpuWaterAuthority.Contains('    dispatchWorldTileSimulation(elapsedSeconds, precipitationRateMmPerHour,') -and
    -not $dsurfGpuWaterAuthority.Contains('    applyWorldTireEvents(tireEvents);') -and
    $dsurfGpuWaterAuthority.Contains('if (prewarm)') -and
    $dsurfGpuWaterAuthority.Contains('return 1.0f / 60.0f;') -and
    $dsurfGpuWaterAuthority.Contains('up to 43 history')
) "LIVETRACK07 retains bounded exact recent-tile Hydro history without duplicate all-scene state"
Check (
    $dsurfGpuWaterAuthorityHeader.Contains('kAtlasColumns = 20u') -and
    $dsurfGpuWaterAuthority.Contains('const int kDsurfAtlasColumns = 20;') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kMaximumBatchTiles = 384u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kMaximumResidentTiles = 357u') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kMaximumTileUpdatesPerFrame = kMaximumResidentTiles') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kMaximumNewTileInitializationsPerFrame =') -and
    $dsurfGpuWaterAuthorityHeader.Contains('kMaximumResidentTiles;') -and
    $dsurfGpuWaterAuthority.Contains('if (distanceM <= kSimulationRadiusM) return 2.0f;') -and
    $dsurfGpuWaterAuthority.Contains('return distanceM <= kSimulationRadiusM ? 0u : 3u;') -and
    $dsurfGpuWaterAuthority.Contains('static_cast<GLuint>(tileCount)') -and
    $dsurfGpuWaterAuthority.Contains('m_stats.dispatchesThisFrame += 2u') -and
    $dsurfGpuWaterAuthority.Contains('due.size() - cohort.size()')
) "LIVETRACK07 advances the complete 100m field as one synchronized 2Hz cohort and submits it in two batched dispatches"
Check (
    $dsurfGpuWaterAuthority.Contains('layout(rgba8, binding = 1) writeonly uniform image2DArray uDestinationBatchScratch;') -and
    $dsurfGpuWaterAuthority.Contains('dispatchWaterBatch(cohort, elapsedSeconds') -and
    $dsurfGpuWaterAuthority.Contains('allocateState(m_water, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE')
) "LIVETRACK06 keeps detailed water evolution and authoritative storage on GPU"
Check (
    $dsurfGpuWaterAuthority.Contains('depth = max(depth, 0.0001);') -and
    $dsurfGpuWaterAuthority.Contains('encodeWaterStochastic(depth, worldCell, uTickIndex)') -and
    $perf19EntityMeshShaders.Contains('dynamicSurfaceFilm = clamp(uSurfaceWeatherFilmWetness, 0.0, 1.0)') -and
    $perf19EntityMeshShaders.Contains('smoothstep(0.00002, 0.00050, dynamicSurfaceDepthM)')
) "LIVETRACK04B restores the proven visible rain film while local Hydro adds puddles and dry-line state"
Check (
    $dsurfGpuWaterAuthority.Contains('q.x < 7 ? -float(7 - q.x) / 7.0') -and
    $dsurfGpuWaterAuthority.Contains('unresolvedRoadReliefM(globalXZ)') -and
    $dsurfGpuWaterAuthority.Contains('vec2(1.0 / 3.0)') -and
    $dsurfGpuWaterAuthority.Contains('float hydraulicHeadDifferenceM = leftDepth - rightDepth + groundDropLeftToRightM;') -and
    $dsurfGpuWaterAuthority.Contains('const float kHeadDeadBandM = 0.000035;') -and
    $dsurfGpuWaterAuthority.Contains('float maximumLeftToRightM = leftDepth * 0.20;') -and
    -not $dsurfGpuWaterAuthority.Contains('float fraction = clamp(abs(signedFlow) * dt * 0.75, 0.0, 0.18);')
) "LIVETRACK08 uses conservative hydraulic-head puddle flow, shallow-slope companding and deterministic sub-grid road relief"
Check (
    $dsurfGpuWaterAuthority.Contains('single Hydro field uses the highest authored receiver') -and
    $dsurfGpuWaterAuthority.Contains('surfaceSheetId = 0u;') -and
    -not $dsurfGpuWaterAuthority.Contains('triangleSheet < bestSheet')
) "LIVETRACK06 keeps one X/Z water field per 10m tile with vertical Hydro sheets disabled"
Check (
    $dsurfSurfaceWorld.Contains('if (!m_gpuDynamicSurfaceAuthorityEnabled)') -and
    $dsurfSurfaceWorld.Contains('m_dynamicSurface.advanceHydro(') -and
    $dsurfSurfaceWorld.Contains('if (m_gpuDynamicSurfaceAuthorityEnabled)') -and
    $dsurfSurfaceWorld.Contains('m_gpuDynamicSurfaceTireEvents')
) "LIVETRACK06 disables CPU per-texel Hydro advancement and routes nearby tire-water interaction to GPU events"
Check (
    $perf19EntityMeshShaders.Contains('vec4 gpuWaterFilteredSingleSample(vec3 positionRelative, out bool valid)') -and
    $perf19EntityMeshShaders.Contains('vec2 blurHalfOffsetM = vec2(0.5 * texelSizeM);') -and
    $perf19EntityMeshShaders.Contains('vec4 filtered = 0.25 * (s00 + s10 + s01 + s11);') -and
    -not $perf19EntityMeshShaders.Contains('if (distanceM <= 50.0)') -and
    $perf19EntityMeshShaders.Contains('GpuWaterDecoded decoded = decodeGpuWater(filtered);') -and
    $perf19EntityMeshShaders.Contains('lodDetail = 1.0 - smoothstep(0.0, 100.0, distanceM);') -and
    -not $perf19EntityMeshShaders.Contains('valid = distanceM <= 100.0')
) "LIVETRACK04B reconstructs rain/puddles with four coherent taps throughout the continuous 100m optical fade"
Check (
    -not $perf19EntityMesh.Contains('drawWetFilmPass(') -and
    -not $perf19WetnessAtlas.Contains('glDrawArrays(GL_TRIANGLES, 0, 3)')
) "LIVETRACK06 keeps water optics in the ordinary authored material draw with no duplicate puddle mesh"
Check (
    $job01Overlay.Contains('LIVETRACK07 GPU Hydro 10m/256x256 <=100m + BOUNDED HISTORY') -and
    $job01Overlay.Contains('bounded 20x20 atlas + 384-layer scratch') -and
    $job01Overlay.Contains('up to 43 recent tiles retain exact state at 1/min') -and
    $job01Overlay.Contains('per-tile dispatch/copy/barrier OFF')
) "LIVETRACK07 F8 exposes the bounded 100m simulation and recent-history storage contract"
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
$weather06EntityPath = Join-Path $Root "Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
$weather06EnvironmentPath = Join-Path $Root "Engine\HeritageEngine\Graphics\EnvironmentMap.cpp"
$weather06ProjectPath = Join-Path $Root "Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
$weather06Renderer = ReadText $weather06RendererPath
$weather06RendererHeader = ReadText $weather06RendererHeaderPath
$weather06Sky = ReadText $weather06SkyPath
$weather06Entity = ReadText $weather06EntityPath
$weather06Environment = ReadText $weather06EnvironmentPath
$weather06Project = ReadText $weather06ProjectPath
Check ((Test-Path $weather06RendererPath) -and (Test-Path $weather06RendererHeaderPath) -and $weather06Project.Contains('Graphics\Renderer\WeatherPresentationRenderer.cpp') -and $weather06Project.Contains('Graphics\Renderer\WeatherPresentationRenderer.hpp')) "WEATHER06A modular weather presentation renderer is compiled by the engine"
Check (($weather06Renderer.Contains("kRainGridX = 32") -and $weather06Renderer.Contains("glDrawArraysInstanced")) -or ($weather06Renderer.Contains("kRainComputeShader") -and $weather06Renderer.Contains("glDrawArraysIndirect") -and $weather06Renderer.Contains("absoluteCell = uBaseCell + localCell"))) "WEATHER06A+ falling rain remains a bounded world-space GPU population rather than CPU particles"
Check ($weather06Sky.Contains("viewportWidth / 3") -and $weather06Sky.Contains("kSteps = 10") -and $weather06Sky.Contains("cloudDensity") -and $weather06Sky.Contains("windOffset")) "WEATHER06A volumetric clouds use bounded one-third-resolution world-space integration"
Check ($weather06Entity.Contains("applyWeatherLighting") -and $weather06Entity.Contains("weatherFogDensity") -and -not $weather06Entity.Contains("kRainVertexShader")) "WEATHER06A entity renderer only consumes storm lighting/fog and does not own rain particles"
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
# DSURF03 supersedes the WATER15-18 presentation collectors. The smooth
# weather film remains available as a fallback while authoritative hydrology
# depth/moisture is migrated into persistent Heritage Dynamic Surface pages.
$dsurf03SurfaceStateWetFilm = (
    $perf19WetnessAtlas.Contains("updateDynamicSurfaceStatePages(") -and
    $perf19WetnessAtlas.Contains("dynamicSurface.rasterHydroPage(") -and
    $perf19WetnessAtlas.Contains("weather.waterFilmDepthM") -and
    $perf19WetnessAtlas.Contains("weather.effectiveWetness") -and
    $perf19EntityMeshShaders.Contains("uSurfaceWeatherFilmWetness") -and
    $perf19EntityMeshShaders.Contains("uSurfaceWeatherFilmDepthM") -and
    $perf19EntityMeshShaders.Contains("uDynamicSurfaceHydroPages") -and
    $perf19EntityMeshShaders.Contains("applyDynamicSurfaceWater") -and
    -not $perf19EntityMesh.Contains("drawWetFilmPass(") -and
    -not $perf19WetnessAtlas.Contains("for (int neighbor") -and
    -not $perf19EntityMeshShaders.Contains("for (int neighbor")
)
Check ($weather06LegacyHydrologyWetFilm -or $dsurf03SurfaceStateWetFilm) "WEATHER06A+ wet-surface presentation uses smooth weather film plus persistent Dynamic Surface hydrology without expensive rain-neighbor fragment loops"
Check (Test-Path (Join-Path $Root "Docs\Decisions\ADR-079-Integrated-OpenGL-Weather-Presentation.md")) "WEATHER06A OpenGL weather presentation decision is documented"
Check (Test-Path (Join-Path $Root "Build\Reports\WEATHER06A_IntegratedRainClouds.txt")) "WEATHER06A milestone report is present"

# WEATHER06D: the live WEATHER06C fullscreen streak veil visibly travelled with
# the camera. Keep rain existence/trajectory in world cells and derive direct-
# precipitation cover from the already-baked layered hydrology surface field.
Check (-not $weather06Renderer.Contains("kRainOverlayVertexShader") -and -not $weather06Renderer.Contains("kRainOverlayFragmentShader")) "WEATHER06D removes camera-attached fullscreen streak rain from the live renderer"
Check ($weather06Renderer.Contains("hasPrecipitationCoverAbove(") -and $job01Hydrology.Contains("bool SurfaceHydrology::hasPrecipitationCoverAbove(") -and -not $weather06Renderer.Contains("raycast")) "WEATHER06H precipitation-cover diagnostics use an exact hydrology-column query instead of per-drop CPU raycasts"
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
$weather07SurfacePanelPath = Join-Path $Root "Modules\RacingUnited\Scripts\UI\Vehicle\SurfacesPanel.lua"
$weather07RegressionPath = Join-Path $Root "Engine\HeritageEngine\Tests\WeatherRegression.cpp"
$weather07Microphysics = ReadText $weather07MicrophysicsPath
$weather07Field = ReadText $weather07FieldPath
$weather07FieldHeader = ReadText $weather07FieldHeaderPath
$weather07SurfaceWorldHeader = ReadText $weather07SurfaceWorldHeaderPath
$weather07SurfaceWeatherHeader = ReadText $weather07SurfaceWeatherHeaderPath
$weather07LuaWeather = ReadText $weather07LuaWeatherPath
$weather07SurfacePanel = ReadText $weather07SurfacePanelPath
$weather07Regression = ReadText $weather07RegressionPath
Check ((Test-Path $weather07MicrophysicsHeaderPath) -and (Test-Path $weather07FieldHeaderPath) -and $weather06Project.Contains('Physics\Weather\RainMicrophysics.cpp') -and $weather06Project.Contains('Physics\Weather\PrecipitationField.cpp')) "WEATHER07A physical rain microphysics and precipitation field are compiled as reusable engine subsystems"
Check ($weather07Microphysics.Contains("kMarshallPalmerLambdaCoefficient = 4.1") -and $weather07Microphysics.Contains("kMarshallPalmerLambdaRainExponent = -0.21") -and $weather07Microphysics.Contains("9.65 - 10.3 * std::exp(-0.6 * dMm)")) "WEATHER07A rain population uses Marshall-Palmer size structure and bounded Atlas terminal velocity"
Check ($weather07Microphysics.Contains("requestedVolumeFluxMps") -and $weather07Microphysics.Contains("populationScale = requestedVolumeFluxMps / baseVolumeFlux") -and $weather07Microphysics.Contains("massFluxKgPerM2PerSecond")) "WEATHER07A statistical drop population is mass-normalized to authoritative mm/h rainfall"
Check ($weather07FieldHeader.Contains("class PrecipitationField") -and $weather07Field.Contains("hashCell(") -and $weather07Field.Contains("sampleRainRepresentative") -and $weather07Field.Contains("m_elapsedSeconds * drop.terminalVelocityMps")) "WEATHER07A precipitation representatives are deterministic world-cell trajectories rather than camera-owned particles"
Check ($weather07SurfaceWorldHeader.Contains("weather::PrecipitationField m_precipitation") -and $weather07SurfaceWorldHeader.Contains("const weather::PrecipitationField& precipitation() const")) "WEATHER07A SurfaceWorld exposes one shared physical precipitation field to all views"
Check ($weather07SurfaceWeatherHeader.Contains("windDirectionDegrees") -and $weather07LuaWeather.Contains('"wind_direction_deg"') -and $weather07SurfacePanel.Contains('"Wind direction"') -and $weather06Sky.Contains("uWindVelocityXZ") -and -not $weather06Sky.Contains("vec2(0.72, 0.69)")) "WEATHER07A weather has explicit world wind heading shared by precipitation and cloud advection rather than a hard-coded renderer direction"
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
