@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "SOLUTION=%ROOT%\Engine\HeritageEngine\HeritageEngine.slnx"
rem Both projects use the solution-level OutDir under Engine\HeritageEngine.
rem Do not point back into the individual project folders: those contain stale
rem historical binaries and caused new Lua to run against an old native API.
set "ENGINE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "TEST_EXE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritagePhysicsTests.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "BUILD_LOG=%REPORTS%\CurrentBuild.log"
set "TEST_LOG=%DIAGNOSTICS%\physics_regression_current.txt"
set "CODE_HEALTH_REPORT=%REPORTS%\CodeHealthSnapshot.txt"
set "RUNTIME_CAPTURE=%ROOT%\Tools\Diagnostics\LaunchEngineCaptured.ps1"
set "RUNTIME_LOG=%DIAGNOSTICS%\RuntimeConsoleLatest.log"
set "RUNTIME_CRASH=%DIAGNOSTICS%\RuntimeCrashLatest.txt"
set "MILESTONE=SUSP13_SEMI_TRAILING_ARM_TWIST_BEAM"
set "MSBUILD_TARGET=Build"
set "BUILD_MODE=incremental"
if /I "%~1"=="full" (
    set "MSBUILD_TARGET=Rebuild"
    set "BUILD_MODE=full rebuild"
)

if not exist "%REPORTS%" mkdir "%REPORTS%"
if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"

