@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: current core ownership regressions...

for %%F in (test\cases\index_ownership*.xl) do (
    release\xl.exe check "%%F"
    if errorlevel 1 exit /b 1
    release\xl.exe run "%%F"
    if errorlevel 1 exit /b 1
    release\xl.exe build "%%F" -o "build\test\%%~nF.exe"
    if errorlevel 1 exit /b 1
    "build\test\%%~nF.exe"
    if errorlevel 1 exit /b 1
)

release\xl.exe cgen test\cases\index_ownership_top_level_var_return.xl --emit build\test\index_ownership_top_level_var_return.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_xrt_value_to_i64_release(xvalue value)" build\test\index_ownership_top_level_var_return.c >nul
if errorlevel 1 (
    echo index_ownership_top_level_var_return.c missing xvalue scalar bridge definition.
    exit /b 1
)
findstr /C:"xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xvoUnref);" build\test\index_ownership_top_level_var_return.c >nul
if errorlevel 1 (
    echo index_ownership_top_level_var_return.c bypassed unified temporary release.
    exit /b 1
)
powershell.exe -NoProfile -Command "$count = (Select-String -Path 'build\test\index_ownership_top_level_var_return.c' -Pattern '^static long long xl6_xrt_value_to_i64_release\(').Count; if ($count -ne 1) { exit 1 }"
if errorlevel 1 (
    echo index_ownership_top_level_var_return.c emitted duplicate scalar bridge definitions.
    exit /b 1
)

