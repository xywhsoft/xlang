@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 1/8...

release\xl.exe cgen test\cases\control_flow_do_while.xl --emit build\test\control_flow_do_while.c
if errorlevel 1 exit /b 1
findstr /C:"while (1) {" build\test\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing canonical loop.
    exit /b 1
)
findstr /C:"if (!(value < 3LL)) {" build\test\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing tail condition.
    exit /b 1
)
findstr /C:"break;" build\test\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing tail break.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\control_flow_do_while.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\control_flow_do_while.exe
if errorlevel 1 exit /b 1
build\test\control_flow_do_while.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_levels.xl --emit build\test\cgen_loop_levels.c
if errorlevel 1 exit /b 1
findstr /C:"goto __xl6_break_" build\test\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level break jump.
    exit /b 1
)
findstr /C:"goto __xl6_continue_" build\test\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level continue jump.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_loop_levels.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_loop_levels.exe
if errorlevel 1 exit /b 1
build\test\cgen_loop_levels.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_loop_multilevel_cleanup.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_multilevel_cleanup.xl --emit build\test\cgen_loop_multilevel_cleanup.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\cgen_loop_multilevel_cleanup.c'; $breakGoto=($lines | Select-String -SimpleMatch 'goto __xl6_break_' | Select-Object -First 1).LineNumber; $breakInner=($lines | Select-String -SimpleMatch 'xvoUnref(innerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $breakOuter=($lines | Select-String -SimpleMatch 'xvoUnref(outerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $continueGoto=($lines | Select-String -SimpleMatch 'goto __xl6_continue_' | Select-Object -First 1).LineNumber; $continueInner=($lines | Select-String -SimpleMatch 'xvoUnref(innerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; $continueOuter=($lines | Select-String -SimpleMatch 'xvoUnref(outerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; if(-not $breakInner -or -not $breakOuter -or -not $breakGoto -or -not ($breakInner -lt $breakOuter -and $breakOuter -lt $breakGoto)){exit 1}; if(-not $continueInner -or -not $continueOuter -or -not $continueGoto -or -not ($continueInner -lt $continueOuter -and $continueOuter -lt $continueGoto)){exit 1}"
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c should destroy inner and outer locals before multi-level jump.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\cgen_loop_multilevel_cleanup.c'; $breakGotos=@($lines | Select-String -SimpleMatch 'goto __xl6_break_'); $continueGotos=@($lines | Select-String -SimpleMatch 'goto __xl6_continue_'); if($breakGotos.Count -lt 2 -or $continueGotos.Count -lt 2){exit 1}; $breakGoto=$breakGotos[1].LineNumber; $breakInner=($lines | Select-String -SimpleMatch 'xvoUnref(innerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $breakOuter=($lines | Select-String -SimpleMatch 'xvoUnref(outerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $continueGoto=$continueGotos[1].LineNumber; $continueInner=($lines | Select-String -SimpleMatch 'xvoUnref(innerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; $continueOuter=($lines | Select-String -SimpleMatch 'xvoUnref(outerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; if(-not $breakInner -or -not $breakOuter -or -not ($breakInner -lt $breakOuter -and $breakOuter -lt $breakGoto)){exit 1}; if(-not $continueInner -or -not $continueOuter -or -not ($continueInner -lt $continueOuter -and $continueOuter -lt $continueGoto)){exit 1}"
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c should destroy while-loop inner and outer locals before multi-level jump.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 6LL);" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing multi-level cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"breakTwoWhileCleanup();" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup call.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 8LL);" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"expect_i64(destroy_value(), 300LL);" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup destroy value assertion.
    exit /b 1
)
findstr /C:"continueTwoWhileCleanup();" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup call.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 12LL);" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"expect_i64(destroy_value(), 501LL);" build\test\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup destroy value assertion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_loop_multilevel_cleanup.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_loop_multilevel_cleanup.exe
if errorlevel 1 exit /b 1
build\test\cgen_loop_multilevel_cleanup.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_loop_multilevel_cleanup.exe
    if errorlevel 1 (
        echo cgen_loop_multilevel_cleanup.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_named_args.xl --emit build\test\cgen_named_args.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_fn_add(1LL, 1LL, 2LL), 4LL);" build\test\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing default argument lowering.
    exit /b 1
)
findstr /C:"expect_i64(xl_fn_add(1LL, 1LL, 4LL), 6LL);" build\test\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named default lowering.
    exit /b 1
)
findstr /C:"xvoCreateObject(xl6_type_desc_XL_Class_Box())" build\test\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing managed class object allocation.
    exit /b 1
)
findstr /C:"xl_class_Box__create(&(*__xl6_object_data_" build\test\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named class payload constructor lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_named_args.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_named_args.exe
if errorlevel 1 exit /b 1
build\test\cgen_named_args.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return.xl --emit build\test\cgen_multi_return.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ __xl6_multi_'){exit 2}"
if errorlevel 1 (
    echo cgen_multi_return.c missing signature return struct or temp.
    exit /b 1
)
findstr /C:"left = __xl6_multi_" build\test\cgen_multi_return.c >nul
if errorlevel 1 (
    echo cgen_multi_return.c missing multi-assign lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return_struct_dedupe.xl --emit build\test\cgen_multi_return_struct_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_multi_return_struct_dedupe.c' -Raw; if(([regex]::Matches($t, 'typedef struct XL6_Function_Return_[0-9A-F]+')).Count -ne 1){exit 1}; if($t -match 'XL6_Return_leftPair'){exit 2}; if($t -match 'XL6_Return_rightPair'){exit 3}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ xl_fn_leftPair\(void\);')).Count -ne 1){exit 4}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ xl_fn_rightPair\(void\);')).Count -ne 1){exit 5}"
