@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: typed HIR, nullable, weak, and definite assignment...

for %%F in (
    hir_nullable_types_check
    hir_nullable_parse_probe
    hir_modern_ownership_matrix
    hir_flow_value_ownership_matrix
    typed_container_iteration_ownership
    typed_nullable_container_matrix
    nullable_expression_semantics
    nullable_flow_narrowing
    nullable_function_frames
    nullable_function_lifecycle
    nullable_runtime_cast
    nullable_safe_navigation
    nullable_storage_matrix
    nullable_string_lifecycle
    weak_class_lifecycle
    weak_async_lifecycle
    definite_assignment_branches
    definite_assignment_default_values
    definite_assignment_flow
    cgen_base_method_typed_hir
    cgen_typed_array_class_return
    cgen_context_type_text_matrix
    cgen_repeated_probe
    runtime_is_flow_narrowing
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\cgen_threaded_context_matrix.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_threaded_context_matrix.xl --emit build\test\cgen_threaded_context_matrix.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_temp_managed_state_existing" build\test\cgen_threaded_context_matrix.c >nul
if errorlevel 1 exit /b 1
findstr /C:"host_thread_checkpoint(0LL, 4LL);" build\test\cgen_threaded_context_matrix.c >nul
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$t=[IO.File]::ReadAllText('build/test/cgen_threaded_context_matrix.c');$p=$t.IndexOf('void xl_script_module_unit(void)');if($p -lt 0 -or $t.Substring($p).Contains('__xl6_match_value_')){exit 1}"
if errorlevel 1 (
    echo module cleanup retained a transient match value.
    exit /b 1
)

for %%F in (
    hir_nullable_nonnull_class_error
    hir_nullable_scalar_error
    hir_nullable_spelling_error
    hir_weak_nonclass_error
    sem_definite_assignment_branch_error
    sem_definite_assignment_read_error
    sem_nullable_conversion_requires_unwrap
    sem_nullable_function_direct_call_error
    bad_nullable_guard_reassign
) do (
    release\xl.exe check test\cases\%%F.xl > build\test\%%F.err.txt 2>&1
    if not errorlevel 1 (
        echo %%F.xl should fail.
        exit /b 1
    )
    findstr /C:"[sem]" build\test\%%F.err.txt >nul
    if errorlevel 1 (
        echo %%F.xl missing semantic diagnostic.
        type build\test\%%F.err.txt
        exit /b 1
    )
)

release\xl.exe ir test\cases\hir_nullable_types_check.xl --emit build\test\hir_nullable_types_check.xlir
if errorlevel 1 exit /b 1
findstr /C:"nullable<__class_handle<Node>>" build\test\hir_nullable_types_check.xlir >nul
if errorlevel 1 exit /b 1
findstr /C:"weak<__class_handle<Node>>" build\test\hir_nullable_types_check.xlir >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\nullable_string_lifecycle.xl --emit build\test\nullable_string_lifecycle.c
if errorlevel 1 exit /b 1
findstr /C:"= xl6_nullable_string_copy(__xl6_return_" build\test\nullable_string_lifecycle.c >nul
if errorlevel 1 (
    echo nullable string return was not copied before local cleanup.
    exit /b 1
)

release\xl.exe cgen test\cases\typed_nullable_container_matrix.xl --emit build\test\typed_nullable_container_matrix.c
if errorlevel 1 exit /b 1
findstr /C:"_owned_copy" build\test\typed_nullable_container_matrix.c >nul
if errorlevel 1 (
    echo typed nullable container reads did not establish owned values.
    exit /b 1
)
findstr /C:"firstProtocol = xl6_nullable_ops_" build\test\typed_nullable_container_matrix.c >nul
if errorlevel 1 exit /b 1
findstr /C:"owned_copy(*(xvalue*)xl6_xrt_typed_require_item" build\test\typed_nullable_container_matrix.c >nul
if errorlevel 1 (
    echo nullable protocol container item did not use xvalue storage.
    exit /b 1
)
powershell -NoProfile -Command "$t=[IO.File]::ReadAllText('build/test/typed_nullable_container_matrix.c');if($t -match 'xrtTyped(Array|List|Set|Dict)Create\(0(?:,|\))'){Write-Host 'typed nullable container used a null TypeDesc';exit 1};$m=[regex]::Matches($t,'static const xrt_type_desc\* (xl6_runtime_type_desc_[0-9A-F]+)\(void\) \{');$g=$m|%%{$_.Groups[1].Value}|Group-Object|?{$_.Count -ne 1};if($g){Write-Host 'nullable TypeDesc was emitted more than once';exit 1}"
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\hir_flow_value_ownership_matrix.xl --emit build\test\hir_flow_value_ownership_matrix.c
if errorlevel 1 exit /b 1
findstr /C:"boxed = xl6_enum_op_box_value_" build\test\hir_flow_value_ownership_matrix.c >nul
if errorlevel 1 (
    echo generic enum was not boxed through its runtime type operation.
    exit /b 1
)
findstr /C:"copied = xrtTypedArrayRetain(" build\test\hir_flow_value_ownership_matrix.c >nul
if errorlevel 1 (
    echo nested typed array read did not retain its managed handle.
    exit /b 1
)
powershell -NoProfile -Command "$t=[IO.File]::ReadAllText('build/test/hir_flow_value_ownership_matrix.c');if($t -match 'xrtTyped(Array|List|Dict)Create\(0(?:,|\))'){Write-Host 'cross ownership matrix used a null TypeDesc';exit 1}"
if errorlevel 1 exit /b 1
"%XL6_BOOTSTRAP_CC%" -std=c11 -Werror=incompatible-pointer-types -Ibuild\test -fsyntax-only build\test\hir_flow_value_ownership_matrix.c
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\tutorial\13_modern_type_semantics.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\tutorial\13_modern_type_semantics.xl
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: typed HIR, nullable, weak, and definite assignment
exit /b 0