release\xl.exe check test\cases\hir_xvalue_scalar_bridge_matrix.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\hir_xvalue_scalar_bridge_matrix.xl --emit build\test\hir_xvalue_scalar_bridge_matrix.c
if errorlevel 1 exit /b 1
findstr /C:"xrtCopyStr(((const char*)xl6_runtime_value_as_string(value" build\test\hir_xvalue_scalar_bridge_matrix.c >nul
if errorlevel 1 (
    echo hir_xvalue_scalar_bridge_matrix.c missing single narrowed string conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(((const char*)xl6_runtime_value_as_string" build\test\hir_xvalue_scalar_bridge_matrix.c >nul
if not errorlevel 1 (
    echo hir_xvalue_scalar_bridge_matrix.c emitted a double xvalue string conversion.
    exit /b 1
)
release\xl.exe run test\cases\hir_xvalue_scalar_bridge_matrix.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\hir_xvalue_scalar_bridge_matrix.xl -o build\test\hir_xvalue_scalar_bridge_matrix.exe
if errorlevel 1 exit /b 1
build\test\hir_xvalue_scalar_bridge_matrix.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_global_identifier_collision.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_global_identifier_collision.xl --emit build\test\cgen_global_identifier_collision.c
if errorlevel 1 exit /b 1
findstr /C:"static void* __xl6_user_global_ptr;" build\test\cgen_global_identifier_collision.c >nul
if errorlevel 1 (
    echo cgen_global_identifier_collision.c missing internal point global symbol.
    exit /b 1
)
findstr /C:"static XL6_Function __xl6_user_global_xrtCall;" build\test\cgen_global_identifier_collision.c >nul
if errorlevel 1 (
    echo cgen_global_identifier_collision.c missing internal function-value global symbol.
    exit /b 1
)
findstr /C:"xl6_channel_send_value(__xl6_user_global_xrtChannelSend" build\test\cgen_global_identifier_collision.c >nul
if errorlevel 1 (
    echo cgen_global_identifier_collision.c did not escape the channel receiver global.
    exit /b 1
)
release\xl.exe run test\cases\cgen_global_identifier_collision.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_global_identifier_collision.xl -o build\test\cgen_global_identifier_collision.exe
if errorlevel 1 exit /b 1
build\test\cgen_global_identifier_collision.exe
if errorlevel 1 exit /b 1

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

release\xl.exe run test\cases\anonymous_functions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\anonymous_functions.xl -o build\test\anonymous_functions.exe
if errorlevel 1 exit /b 1
build\test\anonymous_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_multiline_anonymous_call_arg.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_multiline_anonymous_call_arg.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_multiline_anonymous_call_arg.xl -o build\test\cgen_multiline_anonymous_call_arg.exe
if errorlevel 1 exit /b 1
build\test\cgen_multiline_anonymous_call_arg.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_nested_anonymous_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_nested_anonymous_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_nested_anonymous_metadata.xl -o build\test\cgen_nested_anonymous_metadata.exe
if errorlevel 1 exit /b 1
build\test\cgen_nested_anonymous_metadata.exe
if errorlevel 1 exit /b 1

for %%F in (
    hir_closure_nested_capture
    hir_closure_shadow_capture
    hir_closure_for_capture
    hir_closure_for_string_capture
    hir_closure_for_named_typed_array
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\cgen_function_value_error_propagation.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_function_value_error_propagation.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_function_value_error_propagation.xl -o build\test\cgen_function_value_error_propagation.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_value_error_propagation.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_float_to_int_range_error.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_float_to_int_range_error.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_float_to_int_range_error.xl -o build\test\cgen_float_to_int_range_error.exe
if errorlevel 1 exit /b 1
build\test\cgen_float_to_int_range_error.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_c_macro_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_c_macro_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_c_macro_identifiers.xl -o build\test\cgen_c_macro_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_c_macro_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\runtime_uint64_format.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_uint64_format.xl > build\test\runtime_uint64_format.run.txt
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "if (-not ((Get-Content -Encoding UTF8 'build\test\runtime_uint64_format.run.txt') -contains '9223372036854775808')) { exit 1 }"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "if (-not ((Get-Content -Encoding UTF8 'build\test\runtime_uint64_format.run.txt') -contains '18446744073709551615')) { exit 1 }"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\runtime_uint64_format.xl -o build\test\runtime_uint64_format.exe
if errorlevel 1 exit /b 1
build\test\runtime_uint64_format.exe > build\test\runtime_uint64_format.exe.txt
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "if (-not ((Get-Content -Encoding UTF8 'build\test\runtime_uint64_format.exe.txt') -contains '9223372036854775808')) { exit 1 }"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "if (-not ((Get-Content -Encoding UTF8 'build\test\runtime_uint64_format.exe.txt') -contains '18446744073709551615')) { exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_integer_division_errors.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_integer_division_errors.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_integer_division_errors.xl -o build\test\cgen_integer_division_errors.exe
if errorlevel 1 exit /b 1
build\test\cgen_integer_division_errors.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\runtime_integer_division_uncaught.xl > build\test\runtime_integer_division_uncaught.out.txt 2> build\test\runtime_integer_division_uncaught.err.txt
if not errorlevel 1 (
    echo runtime_integer_division_uncaught.xl should fail.
    exit /b 1
)
for %%I in (build\test\runtime_integer_division_uncaught.out.txt) do if not "%%~zI"=="0" (
    echo runtime_integer_division_uncaught.xl printed a fallback value after raising.
    exit /b 1
)
powershell.exe -NoProfile -Command "if (-not (Select-String -Quiet -SimpleMatch 'ZeroDivisionError' 'build\test\runtime_integer_division_uncaught.err.txt')) { exit 1 }"
if errorlevel 1 (
    echo runtime_integer_division_uncaught.err.txt missing ZeroDivisionError.
    exit /b 1
)

release\xl.exe run test\cases\c_run_exit_code.c
set "C_RUN_EXIT=%ERRORLEVEL%"
if not "%C_RUN_EXIT%"=="37" (
    echo c_run_exit_code.c returned %C_RUN_EXIT%, expected 37.
    exit /b 1
)

release\xl.exe check test\cases\cgen_implicit_string_methods.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_implicit_string_methods.xl --emit build\test\cgen_implicit_string_methods.c
if errorlevel 1 exit /b 1
findstr /C:"__xl_std_string_trim(" build\test\cgen_implicit_string_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_string_methods.c missing implicit trim lowering.
    exit /b 1
)
findstr /C:"__xl_std_string_lower(__xl_std_string_trim(" build\test\cgen_implicit_string_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_string_methods.c missing chained implicit string lowering.
    exit /b 1
)
release\xl.exe run test\cases\cgen_implicit_string_methods.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_implicit_string_methods.xl -o build\test\cgen_implicit_string_methods.exe
if errorlevel 1 exit /b 1
build\test\cgen_implicit_string_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_generic_class_return_self.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generic_class_return_self.xl --emit build\test\cgen_generic_class_return_self.c
if errorlevel 1 exit /b 1
findstr /C:"xvoGetRecordOwner(self)" build\test\cgen_generic_class_return_self.c >nul
if errorlevel 1 (
    echo cgen_generic_class_return_self.c missing owned self return.
    exit /b 1
)
findstr /C:"xvalue __xl6_call_receiver_" build\test\cgen_generic_class_return_self.c >nul
if errorlevel 1 (
    echo cgen_generic_class_return_self.c missing chained class receiver temporary.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_call_receiver_" build\test\cgen_generic_class_return_self.c >nul
if errorlevel 1 (
    echo cgen_generic_class_return_self.c missing chained class receiver cleanup.
    exit /b 1
)
release\xl.exe run test\cases\cgen_generic_class_return_self.xl test\cases\cgen_class_host.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_generic_class_return_self.xl test\cases\cgen_class_host.c -o build\test\cgen_generic_class_return_self.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_return_self.exe
if errorlevel 1 exit /b 1

for %%F in (
    cgen_inline_container_for_in
    cgen_shift_range_errors
    cgen_typed_container_var_identity
    hir_stdlib_array_conversion
    hir_array_conversion_direct
    hir_persistence_typed_container_matrix
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe cgen test\cases\cgen_typed_container_var_identity.xl --emit build\test\cgen_typed_container_var_identity.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_container_box_ref(" build\test\cgen_typed_container_var_identity.c >nul
if errorlevel 1 (
    echo cgen_typed_container_var_identity.c lost handle-preserving boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_container_snapshot(" build\test\cgen_typed_container_var_identity.c >nul
if errorlevel 1 (
    echo cgen_typed_container_var_identity.c lost stable for-in snapshot lowering.
    exit /b 1
)

echo DEMO6 suite ok: current core ownership regressions
exit /b 0
