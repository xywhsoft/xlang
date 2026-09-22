@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_phase1_extlibs.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_mqtt.ps1
if errorlevel 1 exit /b 1

exit /b 0