cls
echo ============================================================
echo Heritage Engine - CURRENT build + run [%MILESTONE%]
echo CELESTIAL05: material day/night ambient and wet reflections now use TRUE solar elevation, never the blended Sun/Moon key direction; Moon altitude can no longer behave like a second sunrise/sunset
echo CELESTIAL05: PBSKY hands off continuously into twilight/night, stars begin fading in while the sky is still orange, and low-Moon cloud/halo lighting stays neutral instead of sunset-red
echo CELESTIAL05A: CLEAN05 EntityMeshRenderer orchestrator guard restored without weakening the ^<1200-line contract; the new true-solar-elevation upload remains behavior-identical
echo CELESTIAL06: EnricoMonese DayNightCycle control architecture is adapted natively: one normalized solar-cycle scalar now owns sky, stars, atmosphere, weather/fog and material day/night presentation
echo CELESTIAL06: competing raw-Sun twilight/night thresholds are retired; cloud TAA rejects stale resolved lighting history when accelerated time moves through dawn/dusk
echo CELESTIAL07: ordinary CSM/direct-light and the detailed ground cloud cookie now follow one REAL Sun or Moon direction at a time; the old synthetic twilight shadow sweep is retired
echo CELESTIAL07: low-angle tangent cloud-shadow traces fade safely, Sun/Moon ownership changes through a zero-strength twilight bridge, and the detailed cookie is applied once by the dedicated receiver instead of twice
echo CELESTIAL08: cloud-driven direct-Sun attenuation is one tenth of CELESTIAL07 across broad weather, regional material filtering and the detailed ground receiver; cloud opacity/shape are unchanged
echo CELESTIAL08: permanent atmospheric haze is decoupled from binary camera-local cloud cells, regional weather modulation is low-pass filtered, and PBSKY aerosol LUTs follow stable authored scene climate
echo CELESTIAL09: staged environment cubemaps cross-fade old-to-new for dawn/dusk IBL continuity; materials and cloud ambient no longer receive one-frame night-to-morning cubemap swaps
echo CELESTIAL09: night atmospheric extinction remains, but luminous air-light is reduced and the artificial deep-night haze-density bonus is retired; HDR stars gain a restrained 1.22x deep-night visibility lift
echo CELESTIAL10: visible deep-night atmospheric air-light is one tenth of CELESTIAL09 while extinction remains physical; rain/mist air-light obeys the same nocturnal illumination scale
echo CELESTIAL10: PBSKY receives a continuous twilight solar-scattering input independent of ground direct-Sun visibility, and physical-sky authority can no longer blend to an unlit LUT before dawn
echo CELESTIAL11: normal deep night has zero global atmospheric air-light; aerosol/rain extinction remains but fades into darkness instead of a blue-grey haze veil
echo CELESTIAL11: Moonlight no longer creates scene-wide haze; visible nocturnal mist is reserved for a future low-altitude local-light scattering path around headlights, streetlights and floodlights
echo CELESTIAL12: rural night sky is recalibrated darker overall; deep-night zenith/horizon fallback colours are near-black so stars sit against a truly dark background instead of a navy wash
echo CELESTIAL12: the visible Moon and local halo remain, but the global lunar sky/ground lift is reduced sharply so a full Moon no longer brightens the whole dome unrealistically
echo TIRE46: production tire physics is frozen for the suspension phase; Distributed3x3 is the native default for every vehicle and the 150-car benchmark exercises all 600 tires through the bounded 3x3 allocator
echo TIRE46: seven-node construction thermals, full physical damage/endurance, signed PHYP turn-slip and motorcycle crown support geometry are active; historical tire uncertainty is provenance-labelled data work, not missing solver architecture
echo SUSP05: hardpoint-derived MacPherson/trailing-arm wheel-centre scrub is now physical tire-support-query authority; the old straight localMount ray can no longer ignore lateral/longitudinal linkage travel
echo SUSP05: only the linkage motion perpendicular to the suspension axis offsets the 1 kHz support ray, so bump/droop travel remains single-authority in the ray/unsprung solve and is not double-counted
echo SUSP06: double_wishbone_v1 is now a real unequal-length A-arm hardpoint solver; upper/lower arm rotations preserve rigid upright length while satisfying requested wheel-centre travel
echo SUSP06: physical ball-joint steering axis, tie-rod bump steer, camber/caster/KPI migration, SUSP05 contact scrub and geometry-derived direct damper motion ratio are authoritative; pushrod/rocker remains separate
echo SUSP06A: OPT01 retirement convergence no longer deletes the now-live DoubleWishboneKinematics.cpp implementation; SUSP06 validation and build can see the actual compiled provider
echo SUSP07: pushrod_double_wishbone_v1 layers a rigid pushrod + chassis-axis rocker on SUSP06 linkage authority; the bellcrank angle is solved from exact pushrod length instead of a wheel-travel approximation
echo SUSP07: spring and damper use independent rocker pickups with actual shaft compression and separate nonlinear motion ratios; virtual-work force leverage follows the current geometry through travel
echo SUSP08: live_axle_v1 treats paired wheels as one rigid axle member; one shared 1 kHz pair snapshot drives axle roll while Panhard and trailing-link constraints produce real lateral/longitudinal axle path
echo SUSP08: left/right spring and damper shaft compression/motion ratio come from moved axle hardpoints; SUSP05 makes axle scrub physical and same-instant compression snapshots remove wheel-order coupling
echo SUSP09: live_axle_leaf_v1 keeps SUSP08 one-rigid-axle authority while real front-eye/rear-shackle leaf geometry supplies bending compression and nonlinear leverage; interleaf Coulomb+viscous hysteresis is separate from shock damping
echo SUSP09: paired axle-housing torsional state is driven by tire longitudinal reaction torque with rate-stable stiffness/damping and bounded wrap/jacking coupling; VehicleDefinition requires the base axle plus eight leaf/shackle points
echo SUSP10: motorcycle_telescopic_fork_v1 slides the axle on the authored steering-stem axis and rotates it about that same physical axis, so fork travel/steer change the real SUSP05 support path and wheelbase rather than a cosmetic upright only
echo SUSP10: motorcycle_swingarm_linkage_v1 solves rigid swingarm + fixed dogbone + rocker geometry for nonlinear shock leverage; countershaft-to-axle length variation maps real tire drive force into bounded chain anti-squat/jacking by virtual work
echo SUSP11: kart_chassis_flex_v1 has no fictitious wheel springs or independent travel; physical inclined front kingpins preserve steering jacking into the 1 kHz support query while rear wheel centres remain fixed to one rigid axle
echo SUSP11: kart support is pneumatic tire compliance plus chassis_torsional_mode_v1 frame twist; the generic spring/damper path returns zero and tire radial normal force is transmitted directly to the chassis
echo SUSP12: multilink_v1 solves one rigid upright from five fixed-length physical links plus requested wheel travel; passive bump steer/camber/scrub emerge from the constraints instead of authored curves
echo SUSP12: link 5 is the toe/steering link; rack-axis motion produces steerable front multi-link while a fixed rack preserves passive rear bump steer, and separate spring/damper shaft leverage is geometry-derived
echo SUSP13: semi_trailing_arm_v1 rotates one rigid arm about an arbitrary swept pivot axis so camber/toe migration and wheel scrub emerge physically; coil spring and damper have separate geometry-derived leverage
echo SUSP13: twist_beam_v1 pairs two semi-trailing arms through one torsional crossbeam; relative arm angle/rate create equal structural torque with side-specific virtual-work wheel forces instead of a detached anti-roll approximation
echo LIVETRACK22A: tire-mark atlas allocation/reset ownership is kept in DynamicSurfaceGpuResources; the production coordinator is back under the OPT03 ^<500-line split-responsibility safety contract with no runtime behavior change
echo LIVETRACK22: tire marks are now an R8 material state in the SAME 10m / near-256x256 Dynamic Surface tile slots and indirection as production water; FP64 mark history reconstructs returning tiles
echo LIVETRACK22: mark visibility fades continuously across 0-500m; live wet film/puddles suppress contrast, full-wet deposition is reduced, and far vector LOD uses corrected depth bias + 85-110m material handoff
echo TIRE45K: physical MF/contact Fy is distributed as real lower-tread shear force inside the 24x13 carcass solve; cornering side-bend no longer depends on large rigid-ring offsets
echo TIRE45K: physical aligning moment Mz is resolved as an equal/opposed front-rear footprint shear pair, restoring visible tread torsion while preserving zero straight-line lateral forcing
echo TIRE45J: rigid-ring yaw/contact twist remain physically gated/capped auxiliary modes, while real curb-side unilateral contact remains independent
echo PEUGEOT_SUSP01: 206 RC uses live MacPherson-strut front and trailing-arm/transverse-torsion-bar rear providers through an explicit reference-constrained hardpoint profile
echo PEUGEOT_SUSP01: stock midpoint alignment is front 0.00 camber / 0.06 deg toe-out per wheel / 3.20 deg caster and rear -1.00 camber / 0.26 deg toe-in per wheel
echo PEUGEOT_SUSP01: generic low-confidence hardpoint estimates remain authoring-only; Peugeot measured or asset-authored points will supersede its labelled package estimates
echo TIRE45I1: regression now checks flat-road planarity across the authored tread only; intentional outer shoulder/sidewall curvature is no longer misclassified as a failure
echo TIRE45I: flexible-ring rest profile now matches the broad authored GLB tread; pure radial road compression can no longer become a cross-width inward wedge when its displacement field is presented
echo TIRE45H: straight/top-road carcass obeys a zero-lateral symmetry invariant; stale side deformation is cleared unless real rigid lateral load or steep curb-side contact is present
echo TIRE45H: low-speed contact-patch torsion remains tire-force/moment state but no longer rotates production carcass geometry; ordinary road-top radial collapse is bounded by live hub/road overlap + wheel load
echo TIRE45G: unmeasured prototype rigid-ring yaw remains simulated/telemetry-visible but no longer rotates the production carcass field; real lateral force and curb-contact deformation remain active
echo TIRE45C: tire flexible-ring field is sampled/applied in authoritative WORLD wheel basis; GLB mirror/scale/pivot chains cannot rotate deformation components
echo TIRE45C TRACE: final bridge reports native field maxima + ring/upright state while rolling so physics-vs-presentation faults are conclusive
echo TIRE45B: moving-wheel carcass road-cache geometry remains re-anchored to the live 1 kHz centre contact
echo TIRE45: physics-owned 24x13 carcass Megalab exposes 217 live structural parameters, exact scenarios and deterministic brute-force search
echo Road-envelope collision samples and rim/flange boundaries are unilateral constraints inside the structural solve
echo Tire LAB search can evaluate up to 1,000,000 candidates in bounded exact-solver batches; search score never feeds tire forces
echo No tireDeflection-to-vertex target, no support-plane snap, no world-Z flattening and no render-time tire collision solve
echo MF6.2/contact physics remain force authority; carcass deformation consumes the same physical pressure/contact/rigid-ring state
echo Heritage production vehicle/weather audio, fleet LOD, geometry occlusion and shared acoustic reverb remain intact
echo CLOUDURP15EE: occupied-interval microstep integration attacks the remaining stack-of-slices artifact directly inside the raymarch while keeping ED full-resolution 64-step reconstruction
echo CLOUDURP15EE: each occupied march interval is resolved as four shorter substeps; moderate temporal history is strengthened slightly to calm residual shimmer without reverting to cartoon blur
echo CLOUDURP15EE: EC/ED footprint filtering, RGB+coverage temporal authority and the current-frame AABB anti-ghosting clamp remain intact
echo CLOUDURP15EG: the coarse 128x128x128 shape volume is sampled through two fully rotated frames so its ~156m voxel stack cannot appear as horizontal density slices across tall clouds
echo CLOUDURP15EG: vertical cloud-profile LUT resolution is raised 64 to 256 samples; EF de-aliased erosion, EE occupied microsteps, full-resolution reconstruction and RGB+coverage temporal stabilization remain intact
echo CLOUDURP15EH3: packed-RGBA shape experiment is fully retired from runtime after it erased cloud occupancy; renderer is restored to the last known-visible WorleyNoise128R density path
echo CLOUDURP15EH3: no fBm packed-shape remap remains in visible or shadow density; EG de-grid rotation, 256-sample profile LUT and EE 4x occupied microsteps remain intact
echo CLOUDURP15ED1: VCLOUD01 validation now accepts midpoint interval sampling as the production descendant of the original integration-jitter architecture; runtime cloud behavior is unchanged
echo CLOUDURP15EI: source audit against jiaozi158 UnityVolumetricCloudsURP restores the upstream full 0..1 stochastic first-step phase; narrowed EC-EH jitter was exposing the fixed march lattice as horizontal slices
echo CLOUDURP15EI: white hash dither is replaced by a full-range low-discrepancy interleaved-gradient + golden-ratio temporal sequence; 64 real steps remain and TAA is 0.985/0.995/0.999 to converge the dither instead of revealing bands
echo CLOUDURP15EI: EF/EG de-axis density experiments are retired from production sampling; visible/shadow density return to upstream direct trilinear R-channel Worley + Perlin semantics while the reduced 0.34/140 micro erosion and 256-sample profile remain
echo OPT03C4 single GPU-water authority, OPT03B tire-water bridge and byte-compatible OPT02 .hhyd v15 architecture remain intact
echo Native stdout/stderr and Windows crash/minidump capture remain enabled
echo ============================================================
echo Root: %ROOT%
echo Build mode: %BUILD_MODE%  ^(pass FULL for an explicit full rebuild^)
echo.

