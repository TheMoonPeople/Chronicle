@echo off
if /I "%~1"=="switch" goto switch
if not exist "%~dp0tools\kazarov\scripts\kazarov.cmd" git -C "%~dp0." submodule update --init tools/kazarov || exit /b 1
call "%~dp0tools\kazarov\scripts\kazarov.cmd" "%~dp0." build %*
exit /b %errorlevel%

:switch
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0port\switch\build.ps1" %*
exit /b %errorlevel%
