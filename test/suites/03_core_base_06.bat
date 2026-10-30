@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 6/10...

rem Keep the original program; validate physical XLIR instead of legacy names.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_runtime_callable_value_abi.ps1
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_runtime_objects.xl --emit build\test\cgen_var_runtime_objects.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_var_runtime_objects.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_var_runtime_objects.strict.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_runtime_objects.strict.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_runtime_objects.xl test\cases\cgen_class_host.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_var_runtime_objects.xl test\cases\cgen_class_host.c -o build\test\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\stdlib_release_api_contracts.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_release_api_contracts.xl -o build\test\stdlib_release_api_contracts.exe
if errorlevel 1 exit /b 1
build\test\stdlib_release_api_contracts.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 6/10
exit /b 0
