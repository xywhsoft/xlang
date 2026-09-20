@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 1/10...

release\xl.exe cgen test\cases\cgen_array_negative_index.xl --emit build\test\cgen_array_negative_index.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_array_set_i64_new" build\test\cgen_array_negative_index.c >nul
if errorlevel 1 (
    echo cgen_array_negative_index.c missing signed array write helper.
    exit /b 1
)
findstr /C:"xrtValueArrayAt(" build\test\cgen_array_negative_index.c >nul
if errorlevel 1 (
    echo cgen_array_negative_index.c missing signed typed array read.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_resolve_index" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h missing typed array negative index resolver.
    exit /b 1
)
release\xl.exe run test\cases\cgen_array_negative_index.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_array_negative_index.xl -o build\test\cgen_array_negative_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_array_negative_index.exe
if errorlevel 1 exit /b 1

release\xl.exe ir release\examples\hello.xl > build\test\hello.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=function-declaration/4 source=func/3" build\test\hello.xlir >nul
if errorlevel 1 (
    echo hello.xlir missing typed function declaration.
    exit /b 1
)
findstr /C:"text=add @" build\test\hello.xlir >nul
if errorlevel 1 (
    echo hello.xlir missing function name binding.
    exit /b 1
)
release\xl.exe ir release\examples\hello.xl --json --emit build\test\hello.ir.json
if errorlevel 1 exit /b 1
findstr /C:"stage" build\test\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing stage field.
    exit /b 1
)
findstr /C:"add" build\test\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing function name.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow.xl --emit build\test\control_flow.xlir
if errorlevel 1 exit /b 1
findstr /C:"loop=range-i64-index-value" build\test\control_flow.xlir >nul
if errorlevel 1 (
    echo control_flow.xlir missing typed range index/value loop ABI.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_do_while.xl --emit build\test\control_flow_do_while.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=loop/19 source=do_while/72" build\test\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing do block.
    exit /b 1
)
findstr /C:"kind=binary/31 source=binary/58" build\test\control_flow_do_while.xlir | findstr /C:"opcode=lt" >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing typed loop condition.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_inline_if.xl --emit build\test\control_flow_inline_if.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=return/12 source=return/34" build\test\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing typed inline return.
    exit /b 1
)
findstr /C:"kind=if/18 source=if/69" build\test\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing typed inline conditional.
    exit /b 1
)

release\xl.exe ir test\cases\operators.xl --emit build\test\operators.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=assign/29 source=assign/55" build\test\operators.xlir | findstr /C:"opcode=add" >nul
if errorlevel 1 (
    echo operators.xlir missing typed compound assignment.
    exit /b 1
)
findstr /C:"kind=binary/31 source=binary/58" build\test\operators.xlir | findstr /C:"opcode=null-coalesce" >nul
if errorlevel 1 (
    echo operators.xlir missing typed null coalesce expression.
    exit /b 1
)

release\xl.exe ir test\cases\member_index.xl --emit build\test\member_index.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=member/27 source=member/48" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing typed member access.
    exit /b 1
)
findstr /C:"kind=member/27 source=safe_member/49" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing typed safe member access.
    exit /b 1
)
findstr /C:"kind=index/28 source=index/50" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing typed index access.
    exit /b 1
)
findstr /C:"kind=index/28 source=safe_index/51" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing typed safe index access.
    exit /b 1
)

release\xl.exe ir test\cases\function_types.xl --emit build\test\function_types.xlir
if errorlevel 1 exit /b 1
findstr /C:"callable=fixed" build\test\function_types.xlir | findstr /C:"params=[t4/default/value,t4/default/value]" >nul
if errorlevel 1 (
    echo function_types.xlir missing fixed binary callable ABI.
    exit /b 1
)
findstr /C:"name=__tuple" build\test\function_types.xlir | findstr /C:"params=[t1,t4]" >nul
if errorlevel 1 (
    echo function_types.xlir missing typed multi-return tuple.
    exit /b 1
)