for %%F in (
    "%SOLUTION%"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Tools\GenerateLuaApiManifest.ps1"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\EnsureIncrementalBuildFreshness.ps1"
    "%ROOT%\Tools\Diagnostics\CodeHealthAudit.ps1"
    "%ROOT%\Tools\Diagnostics\ApplyOPT01Retirement.ps1"
    "%ROOT%\Tools\Diagnostics\ApplyOPT02Retirement.ps1"
    "%ROOT%\Tools\Diagnostics\ApplyOPT03Retirement.ps1"
    "%RUNTIME_CAPTURE%"
) do if not exist "%%~F" (
    echo ERROR: Required build/safety infrastructure is missing:
    echo %%~F
    echo.
    echo Detailed repository requirements are owned by ValidateProject.ps1.
    pause
    exit /b 1
)

rem Overlay ZIP extraction cannot delete obsolete files. Remove known legacy
rem copies before validation so the repository on disk matches the architecture.
for %%F in (
    "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.cpp"
    "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.hpp"
    "%ROOT%\Engine\HeritageEngine\Vehicles\AerodynamicsSystem.cpp"
    "%ROOT%\Engine\HeritageEngine\Vehicles\AeroSurface.cpp"
    "%ROOT%\Engine\HeritageEngine\Vehicles\GroundEffect.cpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.cpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.hpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterContourMesher.hpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterSurfaceStitcher.hpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\EntityMeshShaders.hpp.bak_livetrack01"
    "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.cpp"
    "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.hpp"
    "%ROOT%\Modules\RacingUnited\Scripts\UI\Vehicle\WaterLaboratoryPanel.lua"
    "%ROOT%\Modules\RacingUnited\Scripts\UI\Scene\WaterLaboratoryPanel.lua"
    "%ROOT%\Engine\HeritageEngine\Physics\Surfaces\Water\WaterLaboratory.hpp"
    "%ROOT%\Docs\Water-Laboratory.md"
    "%ROOT%\Tools\WEATHER08A_BuildAndRun.cmd"
) do if exist "%%~F" del /f /q "%%~F" >nul 2>nul

