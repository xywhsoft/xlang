@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1
set "XLANG_PACKAGE_PATH=release\extlibs;test\fixtures"

call build.bat
if errorlevel 1 exit /b 1

if exist build\test (
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\clear_test_artifacts.ps1 build\test
    if errorlevel 1 exit /b 1
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
rem Keep the full-regression mirror identical to test\suites\common.bat.
rem Generated C selects private resident declarations from verified XLIR.
for %%H in (src\runtime\xl6_*.h) do copy /Y "%%H" "build\test\__xl_runtime\%%~nxH" >nul
if errorlevel 1 exit /b 1
copy /Y release\xl.dll build\test\xl.dll >nul
if errorlevel 1 exit /b 1
copy /Y release\xl.dll build\tmp\xl.dll >nul
if errorlevel 1 exit /b 1

rem cgen output includes a private runtime path. The full regression prepares
rem the same mirror as test\suites\common.bat before suite early-exit is enabled.
set "XL6_TCC_INCLUDES=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"
set "XL6_TCC_INCLUDES_WINAPI=-Ibuild\test %XL6_TCC_INCLUDES_WINAPI%"

rem Raw generated C consumes the private resident XLIR ABI exported by xl.dll;
rem standalone artifact gates cover self-contained source/runtime packaging.
rem Preserve an explicit XRT-only set for low-level non-generated fixtures.
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

set "XL6_SUITE_PREPARED=1"

call test\suites\00_static_smoke.bat
if errorlevel 1 exit /b 1
call test\suites\01_import_package_source.bat
if errorlevel 1 exit /b 1
call test\suites\02_language_frontend.bat
if errorlevel 1 exit /b 1
call test\suites\03_cgen_runtime_core.bat
if errorlevel 1 exit /b 1
call test\suites\03_type_semantics.bat
if errorlevel 1 exit /b 1
call test\suites\04_oop_abi_export.bat
if errorlevel 1 exit /b 1
call test\suites\04_user_protocol.bat
if errorlevel 1 exit /b 1
call test\suites\04_enum_match.bat
if errorlevel 1 exit /b 1
call test\suites\05_semantics_exceptions_concurrency_base.bat
if errorlevel 1 exit /b 1
call test\suites\06_task_api_spawn.bat
if errorlevel 1 exit /b 1
call test\suites\07_async_generator_channel_docs.bat
if errorlevel 1 exit /b 1
call test\suites\08_stdlib_extended.bat
if errorlevel 1 exit /b 1
call test\suites\09_artifact_closure.bat
if errorlevel 1 exit /b 1

echo DEMO6 smoke tests ok.
exit /b 0

