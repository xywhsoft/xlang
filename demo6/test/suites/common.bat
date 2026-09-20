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
copy /Y release\include\xl_extlib_abi.h build\test\__xl_runtime\xl_extlib_abi.h >nul
if errorlevel 1 exit /b 1
copy /Y release\include\xrt_decl.h build\test\__xl_runtime\xrt_decl.h >nul
if errorlevel 1 exit /b 1
copy /Y release\include\xruntime_decl.h build\test\__xl_runtime\xruntime_decl.h >nul
if errorlevel 1 exit /b 1
rem XLIR generated C includes only the private ABI headers for resident
rem components selected by its verified dependency plan. Stage the complete
rem declaration set so every gate is hermetic on a clean build\test tree.
for %%H in (src\runtime\xl6_*.h) do copy /Y "%%H" "build\test\__xl_runtime\%%~nxH" >nul
if errorlevel 1 exit /b 1
copy /Y release\xl.dll build\test\xl.dll >nul
if errorlevel 1 exit /b 1
copy /Y release\xl.dll build\tmp\xl.dll >nul
if errorlevel 1 exit /b 1

rem cgen output includes the private runtime and may use Windows TLS primitives.
rem All generated-C compilations therefore use the WinAPI include closure; host-only
rem C fixtures tolerate the superset and keep one stable test compiler environment.
set "XL6_TCC_INCLUDES=-std=c11 -Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"
set "XL6_TCC_INCLUDES_WINAPI=-std=c11 -Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"

rem Raw generated C consumes the private resident XLIR ABI exported by xl.dll;
rem standalone `xl build` gates separately prove source-packaged artifact closure.
rem Keep an explicit XRT-only link set for low-level fixtures that intentionally
rem do not consume generated language-runtime declarations.
set "XL6_CGEN_CC=%XL6_BOOTSTRAP_CC%"
set "XL6_CGEN_OBJ=%XL6_ROOT%\build\tmp\xrt_cgen.o"
set "XL6_CGEN_FLAGS=-std=c11 -Wall -Wextra -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers"
set "XL6_CGEN_INCLUDES=-Ibuild\test -Irelease\include -Ilib\xrt -Ilib\xruntime -I."
set XL6_CGEN_XRT_LINKS="%XL6_CGEN_OBJ%" -lWs2_32 -lIPHLPAPI -lShell32
set XL6_CGEN_LINKS=release\xl.dll -lWs2_32 -lIPHLPAPI -lShell32
set XL6_CGEN_XLANG_LINKS=release\xl.dll -lWs2_32 -lIPHLPAPI -lShell32

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_cgen_obj.bat
if errorlevel 1 exit /b 1

rem Toolchain helpers use @echo off; re-enable tracing after they return.
if "%XL6_TRACE_TESTS%"=="1" echo on
exit /b 0

