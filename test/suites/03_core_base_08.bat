@echo off
setlocal EnableDelayedExpansion
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 8/10...

rem Verify language/physical IR contracts instead of legacy C wrapper names.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_core_callable_control_contract.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_named_function_adapter.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callable_field_frame_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_binding.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_range_for_abi.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_function_value.xl --emit build\test\cgen_extern_function_value.c --emit-header build\test\cgen_extern_function_value.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_multi_return.xl --emit build\test\cgen_function_multi_return.c --emit-header build\test\cgen_function_multi_return.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_member_function_multi_return.xl --emit build\test\cgen_member_function_multi_return.c --emit-header build\test\cgen_member_function_multi_return.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_member_function_class_return.xl --emit build\test\cgen_member_function_class_return.c --emit-header build\test\cgen_member_function_class_return.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_default.xl --emit build\test\cgen_function_default.c --emit-header build\test\cgen_function_default.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_null_compare.xl --emit build\test\cgen_function_null_compare.c --emit-header build\test\cgen_function_null_compare.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_optional_nullable_function_default.xl --emit build\test\cgen_optional_nullable_function_default.c --emit-header build\test\cgen_optional_nullable_function_default.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_function_adapter_dedupe.xl --emit build\test\cgen_function_adapter_dedupe.c --emit-header build\test\cgen_function_adapter_dedupe.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\call_defaults.xl --emit build\test\call_defaults.c --emit-header build\test\call_defaults.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_range.xl --emit build\test\cgen_range.c --emit-header build\test\cgen_range.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_range_assert.xl --emit build\test\cgen_range_assert.c --emit-header build\test\cgen_range_assert.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_assert_dynamic_condition.xl --emit build\test\cgen_assert_dynamic_condition.c --emit-header build\test\cgen_assert_dynamic_condition.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_comparison_expressions.xl --emit build\test\cgen_var_comparison_expressions.c --emit-header build\test\cgen_var_comparison_expressions.h
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_named_callable_owner_lifetime.xl --emit build\test\xlir_named_callable_owner_lifetime.c --emit-header build\test\xlir_named_callable_owner_lifetime.h
if errorlevel 1 exit /b 1

rem Private class layout belongs to IR; public headers expose only exported ABI.
for %%F in (
    cgen_extern_function_value
    cgen_function_multi_return
    cgen_member_function_multi_return
    cgen_member_function_class_return
    cgen_function_default
    cgen_function_null_compare
    cgen_optional_nullable_function_default
    cgen_function_adapter_dedupe
    call_defaults
    cgen_range
    cgen_range_assert
    cgen_assert_dynamic_condition
    cgen_var_comparison_expressions
    xlir_named_callable_owner_lifetime
) do (
    set "XL6_IMPORT_HOST=test\cases\cgen_class_host.c"
    if "%%F"=="cgen_extern_function_value" set "XL6_IMPORT_HOST=test\cases\cgen_extern_function_value_host.c"
    set "XL6_ENTRY_FLAG="
    if "%%F"=="cgen_function_null_compare" set "XL6_ENTRY_FLAG=-DXLIR_GENERATED_MODULE_USER_ENTRY"
    if "%%F"=="cgen_optional_nullable_function_default" set "XL6_ENTRY_FLAG=-DXLIR_GENERATED_MODULE_USER_ENTRY"
    "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -x c -fsyntax-only build\test\%%F.h
    if errorlevel 1 exit /b 1
    "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/test/%%F.c\" !XL6_ENTRY_FLAG! test\cases\xlir_generated_module_host.c !XL6_IMPORT_HOST! !XL6_CGEN_LINKS! -o build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl !XL6_IMPORT_HOST!
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl !XL6_IMPORT_HOST! -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)
for /L %%I in (1,1,3) do (
    build\test\cgen_member_function_class_return.exe
    if errorlevel 1 exit /b 1
)

rem Count real generated allocations, including capture-free adapter objects.
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_named_callable_owner_lifetime_host.c !XL6_CGEN_LINKS! -o build\test\xlir_named_callable_owner_lifetime.alloc.exe
if errorlevel 1 exit /b 1
build\test\xlir_named_callable_owner_lifetime.alloc.exe
if errorlevel 1 exit /b 1
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_ALLOCATION_NULL_COMPARE -DXLIR_GENERATED_MODULE_USER_ENTRY -DXLIR_MIN_ALLOCATIONS=1 test\cases\xlir_named_callable_owner_lifetime_host.c !XL6_CGEN_LINKS! -o build\test\xlir_closure_null_allocation.exe
if errorlevel 1 exit /b 1
build\test\xlir_closure_null_allocation.exe
if errorlevel 1 exit /b 1

rem Preserve the dedicated dynamic-comparison host and original runtime negative.
release\xl.exe cgen test\cases\cgen_var_comparison_expressions.xl --backend xlir --emit build\test\xlir_dynamic_comparison_generated.c
if errorlevel 1 exit /b 1
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_dynamic_comparison_host.c !XL6_CGEN_LINKS! -o build\test\xlir_dynamic_comparison_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_comparison_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_range_step_runtime_zero.xl --emit build\test\cgen_range_step_runtime_zero.c
if errorlevel 1 exit /b 1
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_range_step_runtime_zero.c\" test\cases\xlir_generated_module_host.c !XL6_CGEN_LINKS! -o build\test\cgen_range_step_runtime_zero.strict.exe
if errorlevel 1 exit /b 1
build\test\cgen_range_step_runtime_zero.strict.exe
if not errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_range_step_runtime_zero.xl > build\test\cgen_range_step_runtime_zero.run.out.txt 2> build\test\cgen_range_step_runtime_zero.run.err.txt
if not errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_range_step_runtime_zero.xl -o build\test\cgen_range_step_runtime_zero.exe
if errorlevel 1 exit /b 1
build\test\cgen_range_step_runtime_zero.exe > build\test\cgen_range_step_runtime_zero.out.txt 2> build\test\cgen_range_step_runtime_zero.err.txt
if not errorlevel 1 exit /b 1
for %%F in (cgen_range_step_runtime_zero.run.err.txt cgen_range_step_runtime_zero.err.txt) do (
    findstr /C:"RangeError" build\test\%%F >nul
    if errorlevel 1 exit /b 1
    findstr /C:"range step cannot be 0" build\test\%%F >nul
    if errorlevel 1 exit /b 1
)

echo DEMO6 suite ok: core C generation and runtime baseline 8/10
exit /b 0
