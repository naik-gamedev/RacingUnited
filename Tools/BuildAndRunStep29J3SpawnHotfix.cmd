@echo off
setlocal EnableExtensions EnableDelayedExpansion

for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "REPORTS=%ROOT%\Build\Reports"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "LOG=%DIAGNOSTICS%\step29j3a_spawn_hotfix_build_and_run.txt"

if not exist "%DIAGNOSTICS%" mkdir "%DIAGNOSTICS%"
if not exist "%REPORTS%" mkdir "%REPORTS%"

echo ============================================================
echo Heritage Engine - Step 29J.3a Blender OBJ Spawn Marker Hotfix
echo Fixes Blender object-name vs OBJ group-name ownership.
echo Project root: %ROOT%
echo ============================================================
echo.

for %%F in (
    "%PROJECT%"
    "%ROOT%\Tools\GenerateBuildIdentity.ps1"
    "%ROOT%\Tools\ValidateProject.ps1"
    "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp"
    "%ROOT%\Docs\Decisions\ADR-010-Blender-Authoring-Player-Scene.md"
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

findstr /C:"std::string currentObject" "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp" >nul || goto :layout_failed
findstr /C:"currentObject.empty()" "%ROOT%\Engine\HeritageEngine\Physics\StaticBoxSceneImporter.cpp" >nul || goto :layout_failed
findstr /C:"SPAWN_PLAYER" "%ROOT%\Docs\Decisions\ADR-010-Blender-Authoring-Player-Scene.md" >nul || goto :layout_failed

echo Generating Step 29J.3a build identity...
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Release" -Milestone "29J.3a"
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
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p)); if($s.Contains('Collision OBJ contains an invalid vertex index in object/group') -and $s.Contains('auto-ground')){exit 0}else{exit 7}"
if errorlevel 1 (
    echo ERROR: Fresh executable does not contain the Step 29J.3a importer markers.
    pause
    exit /b 1
)

>"%LOG%" echo RESULT=STEP_29J3A_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%

echo.
echo ============================================================
echo BUILD SUCCEEDED - Step 29J.3a spawn-marker hotfix installed.
echo No creator OBJ files were supplied or replaced by this hotfix.
echo Open SCENE ^> WORLD and press LOAD / RELOAD PLAYER SCENE.
echo If your Blender object is named SPAWN_PLAYER, Spawn source should now say marker.
echo ============================================================
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:layout_failed
echo.
echo ERROR: Step 29J.3a source/layout verification failed.
echo Re-extract the spawn hotfix and allow file replacement.
pause
exit /b 1

:validation_failed
echo.
echo ERROR: Step 29J.3a project validation failed.
echo Send Build\Reports\ValidationReport.txt plus a screenshot.
pause
exit /b 1

:build_failed
echo.
echo ERROR: Step 29J.3a build failed.
echo Send the first compiler or linker error shown above.
pause
exit /b 1
