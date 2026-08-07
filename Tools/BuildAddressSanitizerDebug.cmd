@echo off
setlocal EnableExtensions EnableDelayedExpansion
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "PROJECT=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Debug\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"

 echo ============================================================
 echo Heritage Engine - Optional MSVC AddressSanitizer Debug Build
 echo Use this when investigating native memory lifetime corruption.
 echo ============================================================

powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\GenerateBuildIdentity.ps1" -Root "%ROOT%" -Configuration "Debug-ASan" -Milestone "29E.3"
if errorlevel 1 goto :failed
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\Tools\ValidateProject.ps1" -Root "%ROOT%"
if errorlevel 1 goto :failed

set "MSBUILD="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do if not defined MSBUILD set "MSBUILD=%%I"
)
if not defined MSBUILD (
    echo ERROR: MSBuild.exe could not be found.
    pause
    exit /b 1
)

taskkill /IM HeritageEngine.exe /F >nul 2>nul
 echo Building Debug x64 with AddressSanitizer enabled...
"%MSBUILD%" "%PROJECT%" /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /p:EnableASAN=true /m /nologo
if errorlevel 1 goto :failed
if not exist "%ENGINE%" goto :failed

 echo AddressSanitizer build succeeded. Launching it now.
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0

:failed
 echo.
 echo AddressSanitizer build failed. The normal Release build is unaffected.
 echo Send the first error shown above if this diagnostic build is needed.
pause
exit /b 1
