@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: exceptions and synchronization...

release\xl.exe check test\cases\sem_native_class_duplicate_c_symbol.xl >nul 2> build\test\sem_native_class_duplicate_c_symbol.err.txt
if not errorlevel 1 (
    echo sem_native_class_duplicate_c_symbol.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_bind_non_point.xl >nul 2> build\test\sem_native_class_bind_non_point.err.txt
if not errorlevel 1 (
    echo sem_native_class_bind_non_point.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_create_point_bind_conflict.xl >nul 2> build\test\sem_native_class_create_point_bind_conflict.err.txt
if not errorlevel 1 (
    echo sem_native_class_create_point_bind_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_extern_bad_type.xl >nul 2> build\test\sem_native_class_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_native_class_extern_bad_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_native_class_extern_bad_type.err.txt >nul
if not errorlevel 1 (
    echo sem_native_class_extern_bad_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_extern_return_bad.xl >nul 2> build\test\sem_native_class_extern_return_bad.err.txt
if not errorlevel 1 (
    echo sem_native_class_extern_return_bad.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_native_class_extern_return_bad.err.txt >nul
if not errorlevel 1 (
    echo sem_native_class_extern_return_bad.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_extern_vararg.xl >nul 2> build\test\sem_native_class_extern_vararg.err.txt
if not errorlevel 1 (
    echo sem_native_class_extern_vararg.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_native_class_extern_vararg.err.txt >nul
if not errorlevel 1 (
    echo sem_native_class_extern_vararg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_native_class_method_overload_conflict.xl >nul 2> build\test\sem_native_class_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_native_class_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_param.xl >nul 2> build\test\sem_auto_param.err.txt
if not errorlevel 1 (
    echo sem_auto_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_return.xl >nul 2> build\test\sem_auto_return.err.txt
if not errorlevel 1 (
    echo sem_auto_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_field.xl >nul 2> build\test\sem_auto_field.err.txt
if not errorlevel 1 (
    echo sem_auto_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_alias.xl >nul 2> build\test\sem_auto_alias.err.txt
if not errorlevel 1 (
    echo sem_auto_alias.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_function_type.xl >nul 2> build\test\sem_auto_function_type.err.txt
if not errorlevel 1 (
    echo sem_auto_function_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_void_type.xl >nul 2> build\test\sem_void_type.err.txt
if not errorlevel 1 (
    echo sem_void_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_as_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_type_builtin_name.xl >nul 2> build\test\sem_type_builtin_name.err.txt
if not errorlevel 1 (
    echo sem_type_builtin_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_duplicate_decl.xl >nul 2> build\test\sem_type_duplicate_decl.err.txt
if not errorlevel 1 (
    echo sem_type_duplicate_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_unknown_type.xl >nul 2> build\test\sem_unknown_type.err.txt
if not errorlevel 1 (
    echo sem_unknown_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_duplicate.xl >nul 2> build\test\sem_export_duplicate.err.txt
if not errorlevel 1 (
    echo sem_export_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_optional.xl >nul 2> build\test\sem_export_optional.err.txt
if not errorlevel 1 (
    echo sem_export_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_vararg.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_vararg_export.xl >nul 2> build\test\sem_c_vararg_export.err.txt
if not errorlevel 1 (
    echo sem_c_vararg_export.xl should fail.
    exit /b 1
)
findstr /C:"C ABI export" build\test\sem_c_vararg_export.err.txt >nul
if errorlevel 1 (
    echo sem_c_vararg_export.err.txt missing C ABI export vararg diagnostic.
    exit /b 1
)
findstr /C:"c_vararg" build\test\sem_c_vararg_export.err.txt >nul
if errorlevel 1 (
    echo sem_c_vararg_export.err.txt missing c_vararg extern diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_var_init_type.xl >nul 2> build\test\sem_export_var_init_type.err.txt
if not errorlevel 1 (
    echo sem_export_var_init_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_export_var_init_type.err.txt >nul
if errorlevel 1 (
    echo sem_export_var_init_type.xl missing export variable initializer type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_string_return.xl >nul 2> build\test\sem_export_string_return.err.txt
if not errorlevel 1 (
    echo sem_export_string_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_generic.xl >nul 2> build\test\sem_export_generic.err.txt
if not errorlevel 1 (
    echo sem_export_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_static.xl >nul 2> build\test\sem_class_export_static.err.txt
if not errorlevel 1 (
    echo sem_class_export_static.xl should fail.
    exit /b 1
)
findstr /C:"class C ABI export member cannot be static" build\test\sem_class_export_static.err.txt >nul
if errorlevel 1 (
    echo sem_class_export_static.err.txt missing class export static diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_generic.xl >nul 2> build\test\sem_class_export_generic.err.txt
if not errorlevel 1 (
    echo sem_class_export_generic.xl should fail.
    exit /b 1
)
findstr /C:"class C ABI export member does not support generic class" build\test\sem_class_export_generic.err.txt >nul
if errorlevel 1 (
    echo sem_class_export_generic.err.txt missing class export generic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_export_duplicate.xl >nul 2> build\test\sem_class_export_duplicate.err.txt
if not errorlevel 1 (
    echo sem_class_export_duplicate.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$diag=([char]0x91CD)+([char]0x590D)+([char]0x7684)+' C export symbol'; $text=[System.IO.File]::ReadAllText('build\test\sem_class_export_duplicate.err.txt',[System.Text.Encoding]::UTF8); if($text.IndexOf($diag) -lt 0){ exit 1 }"
if errorlevel 1 (
    echo sem_class_export_duplicate.err.txt missing duplicate C export diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_arity.xl >nul 2> build\test\sem_generic_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" build\test\sem_generic_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_arity.xl missing generic argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_param_duplicate.xl >nul 2> build\test\sem_generic_param_duplicate.err.txt
if not errorlevel 1 (
    echo sem_generic_param_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_unknown_type_arg.xl >nul 2> build\test\sem_generic_unknown_type_arg.err.txt
if not errorlevel 1 (
    echo sem_generic_unknown_type_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_infer_conflict.xl >nul 2> build\test\sem_generic_infer_conflict.err.txt
if not errorlevel 1 (
    echo sem_generic_infer_conflict.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_generic_infer_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_generic_infer_conflict.xl missing generic inference conflict context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_func_inst_arity.xl >nul 2> build\test\sem_generic_func_inst_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_func_inst_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" build\test\sem_generic_func_inst_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_func_inst_arity.xl missing explicit generic function argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_arity.xl >nul 2> build\test\sem_generic_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" build\test\sem_generic_class_ctor_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_class_ctor_arity.xl missing generic class constructor count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_open_ctor.xl >nul 2> build\test\sem_generic_class_open_ctor.err.txt
if not errorlevel 1 (
    echo sem_generic_class_open_ctor.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_type_mismatch.xl >nul 2> build\test\sem_generic_class_ctor_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_type_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_arity.xl >nul 2> build\test\sem_generic_class_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" build\test\sem_generic_class_arity.err.txt >nul
if errorlevel 1 (
    echo sem_generic_class_arity.xl missing generic class type argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_extends.xl >nul 2> build\test\sem_generic_class_extends.err.txt
if not errorlevel 1 (
    echo sem_generic_class_extends.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_return_only.xl >nul 2> build\test\sem_overload_return_only.err.txt
if not errorlevel 1 (
    echo sem_overload_return_only.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_optional_conflict.xl >nul 2> build\test\sem_overload_optional_conflict.err.txt
if not errorlevel 1 (
    echo sem_overload_optional_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_vararg_same_kind.xl >nul 2> build\test\sem_overload_vararg_same_kind.err.txt
if not errorlevel 1 (
    echo sem_overload_vararg_same_kind.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_overload_conflict.xl >nul 2> build\test\sem_alias_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_alias_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_cycle.xl >nul 2> build\test\sem_alias_cycle.err.txt
if not errorlevel 1 (
    echo sem_alias_cycle.xl should fail.
    exit /b 1
)
findstr /C:"type alias cycle" build\test\sem_alias_cycle.err.txt >nul
if errorlevel 1 (
    echo sem_alias_cycle.xl should report alias cycle.
    exit /b 1
)
findstr /C:"target First" build\test\sem_alias_cycle.err.txt >nul
if not errorlevel 1 (
    echo sem_alias_cycle.xl should not report initializer cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_mismatch.xl >nul 2> build\test\sem_extern_c_symbol_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_kind_mismatch.xl >nul 2> build\test\sem_extern_c_symbol_kind_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_kind_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_bad_type.xl >nul 2> build\test\sem_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_extern_bad_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_multi_return.xl >nul 2> build\test\sem_extern_multi_return.err.txt
if not errorlevel 1 (
    echo sem_extern_multi_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_optional.xl >nul 2> build\test\sem_extern_optional.err.txt
if not errorlevel 1 (
    echo sem_extern_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_vararg.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_c_vararg_nonextern.xl >nul 2> build\test\sem_c_vararg_nonextern.err.txt
if not errorlevel 1 (
    echo sem_c_vararg_nonextern.xl should fail.
    exit /b 1
)
findstr /C:"c_vararg" build\test\sem_c_vararg_nonextern.err.txt >nul
if errorlevel 1 (
    echo sem_c_vararg_nonextern.err.txt missing c_vararg extern diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_c_vararg_not_last.xl >nul 2> build\test\sem_c_vararg_not_last.err.txt
if not errorlevel 1 (
    echo sem_c_vararg_not_last.xl should fail.
    exit /b 1
)
findstr /C:"c_vararg must be the last extern C parameter" build\test\sem_c_vararg_not_last.err.txt >nul
if errorlevel 1 (
    echo sem_c_vararg_not_last.err.txt missing c_vararg position diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_c_vararg_xvalue_fixed.xl >nul 2> build\test\sem_c_vararg_xvalue_fixed.err.txt
if not errorlevel 1 (
    echo sem_c_vararg_xvalue_fixed.xl should fail.
    exit /b 1
)
findstr /C:"c_vararg extern cannot mix with xvalue managed parameters" build\test\sem_c_vararg_xvalue_fixed.err.txt >nul
if errorlevel 1 (
    echo sem_c_vararg_xvalue_fixed.err.txt missing c_vararg managed-parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_generic.xl >nul 2> build\test\sem_extern_generic.err.txt
if not errorlevel 1 (
    echo sem_extern_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_concat_mismatch.xl >nul 2> build\test\sem_string_concat_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_concat_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"numeric operator operands must be numeric" build\test\sem_string_concat_mismatch.err.txt >nul
if not errorlevel 1 (
    echo sem_string_concat_mismatch.xl should not report numeric cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_assign_mismatch.xl >nul 2> build\test\sem_string_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_assign_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"string compound assignment operands must both be string" build\test\sem_string_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_string_assign_mismatch.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left string, right int" build\test\sem_string_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_string_assign_mismatch.xl missing compound diagnostic context.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_compare_mismatch.xl >nul 2> build\test\sem_string_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_bad.xl >nul 2> build\test\sem_time_literal_bad.err.txt
if not errorlevel 1 (
    echo sem_time_literal_bad.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_newline.xl >nul 2> build\test\sem_time_literal_newline.err.txt
if not errorlevel 1 (
    echo sem_time_literal_newline.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_assign_mismatch.xl >nul 2> build\test\sem_time_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_assign_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target time, value int" build\test\sem_time_assign_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_time_assign_mismatch.xl missing declaration init type mismatch context.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_compare_mismatch.xl >nul 2> build\test\sem_time_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_native_class_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_native_class_assign.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_native_class_arg.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_function_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_var_call_arg_destroy_class.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_call_arg_destroy_class.xl --emit build\test\cgen_var_call_arg_destroy_class.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_function_to_xvalue(" build\test\cgen_var_call_arg_destroy_class.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_retain(__xl6_class_arg_" build\test\cgen_var_call_arg_destroy_class.c >nul
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_call_arg_destroy_class.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_call_named_order.xl >nul 2> build\test\sem_var_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_var_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_var_destroy_class_init.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_destroy_class_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_var_destroy_class_return.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_destroy_class_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_overload_ambiguous.xl >nul 2> build\test\sem_overload_ambiguous.err.txt
if not errorlevel 1 (
    echo sem_overload_ambiguous.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_literal.xl >nul 2> build\test\sem_typed_literal.err.txt
if not errorlevel 1 (
    echo sem_typed_literal.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_set_pair.xl >nul 2> build\test\sem_typed_set_pair.err.txt
if not errorlevel 1 (
    echo sem_typed_set_pair.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_value_mismatch.xl >nul 2> build\test\sem_type_value_mismatch.err.txt
if not errorlevel 1 (
    echo sem_type_value_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_compare_int.xl >nul 2> build\test\sem_type_compare_int.err.txt
if not errorlevel 1 (
    echo sem_type_compare_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_order_compare.xl >nul 2> build\test\sem_type_order_compare.err.txt
if not errorlevel 1 (
    echo sem_type_order_compare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_call_arity.xl >nul 2> build\test\sem_type_call_arity.err.txt
if not errorlevel 1 (
    echo sem_type_call_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_arity.xl >nul 2> build\test\sem_range_arity.err.txt
if not errorlevel 1 (
    echo sem_range_arity.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_range_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_range_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_step_zero.xl >nul 2> build\test\sem_range_step_zero.err.txt
if not errorlevel 1 (
    echo sem_range_step_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_in_three_targets.xl >nul 2> build\test\sem_for_in_three_targets.err.txt
if not errorlevel 1 (
    echo sem_for_in_three_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_for_in_three_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_for_in_three_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_await_three_targets.xl >nul 2> build\test\sem_for_await_three_targets.err.txt
if not errorlevel 1 (
    echo sem_for_await_three_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_for_await_three_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_for_await_three_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_exceptions.xl -o build\test\cgen_exceptions.exe
if errorlevel 1 exit /b 1

build\test\cgen_exceptions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_exceptions.xl --emit build\test\cgen_exceptions.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise" build\test\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing error runtime raise.
    exit /b 1
)
findstr /C:"InterlockedCompareExchange(&xl6__active_error_tls_state" build\test\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing race-free Windows error TLS initialization.
    exit /b 1
)
findstr /C:"TlsFree(xl6__active_error_tls_index)" build\test\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing Windows error TLS cleanup.
    exit /b 1
)
findstr /C:"xl6_error_tls_unit();" build\test\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing executable error TLS cleanup call.
    exit /b 1
)
release\xl.exe run test\cases\cgen_error_member_lifetime.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_error_member_lifetime.xl -o build\test\cgen_error_member_lifetime.exe
if errorlevel 1 exit /b 1
build\test\cgen_error_member_lifetime.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_error_member_lifetime.xl --emit build\test\cgen_error_member_lifetime.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_text_copy" build\test\cgen_error_member_lifetime.c >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_loop_exception_propagation.xl -o build\test\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

build\test\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_exception_propagation.xl --emit build\test\cgen_loop_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"loop boom\"" build\test\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop throw raise.
    exit /b 1
)

findstr /C:"goto __xl6_try_finally_" build\test\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop exception try/finally jump.
    exit /b 1
)

release\xl.exe run test\cases\cgen_exception_uncaught.xl >nul 2> build\test\cgen_exception_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_exception_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: uncaught" build\test\cgen_exception_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_exception_uncaught.err.txt missing uncaught exception diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_global_shared_publish.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_global_shared_publish.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_global_shared_publish.xl -o build\test\cgen_global_shared_publish.exe
if errorlevel 1 exit /b 1

for /L %%I in (1,1,5) do (
    build\test\cgen_global_shared_publish.exe
    if errorlevel 1 (
        echo cgen_global_shared_publish.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_global_shared_publish.xl --emit build\test\cgen_global_shared_publish.c
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_global_shared_publish.c',[System.Text.Encoding]::UTF8); $checks=@('gPublished = __xl6_global_init_','xl6_xrt_move_to_shared(__xl6_global_init_','__xl6_assign_','XRT_OBJMODE_SHARED','__xl6_typed_container_source_','.posts = xl6_xrt_move_to_shared(','.values = xrtTypedArrayMoveToShared('); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_global_shared_publish.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_global_shared_publish.c missing shared publication lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_sync.xl -o build\test\cgen_sync.exe
if errorlevel 1 exit /b 1

build\test\cgen_sync.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_sync.exe
    if errorlevel 1 (
        echo cgen_sync.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_sync.xl --emit build\test\cgen_sync.c
if errorlevel 1 exit /b 1

findstr /C:"xrtMutexCreate" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtMutexCreate.
    exit /b 1
)
findstr /C:"xrtSemCreate" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtSemCreate.
    exit /b 1
)
findstr /C:"xrtRWLockCreate" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtRWLockCreate.
    exit /b 1
)
findstr /C:"xrtSemPostMultiple(gate" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing sem postMultiple path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('cappedOverflow = xrtSemPostMultiple(capped, 2LL)','cappedOriginal = (xrtSemWaitTimeout(capped, 10LL) == 0)','cappedPartial = xrtSemTryWait(capped)','!cappedOverflow','!cappedPartial','fullPost = xrtSemPost(full)','fullWaited = (xrtSemWaitTimeout(full, 10LL) == 0)','refillPost = xrtSemPost(full)','refillWaited = (xrtSemWaitTimeout(full, 10LL) == 0)','!fullPost'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing sem overflow/full guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtSemWaitTimeout(semWakeGate, 2000LL) == 0','semWakeWorker = xl6_anon_','_make()','xl6_thread_start(semWakeWorker, 1','xrtThreadWaitTimeout(semWakeThread, 1LL) == 0','xrtSemPost(semWakeGate)','xrtThreadWaitTimeout(semWakeThread, 2000LL) == 0','(int)xrtThreadGetExitCode(semWakeThread) == 23LL'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing cross-thread sem wake guard lowering.
    exit /b 1
)
findstr /C:"xrtCondWaitTimeout(c, condMutex" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing cond waitTimeout path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtSemWaitTimeout(wakeReady, 2000LL) == 0','xrtMutexLock(wakeMutex)','xrtCondSignal(wakeCond)','xrtMutexUnlock(wakeMutex)','xl6_thread_start(wakeWorker, 1','xrtSemPost(wakeReady)','wakeSignaled = (xrtCondWaitTimeout(wakeCond, wakeMutex, 2000LL) == 0)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing cross-thread cond signal guard lowering.
    exit /b 1
)
findstr /C:"xrtRWLockTryReadLock(rw" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing rwlock tryReadLock path.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('invalidUpgrade = xrtRWLockUpgrade(upgradeRw)','xrtRWLockReadLock(upgradeRw)','upgraded = xrtRWLockUpgrade(upgradeRw)','xrtRWLockWriteUnlock(upgradeRw)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing rwlock upgrade guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_sync.c',[System.Text.Encoding]::UTF8); $checks=@('xrtRWLockWriteLock(capturedRw)','xrtRWLockDowngrade(capturedRw)','xrtRWLockReadUnlock(capturedRw)','capturedRwWorker = xl6_anon_','_make()','xrtThreadWaitTimeout(capturedRwThread, 2000LL) == 0','(int)xrtThreadGetExitCode(capturedRwThread) == 29LL','xrtRWLockTryWriteLock(capturedRw)','xrtRWLockWriteUnlock(capturedRw)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_sync.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_sync.c missing captured rwlock receiver guard lowering.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtMutexCreate()" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing mutex constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtSemCreate(" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing sem constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtCondCreate()" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing cond constructor error guard.
    exit /b 1
)
findstr /C:"xl6_sync_require_handle(xrtRWLockCreate()" build\test\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing rwlock constructor error guard.
    exit /b 1
)


echo DEMO6 suite ok: exceptions and synchronization
exit /b 0
