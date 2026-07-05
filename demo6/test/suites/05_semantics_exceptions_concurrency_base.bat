@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: semantic negatives exceptions sync thread async...

release\xl.exe check test\cases\sem_logical_operand_type.xl >nul 2> release\tmp\sem_logical_operand_type.err.txt
if not errorlevel 1 (
    echo sem_logical_operand_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_outside_func.xl >nul 2> release\tmp\sem_return_outside_func.err.txt
if not errorlevel 1 (
    echo sem_return_outside_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_value_in_no_return.xl >nul 2> release\tmp\sem_return_value_in_no_return.err.txt
if not errorlevel 1 (
    echo sem_return_value_in_no_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_missing_value.xl >nul 2> release\tmp\sem_return_missing_value.err.txt
if not errorlevel 1 (
    echo sem_return_missing_value.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_multi_count.xl >nul 2> release\tmp\sem_return_multi_count.err.txt
if not errorlevel 1 (
    echo sem_return_multi_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" release\tmp\sem_return_multi_count.err.txt >nul
if errorlevel 1 (
    echo sem_return_multi_count.xl missing return count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_count.xl >nul 2> release\tmp\sem_multi_decl_count.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" release\tmp\sem_multi_decl_count.err.txt >nul
if errorlevel 1 (
    echo sem_multi_decl_count.xl missing multi declaration count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_rhs.xl >nul 2> release\tmp\sem_multi_decl_rhs.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_rhs.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_assign_count.xl >nul 2> release\tmp\sem_multi_assign_count.err.txt
if not errorlevel 1 (
    echo sem_multi_assign_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" release\tmp\sem_multi_assign_count.err.txt >nul
if errorlevel 1 (
    echo sem_multi_assign_count.xl missing multi assignment count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_assign_type_mismatch.xl >nul 2> release\tmp\sem_multi_assign_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_multi_assign_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_multi_assign_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_multi_assign_type_mismatch.xl missing multi-return variable assignment type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_decl.xl >nul 2> release\tmp\sem_multi_single_decl.err.txt
if not errorlevel 1 (
    echo sem_multi_single_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_assign.xl >nul 2> release\tmp\sem_multi_single_assign.err.txt
if not errorlevel 1 (
    echo sem_multi_single_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_arg.xl >nul 2> release\tmp\sem_multi_return_arg.err.txt
if not errorlevel 1 (
    echo sem_multi_return_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_single.xl >nul 2> release\tmp\sem_multi_return_single.err.txt
if not errorlevel 1 (
    echo sem_multi_return_single.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_named_order.xl >nul 2> release\tmp\sem_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_duplicate_named.xl >nul 2> release\tmp\sem_call_duplicate_named.err.txt
if not errorlevel 1 (
    echo sem_call_duplicate_named.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_arg_type.xl >nul 2> release\tmp\sem_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_call_arg_type.xl missing function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_missing_required.xl >nul 2> release\tmp\sem_call_missing_required.err.txt
if not errorlevel 1 (
    echo sem_call_missing_required.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" release\tmp\sem_call_missing_required.err.txt >nul
if errorlevel 1 (
    echo sem_call_missing_required.xl missing function argument count context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_call_missing_required.err.txt >nul
if not errorlevel 1 (
    echo sem_call_missing_required.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_order.xl >nul 2> release\tmp\sem_param_order.err.txt
if not errorlevel 1 (
    echo sem_param_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_position.xl >nul 2> release\tmp\sem_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_arg_redeclare.xl >nul 2> release\tmp\sem_vararg_arg_redeclare.err.txt
if not errorlevel 1 (
    echo sem_vararg_arg_redeclare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_bad_name.xl >nul 2> release\tmp\sem_function_type_bad_name.err.txt
if not errorlevel 1 (
    echo sem_function_type_bad_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_vararg_position.xl >nul 2> release\tmp\sem_function_type_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_function_type_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_signature_mismatch.xl >nul 2> release\tmp\sem_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo sem_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"target function, value function" release\tmp\sem_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt missing function signature type context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_function_signature_mismatch.xl >nul 2> release\tmp\cgen_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\cgen_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"target function, value function" release\tmp\cgen_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt missing function signature type context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\cgen_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_count_missing.xl >nul 2> release\tmp\sem_function_call_arg_count_missing.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_count_missing.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" release\tmp\sem_function_call_arg_count_missing.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_count_missing.xl missing function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_count_extra.xl >nul 2> release\tmp\sem_function_call_arg_count_extra.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_count_extra.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" release\tmp\sem_function_call_arg_count_extra.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_count_extra.xl missing function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_type.xl >nul 2> release\tmp\sem_function_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_function_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_type.xl missing function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_function_field_call_arg_type.xl >nul 2> release\tmp\sem_member_function_field_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_member_function_field_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_member_function_field_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_function_field_call_arg_type.xl missing member function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_named_arg.xl >nul 2> release\tmp\sem_function_bare_named_arg.err.txt
if not errorlevel 1 (
    echo sem_function_bare_named_arg.xl should fail.
    exit /b 1
)
findstr /C:"func" release\tmp\sem_function_bare_named_arg.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt missing func diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_bare_named_arg.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_named_order.xl >nul 2> release\tmp\sem_function_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_function_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_overload_name.xl >nul 2> release\tmp\sem_function_bare_overload_name.err.txt
if not errorlevel 1 (
    echo sem_function_bare_overload_name.xl should fail.
    exit /b 1
)
findstr /C:"func" release\tmp\sem_function_bare_overload_name.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt missing func overload diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_bare_overload_name.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
findstr /C:"target function, value <unknown>" release\tmp\sem_function_bare_overload_name.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt regressed to cascaded unknown-value mismatch.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_multi_return_arg_type.xl >nul 2> release\tmp\sem_function_multi_return_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_multi_return_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_function_multi_return_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_function_multi_return_arg_type.xl missing multi-return function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_type_mismatch.xl >nul 2> release\tmp\sem_return_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_return_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_return_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_return_type_mismatch.xl missing return type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_default_type.xl >nul 2> release\tmp\sem_param_default_type.err.txt
if not errorlevel 1 (
    echo sem_param_default_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_param_default_type.err.txt >nul
if errorlevel 1 (
    echo sem_param_default_type.xl missing parameter default type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_convert_invalid.xl >nul 2> release\tmp\sem_type_convert_invalid.err.txt
if not errorlevel 1 (
    echo sem_type_convert_invalid.xl should fail.
    exit /b 1
)
findstr /C:"target point, source int" release\tmp\sem_type_convert_invalid.err.txt >nul
if errorlevel 1 (
    echo sem_type_convert_invalid.xl missing type conversion context.
    exit /b 1
)

release\xl.exe check test\cases\sem_anon_capture_assign.xl >nul 2> release\tmp\sem_anon_capture_assign.err.txt
if not errorlevel 1 (
    echo sem_anon_capture_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_func_capture_top_level.xl >nul 2> release\tmp\sem_func_capture_top_level.err.txt
if not errorlevel 1 (
    echo sem_func_capture_top_level.xl should fail.
    exit /b 1
)
powershell -NoProfile -Command "$needle = -join ([char[]](0x51FD,0x6570,0x4F53,0x4E0D,0x80FD,0x9690,0x5F0F,0x6355,0x83B7,0x5916,0x5C42,0x5C40,0x90E8,0x53D8,0x91CF)); if (-not (Select-String -LiteralPath 'release\tmp\sem_func_capture_top_level.err.txt' -SimpleMatch $needle)) { exit 1 }"
if errorlevel 1 (
    echo sem_func_capture_top_level.xl missing implicit capture diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_capture_top_level.xl >nul 2> release\tmp\sem_async_capture_top_level.err.txt
if not errorlevel 1 (
    echo sem_async_capture_top_level.xl should fail.
    exit /b 1
)
powershell -NoProfile -Command "$needle = -join ([char[]](0x51FD,0x6570,0x4F53,0x4E0D,0x80FD,0x9690,0x5F0F,0x6355,0x83B7,0x5916,0x5C42,0x5C40,0x90E8,0x53D8,0x91CF)); if (-not (Select-String -LiteralPath 'release\tmp\sem_async_capture_top_level.err.txt' -SimpleMatch $needle)) { exit 1 }"
if errorlevel 1 (
    echo sem_async_capture_top_level.xl missing implicit capture diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_destroy_class.xl >nul 2> release\tmp\sem_closure_capture_destroy_class.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_destroy_class.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_custom.xl >nul 2> release\tmp\sem_closure_capture_custom.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_custom.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_custom_field.xl >nul 2> release\tmp\sem_closure_capture_custom_field.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_custom_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_class_copy_init.xl >nul 2> release\tmp\sem_noncopyable_class_copy_init.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_class_copy_init.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_assign.xl >nul 2> release\tmp\sem_noncopyable_assign.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_custom_param.xl >nul 2> release\tmp\sem_noncopyable_custom_param.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_custom_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_fresh_return.xl >nul
if errorlevel 1 (
    echo sem_noncopyable_fresh_return.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_return.xl >nul 2> release\tmp\sem_noncopyable_return.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_custom.xl >nul 2> release\tmp\sem_typed_container_noncopyable_custom.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_custom.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_custom.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_custom.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_class.xl >nul 2> release\tmp\sem_typed_container_noncopyable_class.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_class.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_class.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_class.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_nested.xl >nul 2> release\tmp\sem_typed_container_noncopyable_nested.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_nested.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_nested.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_nested.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_unknown_base.xl >nul 2> release\tmp\sem_class_unknown_base.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_base.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_create_missing.xl >nul 2> release\tmp\sem_class_base_create_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_create_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_default_missing.xl >nul 2> release\tmp\sem_class_base_default_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_default_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_outside_create.xl >nul 2> release\tmp\sem_class_base_outside_create.err.txt
if not errorlevel 1 (
    echo sem_class_base_outside_create.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_overload_missing.xl >nul 2> release\tmp\sem_class_base_overload_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_overload_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_ctor_arity.xl >nul 2> release\tmp\sem_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_class_ctor_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 0" release\tmp\sem_class_ctor_arity.err.txt >nul
if errorlevel 1 (
    echo sem_class_ctor_arity.err.txt missing create argument count context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_ctor_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_class_ctor_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_default_ctor_args.xl >nul 2> release\tmp\sem_class_default_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_class_default_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"expected 0, actual 1" release\tmp\sem_class_default_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt missing default constructor count context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_default_ctor_args.err.txt >nul
if not errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_destroy_params.xl >nul 2> release\tmp\sem_class_destroy_params.err.txt
if not errorlevel 1 (
    echo sem_class_destroy_params.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_conflict.xl >nul 2> release\tmp\sem_class_field_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_field_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_default_type.xl >nul 2> release\tmp\sem_class_field_default_type.err.txt
if not errorlevel 1 (
    echo sem_class_field_default_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_class_field_default_type.err.txt >nul
if errorlevel 1 (
    echo sem_class_field_default_type.err.txt missing field default type context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_field_default_type.err.txt >nul
if not errorlevel 1 (
    echo sem_class_field_default_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_method_overload_conflict.xl >nul 2> release\tmp\sem_class_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_private_access.xl >nul 2> release\tmp\sem_class_private_access.err.txt
if not errorlevel 1 (
    echo sem_class_private_access.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_protected_access.xl >nul 2> release\tmp\sem_class_protected_access.err.txt
if not errorlevel 1 (
    echo sem_class_protected_access.xl should fail.
    exit /b 1
)
findstr /C:"protected class member is not accessible" release\tmp\sem_class_protected_access.err.txt >nul
if errorlevel 1 (
    echo sem_class_protected_access.xl missing protected access diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_access_conflict.xl >nul 2> release\tmp\sem_class_access_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_access_conflict.xl should fail.
    exit /b 1
)
findstr /C:"public, private or protected" release\tmp\sem_class_access_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_class_access_conflict.xl missing access conflict diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_static_self.xl >nul 2> release\tmp\sem_class_static_self.err.txt
if not errorlevel 1 (
    echo sem_class_static_self.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_missing.xl >nul 2> release\tmp\sem_class_override_missing.err.txt
if not errorlevel 1 (
    echo sem_class_override_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_nonvirtual.xl >nul 2> release\tmp\sem_class_override_nonvirtual.err.txt
if not errorlevel 1 (
    echo sem_class_override_nonvirtual.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_final.xl >nul 2> release\tmp\sem_class_override_final.err.txt
if not errorlevel 1 (
    echo sem_class_override_final.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_self_assign.xl >nul 2> release\tmp\sem_self_assign.err.txt
if not errorlevel 1 (
    echo sem_self_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\parse_removed_keyword.xl >nul 2> release\tmp\parse_removed_keyword.err.txt
if not errorlevel 1 (
    echo parse_removed_keyword.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" release\tmp\parse_removed_keyword.err.txt >nul
if errorlevel 1 (
    echo parse_removed_keyword.err.txt missing syntax diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_reserved_keyword_identifier.xl >nul 2> release\tmp\parse_reserved_keyword_identifier.err.txt
if not errorlevel 1 (
    echo parse_reserved_keyword_identifier.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" release\tmp\parse_reserved_keyword_identifier.err.txt >nul
if errorlevel 1 (
    echo parse_reserved_keyword_identifier.err.txt missing syntax diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_bad_special.xl >nul 2> release\tmp\sem_custom_bad_special.err.txt
if not errorlevel 1 (
    echo sem_custom_bad_special.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_missing_handle.xl >nul 2> release\tmp\sem_native_class_missing_handle.err.txt
if not errorlevel 1 (
    echo sem_native_class_missing_handle.xl should fail.
    exit /b 1
)
findstr /C:"native-backed class extern member requires native point handle field" release\tmp\sem_native_class_missing_handle.err.txt >nul
if errorlevel 1 (
    echo sem_native_class_missing_handle.err.txt missing native handle diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_missing_handle.xl >nul 2> release\tmp\sem_native_property_missing_handle.err.txt
if not errorlevel 1 (
    echo sem_native_property_missing_handle.xl should fail.
    exit /b 1
)
findstr /C:"native-backed class extern member requires native point handle field" release\tmp\sem_native_property_missing_handle.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_missing_handle.err.txt missing native handle diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_get_no_return.xl >nul 2> release\tmp\sem_native_property_get_no_return.err.txt
if not errorlevel 1 (
    echo sem_native_property_get_no_return.xl should fail.
    exit /b 1
)
findstr /C:"extern get property must declare exactly one return type" release\tmp\sem_native_property_get_no_return.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_get_no_return.err.txt missing extern get diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_let_no_value.xl >nul 2> release\tmp\sem_native_property_let_no_value.err.txt
if not errorlevel 1 (
    echo sem_native_property_let_no_value.xl should fail.
    exit /b 1
)
findstr /C:"extern let property must declare a value parameter" release\tmp\sem_native_property_let_no_value.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_let_no_value.err.txt missing extern let diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_optional.xl >nul 2> release\tmp\sem_native_property_optional.err.txt
if not errorlevel 1 (
    echo sem_native_property_optional.xl should fail.
    exit /b 1
)
findstr /C:"extern property parameters cannot be optional" release\tmp\sem_native_property_optional.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_optional.err.txt missing extern property optional diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_vararg.xl >nul 2> release\tmp\sem_native_property_vararg.err.txt
if not errorlevel 1 (
    echo sem_native_property_vararg.xl should fail.
    exit /b 1
)
findstr /C:"extern property parameters cannot be vararg" release\tmp\sem_native_property_vararg.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_vararg.err.txt missing extern property vararg diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_duplicate_c_symbol.xl >nul 2> release\tmp\sem_native_property_duplicate_c_symbol.err.txt
if not errorlevel 1 (
    echo sem_native_property_duplicate_c_symbol.xl should fail.
    exit /b 1
)
findstr /C:"same native-backed class cannot bind the same C symbol twice" release\tmp\sem_native_property_duplicate_c_symbol.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_duplicate_c_symbol.err.txt missing duplicate C symbol diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_duplicate_c_symbol.xl >nul 2> release\tmp\sem_custom_duplicate_c_symbol.err.txt
if not errorlevel 1 (
    echo sem_custom_duplicate_c_symbol.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_bind_non_point.xl >nul 2> release\tmp\sem_custom_bind_non_point.err.txt
if not errorlevel 1 (
    echo sem_custom_bind_non_point.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_create_point_bind_conflict.xl >nul 2> release\tmp\sem_custom_create_point_bind_conflict.err.txt
if not errorlevel 1 (
    echo sem_custom_create_point_bind_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_bad_type.xl >nul 2> release\tmp\sem_custom_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_bad_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_bad_type.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_bad_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_return_bad.xl >nul 2> release\tmp\sem_custom_extern_return_bad.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_return_bad.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_return_bad.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_return_bad.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_vararg.xl >nul 2> release\tmp\sem_custom_extern_vararg.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_vararg.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_vararg.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_vararg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_method_overload_conflict.xl >nul 2> release\tmp\sem_custom_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_custom_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_param.xl >nul 2> release\tmp\sem_auto_param.err.txt
if not errorlevel 1 (
    echo sem_auto_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_return.xl >nul 2> release\tmp\sem_auto_return.err.txt
if not errorlevel 1 (
    echo sem_auto_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_field.xl >nul 2> release\tmp\sem_auto_field.err.txt
if not errorlevel 1 (
    echo sem_auto_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_alias.xl >nul 2> release\tmp\sem_auto_alias.err.txt
if not errorlevel 1 (
    echo sem_auto_alias.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_function_type.xl >nul 2> release\tmp\sem_auto_function_type.err.txt
if not errorlevel 1 (
    echo sem_auto_function_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_void_type.xl >nul 2> release\tmp\sem_void_type.err.txt
if not errorlevel 1 (
    echo sem_void_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_as_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_type_builtin_name.xl >nul 2> release\tmp\sem_type_builtin_name.err.txt
if not errorlevel 1 (
    echo sem_type_builtin_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_duplicate_decl.xl >nul 2> release\tmp\sem_type_duplicate_decl.err.txt
if not errorlevel 1 (
    echo sem_type_duplicate_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_unknown_type.xl >nul 2> release\tmp\sem_unknown_type.err.txt
if not errorlevel 1 (
    echo sem_unknown_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_duplicate.xl >nul 2> release\tmp\sem_export_duplicate.err.txt
if not errorlevel 1 (
    echo sem_export_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_optional.xl >nul 2> release\tmp\sem_export_optional.err.txt
if not errorlevel 1 (
    echo sem_export_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_vararg.xl >nul 2> release\tmp\sem_export_vararg.err.txt
if not errorlevel 1 (
    echo sem_export_vararg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_var_init_type.xl >nul 2> release\tmp\sem_export_var_init_type.err.txt
if not errorlevel 1 (
    echo sem_export_var_init_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_export_var_init_type.err.txt >nul
if errorlevel 1 (
    echo sem_export_var_init_type.xl missing export variable initializer type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_string_return.xl >nul 2> release\tmp\sem_export_string_return.err.txt
if not errorlevel 1 (
    echo sem_export_string_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_generic.xl >nul 2> release\tmp\sem_export_generic.err.txt
if not errorlevel 1 (
    echo sem_export_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_static.xl >nul 2> release\tmp\sem_class_export_static.err.txt
if not errorlevel 1 (
    echo sem_class_export_static.xl should fail.
    exit /b 1
)
findstr /C:"class C ABI export member cannot be static" release\tmp\sem_class_export_static.err.txt >nul
if errorlevel 1 (
    echo sem_class_export_static.err.txt missing class export static diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_generic.xl >nul 2> release\tmp\sem_class_export_generic.err.txt
if not errorlevel 1 (
    echo sem_class_export_generic.xl should fail.
    exit /b 1
)
findstr /C:"class C ABI export member does not support generic class" release\tmp\sem_class_export_generic.err.txt >nul
if errorlevel 1 (
    echo sem_class_export_generic.err.txt missing class export generic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_duplicate.xl >nul 2> release\tmp\sem_class_export_duplicate.err.txt
if not errorlevel 1 (
    echo sem_class_export_duplicate.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$diag=([char]0x91CD)+([char]0x590D)+([char]0x7684)+' C export symbol'; $text=[System.IO.File]::ReadAllText('release\tmp\sem_class_export_duplicate.err.txt',[System.Text.Encoding]::UTF8); if($text.IndexOf($diag) -lt 0){ exit 1 }"
if errorlevel 1 (
    echo sem_class_export_duplicate.err.txt missing duplicate C export diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_arity.xl >nul 2> release\tmp\sem_generic_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" release\tmp\sem_generic_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_arity.xl missing generic argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_param_duplicate.xl >nul 2> release\tmp\sem_generic_param_duplicate.err.txt
if not errorlevel 1 (
    echo sem_generic_param_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_unknown_type_arg.xl >nul 2> release\tmp\sem_generic_unknown_type_arg.err.txt
if not errorlevel 1 (
    echo sem_generic_unknown_type_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_infer_conflict.xl >nul 2> release\tmp\sem_generic_infer_conflict.err.txt
if not errorlevel 1 (
    echo sem_generic_infer_conflict.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_generic_infer_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_generic_infer_conflict.xl missing generic inference conflict context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_func_inst_arity.xl >nul 2> release\tmp\sem_generic_func_inst_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_func_inst_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" release\tmp\sem_generic_func_inst_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_func_inst_arity.xl missing explicit generic function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_arity.xl >nul 2> release\tmp\sem_generic_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" release\tmp\sem_generic_class_ctor_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_class_ctor_arity.xl missing generic class constructor count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_open_ctor.xl >nul 2> release\tmp\sem_generic_class_open_ctor.err.txt
if not errorlevel 1 (
    echo sem_generic_class_open_ctor.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_type_mismatch.xl >nul 2> release\tmp\sem_generic_class_ctor_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_type_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_arity.xl >nul 2> release\tmp\sem_generic_class_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" release\tmp\sem_generic_class_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_class_arity.xl missing generic class type argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_extends.xl >nul 2> release\tmp\sem_generic_class_extends.err.txt
if not errorlevel 1 (
    echo sem_generic_class_extends.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_return_only.xl >nul 2> release\tmp\sem_overload_return_only.err.txt
if not errorlevel 1 (
    echo sem_overload_return_only.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_optional_conflict.xl >nul 2> release\tmp\sem_overload_optional_conflict.err.txt
if not errorlevel 1 (
    echo sem_overload_optional_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_vararg_same_kind.xl >nul 2> release\tmp\sem_overload_vararg_same_kind.err.txt
if not errorlevel 1 (
    echo sem_overload_vararg_same_kind.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_overload_conflict.xl >nul 2> release\tmp\sem_alias_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_alias_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_cycle.xl >nul 2> release\tmp\sem_alias_cycle.err.txt
if not errorlevel 1 (
    echo sem_alias_cycle.xl should fail.
    exit /b 1
)
findstr /C:"type alias cycle" release\tmp\sem_alias_cycle.err.txt >nul
if errorlevel 1 (
    echo sem_alias_cycle.xl should report alias cycle.
    exit /b 1
)
findstr /C:"target First" release\tmp\sem_alias_cycle.err.txt >nul
if not errorlevel 1 (
    echo sem_alias_cycle.xl should not report initializer cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_mismatch.xl >nul 2> release\tmp\sem_extern_c_symbol_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_kind_mismatch.xl >nul 2> release\tmp\sem_extern_c_symbol_kind_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_kind_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_bad_type.xl >nul 2> release\tmp\sem_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_extern_bad_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_multi_return.xl >nul 2> release\tmp\sem_extern_multi_return.err.txt
if not errorlevel 1 (
    echo sem_extern_multi_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_optional.xl >nul 2> release\tmp\sem_extern_optional.err.txt
if not errorlevel 1 (
    echo sem_extern_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_vararg.xl >nul 2> release\tmp\sem_extern_vararg.err.txt
if not errorlevel 1 (
    echo sem_extern_vararg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_generic.xl >nul 2> release\tmp\sem_extern_generic.err.txt
if not errorlevel 1 (
    echo sem_extern_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_concat_mismatch.xl >nul 2> release\tmp\sem_string_concat_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_concat_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"numeric operator operands must be numeric" release\tmp\sem_string_concat_mismatch.err.txt >nul
if not errorlevel 1 (
    echo sem_string_concat_mismatch.xl should not report numeric cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_assign_mismatch.xl >nul 2> release\tmp\sem_string_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_assign_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"string compound assignment operands must both be string" release\tmp\sem_string_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_string_assign_mismatch.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left string, right int" release\tmp\sem_string_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_string_assign_mismatch.xl missing compound diagnostic context.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_compare_mismatch.xl >nul 2> release\tmp\sem_string_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_bad.xl >nul 2> release\tmp\sem_time_literal_bad.err.txt
if not errorlevel 1 (
    echo sem_time_literal_bad.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_newline.xl >nul 2> release\tmp\sem_time_literal_newline.err.txt
if not errorlevel 1 (
    echo sem_time_literal_newline.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_assign_mismatch.xl >nul 2> release\tmp\sem_time_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_assign_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target time, value int" release\tmp\sem_time_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_time_assign_mismatch.xl missing declaration init type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_compare_mismatch.xl >nul 2> release\tmp\sem_time_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_custom_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_custom_assign.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_custom_arg.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_function_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_call_arg_destroy_class.xl >nul 2> release\tmp\sem_var_call_arg_destroy_class.err.txt
if not errorlevel 1 (
    echo sem_var_call_arg_destroy_class.xl should fail.
    exit /b 1
)
findstr /C:"target var, source Resource" release\tmp\sem_var_call_arg_destroy_class.err.txt >nul
if errorlevel 1 (
    echo sem_var_call_arg_destroy_class.xl missing var call argument boxing context.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_call_named_order.xl >nul 2> release\tmp\sem_var_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_var_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_destroy_class_init.xl >nul 2> release\tmp\sem_var_destroy_class_init.err.txt
if not errorlevel 1 (
    echo sem_var_destroy_class_init.xl should fail.
    exit /b 1
)
findstr /C:"target var, value Resource" release\tmp\sem_var_destroy_class_init.err.txt >nul
if errorlevel 1 (
    echo sem_var_destroy_class_init.xl missing constructor value type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_destroy_class_return.xl >nul 2> release\tmp\sem_var_destroy_class_return.err.txt
if not errorlevel 1 (
    echo sem_var_destroy_class_return.xl should fail.
    exit /b 1
)
findstr /C:"target var, value Resource" release\tmp\sem_var_destroy_class_return.err.txt >nul
if errorlevel 1 (
    echo sem_var_destroy_class_return.xl missing constructor value type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_ambiguous.xl >nul 2> release\tmp\sem_overload_ambiguous.err.txt
if not errorlevel 1 (
    echo sem_overload_ambiguous.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_literal.xl >nul 2> release\tmp\sem_typed_literal.err.txt
if not errorlevel 1 (
    echo sem_typed_literal.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_set_pair.xl >nul 2> release\tmp\sem_typed_set_pair.err.txt
if not errorlevel 1 (
    echo sem_typed_set_pair.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_value_mismatch.xl >nul 2> release\tmp\sem_type_value_mismatch.err.txt
if not errorlevel 1 (
    echo sem_type_value_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_compare_int.xl >nul 2> release\tmp\sem_type_compare_int.err.txt
if not errorlevel 1 (
    echo sem_type_compare_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_order_compare.xl >nul 2> release\tmp\sem_type_order_compare.err.txt
if not errorlevel 1 (
    echo sem_type_order_compare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_call_arity.xl >nul 2> release\tmp\sem_type_call_arity.err.txt
if not errorlevel 1 (
    echo sem_type_call_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_arity.xl >nul 2> release\tmp\sem_range_arity.err.txt
if not errorlevel 1 (
    echo sem_range_arity.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_range_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_range_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_step_zero.xl >nul 2> release\tmp\sem_range_step_zero.err.txt
if not errorlevel 1 (
    echo sem_range_step_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_in_three_targets.xl >nul 2> release\tmp\sem_for_in_three_targets.err.txt
if not errorlevel 1 (
    echo sem_for_in_three_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_for_in_three_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_for_in_three_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_await_three_targets.xl >nul 2> release\tmp\sem_for_await_three_targets.err.txt
if not errorlevel 1 (
    echo sem_for_await_three_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_for_await_three_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_for_await_three_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_exceptions.xl -o release\tmp\cgen_exceptions.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_exceptions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_exceptions.xl --emit release\tmp\cgen_exceptions.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise" release\tmp\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing error runtime raise.
    exit /b 1
)

release\xl.exe check test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_loop_exception_propagation.xl -o release\tmp\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_exception_propagation.xl --emit release\tmp\cgen_loop_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"loop boom\"" release\tmp\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop throw raise.
    exit /b 1
)

findstr /C:"goto __xl6_try_finally_" release\tmp\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop exception try/finally jump.
    exit /b 1
)

release\xl.exe run test\cases\cgen_exception_uncaught.xl >nul 2> release\tmp\cgen_exception_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_exception_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: uncaught" release\tmp\cgen_exception_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_exception_uncaught.err.txt missing uncaught exception diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_sync.xl -o release\tmp\cgen_sync.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_sync.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_sync.exe
    if errorlevel 1 (
        echo cgen_sync.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_sync.xl --emit release\tmp\cgen_sync.c
if errorlevel 1 exit /b 1

findstr /C:"xrtMutexCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtMutexCreate.
    exit /b 1
)
findstr /C:"xrtSemCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtSemCreate.
    exit /b 1
)
findstr /C:"xrtRWLockCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtRWLockCreate.
    exit /b 1
)
findstr /C:"xrtSemPostMultiple(gate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing sem postMultiple path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('int cappedOverflow = xrtSemPostMultiple(capped, 2LL)','int cappedOriginal = (xrtSemWaitTimeout(capped, 10LL) == 0)','int cappedPartial = xrtSemTryWait(capped)','!cappedOverflow','!cappedPartial','int fullPost = xrtSemPost(full)','int fullWaited = (xrtSemWaitTimeout(full, 10LL) == 0)','int refillPost = xrtSemPost(full)','int refillWaited = (xrtSemWaitTimeout(full, 10LL) == 0)','!fullPost'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing sem overflow/full guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtSemWaitTimeout(__env->semWakeGate, 2000LL) == 0','XL6_Function semWakeWorker = xl6_anon_','_make(semWakeGate)','xl6_thread_start(semWakeWorker, 1','xrtThreadWaitTimeout(semWakeThread, 1LL) == 0','xrtSemPost(semWakeGate)','xrtThreadWaitTimeout(semWakeThread, 2000LL) == 0','(int)xrtThreadGetExitCode(semWakeThread) == 23LL'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing cross-thread sem wake guard lowering.
    exit /b 1
)
findstr /C:"xrtCondWaitTimeout(c, condMutex" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing cond waitTimeout path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtSemWaitTimeout(__env->wakeReady, 2000LL) == 0','xrtMutexLock(__env->wakeMutex)','xrtCondSignal(__env->wakeCond)','xrtMutexUnlock(__env->wakeMutex)','xl6_thread_start(wakeWorker, 1','xrtSemPost(wakeReady)','int wakeSignaled = (xrtCondWaitTimeout(wakeCond, wakeMutex, 2000LL) == 0)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing cross-thread cond signal guard lowering.
    exit /b 1
)
findstr /C:"xrtRWLockTryReadLock(rw" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing rwlock tryReadLock path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('int invalidUpgrade = xrtRWLockUpgrade(upgradeRw)','xrtRWLockReadLock(upgradeRw)','int upgraded = xrtRWLockUpgrade(upgradeRw)','xrtRWLockWriteUnlock(upgradeRw)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing rwlock upgrade guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtRWLockWriteLock(__env->capturedRw)','xrtRWLockDowngrade(__env->capturedRw)','xrtRWLockReadUnlock(__env->capturedRw)','_make(capturedRw)','xrtThreadWaitTimeout(capturedRwThread, 2000LL) == 0','(int)xrtThreadGetExitCode(capturedRwThread) == 29LL','xrtRWLockTryWriteLock(capturedRw)','xrtRWLockWriteUnlock(capturedRw)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing captured rwlock receiver guard lowering.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtMutexCreate()" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing mutex constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtSemCreate(" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing sem constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtCondCreate()" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing cond constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtRWLockCreate()" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing rwlock constructor error guard.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_ctor_args.xl >nul 2> release\tmp\sem_sync_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_sync_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"sync constructor does not accept arguments" release\tmp\sem_sync_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_sync_ctor_args.xl missing sync constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_ctor_named_argument.xl >nul 2> release\tmp\sem_sync_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_sync_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"sync constructor accepts positional arguments only" release\tmp\sem_sync_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_sync_ctor_named_argument.xl missing sync constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_method_args.xl >nul 2> release\tmp\sem_sync_method_args.err.txt
if not errorlevel 1 (
    echo sem_sync_method_args.xl should fail.
    exit /b 1
)
findstr /C:"sync method does not accept arguments" release\tmp\sem_sync_method_args.err.txt >nul
if errorlevel 1 (
    echo sem_sync_method_args.xl missing sync method argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_wait_timeout_type.xl >nul 2> release\tmp\sem_sync_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_sync_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"sync method argument must be int" release\tmp\sem_sync_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_wait_timeout_type.xl missing sync timeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_post_multiple_type.xl >nul 2> release\tmp\sem_sync_post_multiple_type.err.txt
if not errorlevel 1 (
    echo sem_sync_post_multiple_type.xl should fail.
    exit /b 1
)
findstr /C:"sync method argument must be int" release\tmp\sem_sync_post_multiple_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_post_multiple_type.xl missing sync postMultiple type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_unknown_method.xl >nul 2> release\tmp\sem_sync_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_sync_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown sync method" release\tmp\sem_sync_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_sync_unknown_method.xl missing unknown sync method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_receiver_expr.xl >nul 2> release\tmp\sem_sync_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_sync_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"sync method receiver" release\tmp\sem_sync_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_sync_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_cond_mutex_arg.xl >nul 2> release\tmp\sem_sync_cond_mutex_arg.err.txt
if not errorlevel 1 (
    echo sem_sync_cond_mutex_arg.xl should fail.
    exit /b 1
)
findstr /C:"cond wait first argument must be mutex" release\tmp\sem_sync_cond_mutex_arg.err.txt >nul
if errorlevel 1 (
    echo sem_sync_cond_mutex_arg.xl missing cond mutex diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_cond_wait_timeout_type.xl >nul 2> release\tmp\sem_sync_cond_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_sync_cond_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"cond wait timeout must be int" release\tmp\sem_sync_cond_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_sync_cond_wait_timeout_type.xl missing cond timeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_copy.xl >nul 2> release\tmp\sem_sync_copy.err.txt
if not errorlevel 1 (
    echo sem_sync_copy.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread.xl -o release\tmp\cgen_thread.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_thread.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_thread.exe
    if errorlevel 1 (
        echo cgen_thread.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_thread.xl --emit release\tmp\cgen_thread.c
if errorlevel 1 exit /b 1

findstr /C:"xrtThreadCreate" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing xrtThreadCreate.
    exit /b 1
)
findstr /C:"xl6_thread_start" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread start helper.
    exit /b 1
)
findstr /C:"xl6_thread_require_handle(xl6_thread_start" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread constructor error guard.
    exit /b 1
)
findstr /C:"if ( xl6_error_has() ) { return 0; }" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread active error preservation guard.
    exit /b 1
)
findstr /C:"thread requires a callable func value" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread diagnostic.
    exit /b 1
)
findstr /C:"XL6_Function missingWorker = {0};" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread fixture.
    exit /b 1
)
findstr /C:"xl6_thread_start(missingWorker, 1" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing empty function value thread start call.
    exit /b 1
)
findstr /C:"thread start failed" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread start failure diagnostic.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_thread.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( fn\.invoke == 0 \) \{[\s\S]*?if \( !retainBorrow && fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?thread requires a callable func value' -or $text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( arg == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while starting thread' -or $text -notmatch 'static void\* xl6_thread_start\([\s\S]*?if \( pThread == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?xrtFree\(arg\);[\s\S]*?thread start failed'){ exit 1 }"
if errorlevel 1 (
    echo cgen_thread.c missing thread start failure cleanup.
    exit /b 1
)
findstr /C:"xl6_thread_release" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread release helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('void* slow = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_slowWorker_make(), 0','int slowImmediate = (xrtThreadWaitTimeout(slow, 0LL) == 0)','!slowImmediate','int slowWaited = (xrtThreadWaitTimeout(slow, 2000LL) == 0)','long long slowCode = (int)xrtThreadGetExitCode(slow)','int slowRepeatedWait = (xrtThreadWait(slow), true)','long long slowCodeAgain = (int)xrtThreadGetExitCode(slow)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing waitTimeout timeout/completed-state guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('conditionalBorrowedThread = xl6_thread_require_handle(xl6_thread_start((chooseBorrowedThread ? borrowedThreadLeft : borrowedThreadRight), 1','conditionalBorrowedThreadAgain = xl6_thread_require_handle(xl6_thread_start((chooseBorrowedThread ? borrowedThreadLeft : borrowedThreadRight), 1','mixedThreadOwned = xl6_thread_require_handle(xl6_thread_start((chooseMixedThread ? xl6_function_copy_value(mixedThreadSource) : makeThreadWorker(16LL)), 0','mixedThreadBorrowed = xl6_thread_require_handle(xl6_thread_start((chooseMixedThread ? xl6_function_copy_value(mixedThreadSource) : makeThreadWorker(17LL)), 0','conditionalMemberThread = xl6_thread_require_handle(xl6_thread_start((chooseThreadMember ? threadBox.op : threadBoxAlt.op), 1','borrowedThreadRight.signature ==','borrowedThreadLeft.signature ==','mixedThreadSource.signature ==','xl6_function_call_var(threadBoxAlt.op, 0)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing borrowed conditional thread callable ownership guard.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('void* capturedReceiverThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_worker_make(), 0','void* capturedReceiverThread;','xrtThreadWait(__env->capturedReceiverThread), true','(int)xrtThreadGetExitCode(__env->capturedReceiverThread)','XL6_Function threadJoiner = xl6_anon_','_make(capturedReceiverThread)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing captured thread receiver guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_thread.c',[System.Text.Encoding]::UTF8); $checks=@('void* capturedTimedThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_monitorSlowWorker_make(), 0','void* capturedStopThread = xl6_thread_require_handle(xl6_thread_start(xl6_fnvalue_slowWorker_make(), 0','_make(capturedTimedThread)','xrtThreadWaitTimeout(__env->capturedTimedThread, 0LL) == 0','xrtThreadGetState(__env->capturedTimedThread)','xrtThreadWaitTimeout(__env->capturedTimedThread, 2000LL) == 0','(int)xrtThreadGetExitCode(__env->capturedTimedThread)','_make(capturedStopThread)','xrtThreadStop(__env->capturedStopThread), true','xrtThreadWait(__env->capturedStopThread), true'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_thread.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_thread.c missing captured thread waitTimeout/state/stop guard lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread_default.xl -o release\tmp\cgen_thread_default.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_thread_default.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    release\tmp\cgen_thread_default.exe
    if errorlevel 1 (
        echo cgen_thread_default.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_thread_default.xl --emit release\tmp\cgen_thread_default.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_thread_release" release\tmp\cgen_thread_default.c >nul
if errorlevel 1 (
    echo cgen_thread_default.c missing thread release helper.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_ctor_args.xl >nul 2> release\tmp\sem_thread_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_thread_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"thread constructor requires one func() as int entry argument" release\tmp\sem_thread_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_thread_ctor_args.xl missing thread constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_ctor_named_argument.xl >nul 2> release\tmp\sem_thread_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_thread_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"thread constructor accepts positional arguments only" release\tmp\sem_thread_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_thread_ctor_named_argument.xl missing thread constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_entry_signature.xl >nul 2> release\tmp\sem_thread_entry_signature.err.txt
if not errorlevel 1 (
    echo sem_thread_entry_signature.xl should fail.
    exit /b 1
)
findstr /C:"thread entry must match func() as int" release\tmp\sem_thread_entry_signature.err.txt >nul
if errorlevel 1 (
    echo sem_thread_entry_signature.xl missing thread entry signature diagnostic.
    exit /b 1
)
findstr /C:"function " release\tmp\sem_thread_entry_signature.err.txt >nul
if not errorlevel 1 (
    echo sem_thread_entry_signature.xl should not emit generic function value diagnostics.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_method_args.xl >nul 2> release\tmp\sem_thread_method_args.err.txt
if not errorlevel 1 (
    echo sem_thread_method_args.xl should fail.
    exit /b 1
)
findstr /C:"thread method does not accept arguments" release\tmp\sem_thread_method_args.err.txt >nul
if errorlevel 1 (
    echo sem_thread_method_args.xl missing thread method argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_wait_timeout_type.xl >nul 2> release\tmp\sem_thread_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_thread_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"thread.waitTimeout argument must be int" release\tmp\sem_thread_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_thread_wait_timeout_type.xl missing thread waitTimeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_unknown_method.xl >nul 2> release\tmp\sem_thread_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_thread_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown thread method" release\tmp\sem_thread_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_thread_unknown_method.xl missing unknown thread method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_receiver_expr.xl >nul 2> release\tmp\sem_thread_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_thread_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"thread method receiver" release\tmp\sem_thread_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_thread_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_copy.xl >nul 2> release\tmp\sem_thread_copy.err.txt
if not errorlevel 1 (
    echo sem_thread_copy.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async.xl -o release\tmp\cgen_async.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_async.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_async.exe
    if errorlevel 1 (
        echo cgen_async.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_async.xl --emit release\tmp\cgen_async.c
if errorlevel 1 exit /b 1

findstr /C:"xTaskRunThread" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing xTaskRunThread.
    exit /b 1
)
findstr /C:"xl6_async_require_future(xTaskRunThread" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async no-arg task start guard.
    exit /b 1
)
findstr /C:"async task args allocation failed" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async args allocation diagnostic.
    exit /b 1
)
findstr /C:"async task start failed" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async task start diagnostic.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_async_require_future\([\s\S]*?if \( pFuture != 0 \) \{ return pFuture; \}[\s\S]*?if \( xl6_error_has\(\) \) \{ return 0; \}[\s\S]*?async task start failed'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async.c missing async start active error preservation.
    exit /b 1
)
findstr /C:"xl6_future_await_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await helper.
    exit /b 1
)
findstr /C:"xFutureRelease" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing future cleanup.
    exit /b 1
)
findstr /C:"__xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async return temporary.
    exit /b 1
)
findstr /C:".op.retain != 0" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async class function-field retain.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl_async_result_free_makeBoxAsync" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async class result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_function_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async function result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_xvalue_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async xvalue result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_array_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed array result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_list_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed list result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_set_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed set result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_dict_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed dict result free hook.
    exit /b 1
)
findstr /C:"fnA.retain(fnA.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await function retain.
    exit /b 1
)
findstr /C:"secondBox.op.retain(secondBox.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing repeated await class retain.
    exit /b 1
)
findstr /C:"autoBox.op.retain(autoBox.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class retain.
    exit /b 1
)
findstr /C:"xl_class_AsyncBox__destroy(&autoBox)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class cleanup.
    exit /b 1
)
findstr /C:"long long temporaryValue = xl6_future_await_i64_release(addAsync(8LL, 9LL)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await scalar release helper.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(doneAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await void release helper.
    exit /b 1
)
findstr /C:"XL6_Function temporaryFn = xl6_future_await_function_release(makeAdderAsync(60LL)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await function release helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_future_await_xvalue_release" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await xvalue release helper.
    exit /b 1
)
findstr /C:"xvalue awaitedVar = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(varFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed await xvalue retain.
    exit /b 1
)
findstr /C:"xvalue temporaryVar = xl6_future_await_xvalue_release(makeVarAsync(130LL)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await xvalue helper call.
    exit /b 1
)
findstr /C:"static xfuture* xl6_future_add_ref_if_needed" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await future add-ref helper.
    exit /b 1
)
findstr /C:"conditionalAwaitBorrowed = xl6_future_await_i64_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalAwaitSource) : addAsync(310LL, 1LL))" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed scalar normalization.
    exit /b 1
)
findstr /C:"conditionalAwaitOwned = xl6_future_await_i64_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalAwaitSource) : addAsync(320LL, 1LL))" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned scalar normalization.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalVoidSource) : doneAsync())" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await void normalization.
    exit /b 1
)
findstr /C:"conditionalVarBorrowed = xl6_future_await_xvalue_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalVarSource) : makeVarAsync(340LL))" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed var normalization.
    exit /b 1
)
findstr /C:"conditionalFnBorrowed = xl6_future_await_function_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalFnSource) : makeAdderAsync(370LL))" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed function normalization.
    exit /b 1
)
findstr /C:"conditionalBoxBorrowed = (chooseConditionalAwait ? xl6_future_await_class_makeBoxAsync_release(xl6_future_add_ref_if_needed(conditionalBoxSource)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed class release helper.
    exit /b 1
)
findstr /C:": xl6_future_await_class_makeBoxAsync_release(makeBoxAsync(395LL)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned class release helper.
    exit /b 1
)
findstr /C:"conditionalGreetingBorrowed = (chooseConditionalGreeting ? xl6_future_await_cstr_copy(conditionalGreetingSource" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed string copy.
    exit /b 1
)
findstr /C:": xl6_future_await_cstr_release(greetAsync(" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned string release.
    exit /b 1
)
findstr /C:"conditionalItemsBorrowed = (chooseConditionalItems ? xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(conditionalItemsSource" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed array copy.
    exit /b 1
)
findstr /C:": (xtarray)xl6_future_await_handle_release(slowMakeItemsAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed array release.
    exit /b 1
)
findstr /C:"conditionalListBorrowed = (chooseConditionalList ? xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(conditionalListSource" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed list copy.
    exit /b 1
)
findstr /C:": (xtlist)xl6_future_await_handle_release(slowMakeListAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed list release.
    exit /b 1
)
findstr /C:"conditionalSetBorrowed = (chooseConditionalSet ? xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(conditionalSetSource" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed set copy.
    exit /b 1
)
findstr /C:": (xtset)xl6_future_await_handle_release(slowMakeSetAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed set release.
    exit /b 1
)
findstr /C:"conditionalDictBorrowed = (chooseConditionalDict ? xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(conditionalDictSource" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed dict copy.
    exit /b 1
)
findstr /C:": (xtdict)xl6_future_await_handle_release(slowMakeDictAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed dict release.
    exit /b 1
)
findstr /C:"conditionalMissingAwaitValue = xl6_future_await_i64_release((chooseConditionalMissingAwait ? addAsync(390LL, 1LL) : xl6_future_add_ref_if_needed(conditionalMissingAwait))" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await null branch normalization.
    exit /b 1
)
findstr /C:"static const char* xl6_future_await_cstr_copy" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing named string await copy helper.
    exit /b 1
)
findstr /C:"temporaryGreeting = xl6_future_await_cstr_release(greetAsync(" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await string release helper call.
    exit /b 1
)
findstr /C:"namedGreetingA = xl6_future_await_cstr_copy(greetingFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first named future string copy.
    exit /b 1
)
findstr /C:"namedGreetingB = xl6_future_await_cstr_copy(greetingFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing second named future string copy.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(futureValue, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async int future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelValue = (*(long long*)xl6_future_await_value(futureValue" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async int future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(voidFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async void future cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(boolFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async bool future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelBool = (*(int*)xl6_future_await_value(boolFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async bool future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(halfFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async float future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelHalf = (*(double*)xl6_future_await_value(halfFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async float future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(boxFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async class future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelBox = (*(XL_Class_AsyncBox*)xl6_future_await_value(boxFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async class future awaitable after cancel.
    exit /b 1
)
findstr /C:"postCancelBox.op.retain(postCancelBox.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing post-cancel async class retained function field.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(fnFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async function future cancel as borrowed.
    exit /b 1
)
findstr /C:"XL6_Function fnC = (*(XL6_Function*)xl6_future_await_value(fnFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async function future awaitable after cancel.
    exit /b 1
)
findstr /C:"fnC.retain(fnC.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing post-cancel async function retain.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(varFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async var future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelVar = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(varFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async var future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(greetingFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async string future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelGreeting = xl6_future_await_cstr_copy(greetingFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async string future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitInt = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null int future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitIntValue = (*(long long*)xl6_future_await_value(missingAwaitInt" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null int future await path.
    exit /b 1
)
findstr /C:"missingAwaitStringValue = xl6_future_await_cstr_copy(missingAwaitString" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null string future await path.
    exit /b 1
)
findstr /C:"missingAwaitItemsValue = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(missingAwaitItems" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed array future await path.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitBool = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitBoolValue = (*(int*)xl6_future_await_value(missingAwaitBool" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await path.
    exit /b 1
)
findstr /C:"missingAwaitBoolState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitFloat = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitFloatValue = (*(double*)xl6_future_await_value(missingAwaitFloat" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await path.
    exit /b 1
)
findstr /C:"missingAwaitFloatState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitVar = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitVarValue = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(missingAwaitVar" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await path.
    exit /b 1
)
findstr /C:"missingAwaitVarState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitFn = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitFnValue = (*(XL6_Function*)xl6_future_await_value(missingAwaitFn" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await path.
    exit /b 1
)
findstr /C:"missingAwaitFnState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitBox = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitBoxValue = (*(XL_Class_AsyncBox*)xl6_future_await_value(missingAwaitBox" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class future await path.
    exit /b 1
)
findstr /C:"missingAwaitBoxState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitList = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitListValue = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(missingAwaitList" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await path.
    exit /b 1
)
findstr /C:"missingAwaitListState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitSet = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitSetValue = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(missingAwaitSet" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await path.
    exit /b 1
)
findstr /C:"missingAwaitSetState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitDict = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitDictValue = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(missingAwaitDict" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await path.
    exit /b 1
)
findstr /C:"missingAwaitDictState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await assertion.
    exit /b 1
)
findstr /C:"xrtStrComp(xl6_error_message(e), \"await on null future\"" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null future await diagnostic check.
    exit /b 1
)
findstr /C:"missingAwaitItemsState == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed array future await assertion.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncInt = slowAddAsync(40LL, 2LL);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async int cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncInt, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async int future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncIntValueA = (*(long long*)xl6_future_await_value(pendingAsyncInt" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first int await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncIntValueB = (*(long long*)xl6_future_await_value(pendingAsyncInt" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late int await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncIntStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async int future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncString = slowGreetAsync(\"pending\");" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async string cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncString, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async string future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncStringValueA = xl6_future_await_cstr_copy(pendingAsyncString" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first string await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncStringValueB = xl6_future_await_cstr_copy(pendingAsyncString" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late string await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncStringStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async string future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncBool = slowFlipAsync(1);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async bool cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncBool, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async bool future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncBoolValueA = (*(int*)xl6_future_await_value(pendingAsyncBool" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first bool await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncBoolValueB = (*(int*)xl6_future_await_value(pendingAsyncBool" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late bool await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncBoolStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async bool future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncFloat = slowHalfAsync(1.5);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async float cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncFloat, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async float future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncFloatValueA = (*(double*)xl6_future_await_value(pendingAsyncFloat" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first float await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFloatValueB = (*(double*)xl6_future_await_value(pendingAsyncFloat" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late float await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncFloatStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async float future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncVoid = slowDoneAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async void cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncVoid, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async void future as borrowed.
    exit /b 1
)
findstr /C:"xl6_future_await_value(pendingAsyncVoid" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing void await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncVoidStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async void future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncVar = slowMakeVarAsync(150LL);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async var cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncVar, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async var future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncVarValueA = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(pendingAsyncVar" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first var await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncVarValueB = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(pendingAsyncVar" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late var await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncVarStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async var future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncFn = slowMakeAdderAsync(160LL);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async function cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncFn, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async function future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueA = (*(XL6_Function*)xl6_future_await_value(pendingAsyncFn" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first function await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueA.retain(pendingAsyncFnValueA.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first function await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueB = (*(XL6_Function*)xl6_future_await_value(pendingAsyncFn" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late function await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncFnStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async function future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncBox = slowMakeBoxAsync(170LL);" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async class cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncBox, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async class future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncBoxValueA = (*(XL_Class_AsyncBox*)xl6_future_await_value(pendingAsyncBox" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first class await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncBoxValueA.op.retain(pendingAsyncBoxValueA.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first class await retained function field.
    exit /b 1
)
findstr /C:"pendingAsyncBoxValueB = (*(XL_Class_AsyncBox*)xl6_future_await_value(pendingAsyncBox" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late class await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncBoxStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async class future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncItems = slowMakeItemsAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed array cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncItems, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed array future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncItemsValueA = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(pendingAsyncItems" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed array await copy after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncItemsValueB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(pendingAsyncItems" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed array await copy after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncItemsStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed array future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncList = slowMakeListAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed list cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncList, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed list future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncListValueA = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(pendingAsyncList" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed list await copy after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncListValueB = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(pendingAsyncList" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed list await copy after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncListStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed list future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncSet = slowMakeSetAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed set cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncSet, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed set future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncSetValueA = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(pendingAsyncSet" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed set await copy after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncSetValueB = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(pendingAsyncSet" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed set await copy after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncSetStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed set future canceled after worker completes.
    exit /b 1
)
findstr /C:"xfuture* pendingAsyncDict = slowMakeDictAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed dict cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncDict, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed dict future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncDictValueA = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(pendingAsyncDict" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed dict await copy after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncDictValueB = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(pendingAsyncDict" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed dict await copy after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncDictStateB == 42LL" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed dict future canceled after worker completes.
    exit /b 1
)
findstr /C:"*(const char**)value = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary string await detach.
    exit /b 1
)
findstr /C:"xtarray temporaryItems = xl6_future_await_handle_release(makeItemsAsync()" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await typed container helper call.
    exit /b 1
)
findstr /C:"xtarray __xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed container async return storage.
    exit /b 1
)
findstr /C:"xtlist __xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed list async return storage.
    exit /b 1
)
findstr /C:"xtset __xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed set async return storage.
    exit /b 1
)
findstr /C:"xtdict __xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed dict async return storage.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_SHARED)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed container async shared create.
    exit /b 1
)
findstr /C:"*(void**)value = 0;" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary typed container await detach.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), values, XRT_OBJMODE_SHARED)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed container async shared copy.
    exit /b 1
)
findstr /C:"xfuture* itemsFuture = makeItemsAsync();" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing named future typed container fixture.
    exit /b 1
)
findstr /C:"namedItemsA = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(itemsFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed container copy for first await.
    exit /b 1
)
findstr /C:"namedItemsB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(itemsFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed container copy for second await.
    exit /b 1
)
findstr /C:"namedListA = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(listFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed list copy for first await.
    exit /b 1
)
findstr /C:"namedListB = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(listFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed list copy for second await.
    exit /b 1
)
findstr /C:"namedSetA = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(setFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed set copy for first await.
    exit /b 1
)
findstr /C:"namedSetB = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(setFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed set copy for second await.
    exit /b 1
)
findstr /C:"namedDictA = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(dictFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed dict copy for first await.
    exit /b 1
)
findstr /C:"namedDictB = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(dictFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed dict copy for second await.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(itemsFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed array future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelItems = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(itemsFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed array future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(listFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed list future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelList = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(listFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed list future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(setFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed set future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelSet = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(setFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed set future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(dictFuture, 0)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed dict future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelDict = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(dictFuture" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed dict future awaitable after cancel.
    exit /b 1
)
findstr /C:"long long temporaryValue = ({ xfuture*" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary scalar await.
    exit /b 1
)
findstr /C:"XL6_Function temporaryFn = ({ xfuture*" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary function await.
    exit /b 1
)
findstr /C:"({ xfuture* __xl6_await_future_" release\tmp\cgen_async.c | findstr /C:"doneAsync" >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary void await.
    exit /b 1
)
findstr /C:"static XL_Class_AsyncBox xl6_future_await_class_makeBoxAsync_release" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class release helper.
    exit /b 1
)
findstr /C:"temporaryBox = xl6_future_await_class_makeBoxAsync_release(makeBoxAsync(110LL)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class helper call.
    exit /b 1
)
findstr /C:"result.op.retain(result.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class retained function field.
    exit /b 1
)
findstr /C:"xl6_future_release_if_needed(pFuture)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await helper future release.
    exit /b 1
)
findstr /C:"temporaryBox = ({ xfuture*" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary class await.
    exit /b 1
)
findstr /C:"xvalue temporaryVar = ({ xfuture*" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary xvalue await.
    exit /b 1
)
findstr /C:"__xl6_await_future_" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use temporary await future block.
    exit /b 1
)
findstr /C:"temporaryFn.retain(temporaryFn.env)" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not double-retain temporary await function result.
    exit /b 1
)
findstr /C:"temporaryBox.op.retain(temporaryBox.op.env)" release\tmp\cgen_async.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not double-retain temporary await class result.
    exit /b 1
)


echo DEMO6 suite ok: semantic negatives exceptions sync thread async
exit /b 0

