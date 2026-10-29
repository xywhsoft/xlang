@echo off
call "%~dp0build.bat"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test_smoke.ps1"
exit /b %ERRORLEVEL%
