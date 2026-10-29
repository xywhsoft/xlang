@echo off
setlocal
cd /d %~dp0\..

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist build mkdir build
if not exist build\tmp mkdir build\tmp

if not defined XL6_CGEN_OBJ set "XL6_CGEN_OBJ=%CD%\build\tmp\xrt_cgen.o"

echo Building DEMO6 generated-C runtime object...
"%XL6_BOOTSTRAP_CC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -c tools\xrt_single_header_impl.c -o "%XL6_CGEN_OBJ%"
if errorlevel 1 (
	echo DEMO6 generated-C runtime object build failed.
	exit /b 1
)

exit /b 0
