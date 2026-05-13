@echo off
setlocal
cd /d "%~dp0"

where gcc.exe >nul 2>nul
if errorlevel 1 (
	echo [error] gcc.exe was not found.
	exit /b 1
)

gcc -std=c99 -O2 -shared -I"..\include" opencv_xlang_stb.c -o "..\bin\opencv_xlang.dll"
if errorlevel 1 exit /b 1

echo [ok] built ..\bin\opencv_xlang.dll
