@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "ROOT=%CD%"
set "ENGINE=%ROOT%\Engine\HeritageEngine\HeritageEngine\x64\Release\HeritageEngine.exe"
set "MODULE=%ROOT%\Modules\RacingUnited"
set "SOURCE=%ROOT%\Engine\HeritageEngine\Core\Modules\LuaModuleRuntime.cpp"
set "LOG=%ROOT%\UserData\step26f_exact_release.txt"

echo ============================================================
echo Heritage Engine - exact freshly built Release launch
echo ============================================================
echo Engine: %ENGINE%
echo Module: %MODULE%
echo.

if not exist "%ENGINE%" (
  echo ERROR: The freshly built Release executable was not found:
  echo %ENGINE%
  pause
  exit /b 1
)

if not exist "%SOURCE%" (
  echo ERROR: LuaModuleRuntime.cpp was not found:
  echo %SOURCE%
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$s=[IO.File]::ReadAllText('%SOURCE%'); $q=[char]34; $needle='registerFunction('+$q+'Input'+$q; if($s.Contains($needle)){exit 0}else{exit 6}"
if errorlevel 1 (
  echo ERROR: The live source does not register the Lua Input table.
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p='%ENGINE%'; $s=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($p)); if($s.Contains('GetBindingCount') -and $s.Contains('RegisterAction')){exit 0}else{exit 7}"
if errorlevel 1 (
  echo ERROR: The freshly built executable does not contain the Lua Input API.
  pause
  exit /b 1
)

if not exist "%ROOT%\UserData" mkdir "%ROOT%\UserData"
>"%LOG%" echo RESULT=EXACT_RELEASE_VERIFIED_AND_LAUNCHED
>>"%LOG%" echo TIME=%DATE% %TIME%
>>"%LOG%" echo ENGINE=%ENGINE%
>>"%LOG%" echo MODULE=%MODULE%

taskkill /IM HeritageEngine.exe /F >nul 2>nul
taskkill /IM Launcher.exe /F >nul 2>nul

echo Verified: source and exact Release executable both contain the Lua Input API.
echo Launching the correct nested Release executable now...
echo.
start "" "%ENGINE%" --project-root "%ROOT%" --module-path "%MODULE%" --module RacingUnited
pause
exit /b 0
