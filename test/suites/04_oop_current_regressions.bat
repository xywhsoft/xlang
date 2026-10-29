@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: current OOP and parameter-syntax regressions...

release\xl.exe check test\cases\cgen_var_class_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_class_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_class_dispatch.xl --emit build\test\cgen_var_class_dispatch.c
if errorlevel 1 exit /b 1
findstr /C:"methodValue = xrtValueInt(" build\test\cgen_var_class_dispatch.c >nul
if errorlevel 1 (
    echo cgen_var_class_dispatch.c lost static var class return boxing.
    exit /b 1
)
findstr /C:"propertyValue = xl6_xrt_value_text_create(" build\test\cgen_var_class_dispatch.c >nul
if errorlevel 1 (
    echo cgen_var_class_dispatch.c lost static var property return boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_checked_release(xl6_var_call(" build\test\cgen_var_class_dispatch.c >nul
if errorlevel 1 (
    echo cgen_var_class_dispatch.c kept a stale class flow across a control-flow merge.
    exit /b 1
)
findstr /C:"xl6_record_field_get_owned(boxed, \"label\", 0)" build\test\cgen_var_class_dispatch.c >nul
if errorlevel 1 (
    echo cgen_var_class_dispatch.c lost dynamic class property dispatch after a flow merge.
    exit /b 1
)
release\xl.exe build test\cases\cgen_var_class_dispatch.xl -o build\test\cgen_var_class_dispatch.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_class_dispatch.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_local_var_dynamic_member_storage.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_local_var_dynamic_member_storage.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_local_var_dynamic_member_storage.xl -o build\test\cgen_local_var_dynamic_member_storage.exe
if errorlevel 1 exit /b 1
build\test\cgen_local_var_dynamic_member_storage.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\xruntime_dynamic_fields.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\xruntime_dynamic_fields.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\xruntime_dynamic_fields.xl --emit build\test\xruntime_dynamic_fields.c
if errorlevel 1 exit /b 1
findstr /C:"#define XRUNTIME_MODULE_RUNTIME_DYNAMIC_FIELD 1" build\test\xruntime_dynamic_fields.c >nul
if errorlevel 1 (
    echo xruntime_dynamic_fields.c missing runtime dynamic-field trim closure.
    exit /b 1
)
findstr /C:"xl6_xrt_dynamic_fields_set" build\test\xruntime_dynamic_fields.c >nul
if errorlevel 1 (
    echo xruntime_dynamic_fields.c missing xruntime dynamic-field bridge calls.
    exit /b 1
)
release\xl.exe build test\cases\xruntime_dynamic_fields.xl -o build\test\xruntime_dynamic_fields.exe
if errorlevel 1 exit /b 1
build\test\xruntime_dynamic_fields.exe
if errorlevel 1 exit /b 1

for %%F in (
    cgen_narrowed_var_storage_return
    cgen_typed_container_storage_copy
    cgen_try_scope_shadow_call
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

release\xl.exe check test\cases\cgen_generic_multi_return.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generic_multi_return.xl --emit build\test\cgen_generic_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"xl_gen_pair__int(41LL)" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing inferred generic multi-return call.
    exit /b 1
)
findstr /C:"xl_gen_pair__string(\"ready\")" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing explicit generic multi-return call.
    exit /b 1
)
findstr /C:"xl_class_Box__int__take(" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing generic class multi-return call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_generic_multi_return.c %XL6_CGEN_LINKS% -o build\test\cgen_generic_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_multi_return.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe check test\cases\cgen_class_field_module_const.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_field_module_const.xl --emit build\test\cgen_class_field_module_const.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_class_field_module_const.c' -Raw; $intName='__xl6_user_global_DEFAULT__SCORE'; $textName='__xl6_user_global_DEFAULT__NAME'; $intDecl=$t.IndexOf('static long long '+$intName+';'); $textDecl=$t.IndexOf('static const char* '+$textName+';'); $intUse=$t.IndexOf('= '+$intName, $intDecl + 1); $textUse=$t.IndexOf($textName+', 0u', $textDecl + 1); if($intDecl -lt 0 -or $textDecl -lt 0 -or $intUse -lt 0 -or $textUse -lt 0 -or $intDecl -ge $intUse -or $textDecl -ge $textUse){exit 1}"
if errorlevel 1 (
    echo cgen_class_field_module_const.c has an invalid module constant declaration order.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_class_field_module_const.c %XL6_CGEN_LINKS% -o build\test\cgen_class_field_module_const.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_field_module_const.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe check test\cases\cgen_generic_self_method.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generic_self_method.xl --emit build\test\cgen_generic_self_method.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Ring__int__reset(self, initial);" build\test\cgen_generic_self_method.c >nul
if errorlevel 1 (
    echo cgen_generic_self_method.c lost the concrete generic class symbol.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_generic_self_method.c %XL6_CGEN_LINKS% -o build\test\cgen_generic_self_method.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_self_method.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

rem ------------------------------------------------------------------
rem class: default reference semantics, polymorphism and parameter modes.
rem ------------------------------------------------------------------
release\xl.exe cgen test\cases\cgen_property_managed_chain.xl --emit build\test\cgen_property_managed_chain.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_temp_managed_release((void*)__xl6_call_receiver_" build\test\cgen_property_managed_chain.c >nul
if errorlevel 1 (
    echo cgen_property_managed_chain.c lost managed getter receiver disown.
    exit /b 1
)

release\xl.exe check test\cases\cgen_imported_class_method_nested_arg.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_imported_class_method_nested_arg.xl --emit build\test\cgen_imported_class_method_nested_arg.c
if errorlevel 1 exit /b 1
findstr /C:"XL6_Function __xl6_member_fn_stmt_" build\test\cgen_imported_class_method_nested_arg.c >nul
if not errorlevel 1 (
    echo cgen_imported_class_method_nested_arg.c lowered an imported class method as a function field.
    exit /b 1
)

for %%F in (
    cgen_class_polymorphism
    cgen_class_null_identity
    cgen_class_self_property_dispatch
    cgen_property_managed_chain
    cgen_imported_class_method_nested_arg
    cgen_param_modes_class
    cgen_param_modes_container
    cgen_param_modes_container_matrix
    cgen_union_value_semantics
    cgen_out_definite_assignment
    cgen_function_param_modes
) do (
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
    if not errorlevel 0 exit /b 1
)

release\xl.exe cgen test\cases\cgen_function_param_modes.xl --emit build\test\cgen_function_param_modes.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_1(int64_t" build\test\cgen_function_param_modes.c >nul
if errorlevel 1 (
    echo cgen_function_param_modes.c lost the by-value integer parameter ABI.
    exit /b 1
)
findstr /C:"static int64_t xlir_fn_2(void)" build\test\cgen_function_param_modes.c >nul
if errorlevel 1 (
    echo cgen_function_param_modes.c lost the zero-argument value-return ABI.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_polymorphism.xl --emit build\test\cgen_class_polymorphism.c
if errorlevel 1 exit /b 1
findstr /C:"xl6ValueObject(xl6_type_desc_XL_Class_RefDerived()" build\test\cgen_class_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_polymorphism.c missing managed derived object allocation.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_append_value_release(__xl6_user_global_values," build\test\cgen_class_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_polymorphism.c missing inline class container append.
    exit /b 1
)
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\cgen_class_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_polymorphism.c lost the shared runtime header.
    exit /b 1
)
findstr /C:"XL6_XRT_TYPE_KIND_CLASS ) {" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h may overwrite concrete class runtime descriptors.
    exit /b 1
)

for %%F in (
    cgen_class_handle_lifetime_overload
    cgen_class_generic_handle
    cgen_class_copy
    cgen_class_fresh_return
) do (
    release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_destroy_host.c -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
    if not errorlevel 0 exit /b 1
)

release\xl.exe check test\cases\cgen_class_null_dereference.xl > build\test\cgen_class_null_dereference.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_class_null_dereference.xl should reject an un-narrowed nullable receiver.
    exit /b 1
)
findstr /C:"nullable receiver must be narrowed" build\test\cgen_class_null_dereference.err.txt >nul
if errorlevel 1 (
    echo cgen_class_null_dereference.xl lost the nullable member-access diagnostic.
    exit /b 1
)

release\xl.exe abi test\cases\export_class_handle.xl --json --emit build\test\export_class_handle.abi.json
if errorlevel 1 exit /b 1
findstr /C:"fn:app.makeHandle(int) as ExportHandleBase" build\test\export_class_handle.abi.json >nul
if errorlevel 1 (
    echo export_class_handle.abi.json lost the canonical class return type.
    exit /b 1
)
findstr /C:"fn:app.readHandle(ExportHandleBase) as int" build\test\export_class_handle.abi.json >nul
if errorlevel 1 (
    echo export_class_handle.abi.json lost the canonical class parameter type.
    exit /b 1
)
release\xl.exe cgen test\cases\export_class_handle.xl --emit build\test\export_class_handle.c --emit-header build\test\export_class_handle.h
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl6_class_make(int64_t value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_class_read(xvalue* value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_mode_increment(int64_t value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl6_mode_make(void);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"fn:app.increment(int) as int" build\test\export_class_handle.abi.json >nul
if errorlevel 1 exit /b 1
findstr /C:"fn:app.makeOut() as ExportHandleBase" build\test\export_class_handle.abi.json >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\export_class_handle.xl test\cases\export_class_handle_header_host.c -o build\test\export_class_handle_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_handle_header_host.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe check test\cases\import_package_class_handle_use.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_package_class_handle_use.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe build test\cases\import_package_class_handle_use.xl -o build\test\import_package_class_handle_use.exe
if errorlevel 1 exit /b 1
build\test\import_package_class_handle_use.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe run test\cases\cgen_class_recursive_object.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe build test\cases\cgen_class_recursive_object.xl -o build\test\cgen_class_recursive_object.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_recursive_object.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe check test\cases\cgen_class_self_free_call.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_class_self_free_call.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe build test\cases\cgen_class_self_free_call.xl -o build\test\cgen_class_self_free_call.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_self_free_call.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe run release\examples\class_reference_model.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe run release\examples\typed_point.xl
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1

release\xl.exe check test\cases\sem_struct_removed.xl > build\test\sem_struct_removed.err.txt 2>&1
if not errorlevel 1 (
    echo sem_struct_removed.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_struct_removed.err.txt >nul
if errorlevel 1 (
    echo sem_struct_removed.xl missing ordinary syntax diagnostic.
    exit /b 1
)

for %%F in (
    sem_closure_capture_destroy_class
    sem_closure_capture_native_class
    sem_closure_capture_native_class_field
    sem_noncopyable_class_copy_init
    sem_noncopyable_assign
    sem_noncopyable_native_class_param
    sem_typed_container_noncopyable_native_class
    sem_typed_container_noncopyable_class
    sem_typed_container_noncopyable_nested
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\sem_param_modes_invalid.xl > build\test\sem_param_modes_invalid.err.txt 2>&1
if not errorlevel 1 (
    echo sem_param_modes_invalid.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_param_modes_invalid.err.txt >nul
if errorlevel 1 (
    echo sem_param_modes_invalid.xl missing removed-syntax diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_language_param_modes_rejected.xl > build\test\sem_language_param_modes_rejected.err.txt 2>&1
if not errorlevel 1 (
    echo sem_language_param_modes_rejected.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_language_param_modes_rejected.err.txt >nul
if errorlevel 1 exit /b 1

for %%F in (syntax_byval_param_removed syntax_byref_param_removed syntax_out_param_removed syntax_byval_arg_removed syntax_byref_arg_removed syntax_out_arg_removed) do (
    release\xl.exe check test\cases\%%F.xl > build\test\%%F.err.txt 2>&1
    if not errorlevel 1 exit /b 1
    findstr /C:"[parse]" build\test\%%F.err.txt >nul
    if errorlevel 1 exit /b 1
)
release\xl.exe run test\cases\syntax_mode_names_ordinary.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_foreign_pointer_callable_escape.xl > build\test\sem_foreign_pointer_callable_escape.err.txt 2>&1
if not errorlevel 1 (
    echo sem_foreign_pointer_callable_escape.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_foreign_pointer_callable_escape.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_foreign_pointer_var_escape.xl > build\test\sem_foreign_pointer_var_escape.err.txt 2>&1
if not errorlevel 1 (
    echo sem_foreign_pointer_var_escape.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_foreign_pointer_var_escape.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_foreign_pointer_func_escape.xl > build\test\sem_foreign_pointer_func_escape.err.txt 2>&1
if not errorlevel 1 (
    echo sem_foreign_pointer_func_escape.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_foreign_pointer_func_escape.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_foreign_member_pointer_callable_escape.xl > build\test\sem_foreign_member_pointer_callable_escape.err.txt 2>&1
if not errorlevel 1 (
    echo sem_foreign_member_pointer_callable_escape.xl should fail.
    exit /b 1
)
findstr /C:"[parse]" build\test\sem_foreign_member_pointer_callable_escape.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_typed_point.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_typed_point.xl -o build\test\cgen_typed_point.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_point.exe
if errorlevel 1 exit /b 1
if not errorlevel 0 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_point.xl --emit build\test\cgen_typed_point.c
if errorlevel 1 exit /b 1
findstr /C:"void* xl_fn_keep(void* value);" build\test\cgen_typed_point.c >nul
if errorlevel 1 (
    echo cgen_typed_point.c lost the zero-cost void pointer ABI.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_point_mismatch.xl > build\test\sem_typed_point_mismatch.err.txt 2>&1
if not errorlevel 1 (
    echo sem_typed_point_mismatch.xl should reject different point tags.
    exit /b 1
)
findstr /C:"target point<AudioTag>, value point<ImageTag>" build\test\sem_typed_point_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_typed_point_mismatch.xl lost concrete typed point diagnostics.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_missing_operator_protocol.xl --emit build\test\cgen_class_missing_operator_protocol.c > build\test\cgen_class_missing_operator_protocol.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_class_missing_operator_protocol.xl should fail.
    exit /b 1
)
findstr /C:"class operator requires matching protocol method" build\test\cgen_class_missing_operator_protocol.err.txt >nul
if errorlevel 1 (
    echo cgen_class_missing_operator_protocol.xl missing protocol diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_missing_operator_protocol_right.xl --emit build\test\cgen_class_missing_operator_protocol_right.c > build\test\cgen_class_missing_operator_protocol_right.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_class_missing_operator_protocol_right.xl should fail.
    exit /b 1
)
findstr /C:"class operator requires matching protocol method" build\test\cgen_class_missing_operator_protocol_right.err.txt >nul
if errorlevel 1 (
    echo cgen_class_missing_operator_protocol_right.xl missing protocol diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_base_method_static_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_base_method_static_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_base_method_static_dispatch.xl --emit build\test\cgen_base_method_static_dispatch.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_ParentSpeaker__speak(&self->base)" build\test\cgen_base_method_static_dispatch.c >nul
if errorlevel 1 (
    echo cgen_base_method_static_dispatch.c did not emit direct parent dispatch.
    exit /b 1
)
release\xl.exe build test\cases\cgen_base_method_static_dispatch.xl -o build\test\cgen_base_method_static_dispatch.exe
if errorlevel 1 exit /b 1
build\test\cgen_base_method_static_dispatch.exe
if errorlevel 1 exit /b 1

rem Dynamic var receivers must prefer real class methods and retain container methods as fallback.
for %%F in (
    cgen_global_var_member_call
    cgen_dynamic_method_overload
    cgen_dynamic_method_inheritance
    cgen_dynamic_method_bound_lifetime
    cgen_dynamic_method_arguments
    cgen_generic_dynamic_method
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

echo Verifying XLIR dynamic method argument dispatch...
release\xl.exe cgen test\cases\cgen_dynamic_method_arguments.xl --backend xlir --emit build\test\cgen_dynamic_method_arguments.c
if errorlevel 1 (
    echo XLIR Cgen failed for dynamic method arguments.
    exit /b 1
)
echo XLIR dynamic method argument Cgen ok.
findstr /C:"defaultResult = xl6_var_call(" build\test\cgen_dynamic_method_arguments.c >nul
if errorlevel 1 (
    echo cgen_dynamic_method_arguments.c lost dynamic var return typing.
    exit /b 1
)
findstr /C:"defaultResult = xvoCreateBool(xl6_var_call(" build\test\cgen_dynamic_method_arguments.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_method_arguments.c wrapped a dynamic return as bool.
    exit /b 1
)

rem XLIR owns dynamic method binding/default/variadic semantics as verified IR.
release\xl.exe ir test\cases\cgen_dynamic_method_arguments.xl --lowered > build\test\cgen_dynamic_method_arguments_xlir.txt
if errorlevel 1 (
    echo Lowered XLIR generation failed for dynamic method arguments.
    exit /b 1
)
echo Lowered dynamic method argument XLIR ok.
findstr /C:"name=right type=t3 abi=value match=i64 default=n12/i64 literal=2LL" build\test\cgen_dynamic_method_arguments_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR lost the closed dynamic-method default descriptor.
    exit /b 1
)
findstr /C:"member=add fallback=add" build\test\cgen_dynamic_method_arguments_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR lost class-first dynamic method collection fallback.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_dynamic_method_arguments.xl --backend xlir --emit build\test\cgen_dynamic_method_arguments_xlir_generated.c
if errorlevel 1 (
    echo XLIR Cgen failed for dynamic method arguments.
    exit /b 1
)
echo Dynamic method argument XLIR Cgen ok.
findstr /C:"static xvalue* xlir_dynamic_method_dm0(" build\test\cgen_dynamic_method_arguments_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not emit the verified dynamic-method adapter.
    exit /b 1
)
findstr /C:"else a1 = 2LL;" build\test\cgen_dynamic_method_arguments_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not consume the canonical dynamic-method default.
    exit /b 1
)
findstr /C:"xlir_xvalue_dynamic_collection_method(receiver, XLIR_DYNAMIC_COLLECTION_ADD" build\test\cgen_dynamic_method_arguments_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not consume the explicit dynamic collection fallback.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_dynamic_method_arguments_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_method_arguments_host.exe
if errorlevel 1 (
    echo XLIR dynamic method argument host compile failed.
    exit /b 1
)
build\test\xlir_dynamic_method_arguments_host.exe
if errorlevel 1 (
    echo XLIR dynamic method argument host runtime failed.
    exit /b 1
)

release\xl.exe ir test\cases\cgen_dynamic_method_overload.xl --lowered > build\test\cgen_dynamic_method_overload_xlir.txt
if errorlevel 1 exit /b 1
findstr /C:"name=value type=t4 abi=value match=f64" build\test\cgen_dynamic_method_overload_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR lost the exact dynamic overload match ABI.
    exit /b 1
)
findstr /C:"name=pick returns=1/t3 targetError=may-raise adapter=verified" build\test\cgen_dynamic_method_overload_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR did not preserve the may-raise overload target ABI.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_dynamic_method_overload.xl --backend xlir --emit build\test\cgen_dynamic_method_overload_xlir_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static bool xlir_dynamic_method_match_dm1(" build\test\cgen_dynamic_method_overload_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not emit non-mutating overload selection.
    exit /b 1
)
findstr /C:"callResult = xlir_fn_3(receiver, a0);" build\test\cgen_dynamic_method_overload_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not consume the may-raise overload result carrier.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_dynamic_method_overload_host.c %XL6_CGEN_LINKS% -o build\test\xlir_dynamic_method_overload_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_dynamic_method_overload_host.exe
if errorlevel 1 exit /b 1

rem A dynamic call on Generic<T> must dispatch to an isolated monomorphic
rem registry row.  Neither Cgen nor the runtime may interpret the source T.
echo Verifying XLIR generic dynamic method dispatch...
release\xl.exe ir test\cases\cgen_generic_dynamic_method.xl --lowered > build\test\cgen_generic_dynamic_method_xlir.txt
if errorlevel 1 exit /b 1
findstr /C:"name=get returns=1/t6 targetError=no-raise adapter=verified" build\test\cgen_generic_dynamic_method_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR did not specialize the generic dynamic return ABI.
    exit /b 1
)
findstr /C:"name=next type=t6 abi=value match=i64" build\test\cgen_generic_dynamic_method_xlir.txt >nul
if errorlevel 1 (
    echo Lowered XLIR did not specialize the generic dynamic parameter ABI.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_generic_dynamic_method.xl --backend xlir --emit build\test\cgen_generic_dynamic_method_xlir_generated.c
if errorlevel 1 exit /b 1
findstr /C:"static xvalue* xlir_dynamic_method_dm0(" build\test\cgen_generic_dynamic_method_xlir_generated.c >nul
if errorlevel 1 (
    echo XLIR Cgen did not emit the specialized generic dynamic adapter.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_generic_dynamic_method_host.c %XL6_CGEN_LINKS% -o build\test\xlir_generic_dynamic_method_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_generic_dynamic_method_host.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: current OOP and parameter-mode regressions
exit /b 0
