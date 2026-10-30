@echo off
setlocal
cd /d %~dp0

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist "%XL6_RELEASE_DIR%" mkdir "%XL6_RELEASE_DIR%"
if not exist build mkdir build
if not exist build\tmp mkdir build\tmp

rem Regenerate the static stdlib hash index before every build.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\generate_stdlib_index.ps1
if errorlevel 1 exit /b 1

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call build_dll.bat
if errorlevel 1 exit /b 1

echo Building DEMO6 xl.exe xlc.dll shell...
"%XL6_TCC%" -std=c11 -Wall -D_CRT_SECURE_NO_WARNINGS -I. -Irelease %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% -o "%XL6_RELEASE_DIR%\xl.exe" ^
    "%XL6_BUILD_TMP%\xlang_cli.res" ^
    src\driver\xl6_xlc_shell.c ^
    src\common\xl6_console.c ^
    "%XL6_RELEASE_DIR%\xlc.def" ^
    -lShell32

if errorlevel 1 (
    echo DEMO6 xl.exe xlc.dll shell build failed.
    exit /b 1
)

echo DEMO6 build ok: "%XL6_RELEASE_DIR%\xl.exe"
exit /b 0
