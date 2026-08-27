@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist "Build\AudioSourceCache" mkdir "Build\AudioSourceCache"
cl.exe /nologo /std:c++20 /EHsc /O2 "Tools\Audio\OggToWav.cpp" /Fo:"Build\AudioSourceCache\OggToWav.obj" /Fe:"Build\AudioSourceCache\OggToWav.exe"
exit /b %errorlevel%
