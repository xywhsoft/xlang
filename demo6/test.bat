@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1
set "XLANG_PACKAGE_PATH=release\extlibs;test\fixtures"

call build.bat
if errorlevel 1 exit /b 1

if exist release\tmp (
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\clear_release_tmp_files.ps1 release\tmp
    if errorlevel 1 exit /b 1
)
if not exist release\tmp mkdir release\tmp

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

set "XL6_SUITE_PREPARED=1"

call test\suites\00_static_smoke.bat
if errorlevel 1 exit /b 1
call test\suites\01_import_package_xapi.bat
if errorlevel 1 exit /b 1
call test\suites\02_language_frontend.bat
if errorlevel 1 exit /b 1
call test\suites\03_cgen_runtime_core.bat
if errorlevel 1 exit /b 1
call test\suites\04_oop_abi_export.bat
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

