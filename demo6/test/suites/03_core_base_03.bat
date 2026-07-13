@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 3/10...

release\xl.exe check test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_for_in_returned.xl --emit build\test\cgen_typed_container_for_in_returned.c
if errorlevel 1 exit /b 1
findstr /C:"xtarray xl_fn_makeValues(void)" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array return ABI.
    exit /b 1
)
findstr /C:"xvalue xl_fn_makeValues(void)" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to xvalue array return.
    exit /b 1
)
findstr /C:"static xtarray xl6_fnvalue_xl_fn_makeValues_invoke" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed function value invoke ABI.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_stmt_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing member function statement temp.
    exit /b 1
)
findstr /C:"((xtarray (*)(void*))__xl6_member_fn_stmt_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed member function invoke cast.
    exit /b 1
)
findstr /C:"xl6_function_call_var(__xl6_member_fn_stmt_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to erased member typed container call.
    exit /b 1
)
findstr /C:"(xvalue (*)(void*))__xl6_member_fn_stmt_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to xvalue member typed container invoke cast.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to GNU member function expression block.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_get_at(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed list for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedSetItemAt(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed set for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictClone(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed dict stable snapshot.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(__xl6_typed_dict_snapshot_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict snapshot for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictDestroy(__xl6_typed_dict_snapshot_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict snapshot cleanup.
    exit /b 1
)
findstr /C:"void* __xl6_typed_item_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed for-in item temp.
    exit /b 1
)
findstr /C:"if (__xl6_typed_item_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed for-in item guard.
    exit /b 1
)
findstr /C:"*)xl6_xrt_typed_array_get_at(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed array for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedListItemAt(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed list for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedSetItemAt(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed set for-in item.
    exit /b 1
)
findstr /C:"*)xrtTypedDictItemAt(__xl6_typed_dict_snapshot_" build\test\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c should not directly dereference typed dict for-in item.
    exit /b 1
)
findstr /C:"const char* key = xrtCopyStr(__xl6_typed_key_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key value copy.
    exit /b 1
)
findstr /C:"__xl6_typed_key_len_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key length copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)key);" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(key != 0\) \{\s*xrtFree\(\(void\*\)key\);\s*key = 0;\s*}\s*goto __xl6_continue_\d+;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before continue.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typed dict loop boom[\s\S]*?if \(key != 0\) \{[\s\S]*?xrtFree\(\(void\*\)key\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(key != 0\) \{\s*xrtFree\(\(void\*\)key\);\s*key = 0;\s*}\s*if \(__xl6_typed_dict_snapshot_(\d+) != 0\) \{\s*xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\1\);\s*__xl6_typed_dict_snapshot_\1 = 0;\s*}\s*if \(__xl6_typed_iter_\1 != 0\) \{\s*xrtTypedDictDestroy\(__xl6_typed_iter_\1\);\s*__xl6_typed_iter_\1 = 0;\s*}\s*goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = score;[\s\S]*?if \(key != 0\) \{[\s\S]*?xrtFree\(\(void\*\)key\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict key cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'nestedDictTotal = \(\(nestedDictTotal \+ \(outerScore \* 10LL\)\) \+ innerScore\);[\s\S]*?if \(innerKey != 0\) \{[\s\S]*?xrtFree\(\(void\*\)innerKey\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_(\d+)\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = \(outerScore \+ innerScore\);[\s\S]*?xrtFree\(\(void\*\)innerKey\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);[\s\S]*?xrtFree\(\(void\*\)outerKey\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner and outer cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"nested typed dict loop boom\"[\s\S]*?xrtFree\(\(void\*\)innerKey\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);[\s\S]*?xrtFree\(\(void\*\)outerKey\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_dict_snapshot_\d+\);[\s\S]*?xrtTypedDictDestroy\(__xl6_typed_iter_\d+\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing nested typed dict inner and outer cleanup before throw.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'const char\* __xl6_return_(\d+) = xrtCopyStr\(name, 0u\);[\s\S]*?xrtFree\(\(void\*\)name\);[\s\S]*?xrtTypedListDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?return xrtTempStringTake\(\(char\*\)__xl6_return_\1, 0u\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list string copy before return cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'listContinueBreakTotal = index;[\s\S]*?xrtTypedListDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed list loop boom\"[\s\S]*?xrtTypedListDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed list cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = id;[\s\S]*?if \(__xl6_typed_iter_(\d+) != 0\) \{\s*xrtTypedSetDestroy\(__xl6_typed_iter_\2\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'setContinueBreakValue = id;[\s\S]*?xrtTypedSetDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed set loop boom\"[\s\S]*?xrtTypedSetDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed set cleanup before throw catch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_return_(\d+) = value;[\s\S]*?if \(__xl6_typed_iter_(\d+) != 0\) \{\s*xrtTypedArrayDestroy\(__xl6_typed_iter_\2\);[\s\S]*?return __xl6_return_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before return.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'arrayContinueBreakValue = value;[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before break.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_error_raise\(\"Error\", \"typed array loop boom\"[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_(\d+)\);[\s\S]*?goto __xl6_try_catch_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup before throw catch.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(__xl6_typed_iter_" build\test\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_typed_container_for_in_returned.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'memberDirectTotal = 0LL;[\s\S]*?xtarray __xl6_typed_iter_(\d+) = [^\r\n]*\.values\.signature == 0xDAD5AD8DCC27AA57ULL[\s\S]*?memberDirectTotal = \(memberDirectTotal \+ value\);[\s\S]*?xrtTypedArrayDestroy\(__xl6_typed_iter_\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing direct member returned typed array for-in cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_for_in_returned.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_typed_container_for_in_returned.exe
    if errorlevel 1 (
        echo cgen_typed_container_for_in_returned.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_typed_container_box_to_var.xl --emit build\test\cgen_typed_container_box_to_var.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_to_xvalue" build\test\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed array boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_to_xvalue" build\test\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed list boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_to_xvalue" build\test\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed set boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_to_xvalue" build\test\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed dict boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_bind_type(xl6_xrt_typed_array_to_xvalue" build\test\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing runtime type binding for boxed typed containers.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_box_to_var.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_type_desc_dedupe.xl --emit build\test\cgen_type_desc_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_type_desc_dedupe.c' -Raw; if(([regex]::Matches($t, 'static const xrt_type_desc\* xl6_runtime_type_desc_[0-9A-F]+\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static const xrt_generic_type generic')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_type_desc_dedupe.c duplicated runtime type desc for same typed container.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_type_desc_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1
build\test\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_assign.xl --emit build\test\cgen_typed_container_copy_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayRetain" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array shared retain.
    exit /b 1
)
findstr /C:"xrtTypedListRetain" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list shared retain.
    exit /b 1
)
findstr /C:"xrtTypedSetRetain" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set shared retain.
    exit /b 1
)
findstr /C:"xrtTypedDictRetain" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict shared retain.
    exit /b 1
)
findstr /C:"xrtTypedArrayCount(valuesCopy)" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed container length lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_contains_value_release(idsCopy" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set contains lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_contains_value_release(values" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array contains lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_contains_value_release(names" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list contains lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictExists(scoresCopy" build\test\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict contains lowering.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_in_values_" build\test\cgen_typed_container_copy_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_copy_assign.c should pre-evaluate typed container in arguments.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_copy_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_objects_functions.xl --emit build\test\cgen_typed_container_copy_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayRetain(funcs)" build\test\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed function array shared retain.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain(boxes)" build\test\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class array shared retain.
    exit /b 1
)
findstr /C:"xrtTypedDictRetain(named)" build\test\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class dict shared retain.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain(natives)" build\test\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed native-backed class array shared retain.
    exit /b 1
)
findstr /C:"static void xl6_runtime_class_copy" build\test\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing class handle retain operation.
    exit /b 1
)
findstr /C:"(*(XL_Class_FnBox*)dst).op.retain" build\test\cgen_typed_container_copy_objects_functions.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c regressed to class payload copying.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_copy_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_functions.xl --emit build\test\cgen_typed_container_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeFunction()" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function array create.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xrtTypeFunction()" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function set create.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing guarded callable to function bridge.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(funcs" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function array item call.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedListGet(listed" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function list item call.
    exit /b 1
)
findstr /C:"xl6_function_call_var_release(xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedDictGet(named" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing release fallback for direct typed function dict item call.
    exit /b 1
)
findstr /C:"xrt_typed_array_get_at(funcs, (unsigned int)(0LL)))).invoke)" build\test\cgen_typed_container_functions.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_functions.c should not repeat direct typed function array item invoke.
    exit /b 1
)
findstr /C:"xl6_function_call_var(first" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing function fallback call.
    exit /b 1
)
findstr /C:"XL6_Function each = xl6_function_from_callable" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration bridge.
    exit /b 1
)
findstr /C:"each.release(each.env);" build\test\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_objects.xl --emit build\test\cgen_typed_container_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_type_unbox_XL_Class_Pair" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Class_Pair" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Class_Pair" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class hash type op.
    exit /b 1
)
findstr /C:"xl6_type_unbox_XL_Class_Native" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Class_Native" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Class_Native" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing native-backed class hash type op.
    exit /b 1
)
findstr /C:"xrtTypedSetAddValue(__xl6_global_init_" build\test\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing typed class set fill.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_function_field_objects.xl --emit build\test\cgen_typed_container_function_field_objects.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_runtime_class_copy" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class handle retain helper.
    exit /b 1
)
findstr /C:"static void xl6_type_copy_XL_Class_FnBox" build\test\cgen_typed_container_function_field_objects.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c regressed to class payload copy helper.
    exit /b 1
)
findstr /C:"__xl6_function_field_assign_" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing function field assignment lowering.
    exit /b 1
)
findstr /C:"__xl6_assign_" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class handle assignment path.
    exit /b 1
)
findstr /C:"xl6_function_call_var((*((XL_Class_FnBox*)xl6_class_data(fromArray" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing member function call lowering.
    exit /b 1
)
findstr /C:"xl6_function_call_var((*((XL_Class_FnBox*)xl6_class_data(item" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing iteration member function call lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" build\test\cgen_typed_container_function_field_objects.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c should not use GNU statement expressions for member function calls.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing var-call fallback runtime.
    exit /b 1
)
findstr /C:"fromArray = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain(*((xvalue*)" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed index class handle retain.
    exit /b 1
)
findstr /C:"if (item != 0) { xvoAddRef(item); }" build\test\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed iteration class handle retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_function_field_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 3/10
exit /b 0
