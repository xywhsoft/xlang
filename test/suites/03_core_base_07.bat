@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 7/10...

rem Physical IR, checked failure behavior and real lifetimes replace old C names.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_core_type_closure_contract.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_composite_cast_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_checked_cast_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_nominal_cast_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_named_function_adapter.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_open_function_abi.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_type_baseline.xl --emit build\test\cgen_runtime_type_baseline.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_runtime_is_as.xl --emit build\test\cgen_runtime_is_as.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_type_conversions.xl --emit build\test\cgen_type_conversions.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_implicit_methods.xl --emit build\test\cgen_implicit_methods.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_closure.xl --emit build\test\cgen_closure.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_closure_class_ctor_scope.xl --emit build\test\cgen_closure_class_ctor_scope.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_closure_resource_capture.xl --emit build\test\cgen_closure_resource_capture.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_bare_call.xl --emit build\test\cgen_function_bare_call.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_named_value.xl --emit build\test\cgen_function_named_value.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_named_function_argument.xl --emit build\test\cgen_named_function_argument.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_open_object_field_lifetime.xl --emit build\test\xlir_open_object_field_lifetime.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_refcounted_storage_assignment_lifetime.xl --emit build\test\xlir_refcounted_storage_assignment_lifetime.c
if errorlevel 1 exit /b 1

for %%F in (
    cgen_runtime_type_baseline
    cgen_runtime_is_as
    cgen_type_conversions
    cgen_implicit_methods
    cgen_closure
    cgen_closure_class_ctor_scope
    cgen_closure_resource_capture
    cgen_function_bare_call
    cgen_function_named_value
    cgen_named_function_argument
    xlir_open_object_field_lifetime
    xlir_refcounted_storage_assignment_lifetime
) do (
    "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/%%F.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl test\cases\cgen_class_host.c
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_host.c -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

rem Preserve all original negative sources and their diagnostic domains.
release\xl.exe cgen test\cases\cgen_runtime_as_fail.xl --emit build\test\cgen_runtime_as_fail.c
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_runtime_as_fail.xl > build\test\cgen_runtime_as_fail.out.txt 2> build\test\cgen_runtime_as_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"TypeError" build\test\cgen_runtime_as_fail.err.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_runtime_as_fail.xl -o build\test\cgen_runtime_as_fail.exe
if errorlevel 1 exit /b 1
build\test\cgen_runtime_as_fail.exe > build\test\cgen_runtime_as_fail.exe.out.txt 2> build\test\cgen_runtime_as_fail.exe.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"TypeError" build\test\cgen_runtime_as_fail.exe.err.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_runtime_as_container_target.xl >nul 2> build\test\sem_runtime_as_container_target.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"[sem] as" build\test\sem_runtime_as_container_target.err.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_runtime_as_static_mismatch.xl >nul 2> build\test\sem_runtime_as_static_mismatch.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"[sem] as" build\test\sem_runtime_as_static_mismatch.err.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_runtime_is_var_target.xl >nul 2> build\test\sem_runtime_is_var_target.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"[sem] is" build\test\sem_runtime_is_var_target.err.txt >nul
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 7/10
exit /b 0
