@echo off
cd /d "%~dp0\..\.."

rem Enable command echo explicitly while debugging a suite.
if "%XL6_TRACE_TESTS%"=="1" echo on

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

rem cgen output includes the private runtime and may use Windows TLS primitives.
rem All generated-C compilations therefore use the WinAPI include closure; host-only
rem C fixtures tolerate the superset and keep one stable test compiler environment.
set "XL6_TCC_INCLUDES=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"
set "XL6_TCC_INCLUDES_WINAPI=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

exit /b 0

