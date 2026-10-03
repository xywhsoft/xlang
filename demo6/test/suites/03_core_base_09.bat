@echo off
setlocal EnableDelayedExpansion
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 9/10...

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_text_values.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_buffer_values.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_buffer_codecs.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_buffer_core.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_stack_native.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_stack_ownership.ps1
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_stack_ownership.ps1 -Fixture test/fixtures/stdlib53_pending/stack_detached_cycle.xl
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_module_product_globals.ps1
if errorlevel 1 exit /b 1

rem Language semantics belong to verified IR and observable runtimes.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_range_membership.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_membership.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_xvalue_membership.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_var_iterator_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_erased_collection_iterator_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_membership_evaluation_lifetime.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_string_bridges.ps1
if errorlevel 1 exit /b 1

rem Keep explicit production-command inputs visible to the admission inventory.
release\xl.exe cgen test\cases\cgen_in_range.xl --emit build\test\cgen_in_range.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_in_containers.xl --emit build\test\cgen_in_containers.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_for_in_containers.xl --emit build\test\cgen_for_in_containers.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_approx.xl --emit build\test\cgen_approx.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_math.xl --emit build\test\stdlib_math.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_string.xl --emit build\test\stdlib_string.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time.xl --emit build\test\stdlib_time.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_extra.xl --emit build\test\stdlib_time_extra.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_calendar.xl --emit build\test\stdlib_time_calendar.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_runtime.xl --emit build\test\stdlib_time_runtime.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_random.xl --emit build\test\stdlib_random.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xlir_membership_operand_lifetime.xl --emit build\test\xlir_membership_operand_lifetime.c
if errorlevel 1 exit /b 1

rem Every language positive remains mandatory in strict C, run and build modes.
rem Public example companions: release/examples/stdlib_math_c.c,
rem release/examples/stdlib_string_c.c, release/examples/stdlib_time_c.c.
for %%F in (
    cgen_in_range
    cgen_in_containers
    cgen_for_in_containers
    cgen_approx
    xlir_membership_operand_lifetime
    stdlib_math
) do (
    echo Verifying core 9 runtime: %%F
    set "XL6_ENTRY_FLAG="
    if "%%F"=="stdlib_time_runtime" set "XL6_ENTRY_FLAG=-DXLIR_GENERATED_MODULE_USER_ENTRY"
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/test/%%F.c\" !XL6_ENTRY_FLAG! test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\%%F.strict.exe
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

rem User-approved minimum stdlib scope: admission/strict syntax stays mandatory.
rem Legacy string/time/random wrappers remain deferred, NOT runtime passes.
rem XL6_EXTENDED_STDLIB_RUNTIME=1 explicitly reruns their known runtime gaps.
for %%F in (stdlib_string stdlib_time stdlib_time_extra stdlib_time_calendar stdlib_time_runtime stdlib_random) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -fsyntax-only build\test\%%F.c
    if errorlevel 1 exit /b 1
    if "!XL6_EXTENDED_STDLIB_RUNTIME!"=="1" (
        set "XL6_ENTRY_FLAG="
        if "%%F"=="stdlib_time_runtime" set "XL6_ENTRY_FLAG=-DXLIR_GENERATED_MODULE_USER_ENTRY"
        "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/test/%%F.c\" !XL6_ENTRY_FLAG! test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\%%F.strict.exe
        if errorlevel 1 exit /b 1
        build\test\%%F.strict.exe
        if errorlevel 1 exit /b 1
        release\xl.exe run test\cases\%%F.xl test\cases\cgen_class_host.c
        if errorlevel 1 exit /b 1
        release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_host.c -o build\test\%%F.exe
        if errorlevel 1 exit /b 1
        build\test\%%F.exe
        if errorlevel 1 exit /b 1
    ) else (
        echo Deferred extended stdlib runtime, not counted as passed: %%F
    )
)

rem Track physical allocations as well as language destructors, including strings.
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_ALLOCATION_MEMBERSHIP -DXLIR_MIN_ALLOCATIONS=5600 test\cases\xlir_named_callable_owner_lifetime_host.c !XL6_CGEN_LINKS! -o build\test\xlir_membership_operand_lifetime.alloc.exe
if errorlevel 1 exit /b 1
build\test\xlir_membership_operand_lifetime.alloc.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\cgen_approx.xl --lowered --emit build\test\xlir_approx.xlir
if errorlevel 1 exit /b 1
findstr /C:"runtime=i64-approx" build\test\xlir_approx.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"runtime=f64-approx" build\test\xlir_approx.xlir >nul
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_approx.xl --backend xlir --emit build\test\xlir_approx_generated.c
if errorlevel 1 exit /b 1
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror test\cases\xlir_approx_host.c test\cases\cgen_class_host.c !XL6_CGEN_LINKS! -o build\test\xlir_approx_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_approx_host.exe
if errorlevel 1 exit /b 1