release\xl.exe ir test\cases\anonymous_functions.xl --emit build\test\anonymous_functions.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=function-declaration/4 source=anon_func/5" build\test\anonymous_functions.xlir | findstr /C:"text=func @1:12" >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing typed arrow anonymous func.
    exit /b 1
)
findstr /C:"kind=function-declaration/4 source=anon_func/5" build\test\anonymous_functions.xlir | findstr /C:"text=func @8:12" >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing typed captured anonymous func return.
    exit /b 1
)

release\xl.exe ir test\cases\class_native_backed.xl --emit build\test\class_native_backed.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=type-declaration/5 source=class/13" build\test\class_native_backed.xlir | findstr /C:"text=Point @" >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing typed class declaration.
    exit /b 1
)
findstr /C:"kind=field-declaration/8 source=field/19" build\test\class_native_backed.xlir | findstr /C:"text=x @" >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing typed class field.
    exit /b 1
)
findstr /C:"kind=function-declaration/4 source=native_extern_func/24" build\test\class_native_backed.xlir | findstr /C:"text=\"bag_count\" @" >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing typed native-backed class extern method.
    exit /b 1
)

release\xl.exe ir test\cases\generics.xl --emit build\test\generics.xlir
if errorlevel 1 exit /b 1
findstr /C:"source=type_params/25" build\test\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing typed generic parameters.
    exit /b 1
)
findstr /C:"kind=type-declaration/5 source=class/13" build\test\generics.xlir | findstr /C:"text=Box @" >nul
if errorlevel 1 (
    echo generics.xlir missing typed generic class.
    exit /b 1
)
findstr /C:"kind=call/26 source=generic_instant/52" build\test\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing typed explicit generic instantiation.
    exit /b 1
)

release\xl.exe ir test\cases\overloads.xl --emit build\test\overloads.xlir
if errorlevel 1 exit /b 1
findstr /C:"kind=parameter-declaration/9 source=param/28" build\test\overloads.xlir | findstr /C:"text=value @1:15" >nul
if errorlevel 1 (
    echo overloads.xlir missing typed int overload parameter.
    exit /b 1
)
findstr /C:"kind=parameter-declaration/9 source=param/28" build\test\overloads.xlir | findstr /C:"text=value @5:18" >nul
if errorlevel 1 (
    echo overloads.xlir missing typed string overload parameter.
    exit /b 1
)

