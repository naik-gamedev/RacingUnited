@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0HeritageKTX2Workbench.ps1"
if errorlevel 1 (
  echo.
  echo The GUI exited with an error.
  pause
)
endlocal
