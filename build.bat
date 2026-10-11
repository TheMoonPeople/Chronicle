@echo off
if not exist "%~dp0tools\kazarov\host\kazarov.cmd" git -C "%~dp0." submodule update --init tools/kazarov || exit /b 1
call "%~dp0tools\kazarov\host\kazarov.cmd" "%~dp0." build %*
