@echo off
setlocal EnableExtensions

for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"
set "TEST_PROJECT=%PROJECT_ROOT%\Engine\HeritageEngine\Tests\HeritagePhysicsTests.vcxproj"
set "TEST_EXE=%PROJECT_ROOT%\Engine\HeritageEngine\Tests\x64\Release\HeritagePhysicsTests.exe"

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

echo Building deterministic Heritage physics regressions...
"%MSBUILD%" "%TEST_PROJECT%" /t:Build /p:Configuration=Release /p:Platform=x64 /m /nologo
if errorlevel 1 goto :failed

echo.
echo Running flat-rest, 1000 Hz, parking-brake, wake, and slope tests...
"%TEST_EXE%"
if errorlevel 1 goto :failed

echo.
echo ALL HERITAGE PHYSICS TESTS PASSED.
pause
exit /b 0

:failed
echo.
echo HERITAGE PHYSICS TESTS FAILED. Read the first FAIL line above.
pause
exit /b 1
