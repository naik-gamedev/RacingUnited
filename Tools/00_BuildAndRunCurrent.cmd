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
set "MILESTONE=CLOUDURP15E7_SELECTIVE_STOCHASTIC_ACCUMULATION"
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
echo CLOUDURP15E7: upstream UnityVolumetricCloudsURP/HDRP temporal denoiser remains the sole TAA authority; stochastic partial samples receive selective extra persistence
echo Temporal path: full-resolution scene+cloud ^| 5-pixel AABB ^| point history ^| coherent cloud = 95%% ^| noisy partial samples = up to 98.5-99.75%%
echo Single clear stochastic holes at cloud boundaries now accumulate; only a fully clear 5-pixel neighbourhood bypasses history
echo Raymarch jitter also matches upstream semantics: one 0..1 sample per pixel/frame, raw initial offset, same sample only on first relative step
echo CELESTIAL04 dedicated post-opaque Sun/Moon cloud-shadow receiver and the existing 256x256 optical-depth cookie are otherwise unchanged
echo Scene materials attenuate direct celestial light strongly and diffuse sky/IBL modestly under the same cookie, making moving cloud shadows visible without black decals
echo PBSKY01 atmosphere, VCLOUD01 cloud morphology, PERF05 link-status caching, PERF06A F8 diagnostics and OPT00 async timing remain intact
echo Heritage regional weather remains the sole radar/rain/hydrology/cloud-map authority; no second weather simulation is introduced
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
