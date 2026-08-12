@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "ENGINE_PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "TEST_PROJECT=%ROOT%\Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "TEST_EXE=%ROOT%\Engine\HeritageEngine\Tests\x64\Release\HeritagePhysicsTests.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "BUILD_LOG=%REPORTS%\CurrentBuild.log"
set "TEST_LOG=%DIAGNOSTICS%\physics_regression_current.txt"
set "MILESTONE=TIRE33A-GLSL-CARCASS-HOTFIX"
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
echo TIRE33A TIRE33 carcass relaxation + corrected live GLSL radial mask
echo ============================================================
echo Root: %ROOT%
echo Build mode: %BUILD_MODE%  ^(pass FULL for an explicit full rebuild^)
echo.

for %%F in (
    "%ENGINE_PROJECT%"
    "%TEST_PROJECT%"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Tools\GenerateLuaApiManifest.ps1"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\EnsureIncrementalBuildFreshness.ps1"
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
) do if exist "%%~F" del /f /q "%%~F" >nul 2>nul

if exist "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Scenes\RacingUnitedBootScene.hpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\AerodynamicsSystem.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\AeroSurface.cpp" goto :legacy_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Vehicles\GroundEffect.cpp" goto :legacy_cleanup_failed

echo [0/4] Incremental-build freshness guard...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\EnsureIncrementalBuildFreshness.ps1" -Root "%ROOT%"
if errorlevel 1 goto :freshness_failed

echo.
echo [1/4] Static repository validation...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\ValidateProject.ps1" -Root "%ROOT%"
if errorlevel 1 goto :validation_failed

echo.
echo [2/4] Build identity...
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
echo [3/4] Headless native physics regressions...
"%MSBUILD%" "%TEST_PROJECT%" /t:%MSBUILD_TARGET% /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
if errorlevel 1 goto :test_build_failed
if not exist "%TEST_EXE%" goto :test_exe_missing
"%TEST_EXE%" > "%TEST_LOG%" 2>&1
set "TEST_RESULT=!ERRORLEVEL!"
type "%TEST_LOG%"
if not "!TEST_RESULT!"=="0" goto :test_run_failed

echo.
echo [4/4] Building Heritage Engine Release x64 [%BUILD_MODE%]...
taskkill /IM HeritageEngine.exe /F >nul 2>nul
if exist "%ENGINE%" del /q "%ENGINE%" >nul 2>nul
"%MSBUILD%" "%ENGINE_PROJECT%" /t:%MSBUILD_TARGET% /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal /fl /flp:"logfile=%BUILD_LOG%;verbosity=normal"
if errorlevel 1 goto :engine_build_failed
if not exist "%ENGINE%" goto :engine_exe_missing

echo.
echo ============================================================
echo BUILD + REGRESSION SUCCEEDED
echo Validation: %REPORTS%\ValidationReport.txt
echo Physics:    %TEST_LOG%
echo Build:      %BUILD_LOG%
echo Launching the exact freshly built Racing United module now.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:legacy_cleanup_failed
echo ERROR: Could not remove obsolete/misplaced architecture files.
echo Close editors/processes locking those files and run this helper again.
goto :failed

:freshness_failed
echo.
echo ERROR: Incremental-build freshness guard failed.
echo Run this helper with FULL as a temporary fallback and send the PowerShell error above.
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

:test_build_failed
echo.
echo ERROR: HeritagePhysicsTests Release x64 did not build.
echo Send the first compiler/linker error above.
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

:engine_build_failed
echo.
echo ERROR: Heritage Engine Release x64 build failed.
echo Send the first compiler/linker error plus:
echo %BUILD_LOG%
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
