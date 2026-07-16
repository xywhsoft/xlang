@echo off
setlocal
cd /d "%~dp0\..\..\.."

powershell -NoProfile -ExecutionPolicy Bypass -File projects\xs\tests\memory_stability.ps1 || exit /b 1

echo xs2 memcheck suite ok
exit /b 0