if exist "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\AerodynamicsSystem.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\AeroSurface.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\GroundEffect.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterParcelRenderer.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterContourMesher.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Graphics\Renderer\WaterSurfaceStitcher.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Modules\RacingUnited\Scripts\UI\Vehicle\WaterLaboratoryPanel.lua" goto :legacy_cleanup_failed
if exist "%ROOT%\Modules\RacingUnited\Scripts\UI\Scene\WaterLaboratoryPanel.lua" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Physics\Surfaces\Water\WaterLaboratory.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Docs\Water-Laboratory.md" goto :legacy_cleanup_failed

echo [pre] Converging OPT01 retirement deletions...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\Diagnostics\ApplyOPT01Retirement.ps1" -Root "%ROOT%"
if errorlevel 1 goto :retirement_cleanup_failed
echo.
echo [pre] Converging OPT02 hydrology retirement deletions...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\Diagnostics\ApplyOPT02Retirement.ps1" -Root "%ROOT%"
if errorlevel 1 goto :retirement_cleanup_failed
echo.
echo [pre] Converging OPT03 production-water/runtime retirement deletions...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\Diagnostics\ApplyOPT03Retirement.ps1" -Root "%ROOT%"
if errorlevel 1 goto :retirement_cleanup_failed
echo.
echo [pre] Converging OPT03C static-bake CPU-Hydro retirement...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\Diagnostics\ApplyOPT03C4StaticBakeConvergence.ps1" -Root "%ROOT%"
if errorlevel 1 goto :retirement_cleanup_failed
echo.

