@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageStudio\HeritageStudio.vcxproj"
set "STUDIO=%ROOT%\Build\Studio\Release\HeritageStudio.exe"
set "REPORTS=%ROOT%\Build\Reports"
set "BUILD_LOG=%REPORTS%\HeritageStudioBuild.log"

if not exist "%REPORTS%" mkdir "%REPORTS%" >nul 2>nul

echo ============================================================
echo Heritage Studio - build + run [STUDIO31_DEPTH_AWARE_ADAPTIVE_BLENDER_GRID]
echo Standalone authoring executable: Scene / Race / Traffic / Weather / Vehicle / Audio / Assets / Gameplay
echo Blender-style authoring UX plus live Scene_*.glb PBR preview, STUDIO28 cone-course gameplay, and STUDIO31 depth-aware derivative-filtered infinite floor grid / X-Z world axes without horizon combing.
echo ============================================================
echo Root: %ROOT%
echo.

if not exist "%PROJECT%" (
    echo ERROR: Heritage Studio project is missing:
    echo %PROJECT%
    pause
    exit /b 1
)

rem TIRE44A overlay convergence: ZIP extraction cannot delete the retired
rem render-time carcass bridge left by TIRE43/STUDIO31. Remove it before the
rem shared repository validator inventories all Lua binding translation units.
for %%F in (
    "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.cpp"
    "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.hpp"
) do if exist "%%~F" del /f /q "%%~F" >nul 2>nul
if exist "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.cpp" goto :tire44_cleanup_failed
if exist "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaBindings\Entity\LuaEntityTireFlexibleRingBridge.hpp" goto :tire44_cleanup_failed

echo [1/3] Repository validation...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\ValidateProject.ps1" -Root "%ROOT%"
if errorlevel 1 goto :validation_failed

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
echo [2/3] Building HeritageStudio Release x64...
taskkill /IM HeritageStudio.exe /F >nul 2>nul
if exist "%STUDIO%" del /q "%STUDIO%" >nul 2>nul
"%MSBUILD%" "%PROJECT%" /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal /fl /flp:"logfile=%BUILD_LOG%;verbosity=normal"
if errorlevel 1 goto :build_failed
if not exist "%STUDIO%" goto :exe_missing

echo.
echo [3/3] Launching HeritageStudio.exe...
echo %STUDIO%
pushd "%ROOT%"
start "Heritage Studio" /wait "%STUDIO%"
set "RESULT=!ERRORLEVEL!"
popd
exit /b !RESULT!

:tire44_cleanup_failed
echo.
echo ERROR: Could not remove the retired TIRE44 render-time carcass bridge.
echo Close editors/processes locking the file and run this helper again.
goto :failed

:validation_failed
echo.
echo ERROR: Repository validation failed.
echo Open %REPORTS%\ValidationReport.txt and send the failure lines.
goto :failed

:msbuild_missing
echo.
echo ERROR: MSBuild.exe could not be found.
goto :failed

:build_failed
echo.
echo ERROR: HeritageStudio Release x64 did not build.
echo Full build log: %BUILD_LOG%
goto :failed

:exe_missing
echo.
echo ERROR: Build succeeded but HeritageStudio.exe was not created:
echo %STUDIO%
goto :failed

:failed
echo.
pause
exit /b 1
