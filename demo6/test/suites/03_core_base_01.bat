@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 1/10...

release\xl.exe cgen test\cases\cgen_array_negative_index.xl --emit build\test\cgen_array_negative_index.c
if errorlevel 1 exit /b 1
findstr /C:"xvoIndexSetI64" build\test\cgen_array_negative_index.c >nul
if errorlevel 1 (
    echo cgen_array_negative_index.c missing signed array write helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_get_at(typedValues, (long long)((-1LL)))" build\test\cgen_array_negative_index.c >nul
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
findstr /C:"func add(a: int, b: int) as int" build\test\hello.xlir >nul
if errorlevel 1 (
    echo hello.xlir missing func signature.
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
findstr /C:"for index, value in call range(1, 10, 2)" build\test\control_flow.xlir >nul
if errorlevel 1 (
    echo control_flow.xlir missing range for loop.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_do_while.xl --emit build\test\control_flow_do_while.xlir
if errorlevel 1 exit /b 1
findstr /C:"do" build\test\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing do block.
    exit /b 1
)
findstr /C:"while (value < 3)" build\test\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing while condition.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_inline_if.xl --emit build\test\control_flow_inline_if.xlir
if errorlevel 1 exit /b 1
findstr /C:"return 1" build\test\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline return.
    exit /b 1
)
findstr /C:"else" build\test\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline else.
    exit /b 1
)

release\xl.exe ir test\cases\operators.xl --emit build\test\operators.xlir
if errorlevel 1 exit /b 1
findstr /C:"a += 2" build\test\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing compound assignment.
    exit /b 1
)
findstr /C:"null ?? 9" build\test\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing null coalesce expression.
    exit /b 1
)

release\xl.exe ir test\cases\member_index.xl --emit build\test\member_index.xlir
if errorlevel 1 exit /b 1
findstr /C:"obj.name" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing member access.
    exit /b 1
)
findstr /C:"obj?.name" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe member access.
    exit /b 1
)
findstr /C:"obj[0]" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing index access.
    exit /b 1
)
findstr /C:"obj?[1]" build\test\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe index access.
    exit /b 1
)

release\xl.exe ir test\cases\function_types.xl --emit build\test\function_types.xlir
if errorlevel 1 exit /b 1
findstr /C:"var binary : func(int, int) as int" build\test\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing func type signature.
    exit /b 1
)
findstr /C:"var parser : func(string) as (bool, int)" build\test\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing multi-return func type.
    exit /b 1
)

release\xl.exe ir test\cases\anonymous_functions.xl --emit build\test\anonymous_functions.xlir
if errorlevel 1 exit /b 1
findstr /C:"var inc : auto = func(value: int) as int <body>" build\test\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing arrow anonymous func.
    exit /b 1
)
findstr /C:"return func(value: int) as int <body>" build\test\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing captured anonymous func return.
    exit /b 1
)

release\xl.exe ir test\cases\class_native_backed.xl --emit build\test\class_native_backed.xlir
if errorlevel 1 exit /b 1
findstr /C:"class Point" build\test\class_native_backed.xlir >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing class declaration.
    exit /b 1
)
findstr /C:"field x : int = 0" build\test\class_native_backed.xlir >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing class field.
    exit /b 1
)
findstr /C:"extern \"bag_count\" func count() as int" build\test\class_native_backed.xlir >nul
if errorlevel 1 (
    echo class_native_backed.xlir missing native-backed class extern method.
    exit /b 1
)

release\xl.exe ir test\cases\generics.xl --emit build\test\generics.xlir
if errorlevel 1 exit /b 1
findstr /C:"func identity<T>(value: T) as T" build\test\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic func.
    exit /b 1
)
findstr /C:"class Box<T>" build\test\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic class.
    exit /b 1
)
findstr /C:"identity::<int>" build\test\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing explicit generic instantiation.
    exit /b 1
)

release\xl.exe ir test\cases\overloads.xl --emit build\test\overloads.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: int) as int" build\test\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing int overload.
    exit /b 1
)
findstr /C:"func pick(value: string) as string" build\test\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing string overload.
    exit /b 1
)

release\xl.exe ir test\cases\overload_varargs.xl --emit build\test\overload_varargs.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: string) as int" build\test\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing fixed overload.
    exit /b 1
)
findstr /C:"func pick(value: string, arg: array<var>) as int" build\test\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing array vararg overload.
    exit /b 1
)
findstr /C:"func options(path: string, arg: dict<var>) as int" build\test\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing dict vararg overload.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_basic.xl --emit build\test\cgen_basic.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_fn_add(long long a, long long b);" build\test\cgen_basic.c >nul
if errorlevel 1 (
    echo cgen_basic.c missing add prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_basic.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_basic.exe
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
findstr /C:"xtarray value0;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xtlist value1;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xtset value2;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xtdict value3;" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayDestroy(__xl6_multi_" build\test\cgen_multi_return_typed_containers.c >nul
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_dynamic_to_typed_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_dynamic_to_typed_containers.xl --emit build\test\cgen_dynamic_to_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_from_value_release(" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_list_from_value_release(" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_set_from_value_release(" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_dict_from_value_release(" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 exit /b 1
findstr /C:"XRT_OBJMODE_SHARED" build\test\cgen_dynamic_to_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_dynamic_to_typed_containers.c missing shared module-global import.
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
findstr /C:"long long count;" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing int default init.
    exit /b 1
)
findstr /C:"name = xrtCopyStr(\"\", 0u);" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing string default init.
    exit /b 1
)
findstr /C:"void* handle;" build\test\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing point default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_default_init.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_default_init.exe
if errorlevel 1 exit /b 1
build\test\cgen_default_init.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union.xl --emit build\test\cgen_union.c
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" build\test\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing top-level union typedef.
    exit /b 1
)
findstr /C:"union {" build\test\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing inline class union.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_union.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_union.exe
if errorlevel 1 exit /b 1
build\test\cgen_union.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union_var_box.xl --emit build\test\cgen_union_var_box.c
if errorlevel 1 exit /b 1
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Union_Payload" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union XRT type descriptor.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Union_Payload" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Union_Payload" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union hash type op.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(p)" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union var boxing helper call.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xl6_type_desc_XL_Union_Payload()" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array descriptor.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xl6_type_desc_XL_Union_Payload(), copied)" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array boxing.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xl6_type_desc_XL_Union_Payload()" build\test\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union set descriptor.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_union_var_box.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_union_var_box.exe
if errorlevel 1 exit /b 1
build\test\cgen_union_var_box.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_inline_union_generic_class_field.xl --emit build\test\cgen_inline_union_generic_class_field.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" build\test\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing generic class dependency.
    exit /b 1
)
findstr /C:"XL_Class_Box__int boxed;" build\test\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic class field.
    exit /b 1
)
findstr /C:"XL_Class_Box__T" build\test\cgen_inline_union_generic_class_field.c >nul
if not errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c leaked unresolved generic class field.
    exit /b 1
)
findstr /C:"packet.boxed.value = 41LL;" build\test\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic member write.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_inline_union_generic_class_field.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1
build\test\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union.xl --dump-dir build\test\dump_export_union
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" build\test\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload p" build\test\dump_export_union\export_union.h >nul
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