echo [0/5] Incremental-build freshness guard...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\EnsureIncrementalBuildFreshness.ps1" -Root "%ROOT%"
if errorlevel 1 goto :freshness_failed

echo.
echo [1/5] Static code-health snapshot...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\Diagnostics\CodeHealthAudit.ps1" -Root "%ROOT%"
if errorlevel 1 goto :audit_failed

echo.
echo [2/5] Static repository validation...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\ValidateProject.ps1" -Root "%ROOT%"
if errorlevel 1 goto :validation_failed

echo.
echo [3/5] Build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "%MILESTONE%"
if errorlevel 1 goto :identity_failed

set "MSBUILD="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do if not defined MSBUILD set "MSBUILD=%%I"
)
if not defined MSBUILD (
    for %%V in (18 2026 2025 2022) do (
        for %%E in (Community Professional Enterprise BuildTools) do (
            if not defined MSBUILD if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe"
            if not defined MSBUILD if exist "C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe"
        )
    )
)
if not defined MSBUILD goto :msbuild_missing

echo.
echo [4/5] Building the current solution Release x64 [%BUILD_MODE%]...
taskkill /IM HeritageEngine.exe /F >nul 2>nul
if exist "%ENGINE%" del /q "%ENGINE%" >nul 2>nul
"%MSBUILD%" "%SOLUTION%" /t:%MSBUILD_TARGET% /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal /fl /flp:"logfile=%BUILD_LOG%;verbosity=normal"
if errorlevel 1 goto :solution_build_failed
if not exist "%TEST_EXE%" goto :test_exe_missing
if not exist "%ENGINE%" goto :engine_exe_missing

