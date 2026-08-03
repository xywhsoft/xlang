@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 8/10...

release\xl.exe cgen test\cases\cgen_extern_function_value.xl --emit build\test\cgen_extern_function_value.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_extern_function_value.c' -Raw; if($t -notmatch 'static long long xl6_fnvalue_xl_fn_cadd_sig[0-9A-F]+_invoke\(void\* __xl6_env, long long left, long long right\)'){exit 1}"
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function typed wrapper.
    exit /b 1
)
findstr /C:"native_add_i64(left, right)" build\test\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing native extern target call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_extern_function_value.c' -Raw; if($t -notmatch 'xl6_fnvalue_xl_fn_cadd_sig[0-9A-F]+_xcall'){exit 1}"
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function xcall adapter.
    exit /b 1
)
findstr /C:"xl6_export_exportedAdd(left, right)" build\test\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing export function internal target call.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_extern_function_value.c test\cases\cgen_extern_function_value_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1
build\test\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_multi_return.xl --emit build\test\cgen_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function_Return_" build\test\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing function multi-return struct.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_function_multi_return.c' -Raw; if($t -notmatch 'pairFn = xl6_fnvalue_xl_fn_pair_sig[0-9A-F]+_make\(\);'){exit 1}"
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return named function value.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_function_multi_return.c' -Raw; if($t -notmatch 'defaultFn = xl6_fnvalue_xl_fn_pairDefault_sig[0-9A-F]+_arity1_make\(\);'){exit 1}"
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return optional adapter.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_member_function_multi_return.xl --emit build\test\cgen_member_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"XL6_Function __xl6_member_fn_multi_" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return function lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_multi_" build\test\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not use GNU statement expressions for member multi-return calls.
    exit /b 1
)
findstr /C:"XL6_Function_Return_" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return struct.
    exit /b 1
)
findstr /C:".value0 = xl6_xrt_retain(box)" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing class handle return retain.
    exit /b 1
)
findstr /C:"assignedBox = __xl6_multi_" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing owned class handle multi-assignment.
    exit /b 1
)
findstr /C:"returned.op.retain" build\test\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for class rvalue initialization.
    exit /b 1
)
findstr /C:"assignedBox.op.retain" build\test\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for multi-return rvalue assignment.
    exit /b 1
)
findstr /C:"if (__xl6_user_global_assignedBox != 0)" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing previous class handle release before multi-assignment.
    exit /b 1
)
findstr /C:"__xl6_user_global_assignedOp.release != 0" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing function multi-assignment release.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_member_function_multi_return.c' -Raw; if($t -notmatch 'if \(__xl6_multi_\d+\.value0 != 0\) \{\s*xvoUnref\(__xl6_multi_\d+\.value0\);'){exit 1}"
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded class handle multi-return release.
    exit /b 1
)
findstr /C:".value1.release != 0" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded function multi-return release.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return function statement lowering.
    exit /b 1
)
findstr /C:"__xl6_member_result_" build\test\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return result temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_member_function_multi_return.c' -Raw; if($t -notmatch 'if \(__xl6_member_result_\d+\.value0 != 0\) \{ xvoUnref\(__xl6_member_result_\d+\.value0\);'){exit 1}"
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return class handle cleanup.
    exit /b 1
)
findstr /C:"__xl6_member_result_" build\test\cgen_member_function_multi_return.c | findstr /C:".value1.release != 0" >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing ignored member multi-return function cleanup.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_stmt_" build\test\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not use GNU statement expressions for ignored member multi-return calls.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_member_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_member_function_class_return.xl --emit build\test\cgen_member_function_class_return.c --emit-header build\test\cgen_member_function_class_return.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" build\test\cgen_member_function_class_return.h >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.h missing public function value type.
    exit /b 1
)
findstr /C:"XL6_Function op;" build\test\cgen_member_function_class_return.h >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.h missing class function field type.
    exit /b 1
)
findstr /C:"void op;" build\test\cgen_member_function_class_return.h >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.h should not emit void function fields.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level member function temp.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" build\test\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not use GNU statement expressions for class-return member function calls.
    exit /b 1
)
findstr /C:"madeText = xrtCopyStr(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing string-return member function declaration conversion.
    exit /b 1
)
findstr /C:"textArgValue = xl_fn_textScore(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing string-return member function call argument conversion.
    exit /b 1
)
findstr /C:"textNestedValue = (xl_fn_textScore(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested string-return member function argument conversion.
    exit /b 1
)
findstr /C:"textTwoArgValue = xl_fn_joinTextScore(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing two string-return member function argument conversions.
    exit /b 1
)
findstr /C:"namedTextArgValue = xl_fn_textScore(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named string-return member function argument conversion.
    exit /b 1
)
findstr /C:"namedTextTwoArgValue = xl_fn_joinTextScore(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing reordered named string-return member function argument conversions.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_user_global_namedTextTwoArgValue, 22LL)" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named string-return member function runtime assertion.
    exit /b 1
)
findstr /C:"xrtCopyStr(xl6_xvalue_to_string_release(xl6_function_call_var((*((XL_Class_TextMaker*)xl6_class_data(__xl6_user_global_textMaker" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing discarded string-return member function conversion.
    exit /b 1
)
findstr /C:"__xl6_assign_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class handle assignment temporary.
    exit /b 1
)
findstr /C:"xvalue __xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class handle receiver temporary.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class handle receiver cleanup.
    exit /b 1
)
findstr /C:"xl6_function_call_var((*((XL_Class_Payload*)xl6_class_data(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing temporary class handle member function call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xvalue (__xl6_class_arg_\d+) = 0;[\s\S]*?tempFunctionFieldValue\s*=\s*[\s\S]*?xl6_class_data\(\1,[\s\S]*?if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level cleanup for temporary class handle function-field call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -match 'tempFunctionFieldValue\s*=\s*\(\([^\r\n]*maker\.make\.invoke'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c should not inline temporary receiver function-field calls.
    exit /b 1
)
findstr /C:"__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class argument temporary.
    exit /b 1
)
findstr /C:"xl_fn_storePayload(xl6_xrt_retain(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing expression-statement class argument call.
    exit /b 1
)
findstr /C:"nestedArgumentValue = (xl_fn_readPayload(xl6_xrt_retain(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested expression class argument temporary.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_user_global_nestedArgumentValue, 91LL)" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing nested expression class argument expectation.
    exit /b 1
)
findstr /C:"expect_i64((xl_fn_readPayload(xl6_xrt_retain(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing extern nested expression class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'twoArgumentValue\s*=\s*xl_fn_sumPayload\([^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*;\s*if \(\2 != 0\) \{ xvoUnref\(\2\); \2 = 0; \}\s*if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing two temporary class arguments with reverse cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'expect_i64\(xl_fn_sumPayload\([^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*,\s*270LL\);\s*if \(\2 != 0\) \{ xvoUnref\(\2\); \2 = 0; \}\s*if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing extern two temporary class arguments with reverse cleanup.
    exit /b 1
)
findstr /C:"namedArgumentValue = xl_fn_readPayload(xl6_xrt_retain(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_fn_storePayload\(xl6_xrt_retain\((__xl6_class_arg_\d+)\),\s*160LL\);\s*if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named expression-statement class argument temporary.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedTwoArgumentValue\s*=\s*xl_fn_sumPayload\([^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*;\s*if \(\2 != 0\) \{ xvoUnref\(\2\); \2 = 0; \}\s*if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing named two temporary class arguments with reverse cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_member_function_class_return.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'expect_i64\(xl_fn_sumPayload\([^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*xl6_xrt_retain\((__xl6_class_arg_\d+)\)[^;]*,\s*390LL\);\s*if \(\1 != 0\) \{ xvoUnref\(\1\); \1 = 0; \}\s*if \(\2 != 0\) \{ xvoUnref\(\2\); \2 = 0; \}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing reordered named two temporary class arguments with reverse cleanup.
    exit /b 1
)
findstr /C:"expect_i64((xl_fn_readPayload(((maker.make.signature" build\test\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not inline class-return member function values in extern nested calls.
    exit /b 1
)
findstr /C:"xl_fn_readPayload(({ XL6_Function __xl6_member_fn_" build\test\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not use GNU statement expressions in nested class argument calls.
    exit /b 1
)
findstr /C:"void*, xvalue, long long))__xl6_member_fn_stmt_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing statement-level member function invoke with class argument.
    exit /b 1
)
findstr /C:"__xl6_member_result_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing ignored member function return temporary.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_member_result_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing ignored member function class handle cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_class_arg_" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class handle argument cleanup.
    exit /b 1
)
findstr /C:"(xl6_function_fatal(3), xvoCreateNull())" build\test\cgen_member_function_class_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_class_return.c missing class handle return fallback default.
    exit /b 1
)
findstr /C:"(XL_Class_Payload)0" build\test\cgen_member_function_class_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_class_return.c should not cast zero to class return value.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_member_function_class_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_member_function_class_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_member_function_class_return.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_member_function_class_return.exe
    if errorlevel 1 (
        echo cgen_member_function_class_return.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_function_default.xl --emit build\test\cgen_function_default.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" build\test\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function runtime.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_user_global_fn;" build\test\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_function_default.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_function_default.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_default.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_adapter_dedupe.xl --emit build\test\cgen_function_adapter_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_function_adapter_dedupe.c' -Raw; if(([regex]::Matches($t, 'static XL6_Function xl6_fnvalue_xl_fn_addDefault_sig[0-9A-F]+_arity1_make\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static long long xl6_fnvalue_xl_fn_addDefault_sig[0-9A-F]+_arity1_invoke\(')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_function_adapter_dedupe.c duplicated default-arg adapter.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_function_adapter_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\call_defaults.xl --emit build\test\call_defaults.c
if errorlevel 1 exit /b 1
findstr /C:"first = xl_fn_add(1LL, 1LL, 2LL);" build\test\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing default arguments.
    exit /b 1
)
findstr /C:"second = xl_fn_add(1LL, 1LL, 4LL);" build\test\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing named argument reorder.
    exit /b 1
)
findstr /C:"third = xl_fn_add(1LL, 2LL, 3LL);" build\test\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing all named arguments.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\call_defaults.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\call_defaults.exe
if errorlevel 1 exit /b 1
build\test\call_defaults.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range.xl --emit build\test\cgen_range.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" build\test\cgen_range.c >nul
if errorlevel 1 (
    echo cgen_range.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_range.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_range.exe
if errorlevel 1 exit /b 1
build\test\cgen_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_assert.xl --emit build\test\cgen_range_assert.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" build\test\cgen_range_assert.c >nul
if errorlevel 1 (
    echo cgen_range_assert.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_range_assert.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_range_assert.exe
if errorlevel 1 exit /b 1
build\test\cgen_range_assert.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_assert_dynamic_condition.xl --emit build\test\cgen_assert_dynamic_condition.c
if errorlevel 1 exit /b 1
findstr /C:"int __xl6_assert_cond_" build\test\cgen_assert_dynamic_condition.c >nul
if errorlevel 1 (
    echo cgen_assert_dynamic_condition.c missing assert condition temporary.
    exit /b 1
)
findstr /C:"if ( !(__xl6_assert_cond_" build\test\cgen_assert_dynamic_condition.c >nul
if errorlevel 1 (
    echo cgen_assert_dynamic_condition.c missing assert condition handoff.
    exit /b 1
)
findstr /C:"({" build\test\cgen_assert_dynamic_condition.c >nul
if not errorlevel 1 (
    echo cgen_assert_dynamic_condition.c should not use GNU statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_assert_dynamic_condition.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_assert_dynamic_condition.exe
if errorlevel 1 exit /b 1
build\test\cgen_assert_dynamic_condition.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_var_comparison_expressions.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_comparison_expressions.xl --emit build\test\cgen_var_comparison_expressions.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_basic_equal_release(" build\test\cgen_var_comparison_expressions.c >nul
if errorlevel 1 (
    echo cgen_var_comparison_expressions.c missing expression-level equality ownership handoff.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_order_release(" build\test\cgen_var_comparison_expressions.c >nul
if errorlevel 1 (
    echo cgen_var_comparison_expressions.c missing expression-level ordering ownership handoff.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_approx_release(" build\test\cgen_var_comparison_expressions.c >nul
if errorlevel 1 (
    echo cgen_var_comparison_expressions.c missing expression-level approximate ownership handoff.
    exit /b 1
)
findstr /C:"dynamic var comparison requires statement-level lowering" build\test\cgen_var_comparison_expressions.c >nul
if not errorlevel 1 (
    echo cgen_var_comparison_expressions.c retained the old statement-only comparison fallback.
    exit /b 1
)
release\xl.exe run test\cases\cgen_var_comparison_expressions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_var_comparison_expressions.xl -o build\test\cgen_var_comparison_expressions.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_comparison_expressions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_step_runtime_zero.xl --emit build\test\cgen_range_step_runtime_zero.c
if errorlevel 1 exit /b 1
findstr /C:"RangeError" build\test\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing range step runtime error guard.
    exit /b 1
)
findstr /C:"xl6_error_print_uncaught" build\test\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing uncaught runtime error printer.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_range_step_runtime_zero.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_range_step_runtime_zero.exe
if errorlevel 1 exit /b 1
build\test\cgen_range_step_runtime_zero.exe >nul 2> build\test\cgen_range_step_runtime_zero.err.txt
if not errorlevel 1 (
    echo cgen_range_step_runtime_zero.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught RangeError: range step cannot be 0" build\test\cgen_range_step_runtime_zero.err.txt >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.err.txt missing uncaught RangeError diagnostic.
    exit /b 1
)


echo DEMO6 suite ok: core C generation and runtime baseline 8/10
exit /b 0