rem A non-container var must fail through the language error path in every mode.
release\xl.exe cgen test\cases\cgen_for_in_var_non_container.xl --emit build\test\cgen_for_in_var_non_container.c
if errorlevel 1 exit /b 1
"!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_for_in_var_non_container.c\" test\cases\xlir_generated_module_diagnostic_host.c !XL6_CGEN_LINKS! -o build\test\cgen_for_in_var_non_container.strict.exe
if errorlevel 1 exit /b 1
build\test\cgen_for_in_var_non_container.strict.exe >nul 2> build\test\cgen_for_in_var_non_container.strict.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"for in value must be array, list, set or dict" build\test\cgen_for_in_var_non_container.strict.err.txt >nul
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_for_in_var_non_container.xl >nul 2> build\test\cgen_for_in_var_non_container.run.err.txt
if not errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_for_in_var_non_container.xl -o build\test\cgen_for_in_var_non_container.exe
if errorlevel 1 exit /b 1
build\test\cgen_for_in_var_non_container.exe >nul 2> build\test\cgen_for_in_var_non_container.err.txt
if not errorlevel 1 exit /b 1
for %%F in (cgen_for_in_var_non_container.run.err.txt cgen_for_in_var_non_container.err.txt) do (
    findstr /C:"TypeError" build\test\%%F >nul
    if errorlevel 1 exit /b 1
    findstr /C:"for in value must be array, list, set or dict" build\test\%%F >nul
    if errorlevel 1 exit /b 1
)

rem Retain the existing minimal public-XRT declaration and example parity checks.
for %%P in (xrtMathClamp xrtTimer xrtSleep xrtTimeOverlap xrtTimeUnix xrtIsLeapYear xrtDaysInMonth xrtDaysInYear xrtRand32) do (
    findstr /C:"%%P(" release\include\xrt_decl.h >nul
    if errorlevel 1 exit /b 1
)
release\xl.exe run release\examples\stdlib_math.xl !XL6_XL_DEFAULT_LINK_ARGS! > build\test\stdlib_math_xlang.out.txt
if errorlevel 1 exit /b 1
for %%F in (stdlib_math) do (
    "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! release\examples\%%F_c.c !XL6_CGEN_LINKS! -o build\test\%%F_c.exe
    if errorlevel 1 exit /b 1
    build\test\%%F_c.exe > build\test\%%F_c.out.txt
    if errorlevel 1 exit /b 1
    fc /b build\test\%%F_xlang.out.txt build\test\%%F_c.out.txt >nul
    if errorlevel 1 exit /b 1
)
if "!XL6_EXTENDED_STDLIB_RUNTIME!"=="1" (
    release\xl.exe run release\examples\stdlib_string.xl !XL6_XL_DEFAULT_LINK_ARGS! > build\test\stdlib_string_xlang.out.txt
    if errorlevel 1 exit /b 1
    release\xl.exe run release\examples\stdlib_time.xl !XL6_XL_DEFAULT_LINK_ARGS! > build\test\stdlib_time_xlang.out.txt
    if errorlevel 1 exit /b 1
    for %%F in (stdlib_string stdlib_time) do (
        "!XL6_CGEN_CC!" !XL6_CGEN_FLAGS! !XL6_CGEN_INCLUDES! release\examples\%%F_c.c !XL6_CGEN_LINKS! -o build\test\%%F_c.exe
        if errorlevel 1 exit /b 1
        build\test\%%F_c.exe > build\test\%%F_c.out.txt
        if errorlevel 1 exit /b 1
        fc /b build\test\%%F_xlang.out.txt build\test\%%F_c.out.txt >nul
        if errorlevel 1 exit /b 1
    )
)
release\xl.exe build release\examples\stdlib_math.xl -o build\test\stdlib_math_trim.exe
if errorlevel 1 exit /b 1
build\test\stdlib_math_trim.exe > build\test\stdlib_math_trim.out.txt
if errorlevel 1 exit /b 1
findstr /C:"constants=ok" build\test\stdlib_math_trim.out.txt >nul
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 9/10
exit /b 0