release\xl.exe ir test\cases\overload_varargs.xl --emit build\test\overload_varargs.xlir
if errorlevel 1 exit /b 1
findstr /C:"call-site c0" build\test\overload_varargs.xlir | findstr /C:"fixed=1 variadic=none" >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing fixed overload call binding.
    exit /b 1
)
findstr /C:"call-site c1" build\test\overload_varargs.xlir | findstr /C:"fixed=1 variadic=array" >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing array variadic call binding.
    exit /b 1
)
findstr /C:"call-site c2" build\test\overload_varargs.xlir | findstr /C:"fixed=1 variadic=dict" >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing dict variadic call binding.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_basic.xl --emit build\test\cgen_basic.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t xlir_fn_1(int64_t" build\test\cgen_basic.c >nul
if errorlevel 1 (
    echo cgen_basic.c missing XLIR-lowered add prototype.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -c build\test\cgen_basic.c -o build\test\cgen_basic.o
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_basic.xl -o build\test\cgen_basic.exe
if errorlevel 1 exit /b 1
build\test\cgen_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_module_globals.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_module_globals.xl -o build\test\cgen_module_globals.exe
if errorlevel 1 exit /b 1
build\test\cgen_module_globals.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_class_static_only.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_class_static_only.xl -o build\test\cgen_class_static_only.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_static_only.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_issue_dynamic_array_concat.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_multi_return_typed_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_multi_return_typed_containers.xl -o build\test\cgen_multi_return_typed_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_multi_return_typed_containers.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_multi_return_typed_containers.xl --emit build\test\cgen_multi_return_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"struct xvalue* e0;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"struct xvalue* e1;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"struct xvalue* e2;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"struct xvalue* e3;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtValueRelease(v" build\test\cgen_multi_return_typed_containers.c | findstr /C:".e0);" >nul
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_dynamic_to_typed_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_dynamic_to_typed_containers.xl --emit build\test\cgen_dynamic_to_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_collection_checked_take(" build\test\cgen_dynamic_to_typed_containers.c | findstr /C:", 9, 2)" >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_collection_checked_take(" build\test\cgen_dynamic_to_typed_containers.c | findstr /C:", 10, 4)" >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_collection_checked_take(" build\test\cgen_dynamic_to_typed_containers.c | findstr /C:", 11, 2)" >nul
if errorlevel 1 exit /b 1
findstr /C:"xlir_xvalue_collection_checked_take(" build\test\cgen_dynamic_to_typed_containers.c | findstr /C:", 12, 2)" >nul
if errorlevel 1 exit /b 1
findstr /C:"static xvalue* s56;" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_dynamic_to_typed_containers.c missing XLIR module-global storage.
    exit /b 1
)
findstr /C:"s56 = v3;" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_dynamic_to_typed_containers.c missing validated module-global ownership transfer.
    exit /b 1
)
findstr /C:"dynamic container contains an incompatible value" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_dynamic_to_typed_containers.c missing checked conversion error path.
    exit /b 1
)
release\xl.exe run test\cases\cgen_dynamic_to_typed_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_dynamic_to_typed_containers.xl -o build\test\cgen_dynamic_to_typed_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_dynamic_to_typed_containers.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_var_string_concat.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_var_string_concat.xl -o build\test\cgen_var_string_concat.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_string_concat.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_var_to_concrete_implicit.xl >nul 2> build\test\sem_var_to_concrete_implicit.err.txt
if not errorlevel 1 (
    echo sem_var_to_concrete_implicit.xl should fail.
    exit /b 1
)
findstr /C:"target string, value var" build\test\sem_var_to_concrete_implicit.err.txt >nul
if errorlevel 1 (
    echo sem_var_to_concrete_implicit.err.txt missing var-to-concrete diagnostic context.
    exit /b 1
)
powershell -NoProfile -Command "$m = Select-String -LiteralPath 'build\test\sem_var_to_concrete_implicit.err.txt' -SimpleMatch 'target string, value var'; if ($m.Count -ne 3) { exit 1 }"
if errorlevel 1 (
    echo sem_var_to_concrete_implicit.err.txt should cover declaration, assignment and return.
    exit /b 1
)
findstr /C:"dynamic var argument requires explicit 'as Type' conversion before overload resolution" build\test\sem_var_to_concrete_implicit.err.txt >nul
if errorlevel 1 (
    echo sem_var_to_concrete_implicit.err.txt missing explicit-conversion call diagnostic.
    exit /b 1
)
release\xl.exe run test\cases\cgen_issue_json_typed_class_array.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_issue_list_json_dict_remove_type.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_dict_json_order.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_dict.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_dict.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_container_index_errors.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_dev_feedback_priority.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_dev_feedback_priority.xl -o build\test\cgen_dev_feedback_priority.exe
if errorlevel 1 exit /b 1
build\test\cgen_dev_feedback_priority.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_overload_var_unique.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_overload_var_unique.xl -o build\test\cgen_overload_var_unique.exe
if errorlevel 1 exit /b 1
build\test\cgen_overload_var_unique.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_dev_handfeel_regression.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_dev_handfeel_regression.xl -o build\test\cgen_dev_handfeel_regression.exe
if errorlevel 1 exit /b 1
build\test\cgen_dev_handfeel_regression.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File test\suites\check_cgen_issue_runtime.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_default_init.xl --emit build\test\cgen_default_init.c
if errorlevel 1 exit /b 1
findstr /C:"static int64_t s3;" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing int default init.
    exit /b 1
)
findstr /C:"xrtStrDup(\"\")" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing string default init.
    exit /b 1
)
findstr /C:"static void* s7;" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing point default init.
    exit /b 1
)
release\xl.exe build test\cases\cgen_default_init.xl test\cases\cgen_class_host.c -o build\test\cgen_default_init.exe
if errorlevel 1 exit /b 1
build\test\cgen_default_init.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union.xl --emit build\test\cgen_union.c
if errorlevel 1 exit /b 1
findstr /C:"typedef union xlir_union_t" build\test\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing top-level union typedef.
    exit /b 1
)
findstr /C:"xlir_xvalue_union_box(" build\test\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing verified union boxing ABI.
    exit /b 1
)
release\xl.exe build test\cases\cgen_union.xl test\cases\cgen_class_host.c -o build\test\cgen_union.exe
if errorlevel 1 exit /b 1
build\test\cgen_union.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union_var_box.xl --emit build\test\cgen_union_var_box.c
if errorlevel 1 exit /b 1
findstr /C:"typedef union xlir_union_t" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing verified union storage ABI.
    exit /b 1
)
findstr /C:"xlir_xvalue_union_box(" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union boxing ABI.
    exit /b 1
)
findstr /C:"xlir_xvalue_union_unbox(" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union unboxing ABI.
    exit /b 1
)
findstr /C:"xrtValueArrayAppendNew(" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing owned typed-array append.
    exit /b 1
)
findstr /C:"xrtValueSetAddNew(" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing owned typed-set insert.
    exit /b 1
)
findstr /C:"xlir_xvalue_type_bind(" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing canonical runtime TypeId binding.
    exit /b 1
)
release\xl.exe build test\cases\cgen_union_var_box.xl test\cases\cgen_class_host.c -o build\test\cgen_union_var_box.exe
if errorlevel 1 exit /b 1
build\test\cgen_union_var_box.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union.xl --dump-dir build\test\dump_export_union
if errorlevel 1 exit /b 1
findstr /C:"typedef union xlir_union_t" build\test\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union typedef.
    exit /b 1
)
findstr /C:"xlir_union_t" build\test\dump_export_union\export_union.h | findstr /C:" p);" >nul
if errorlevel 1 (
    echo export_union.h missing union parameter.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_top_anon.xl >nul 2> build\test\sem_union_top_anon.err.txt
if not errorlevel 1 (
    echo sem_union_top_anon.xl should fail.
    exit /b 1
)
findstr /C:"top-level union must have a name" build\test\sem_union_top_anon.err.txt >nul
if errorlevel 1 (
    echo sem_union_top_anon.err.txt missing named top-level union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_empty.xl >nul 2> build\test\sem_union_empty.err.txt
if not errorlevel 1 (
    echo sem_union_empty.xl should fail.
    exit /b 1
)
findstr /C:"union must contain at least one field" build\test\sem_union_empty.err.txt >nul
if errorlevel 1 (
    echo sem_union_empty.err.txt missing empty union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_default.xl >nul 2> build\test\sem_union_default.err.txt
if not errorlevel 1 (
    echo sem_union_default.xl should fail.
    exit /b 1
)
findstr /C:"union fields do not support default values" build\test\sem_union_default.err.txt >nul
if errorlevel 1 (
    echo sem_union_default.err.txt missing union default diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_duplicate_field.xl >nul 2> build\test\sem_union_duplicate_field.err.txt
if not errorlevel 1 (
    echo sem_union_duplicate_field.xl should fail.
    exit /b 1
)
findstr /C:"duplicate union field name" build\test\sem_union_duplicate_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_duplicate_field.err.txt missing duplicate union field diagnostic.
    exit /b 1
)

echo DEMO6 suite ok: core C generation and runtime baseline 1/10
exit /b 0
