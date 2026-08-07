@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "LOG=%DIAGNOSTICS%\step29j4_triangle_player_scene_build_and_run.txt"

if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"
if not exist "%REPORTS%" mkdir "%REPORTS%"

echo ============================================================
echo Heritage Engine - Step 29J.4 Exact Player-Scene Drive Surface
echo Blender OBJ axes + triangle tire/suspension queries + spawn fix
echo Project root: %ROOT%
echo ============================================================
echo.

for %%F in (
    "%PROJECT%"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Engine\HeritageEngine\Physics\CollisionSystem.cpp"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.hpp"
    "%ROOT%\Engine\HeritageEngine\Graphics\Mesh.cpp"
    "%MODULE%\Scripts\Runtime\PlayerWorld.lua"
    "%MODULE%\Scripts\UI\Prototype\ScenePanel.lua"
    "%MODULE%\Assets\Scenes\Player\PlayerScene.obj"
    "%MODULE%\Assets\Scenes\Player\PlayerScene_Collision.obj"
) do if not exist "%%~F" (
    echo ERROR: Required file is missing:
    echo %%~F
    pause
    exit /b 1
)

findstr /C:"LoadStaticTriangleScene" "%ROOT%\Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp" >nul || goto :layout_missing_lua_triangle
findstr /C:"rayStaticSceneTriangle" "%ROOT%\Engine\HeritageEngine\Physics\CollisionSystem.cpp" >nul || goto :layout_missing_triangle_query
findstr /C:"isSpawnMarkerName" "%ROOT%\Engine\HeritageEngine\Physics\StaticTriangleSceneImporter.cpp" >nul || goto :layout_missing_spawn_parser
findstr /C:"value[0], value[1], -value[2]" "%ROOT%\Engine\HeritageEngine\Graphics\Mesh.cpp" >nul || goto :layout_missing_obj_axes

echo Generating Step 29J.4 build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "29J.4"
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
"%MSBUILD%" "%PROJECT%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 goto :build_failed

if not exist "%ENGINE%" (
    echo ERROR: Release executable was not created:
    echo %ENGINE%
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p)); if($s.Contains('Physics.LoadStaticTriangleScene requires') -and $s.Contains('Static triangle OBJ contained no usable triangles.') -and $s.Contains('triangle-origin')){exit 0}else{exit 7}"
if errorlevel 1 (
    echo ERROR: Fresh executable does not contain the Step 29J.4 triangle-scene markers.
    pause
    exit /b 1
)

>"%LOG%" echo RESULT=STEP_29J4_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%

echo.
echo ============================================================
echo BUILD SUCCEEDED - Step 29J.4 installed.
echo No creator OBJ files were supplied or replaced by this update.
echo Open SCENE ^> WORLD and press LOAD / RELOAD PLAYER SCENE.
echo Your current scene should report Spawn source: marker and thousands
echo of drive-surface triangles instead of 2 giant collision boxes.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:layout_missing_lua_triangle
echo.
echo ERROR: Step 29J.4 source/layout verification failed.
echo Missing marker: Physics.LoadStaticTriangleScene binding.
echo Re-extract Step 29J.4 and allow file replacement.
pause
exit /b 1

:layout_missing_triangle_query
echo.
echo ERROR: Step 29J.4 source/layout verification failed.
echo Missing marker: rayStaticSceneTriangle query path.
echo Re-extract Step 29J.4 and allow file replacement.
pause
exit /b 1

:layout_missing_spawn_parser
echo.
echo ERROR: Step 29J.4 source/layout verification failed.
echo Missing marker: isSpawnMarkerName spawn parser.
echo Re-extract Step 29J.4 and allow file replacement.
pause
exit /b 1

:layout_missing_obj_axes
echo.
echo ERROR: Step 29J.4 source/layout verification failed.
echo Missing marker: Blender OBJ axis conversion.
echo Re-extract Step 29J.4 and allow file replacement.
pause
exit /b 1

:validation_failed
echo.
echo ERROR: Step 29J.4 project validation failed.
echo Send Build\Reports\ValidationReport.txt plus a screenshot.
pause
exit /b 1

:build_failed
echo.
echo ERROR: Step 29J.4 build failed.
echo Send the first compiler or linker error shown above.
pause
exit /b 1
