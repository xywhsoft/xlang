@echo off
setlocal
cd /d "%~dp0\..\..\.."

xl.exe run projects\xs\src\runtime_soak_test.xl || exit /b 1

echo xs2 soak suite ok
exit /b 0
