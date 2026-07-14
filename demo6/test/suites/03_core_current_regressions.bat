@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: current core ownership regressions...

release\xl.exe check test\cases\cgen_function_direct_owned_args.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_function_direct_owned_args.xl test\cases\cgen_class_host.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_direct_owned_args.xl --emit build\test\cgen_function_direct_owned_args.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_retain(value)" build\test\cgen_function_direct_owned_args.c >nul
if errorlevel 1 (
    echo cgen_function_direct_owned_args.c missing per-call ownership retain.
    exit /b 1
)
release\xl.exe build test\cases\cgen_function_direct_owned_args.xl test\cases\cgen_class_host.c -o build\test\cgen_function_direct_owned_args.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_direct_owned_args.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\hir_vararg_length_index.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\hir_vararg_length_index.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\c_run_exit_code.c
set "C_RUN_EXIT=%ERRORLEVEL%"
if not "%C_RUN_EXIT%"=="37" (
    echo c_run_exit_code.c returned %C_RUN_EXIT%, expected 37.
    exit /b 1
)

echo DEMO6 suite ok: current core ownership regressions
exit /b 0
