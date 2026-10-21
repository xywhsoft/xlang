@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\test.ps1" %*
exit /b %errorlevel%