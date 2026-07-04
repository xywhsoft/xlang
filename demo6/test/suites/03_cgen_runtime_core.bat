@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime...

release\xl.exe ir release\examples\hello.xl > release\tmp\hello.xlir
if errorlevel 1 exit /b 1
findstr /C:"func add(a: int, b: int) as int" release\tmp\hello.xlir >nul
if errorlevel 1 (
    echo hello.xlir missing func signature.
    exit /b 1
)
release\xl.exe ir release\examples\hello.xl --json --emit release\tmp\hello.ir.json
if errorlevel 1 exit /b 1
findstr /C:"stage" release\tmp\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing stage field.
    exit /b 1
)
findstr /C:"add" release\tmp\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing function name.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow.xl --emit release\tmp\control_flow.xlir
if errorlevel 1 exit /b 1
findstr /C:"for index, value in call range(1, 10, 2)" release\tmp\control_flow.xlir >nul
if errorlevel 1 (
    echo control_flow.xlir missing range for loop.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_do_while.xl --emit release\tmp\control_flow_do_while.xlir
if errorlevel 1 exit /b 1
findstr /C:"do" release\tmp\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing do block.
    exit /b 1
)
findstr /C:"while (value < 3)" release\tmp\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing while condition.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_inline_if.xl --emit release\tmp\control_flow_inline_if.xlir
if errorlevel 1 exit /b 1
findstr /C:"return 1" release\tmp\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline return.
    exit /b 1
)
findstr /C:"else" release\tmp\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline else.
    exit /b 1
)

release\xl.exe ir test\cases\operators.xl --emit release\tmp\operators.xlir
if errorlevel 1 exit /b 1
findstr /C:"a += 2" release\tmp\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing compound assignment.
    exit /b 1
)
findstr /C:"null ?? 9" release\tmp\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing null coalesce expression.
    exit /b 1
)

release\xl.exe ir test\cases\member_index.xl --emit release\tmp\member_index.xlir
if errorlevel 1 exit /b 1
findstr /C:"obj.name" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing member access.
    exit /b 1
)
findstr /C:"obj?.name" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe member access.
    exit /b 1
)
findstr /C:"obj[0]" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing index access.
    exit /b 1
)
findstr /C:"obj?[1]" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe index access.
    exit /b 1
)

release\xl.exe ir test\cases\function_types.xl --emit release\tmp\function_types.xlir
if errorlevel 1 exit /b 1
findstr /C:"var binary : func(int, int) as int" release\tmp\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing func type signature.
    exit /b 1
)
findstr /C:"var parser : func(string) as (bool, int)" release\tmp\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing multi-return func type.
    exit /b 1
)

release\xl.exe ir test\cases\anonymous_functions.xl --emit release\tmp\anonymous_functions.xlir
if errorlevel 1 exit /b 1
findstr /C:"var inc : auto = func(value: int) as int <body>" release\tmp\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing arrow anonymous func.
    exit /b 1
)
findstr /C:"return func(value: int) as int <body>" release\tmp\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing captured anonymous func return.
    exit /b 1
)

release\xl.exe ir test\cases\class_custom.xl --emit release\tmp\class_custom.xlir
if errorlevel 1 exit /b 1
findstr /C:"class Point" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing class declaration.
    exit /b 1
)
findstr /C:"field x : int = 0" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing class field.
    exit /b 1
)
findstr /C:"extern \"bag_count\" func count() as int" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing native-backed class extern method.
    exit /b 1
)

release\xl.exe ir test\cases\generics.xl --emit release\tmp\generics.xlir
if errorlevel 1 exit /b 1
findstr /C:"func identity<T>(value: T) as T" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic func.
    exit /b 1
)
findstr /C:"class Box<T>" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic class.
    exit /b 1
)
findstr /C:"identity::<int>" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing explicit generic instantiation.
    exit /b 1
)

release\xl.exe ir test\cases\overloads.xl --emit release\tmp\overloads.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: int) as int" release\tmp\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing int overload.
    exit /b 1
)
findstr /C:"func pick(value: string) as string" release\tmp\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing string overload.
    exit /b 1
)

