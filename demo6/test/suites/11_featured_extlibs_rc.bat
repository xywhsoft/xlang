@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

echo Running DEMO6 suite: featured extension libraries RC...
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_featured_extlibs_rc.ps1
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: featured extension libraries RC
exit /b 0
