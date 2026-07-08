@echo off
setlocal
cd /d "%~dp0\..\.."

if not exist projects\xs_static\release mkdir projects\xs_static\release

release\xl.exe check projects\xs_static\xs_static.xl
if errorlevel 1 exit /b 1

release\xl.exe build projects\xs_static\xs_static.xl -o projects\xs_static\release\xs_static.exe
if errorlevel 1 exit /b 1

echo xs_static build ok: projects\xs_static\release\xs_static.exe
exit /b 0
