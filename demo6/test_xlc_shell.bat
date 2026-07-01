@echo off
setlocal
cd /d %~dp0

call build.bat
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\xlc_shell_smoke.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_dap_smoke.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cli_temp_cleanup.ps1
if errorlevel 1 exit /b 1

echo DEMO6 xlc.dll shell tests ok.
exit /b 0
