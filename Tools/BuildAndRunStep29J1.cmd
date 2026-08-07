@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "LOG=%DIAGNOSTICS%\step29j1_build_and_run.txt"
set "OBSOLETE_MAIN=%ROOT%\Engine\HeritageEngine\main.cpp"
set "OBSOLETE_VEHICLE_DEMO=%MODULE%\Scripts\Runtime\VehicleDemo.lua"

if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"
if not exist "%REPORTS%" mkdir "%REPORTS%"

echo ============================================================
echo Heritage Engine - Step 29J.1 Exact Wheel Centers + Peugeot Reference Geometry
echo Native world-center telemetry + 206 RC reference mounts + per-side wheel facing
echo Project root: %ROOT%
echo ============================================================
echo.

for %%F in (
    "%PROJECT%"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Docs\PROJECT_STATE.md"
    "%ROOT%\Docs\VEHICLE_ARCHITECTURE.md"
    "%ROOT%\Docs\Decisions\ADR-008-Articulated-Wheel-Presentation.md"
    "%ROOT%\Docs\Decisions\ADR-009-Wheel-Coordinate-Contract.md"
    "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua"
    "%MODULE%\Scripts\Vehicles\State.lua"
    "%MODULE%\Scripts\Vehicles\Visuals.lua"
    "%MODULE%\Scripts\Vehicles\VisualWheels.lua"
    "%MODULE%\Scripts\Vehicles\Lifecycle.lua"
    "%MODULE%\Scripts\UI\Vehicle\VisualPanel.lua"
    "%MODULE%\Scripts\UI\Vehicle\Visual\BodyPanel.lua"
    "%MODULE%\Scripts\UI\Vehicle\Visual\WheelsPanel.lua"
    "%MODULE%\Assets\Vehicles\Player\PlayerCar.obj"
    "%MODULE%\Assets\Vehicles\Player\PlayerWheel.obj"
    "%MODULE%\Assets\Vehicles\Player\README_IMPORT.txt"
) do if not exist "%%~F" (
    echo ERROR: Required Step 29J.1 file is missing:
    echo %%~F
    pause
    exit /b 1
)

if exist "%OBSOLETE_MAIN%" del /f /q "%OBSOLETE_MAIN%"
if exist "%OBSOLETE_VEHICLE_DEMO%" del /f /q "%OBSOLETE_VEHICLE_DEMO%"

findstr /C:"articulatedWheels" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"UpdateVehicleWheelPresentation" "%MODULE%\Scripts\Vehicles\VisualWheels.lua" >nul || goto :layout_failed
findstr /C:"telemetry.rotationDegrees" "%MODULE%\Scripts\Vehicles\VisualWheels.lua" >nul || goto :layout_failed
findstr /C:"telemetry.steerAngle" "%MODULE%\Scripts\Vehicles\VisualWheels.lua" >nul || goto :layout_failed
findstr /C:"ARTICULATED WHEELS - STEP 29J.1" "%MODULE%\Scripts\UI\Vehicle\Visual\WheelsPanel.lua" >nul || goto :layout_failed
findstr /C:"wheelbaseM = 2.442" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"frontTrackM = 1.437" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"rearTrackM = 1.428" "%MODULE%\Scripts\Vehicles\Definitions\PrototypeCar.lua" >nul || goto :layout_failed
findstr /C:"Entity.SetWorldPosition" "%MODULE%\Scripts\Vehicles\VisualWheels.lua" >nul || goto :layout_failed
findstr /C:"visualSpinSign" "%MODULE%\Scripts\Vehicles\VisualWheels.lua" >nul || goto :layout_failed
findstr /C:"Vehicles/VisualWheels.lua" "%MODULE%\Scripts\Main.lua" >nul || goto :layout_failed

echo Generating Step 29J.1 build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "29J.1"
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
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p)); if($s.Contains('29J.1') -and $s.Contains('SetWheelTireModel') -and $s.Contains('GetWheelState')){exit 0}else{exit 7}"
if errorlevel 1 (
    echo ERROR: Fresh executable does not contain the Step 29J.1 build identity plus existing native wheel-state markers.
    pause
    exit /b 1
)

>"%LOG%" echo RESULT=STEP_29J1_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo MSBUILD=%MSBUILD%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%
>>"%LOG%" echo LUA_API=%REPORTS%\LuaAPI.md
>>"%LOG%" echo VALIDATION=%REPORTS%\ValidationReport.txt

echo.
echo ============================================================
echo BUILD SUCCEEDED - Step 29J.1 is installed.
echo Your existing PlayerCar.obj was NOT supplied by this update, so the car you imported stays yours.
echo Go to VEHICLE ^> VISUAL ^> WHEELS to inspect the exact wheel-center system.
echo The prototype now uses 2442 mm wheelbase, 1437 mm front track, 1428 mm rear track, and exact native wheel centers.
echo Wheel OBJ convention: origin at wheel center, local X axle, outer face toward +X. Left/right facing is handled per corner.
echo Launching the exact freshly built Release executable now.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:layout_failed
echo.
echo ERROR: Step 29J.1 source/layout verification failed.
echo Re-extract HeritageEngine_Step_29J1.zip and allow file replacement.
pause
exit /b 1

:validation_failed
echo.
echo ERROR: Step 29J.1 project validation failed.
echo Open Build\Reports\ValidationReport.txt and send it with a screenshot.
pause
exit /b 1

:build_failed
echo.
echo ERROR: Step 29J.1 build failed.
echo Send the first compiler or linker error shown above.
pause
exit /b 1
