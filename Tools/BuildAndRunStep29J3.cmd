@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "LOG=%DIAGNOSTICS%\step29j3_build_and_run.txt"
set "OBSOLETE_MAIN=%ROOT%\Engine\HeritageEngine\main.cpp"
set "OBSOLETE_VEHICLE_DEMO=%MODULE%\Scripts\Runtime\VehicleDemo.lua"

if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"
if not exist "%REPORTS%" mkdir "%REPORTS%"

echo ============================================================
echo Heritage Engine - Step 29J.3 Player Spawn + Collision Bridge Hotfix
echo X right/left ^| Y forward/backward ^| Z height ^| authored 1:1
echo Project root: %ROOT%
echo ============================================================
echo.

for %%F in (
    "%PROJECT%"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Docs\PROJECT_STATE.md"
    "%ROOT%\Docs\Decisions\ADR-010-Blender-Authoring-Player-Scene.md"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.hpp"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp"
    "%MODULE%\Scripts\Runtime\PlayerWorld.lua"
    "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua"
    "%MODULE%\Scripts\UI\Prototype\ScenePanel.lua"
    "%MODULE%\Assets\Scenes\Player\PlayerScene.obj"
    "%MODULE%\Assets\Scenes\Player\PlayerScene_Collision.obj"
    "%MODULE%\Assets\Scenes\Player\README_IMPORT.txt"
    "%MODULE%\Assets\Vehicles\Player\PlayerCar.obj"
    "%MODULE%\Assets\Vehicles\Player\PlayerWheel.obj"
) do if not exist "%%~F" (
    echo ERROR: Required Step 29J.3 file is missing:
    echo %%~F
    pause
    exit /b 1
)

if exist "%OBSOLETE_MAIN%" del /f /q "%OBSOLETE_MAIN%"
if exist "%OBSOLETE_VEHICLE_DEMO%" del /f /q "%OBSOLETE_VEHICLE_DEMO%"

findstr /C:"Physics.LoadStaticBoxScene" "%MODULE%\Scripts\Runtime\PlayerWorld.lua" >nul || goto :layout_failed
findstr /C:"PlayerScene.obj" "%MODULE%\Scripts\Runtime\PlayerWorld.lua" >nul || goto :layout_failed
findstr /C:"radiusScale = 1.0" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"widthScale = 1.0" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"offset = { 0.0, 0.0, 0.0 }" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"DRIVEABLE PLAYER SCENE - STEP 29J.3" "%MODULE%\Scripts\UI\Prototype\ScenePanel.lua" >nul || goto :layout_failed
findstr /C:"spawnPosition" "%MODULE%\Scripts\Runtime\PlayerWorld.lua" >nul || goto :layout_failed
findstr /C:"spawn_player" "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp" >nul || goto :layout_failed
findstr /C:"StaticBoxSceneImporter.cpp" "%PROJECT%" >nul || goto :layout_failed
findstr /C:"Runtime/PlayerWorld.lua" "%MODULE%\Scripts\Main.lua" >nul || goto :layout_failed

echo Generating Step 29J.3 build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "29J.3"
if errorlevel 1 goto :validation_failed

echo.
echo Generating exact Lua API manifest and validating repository contracts...
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
        )
    )
)
if not defined MSBUILD (
    echo ERROR: MSBuild.exe could not be found.
    echo Install the Visual Studio Desktop development with C++ workload.
    pause
    exit /b 1
)

echo.
echo Closing running engine instances...
taskkill /IM HeritageEngine.exe /F >nul 2>nul
taskkill /IM Launcher.exe /F >nul 2>nul

echo Rebuilding HeritageEngine Release x64...
"%MSBUILD%" "%PROJECT%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 goto :build_failed

if not exist "%ENGINE%" (
    echo ERROR: Release executable was not created:
    echo %ENGINE%
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p)); if($s.Contains('29J.3') -and $s.Contains('LoadStaticBoxScene') -and $s.Contains('auto-ground') -and $s.Contains('origin-fallback')){exit 0}else{exit 7}"
if errorlevel 1 (
    echo ERROR: Fresh executable does not contain the Step 29J.3 spawn-bridge markers.
    pause
    exit /b 1
)

>"%LOG%" echo RESULT=STEP_29J3_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo MSBUILD=%MSBUILD%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%
>>"%LOG%" echo LUA_API=%REPORTS%\LuaAPI.md
>>"%LOG%" echo VALIDATION=%REPORTS%\ValidationReport.txt

echo.
echo ============================================================
echo BUILD SUCCEEDED - Step 29J.3 is installed.
echo Your existing PlayerCar.obj and PlayerWheel.obj were not supplied by this update.
echo Open the prototype, then SCENE ^> WORLD ^> LOAD / RELOAD PLAYER SCENE.
echo The engine now auto-picks a road/ground spawn; for exact placement, add SPAWN_PLAYER to the collision OBJ.
echo Player Scene authoring: X left/right, Y forward/backward, Z height, 1 unit = 1 metre.
echo Double-click numeric sliders to type exact values and press Enter.
echo Launching the exact freshly built Release executable now.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:layout_failed
echo.
echo ERROR: Step 29J.3 source/layout verification failed.
echo Re-extract HeritageEngine_Step_29J3.zip and allow file replacement.
pause
exit /b 1

:validation_failed
echo.
echo ERROR: Step 29J.3 project validation failed.
echo Open Build\Reports\ValidationReport.txt and send it with a screenshot.
pause
exit /b 1

:build_failed
echo.
echo ERROR: Step 29J.3 build failed.
echo Send the first compiler or linker error shown above.
pause
exit /b 1