release\xl.exe ir test\cases\overload_varargs.xl --emit release\tmp\overload_varargs.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: string) as int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing fixed overload.
    exit /b 1
)
findstr /C:"func pick(value: string, arg: array<var>) as int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing array vararg overload.
    exit /b 1
)
findstr /C:"func options(path: string, arg: dict<var>) as int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing dict vararg overload.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_basic.xl --emit release\tmp\cgen_basic.c
if errorlevel 1 exit /b 1
findstr /C:"long long add(long long a, long long b);" release\tmp\cgen_basic.c >nul
if errorlevel 1 (
    echo cgen_basic.c missing add prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_basic.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_basic.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_default_init.xl --emit release\tmp\cgen_default_init.c
if errorlevel 1 exit /b 1
findstr /C:"long long count = 0;" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing int default init.
    exit /b 1
)
findstr /C:"const char* name = \"\";" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing string default init.
    exit /b 1
)
findstr /C:"void* handle = 0;" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing point default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_default_init.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_default_init.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_default_init.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union.xl --emit release\tmp\cgen_union.c
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing top-level union typedef.
    exit /b 1
)
findstr /C:"union {" release\tmp\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing inline class union.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_union.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_union.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_union.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union_var_box.xl --emit release\tmp\cgen_union_var_box.c
if errorlevel 1 exit /b 1
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union XRT type descriptor.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union hash type op.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(p)" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union var boxing helper call.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xl6_type_desc_XL_Union_Payload()" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array descriptor.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xl6_type_desc_XL_Union_Payload(), copied)" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array boxing.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xl6_type_desc_XL_Union_Payload()" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union set descriptor.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_union_var_box.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_union_var_box.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_union_var_box.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_inline_union_generic_class_field.xl --emit release\tmp\cgen_inline_union_generic_class_field.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing generic class dependency.
    exit /b 1
)
findstr /C:"XL_Class_Box__int boxed;" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic class field.
    exit /b 1
)
findstr /C:"XL_Class_Box__T" release\tmp\cgen_inline_union_generic_class_field.c >nul
if not errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c leaked unresolved generic class field.
    exit /b 1
)
findstr /C:"packet.boxed.value = 41LL;" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic member write.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_inline_union_generic_class_field.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union.xl --dump-dir release\tmp\dump_export_union
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload p" release\tmp\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union parameter.
    exit /b 1
)
findstr /C:"\"kind\":\"union\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union type metadata.
    exit /b 1
)
findstr /C:"\"fields\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union fields metadata.
    exit /b 1
)
findstr /C:"\"name\":\"i\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union field i metadata.
    exit /b 1
)
findstr /C:"\"type\":\"c_double\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union c_double field type.
    exit /b 1
)
release\xl.exe xapi test\cases\xapi_type_fields.xl --emit release\tmp\xapi_type_fields.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"class\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class type metadata.
    exit /b 1
)
findstr /C:"\"name\":\"tag\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class field tag metadata.
    exit /b 1
)
findstr /C:"Packet tag field." release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class field doc metadata.
    exit /b 1
)
findstr /C:"\"kind\":\"desc\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing structured class field doc metadata.
    exit /b 1
)
findstr /C:"\"base\":\"Packet\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class base metadata.
    exit /b 1
)
findstr /C:"\"typeParams\":[\"T\"]" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing generic class type parameter metadata.
    exit /b 1
)
findstr /C:"\"storage\":\"inline-union\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing inline union field storage metadata.
    exit /b 1
)
findstr /C:"\"name\":\"code\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing inline union field code metadata.
    exit /b 1
)
findstr /C:"\"name\":\"ClassPayload\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing union class field owner metadata.
    exit /b 1
)
findstr /C:"\"type\":\"Point\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing union class field type metadata.
    exit /b 1
)
findstr /C:"\"name\":\"NativeView\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing native-backed class type metadata.
    exit /b 1
)
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\xapi_load_types_host.c src\abi\xl6_xapi_load.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\xapi_load_types_host.exe
if errorlevel 1 exit /b 1
release\tmp\xapi_load_types_host.exe release\tmp\xapi_type_fields.json
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\xapi_loader_defaults_host.c src\abi\xl6_xapi_load.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\xapi_loader_defaults_host.exe
if errorlevel 1 exit /b 1
release\tmp\xapi_loader_defaults_host.exe unknowns test\cases\xapi_unknown_fields.xapi.json
if errorlevel 1 exit /b 1
release\tmp\xapi_loader_defaults_host.exe unknowns test\cases\xapi_invalid_doc_string.xapi.json >nul 2> release\tmp\xapi_invalid_doc_string.err.txt
if not errorlevel 1 (
    echo xapi_invalid_doc_string.xapi.json should fail.
    exit /b 1
)
findstr /C:"XAPI doc" release\tmp\xapi_invalid_doc_string.err.txt >nul
if errorlevel 1 (
    echo xapi_invalid_doc_string.err.txt missing strict doc diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_top_anon.xl >nul 2> release\tmp\sem_union_top_anon.err.txt
if not errorlevel 1 (
    echo sem_union_top_anon.xl should fail.
    exit /b 1
)
findstr /C:"top-level union must have a name" release\tmp\sem_union_top_anon.err.txt >nul
if errorlevel 1 (
    echo sem_union_top_anon.err.txt missing named top-level union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_empty.xl >nul 2> release\tmp\sem_union_empty.err.txt
if not errorlevel 1 (
    echo sem_union_empty.xl should fail.
    exit /b 1
)
findstr /C:"union must contain at least one field" release\tmp\sem_union_empty.err.txt >nul
if errorlevel 1 (
    echo sem_union_empty.err.txt missing empty union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_default.xl >nul 2> release\tmp\sem_union_default.err.txt
if not errorlevel 1 (
    echo sem_union_default.xl should fail.
    exit /b 1
)
findstr /C:"union fields do not support default values" release\tmp\sem_union_default.err.txt >nul
if errorlevel 1 (
    echo sem_union_default.err.txt missing union default diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_duplicate_field.xl >nul 2> release\tmp\sem_union_duplicate_field.err.txt
if not errorlevel 1 (
    echo sem_union_duplicate_field.xl should fail.
    exit /b 1
)
findstr /C:"duplicate union field name" release\tmp\sem_union_duplicate_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_duplicate_field.err.txt missing duplicate union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_anonymous_conflict.xl >nul 2> release\tmp\sem_union_anonymous_conflict.err.txt
if not errorlevel 1 (
    echo sem_union_anonymous_conflict.xl should fail.
    exit /b 1
)
findstr /C:"class member layout name conflicts with an existing field or union" release\tmp\sem_union_anonymous_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_union_anonymous_conflict.err.txt missing inline union conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_named_inline.xl >nul 2> release\tmp\sem_union_named_inline.err.txt
if not errorlevel 1 (
    echo sem_union_named_inline.xl should fail.
    exit /b 1
)
findstr /C:"class inline union must be anonymous in this phase" release\tmp\sem_union_named_inline.err.txt >nul
if errorlevel 1 (
    echo sem_union_named_inline.err.txt missing named inline union diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_unknown_field.xl >nul 2> release\tmp\sem_class_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"unknown class/union field" release\tmp\sem_class_unknown_field.err.txt >nul
if errorlevel 1 (
    echo sem_class_unknown_field.err.txt missing unknown field diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" release\tmp\sem_class_unknown_field.err.txt >nul
if not errorlevel 1 (
    echo sem_class_unknown_field.err.txt should not report assignment target cascade.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_unknown_field.err.txt >nul
if not errorlevel 1 (
    echo sem_class_unknown_field.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_unknown_field.xl >nul 2> release\tmp\sem_union_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_union_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"unknown class/union field" release\tmp\sem_union_unknown_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_unknown_field.err.txt missing unknown union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_function_field.xl >nul 2> release\tmp\sem_union_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_function_field.err.txt missing union function field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_destroy_class_field.xl >nul 2> release\tmp\sem_union_destroy_class_field.err.txt
if not errorlevel 1 (
    echo sem_union_destroy_class_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_destroy_class_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_destroy_class_field.err.txt missing union non-plain class field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_generic_inline_function_field.xl >nul 2> release\tmp\sem_union_generic_inline_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_generic_inline_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_generic_inline_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_generic_inline_function_field.err.txt missing generic inline union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_assign_type_mismatch.xl >nul 2> release\tmp\sem_assign_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_assign_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_assign_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_assign_type_mismatch.xl missing variable assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_narrowing_int.xl >nul 2> release\tmp\sem_numeric_narrowing_int.err.txt
if not errorlevel 1 (
    echo sem_numeric_narrowing_int.xl should fail.
    exit /b 1
)
findstr /C:"target int8, value int64" release\tmp\sem_numeric_narrowing_int.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_narrowing_int.xl missing numeric narrowing type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_signed_to_unsigned.xl >nul 2> release\tmp\sem_numeric_signed_to_unsigned.err.txt
if not errorlevel 1 (
    echo sem_numeric_signed_to_unsigned.xl should fail.
    exit /b 1
)
findstr /C:"target uint16, value int8" release\tmp\sem_numeric_signed_to_unsigned.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_signed_to_unsigned.xl missing signed to unsigned type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_int_to_float_implicit.xl >nul 2> release\tmp\sem_numeric_int_to_float_implicit.err.txt
if not errorlevel 1 (
    echo sem_numeric_int_to_float_implicit.xl should fail.
    exit /b 1
)
findstr /C:"target float, value int" release\tmp\sem_numeric_int_to_float_implicit.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_int_to_float_implicit.xl missing int to float type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_float_narrowing.xl >nul 2> release\tmp\sem_numeric_float_narrowing.err.txt
if not errorlevel 1 (
    echo sem_numeric_float_narrowing.xl should fail.
    exit /b 1
)
findstr /C:"target float32, value float64" release\tmp\sem_numeric_float_narrowing.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_float_narrowing.xl missing float narrowing type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_member_assign_type.xl >nul 2> release\tmp\sem_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" release\tmp\sem_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_assign_type.xl missing member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_assign_type.xl missing member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_nested_member_assign_type.xl >nul 2> release\tmp\sem_nested_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_nested_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" release\tmp\sem_nested_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_nested_member_assign_type.xl missing nested member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_nested_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_nested_member_assign_type.xl missing nested member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_multi_member_assign_type.xl >nul 2> release\tmp\sem_multi_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_multi_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"multi-return assignment type does not match member/index target type" release\tmp\sem_multi_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_multi_member_assign_type.xl missing multi-return member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_multi_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_multi_member_assign_type.xl missing multi-return member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_array_assign_type.xl >nul 2> release\tmp\sem_typed_array_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_array_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" release\tmp\sem_typed_array_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt missing typed array assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_typed_array_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt missing typed array assignment type context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_array_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_type.xl >nul 2> release\tmp\sem_typed_dict_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" release\tmp\sem_typed_dict_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt missing typed dict assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_typed_dict_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt missing typed dict assignment type context.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_dict_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_key_type.xl >nul 2> release\tmp\sem_typed_dict_assign_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_dict_assign_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_safe_dict_key_type.xl >nul 2> release\tmp\sem_typed_safe_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_safe_dict_key_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_string_index_compound_type.xl >nul 2> release\tmp\sem_typed_string_index_compound_type.err.txt
if not errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl should fail.
    exit /b 1
)
findstr /C:"string compound assignment operands must both be string" release\tmp\sem_typed_string_index_compound_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left string, right int" release\tmp\sem_typed_string_index_compound_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl missing compound diagnostic context.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_primitive_widths.xl --emit release\tmp\cgen_primitive_widths.c
if errorlevel 1 exit /b 1
findstr /C:"signed char i8" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int8 C type.
    exit /b 1
)
findstr /C:"unsigned char u8" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint8 C type.
    exit /b 1
)
findstr /C:"short i16" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int16 C type.
    exit /b 1
)
findstr /C:"unsigned short u16" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint16 C type.
    exit /b 1
)
findstr /C:"int i32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int32 C type.
    exit /b 1
)
findstr /C:"unsigned int u32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint32 C type.
    exit /b 1
)
findstr /C:"long long i64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int64 C type.
    exit /b 1
)
findstr /C:"unsigned long long u64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint64 C type.
    exit /b 1
)
findstr /C:"float f32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float32 C type.
    exit /b 1
)
findstr /C:"double f64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float64 C type.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_primitive_widths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_widening.xl --emit release\tmp\cgen_safe_widening.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_safe_widening.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_safe_widening.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_safe_widening.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_containers.xl --emit release\tmp\cgen_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue emptyArray = xvoCreateArray();" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing array default init.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(values, __xl6_array_item_" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing statement-level array literal append.
    exit /b 1
)
findstr /C:"xl6_xrt_list_literal(2" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing list literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_literal(3" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing set literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing dict literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_literal(0" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty set lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(0" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty dict lowering.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(prefixedArray, __xl6_array_item_" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing prefixed array statement-level append.
    exit /b 1
)
findstr /C:"xvalue prefixedDict = xl6_xrt_dict_varargs(1" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing prefixed dict expression fallback.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed array lowering.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed dict iteration.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedArray)" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed container cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_static_typed_containers.xl --emit release\tmp\cgen_static_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed array create.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(values" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed array index.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xrtTypedListGet(names" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed list index.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xrtTypedDictGet(table" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed dict index.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(flags" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed array read width normalization.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xrtTypedListGet(checks" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed list read width normalization.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xrtTypedDictGet(states" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed dict read width normalization.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedSetItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed set iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict iteration.
    exit /b 1
)
findstr /C:"int flag = ((int)(*(bool*)__xl6_typed_item_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed array for-in normalization.
    exit /b 1
)
findstr /C:"int check = ((int)(*(bool*)__xl6_typed_item_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed list for-in normalization.
    exit /b 1
)
findstr /C:"int item = ((int)(*(bool*)__xl6_typed_item_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed set for-in normalization.
    exit /b 1
)
findstr /C:"int state = ((int)(*(bool*)__xl6_typed_item_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed dict for-in normalization.
    exit /b 1
)
findstr /C:"char* __xl6_typed_key_copy_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key copy buffer.
    exit /b 1
)
findstr /C:"memcpy(__xl6_typed_key_copy_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key length copy.
    exit /b 1
)
findstr /C:"xrtFree(__xl6_typed_key_copy_" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_static_typed_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_for_in_returned.xl --emit release\tmp\cgen_typed_container_for_in_returned.c
if errorlevel 1 exit /b 1
findstr /C:"xtarray makeValues(void)" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array return ABI.
    exit /b 1
)
findstr /C:"xvalue makeValues(void)" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to xvalue array return.
    exit /b 1
)
findstr /C:"static xtarray xl6_fnvalue_makeValues_invoke" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed function value invoke ABI.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing member function statement temp.
    exit /b 1
)
findstr /C:"((xtarray (*)(void*))__xl6_member_fn_stmt_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed member function invoke cast.
    exit /b 1
)
findstr /C:"xl6_function_call_var(__xl6_member_fn_stmt_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to erased member typed container call.
    exit /b 1
)
findstr /C:"(xvalue (*)(void*))__xl6_member_fn_stmt_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to xvalue member typed container invoke cast.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to GNU member function expression block.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_get_at(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed list for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedSetItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed set for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict for-in lowering.
    exit /b 1
)
findstr /C:"void* __xl6_typed_item_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed for-in item temp.
    exit /b 1
)
findstr /C:"if (__xl6_typed_item_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed for-in item guard.
    exit /b 1
)
findstr /C:"*)xl6_xrt_typed_array_get_at(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed array for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedListItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed list for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedSetItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed set for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedDictItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed dict for-in item.
    exit /b 1
)
findstr /C:"char* __xl6_typed_key_copy_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key copy buffer.
    exit /b 1
)
findstr /C:"memcpy(__xl6_typed_key_copy_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key length copy.
    exit /b 1
)
findstr /C:"xrtFree(__xl6_typed_key_copy_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtFree\(__xl6_typed_key_copy_(\d+)\);\s*__xl6_typed_key_copy_\1\s*=\s*0;\s*}\s*goto __xl6_continue_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before continue.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtFree\(__xl6_typed_key_copy_(\d+)\);\s*__xl6_typed_key_copy_\1\s*=\s*0;\s*}\s*if \(__xl6_typed_iter_\1 != 0\) \{\s*xrtTypedDictDestroy\(__xl6_typed_iter_\1\);\s*__xl6_typed_iter_\1 = 0;\s*}\s*goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtFree\(__xl6_typed_key_copy_(\d+)\);\s*__xl6_typed_key_copy_\1\s*=\s*0;\s*}\s*if \(__xl6_typed_iter_\1 != 0\) \{\s*xrtTypedDictDestroy\(__xl6_typed_iter_\1\);\s*__xl6_typed_iter_\1 = 0;\s*}\s*goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = score;\s*if \(__xl6_typed_key_copy_(\d+) != 0\) \{\s*xrtFree\(__xl6_typed_key_copy_\2\);\s*__xl6_typed_key_copy_\2\s*=\s*0;\s*}\s*if \(__xl6_typed_iter_\2 != 0\) \{\s*xrtTypedDictDestroy\(__xl6_typed_iter_\2\);\s*__xl6_typed_iter_\2 = 0;\s*}\s*return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'nestedDictTotal = \(\(nestedDictTotal \+ \(outerScore \* 10LL\)\) \+ innerScore\);[\s\S]*?if \(__xl6_typed_key_copy_(\d+) != 0\) \{[\s\S]*?xrtFree\(__xl6_typed_key_copy_\1\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = \(outerScore \+ innerScore\);[\s\S]*?xrtFree\(__xl6_typed_key_copy_(\d+)\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\2\);[\s\S]*?xrtFree\(__xl6_typed_key_copy_(\d+)\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\3\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner and outer cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"nested typed dict loop boom\"[\s\S]*?xrtFree\(__xl6_typed_key_copy_(\d+)\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\1\);[\s\S]*?xrtFree\(__xl6_typed_key_copy_(\d+)\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\2\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner and outer cleanup before throw.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'const char\* __xl6_return_(\d+) = xl6_xrt_str_dup\(name\);\s*if \(__xl6_typed_iter_(\d+) != 0\) \{\s*xrtTypedListDestroy\(__xl6_typed_iter_\2\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list string copy before return cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'listContinueBreakTotal = index;[\s\S]*?xrtTypedListDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed list loop boom\"[\s\S]*?xrtTypedListDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = id;\s*if \(__xl6_typed_iter_(\d+) != 0\) \{\s*xrtTypedSetDestroy\(__xl6_typed_iter_\2\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'setContinueBreakValue = id;[\s\S]*?xrtTypedSetDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed set loop boom\"[\s\S]*?xrtTypedSetDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = value;\s*if \(__xl6_typed_iter_(\d+) != 0\) \{\s*xrtTypedArrayDestroy\(__xl6_typed_iter_\2\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'arrayContinueBreakValue = value;[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed array loop boom\"[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before throw catch.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'memberDirectTotal = 0LL;[\s\S]*?xtarray __xl6_typed_iter_(\d+) = \(\(maker\.values\.signature == 0xDAD5AD8DCC27AA57ULL\)[\s\S]*?memberDirectTotal = \(memberDirectTotal \+ value\);[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing direct member returned typed array for-in cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_for_in_returned.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_typed_container_for_in_returned.exe
    if errorlevel 1 (
        echo cgen_typed_container_for_in_returned.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_typed_container_box_to_var.xl --emit release\tmp\cgen_typed_container_box_to_var.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed array boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed list boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed set boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed dict boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_bind_type(xl6_xrt_typed_array_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing runtime type binding for boxed typed containers.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_box_to_var.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_type_desc_dedupe.xl --emit release\tmp\cgen_type_desc_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_type_desc_dedupe.c' -Raw; if(([regex]::Matches($t, 'static const xrt_type_desc\* xl6_runtime_type_desc_[0-9A-F]+\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static const xrt_generic_type generic')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_type_desc_dedupe.c duplicated runtime type desc for same typed container.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_type_desc_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_assign.xl --emit release\tmp\cgen_typed_container_copy_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict copy helper.
    exit /b 1
)
findstr /C:"xrtTypedArrayCount(valuesCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed container length lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_to_xvalue(xrtTypeInt(), idsCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xrtTypeInt(), values)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_to_xvalue(xrtTypeString(), names)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_to_xvalue(xrtTypeInt(), scoresCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict in lowering.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_in_values_" release\tmp\cgen_typed_container_copy_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_copy_assign.c should pre-evaluate typed container in arguments.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_copy_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_objects_functions.xl --emit release\tmp\cgen_typed_container_copy_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), funcs, XRT_OBJMODE_LOCAL)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed function array copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_FnBox(), boxes, XRT_OBJMODE_LOCAL)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class array copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_copy_mode(xl6_type_desc_XL_Class_FnBox(), named, XRT_OBJMODE_LOCAL)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class dict copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_Native(), natives, XRT_OBJMODE_LOCAL)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed native-backed class array copy.
    exit /b 1
)
findstr /C:"(*(XL_Class_FnBox*)dst).op.retain" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing class copy function retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_copy_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_functions.xl --emit release\tmp\cgen_typed_container_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeFunction()" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function array create.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xrtTypeFunction()" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function set create.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing guarded callable to function bridge.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(funcs" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function array item call.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedListGet(listed" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function list item call.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedDictGet(named" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function dict item call.
    exit /b 1
)
findstr /C:"xrt_typed_array_get_at(funcs, (unsigned int)(0LL)))).invoke)" release\tmp\cgen_typed_container_functions.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_functions.c should not repeat direct typed function array item invoke.
    exit /b 1
)
findstr /C:"xl6_function_call_var(first" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing function fallback call.
    exit /b 1
)
findstr /C:"XL6_Function each = xl6_function_from_callable" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration bridge.
    exit /b 1
)
findstr /C:"each.release(each.env);" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_objects.xl --emit release\tmp\cgen_typed_container_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_type_unbox_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class hash type op.
    exit /b 1
)
findstr /C:"xl6_type_unbox_XL_Class_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Class_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Class_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class hash type op.
    exit /b 1
)
findstr /C:"xrtTypedSetAddValue(pairSet" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing typed class set fill.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_function_field_objects.xl --emit release\tmp\cgen_typed_container_function_field_objects.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_copy_XL_Class_FnBox" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class type copy helper.
    exit /b 1
)
findstr /C:"__xl6_function_field_assign_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing function field assignment lowering.
    exit /b 1
)
findstr /C:"__xl6_class_assign_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class assignment retain path.
    exit /b 1
)
findstr /C:"xl6_function_call_var(fromArray.op" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing member function call lowering.
    exit /b 1
)
findstr /C:"xl6_function_call_var(item.op" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing iteration member function call lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c should not use GNU statement expressions for member function calls.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing var-call fallback runtime.
    exit /b 1
)
findstr /C:"fromArray.op.retain(fromArray.op.env);" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed index class retain.
    exit /b 1
)
findstr /C:"item.op.retain(item.op.env);" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed iteration class retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_function_field_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign.xl --emit release\tmp\cgen_typed_container_index_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_set_at(values" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed array index assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(names" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed list index assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(scores" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed dict index assignment.
    exit /b 1
)
findstr /C:"static void xl6_runtime_fatal(int code)" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime fatal helper.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(3);" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime fatal call.
    exit /b 1
)
findstr /C:"exit(3);" release\tmp\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" release\tmp\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign_objects_functions.xl --emit release\tmp\cgen_typed_container_index_assign_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedListSetValue(pairList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(pairDict" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(pairDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(payloadArray" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(payloadList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(payloadDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(nativeArray" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(nativeList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(nativeDict" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(nativeDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(funcs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(listedFuncs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(table, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(next)" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing function boxing for typed assignment.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedListGet(listedFuncs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing guarded typed function list get lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(table" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict get lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_assign_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_multi_assign_index.xl --emit release\tmp\cgen_typed_container_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_set_at(values" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed array multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(scores" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed dict multi assignment.
    exit /b 1
)
findstr /C:"xl6_box_XL_Class_Pair(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing class multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing union multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Class_Native(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing native-backed class multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function multi assignment boxing.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(payloads" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed list multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(natives, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing native-backed class dynamic dict multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(table, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function dynamic dict multi assignment.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_typed_container_multi_assign_index.c' -Raw; $simple=[regex]::Matches($t,'xvalue (__xl6_multi_index_\d+) = (?:xvoCreateInt|xl6_box_XL_Class_Pair|xl6_box_XL_Union_Payload|xl6_function_to_xvalue)[^;]*;\s*if \(!(?:xl6_xrt_typed_array_set_at|xrtTyped(?:List|Dict)SetValue)\([^\n]*\1\)\) \{\s*xl6_runtime_fatal\(3\);\s*\}\s*xvoUnref\(\1\);',[Text.RegularExpressions.RegexOptions]::Singleline); if($simple.Count -lt 6){exit 1}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = xl6_box_XL_Class_Native\(__xl6_multi_[^;]+;[\s\S]*?xrtTypedDictSetValue\(natives,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 2}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = xl6_function_to_xvalue\(__xl6_multi_[^;]+;[\s\S]*?xrtTypedDictSetValue\(table,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 3}"
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing multi-return typed index temp cleanup ordering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_compound_assign.xl --emit release\tmp\cgen_typed_container_index_compound_assign.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_typed_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index compound temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_str_concat" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing string compound helper.
    exit /b 1
)
findstr /C:"xvoCreateInt(xl6_int_pow_i64" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index pow compound lowering.
    exit /b 1
)
findstr /C:"xvoCreateFloat(__xl6_typed_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index float compound lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c should not use xvalue current for typed index compound assignment.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_compound_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\type_values.xl --emit release\tmp\type_values.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned long long valueType = 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type value declaration.
    exit /b 1
)
findstr /C:"int intOk = (valueType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type literal comparison.
    exit /b 1
)
findstr /C:"int classOk = (boxType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing class type literal comparison.
    exit /b 1
)
findstr /C:"int arrayOk = (arrayType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing array type literal comparison.
    exit /b 1
)
findstr /C:"int dictOk = (dictType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing dict type literal comparison.
    exit /b 1
)
findstr /C:"static const xrt_generic_type generic" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing XRT generic type metadata.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\type_values.exe
if errorlevel 1 exit /b 1
release\tmp\type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_minimal_runtime.xl --emit release\tmp\cgen_minimal_runtime.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_minimal_runtime.c' -Raw; if($t -match 'typedef struct XL6_Function'){exit 1}; if($t -match '__xl_std_'){exit 2}; if($t -match 'xl6_xrt_array_varargs'){exit 3}; if($t -match 'static long long xl6_xrt_value_to_i64_release'){exit 4}; if($t -notmatch 'int xlang_main\(xvalue arg\)'){exit 5}; if($t -notmatch 'xvoUnref\(__xl6_arg\);'){exit 6}; if($t -notmatch 'int main\(int argc, char\*\* argv\)'){exit 7}"
if errorlevel 1 (
    echo cgen_minimal_runtime.c runtime needs negative snapshot failed.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_minimal_runtime.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\type_aliases.xl --emit release\tmp\type_aliases.xlir
if errorlevel 1 exit /b 1
findstr /C:"typedef MyInt = int" release\tmp\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing typedef alias.
    exit /b 1
)
findstr /C:"extern type HANDLE = point" release\tmp\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing extern type alias.
    exit /b 1
)
release\xl.exe cgen test\cases\type_aliases.xl --emit release\tmp\type_aliases.c
if errorlevel 1 exit /b 1
findstr /C:"long long value = 41LL;" release\tmp\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing int alias lowering.
    exit /b 1
)
findstr /C:"void* handle = 0;" release\tmp\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing point alias lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\type_aliases.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\type_aliases.exe
if errorlevel 1 exit /b 1
release\tmp\type_aliases.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_concat.xl --emit release\tmp\cgen_string_concat.c
if errorlevel 1 exit /b 1
findstr /C:"static const char* xl6_xrt_str_concat" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string concat helper.
    exit /b 1
)
findstr /C:"const char* joined = xl6_xrt_str_concat" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string concat declaration.
    exit /b 1
)
findstr /C:"joined = xl6_xrt_str_concat(joined, \"!\");" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string += lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_concat.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_concat.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_concat.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_compare.xl --emit release\tmp\cgen_string_compare.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(left, same, 0, false) == 0" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string equality lowering.
    exit /b 1
)
findstr /C:"xrtStrComp(left, right, 0, false) < 0" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string ordered comparison lowering.
    exit /b 1
)
findstr /C:"xrtStrApprox(left, 0, same, 0)" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_compare.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_compare.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_compare.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_literals.xl --emit release\tmp\cgen_string_literals.c
if errorlevel 1 exit /b 1
findstr /C:"const char* single =" release\tmp\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing single quote string lowering.
    exit /b 1
)
findstr /C:"const char* rawPath =" release\tmp\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing raw string lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_literals.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_fstring.xl --emit release\tmp\cgen_fstring.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_fstr_join" release\tmp\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing f-string join helper.
    exit /b 1
)
findstr /C:"xrtBufferInit(&buffer, 256u);" release\tmp\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing xbuffer initialization.
    exit /b 1
)
findstr /C:"({ xbuffer_struct __xl6_fstr_" release\tmp\cgen_fstring.c >nul
if not errorlevel 1 (
    echo cgen_fstring.c should not emit GNU statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string" release\tmp\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing int interpolation conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(dynamic)" release\tmp\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing var interpolation conversion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_fstring.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_fstring.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_fstring.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_time_literals.xl --emit release\tmp\cgen_time_literals.c
if errorlevel 1 exit /b 1
findstr /C:"long long dateOnly = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing date literal lowering.
    exit /b 1
)
findstr /C:"long long timeOnly = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time literal lowering.
    exit /b 1
)
findstr /C:"long long both = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing datetime literal lowering.
    exit /b 1
)
findstr /C:"long long inferred = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time auto inference.
    exit /b 1
)
findstr /C:"xrtTimeApprox(both, xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_time_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_time_literals.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_time_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_auto_expressions.xl --emit release\tmp\cgen_auto_expressions.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue dynSum = 0;" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto dynamic expression lowering.
    exit /b 1
)
findstr /C:"dynSum = xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto dynamic binary statement assignment.
    exit /b 1
)
findstr /C:"xvalue dynSum = ({ xvalue __xl6_bin_left_" release\tmp\cgen_auto_expressions.c >nul
if not errorlevel 1 (
    echo cgen_auto_expressions.c should not emit auto dynamic binary declaration statement expressions.
    exit /b 1
)
findstr /C:"long long sum =" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto integer expression inference.
    exit /b 1
)
findstr /C:"double floatSum =" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto float expression inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_auto_expressions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_auto_inference_matrix.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_auto_inference_matrix.xl --emit release\tmp\cgen_auto_inference_matrix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_auto_inference_matrix.c'; $checks=@('long long literalInt =','double literalFloat =','int literalBool =','const char* literalString =','xvalue dynamicArray =','xvalue dynamicList =','xvalue dynamicDict =','long long typedArrayItem = (*(long long*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedArray','const char* typedListItem = (*(const char**)xl6_xrt_typed_require_item(xrtTypedListGet(typedList','long long typedDictItem = (*(long long*)xl6_xrt_typed_require_item(xrtTypedDictGet(typedDict','xvalue missingTypedItem = xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(typedArray','xvalue dynamicArrayItem = xl6_xrt_retain(xvoArrayGetValue(dynamicArray','xvalue dynamicListItem = xl6_xrt_retain(xvoListGetValue(dynamicList','xvalue dynamicDictItem = xl6_xrt_retain(xvoTableGetValue(dynamicDict','XL_Class_MatrixBox box = {0};','box = makeBox(7LL,','long long fieldValue = box.value;','const char* fieldText = box.label;','long long methodValue = xl_class_MatrixBox__getValue(&box);','XL6_Function fn = makeAdder(10LL);','long long fnResult =','long long callResult = add(fieldValue, methodValue);','xfuture* futureValue = doubleAsync(6LL);','long long awaitedValue =','xchannel autoCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','int sent = xl6_channel_send_value(autoCh','int received = xl6_channel_recv_into(autoCh'); foreach($p in $checks){ if($t.IndexOf($p) -lt 0){ Write-Host ('cgen_auto_inference_matrix.c missing '+$p); exit 1 } }"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_auto_inference_matrix.xl test\cases\cgen_class_host.c -o release\tmp\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_call_var_arg_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_var_arg_exprs.xl --emit release\tmp\cgen_call_var_arg_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_call_arg_" release\tmp\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var call argument temporary.
    exit /b 1
)
findstr /C:"record_xvalue(__xl6_call_arg_" release\tmp\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing call argument replacement.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" release\tmp\cgen_call_var_arg_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument binary lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_nc_" release\tmp\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument null-coalesce temporary.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" release\tmp\cgen_call_var_arg_exprs.c | findstr /C:"makeValue(7LL);" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument conditional branch.
    exit /b 1
)
findstr /C:"xl6_var_call(dynamicSum, 2, __xl6_call_arg_" release\tmp\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var callable positional argument lowering.
    exit /b 1
)
findstr /C:"xl6_var_call_kwargs(dynamicSum, xl6_xrt_dict_varargs(2," release\tmp\cgen_call_var_arg_exprs.c | findstr /C:"__xl6_call_arg_" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var callable named argument lowering.
    exit /b 1
)
findstr /C:"({" release\tmp\cgen_call_var_arg_exprs.c >nul
if not errorlevel 1 (
    echo cgen_call_var_arg_exprs.c should not emit statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_call_var_arg_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_call_var_arg_exprs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_call_var_arg_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_call_scalar_arg_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_scalar_arg_exprs.xl --emit release\tmp\cgen_call_scalar_arg_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"long long __xl6_call_arg_" release\tmp\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing scalar call argument temporary.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(__xl6_call_value_" release\tmp\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing dynamic scalar argument conversion.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" release\tmp\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing call argument replacement.
    exit /b 1
)
findstr /C:"({" release\tmp\cgen_call_scalar_arg_exprs.c >nul
if not errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c should not emit statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_call_scalar_arg_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_call_scalar_arg_exprs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_call_scalar_arg_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_var_return_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_return_exprs.xl --emit release\tmp\cgen_var_return_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_return_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing xvalue return temporary.
    exit /b 1
)
findstr /C:"int __xl6_return_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing scalar return temporary.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return binary lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_nested_value_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing nested xvalue statement temporary.
    exit /b 1
)
findstr /C:"xvalue __xl6_return_value_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return xvalue field temporary.
    exit /b 1
)
findstr /C:"value0 = __xl6_return_value_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return first field handoff.
    exit /b 1
)
findstr /C:"value1 = __xl6_return_value_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return second field handoff.
    exit /b 1
)
findstr /C:"__xl6_return_value_" release\tmp\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return dynamic binary lowering.
    exit /b 1
)
findstr /C:"__xl6_return_value_" release\tmp\cgen_var_return_exprs.c | findstr /C:"makeValue(14LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return conditional branch lowering.
    exit /b 1
)
findstr /C:"__xl6_return_value_" release\tmp\cgen_var_return_exprs.c | findstr /C:"makeValue(15LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return null-coalesce fallback lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar field temporary.
    exit /b 1
)
findstr /C:"value0 = __xl6_return_field_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return first scalar field handoff.
    exit /b 1
)
findstr /C:"value1 = __xl6_return_field_" release\tmp\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return second scalar field handoff.
    exit /b 1
)
findstr /C:"__xl6_return_field_" release\tmp\cgen_var_return_exprs.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar compare lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" release\tmp\cgen_var_return_exprs.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar approximate lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" release\tmp\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar logical lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" release\tmp\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_to_i64_release(__xl6_call_value_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar conversion lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return compare lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return approximate lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return logical lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"makeValue(40LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return conditional branch.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_var_return_exprs.c | findstr /C:"({" >nul
if not errorlevel 1 (
    echo cgen_var_return_exprs.c should not emit return statement expressions.
    exit /b 1
)
findstr /C:"({" release\tmp\cgen_var_return_exprs.c >nul
if not errorlevel 1 (
    echo cgen_var_return_exprs.c should not emit any statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_var_return_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_var_return_exprs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_var_return_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_expr_statement_discard.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_expr_statement_discard.xl --emit release\tmp\cgen_expr_statement_discard.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing xvalue expression statement temporary.
    exit /b 1
)
findstr /C:"int __xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing scalar expression statement temporary.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing discarded xvalue expression cleanup.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level binary discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level compare discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level approximate discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level logical discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_array_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level membership discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" release\tmp\cgen_expr_statement_discard.c | findstr /C:"({" >nul
if not errorlevel 1 (
    echo cgen_expr_statement_discard.c should not emit expression statement statement-expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_expr_statement_discard.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_expr_statement_discard.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_expr_statement_discard.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_array_literal_expr_elements.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_array_literal_expr_elements.xl --emit release\tmp\cgen_array_literal_expr_elements.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_return_" release\tmp\cgen_array_literal_expr_elements.c | findstr /C:"xvoCreateArray()" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal return creation.
    exit /b 1
)
findstr /C:"xvalue declared = xvoCreateArray();" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal declaration creation.
    exit /b 1
)
findstr /C:"xvalue __xl6_expr_stmt_" release\tmp\cgen_array_literal_expr_elements.c | findstr /C:"xvoCreateArray()" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal expression statement creation.
    exit /b 1
)
findstr /C:"xvalue __xl6_array_item_" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal item temporaries.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_return_" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal return append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(declared" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal declaration append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(prefixed" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing prefixed array declaration append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_expr_stmt_" release\tmp\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal expression statement append.
    exit /b 1
)
findstr /C:"__xl6_array_item_" release\tmp\cgen_array_literal_expr_elements.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal item binary lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3, ({" release\tmp\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not emit array varargs with statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(2, ({" release\tmp\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not emit prefixed array varargs with statement expressions.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_bin_left_" release\tmp\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should pre-evaluate array literal items.
    exit /b 1
)
findstr /C:"runtime_type_desc_" release\tmp\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not bind dynamic array<var> runtime descriptors.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_array_literal_expr_elements.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_array_literal_expr_elements.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_array_literal_expr_elements.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_var_values.xl --emit release\tmp\cgen_var_values.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue empty = xvoCreateNull();" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var null initialization.
    exit /b 1
)
findstr /C:"xvalue number = xvoCreateInt((long long)(123LL));" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing int var boxing.
    exit /b 1
)
findstr /C:"xvalue moment = xvoCreateTime((long long)(xrtStrToTime(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing time var boxing.
    exit /b 1
)
findstr /C:"xvalue __xl6_assign_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var assignment ownership handoff.
    exit /b 1
)
findstr /C:"xvoCreateTime((long long)(xrtStrToTime(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing datetime vararg boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic var type helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_equal(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value equality helper.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_order(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value ordering helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic helper.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic cleanup.
    exit /b 1
)
findstr /C:"xvalue sumNumber = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level sumNumber dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"sumNumber = xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level sumNumber dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue sumNumber = ({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit sumNumber dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue joinedText = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level joinedText dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"joinedText = xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level joinedText dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue joinedText = ({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit joinedText dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue mixedPlus = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedPlus dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"mixedPlus = xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedPlus dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue mixedPlus = ({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedPlus dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue conditionalPick = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalPick declaration.
    exit /b 1
)
findstr /C:"conditionalPick = xl6_xrt_retain(lowNumber);" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalPick assignment.
    exit /b 1
)
findstr /C:"xvalue conditionalPick = ({ xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit conditionalPick declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue conditionalTemp = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalTemp declaration.
    exit /b 1
)
findstr /C:"conditionalTemp = makeValue(40LL);" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalTemp assignment.
    exit /b 1
)
findstr /C:"xvalue conditionalTemp = ({ xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit conditionalTemp declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue mixedAuto = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedAuto declaration.
    exit /b 1
)
findstr /C:"mixedAuto = xvoCreateInt((long long)(12LL));" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedAuto assignment.
    exit /b 1
)
findstr /C:"xvalue mixedAuto = ({ xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedAuto declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue mixedConditional = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedConditional declaration.
    exit /b 1
)
findstr /C:"mixedConditional = xvoCreateText((void*)(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedConditional assignment.
    exit /b 1
)
findstr /C:"xvalue mixedConditional = ({ xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedConditional declaration statement expressions.
    exit /b 1
)
findstr /C:"int compareDecl = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level compareDecl declaration.
    exit /b 1
)
findstr /C:"compareDecl = (int)(__xl6_cmp_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level compareDecl assignment.
    exit /b 1
)
findstr /C:"int compareDecl = ({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit compareDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"long long orderDecl = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level orderDecl declaration.
    exit /b 1
)
findstr /C:"orderDecl = (long long)(__xl6_cmp_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level orderDecl assignment.
    exit /b 1
)
findstr /C:"long long orderDecl = ({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit orderDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int autoCompareDecl = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoCompareDecl declaration.
    exit /b 1
)
findstr /C:"autoCompareDecl = (int)(__xl6_cmp_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoCompareDecl assignment.
    exit /b 1
)
findstr /C:"int autoCompareDecl = ({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit autoCompareDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int approxDecl = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level approxDecl declaration.
    exit /b 1
)
findstr /C:"approxDecl = (int)(__xl6_approx_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level approxDecl assignment.
    exit /b 1
)
findstr /C:"int approxDecl = ({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit approxDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int autoApproxDecl = 0;" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoApproxDecl declaration.
    exit /b 1
)
findstr /C:"autoApproxDecl = (int)(__xl6_approx_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoApproxDecl assignment.
    exit /b 1
)
findstr /C:"int autoApproxDecl = ({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit autoApproxDecl declaration statement expressions.
    exit /b 1
)
findstr /R /C:"__xl6_assign_[0-9][0-9]* = xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignBinary dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"assignBinary = __xl6_assign_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing assignBinary target handoff.
    exit /b 1
)
findstr /R /C:"xvalue __xl6_assign_[0-9][0-9]* = ({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignBinary statement expression initializer.
    exit /b 1
)
findstr /C:"__xl6_assign_" release\tmp\cgen_var_values.c | findstr /C:"makeValue(60LL);" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignConditional false branch.
    exit /b 1
)
findstr /C:"assignConditional = __xl6_assign_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing assignConditional target handoff.
    exit /b 1
)
findstr /R /C:"xvalue __xl6_assign_[0-9][0-9]* = ({ xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignConditional statement expression initializer.
    exit /b 1
)
findstr /C:"assignCompare = (int)(__xl6_cmp_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignCompare assignment.
    exit /b 1
)
findstr /C:"assignCompare = ({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignCompare statement expressions.
    exit /b 1
)
findstr /C:"assignOrder = (long long)(__xl6_cmp_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignOrder assignment.
    exit /b 1
)
findstr /C:"assignOrder = ({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignOrder statement expressions.
    exit /b 1
)
findstr /C:"assignApprox = (int)(__xl6_approx_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignApprox assignment.
    exit /b 1
)
findstr /C:"assignApprox = ({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignApprox statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_unary_release(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic unary release helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_truthy_release(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic truthy release helper.
    exit /b 1
)
findstr /C:"__xl6_unary_value_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic unary statement temporaries.
    exit /b 1
)
findstr /C:"__xl6_not_value_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic not statement temporaries.
    exit /b 1
)
findstr /C:"__xl6_truth_value_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic condition truthy statement temporaries.
    exit /b 1
)
findstr /C:"xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical temporaries.
    exit /b 1
)
findstr /C:"__xl6_logic_right_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical right operand path.
    exit /b 1
)
findstr /C:"xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_approx(__xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate helper.
    exit /b 1
)
findstr /C:"int __xl6_if_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic if condition temporaries.
    exit /b 1
)
findstr /C:"if (__xl6_if_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if condition handoff.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" release\tmp\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if approximate condition lowering.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in if conditions.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in if conditions.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in if conditions.
    exit /b 1
)
findstr /C:"int __xl6_elseif_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic elseif condition temporaries.
    exit /b 1
)
findstr /C:"if (__xl6_elseif_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif condition handoff.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" release\tmp\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif approximate condition lowering.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"int __xl6_while_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic while condition temporaries.
    exit /b 1
)
findstr /C:"if (!__xl6_while_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while condition break guard.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" release\tmp\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while approximate condition lowering.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in while conditions.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in while conditions.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in while conditions.
    exit /b 1
)
findstr /C:"int __xl6_do_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic do while condition temporaries.
    exit /b 1
)
findstr /C:"if (!__xl6_do_cond_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while condition break guard.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" release\tmp\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" release\tmp\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while approximate condition lowering.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"long long __xl6_call_arg_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level scalar call argument temporaries.
    exit /b 1
)
findstr /C:"__xl6_call_value_" release\tmp\cgen_var_values.c | findstr /C:"makeValue(40LL);" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic conditional call argument path.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing scalar call argument handoff.
    exit /b 1
)
findstr /C:"const char* __xl6_call_str_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level string call argument temporaries.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions as direct call arguments.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions as direct call arguments.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic arithmetic statement expressions as direct scalar call arguments.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(({ xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic arithmetic statement expressions as direct string call arguments.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(number);" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_var_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_var_values.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_var_values.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_var_runtime_objects.xl --emit release\tmp\cgen_var_runtime_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xvoCreateCallable(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable xvalue boxing.
    exit /b 1
)
findstr /C:"xrtCallableCreate(fn.sig" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable signature binding.
    exit /b 1
)
findstr /C:"box->fn.invoke_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing function xcall dispatch.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 1u" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing multi-return xcall result.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 4u" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing overflow multi-return xcall result.
    exit /b 1
)
findstr /C:"xl6_fnvalue_pairDefault_arity1_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional multi-return adapter xcall.
    exit /b 1
)
findstr /C:"xl6_fnvalue_totalDefault_arity0_vararg_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional vararg adapter xcall.
    exit /b 1
)
findstr /C:"xrtFuncSigTypeId(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing precise function type query.
    exit /b 1
)
findstr /C:"xvoCreateRecord(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record boxing.
    exit /b 1
)
findstr /C:"xvoCreateHandle(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing native-backed class handle boxing.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Class_Pair" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record type ops.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Class_Native" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing native-backed class handle type ops.
    exit /b 1
)
findstr /C:"xl6_var_call(boxedFn" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var function call.
    exit /b 1
)
findstr /C:"xl6_var_call_kwargs(boxedOptions" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var kwargs call.
    exit /b 1
)
findstr /C:"frame.Kwargs = kwargs" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing kwargs call frame binding.
    exit /b 1
)
findstr /C:"xvoTableRemove(__xl6_vararg, \"path\", 0)" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing fixed named parameter filtering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_var_runtime_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_type_baseline.xl --emit release\tmp\cgen_runtime_type_baseline.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_type_value_id(boxedChild" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing boxed class runtime type query.
    exit /b 1
)
findstr /C:"xvoCreateRecord(xl6_type_desc_XL_Class_Actor()" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing object-derived class record boxing.
    exit /b 1
)
findstr /C:"xvoCreateHandle(xl6_type_desc_XL_Class_Native()" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing native-backed class handle runtime type boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(payload)" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing union runtime type boxing.
    exit /b 1
)
findstr /C:"xvoCreateCallable(" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing callable runtime type boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xl6_type_desc_XL_Class_Child()" release\tmp\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing typed class array runtime descriptor binding.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_runtime_type_baseline.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_runtime_type_baseline.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_runtime_type_baseline.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_is_as.xl --emit release\tmp\cgen_runtime_is_as.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_runtime_value_is(" release\tmp\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime is helper.
    exit /b 1
)
findstr /C:"xl6_runtime_value_as_record(" release\tmp\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime record downcast helper.
    exit /b 1
)
findstr /C:"xl6_runtime_type_is(actual" release\tmp\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime class relation helper.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_runtime_is_as.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_runtime_is_as.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_runtime_is_as.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_as_fail.xl --emit release\tmp\cgen_runtime_as_fail.c
if errorlevel 1 (
    echo cgen_runtime_as_fail.xl should generate runtime failure path.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(4);" release\tmp\cgen_runtime_as_fail.c >nul
if errorlevel 1 (
    echo cgen_runtime_as_fail.c missing runtime as failure branch.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_as_container_target.xl >nul 2> release\tmp\sem_runtime_as_container_target.err.txt
if not errorlevel 1 (
    echo sem_runtime_as_container_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem] as" release\tmp\sem_runtime_as_container_target.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_as_container_target.err.txt missing as target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_as_static_mismatch.xl >nul 2> release\tmp\sem_runtime_as_static_mismatch.err.txt
if not errorlevel 1 (
    echo sem_runtime_as_static_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem] as" release\tmp\sem_runtime_as_static_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_as_static_mismatch.err.txt missing as source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_is_var_target.xl >nul 2> release\tmp\sem_runtime_is_var_target.err.txt
if not errorlevel 1 (
    echo sem_runtime_is_var_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem] is" release\tmp\sem_runtime_is_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_is_var_target.err.txt missing is target diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_type_conversions.xl --emit release\tmp\cgen_type_conversions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrToI64(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to int conversion.
    exit /b 1
)
findstr /C:"xrtStrToNum(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to float conversion.
    exit /b 1
)
findstr /C:"xrtStrToTime(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to time conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing int to string helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(boxedInt)" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing var to string conversion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_type_conversions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_type_conversions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_type_conversions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_implicit_methods.xl --emit release\tmp\cgen_implicit_methods.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_value_to_string_release" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing implicit toString lowering.
    exit /b 1
)
findstr /C:"xvoToString(value, 0)" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing XRT xvoToString bridge.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(123LL))" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing literal toString boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_implicit_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure.xl --emit release\tmp\cgen_closure.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function runtime.
    exit /b 1
)
findstr /C:"typedef struct XL6_Closure_" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure environment.
    exit /b 1
)
findstr /C:"void (*retain)(void* env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function retain field.
    exit /b 1
)
findstr /C:"__env->ref_count = 1;" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure ref count init.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_closure.c >nul
if not errorlevel 1 (
    echo cgen_closure.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'release\tmp\cgen_closure.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_closure.c still contains CRT free.
    exit /b 1
)
findstr /C:"copy.retain(copy.env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function copy retain.
    exit /b 1
)
findstr /C:"long long value = ((add10.signature == " release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function signature guard.
    exit /b 1
)
findstr /C:"((long long (*)(void*, long long))add10.invoke)(add10.env, 5LL)" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function value invoke.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_closure.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_closure.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_closure.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure_resource_capture.xl --emit release\tmp\cgen_closure_resource_capture.c
if errorlevel 1 exit /b 1
findstr /C:"__env->values = xl6_xrt_retain(values);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture retain.
    exit /b 1
)
findstr /C:"xvoUnref(__env->values);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture release.
    exit /b 1
)
findstr /C:"__env->inner.retain(__env->inner.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture retain.
    exit /b 1
)
findstr /C:"__env->inner.release(__env->inner.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture release.
    exit /b 1
)
findstr /C:"current.release(current.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function assignment release.
    exit /b 1
)
findstr /C:".retain(__xl6_function_assign_" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing borrowed function assignment retain.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_closure_resource_capture.c >nul
if not errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'release\tmp\cgen_closure_resource_capture.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT free.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_closure_resource_capture.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_bare_call.xl --emit release\tmp\cgen_function_bare_call.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue (*invoke_var)(void* env, int argc, xvalue* argv);" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function invoke slot.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var(XL6_Function fn, int argc, ...)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function call helper.
    exit /b 1
)
findstr /C:"xvoType(__xl6_argv[0]) != XL6_XVO_DT_INT" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function argument type check.
    exit /b 1
)
findstr /C:"xvalue value = xl6_function_call_var(add, 2" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing bare function call lowering.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg arity check.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg packing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_bare_call.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_named_value.xl --emit release\tmp\cgen_function_named_value.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_fnvalue_add_invoke(void* __xl6_env, long long left, long long right)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function typed wrapper.
    exit /b 1
)
findstr /C:"static xvalue xl6_fnvalue_add_invoke_var(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function erased wrapper.
    exit /b 1
)
findstr /C:"XL6_Function typed = xl6_fnvalue_add_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function value init.
    exit /b 1
)
findstr /C:"XL6_Function erased = xl6_fnvalue_add_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing bare function value init.
    exit /b 1
)
findstr /C:"XL6_Function oneDefault = xl6_fnvalue_addDefault_arity1_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional parameter function adapter.
    exit /b 1
)
findstr /C:"XL6_Function variadicDefault = xl6_fnvalue_totalDefault_arity0_vararg_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional vararg function adapter.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_function_named_value.c',[System.Text.Encoding]::UTF8); if($text.IndexOf('XL6_Function mixedPicker = (chooseMixedPicker ? xl6_function_copy_value(borrowedMixedPicker) : makePicker(200LL));') -lt 0 -or $text.IndexOf('XL6_Function __xl6_function_assign_') -lt 0 -or $text.IndexOf('= (chooseMixedPicker ? xl6_function_copy_value(borrowedMixedPicker) : makePicker(300LL));') -lt 0){ exit 1 }"
if errorlevel 1 (
    echo cgen_function_named_value.c missing mixed conditional function value branch copy.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1 || __xl6_argc > 2)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing erased optional arity check.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function array vararg lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function dict vararg lowering.
    exit /b 1
)
findstr /C:"static xrt_func_param __xl6_sig_params[2u];" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature parameter metadata array.
    exit /b 1
)
findstr /C:"static const struct xrt_type_desc* __xl6_sig_returns[1u];" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature return type metadata array.
    exit /b 1
)
findstr /C:"__xl6_sig.Params = __xl6_sig_params;" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature Params binding.
    exit /b 1
)
findstr /C:"__xl6_sig.ReturnTypes = __xl6_sig_returns;" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature ReturnTypes binding.
    exit /b 1
)
findstr /C:"__xl6_sig_params[1].Flags = 0x00000001U;" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional parameter metadata flag.
    exit /b 1
)
findstr /C:"__xl6_sig_params[0].Type = xrtTypeArray();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing array vararg metadata type.
    exit /b 1
)
findstr /C:"__xl6_sig_params[1].Type = xrtTypeDict();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing dict vararg metadata type.
    exit /b 1
)
findstr /C:"__xl6_sig.Flags = 0x00000001U;" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing array vararg signature flag.
    exit /b 1
)
findstr /C:"__xl6_sig.Flags = 0x00000002U;" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing dict vararg signature flag.
    exit /b 1
)
findstr /C:"copy = (char*)xrtMalloc(len + 1u);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for function fallback string.
    exit /b 1
)
findstr /C:"copy = (char*)malloc(len + 1u);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still uses CRT malloc for function fallback string.
    exit /b 1
)
findstr /C:"box = (XL6_Function_Callable_Env*)xrtMalloc(sizeof(XL6_Function_Callable_Env));" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for callable env.
    exit /b 1
)
findstr /C:"xrtFree(box);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for callable env.
    exit /b 1
)
findstr /C:"argv = (xvalue*)xrtMalloc(sizeof(xvalue) * (size_t)argc);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for erased call argv.
    exit /b 1
)
findstr /C:"xrtFree(argv);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for erased call argv.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'release\tmp\cgen_function_named_value.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT free.
    exit /b 1
)
findstr /C:"static void xl6_function_fatal(int code)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal helper.
    exit /b 1
)
findstr /C:"static void xl6_runtime_fatal(int code)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized runtime fatal helper.
    exit /b 1
)
findstr /C:"xl6_function_fatal(3);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal call.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(code);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function fatal delegation.
    exit /b 1
)
findstr /C:"exit(3);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_named_value.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_named_value.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_named_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_function_value.xl --emit release\tmp\cgen_extern_function_value.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_fnvalue_cadd_invoke(void* __xl6_env, long long left, long long right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function typed wrapper.
    exit /b 1
)
findstr /C:"native_add_i64(left, right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing native extern target call.
    exit /b 1
)
findstr /C:"xl6_fnvalue_cadd_xcall" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function xcall adapter.
    exit /b 1
)
findstr /C:"xl6_export_exportedAdd(left, right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing export function internal target call.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_extern_function_value.c test\cases\cgen_extern_function_value_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_multi_return.xl --emit release\tmp\cgen_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function_Return_" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing function multi-return struct.
    exit /b 1
)
findstr /C:"XL6_Function pairFn = xl6_fnvalue_pair_make();" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return named function value.
    exit /b 1
)
findstr /C:"XL6_Function defaultFn = xl6_fnvalue_pairDefault_arity1_make();" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return optional adapter.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_member_function_multi_return.xl --emit release\tmp\cgen_member_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"XL6_Function __xl6_member_fn_multi_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return function lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_multi_" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not use GNU statement expressions for member multi-return calls.
    exit /b 1
)
findstr /C:"XL6_Function_Return_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return struct.
    exit /b 1
)
findstr /C:".op.retain != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing class return function-field retain.
    exit /b 1
)
findstr /C:"__xl6_class_assign_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing borrowed class assignment temporary.
    exit /b 1
)
findstr /C:"returned.op.retain" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for class rvalue initialization.
    exit /b 1
)
findstr /C:"assignedBox.op.retain" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for multi-return rvalue assignment.
    exit /b 1
)
findstr /C:"assignedBox.op.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing class multi-assignment release.
    exit /b 1
)
findstr /C:"assignedOp.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing function multi-assignment release.
    exit /b 1
)
findstr /C:".value0.op.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded class multi-return release.
    exit /b 1
)
findstr /C:".value1.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded function multi-return release.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return function statement lowering.
    exit /b 1
)
findstr /C:"__xl6_member_result_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return result temporary.
    exit /b 1
)
findstr /C:"xl_class_PairBox__destroy(&__xl6_member_result_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return class cleanup.
    exit /b 1
)
findstr /C:"__xl6_member_result_" release\tmp\cgen_member_function_multi_return.c | findstr /C:".value1.release != 0" >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return function cleanup.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_stmt_" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not use GNU statement expressions for ignored member multi-return calls.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_member_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_member_function_class_return.xl --emit release\tmp\cgen_member_function_class_return.c --emit-header release\tmp\cgen_member_function_class_return.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" release\tmp\cgen_member_function_class_return.h >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.h missing public function value type.
    exit /b 1
)
findstr /C:"XL6_Function op;" release\tmp\cgen_member_function_class_return.h >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.h missing class function field type.
    exit /b 1
)
findstr /C:"void op;" release\tmp\cgen_member_function_class_return.h >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.h should not emit void function fields.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level member function temp.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" release\tmp\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not use GNU statement expressions for class-return member function calls.
    exit /b 1
)
findstr /C:"madeText = xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing string-return member function declaration conversion.
    exit /b 1
)
findstr /C:"textArgValue = textScore(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing string-return member function call argument conversion.
    exit /b 1
)
findstr /C:"textNestedValue = (textScore(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested string-return member function argument conversion.
    exit /b 1
)
findstr /C:"textTwoArgValue = joinTextScore(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing two string-return member function argument conversions.
    exit /b 1
)
findstr /C:"namedTextArgValue = textScore(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named string-return member function argument conversion.
    exit /b 1
)
findstr /C:"namedTextTwoArgValue = joinTextScore(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing reordered named string-return member function argument conversions.
    exit /b 1
)
findstr /C:"expect_i64(namedTextTwoArgValue, 22LL)" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named string-return member function runtime assertion.
    exit /b 1
)
findstr /C:"xl6_xrt_str_dup(xl6_xvalue_to_string_release(xl6_function_call_var(textMaker.make" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing discarded string-return member function conversion.
    exit /b 1
)
findstr /C:"__xl6_class_assign_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class assignment temporary.
    exit /b 1
)
findstr /C:"__xl6_class_field_recv_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class field receiver temporary.
    exit /b 1
)
findstr /C:"xl_class_Payload__destroy(&__xl6_class_field_recv_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class field receiver cleanup.
    exit /b 1
)
findstr /C:"__xl6_member_fn_call_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing temporary receiver member function call value.
    exit /b 1
)
findstr /C:"xl_class_Payload__destroy(&__xl6_member_recv_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing temporary receiver member function cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'tempFunctionFieldValue\s*=\s*0;\s*\{[\s\S]*?XL_Class_Payload\s+(__xl6_member_recv_\d+)\s*=\s*\{0\};[\s\S]*?XL6_Function\s+__xl6_member_fn_call_\d+\s*=\s*\1\.op;[\s\S]*?tempFunctionFieldValue\s*=[\s\S]*?xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level cleanup for temporary receiver function-field call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -match 'tempFunctionFieldValue\s*=\s*\(\([^\r\n]*maker\.make\.invoke'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c should not inline temporary receiver function-field calls.
    exit /b 1
)
findstr /C:"__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class argument temporary.
    exit /b 1
)
findstr /C:"storePayload(__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing expression-statement class argument call.
    exit /b 1
)
findstr /C:"nestedArgumentValue = (readPayload(__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested expression class argument temporary.
    exit /b 1
)
findstr /C:"expect_i64(nestedArgumentValue, 91LL)" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested expression class argument expectation.
    exit /b 1
)
findstr /C:"expect_i64((readPayload(__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing extern nested expression class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'twoArgumentValue\s*=\s*sumPayload\((__xl6_class_arg_\d+),\s*(__xl6_class_arg_\d+)\);\s*xl_class_Payload__destroy\(&\2\);\s*xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing two temporary class arguments with reverse cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'expect_i64\(sumPayload\((__xl6_class_arg_\d+),\s*(__xl6_class_arg_\d+)\),\s*270LL\);\s*xl_class_Payload__destroy\(&\2\);\s*xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing extern two temporary class arguments with reverse cleanup.
    exit /b 1
)
findstr /C:"namedArgumentValue = readPayload(__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'storePayload\((__xl6_class_arg_\d+),\s*160LL\);\s*xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named expression-statement class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedTwoArgumentValue\s*=\s*sumPayload\((__xl6_class_arg_\d+),\s*(__xl6_class_arg_\d+)\);\s*xl_class_Payload__destroy\(&\2\);\s*xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named two temporary class arguments with reverse cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'expect_i64\(sumPayload\((__xl6_class_arg_\d+),\s*(__xl6_class_arg_\d+)\),\s*390LL\);\s*xl_class_Payload__destroy\(&\2\);\s*xl_class_Payload__destroy\(&\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing reordered named two temporary class arguments with reverse cleanup.
    exit /b 1
)
findstr /C:"expect_i64((readPayload(((maker.make.signature" release\tmp\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not inline class-return member function values in extern nested calls.
    exit /b 1
)
findstr /C:"readPayload(({ XL6_Function __xl6_member_fn_" release\tmp\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not use GNU statement expressions in nested class argument calls.
    exit /b 1
)
findstr /C:"void*, XL_Class_Payload, long long))__xl6_member_fn_stmt_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level member function invoke with class argument.
    exit /b 1
)
findstr /C:"__xl6_member_result_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing ignored member function return temporary.
    exit /b 1
)
findstr /C:"xl_class_Payload__destroy(&__xl6_member_result_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing ignored member function return cleanup.
    exit /b 1
)
findstr /C:"xl_class_Payload__destroy(&__xl6_class_arg_" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class argument cleanup.
    exit /b 1
)
findstr /C:"(XL_Class_Payload){0}" release\tmp\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class return fallback default.
    exit /b 1
)
findstr /C:"(XL_Class_Payload)0" release\tmp\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not cast zero to class return value.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_member_function_class_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_member_function_class_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_member_function_class_return.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_member_function_class_return.exe
    if errorlevel 1 (
        echo cgen_member_function_class_return.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_function_default.xl --emit release\tmp\cgen_function_default.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" release\tmp\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function runtime.
    exit /b 1
)
findstr /C:"XL6_Function fn = {0};" release\tmp\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_default.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_default.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_default.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_adapter_dedupe.xl --emit release\tmp\cgen_function_adapter_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_function_adapter_dedupe.c' -Raw; if(([regex]::Matches($t, 'static XL6_Function xl6_fnvalue_addDefault_arity1_make\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static long long xl6_fnvalue_addDefault_arity1_invoke\(')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_function_adapter_dedupe.c duplicated default-arg adapter.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_adapter_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\call_defaults.xl --emit release\tmp\call_defaults.c
if errorlevel 1 exit /b 1
findstr /C:"long long first = add(1LL, 1LL, 2LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing default arguments.
    exit /b 1
)
findstr /C:"long long second = add(1LL, 1LL, 4LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing named argument reorder.
    exit /b 1
)
findstr /C:"long long third = add(1LL, 2LL, 3LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing all named arguments.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\call_defaults.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\call_defaults.exe
if errorlevel 1 exit /b 1
release\tmp\call_defaults.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range.xl --emit release\tmp\cgen_range.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" release\tmp\cgen_range.c >nul
if errorlevel 1 (
    echo cgen_range.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_assert.xl --emit release\tmp\cgen_range_assert.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" release\tmp\cgen_range_assert.c >nul
if errorlevel 1 (
    echo cgen_range_assert.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range_assert.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range_assert.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range_assert.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_assert_dynamic_condition.xl --emit release\tmp\cgen_assert_dynamic_condition.c
if errorlevel 1 exit /b 1
findstr /C:"int __xl6_assert_cond_" release\tmp\cgen_assert_dynamic_condition.c >nul
if errorlevel 1 (
    echo cgen_assert_dynamic_condition.c missing assert condition temporary.
    exit /b 1
)
findstr /C:"if ( !(__xl6_assert_cond_" release\tmp\cgen_assert_dynamic_condition.c >nul
if errorlevel 1 (
    echo cgen_assert_dynamic_condition.c missing assert condition handoff.
    exit /b 1
)
findstr /C:"({" release\tmp\cgen_assert_dynamic_condition.c >nul
if not errorlevel 1 (
    echo cgen_assert_dynamic_condition.c should not use GNU statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_assert_dynamic_condition.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_assert_dynamic_condition.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_assert_dynamic_condition.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_step_runtime_zero.xl --emit release\tmp\cgen_range_step_runtime_zero.c
if errorlevel 1 exit /b 1
findstr /C:"RangeError" release\tmp\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing range step runtime error guard.
    exit /b 1
)
findstr /C:"xl6_error_print_uncaught" release\tmp\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing uncaught runtime error printer.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range_step_runtime_zero.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range_step_runtime_zero.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range_step_runtime_zero.exe >nul 2> release\tmp\cgen_range_step_runtime_zero.err.txt
if not errorlevel 1 (
    echo cgen_range_step_runtime_zero.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught RangeError: range step cannot be 0" release\tmp\cgen_range_step_runtime_zero.err.txt >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.err.txt missing uncaught RangeError diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_in_range.xl --emit release\tmp\cgen_in_range.c
if errorlevel 1 exit /b 1
findstr /C:"static int xl6_range_contains_i64" release\tmp\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains helper.
    exit /b 1
)
findstr /C:" = xl6_range_contains_i64(3LL, 0, 5LL, 1);" release\tmp\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains call.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" release\tmp\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains temporary argument.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_in_range.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_in_range.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_in_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_in_containers.xl --emit release\tmp\cgen_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFindStr(text, 0, \"pha\", 0, false) != 0" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing string in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_contains(values" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing array in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_list_contains(sparse" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing list in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_contains(unique" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing set in lowering.
    exit /b 1
)
findstr /C:"xvoTableExists(table, \"a\", 0)" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing dict in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_value_contains(dynArray" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing dynamic var in lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_in_containers.c | findstr /C:"xl6_xrt_array_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level literal array in return lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c | findstr /C:"makeDynArray()" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic array in return temp.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_in_containers.c | findstr /C:"xl6_xrt_value_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic container in return lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" release\tmp\cgen_in_containers.c | findstr /C:"xvoTableExists(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level literal dict in return lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c | findstr /C:"makeDynTable()" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic dict in return temp.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container for in lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container release.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if not errorlevel 1 (
    echo cgen_in_containers.c should pre-evaluate in arguments.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if not errorlevel 1 (
    echo cgen_in_containers.c should not use statement expressions for in temporaries.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_in_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_for_in_containers.xl --emit release\tmp\cgen_for_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArrayItemCount(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing array for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_list_snapshot(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing list for-in lowering.
    exit /b 1
)
findstr /C:"xvoSetGetValueAt(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing set for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_snapshot(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing dict for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_iter_snapshot_free(__xl6_list_snapshot_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing list snapshot cleanup.
    exit /b 1
)
findstr /C:"xl6_xrt_iter_snapshot_free(__xl6_dict_snapshot_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing dict snapshot cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_for_in_containers.c'; $bad=@('xrtAVLTB_Iter','__xl6_avl_','__xl6_list_data_','__xl6_dict_data_'); foreach($p in $bad){ if($text.IndexOf($p) -ge 0){ Write-Host ('cgen_for_in_containers.c regressed to AVL iterator shape: '+$p); exit 1 } }; $list=[regex]::Matches($text,'XL6_XrtIterSnapshot\* (__xl6_list_snapshot_\d+) = xl6_xrt_list_snapshot\(__xl6_iter_\d+\);[\s\S]*?for \(unsigned int __xl6_list_pos_\d+ = 0;[\s\S]*?XL6_XrtIterItem\* __xl6_list_item_\d+ = &\1->items\[[^\]]+\];[\s\S]*?xl6_xrt_iter_snapshot_free\(\1\);'); $dict=[regex]::Matches($text,'XL6_XrtIterSnapshot\* (__xl6_dict_snapshot_\d+) = xl6_xrt_dict_snapshot\(__xl6_iter_\d+\);[\s\S]*?for \(unsigned int __xl6_dict_pos_\d+ = 0;[\s\S]*?XL6_XrtIterItem\* __xl6_dict_item_\d+ = &\1->items\[[^\]]+\];[\s\S]*?xl6_xrt_iter_snapshot_free\(\1\);'); if ($list.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing list snapshot sequence.'; exit 1 }; if ($dict.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing dict snapshot sequence.'; exit 1 }"
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(xl6_xrt_value_to_string(key), \"b\", 0, false)" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing var-to-string comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_for_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_for_in_var_non_container.xl --emit release\tmp\cgen_for_in_var_non_container.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_raise(\"TypeError\", \"for in value must be array, list, set or dict\"" release\tmp\cgen_for_in_var_non_container.c >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.c missing var for-in TypeError guard.
    exit /b 1
)
findstr /C:"xl6_error_print_uncaught" release\tmp\cgen_for_in_var_non_container.c >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.c missing uncaught runtime error printer.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_for_in_var_non_container.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_for_in_var_non_container.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_for_in_var_non_container.exe >nul 2> release\tmp\cgen_for_in_var_non_container.err.txt
if not errorlevel 1 (
    echo cgen_for_in_var_non_container.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught TypeError: for in value must be array, list, set or dict" release\tmp\cgen_for_in_var_non_container.err.txt >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.err.txt missing uncaught TypeError diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_approx.xl --emit release\tmp\cgen_approx.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtIntApprox(long long a, long long b);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing xrtIntApprox prototype.
    exit /b 1
)
findstr /C:"expect_i64(xrtIntApprox(a, b), 1LL);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing int approx call.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(x, y), 1LL);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing num approx call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_approx.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_approx.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_approx.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_math.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_math.xl --emit release\tmp\stdlib_math.c
if errorlevel 1 exit /b 1
findstr /C:"double sqrt(double x);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing sqrt extern prototype.
    exit /b 1
)
findstr /C:"double __xl_std_math_round(double x);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(__xl_std_math_round(1.6), 2.0), 1LL);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper call.
    exit /b 1
)
findstr /C:"double xrtMathClamp(double value, double minValue, double maxValue);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing xrtMathClamp extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_math.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_math.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_math.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_math.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_math_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_math_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_math_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_math_c.exe > release\tmp\stdlib_math_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_math_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_math_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib math xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_string.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_string.xl --emit release\tmp\stdlib_string.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing strlen c_size extern prototype.
    exit /b 1
)
findstr /C:"int xrtStrComp" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrComp extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_string_compare" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing compare helper.
    exit /b 1
)
findstr /C:"unsigned int xrtInStr" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtInStr extern prototype.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_starts_with" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string startsWith runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_ends_with" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string endsWith runtime helper.
    exit /b 1
)
findstr /C:"void* xrtCheckStr" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtCheckStr extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_string_has_any" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.hasAny helper.
    exit /b 1
)
findstr /C:"double xrtStrSim" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrSim extern prototype.
    exit /b 1
)
findstr /C:"void* xrtIntFormat" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtIntFormat extern prototype.
    exit /b 1
)
findstr /C:"void* xrtNumFormat" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtNumFormat extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_int" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatInt helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_float" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatFloat helper.
    exit /b 1
)
findstr /C:"char** xrtSplit" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtSplit extern prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_string_split" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string split runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_string_split" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.split helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_string.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_string.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_string.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_string.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_string_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_string_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_string_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_string_c.exe > release\tmp\stdlib_string_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_string_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_string_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib string xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time.xl --emit release\tmp\stdlib_time.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtDateAdd" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtDateAdd extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_time_from_datetime" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing fromDateTime helper.
    exit /b 1
)
findstr /C:"int64_t xrtStrToTime" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtStrToTime extern prototype.
    exit /b 1
)
findstr /C:"void* xrtTimeFormat" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeFormat extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtTimeParse" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeParse extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTryStrToTime" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTryStrToTime extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTryTimeParse" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTryTimeParse extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtDateDiff" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtDateDiff extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_time.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_time_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_time_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time_c.exe > release\tmp\stdlib_time_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_time_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_time_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib time xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_extra.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_extra.xl --emit release\tmp\stdlib_time_extra.c
if errorlevel 1 exit /b 1
findstr /C:"double xrtTimer" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimer extern prototype.
    exit /b 1
)
findstr /C:"void xrtSleep" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtSleep extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTimeRangeOverlap" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimeRangeOverlap extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRelativeTime" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtRelativeTime extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time_extra.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time_extra.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time_extra.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_calendar.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_calendar.xl --emit release\tmp\stdlib_time_calendar.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtToUnixTime" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtToUnixTime extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFirstDayOfMonth" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtFirstDayOfMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtWeekOfYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtWeekOfYear extern prototype.
    exit /b 1
)
findstr /C:"bool xrtIsLeapYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtIsLeapYear extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInMonth" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInYear extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time_calendar.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_random.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_random.xl --emit release\tmp\stdlib_random.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned int xrtRand32" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRand32 extern prototype.
    exit /b 1
)
findstr /C:"int xrtRandRange" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandRange extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandStr" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandStr extern prototype.
    exit /b 1
)
findstr /C:"void xrtRandSeedThread" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandSeedThread extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandCreate" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandCreate extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtRand32Obj" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRand32Obj extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandStrObj" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandStrObj extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_random_string" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing random.string helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_random.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_random.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_random.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_random.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_random_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_random_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_random_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_random_c.exe > release\tmp\stdlib_random_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_random_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_random_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib random xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_buffer.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_buffer.xl --emit release\tmp\stdlib_buffer.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Buffer" release\tmp\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer class bridge.
    exit /b 1
)
findstr /C:"void* xrtBufferCreate" release\tmp\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing xrtBufferCreate extern prototype.
    exit /b 1
)
findstr /C:"bool xrtBufferAppend" release\tmp\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing xrtBufferAppend extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_buffer_create" release\tmp\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer create runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_buffer_append_bytes" release\tmp\stdlib_buffer.c >nul
if errorlevel 1 (
    echo stdlib_buffer.c missing Buffer appendBytes runtime helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_buffer.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_buffer.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_buffer.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_buffer.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_buffer_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_buffer_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_buffer_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_buffer_c.exe > release\tmp\stdlib_buffer_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_buffer_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_buffer_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib buffer xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_async.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_async.xl --emit release\tmp\stdlib_file_async.c
if errorlevel 1 exit /b 1
findstr /C:"xfuture* xl6_xrt_file_read_text_async_utf8" release\tmp\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing async file read helper.
    exit /b 1
)
findstr /C:"xTaskRunThread(xl6_xrt_file_read_text_async_entry" release\tmp\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing XRT task-backed read entry.
    exit /b 1
)
findstr /C:"xfuture* xl6_xrt_file_delete_async" release\tmp\stdlib_file_async.c >nul
if errorlevel 1 (
    echo stdlib_file_async.c missing async file delete helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_file_async.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_file_async.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_file_async.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_file_async.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_file_async_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_file_async_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_file_async_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_file_async_c.exe > release\tmp\stdlib_file_async_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_file_async_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_file_async_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib file async xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_net_tcp_udp.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_net_tcp_udp.xl --emit release\tmp\stdlib_net_tcp_udp.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_TcpStream" release\tmp\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing TcpStream class bridge.
    exit /b 1
)
findstr /C:"void* xrtNetTcpConnect" release\tmp\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing xrtNetTcpConnect extern prototype.
    exit /b 1
)
findstr /C:"void* xrtNetUdpBind" release\tmp\stdlib_net_tcp_udp.c >nul
if errorlevel 1 (
    echo stdlib_net_tcp_udp.c missing xrtNetUdpBind extern prototype.
    exit /b 1
)
release\xl.exe build test\cases\stdlib_net_tcp_udp.xl -o release\tmp\stdlib_net_tcp_udp.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_net_tcp_udp.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_net_tcp_udp.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_net_tcp_udp_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_net_tcp_udp_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_net_tcp_udp_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_net_tcp_udp_c.exe > release\tmp\stdlib_net_tcp_udp_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_net_tcp_udp_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_net_tcp_udp_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib net tcp/udp xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_path_file.xl %XL6_XL_DEFAULT_LINK_ARGS% > release\tmp\stdlib_path_file_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_path_file_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_path_file_c.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_path_file_c.exe > release\tmp\stdlib_path_file_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('release\tmp\stdlib_path_file_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('release\tmp\stdlib_path_file_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib path/file xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_path_file.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_path_file.xl --emit release\tmp\stdlib_path_file.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPathExists" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathExists extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathRandom" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathRandom extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathJoin" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathJoin extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_path_join" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join runtime helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_path_join" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join helper.
    exit /b 1
)
findstr /C:"void* xrtPathNormalize" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathNormalize extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathAbs" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathAbs extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathRel" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathRel extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathCwd" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathCwd extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathTemp" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathTemp extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathWithExt" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathWithExt extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathWithName" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathWithName extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileRename" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileRename extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileReplace" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileReplace extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileTouch" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileTouch extern prototype.
    exit /b 1
)
findstr /C:"bool xrtFileWriteAllAtomic" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtFileWriteAllAtomic extern prototype.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_FileInfo" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing FileInfo class bridge.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_File" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing File class bridge.
    exit /b 1
)
findstr /C:"void* xrtOpen" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtOpen extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_file_open" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing file.open runtime helper.
    exit /b 1
)
findstr /C:"int xrtDirDelete" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirDelete extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirCopy" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirCopy extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirMove" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirMove extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_dir_delete" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.delete helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_copy" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.copy helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_move" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.move helper.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_dir_list" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.list runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_files" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.files helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_dirs" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.dirs helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_path_file.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_path_file.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_path_file.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_meta.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_meta.xl --emit release\tmp\stdlib_file_meta.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtFileSetSize" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileSetSize extern prototype.
    exit /b 1
)
findstr /C:"int xrtFileGetAttr" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAttr extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFileGetAccessTime" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAccessTime extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_file_meta.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_file_meta.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_file_meta.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime
exit /b 0

