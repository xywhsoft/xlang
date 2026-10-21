@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build_current.ps1" %*
exit /b %errorlevel%
