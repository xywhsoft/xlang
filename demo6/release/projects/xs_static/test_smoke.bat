@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0test_smoke.ps1"
exit /b %ERRORLEVEL%
