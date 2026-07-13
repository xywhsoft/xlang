@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: semantic negatives...

release\xl.exe check test\cases\sem_logical_operand_type.xl >nul 2> build\test\sem_logical_operand_type.err.txt
if not errorlevel 1 (
    echo sem_logical_operand_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_outside_func.xl >nul 2> build\test\sem_return_outside_func.err.txt
if not errorlevel 1 (
    echo sem_return_outside_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_value_in_no_return.xl >nul 2> build\test\sem_return_value_in_no_return.err.txt
if not errorlevel 1 (
    echo sem_return_value_in_no_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_missing_value.xl >nul 2> build\test\sem_return_missing_value.err.txt
if not errorlevel 1 (
    echo sem_return_missing_value.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_multi_count.xl >nul 2> build\test\sem_return_multi_count.err.txt
if not errorlevel 1 (
    echo sem_return_multi_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" build\test\sem_return_multi_count.err.txt >nul
if errorlevel 1 (
    echo sem_return_multi_count.xl missing return count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_count.xl >nul 2> build\test\sem_multi_decl_count.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" build\test\sem_multi_decl_count.err.txt >nul
if errorlevel 1 (
    echo sem_multi_decl_count.xl missing multi declaration count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_rhs.xl >nul 2> build\test\sem_multi_decl_rhs.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_rhs.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_assign_count.xl >nul 2> build\test\sem_multi_assign_count.err.txt
if not errorlevel 1 (
    echo sem_multi_assign_count.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" build\test\sem_multi_assign_count.err.txt >nul
if errorlevel 1 (
    echo sem_multi_assign_count.xl missing multi assignment count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_assign_type_mismatch.xl >nul 2> build\test\sem_multi_assign_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_multi_assign_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, return string" build\test\sem_multi_assign_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_multi_assign_type_mismatch.xl missing multi-return variable assignment type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_decl.xl >nul 2> build\test\sem_multi_single_decl.err.txt
if not errorlevel 1 (
    echo sem_multi_single_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_assign.xl >nul 2> build\test\sem_multi_single_assign.err.txt
if not errorlevel 1 (
    echo sem_multi_single_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_arg.xl >nul 2> build\test\sem_multi_return_arg.err.txt
if not errorlevel 1 (
    echo sem_multi_return_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_single.xl >nul 2> build\test\sem_multi_return_single.err.txt
if not errorlevel 1 (
    echo sem_multi_return_single.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_named_order.xl >nul 2> build\test\sem_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_duplicate_named.xl >nul 2> build\test\sem_call_duplicate_named.err.txt
if not errorlevel 1 (
    echo sem_call_duplicate_named.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_arg_type.xl >nul 2> build\test\sem_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_call_arg_type.xl missing function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_missing_required.xl >nul 2> build\test\sem_call_missing_required.err.txt
if not errorlevel 1 (
    echo sem_call_missing_required.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" build\test\sem_call_missing_required.err.txt >nul
if errorlevel 1 (
    echo sem_call_missing_required.xl missing function argument count context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_call_missing_required.err.txt >nul
if not errorlevel 1 (
    echo sem_call_missing_required.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_order.xl >nul 2> build\test\sem_param_order.err.txt
if not errorlevel 1 (
    echo sem_param_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_position.xl >nul 2> build\test\sem_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_arg_redeclare.xl >nul 2> build\test\sem_vararg_arg_redeclare.err.txt
if not errorlevel 1 (
    echo sem_vararg_arg_redeclare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_bad_name.xl >nul 2> build\test\sem_function_type_bad_name.err.txt
if not errorlevel 1 (
    echo sem_function_type_bad_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_vararg_position.xl >nul 2> build\test\sem_function_type_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_function_type_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_signature_mismatch.xl >nul 2> build\test\sem_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo sem_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"target function, value function" build\test\sem_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt missing function signature type context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_function_signature_mismatch.xl >nul 2> build\test\cgen_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\cgen_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"target function, value function" build\test\cgen_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt missing function signature type context.
    exit /b 1
)
findstr /C:"code generation error" build\test\cgen_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_count_missing.xl >nul 2> build\test\sem_function_call_arg_count_missing.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_count_missing.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 1" build\test\sem_function_call_arg_count_missing.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_count_missing.xl missing function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_count_extra.xl >nul 2> build\test\sem_function_call_arg_count_extra.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_count_extra.xl should fail.
    exit /b 1
)
findstr /C:"expected 2, actual 3" build\test\sem_function_call_arg_count_extra.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_count_extra.xl missing function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_type.xl >nul 2> build\test\sem_function_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_function_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_function_call_arg_type.xl missing function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_function_field_call_arg_type.xl >nul 2> build\test\sem_member_function_field_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_member_function_field_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_member_function_field_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_function_field_call_arg_type.xl missing member function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_named_arg.xl >nul 2> build\test\sem_function_bare_named_arg.err.txt
if not errorlevel 1 (
    echo sem_function_bare_named_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_typed_named_arg.xl >nul 2> build\test\sem_function_typed_named_arg.err.txt
if not errorlevel 1 (
    echo sem_function_typed_named_arg.xl should fail.
    exit /b 1
)
findstr /C:"func" build\test\sem_function_typed_named_arg.err.txt >nul
if errorlevel 1 (
    echo sem_function_typed_named_arg.err.txt missing func-value named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_property_vararg.xl >nul 2> build\test\sem_class_property_vararg.err.txt
if not errorlevel 1 (
    echo sem_class_property_vararg.xl should fail.
    exit /b 1
)
findstr /C:"property parameters cannot be vararg" build\test\sem_class_property_vararg.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_vararg.err.txt missing property vararg diagnostic.
    exit /b 1
)
findstr /C:"func" build\test\sem_function_bare_named_arg.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt missing func diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_function_bare_named_arg.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_named_order.xl >nul 2> build\test\sem_function_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_function_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_overload_name.xl >nul 2> build\test\sem_function_bare_overload_name.err.txt
if not errorlevel 1 (
    echo sem_function_bare_overload_name.xl should fail.
    exit /b 1
)
findstr /C:"func" build\test\sem_function_bare_overload_name.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt missing func overload diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_function_bare_overload_name.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
findstr /C:"target function, value <unknown>" build\test\sem_function_bare_overload_name.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt regressed to cascaded unknown-value mismatch.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_multi_return_arg_type.xl >nul 2> build\test\sem_function_multi_return_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_multi_return_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_function_multi_return_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_function_multi_return_arg_type.xl missing multi-return function argument type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_type_mismatch.xl >nul 2> build\test\sem_return_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_return_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_return_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_return_type_mismatch.xl missing return type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_default_type.xl >nul 2> build\test\sem_param_default_type.err.txt
if not errorlevel 1 (
    echo sem_param_default_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_param_default_type.err.txt >nul
if errorlevel 1 (
    echo sem_param_default_type.xl missing parameter default type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_convert_invalid.xl >nul 2> build\test\sem_type_convert_invalid.err.txt
if not errorlevel 1 (
    echo sem_type_convert_invalid.xl should fail.
    exit /b 1
)
findstr /C:"target point, source int" build\test\sem_type_convert_invalid.err.txt >nul
if errorlevel 1 (
    echo sem_type_convert_invalid.xl missing type conversion context.
    exit /b 1
)

release\xl.exe check test\cases\sem_anon_capture_assign.xl >nul 2> build\test\sem_anon_capture_assign.err.txt
if not errorlevel 1 (
    echo sem_anon_capture_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_func_capture_top_level.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_func_capture_top_level.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_async_capture_top_level.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_async_capture_top_level.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_noncopyable_fresh_return.xl >nul
if errorlevel 1 (
    echo sem_noncopyable_fresh_return.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_return.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_noncopyable_return.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\sem_noncopyable_return.xl -o build\test\sem_noncopyable_return.exe
if errorlevel 1 exit /b 1
build\test\sem_noncopyable_return.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_class_unknown_base.xl >nul 2> build\test\sem_class_unknown_base.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_base.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_create_missing.xl >nul 2> build\test\sem_class_base_create_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_create_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_default_missing.xl >nul 2> build\test\sem_class_base_default_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_default_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_outside_create.xl >nul 2> build\test\sem_class_base_outside_create.err.txt
if not errorlevel 1 (
    echo sem_class_base_outside_create.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_overload_missing.xl >nul 2> build\test\sem_class_base_overload_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_overload_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_ctor_arity.xl >nul 2> build\test\sem_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_class_ctor_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 0" build\test\sem_class_ctor_arity.err.txt >nul
if errorlevel 1 (
    echo sem_class_ctor_arity.err.txt missing create argument count context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_class_ctor_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_class_ctor_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_default_ctor_args.xl >nul 2> build\test\sem_class_default_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_class_default_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"expected 0, actual 1" build\test\sem_class_default_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt missing default constructor count context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_class_default_ctor_args.err.txt >nul
if not errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_destroy_params.xl >nul 2> build\test\sem_class_destroy_params.err.txt
if not errorlevel 1 (
    echo sem_class_destroy_params.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_conflict.xl >nul 2> build\test\sem_class_field_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_field_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_default_type.xl >nul 2> build\test\sem_class_field_default_type.err.txt
if not errorlevel 1 (
    echo sem_class_field_default_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_class_field_default_type.err.txt >nul
if errorlevel 1 (
    echo sem_class_field_default_type.err.txt missing field default type context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_class_field_default_type.err.txt >nul
if not errorlevel 1 (
    echo sem_class_field_default_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_method_overload_conflict.xl >nul 2> build\test\sem_class_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_private_access.xl >nul 2> build\test\sem_class_private_access.err.txt
if not errorlevel 1 (
    echo sem_class_private_access.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_protected_access.xl >nul 2> build\test\sem_class_protected_access.err.txt
if not errorlevel 1 (
    echo sem_class_protected_access.xl should fail.
    exit /b 1
)
findstr /C:"protected class member is not accessible" build\test\sem_class_protected_access.err.txt >nul
if errorlevel 1 (
    echo sem_class_protected_access.xl missing protected access diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_access_conflict.xl >nul 2> build\test\sem_class_access_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_access_conflict.xl should fail.
    exit /b 1
)
findstr /C:"public, private or protected" build\test\sem_class_access_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_class_access_conflict.xl missing access conflict diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_static_self.xl >nul 2> build\test\sem_class_static_self.err.txt
if not errorlevel 1 (
    echo sem_class_static_self.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_missing.xl >nul 2> build\test\sem_class_override_missing.err.txt
if not errorlevel 1 (
    echo sem_class_override_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_nonvirtual.xl >nul 2> build\test\sem_class_override_nonvirtual.err.txt
if not errorlevel 1 (
    echo sem_class_override_nonvirtual.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_override_final.xl >nul 2> build\test\sem_class_override_final.err.txt
if not errorlevel 1 (
    echo sem_class_override_final.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_self_assign.xl >nul 2> build\test\sem_self_assign.err.txt
if not errorlevel 1 (
    echo sem_self_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\parse_removed_keyword.xl >nul 2> build\test\parse_removed_keyword.err.txt
if not errorlevel 1 (
    echo parse_removed_keyword.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\parse_removed_keyword.err.txt >nul
if errorlevel 1 (
    echo parse_removed_keyword.err.txt missing syntax diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_reserved_keyword_identifier.xl >nul 2> build\test\parse_reserved_keyword_identifier.err.txt
if not errorlevel 1 (
    echo parse_reserved_keyword_identifier.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\parse_reserved_keyword_identifier.err.txt >nul
if errorlevel 1 (
    echo parse_reserved_keyword_identifier.err.txt missing syntax diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_bad_special.xl >nul 2> build\test\sem_native_class_bad_special.err.txt
if not errorlevel 1 (
    echo sem_native_class_bad_special.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_missing_handle.xl >nul 2> build\test\sem_native_class_missing_handle.err.txt
if not errorlevel 1 (
    echo sem_native_class_missing_handle.xl should fail.
    exit /b 1
)
findstr /C:"native-backed class extern member requires native point handle field" build\test\sem_native_class_missing_handle.err.txt >nul
if errorlevel 1 (
    echo sem_native_class_missing_handle.err.txt missing native handle diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_missing_handle.xl >nul 2> build\test\sem_native_property_missing_handle.err.txt
if not errorlevel 1 (
    echo sem_native_property_missing_handle.xl should fail.
    exit /b 1
)
findstr /C:"native-backed class extern member requires native point handle field" build\test\sem_native_property_missing_handle.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_missing_handle.err.txt missing native handle diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_get_no_return.xl >nul 2> build\test\sem_native_property_get_no_return.err.txt
if not errorlevel 1 (
    echo sem_native_property_get_no_return.xl should fail.
    exit /b 1
)
findstr /C:"extern get property must declare exactly one return type" build\test\sem_native_property_get_no_return.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_get_no_return.err.txt missing extern get diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_let_no_value.xl >nul 2> build\test\sem_native_property_let_no_value.err.txt
if not errorlevel 1 (
    echo sem_native_property_let_no_value.xl should fail.
    exit /b 1
)
findstr /C:"extern let property must declare a value parameter" build\test\sem_native_property_let_no_value.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_let_no_value.err.txt missing extern let diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_optional.xl >nul 2> build\test\sem_native_property_optional.err.txt
if not errorlevel 1 (
    echo sem_native_property_optional.xl should fail.
    exit /b 1
)
findstr /C:"extern property parameters cannot be optional" build\test\sem_native_property_optional.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_optional.err.txt missing extern property optional diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_vararg.xl >nul 2> build\test\sem_native_property_vararg.err.txt
if not errorlevel 1 (
    echo sem_native_property_vararg.xl should fail.
    exit /b 1
)
findstr /C:"extern property parameters cannot be vararg" build\test\sem_native_property_vararg.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_vararg.err.txt missing extern property vararg diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_property_duplicate_c_symbol.xl >nul 2> build\test\sem_native_property_duplicate_c_symbol.err.txt
if not errorlevel 1 (
    echo sem_native_property_duplicate_c_symbol.xl should fail.
    exit /b 1
)
findstr /C:"same native-backed class cannot bind the same C symbol twice" build\test\sem_native_property_duplicate_c_symbol.err.txt >nul
if errorlevel 1 (
    echo sem_native_property_duplicate_c_symbol.err.txt missing duplicate C symbol diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_out_read_before_write.xl >nul 2>build\test\sem_out_read_before_write.err.txt
if not errorlevel 1 (
    echo sem_out_read_before_write.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_out_read_before_write.err.txt >nul
if errorlevel 1 (
    echo sem_out_read_before_write.err.txt missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_out_missing_branch.xl >nul 2>build\test\sem_out_missing_branch.err.txt
if not errorlevel 1 (
    echo sem_out_missing_branch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_out_missing_branch.err.txt >nul
if errorlevel 1 (
    echo sem_out_missing_branch.err.txt missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_out_capture.xl >nul 2>build\test\sem_out_capture.err.txt
if not errorlevel 1 (
    echo sem_out_capture.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_out_capture.err.txt >nul
if errorlevel 1 (
    echo sem_out_capture.err.txt missing semantic diagnostic.
    exit /b 1
)

echo DEMO6 suite ok: semantic negatives
exit /b 0
