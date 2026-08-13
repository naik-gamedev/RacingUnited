@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "ENGINE=%ROOT%\Engine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"

if not exist "%ENGINE%" (
    echo ERROR: Release executable not found. Run 00_BuildAndRunCurrent.cmd first.
    pause
    exit /b 1
)

start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module "RacingUnited"
exit /b 0
