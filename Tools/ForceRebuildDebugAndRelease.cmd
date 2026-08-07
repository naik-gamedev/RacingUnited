@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0.."
set "ROOT=%CD%"
set "ENGINE_PROJ=%ROOT%\Engine\HeritageEngine\HeritageEngine\HeritageEngine.vcxproj"
set "LAUNCHER_PROJ=%ROOT%\Engine\Launcher\Launcher.vcxproj"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "DEBUG_EXE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Debug\HeritageEngine.exe"
set "RELEASE_EXE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "LAUNCHER_EXE=%ROOT%\Engine\HeritageEngine\x64\Release\Launcher.exe"
set "LOG=%ROOT%\UserData\last_full_rebuild.txt"

echo ============================================================
echo Heritage Engine - clean Debug + Release x64 rebuild
echo Project root: %ROOT%
echo ============================================================
echo.

if not exist "%ENGINE_PROJ%" (
  echo ERROR: Engine project not found:
  echo %ENGINE_PROJ%
  pause
  exit /b 1
)
if not exist "%LAUNCHER_PROJ%" (
  echo ERROR: Launcher project not found:
  echo %LAUNCHER_PROJ%
  pause
  exit /b 1
)
if not exist "%MODULE%\Scripts\Main.lua" (
  echo ERROR: Racing United Main.lua not found:
  echo %MODULE%\Scripts\Main.lua
  pause
  exit /b 1
)

taskkill /IM HeritageEngine.exe /F >nul 2>nul
taskkill /IM Launcher.exe /F >nul 2>nul

for %%D in (
  "%ROOT%\Engine\HeritageEngine\x64\Debug"
  "%ROOT%\Engine\HeritageEngine\x64\Release"
  "%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Debug"
  "%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release"
  "%ROOT%\Engine\Launcher\x64\Debug"
  "%ROOT%\Engine\Launcher\x64\Release"
) do (
  if exist "%%~D" (
    echo Removing stale folder: %%~D
    rmdir /S /Q "%%~D"
  )
)

set "MSBUILD="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do if not defined MSBUILD set "MSBUILD=%%I"
)
if not defined MSBUILD (
  for %%V in (18 2022 2025 2026) do (
    for %%E in (Community Professional Enterprise BuildTools) do (
      if exist "C:\Program Files\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=C:\Program Files\Microsoft Visual Studio\%%V\%%E\MSBuild\Current\Bin\MSBuild.exe"
    )
  )
)
if not defined MSBUILD (
  echo ERROR: MSBuild.exe could not be found.
  pause
  exit /b 1
)

echo MSBuild: %MSBUILD%
echo.
echo [1/3] Building HeritageEngine Debug x64...
"%MSBUILD%" "%ENGINE_PROJ%" /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m
if errorlevel 1 goto :build_failed

echo.
echo [2/3] Building HeritageEngine Release x64...
"%MSBUILD%" "%ENGINE_PROJ%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :build_failed

echo.
echo [3/3] Building Launcher Release x64...
"%MSBUILD%" "%LAUNCHER_PROJ%" /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 goto :build_failed

if not exist "%DEBUG_EXE%" (
  echo ERROR: Debug executable was not created:
  echo %DEBUG_EXE%
  pause
  exit /b 1
)
if not exist "%RELEASE_EXE%" (
  echo ERROR: Release executable was not created:
  echo %RELEASE_EXE%
  pause
  exit /b 1
)
if not exist "%LAUNCHER_EXE%" (
  echo ERROR: Launcher executable was not created:
  echo %LAUNCHER_EXE%
  pause
  exit /b 1
)

if not exist "%ROOT%\UserData" mkdir "%ROOT%\UserData"
>"%LOG%" echo RESULT=DEBUG_AND_RELEASE_BUILD_SUCCEEDED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo DEBUG_EXE=%DEBUG_EXE%
>>"%LOG%" echo RELEASE_EXE=%RELEASE_EXE%
>>"%LOG%" echo LAUNCHER_EXE=%LAUNCHER_EXE%
>>"%LOG%" echo MODULE=%MODULE%

echo.
echo ============================================================
echo BUILD SUCCEEDED - both Debug and Release are current.
echo ============================================================
echo Debug:   %DEBUG_EXE%
echo Release: %RELEASE_EXE%
echo.
echo Launching the exact freshly built Release engine now.
echo.
start "" "%RELEASE_EXE%" --project-root "%ROOT%" --module-path "%MODULE%" --module RacingUnited
pause
exit /b 0

:build_failed
echo.
echo ERROR: Build failed. Read the first compiler or linker error above.
pause
exit /b 1