echo.
echo [5/5] Headless native physics regressions...
"%TEST_EXE%" > "%TEST_LOG%" 2>&1
set "TEST_RESULT=!ERRORLEVEL!"
type "%TEST_LOG%"
if not "!TEST_RESULT!"=="0" goto :test_run_failed

echo.
echo ============================================================
echo BUILD + REGRESSION SUCCEEDED
echo Validation: %REPORTS%\ValidationReport.txt
echo Code health: %CODE_HEALTH_REPORT%
echo Physics:    %TEST_LOG%
echo Build:      %BUILD_LOG%
echo Launching the exact freshly built Racing United module now IN THE FOREGROUND.
echo If HeritageEngine exits unexpectedly, this console will remain open and show its process exit code.
echo ============================================================
echo.
echo [run] HeritageEngine starting with persistent console/crash capture...
powershell -NoProfile -ExecutionPolicy Bypass -File "%RUNTIME_CAPTURE%" -Engine "%ENGINE%" -Root "%ROOT%" -ModulePath "%MODULE%" -DiagnosticsDirectory "%DIAGNOSTICS%"
set "ENGINE_RESULT=!ERRORLEVEL!"
echo.
echo ============================================================
echo HeritageEngine process exited with code !ENGINE_RESULT!.
echo Diagnostics directory: %DIAGNOSTICS%
echo Runtime console log:  %RUNTIME_LOG%
echo Native crash report:  %RUNTIME_CRASH%
echo Build log:            %BUILD_LOG%
echo ============================================================
if not "!ENGINE_RESULT!"=="0" (
    echo ERROR: HeritageEngine exited abnormally.
    echo Send RuntimeConsoleLatest.log and RuntimeCrashLatest.txt if it exists; the failure is now persistent.
) else (
    echo HeritageEngine returned normally.
)
echo.
pause
exit /b !ENGINE_RESULT!


:retirement_cleanup_failed
echo.
echo ERROR: OPT01/OPT02/OPT03 retirement convergence failed.
echo The repository was not validated because stale retired files may still be present.
pause
exit /b 1
:legacy_cleanup_failed
echo ERROR: Could not remove obsolete/misplaced architecture files.
echo Close editors/processes locking those files and run this helper again.
goto :failed

:freshness_failed
echo.
echo ERROR: Incremental-build freshness guard failed.
echo Run this helper with FULL as a temporary fallback and send the PowerShell error above.
goto :failed

:audit_failed
echo.
echo ERROR: Static code-health audit failed.
echo Send the PowerShell error above.
goto :failed

:validation_failed
echo.
echo ERROR: Repository safety-net validation failed.
echo Open %REPORTS%\ValidationReport.txt and send the failure lines.
goto :failed

:identity_failed
echo.
echo ERROR: Build identity generation failed.
goto :failed

:msbuild_missing
echo.
echo ERROR: MSBuild.exe could not be found.
echo Install/repair the Visual Studio Desktop development with C++ workload.
goto :failed

:solution_build_failed
echo.
echo ERROR: Heritage Engine solution Release x64 did not build.
echo Send the first compiler/linker error above.
echo Full build log: %BUILD_LOG%
goto :failed

:test_exe_missing
echo.
echo ERROR: Physics regression executable was not created:
echo %TEST_EXE%
goto :failed

:test_run_failed
echo.
echo ERROR: Native physics regression failed.
echo Send: %TEST_LOG%
goto :failed

:engine_exe_missing
echo.
echo ERROR: Heritage Engine executable was not created:
echo %ENGINE%
goto :failed

:failed
echo.
pause
exit /b 1
