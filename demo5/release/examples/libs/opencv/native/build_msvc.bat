@echo off
setlocal
cd /d "%~dp0"

set "OUT=..\bin\opencv_xlang.dll"
set "INC=..\include"
set "LIB=..\lib\opencv_world4130.lib"

if not exist "%INC%\opencv2\core.hpp" (
	echo [error] OpenCV headers were not found under %INC%.
	exit /b 1
)

if not exist "%LIB%" (
	echo [error] OpenCV import library was not found: %LIB%
	exit /b 1
)

where cl.exe >nul 2>nul
if errorlevel 1 (
	if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
		for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
		if defined VSINSTALL call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
	)
)

where cl.exe >nul 2>nul
if errorlevel 1 (
	echo [error] MSVC cl.exe was not found.
	echo [hint] Install Visual Studio Build Tools with the C++ x64 toolchain, or rebuild OpenCV for your current compiler.
	exit /b 1
)

cl /nologo /EHsc /O2 /MD /LD /I"%INC%" opencv_xlang.cpp /link /OUT:"%OUT%" "%LIB%"
if errorlevel 1 exit /b 1

echo [ok] built %OUT%
