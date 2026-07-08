@echo off
cd /d "%~dp0\..\.."

if not defined XLANG_PACKAGE_PATH set "XLANG_PACKAGE_PATH=release\extlibs;test\fixtures"

if "%XL6_SUITE_PREPARED%"=="1" exit /b 0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1
set "XLANG_PACKAGE_PATH=release\extlibs;test\fixtures"

set "XL6_NEED_BUILD="
if "%XL6_SUITE_FORCE_BUILD%"=="1" set "XL6_NEED_BUILD=1"
if not exist release\xl.exe set "XL6_NEED_BUILD=1"
if not exist release\xlc.dll set "XL6_NEED_BUILD=1"
if defined XL6_NEED_BUILD (
    call build.bat
    if errorlevel 1 exit /b 1
)

if "%XL6_SUITE_CLEAR_TMP%"=="1" (
    if exist build\test (
        powershell -NoProfile -ExecutionPolicy Bypass -File tools\clear_test_artifacts.ps1 build\test
        if errorlevel 1 exit /b 1
    )
)
if not exist build\test mkdir build\test
if not exist build\test\__xl_runtime mkdir build\test\__xl_runtime
copy /Y release\include\xl_runtime.h build\test\__xl_runtime\xl_runtime.h >nul
if errorlevel 1 exit /b 1

rem cgen output includes a private runtime path. External TCC tests mount only
rem the temporary mirror under build/test so release/include never pollutes xl.exe.
set "XL6_TCC_INCLUDES=-Ibuild\test %XL6_TCC_INCLUDES%"
set "XL6_TCC_INCLUDES_WINAPI=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

exit /b 0

