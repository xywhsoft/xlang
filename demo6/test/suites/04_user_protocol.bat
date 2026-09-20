@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: user protocols...

for %%F in (
    protocol_declaration
    protocol_dynamic_dispatch
    protocol_runtime_is_as
    protocol_value_lifecycle
    protocol_container_matrix
    protocol_weak_lifecycle
    protocol_async_dispatch
    protocol_generic_dispatch
    protocol_generic_class_dispatch
    protocol_generic_constraint
    protocol_constraint_widening_matrix
    protocol_generic_property_dispatch
    protocol_inherited_dispatch
    protocol_overload_dispatch
    protocol_property_dispatch
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\sem_protocol_constraint_widen_unrelated_error.xl > build\test\sem_protocol_constraint_widen_unrelated_error.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_constraint_widen_unrelated_error.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_protocol_constraint_widen_unrelated_error.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_constraint_widen_unrelated_error.xl missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\protocol_native_backed_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\protocol_native_backed_dispatch.xl test\cases\protocol_native_backed_host.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\protocol_native_backed_dispatch.xl test\cases\protocol_native_backed_host.c -o build\test\protocol_native_backed_dispatch.exe
if errorlevel 1 exit /b 1
build\test\protocol_native_backed_dispatch.exe
if errorlevel 1 exit /b 1

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xruntime_protocol_registry.c %XL6_CGEN_LINKS% -o build\test\xruntime_protocol_registry.exe
if errorlevel 1 exit /b 1
build\test\xruntime_protocol_registry.exe
if errorlevel 1 exit /b 1

for %%F in (
    sem_protocol_async_mismatch_error
    sem_protocol_constraint_missing_member_error
    sem_protocol_generic_arity_mismatch_error
    sem_protocol_private_witness_error
) do (
    release\xl.exe check test\cases\%%F.xl > build\test\%%F.err.txt 2>&1
    if not errorlevel 1 (
        echo %%F.xl should fail.
        exit /b 1
    )
    findstr /C:"does not satisfy constraint" build\test\%%F.err.txt >nul
    if errorlevel 1 (
        echo %%F.xl missing protocol constraint diagnostic.
        exit /b 1
    )
)

for %%F in (
    sem_protocol_default_parameter_error
    sem_protocol_duplicate_requirement_error
) do (
    release\xl.exe check test\cases\%%F.xl > build\test\%%F.err.txt 2>&1
    if not errorlevel 1 (
        echo %%F.xl should fail.
        exit /b 1
    )
    findstr /C:"[sem]" build\test\%%F.err.txt >nul
    if errorlevel 1 (
        echo %%F.xl missing semantic diagnostic.
        exit /b 1
    )
)

release\xl.exe check test\cases\sem_protocol_overload_ambiguous_error.xl > build\test\sem_protocol_overload_ambiguous_error.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_overload_ambiguous_error.xl should fail.
    exit /b 1
)
findstr /C:"protocol method call is ambiguous" build\test\sem_protocol_overload_ambiguous_error.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_overload_ambiguous_error.xl missing overload diagnostic.
    exit /b 1
)

release\xl.exe abi test\cases\protocol_declaration.xl --emit build\test\protocol_declaration.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"protocol Writer" build\test\protocol_declaration.abi.txt >nul
if errorlevel 1 (
    echo protocol ABI text metadata is missing.
    exit /b 1
)

release\xl.exe abi test\cases\protocol_declaration.xl --json --emit build\test\protocol_declaration.abi.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"protocol\"" build\test\protocol_declaration.abi.json >nul
if errorlevel 1 (
    echo protocol ABI JSON metadata is missing.
    exit /b 1
)

echo DEMO6 suite ok: user protocols
exit /b 0
