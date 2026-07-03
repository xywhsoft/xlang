@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist release mkdir release
if not exist release\tmp mkdir release\tmp

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call build_dll.bat
if errorlevel 1 exit /b 1

echo Building DEMO6 xl.exe xlc.dll shell...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o release\xl.exe ^
    src\driver\xl6_xlc_shell.c ^
    src\debug\xl6_dap.c ^
    src\common\xl6_console.c ^
    release\xlc.def ^
    -lShell32

if errorlevel 1 (
    echo DEMO6 xl.exe xlc.dll shell build failed.
    exit /b 1
)

echo DEMO6 build ok: release\xl.exe
exit /b 0
