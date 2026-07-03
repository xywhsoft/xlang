@echo off
setlocal
cd /d %~dp0\..

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist release mkdir release
if not exist release\tmp mkdir release\tmp

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

echo Building DEMO6 local XRT object...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. %XL6_TCC_INCLUDES_WINAPI% -c tools\xrt_single_header_impl.c -o "%XL6_XRT_OBJ%"
if errorlevel 1 (
    echo DEMO6 XRT object build failed.
    exit /b 1
)

exit /b 0
