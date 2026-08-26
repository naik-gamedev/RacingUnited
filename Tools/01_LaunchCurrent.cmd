@echo off
setlocal EnableExtensions EnableDelayedExpansion
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "ENGINE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "DIAGNOSTICS=%ROOT%\UserData\Diagnostics"
set "CAPTURE=%ROOT%\Tools\Diagnostics\LaunchEngineCaptured.ps1"

if not exist "%ENGINE%" (
    echo ERROR: Release executable not found. Run 00_BuildAndRunCurrent.cmd first.
    pause
    exit /b 1
)
if not exist "%CAPTURE%" (
    echo ERROR: Runtime capture helper not found:
    echo %CAPTURE%
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%CAPTURE%" -Engine "%ENGINE%" -Root "%ROOT%" -ModulePath "%MODULE%" -DiagnosticsDirectory "%DIAGNOSTICS%"
set "ENGINE_RESULT=!ERRORLEVEL!"
echo.
echo HeritageEngine exited with code !ENGINE_RESULT!.
echo Runtime log: %DIAGNOSTICS%\RuntimeConsoleLatest.log
if exist "%DIAGNOSTICS%\RuntimeCrashLatest.txt" echo Crash report: %DIAGNOSTICS%\RuntimeCrashLatest.txt
pause
exit /b !ENGINE_RESULT!
