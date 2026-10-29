@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test_smoke.ps1"
exit /b %ERRORLEVEL%
