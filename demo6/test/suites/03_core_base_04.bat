@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 4/10...

release\xl.exe cgen test\cases\cgen_typed_container_index_assign.xl --emit build\test\cgen_typed_container_index_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_set_at(__xl6_user_global_values" build\test\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed array index assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(__xl6_user_global_names" build\test\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed list index assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_scores" build\test\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed dict index assignment.
    exit /b 1
)
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime header.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(3);" build\test\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime fatal call.
    exit /b 1
)
findstr /C:"exit(3);" build\test\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" build\test\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_index_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign_objects_functions.xl --emit build\test\cgen_typed_container_index_assign_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedListSetValue(__xl6_user_global_pairList" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_pairDict" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_pairDict, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(__xl6_user_global_payloadArray" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(__xl6_user_global_payloadList" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_payloadDict, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(__xl6_user_global_nativeArray" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(__xl6_user_global_nativeList" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_nativeDict" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_nativeDict, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed native-backed class dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(__xl6_user_global_funcs" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(__xl6_user_global_listedFuncs" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_table, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(__xl6_user_global_next)" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing function boxing for typed assignment.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedListGet(__xl6_user_global_listedFuncs" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing guarded typed function list get lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(__xl6_user_global_table" build\test\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict get lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_index_assign_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_multi_assign_index.xl --emit build\test\cgen_typed_container_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_set_at(__xl6_user_global_values" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed array multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_scores" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed dict multi assignment.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_typed_container_multi_assign_index.c' -Raw; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = __xl6_multi_\d+\.value0;[\s\S]*?xl6_xrt_typed_array_set_at\(__xl6_user_global_pairs,[^\r\n]*\1'){exit 1}"
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing class handle multi assignment.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(__xl6_multi_" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing union multi assignment boxing.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_typed_container_multi_assign_index.c' -Raw; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = __xl6_multi_\d+\.value0;[\s\S]*?xrtTypedDictSetValue\(__xl6_user_global_natives,[\s\S]*?\1'){exit 1}"
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing native-backed class handle multi assignment.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(__xl6_multi_" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function multi assignment boxing.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(__xl6_user_global_payloads" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed list multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_natives, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing native-backed class dynamic dict multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_user_global_table, xvoGetText(__xl6_dict_key_" build\test\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function dynamic dict multi assignment.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_typed_container_multi_assign_index.c' -Raw; $fail='(?:xl6_runtime_fatal\(3\)|xl6_error_raise\([^;]+;)'; $simple=[regex]::Matches($t,'xvalue (__xl6_multi_index_\d+) = (?:xvoCreateInt|__xl6_multi_\d+\.value\d+|xl6_box_XL_Union_Payload|xl6_function_to_xvalue)[^;]*;\s*if \(!(?:xl6_xrt_typed_array_set_at|xrtTyped(?:List|Dict)SetValue)\([^\n]*\1\)\) \{\s*' + $fail + '\s*\}\s*xvoUnref\(\1\);',[Text.RegularExpressions.RegexOptions]::Singleline); if($simple.Count -lt 6){exit 1}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = __xl6_multi_\d+\.value0;[\s\S]*?xrtTypedDictSetValue\(__xl6_user_global_natives,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 2}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = xl6_function_to_xvalue\(__xl6_multi_[^;]+;[\s\S]*?xrtTypedDictSetValue\(__xl6_user_global_table,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 3}"
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing multi-return typed index temp cleanup ordering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_compound_assign.xl --emit build\test\cgen_typed_container_index_compound_assign.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_typed_index_current" build\test\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index compound temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_str_concat" build\test\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing string compound helper.
    exit /b 1
)
findstr /C:"xvoCreateInt(xl6_int_pow_i64" build\test\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index pow compound lowering.
    exit /b 1
)
findstr /C:"xvoCreateFloat(__xl6_typed_index_current" build\test\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index float compound lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_current" build\test\cgen_typed_container_index_compound_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c should not use xvalue current for typed index compound assignment.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_index_compound_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\type_values.xl --emit build\test\type_values.c
if errorlevel 1 exit /b 1
findstr /C:"valueType = 0x" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type value declaration.
    exit /b 1
)
findstr /C:"intOk = (__xl6_user_global_valueType == 0x" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type literal comparison.
    exit /b 1
)
findstr /C:"classOk = (__xl6_user_global_boxType == 0x" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing class type literal comparison.
    exit /b 1
)
findstr /C:"(__xl6_user_global_arrayType == 0x" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing array type literal comparison.
    exit /b 1
)
findstr /C:"(__xl6_user_global_dictType == 0x" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing dict type literal comparison.
    exit /b 1
)
findstr /C:"static const xrt_generic_type generic" build\test\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing XRT generic type metadata.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\type_values.exe
if errorlevel 1 exit /b 1
build\test\type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_minimal_runtime.xl --emit build\test\cgen_minimal_runtime.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_minimal_runtime.c' -Raw; if($t -match 'typedef struct XL6_Function'){exit 1}; if($t -match '__xl_std_'){exit 2}; if($t -match 'xl6_xrt_array_varargs'){exit 3}; if($t -match 'static long long xl6_xrt_value_to_i64_release'){exit 4}; if($t -notmatch 'int xlang_main\(xvalue arg\)'){exit 5}; if($t -notmatch 'xvoUnref\(__xl6_arg\);'){exit 6}; if($t -notmatch 'int main\(int argc, char\*\* argv\)'){exit 7}"
if errorlevel 1 (
    echo cgen_minimal_runtime.c runtime needs negative snapshot failed.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_minimal_runtime.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1
build\test\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_top_level_before_main.xl --emit build\test\cgen_top_level_before_main.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_top_level_before_main.c' -Raw; $top=$t.IndexOf('top:'); $call=$t.IndexOf('__xl6_main_code = __xl6_user_main(arg);'); if($t -notmatch 'int __xl6_user_main\(xvalue arg\)'){exit 1}; if($t -notmatch 'int xlang_main\(xvalue arg\)'){exit 2}; if($call -lt 0){exit 3}; if($top -lt 0){exit 4}; if($top -gt $call){exit 5}"
if errorlevel 1 (
    echo cgen_top_level_before_main.c missing top-level/main dispatch.
    exit /b 1
)
release\xl.exe run test\cases\cgen_top_level_before_main.xl > build\test\cgen_top_level_before_main.out.txt 2> build\test\cgen_top_level_before_main.err.txt
if not "%errorlevel%"=="7" (
    echo cgen_top_level_before_main.xl run should return 7.
    exit /b 1
)
findstr /C:"top:main" build\test\cgen_top_level_before_main.out.txt >nul
if errorlevel 1 (
    echo cgen_top_level_before_main.xl output order is wrong.
    exit /b 1
)
release\xl.exe build test\cases\cgen_top_level_before_main.xl -o build\test\cgen_top_level_before_main.exe
if errorlevel 1 exit /b 1
build\test\cgen_top_level_before_main.exe > build\test\cgen_top_level_before_main.exe.out.txt
if not "%errorlevel%"=="7" (
    echo cgen_top_level_before_main.exe should return 7.
    exit /b 1
)
findstr /C:"top:main" build\test\cgen_top_level_before_main.exe.out.txt >nul
if errorlevel 1 (
    echo cgen_top_level_before_main.exe output order is wrong.
    exit /b 1
)

release\xl.exe ir test\cases\type_aliases.xl --emit build\test\type_aliases.xlir
if errorlevel 1 exit /b 1
findstr /C:"typedef MyInt = int" build\test\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing typedef alias.
    exit /b 1
)
findstr /C:"extern type HANDLE = point" build\test\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing extern type alias.
    exit /b 1
)
release\xl.exe cgen test\cases\type_aliases.xl --emit build\test\type_aliases.c
if errorlevel 1 exit /b 1
findstr /C:"long long __xl6_user_global_value;" build\test\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing int alias lowering.
    exit /b 1
)
findstr /C:"void* __xl6_user_global_handle;" build\test\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing point alias lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\type_aliases.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\type_aliases.exe
if errorlevel 1 exit /b 1
build\test\type_aliases.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_concat.xl --emit build\test\cgen_string_concat.c
if errorlevel 1 exit /b 1
findstr /C:"#define XL_RUNTIME_NEED_STRING 1" build\test\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string runtime trim macro.
    exit /b 1
)
findstr /C:"joined = xrtCopyStr(xl6_xrt_str_concat" build\test\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string concat declaration.
    exit /b 1
)
findstr /C:"char* __xl6_string_value_" build\test\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string += lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_string_concat.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_string_concat.exe
if errorlevel 1 exit /b 1
build\test\cgen_string_concat.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_compare.xl --emit build\test\cgen_string_compare.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(__xl6_user_global_left, __xl6_user_global_same, 0, false) == 0" build\test\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string equality lowering.
    exit /b 1
)
findstr /C:"xrtStrComp(__xl6_user_global_left, __xl6_user_global_right, 0, false) < 0" build\test\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string ordered comparison lowering.
    exit /b 1
)
findstr /C:"xrtStrApprox(__xl6_user_global_left, 0, __xl6_user_global_same, 0)" build\test\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_string_compare.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_string_compare.exe
if errorlevel 1 exit /b 1
build\test\cgen_string_compare.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_literals.xl --emit build\test\cgen_string_literals.c
if errorlevel 1 exit /b 1
findstr /C:"const char* __xl6_user_global_single;" build\test\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing single quote string lowering.
    exit /b 1
)
findstr /C:"const char* __xl6_user_global_rawPath;" build\test\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing raw string lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_string_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_string_literals.exe
if errorlevel 1 exit /b 1
build\test\cgen_string_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_fstring.xl --emit build\test\cgen_fstring.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_fstr_join" build\test\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing f-string join helper.
    exit /b 1
)
findstr /C:"#define XL_RUNTIME_NEED_STRING 1" build\test\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing string runtime trim macro.
    exit /b 1
)
findstr /C:"({ xbuffer_struct __xl6_fstr_" build\test\cgen_fstring.c >nul
if not errorlevel 1 (
    echo cgen_fstring.c should not emit GNU statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string" build\test\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing int interpolation conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(__xl6_user_global_dynamic)" build\test\cgen_fstring.c >nul
if errorlevel 1 (
    echo cgen_fstring.c missing var interpolation conversion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_fstring.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_fstring.exe
if errorlevel 1 exit /b 1
build\test\cgen_fstring.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_time_literals.xl --emit build\test\cgen_time_literals.c
if errorlevel 1 exit /b 1
findstr /C:"dateOnly = xrtStrToTime(" build\test\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing date literal lowering.
    exit /b 1
)
findstr /C:"timeOnly = xrtStrToTime(" build\test\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time literal lowering.
    exit /b 1
)
findstr /C:"both = xrtStrToTime(" build\test\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing datetime literal lowering.
    exit /b 1
)
findstr /C:"inferred = xrtStrToTime(" build\test\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time auto inference.
    exit /b 1
)
findstr /C:"xrtTimeApprox(__xl6_user_global_both, xrtStrToTime(" build\test\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_time_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_time_literals.exe
if errorlevel 1 exit /b 1
build\test\cgen_time_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_auto_expressions.xl --emit build\test\cgen_auto_expressions.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_user_global_dynSum;" build\test\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto dynamic expression lowering.
    exit /b 1
)
findstr /C:"= xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto dynamic binary statement assignment.
    exit /b 1
)
findstr /C:"xvalue dynSum = ({ xvalue __xl6_bin_left_" build\test\cgen_auto_expressions.c >nul
if not errorlevel 1 (
    echo cgen_auto_expressions.c should not emit auto dynamic binary declaration statement expressions.
    exit /b 1
)
findstr /C:"long long __xl6_user_global_sum;" build\test\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto integer expression inference.
    exit /b 1
)
findstr /C:"double __xl6_user_global_floatSum;" build\test\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto float expression inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_auto_expressions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1
build\test\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_auto_inference_matrix.xl
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 4/10
exit /b 0