if errorlevel 1 (
    echo cgen_multi_return_struct_dedupe.c should reuse one signature return struct.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_multi_return_struct_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1
build\test\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_assign_index.xl --emit build\test\cgen_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_set_at(values, (long long)(0LL), __xl6_multi_index_" build\test\cgen_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_multi_assign_index.c missing index target lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_varargs.xl --emit build\test\cgen_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue arg" build\test\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing xvalue arg.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" build\test\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array varargs lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" build\test\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict varargs lowering.
    exit /b 1
)
findstr /C:"xvoArrayItemCount(arg)" build\test\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array length lowering.
    exit /b 1
)
findstr /C:"xvoTableItemCount(arg)" build\test\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict length lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_varargs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_varargs.exe
if errorlevel 1 exit /b 1
build\test\cgen_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_xvalue_vararg.xl --emit build\test\cgen_extern_xvalue_vararg.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue arg" build\test\cgen_extern_xvalue_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_xvalue_vararg.c missing xvalue vararg bridge parameter.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" build\test\cgen_extern_xvalue_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_xvalue_vararg.c missing extern array vararg lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" build\test\cgen_extern_xvalue_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_xvalue_vararg.c missing extern dict vararg lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_extern_xvalue_vararg.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_extern_xvalue_vararg.exe
if errorlevel 1 exit /b 1
build\test\cgen_extern_xvalue_vararg.exe
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\c_vararg_printf.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_c_vararg.xl --emit build\test\cgen_extern_c_vararg.c
if errorlevel 1 exit /b 1
findstr /C:"printf(const char* fmt, ..." build\test\cgen_extern_c_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_c_vararg.c missing native C ellipsis prototype.
    exit /b 1
)
findstr /C:"c_vararg:%%d:%%s" build\test\cgen_extern_c_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_c_vararg.c missing raw C vararg call lowering.
    exit /b 1
)
findstr /C:"((int)(42LL))" build\test\cgen_extern_c_vararg.c >nul
if errorlevel 1 (
    echo cgen_extern_c_vararg.c missing ABI cast for native C vararg argument.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_extern_c_vararg.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_extern_c_vararg.exe
if errorlevel 1 exit /b 1
build\test\cgen_extern_c_vararg.exe > build\test\cgen_extern_c_vararg.out.txt
if errorlevel 1 exit /b 1
findstr /C:"c_vararg:42:ok" build\test\cgen_extern_c_vararg.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_index_access.xl --emit build\test\cgen_index_access.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_get_at(values, (long long)(1LL))" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed array index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_index_retain_release(xl_fn_makeArray(), (long long)(1LL))" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array temporary receiver helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64_retain_release(xl_fn_makeDynArray(), (long long)(0LL))" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var temporary receiver numeric helper.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_get_key_retain_release(xl_fn_makeTable(), \"temp\")" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict temporary receiver helper.
    exit /b 1
)
findstr /C:"xl6_record_field_get_owned(xl_fn_makeDynTable(), \"temp\", 1)" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var temporary receiver field helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_var_key(dynTable, xl6_xrt_retain(dynKey))" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dynamic var key index helper.
    exit /b 1
)
findstr /C:"xrtTypedListGet(sparse, (long long)(2LL))" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed list index lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictGet(table, \"count\", 0)" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed dict index lowering.
    exit /b 1
)
findstr /C:"xvoTableGetValue(arg, \"retry\", 0)" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict vararg index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_set_at(values, (long long)(1LL), __xl6_index_assign_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed array index assignment lowering.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(sparse, (long long)(2LL), __xl6_index_assign_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed list index assignment lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(table, \"count\", 0, __xl6_index_assign_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed dict index assignment lowering.
    exit /b 1
)
findstr /C:"long long __xl6_typed_index_current_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed index compound current temp.
    exit /b 1
)
findstr /C:"xvalue __xl6_typed_index_assign_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing typed index compound int lowering.
    exit /b 1
)
findstr /C:"nc_probe_next_var()" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var-returning compound RHS probe.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(nc_probe_next_var())" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing compound RHS release conversion helper.
    exit /b 1
)
findstr /C:"__xl6_conv_" build\test\cgen_index_access.c >nul
if not errorlevel 1 (
    echo cgen_index_access.c should not use GNU conversion temporaries.
    exit /b 1
)
findstr /C:"if (xvoIsNull(__xl6_index_current_" build\test\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dynamic index null assignment lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_index_current_" build\test\cgen_index_access.c >nul
if not errorlevel 1 (
    echo cgen_index_access.c should not unref borrowed index current values.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_index_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_index_access.exe
if errorlevel 1 exit /b 1
build\test\cgen_index_access.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generic_array_param.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_protocol_field_neg_compare.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_index.xl --emit build\test\cgen_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(values, (long long)(1LL)))" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing typed array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(table, \"count\", 0))" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64(dynamicValues, (long long)(1LL))" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing dynamic array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_record_field_get_owned(dynamicTable, \"count\", 0)" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing dynamic dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64(missing, (long long)(0LL))" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing null safe index lowering.
    exit /b 1
)
findstr /C:"return xvoIndexGetI64(values, index);" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h should delegate numeric safe index misses to XRT.
    exit /b 1
)
findstr /C:"return xvoIndexGetKey(values, (str)key, 0);" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h should delegate keyed safe index misses to XRT.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" build\test\cgen_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_safe_index.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long __xl6_global_init_" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing declaration-level safe index scalar temporary.
    exit /b 1
)
findstr /C:"declaredFallback = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" build\test\cgen_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_safe_index.c should route declaration-level safe index through scalar temporary.
    exit /b 1
)
findstr /C:"declaredFallback = __xl6_global_init_" build\test\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing declaration-level safe index scalar handoff.
    exit /b 1
)
findstr /C:"declaredFallback = ((long long)xl6_xrt_value_to_i64_release(({" build\test\cgen_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_safe_index.c should not use statement expressions for declaration-level safe index fallback.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_safe_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index.xl --emit build\test\cgen_typed_container_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(values" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed list safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed dict safe index lowering.
    exit /b 1
)
findstr /C:"maybeValue = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(values" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed array safe index as xvalue.
    exit /b 1
)
findstr /C:"missingName = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed list safe index as xvalue.
    exit /b 1
)
findstr /C:"missingScore = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed dict safe index as xvalue.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(xl6_xrt_typed_box_or_null" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue release conversion helper.
    exit /b 1
)
findstr /C:"__xl6_conv_" build\test\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should not use GNU conversion temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(xl6_xrt_typed_box_or_null" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue string conversion release.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" build\test\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"expect_str(__xl6_call_arg_" build\test\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing string call argument handoff.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(({ xvalue __xl6_nc_" build\test\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should pre-evaluate null coalesce string arguments.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index_objects_functions.xl --emit build\test\cgen_typed_container_safe_index_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"maybePair = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_runtime_type_desc_" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed array safe index lowering.
    exit /b 1
)
findstr /C:"maybePayload = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xl6_xrt_typed_array_get_at(payloads" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed array safe index lowering.
    exit /b 1
)
findstr /C:"maybeNative = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_runtime_type_desc_" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed array safe index lowering.
    exit /b 1
)
findstr /C:"maybeFn = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeFunction(), xl6_xrt_typed_array_get_at(funcs" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed array safe index lowering.
    exit /b 1
)
findstr /C:"maybeListPair = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_runtime_type_desc_" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed list safe index lowering.
    exit /b 1
)
findstr /C:"maybeListPayload = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedListGet(payloadList" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed list safe index lowering.
    exit /b 1
)
findstr /C:"maybeListNative = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_runtime_type_desc_" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed list safe index lowering.
    exit /b 1
)
findstr /C:"maybeListFn = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeFunction(), xrtTypedListGet(funcList" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed list safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(pairDict" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"maybeDictPayload = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedDictGet(payloadDict" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed dict safe index lowering.
    exit /b 1
)
findstr /C:"maybeDictNative = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xl6_runtime_type_desc_" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(funcDict" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeFn, false) == fnType" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeListFn, false) == fnType" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeDictFn, false) == fnType" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeFn, 1" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeListFn, 1" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeDictFn, 1" build\test\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function safe index var call lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_safe_index_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_class_handle_items.xl --emit build\test\cgen_typed_container_class_handle_items.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_class_handle_items.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_class_handle_items.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_class_handle_items.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_native_class_items.xl --emit build\test\cgen_typed_container_native_class_items.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_typed_container_native_class_items.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_typed_container_native_class_items.exe
if errorlevel 1 exit /b 1
build\test\cgen_typed_container_native_class_items.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 1/8
exit /b 0
