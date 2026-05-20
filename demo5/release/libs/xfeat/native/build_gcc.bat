@echo off
setlocal
cd /d "%~dp0"

where gcc.exe >nul 2>nul
if errorlevel 1 (
	echo [error] gcc.exe was not found.
	exit /b 1
)

if not exist "..\bin" mkdir "..\bin"

gcc -std=c99 -O3 -mavx2 -mfma -ffast-math -shared "..\src\xfeat_artifact.c" -o "..\bin\xfeat_xlang.dll"
if errorlevel 1 exit /b 1

echo [ok] built ..\bin\xfeat_xlang.dll
