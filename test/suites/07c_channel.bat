@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: Channel owned values...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_channel_resource_abi.ps1
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
echo DEMO6 suite ok: Channel owned values
exit /b 0
