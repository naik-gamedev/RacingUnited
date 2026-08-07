@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "LOG=%DIAGNOSTICS%\step29j4a_orientation_fwd_driveability_build_and_run.txt"
set "PROTO=%ROOT%\Modules\RacingUnited\Scripts\Vehicles\Definitions\PrototypeCar.lua"
set "WHEELS=%ROOT%\Modules\RacingUnited\Scripts\Vehicles\VisualWheels.lua"
set "RENDERER=%ROOT%\Engine\HeritageEngine\Graphics\Renderer\EntityMeshRenderer.cpp"
set "ADR=%ROOT%\Docs\Decisions\ADR-011-Vehicle-Forward-Axle-Roles.md"

if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"
if not exist "%REPORTS%" mkdir "%REPORTS%"

echo ============================================================
echo Heritage Engine - Step 29J.4A Peugeot Orientation + Driveability
echo Correct visual forward + front-wheel drive + front-only steering
echo Readable temporary world lighting + Player Scene chase camera
echo Project root: %ROOT%
echo ============================================================
echo.

for %%F in (
    "%PROJECT%"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%RENDERER%"
    "%PROTO%"
    "%WHEELS%"
    "%ADR%"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp"
    "%MODULE%\Assets\Scenes\Player\PlayerScene.obj"
    "%MODULE%\Assets\Scenes\Player\PlayerScene_Collision.obj"
) do if not exist "%%~F" (
    echo ERROR: Required file is missing:
    echo %%~F
    pause
    exit /b 1
)

echo Verifying Step 29J.4A source contract...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p=[IO.File]::ReadAllText('%PROTO%');" ^
  "$w=[IO.File]::ReadAllText('%WHEELS%');" ^
  "$r=[IO.File]::ReadAllText('%RENDERER%');" ^
  "$ok=$p.Contains('rotationDegrees = { 0.0, 180.0, 0.0 }') -and" ^
  "    ([regex]::Matches($p,'driveFactor = 0\.5').Count -ge 2) -and" ^
  "    ([regex]::Matches($p,'driveFactor = 0\.0').Count -ge 2) -and" ^
  "    ([regex]::Matches($p,'steerFactor = 0\.0').Count -ge 2) -and" ^
  "    $w.Contains('visualSteerAngle') -and" ^
  "    $r.Contains('gl_FrontFacing') -and $r.Contains('playerWorldActive');" ^
  "if(-not $ok){exit 7}"
if errorlevel 1 (
    echo ERROR: Step 29J.4A source/layout verification failed.
    echo Re-extract Step 29J.4A and allow file replacement.
    pause
    exit /b 1
)
echo PASS: Step 29J.4A source contract is present.

echo.
echo Generating Step 29J.4A build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "29J.4A"
if errorlevel 1 goto :validation_failed

echo.
echo Running repository safety-net validation...
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
    pause
    exit /b 1
)

echo.
echo Closing running engine instances...
taskkill /IM HeritageEngine.exe /F >nul 2>nul
taskkill /IM Launcher.exe /F >nul 2>nul

echo Rebuilding HeritageEngine Release x64...
if exist "%ENGINE%" del /Q "%ENGINE%" >nul 2>nul
"%MSBUILD%" "%PROJECT%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 goto :build_failed

if not exist "%ENGINE%" (
    echo ERROR: Release executable was not created:
    echo %ENGINE%
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p));" ^
  "if($s.Contains('Physics.LoadStaticTriangleScene requires') -and $s.Contains('gl_FrontFacing') -and $s.Contains('Player Scene Visual')){exit 0}else{exit 7}"
if errorlevel 1 (
    echo ERROR: Fresh executable does not contain the Step 29J.4A native markers.
    pause
    exit /b 1
)

>"%LOG%" echo RESULT=STEP_29J4A_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%
>>"%LOG%" echo VEHICLE_AUTHORING_FORWARD=BLENDER_MINUS_Y
>>"%LOG%" echo PROTOTYPE_DRIVELINE=FWD
>>"%LOG%" echo PROTOTYPE_STEERING=FRONT_ONLY

echo.
echo ============================================================
echo BUILD SUCCEEDED - Step 29J.4A installed.
echo Creator OBJ files were NOT supplied or replaced by this update.
echo Open prototype ^> SCENE ^> WORLD ^> LOAD / RELOAD PLAYER SCENE.
echo Expected: body nose, physical front axle and chase camera agree;
echo only the front wheels steer; the front wheels are driven.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:validation_failed
echo.
echo ERROR: Step 29J.4A project validation failed.
echo Send Build\Reports\ValidationReport.txt plus a screenshot.
pause
exit /b 1

:build_failed
echo.
echo ERROR: Step 29J.4A build failed.
echo Send the first compiler or linker error shown above.
pause
exit /b 1
