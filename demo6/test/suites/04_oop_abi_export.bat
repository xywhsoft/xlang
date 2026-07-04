@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop...

release\xl.exe cgen test\cases\control_flow_do_while.xl --emit release\tmp\control_flow_do_while.c
if errorlevel 1 exit /b 1
findstr /C:"do {" release\tmp\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing do loop.
    exit /b 1
)
findstr /C:"} while ((value < 3LL));" release\tmp\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing while condition.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\control_flow_do_while.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\control_flow_do_while.exe
if errorlevel 1 exit /b 1
release\tmp\control_flow_do_while.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_levels.xl --emit release\tmp\cgen_loop_levels.c
if errorlevel 1 exit /b 1
findstr /C:"goto __xl6_break_" release\tmp\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level break jump.
    exit /b 1
)
findstr /C:"goto __xl6_continue_" release\tmp\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level continue jump.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_loop_levels.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_loop_levels.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_loop_levels.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_loop_multilevel_cleanup.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_multilevel_cleanup.xl --emit release\tmp\cgen_loop_multilevel_cleanup.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_loop_multilevel_cleanup.c'; $breakGoto=($lines | Select-String -SimpleMatch 'goto __xl6_break_' | Select-Object -First 1).LineNumber; $breakInner=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&innerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $breakOuter=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&outerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $continueGoto=($lines | Select-String -SimpleMatch 'goto __xl6_continue_' | Select-Object -First 1).LineNumber; $continueInner=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&innerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; $continueOuter=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&outerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; if(-not $breakInner -or -not $breakOuter -or -not $breakGoto -or -not ($breakInner -lt $breakOuter -and $breakOuter -lt $breakGoto)){exit 1}; if(-not $continueInner -or -not $continueOuter -or -not $continueGoto -or -not ($continueInner -lt $continueOuter -and $continueOuter -lt $continueGoto)){exit 1}"
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c should destroy inner and outer locals before multi-level jump.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_loop_multilevel_cleanup.c'; $breakGotos=@($lines | Select-String -SimpleMatch 'goto __xl6_break_'); $continueGotos=@($lines | Select-String -SimpleMatch 'goto __xl6_continue_'); if($breakGotos.Count -lt 2 -or $continueGotos.Count -lt 2){exit 1}; $breakGoto=$breakGotos[1].LineNumber; $breakInner=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&innerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $breakOuter=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&outerTracker);' | Where-Object { $_.LineNumber -lt $breakGoto } | Select-Object -Last 1).LineNumber; $continueGoto=$continueGotos[1].LineNumber; $continueInner=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&innerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; $continueOuter=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&outerTracker);' | Where-Object { $_.LineNumber -gt $breakGoto -and $_.LineNumber -lt $continueGoto } | Select-Object -Last 1).LineNumber; if(-not $breakInner -or -not $breakOuter -or -not ($breakInner -lt $breakOuter -and $breakOuter -lt $breakGoto)){exit 1}; if(-not $continueInner -or -not $continueOuter -or -not ($continueInner -lt $continueOuter -and $continueOuter -lt $continueGoto)){exit 1}"
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c should destroy while-loop inner and outer locals before multi-level jump.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 6LL);" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing multi-level cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"breakTwoWhileCleanup();" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup call.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 8LL);" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"expect_i64(destroy_value(), 300LL);" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while break cleanup destroy value assertion.
    exit /b 1
)
findstr /C:"continueTwoWhileCleanup();" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup call.
    exit /b 1
)
findstr /C:"expect_i64(destroy_count(), 12LL);" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup destroy count assertion.
    exit /b 1
)
findstr /C:"expect_i64(destroy_value(), 501LL);" release\tmp\cgen_loop_multilevel_cleanup.c >nul
if errorlevel 1 (
    echo cgen_loop_multilevel_cleanup.c missing while continue cleanup destroy value assertion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_loop_multilevel_cleanup.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_loop_multilevel_cleanup.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_loop_multilevel_cleanup.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\tmp\cgen_loop_multilevel_cleanup.exe
    if errorlevel 1 (
        echo cgen_loop_multilevel_cleanup.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_named_args.xl --emit release\tmp\cgen_named_args.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(add(1LL, 1LL, 2LL), 4LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing default argument lowering.
    exit /b 1
)
findstr /C:"expect_i64(add(1LL, 1LL, 4LL), 6LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named default lowering.
    exit /b 1
)
findstr /C:"xl_class_Box__create(&box, 6LL, 4LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named class constructor lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_named_args.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_named_args.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_named_args.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return.xl --emit release\tmp\cgen_multi_return.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ __xl6_multi_'){exit 2}"
if errorlevel 1 (
    echo cgen_multi_return.c missing signature return struct or temp.
    exit /b 1
)
findstr /C:"left = __xl6_multi_" release\tmp\cgen_multi_return.c >nul
if errorlevel 1 (
    echo cgen_multi_return.c missing multi-assign lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return_struct_dedupe.xl --emit release\tmp\cgen_multi_return_struct_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_multi_return_struct_dedupe.c' -Raw; if(([regex]::Matches($t, 'typedef struct XL6_Function_Return_[0-9A-F]+')).Count -ne 1){exit 1}; if($t -match 'XL6_Return_leftPair'){exit 2}; if($t -match 'XL6_Return_rightPair'){exit 3}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ leftPair\(void\);')).Count -ne 1){exit 4}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ rightPair\(void\);')).Count -ne 1){exit 5}"
if errorlevel 1 (
    echo cgen_multi_return_struct_dedupe.c should reuse one signature return struct.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_return_struct_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_assign_index.xl --emit release\tmp\cgen_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArraySetValue(values, (unsigned int)(0LL), __xl6_multi_index_" release\tmp\cgen_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_multi_assign_index.c missing index target lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_varargs.xl --emit release\tmp\cgen_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue arg" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing xvalue arg.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array varargs lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict varargs lowering.
    exit /b 1
)
findstr /C:"xvoArrayItemCount(arg)" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array length lowering.
    exit /b 1
)
findstr /C:"xvoTableItemCount(arg)" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict length lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_varargs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_varargs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_index_access.xl --emit release\tmp\cgen_index_access.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArrayGetValue(values, (unsigned int)(1LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_index_retain_release(makeArray(), (long long)(1LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array temporary receiver helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64_retain_release(makeDynArray(), (long long)(0LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var temporary receiver numeric helper.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_get_key_retain_release(makeTable(), \"temp\")" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict temporary receiver helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_key_retain_release(makeDynTable(), \"temp\")" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var temporary receiver key helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_var_key(dynTable, xl6_xrt_retain(dynKey))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dynamic var key index helper.
    exit /b 1
)
findstr /C:"xvoListGetValue(sparse, (long long)(2LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing list index lowering.
    exit /b 1
)
findstr /C:"xvoTableGetValue(table, \"count\", 0)" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict index lowering.
    exit /b 1
)
findstr /C:"xvoTableGetValue(arg, \"retry\", 0)" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict vararg index lowering.
    exit /b 1
)
findstr /C:"xvoArraySetValue(values, (unsigned int)(1LL), __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array index assignment lowering.
    exit /b 1
)
findstr /C:"xvoListSetValue(sparse, (long long)(2LL), __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing list index assignment lowering.
    exit /b 1
)
findstr /C:"xvoTableSetValue(table, \"count\", 0, __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict index assignment lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index compound current temp.
    exit /b 1
)
findstr /C:"xvoCreateInt(xvoGetInt(__xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index compound int lowering.
    exit /b 1
)
findstr /C:"nc_probe_next_var()" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var-returning compound RHS probe.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(nc_probe_next_var())" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing compound RHS release conversion helper.
    exit /b 1
)
findstr /C:"__xl6_conv_" release\tmp\cgen_index_access.c >nul
if not errorlevel 1 (
    echo cgen_index_access.c should not use GNU conversion temporaries.
    exit /b 1
)
findstr /C:"if (xvoIsNull(__xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index null assignment lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_index_current_" release\tmp\cgen_index_access.c >nul
if not errorlevel 1 (
    echo cgen_index_access.c should not unref borrowed index current values.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_index_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_index_access.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_index_access.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_index.xl --emit release\tmp\cgen_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_index_safe_get_i64(values, (long long)(1LL))" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_key(table, \"count\")" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64(missing, (long long)(0LL))" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing null safe index lowering.
    exit /b 1
)
findstr /C:"return xvoArrayGetValue(0, 0u);" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c should return borrowed null for numeric safe index misses.
    exit /b 1
)
findstr /C:"return xvoTableGetValue(0, key, 0);" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c should return borrowed null for keyed safe index misses.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" release\tmp\cgen_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_safe_index.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long declaredFallback = 0;" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing declaration-level safe index scalar temporary.
    exit /b 1
)
findstr /C:"declaredFallback = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing declaration-level safe index scalar handoff.
    exit /b 1
)
findstr /C:"declaredFallback = ((long long)xl6_xrt_value_to_i64_release(({" release\tmp\cgen_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_safe_index.c should not use statement expressions for declaration-level safe index fallback.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_safe_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index.xl --emit release\tmp\cgen_typed_container_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(values" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed list safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeValue = xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(values" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed array safe index as xvalue.
    exit /b 1
)
findstr /C:"xvalue missingName = xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed list safe index as xvalue.
    exit /b 1
)
findstr /C:"xvalue missingScore = xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed dict safe index as xvalue.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(xl6_xrt_typed_box_or_null" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue release conversion helper.
    exit /b 1
)
findstr /C:"__xl6_conv_" release\tmp\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should not use GNU conversion temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(xl6_xrt_typed_box_or_null" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue string conversion release.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" release\tmp\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"expect_str(__xl6_call_arg_" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing string call argument handoff.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(({ xvalue __xl6_nc_" release\tmp\cgen_typed_container_safe_index.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_safe_index.c should pre-evaluate null coalesce string arguments.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index_objects_functions.xl --emit release\tmp\cgen_typed_container_safe_index_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue maybePair = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Pair(), xl6_xrt_typed_array_get_at(pairs" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybePayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xl6_xrt_typed_array_get_at(payloads" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Native(), xl6_xrt_typed_array_get_at(natives" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeFn = xl6_xrt_typed_box_or_null(xrtTypeFunction(), xl6_xrt_typed_array_get_at(funcs" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListPair = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Pair(), xrtTypedListGet(pairList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListPayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedListGet(payloadList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Native(), xrtTypedListGet(nativeList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListFn = xl6_xrt_typed_box_or_null(xrtTypeFunction(), xrtTypedListGet(funcList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed list safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(pairDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeDictPayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedDictGet(payloadDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeDictNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Native(), xrtTypedDictGet(nativeDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing native-backed class typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_get_var_key(funcDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeListFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeDictFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeListFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeDictFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function safe index var call lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_safe_index_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_dynamic_member_access.xl --emit release\tmp\cgen_dynamic_member_access.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_index_safe_get_key(obj," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing borrowed dynamic member access.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_key_retain_release(makeObj()," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing temporary dynamic member retain-release helper.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_key_retain_release(xl6_xrt_index_safe_get_key_retain_release(makeObj()," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing nested temporary dynamic member retain-release helper.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_member_values_" release\tmp\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should not use GNU statement expressions for temporary dynamic members.
    exit /b 1
)
findstr /C:"xvalue __xl6_member_assign_" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing dynamic member assignment temporary.
    exit /b 1
)
findstr /C:"xvoTableSetValue(obj," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing direct dynamic member assignment lowering.
    exit /b 1
)
findstr /C:"xvoTableSetValue(xl6_xrt_index_safe_get_key(nested," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing nested dynamic member assignment lowering.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" release\tmp\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long declaredSafeMissing = 0;" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing declaration-level dynamic member scalar temporary.
    exit /b 1
)
findstr /C:"declaredSafeMissing = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing declaration-level dynamic member scalar handoff.
    exit /b 1
)
findstr /C:"declaredSafeMissing = ((long long)xl6_xrt_value_to_i64_release(({" release\tmp\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should not use statement expressions for declaration-level dynamic member fallback.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_dynamic_member_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_dynamic_container_param.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_dynamic_container_param.xl --emit release\tmp\cgen_dynamic_container_param.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue take(xvalue values)" release\tmp\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array parameter lowering.
    exit /b 1
)
findstr /C:"xvalue choose(xvalue values)" release\tmp\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic dict parameter lowering.
    exit /b 1
)
findstr /C:"xvoArrayGetValue(values, (unsigned int)(0LL))" release\tmp\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array index return.
    exit /b 1
)
findstr /C:"xvalue direct = take(xl6_xrt_array_varargs(2," release\tmp\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array literal argument.
    exit /b 1
)
findstr /C:"xvalue named = choose(xl6_xrt_dict_varargs(1," release\tmp\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic dict literal argument.
    exit /b 1
)
release\xl.exe build test\cases\cgen_dynamic_container_param.xl -o release\tmp\cgen_dynamic_container_param.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_dynamic_container_param.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_member.xl --emit release\tmp\cgen_safe_member.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_member_safe_length(values)" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing array safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length(missing)" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing null safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length_release(makeArray())" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary array safe member release helper.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length_release(makeDynArray())" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary dynamic safe member release helper.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_member_values_" release\tmp\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should not use GNU statement expressions for temporary safe members.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_nc_" release\tmp\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long declaredLengthFallback = 0;" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing declaration-level safe member scalar temporary.
    exit /b 1
)
findstr /C:"declaredLengthFallback = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing declaration-level safe member scalar handoff.
    exit /b 1
)
findstr /C:"declaredLengthFallback = ((long long)xl6_xrt_value_to_i64_release(({" release\tmp\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should not use statement expressions for declaration-level safe member fallback.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_safe_member.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_safe_member.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_safe_member.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_varargs.xl --emit release\tmp\cgen_overload_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_fn_pick__string(\"x\"), 1LL);" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing fixed overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_fn_pick__string__array_u003C_var_u003E_(\"x\", xl6_xrt_array_varargs(1" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing vararg overload call.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(1" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing dict vararg overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overload_varargs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\control_flow_inline_if.xl --emit release\tmp\control_flow_inline_if.c
if errorlevel 1 exit /b 1
findstr /C:"if ((value > 0LL))" release\tmp\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing first inline if.
    exit /b 1
)
findstr /C:"else {" release\tmp\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing inline else.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\control_flow_inline_if.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\control_flow_inline_if.exe
if errorlevel 1 exit /b 1
release\tmp\control_flow_inline_if.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_assign_ops.xl --emit release\tmp\cgen_assign_ops.c
if errorlevel 1 exit /b 1
findstr /C:"a += 2LL;" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing compound assignment.
    exit /b 1
)
findstr /C:"long long selected = (1 ? inverted : 1LL);" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing ternary expression.
    exit /b 1
)
findstr /C:"int logicAnd = (1 && (!0));" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing logical expression.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_assign_ops.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_assign_ops.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_assign_ops.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_null_coalesce.xl --emit release\tmp\cgen_null_coalesce.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue fallback = 0;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce statement lowering.
    exit /b 1
)
findstr /C:"fallback = xvoCreateInt((long long)(9LL));" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce fallback assignment.
    exit /b 1
)
findstr /C:"xvalue probed = 0;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing single-evaluation null coalesce lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_nc_" release\tmp\cgen_null_coalesce.c | findstr /C:"nc_probe_next_var();" >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce left temporary.
    exit /b 1
)
findstr /C:"long long staticProbed = 0;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static single-evaluation null coalesce lowering.
    exit /b 1
)
findstr /C:"staticProbed = 44LL;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce fallback assignment.
    exit /b 1
)
findstr /C:"long long __xl6_call_arg_" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce call argument temp.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" release\tmp\cgen_null_coalesce.c | findstr /C:"__xl6_nc_" >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce call argument branch assignment.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce call argument replacement.
    exit /b 1
)
findstr /C:"xvalue fallback = ({ xvalue __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses var null coalesce statement expression.
    exit /b 1
)
findstr /C:"xvalue probed = ({ xvalue __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses probed null coalesce statement expression.
    exit /b 1
)
findstr /C:"long long staticProbed = ({ long long __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses static null coalesce statement expression.
    exit /b 1
)
findstr /C:"expect_i64(({ long long __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses call argument null coalesce statement expression.
    exit /b 1
)
findstr /C:"xvalue literal = xvoCreateInt((long long)(7LL));" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing literal null coalesce boxing.
    exit /b 1
)
findstr /C:"long long staticInt = 5LL;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce inference.
    exit /b 1
)
findstr /C:"if (xvoIsNull(assigned))" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_null_coalesce.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_pow.xl --emit release\tmp\cgen_pow.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_int_pow_i64" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing int pow helper.
    exit /b 1
)
findstr /C:"long long rightAssoc = xl6_int_pow_i64(2LL, xl6_int_pow_i64(3LL, 2LL));" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing right-associative pow lowering.
    exit /b 1
)
findstr /C:"value = xl6_int_pow_i64(value, 4LL);" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing pow assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_pow.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_pow.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_pow.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom.xl --emit release\tmp\cgen_custom.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Counter {" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing native-backed class handle struct.
    exit /b 1
)
findstr /C:"xl_class_Counter__create(&counter, 40LL);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing native-backed class create call.
    exit /b 1
)
findstr /C:"long long afterAdd = xl_class_Counter__add(&counter, 2LL);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing native-backed class method call.
    exit /b 1
)
findstr /C:"xl_class_Counter__destroy(&counter);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing native-backed class destroy call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom.c test\cases\cgen_custom_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class.xl --emit release\tmp\cgen_native_class.c
if errorlevel 1 exit /b 1
findstr /C:"void* counter_open(long long seed);" release\tmp\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native create C prototype.
    exit /b 1
)
findstr /C:"self->handle = counter_open(seed);" release\tmp\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native create wrapper.
    exit /b 1
)
findstr /C:"return counter_add(self->handle, value);" release\tmp\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native method wrapper.
    exit /b 1
)
findstr /C:"counter_close(self->handle);" release\tmp\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native destroy wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_native_class.c test\cases\cgen_custom_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_native_class.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_native_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_bind_point.xl --emit release\tmp\cgen_native_class_bind_point.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = raw;" release\tmp\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing native point binding.
    exit /b 1
)
findstr /C:"borrowed_box_close(self->handle);" release\tmp\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing native destroy wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_native_class_bind_point.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_native_class_bind_point.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_native_class_bind_point.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_field.xl --emit release\tmp\cgen_native_class_field.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_NativeBox__value(&self->box)" release\tmp\cgen_native_class_field.c >nul
if errorlevel 1 (
    echo cgen_native_class_field.c missing native class field method call.
    exit /b 1
)
findstr /C:"xl_class_NativeBox__destroy(&self->box);" release\tmp\cgen_native_class_field.c >nul
if errorlevel 1 (
    echo cgen_native_class_field.c missing native class field cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_native_class_field.c test\cases\cgen_class_custom_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_native_class_field.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_native_class_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_property.xl --emit release\tmp\cgen_native_class_property.c
if errorlevel 1 exit /b 1
findstr /C:"long long native_property_get_value(void* self);" release\tmp\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern get C prototype.
    exit /b 1
)
findstr /C:"void native_property_set_at(void* self, long long index, long long value);" release\tmp\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern let C prototype.
    exit /b 1
)
findstr /C:"return native_property_get_value(self->handle);" release\tmp\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern get wrapper.
    exit /b 1
)
findstr /C:"native_property_set_at(self->handle, index, value);" release\tmp\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern let wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_native_class_property.c test\cases\cgen_native_class_property_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_native_class_property.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_native_class_property.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_helper.xl --emit release\tmp\cgen_custom_helper.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Counter__plus(XL_Class_Counter* self, long long extra);" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing native-backed class helper prototype.
    exit /b 1
)
findstr /C:"counter_value(self->handle)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing native-backed class helper self handle call.
    exit /b 1
)
findstr /C:"xl_class_Counter__isZero(&counter)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing native-backed class helper call.
    exit /b 1
)
findstr /C:"xl_class_Counter__plus(&counter, 7LL)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing native-backed class helper call args.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_helper.c test\cases\cgen_custom_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_helper.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_helper.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_bind_point.xl --emit release\tmp\cgen_custom_bind_point.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = make_borrowed_box(55LL);" release\tmp\cgen_custom_bind_point.c >nul
if errorlevel 1 (
    echo cgen_custom_bind_point.c missing point binding.
    exit /b 1
)
findstr /C:"xl_class_BorrowedBox__destroy(&box);" release\tmp\cgen_custom_bind_point.c >nul
if errorlevel 1 (
    echo cgen_custom_bind_point.c missing borrowed destroy.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_bind_point.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_bind_point.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_bind_point.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_ownership.xl --emit release\tmp\cgen_custom_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_OpenOnly__create(&box, 66LL);" release\tmp\cgen_custom_ownership.c >nul
if errorlevel 1 (
    echo cgen_custom_ownership.c missing create-only native-backed class construction.
    exit /b 1
)
findstr /C:"box.handle = handle;" release\tmp\cgen_custom_ownership.c >nul
if errorlevel 1 (
    echo cgen_custom_ownership.c missing view-only point binding.
    exit /b 1
)
findstr /C:"borrowed_box_close" release\tmp\cgen_custom_ownership.c >nul
if not errorlevel 1 (
    echo cgen_custom_ownership.c should not emit destroy without destroy mapping.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_ownership.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_ownership.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_null_destroy.xl --emit release\tmp\cgen_custom_null_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"if (self->handle != 0)" release\tmp\cgen_custom_null_destroy.c >nul
if errorlevel 1 (
    echo cgen_custom_null_destroy.c missing null-handle destroy guard.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_null_destroy.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_null_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_null_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class.xl --emit release\tmp\cgen_class.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point {" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing class struct.
    exit /b 1
)
findstr /C:"XL_Class_Point base;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing base field.
    exit /b 1
)
findstr /C:"p.x = 3LL;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing field default.
    exit /b 1
)
findstr /C:"q.base.x = 10LL;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing inherited field assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create.xl --emit release\tmp\cgen_base_create.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create(&self->base, seed);" release\tmp\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing explicit base create call.
    exit /b 1
)
findstr /C:"xl_class_AutoBase__create(&autoChild.base);" release\tmp\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing auto base create call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_base_create.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_base_create.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_base_create.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create_overloads.xl --emit release\tmp\cgen_base_create_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create__void(&child.base);" release\tmp\cgen_base_create_overloads.c >nul
if errorlevel 1 (
    echo cgen_base_create_overloads.c missing auto no-arg base create overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_base_create_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_methods.xl --emit release\tmp\cgen_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Point__create(XL_Class_Point* self, long long x, long long y);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Point__length2(XL_Class_Point* self);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method prototype.
    exit /b 1
)
findstr /C:"xl_class_Point__create(&p, 3LL, 4LL);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Point__length2(&p);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_method_multi_return.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_method_multi_return.xl --emit release\tmp\cgen_class_method_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function_Return_" release\tmp\cgen_class_method_multi_return.c >nul
if errorlevel 1 (
    echo cgen_class_method_multi_return.c missing class method multi-return struct.
    exit /b 1
)
findstr /C:"xl_class_Pairer__pair(&pairer, 3LL)" release\tmp\cgen_class_method_multi_return.c >nul
if errorlevel 1 (
    echo cgen_class_method_multi_return.c missing class method multi-return call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_method_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_method_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_method_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_object_protocol.xl --emit release\tmp\cgen_object_protocol.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_object______setField____(&a.base" release\tmp\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing object field protocol setter call.
    exit /b 1
)
findstr /C:"xl_class_object______setItem____(&a.base" release\tmp\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing object item protocol setter call.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_protocol_value_" release\tmp\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing protocol value cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_object_protocol.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_object_protocol.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_object_protocol.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_ops.xl --emit release\tmp\cgen_protocol_ops.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Counter______add____(&c" release\tmp\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing operator protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______contains____(&c" release\tmp\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing contains protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______call____(&c" release\tmp\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing call protocol call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_protocol_ops.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_protocol_ops.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_protocol_ops.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_overload_index.xl --emit release\tmp\cgen_protocol_overload_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Grid______item______int__int" release\tmp\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing two-dimensional item overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______setItem______int__int__int" release\tmp\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing two-dimensional setItem overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______call______string" release\tmp\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string call overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______contains______string" release\tmp\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string contains overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______add______string" release\tmp\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string add overload symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_protocol_overload_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_protocol_overload_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_protocol_overload_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_property_get_let.xl --emit release\tmp\cgen_class_property_get_let.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Player______get______hp__void" release\tmp\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing zero-dimensional property getter symbol.
    exit /b 1
)
findstr /C:"xl_class_Player______let______hp__int__int__int" release\tmp\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing multi-dimensional property setter symbol.
    exit /b 1
)
findstr /C:"xl_class_Player______get______hp__int__int(&player, 2LL, 4LL)" release\tmp\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing multi-dimensional property getter call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_property_get_let.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_property_get_let.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_property_get_let.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_complex_receiver.xl --emit release\tmp\cgen_protocol_complex_receiver.c
if errorlevel 1 exit /b 1
findstr /C:"xl_protocol_tmp_Counter______add____" release\tmp\cgen_protocol_complex_receiver.c >nul
if errorlevel 1 (
    echo cgen_protocol_complex_receiver.c missing value receiver protocol wrapper.
    exit /b 1
)
findstr /C:"xl_protocol_tmp_Bag______item____" release\tmp\cgen_protocol_complex_receiver.c >nul
if errorlevel 1 (
    echo cgen_protocol_complex_receiver.c missing item protocol wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_protocol_complex_receiver.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_protocol_complex_receiver.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_protocol_complex_receiver.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_len.xl --emit release\tmp\cgen_protocol_len.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Sized______len____(&value)" release\tmp\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing named receiver len protocol call.
    exit /b 1
)
findstr /C:"xl_protocol_tmp_Sized______len____" release\tmp\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing temporary receiver len protocol wrapper.
    exit /b 1
)
findstr /C:"withField.length = 4LL;" release\tmp\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing static length field assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_protocol_len.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_protocol_len.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_protocol_len.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_protocol_iter.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_protocol_iter.xl --emit release\tmp\cgen_protocol_iter.c
if errorlevel 1 exit /b 1
findstr /C:"xl_protocol_tmp_Counter______iter______void" release\tmp\cgen_protocol_iter.c >nul
if errorlevel 1 (
    echo cgen_protocol_iter.c missing temporary receiver iter protocol wrapper.
    exit /b 1
)
findstr /C:"xl_class_CounterIter______next____(&__xl6_class_iter_" release\tmp\cgen_protocol_iter.c >nul
if errorlevel 1 (
    echo cgen_protocol_iter.c missing iterator next protocol call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_protocol_iter.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_protocol_iter.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_protocol_iter.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_copy_clone.xl --emit release\tmp\cgen_copy_clone.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Packet______copy____" release\tmp\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing derived copy protocol.
    exit /b 1
)
findstr /C:"xl_class_Packet______clone____" release\tmp\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing derived clone protocol.
    exit /b 1
)
findstr /C:"xvoDeepCopy(value.values)" release\tmp\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing deep xvalue field copy.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_copy_clone.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_copy_clone.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_copy_clone.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_deepcopy_removed.xl --emit release\tmp\cgen_deepcopy_removed.c > release\tmp\cgen_deepcopy_removed.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_deepcopy_removed.xl should fail because deepcopy is not a builtin alias.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\cgen_deepcopy_removed.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x6682)+([char]0x4E0D)+([char]0x652F)+([char]0x6301); if($text.IndexOf($expected) -lt 0){ Write-Host 'cgen_deepcopy_removed.err.txt missing unsupported diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_derive_persistence.xl --emit release\tmp\cgen_derive_persistence.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Player______toValue____" release\tmp\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing derived toValue protocol.
    exit /b 1
)
findstr /C:"xl_class_Player______fromValue____" release\tmp\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing derived fromValue protocol.
    exit /b 1
)
findstr /C:"__xl6__stringify__value" release\tmp\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing class stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_derive_persistence.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_derive_persistence.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_derive_persistence.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_field_persistence_attrs.xl --emit release\tmp\cgen_field_persistence_attrs.c
if errorlevel 1 exit /b 1
findstr /C:"display_name" release\tmp\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing json field alias.
    exit /b 1
)
findstr /C:"displayNameX" release\tmp\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing xson field alias.
    exit /b 1
)
findstr /C:"secret_name" release\tmp\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing included private field alias.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_field_persistence_attrs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_field_persistence_attrs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_field_persistence_attrs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_overloads.xl --emit release\tmp\cgen_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Pair__create__void(XL_Class_Pair* self);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing no-arg create overload symbol.
    exit /b 1
)
findstr /C:"void xl_class_Pair__create__int(XL_Class_Pair* self, long long value);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing int create overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__int(XL_Class_Ops* self, long long value);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class int method overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__string(XL_Class_Ops* self, const char* text);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class string method overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Base__pick(&child.base, 7LL)" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing inherited method base receiver path.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\cgen_unicode_identifiers.xl --emit release\tmp\cgen_unicode_identifiers.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"symbol: xl_fn__u52A0__u4E00___int" release\tmp\cgen_unicode_identifiers.abi.txt >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.abi.txt missing encoded func symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_unicode_identifiers.xl --emit release\tmp\cgen_unicode_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class__u70B9_ {" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class type.
    exit /b 1
)
findstr /C:"long long _u6A2A_;" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class field.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00___int(" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function symbol.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00___int(long long _u6570__u503C_)" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function parameter.
    exit /b 1
)
findstr /C:"long long xl_class__u70B9____u6C42__u548C_(XL_Class__u70B9_* self);" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class method symbol.
    exit /b 1
)
findstr /C:"long long _u7ED3__u679C_ = xl_fn__u52A0__u4E00___int(41LL);" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded local variable.
    exit /b 1
)
findstr /C:"XL_Class__u70B9_ _u5750__u6807_ = {0};" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class variable.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_unicode_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_destroy.xl --emit release\tmp\cgen_class_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Tracker__destroy(XL_Class_Tracker* self);" release\tmp\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_Tracker__destroy(&tracker);" release\tmp\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing scope destroy call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_cleanup_order.xl --emit release\tmp\cgen_cleanup_order.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_cleanup_order.c'; $second=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&second);' | Select-Object -First 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&first);' | Select-Object -First 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_cleanup_order.c should destroy later local before earlier local.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_cleanup_order.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_inherit_destroy.xl --emit release\tmp\cgen_class_inherit_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_AutoChild__destroy(XL_Class_AutoChild* self);" release\tmp\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing inherited auto destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_Base__destroy(&self->base);" release\tmp\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing base destroy tail call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_class_inherit_destroy.c'; $body=($lines | Select-String -SimpleMatch 'record_destroy(self->value);' | Select-Object -First 1).LineNumber; $base=($lines | Select-String -SimpleMatch 'xl_class_Base__destroy(&self->base);' | Select-Object -First 1).LineNumber; if(-not $body -or -not $base -or $body -ge $base){exit 1}"
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c should run child destroy body before base destroy tail call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_inherit_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_custom_field.xl --emit release\tmp\cgen_class_custom_field.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Holder__destroy(XL_Class_Holder* self);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing auto destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_NativeBox__create(&self->first, 10LL);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing first custom field create.
    exit /b 1
)
findstr /C:"xl_class_NativeBox__destroy(&self->second);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing second custom field cleanup.
    exit /b 1
)
findstr /C:"xl_class_NativeBox__destroy(&self->first);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing first custom field cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_class_custom_field.c'; $second=($lines | Select-String -SimpleMatch 'xl_class_NativeBox__destroy(&self->second);' | Select-Object -First 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'xl_class_NativeBox__destroy(&self->first);' | Select-Object -First 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_class_custom_field.c should destroy custom fields in reverse declaration order.
    exit /b 1
)
findstr /C:"xl_class_Holder__destroy(&holder);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing holder scope cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_custom_field.c test\cases\cgen_class_custom_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_custom_field.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_custom_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_field_method.xl --emit release\tmp\cgen_custom_field_method.c
if errorlevel 1 exit /b 1
findstr /C:"long long __xl6_return_0 = xl_class_NativeBox__value(&self->box);" release\tmp\cgen_custom_field_method.c >nul
if errorlevel 1 (
    echo cgen_custom_field_method.c missing native-backed class field method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_field_method.c test\cases\cgen_class_custom_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_field_method.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_field_method.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_custom_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_custom_overloads.xl --emit release\tmp\cgen_custom_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_NativeScore__add__int(&score, 5LL)" release\tmp\cgen_custom_overloads.c >nul
if errorlevel 1 (
    echo cgen_custom_overloads.c missing int custom overload call.
    exit /b 1
)
findstr /C:"xl_class_NativeScore__add__string(&score" release\tmp\cgen_custom_overloads.c >nul
if errorlevel 1 (
    echo cgen_custom_overloads.c missing string custom overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_overloads.c test\cases\cgen_custom_overloads_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overloads.xl --emit release\tmp\cgen_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_fn_pick__int(long long value);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing int overload symbol.
    exit /b 1
)
findstr /C:"const char* xl_fn_pick__string(const char* value);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing string overload symbol.
    exit /b 1
)
findstr /C:"long long chosen = xl_fn_pick__int(41LL);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing resolved overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_costs.xl --emit release\tmp\cgen_overload_costs.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overload_costs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overload_costs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overload_costs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_priority.xl --emit release\tmp\cgen_generic_overload_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(choose(1LL), 10LL);" release\tmp\cgen_generic_overload_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_priority.c should prefer exact non-generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_overload_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_var_priority.xl --emit release\tmp\cgen_generic_overload_var_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_gen_choose__int(1LL), 20LL);" release\tmp\cgen_generic_overload_var_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_var_priority.c should prefer generic overload over var boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_overload_var_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_explicit_overload.xl --emit release\tmp\cgen_generic_explicit_overload.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_pick__int__sig__int(long long value);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing scalar explicit generic overload instance.
    exit /b 1
)
findstr /C:"long long xl_gen_pick__int__sig__array_of_int(xvalue values);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing array explicit generic overload instance.
    exit /b 1
)
findstr /C:"long long arrayValue = xl_gen_pick__int__sig__array_of_int(" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose array explicit generic overload.
    exit /b 1
)
findstr /C:"long long scalarValue = xl_gen_pick__int__sig__int(1LL);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose scalar explicit generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_explicit_overload.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_signature_suffix.xl --emit release\tmp\cgen_generic_signature_suffix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_generic_signature_suffix.c'; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\);', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\)\s*\{', 'Multiline')).Count -ne 1){exit 3}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\)\s*\{', 'Multiline')).Count -ne 1){exit 4}; if($t -notmatch 'long long first = xl_gen_order__int__string__sig__int__string\(1LL, \"x\"\);'){exit 5}; if($t -notmatch 'long long second = xl_gen_order__int__string__sig__string__int\(\"x\", 1LL\);'){exit 6}"
if errorlevel 1 (
    echo cgen_generic_signature_suffix.c missing distinct generic signature suffix instances.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_signature_suffix.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_function.xl --emit release\tmp\cgen_generic_function.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic instance prototype.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic instance prototype.
    exit /b 1
)
findstr /C:"long long number = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic call.
    exit /b 1
)
findstr /C:"const char* text = xl_gen_identity__string(\"ok\");" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_function.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_function.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_function.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_dedupe.xl --emit release\tmp\cgen_generic_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_generic_dedupe.c' -Raw; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\)\s*\{', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, 'xl_gen_identity__int\(')).Count -ne 5){exit 3}"
if errorlevel 1 (
    echo cgen_generic_dedupe.c duplicated or lost generic function instance.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_infer.xl --emit release\tmp\cgen_generic_infer.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int identity instance.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string identity instance.
    exit /b 1
)
findstr /C:"long long xl_gen_choose__int(long long left, long long right);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countArray__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred array generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptList__string(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred list generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptSet__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred set generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countDict__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred dict generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countNested__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred nested generic instance.
    exit /b 1
)
findstr /C:"xvalue xl_gen_identity__array_of_dict_of_int(xvalue value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_apply__int(XL6_Function op, long long value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred function-param generic instance.
    exit /b 1
)
findstr /C:"long long number = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int call.
    exit /b 1
)
findstr /C:"const char* text = xl_gen_identity__string(\"ok\");" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string call.
    exit /b 1
)
findstr /C:"long long chosen = xl_gen_choose__int(1LL, 2LL);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose call.
    exit /b 1
)
findstr /C:"long long copied = xl_gen_identity__int(seed);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred call from auto variable.
    exit /b 1
)
findstr /C:"xvalue explicitNested = xl6_xrt_bind_type(xl_gen_identity__array_of_dict_of_int(" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_infer.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_infer.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_infer.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_alias.xl --emit release\tmp\cgen_generic_alias.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical int generic instance.
    exit /b 1
)
findstr /C:"xl_gen_identity__MyInt" release\tmp\cgen_generic_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_alias.c should not mangle transparent alias into generic symbol.
    exit /b 1
)
findstr /C:"long long explicitAlias = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical explicit alias call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_alias.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_alias.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_alias.xl --emit release\tmp\cgen_generic_class_alias.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing canonical generic class layout.
    exit /b 1
)
findstr /C:"XL_Class_Box__MyInt" release\tmp\cgen_generic_class_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_alias.c should not mangle transparent alias into class symbol.
    exit /b 1
)
findstr /C:"XL_Class_Box__int aliasBox = {0};" release\tmp\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing canonical alias variable type.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class.xl --emit release\tmp\cgen_generic_class.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int class instance.
    exit /b 1
)
findstr /C:"long long value;" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int field substitution.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Box__string {" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string class instance.
    exit /b 1
)
findstr /C:"const char* value;" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string field substitution.
    exit /b 1
)
findstr /C:"XL_Class_Box__int intBox = {0};" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int variable declaration.
    exit /b 1
)
findstr /C:"XL_Class_Box__string stringBox = {0};" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string variable declaration.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_fields.xl --emit release\tmp\cgen_generic_class_fields.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing int class instance.
    exit /b 1
)
findstr /C:"intBox.value = 123LL;" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field assignment.
    exit /b 1
)
findstr /C:"expect_i64(intBox.value, 123LL);" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field read.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_function_fields.xl --emit release\tmp\cgen_generic_class_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_copy_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type copy helper.
    exit /b 1
)
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type desc helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_box_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class box helper.
    exit /b 1
)
findstr /C:"xl6_function_call_var(copied.op" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic member function call lowering.
    exit /b 1
)
findstr /C:"xl6_function_call_var(item.op" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic iteration member function call lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" release\tmp\cgen_generic_class_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_function_fields.c should not use GNU statement expressions for generic member function calls.
    exit /b 1
)
findstr /C:"fromArray.op.retain(fromArray.op.env);" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed index retain.
    exit /b 1
)
findstr /C:"item.op.retain(item.op.env);" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_nested_function_fields.xl --emit release\tmp\cgen_generic_class_nested_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"xl_class_Mapper__destroy" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not call unresolved Mapper destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Mapper__int {" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested Mapper int instance.
    exit /b 1
)
findstr /C:"XL_Class_Mapper__int inner;" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing substituted nested class field.
    exit /b 1
)
findstr /C:"xl_class_Mapper__int__destroy(&self->inner);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing generic nested destroy call.
    exit /b 1
)
findstr /C:"fromArray.inner.op.retain(fromArray.inner.op.env);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed index retain.
    exit /b 1
)
findstr /C:"item.inner.op.retain(item.inner.op.env);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_nested_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_deep_function_fields.xl --emit release\tmp\cgen_generic_class_deep_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"XL_Class_Middle__T" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Middle generic type.
    exit /b 1
)
findstr /C:"xl_class_Middle__destroy" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not call unresolved Middle destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Middle__int {" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing nested Middle int instance.
    exit /b 1
)
findstr /C:"XL_Class_Mapper__int mapper;" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Mapper field.
    exit /b 1
)
findstr /C:"XL_Class_Middle__int middle;" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Middle field.
    exit /b 1
)
findstr /C:"xl_class_Middle__int__destroy(&self->middle);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Middle destroy call.
    exit /b 1
)
findstr /C:"xl_class_Mapper__int__destroy(&self->mapper);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Mapper destroy call.
    exit /b 1
)
findstr /C:"fromArray.middle.mapper.op.retain(fromArray.middle.mapper.op.env);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed index retain.
    exit /b 1
)
findstr /C:"item.middle.mapper.op.retain(item.middle.mapper.op.env);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_deep_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_methods.xl --emit release\tmp\cgen_generic_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__valueOf(XL_Class_Box__int* self);" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method prototype.
    exit /b 1
)
findstr /C:"long long __xl6_return_0 = self->value;" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method body.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_auto.xl --emit release\tmp\cgen_generic_class_auto.c
if errorlevel 1 exit /b 1
findstr /C:"long long fromField = intBox.value;" release\tmp\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic field auto inference.
    exit /b 1
)
findstr /C:"long long fromMethod = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic method return auto inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_auto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_overloads.xl --emit release\tmp\cgen_generic_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__pick__int(XL_Class_Box__int* self, long long value);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Box__int__pick__string(XL_Class_Box__int* self, const char* text);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload prototype.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__int(&intBox, 1LL), 10LL);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__string(&intBox, \"x\"), 20LL);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_ctor.xl --emit release\tmp\cgen_generic_class_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Box__int__create(XL_Class_Box__int* self, long long value);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create prototype.
    exit /b 1
)
findstr /C:"self->value = value;" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create body.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&intBox, 123LL);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class method call after create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_iter_protocol.xl --emit release\tmp\cgen_generic_class_iter_protocol.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_PairIter__int {" release\tmp\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic iterator class dependency.
    exit /b 1
)
findstr /C:"XL_Class_PairIter__T" release\tmp\cgen_generic_class_iter_protocol.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c leaked unresolved iterator generic type.
    exit /b 1
)
findstr /C:"XL_Class_PairIter__int xl_class_PairBox__int______iter____" release\tmp\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic __iter__ return type.
    exit /b 1
)
findstr /C:"xl_class_PairIter__int______next____(&__xl6_class_iter_" release\tmp\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic __next__ call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_iter_protocol.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_iter_protocol.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_iter_protocol.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_explicit_ctor.xl --emit release\tmp\cgen_generic_class_explicit_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Box__int intBox = {0};" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class variable declaration.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&intBox, 123LL);" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing method call after explicit create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_explicit_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_type_values.xl --emit release\tmp\cgen_generic_class_type_values.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned long long leftType = 0x" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing generic class type value.
    exit /b 1
)
findstr /C:"expect_i64((leftType == rightType), 1LL);" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing equal generic class type comparison.
    exit /b 1
)
findstr /C:"expect_i64((leftType == textType), 0LL);" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing distinct generic class type comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_basic.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_basic.xl -o release\tmp\xl6_build_basic.exe
if errorlevel 1 exit /b 1
release\tmp\xl6_build_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_range.xl -o release\tmp\xl6_build_range.exe
if errorlevel 1 exit /b 1
release\tmp\xl6_build_range.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_main_entry.xl -o release\tmp\cgen_main_entry.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_main_entry.exe alpha beta
if not errorlevel 3 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)
if errorlevel 4 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)

release\xl.exe run test\cases\xl_run_args.xl -- alpha "two words" "quote\"value" beta ""
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_main_entry.xl --emit release\tmp\cgen_main_entry.c
if errorlevel 1 exit /b 1
findstr /C:"static xvalue xl6_argv_to_arg(int argc, char** argv)" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv-to-arg helper.
    exit /b 1
)
findstr /C:"XL_InitOptions __xl6_options = xlInitOptions(argc, argv);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlInitOptions call.
    exit /b 1
)
findstr /C:"__xl6_arg = xl6_argv_to_arg(argc, argv);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv-to-arg call.
    exit /b 1
)
findstr /C:"__xl6_code = xlang_main(__xl6_arg);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlang main dispatch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_main_entry.c' -Raw; $s=$t.IndexOf('int xlang_main(xvalue arg)'); $e=$t.IndexOf('int main(int argc, char** argv)', $s); if($s -lt 0 -or $e -lt 0){exit 1}; $body=$t.Substring($s, $e - $s); if($body -match 'xvoUnref\(arg\)'){exit 2}"
if errorlevel 1 (
    echo cgen_main_entry.c should borrow xlang_main arg.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_arg);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv arg release.
    exit /b 1
)
findstr /C:"xlUnit();" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlUnit call.
    exit /b 1
)
release\xl.exe xapi test\cases\cgen_main_entry.xl --emit release\tmp\cgen_main_entry.xapi.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_entry.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.kind -ne 'xlang-main'){exit 1}; if($j.entry.cSymbol -ne 'xlang_main'){exit 2}; if($j.entry.returnType -ne 'int'){exit 3}"
if errorlevel 1 (
    echo cgen_main_entry.xapi.json missing xlang main entry metadata.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_main_uncaught.xl --emit release\tmp\cgen_main_uncaught.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_print_uncaught();" release\tmp\cgen_main_uncaught.c >nul
if errorlevel 1 (
    echo cgen_main_uncaught.c missing uncaught main error printer.
    exit /b 1
)
release\xl.exe run test\cases\cgen_main_uncaught.xl >nul 2> release\tmp\cgen_main_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_main_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: main boom" release\tmp\cgen_main_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_main_uncaught.err.txt missing uncaught main exception diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_main_case_sensitive.xl --emit release\tmp\cgen_main_case_sensitive.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_main_case_sensitive.c' -Raw; if($t -match 'xlang_main'){exit 1}; if($t -match 'int main\('){exit 2}; if($t -notmatch 'long long Main\(xvalue arg\)'){exit 3}"
if errorlevel 1 (
    echo cgen_main_case_sensitive.c should treat Main as a normal function.
    exit /b 1
)
release\xl.exe xapi test\cases\cgen_main_case_sensitive.xl --emit release\tmp\cgen_main_case_sensitive.xapi.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_case_sensitive.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.kind -ne 'none'){exit 1}"
if errorlevel 1 (
    echo cgen_main_case_sensitive.xapi.json should report entry none.
    exit /b 1
)
release\xl.exe check test\cases\cgen_main_case_sensitive_script.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_main_case_sensitive_script.xl --emit release\tmp\cgen_main_case_sensitive_script.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_main_case_sensitive_script.c' -Raw; if($t -notmatch 'int xlang_main\(xvalue arg\)'){exit 1}; if($t -notmatch 'int main\('){exit 2}; if($t -notmatch 'long long Main\(xvalue arg\)'){exit 3}; if($t -notmatch 'expect_i64\(1LL, 1LL\);'){exit 4}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.c should treat Main as normal function and top-level code as script main.
    exit /b 1
)
release\xl.exe xapi test\cases\cgen_main_case_sensitive_script.xl --emit release\tmp\cgen_main_case_sensitive_script.xapi.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_case_sensitive_script.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.kind -ne 'script-main'){exit 1}; if($j.entry.returnType -ne 'int'){exit 2}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.xapi.json should report script-main.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_case_sensitive_script.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.cSymbol -ne 'xlang_main'){exit 1}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.xapi.json missing xlang_main cSymbol.
    exit /b 1
)
release\xl.exe build test\cases\cgen_main_case_sensitive_script.xl test\cases\cgen_class_host.c -o release\tmp\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c -o release\tmp\build_mixed_host.exe
if errorlevel 1 exit /b 1
release\tmp\build_mixed_host.exe
if errorlevel 1 exit /b 1

release\xl.exe help > release\tmp\xl_help.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\xl_help.txt' -SimpleMatch 'xl build')){exit 1}"
if errorlevel 1 (
    echo xl_help.txt missing build usage.
    exit /b 1
)

release\xl.exe xapi test\cases\export_add.xl > release\tmp\xapi_stdout.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\xapi_stdout.json' -SimpleMatch 'xlang-xapi-1')){exit 1}"
if errorlevel 1 (
    echo xapi stdout missing format marker.
    exit /b 1
)

release\xl.exe build test\cases\export_add.xl >nul 2> release\tmp\build_missing_output.err.txt
if not errorlevel 1 (
    echo build without -o should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\build_missing_output.err.txt' -SimpleMatch 'build requires -o')){exit 1}"
if errorlevel 1 (
    echo build missing -o diagnostic missing.
    exit /b 1
)

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c -o release\tmp\build_mixed_two_c.exe
if errorlevel 1 exit /b 1
release\tmp\build_mixed_two_c.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$argsList=@('build','release\examples\feature_suite\09_c_export_abi.xl'); for($i=0;$i -lt 33;$i++){ $argsList += 'test\cases\build_mixed_extra.c' }; $argsList += @('-o','release\tmp\too_many_c.exe'); $p=Start-Process -FilePath 'release\xl.exe' -ArgumentList $argsList -NoNewWindow -PassThru -Wait -RedirectStandardOutput 'release\tmp\build_too_many_c.out.txt' -RedirectStandardError 'release\tmp\build_too_many_c.err.txt'; if($p.ExitCode -eq 0){exit 1}; if(-not (Select-String -Path 'release\tmp\build_too_many_c.err.txt' -SimpleMatch 'too many C source files')){exit 2}"
if errorlevel 1 (
    echo build too many C source diagnostic missing.
    exit /b 1
)

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c --save-temps -o release\tmp\build_mixed_save_temps.exe > release\tmp\build_mixed_save_temps.log 2>&1
if errorlevel 1 exit /b 1
release\tmp\build_mixed_save_temps.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $log=Get-Content 'release\tmp\build_mixed_save_temps.log' -Raw; $m=[regex]::Match($log,'temporary files kept:\s*(.+)'); if(-not $m.Success){exit 1}; $dir=$m.Groups[1].Value.Trim(); if(-not (Test-Path -LiteralPath $dir -PathType Container)){exit 2}; foreach($f in @('csrc_0.c','csrc_1.c')){$p=Join-Path $dir $f; if(-not (Test-Path -LiteralPath $p)){exit 3}; $t=Get-Content -LiteralPath $p -Raw; if($t -notmatch 'lib/xrt/xrt\.h'){exit 4}; if($t -notmatch 'module\.h'){exit 5}}; $t0=Get-Content -LiteralPath (Join-Path $dir 'csrc_0.c') -Raw; $t1=Get-Content -LiteralPath (Join-Path $dir 'csrc_1.c') -Raw; if($t0 -notmatch 'build_mixed_host\.c'){exit 6}; if($t1 -notmatch 'build_mixed_extra\.c'){exit 7}; Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction Stop"
if errorlevel 1 (
    echo build save-temps wrapper check failed.
    exit /b 1
)

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_main_entry.xl -o release\tmp\explicit_xrt_link.exe --link %XL6_XRT_OBJ%
if errorlevel 1 exit /b 1
release\tmp\explicit_xrt_link.exe one
if not errorlevel 2 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)
if errorlevel 3 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_build_*').Count; Set-Content 'release\tmp\build_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_main_entry.xl -shared -o release\tmp\bad_main_shared.dll >nul 2> release\tmp\bad_main_shared.err.txt
if not errorlevel 1 (
    echo shared build with xlang main should fail.
    exit /b 1
)
findstr /C:"shared build cannot contain xlang main" release\tmp\bad_main_shared.err.txt >nul
if errorlevel 1 (
    echo shared build error message missing.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'release\tmp\build_fail_dirs.before'); $after=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_build_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed build should clean temporary work directory.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_run_*').Count; Set-Content 'release\tmp\run_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_time_literal_bad.xl >nul 2> release\tmp\run_bad_script.err.txt
if not errorlevel 1 (
    echo bad script run should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'release\tmp\run_fail_dirs.before'); $after=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_run_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed run should clean temporary work directory.
    exit /b 1
)

"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\cli_link_host.c -o release\tmp\cli_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs release\tmp\libxl6clitest.a release\tmp\cli_link_host.o
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cli_link.xl --lib-path release\tmp --link xl6clitest
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cli_link.xl -o release\tmp\cli_link_build.exe --lib-path release\tmp --link xl6clitest
if errorlevel 1 exit /b 1
release\tmp\cli_link_build.exe
if errorlevel 1 exit /b 1

"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\cli_loadlib_host.c -o release\tmp\cli_loadlib_host.dll
if errorlevel 1 exit /b 1
cmd /c "set XL6_TCC=__missing_tcc__.exe&& release\xl.exe run test\cases\cli_loadlib.xl --loadlib release\tmp\cli_loadlib_host.dll"
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_puts.xl --emit release\tmp\extern_puts.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"puts\" func puts(text: string) as int32" release\tmp\extern_puts.xlir >nul
if errorlevel 1 (
    echo extern_puts.xlir missing extern declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_puts.xl --emit release\tmp\extern_puts.c
if errorlevel 1 exit /b 1
findstr /C:"int32_t puts(const char* text);" release\tmp\extern_puts.c >nul
if errorlevel 1 (
    echo extern_puts.c missing C prototype.
    exit /b 1
)

release\xl.exe run test\cases\extern_puts.xl > release\tmp\extern_puts.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang calls c" release\tmp\extern_puts.out.txt >nul
if errorlevel 1 (
    echo extern_puts.xl did not call C puts.
    exit /b 1
)

release\xl.exe run release\examples\c_hello.xl > release\tmp\example_c_hello.out.txt
if errorlevel 1 exit /b 1
findstr /C:"hello from c" release\tmp\example_c_hello.out.txt >nul
if errorlevel 1 (
    echo release\examples\c_hello.xl did not run.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_bool.xl --emit release\tmp\extern_bool.c
if errorlevel 1 exit /b 1
findstr /C:"bool native_flip_bool(bool value);" release\tmp\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool extern prototype.
    exit /b 1
)
findstr /C:"void expect_bool(bool actual, bool expected);" release\tmp\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool expect prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\extern_bool.c test\cases\extern_bool_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\extern_bool.exe
if errorlevel 1 exit /b 1
release\tmp\extern_bool.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\c_abi_types.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\c_abi_types.xl --emit release\tmp\c_abi_types.c
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_c_echo_size(size_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_size prototype.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_c_echo_ssize(xl6_ssize_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize prototype.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_c_echo_ptrdiff(ptrdiff_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ptrdiff prototype.
    exit /b 1
)
findstr /C:"wchar_t xl6_c_echo_wchar(wchar_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_wchar prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\c_abi_types.c test\cases\c_abi_types_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\c_abi_types.exe
if errorlevel 1 exit /b 1
release\tmp\c_abi_types.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_string_return.xl --emit release\tmp\cgen_extern_string_return.c
if errorlevel 1 exit /b 1
findstr /C:"const char* native_text(void);" release\tmp\cgen_extern_string_return.c >nul
if errorlevel 1 (
    echo cgen_extern_string_return.c missing string return extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_extern_string_return.c test\cases\cgen_extern_string_return_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_var.xl --emit release\tmp\extern_var.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"host_counter\" var counter : int" release\tmp\extern_var.xlir >nul
if errorlevel 1 (
    echo extern_var.xlir missing extern var declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_var.xl --emit release\tmp\extern_var.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t host_counter;" release\tmp\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable declaration.
    exit /b 1
)
findstr /C:"host_counter = (host_counter + 2LL);" release\tmp\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\extern_var.c test\cases\extern_var_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\extern_var.exe
if errorlevel 1 exit /b 1
release\tmp\extern_var.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_add.xl --emit release\tmp\export_add.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_add_i64\" func add_i64(a: int, b: int) as int" release\tmp\export_add.xlir >nul
if errorlevel 1 (
    echo export_add.xlir missing export declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_add.xl --emit release\tmp\export_add.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_add_i64" release\tmp\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing C export symbol.
    exit /b 1
)
findstr /C:"symbol: xl6_export_add__i64" release\tmp\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing internal ABI symbol.
    exit /b 1
)

release\xl.exe cgen test\cases\export_add.xl --emit release\tmp\export_add.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_add__i64(long long a, long long b);" release\tmp\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing internal export prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" release\tmp\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing C export wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_add.c test\cases\export_add_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_add_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_add_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_add.xl --emit release\tmp\export_add_header.c --emit-header release\tmp\export_add.h
if errorlevel 1 exit /b 1
findstr /C:"#ifndef XL6_EXPORT_ADD_H" release\tmp\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing header guard.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" release\tmp\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing C export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_add_header.c test\cases\export_add_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_add_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_add_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class.xl --emit release\tmp\export_class.c --emit-header release\tmp\export_class.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class typedef.
    exit /b 1
)
findstr /C:"XL_Class_Point xl6_point_make(int64_t x, int64_t y);" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class return export declaration.
    exit /b 1
)
findstr /C:"int64_t xl6_point_sum(XL_Class_Point point);" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class parameter export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class.c test\cases\export_class_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_method.xl --emit release\tmp\export_class_method.c --emit-header release\tmp\export_class_method.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_point_move(XL_Class_Point* self, int64_t dx, int64_t dy);" release\tmp\export_class_method.h >nul
if errorlevel 1 (
    echo export_class_method.h missing class method C export declaration.
    exit /b 1
)
findstr /C:"void xl6_point_pair(XL_Class_Point* self, int64_t* out0, int64_t* out1);" release\tmp\export_class_method.h >nul
if errorlevel 1 (
    echo export_class_method.h missing class method multi-return C declaration.
    exit /b 1
)
findstr /C:"return xl_class_Point__move(self, dx, dy);" release\tmp\export_class_method.c >nul
if errorlevel 1 (
    echo export_class_method.c missing class export wrapper call.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class_method.c test\cases\export_class_method_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_method_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_method_header_host.exe
if errorlevel 1 exit /b 1
release\xl.exe xapi test\cases\export_class_method.xl --emit release\tmp\export_class_method.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"class_method\"" release\tmp\export_class_method.xapi.json >nul
if errorlevel 1 (
    echo export_class_method.xapi.json missing class_method export metadata.
    exit /b 1
)
findstr /C:"\"owner\":\"Point\"" release\tmp\export_class_method.xapi.json >nul
if errorlevel 1 (
    echo export_class_method.xapi.json missing class method owner metadata.
    exit /b 1
)
release\xl.exe abi test\cases\export_class_method.xl --emit release\tmp\export_class_method.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.Point.move(int,int) as int" release\tmp\export_class_method.abi.txt >nul
if errorlevel 1 (
    echo export_class_method.abi.txt missing class method ABI signature.
    exit /b 1
)
release\xl.exe abi test\cases\export_class_method.xl --json --emit release\tmp\export_class_method.abi.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"export_class_method\"" release\tmp\export_class_method.abi.json >nul
if errorlevel 1 (
    echo export_class_method.abi.json missing class method export kind.
    exit /b 1
)

release\xl.exe cgen test\cases\export_class_inherit.xl --emit release\tmp\export_class_inherit.c --emit-header release\tmp\export_class_inherit.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Entity" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing base class typedef.
    exit /b 1
)
findstr /C:"XL_Class_Entity base;" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited base field.
    exit /b 1
)
findstr /C:"XL_Class_NamedPoint xl6_named_point_make(int64_t id, int64_t x, int64_t y);" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited class return export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class_inherit.c test\cases\export_class_inherit_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_union_field.xl --emit release\tmp\export_class_union_field.c --emit-header release\tmp\export_class_union_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing dependent union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload payload;" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class union field.
    exit /b 1
)
findstr /C:"XL_Class_Box xl6_box_make(int64_t value);" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class-with-union export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class_union_field.c test\cases\export_class_union_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union_class_field.xl --emit release\tmp\export_union_class_field.c --emit-header release\tmp\export_union_class_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing dependent class typedef.
    exit /b 1
)
findstr /C:"typedef union XL_Union_Payload" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union typedef.
    exit /b 1
)
findstr /C:"XL_Class_Point point;" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union class field.
    exit /b 1
)
findstr /C:"XL_Union_Payload xl6_payload_make(int64_t x, int64_t y);" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union-with-class export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_union_class_field.c test\cases\export_union_class_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\export_add.xl -shared -o release\tmp\xl6_export_add.dll --emit-header release\tmp\xl6_export_add_shared.h
if errorlevel 1 exit /b 1
if not exist release\tmp\xl6_export_add.dll (
    echo xl6_export_add.dll missing after build -shared.
    exit /b 1
)
if not exist release\tmp\xl6_export_add_shared.h (
    echo xl6_export_add_shared.h missing after build -shared.
    exit /b 1
)

release\xl.exe abi test\cases\export_multi_return.xl --emit release\tmp\export_multi_return.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.pair(int) as (int,int)" release\tmp\export_multi_return.abi.txt >nul
if errorlevel 1 (
    echo export_multi_return.abi.txt missing multi-return signature.
    exit /b 1
)

release\xl.exe cgen test\cases\export_multi_return.xl --emit release\tmp\export_multi_return.c --emit-header release\tmp\export_multi_return.h
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\export_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing internal return struct.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\export_multi_return.c' -Raw; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ xl6_export_pair\(long long seed\);'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing multi-return internal prototype.
    exit /b 1
)
findstr /C:"void xl6_pair_i64(int64_t seed, int64_t* out0, int64_t* out1);" release\tmp\export_multi_return.h >nul
if errorlevel 1 (
    echo export_multi_return.h missing out-param wrapper declaration.
    exit /b 1
)
findstr /C:"*out1 = (int64_t)__xl6_return.value1;" release\tmp\export_multi_return.c >nul
if errorlevel 1 (
    echo export_multi_return.c missing out-param assignment.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_multi_return.c test\cases\export_multi_return_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_bool.xl --emit release\tmp\export_bool.c --emit-header release\tmp\export_bool.h
if errorlevel 1 exit /b 1
findstr /C:"int xl6_export_isEven(long long value);" release\tmp\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing internal bool export prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" release\tmp\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing C bool wrapper prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" release\tmp\export_bool.h >nul
if errorlevel 1 (
    echo export_bool.h missing C bool wrapper declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_bool.c test\cases\export_bool_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_bool_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_bool_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_primitive_widths.xl --emit release\tmp\export_primitive_widths.c --emit-header release\tmp\export_primitive_widths.h
if errorlevel 1 exit /b 1
findstr /C:"uint64_t xl6_pick_u64(" release\tmp\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing uint64 return wrapper.
    exit /b 1
)
findstr /C:"int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64, float f32, double f64, bool ok, void* handle" release\tmp\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing primitive width parameter mapping.
    exit /b 1
)
findstr /C:"uint64_t xl6_pick_u64(" release\tmp\export_primitive_widths.c >nul
if errorlevel 1 (
    echo export_primitive_widths.c missing C wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" -c -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_primitive_widths.c -o release\tmp\export_primitive_widths.o
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_c_abi_types.xl --emit release\tmp\export_c_abi_types.c --emit-header release\tmp\export_c_abi_types.h
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_echo_c_size(size_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_size export declaration.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_echo_c_ssize(xl6_ssize_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize export declaration.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_echo_c_ptrdiff(ptrdiff_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ptrdiff export declaration.
    exit /b 1
)
findstr /C:"wchar_t xl6_echo_c_wchar(wchar_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_wchar export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_c_abi_types.c test\cases\export_c_abi_types_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_alias.xl --emit release\tmp\export_alias.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.aliasAdd(int,int) as int" release\tmp\export_alias.abi.txt >nul
if errorlevel 1 (
    echo export_alias.abi.txt missing canonical alias signature.
    exit /b 1
)
release\xl.exe abi test\cases\export_alias.xl --json --emit release\tmp\export_alias.abi.json
if errorlevel 1 exit /b 1
findstr /C:"abiVersion" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing ABI version.
    exit /b 1
)
findstr /C:"fn:app.aliasAdd(int,int) as int" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing canonical alias signature.
    exit /b 1
)
findstr /C:"xl6_alias_add" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing C export symbol.
    exit /b 1
)
release\xl.exe abi test\cases\extern_puts.xl --json --emit release\tmp\extern_puts.abi.json
if errorlevel 1 exit /b 1
findstr /C:"cSymbol" release\tmp\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing extern C symbol.
    exit /b 1
)
findstr /C:"puts" release\tmp\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing puts symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\export_alias.xl --emit release\tmp\export_alias.c --emit-header release\tmp\export_alias.h
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_aliasAdd(long long a, long long b);" release\tmp\export_alias.c >nul
if errorlevel 1 (
    echo export_alias.c missing expanded alias prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_alias_add(int64_t a, int64_t b);" release\tmp\export_alias.h >nul
if errorlevel 1 (
    echo export_alias.h missing expanded alias header declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_alias.c test\cases\export_alias_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_alias_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_alias_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\c_variables.xl
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_vars.xl --emit release\tmp\export_vars.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_counter\" var counter : int = 1" release\tmp\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export var declaration.
    exit /b 1
)
findstr /C:"export \"xl6_version\" const version : int = 7" release\tmp\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export const declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_vars.xl --emit release\tmp\export_vars.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_counter" release\tmp\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export var symbol.
    exit /b 1
)
findstr /C:"kind: const" release\tmp\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export const kind.
    exit /b 1
)

release\xl.exe cgen test\cases\export_vars.xl --emit release\tmp\export_vars.c --emit-header release\tmp\export_vars.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_counter = 1LL;" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var definition.
    exit /b 1
)
findstr /C:"const int64_t xl6_version = 7LL;" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export const definition.
    exit /b 1
)
findstr /C:"long long __xl6_return_0 = (xl6_counter + xl6_version);" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var read inside wrapper target.
    exit /b 1
)
findstr /C:"extern int64_t xl6_counter;" release\tmp\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export var declaration.
    exit /b 1
)
findstr /C:"extern const int64_t xl6_version;" release\tmp\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export const declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_vars.c test\cases\export_vars_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_vars_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_vars_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_call.xl --emit release\tmp\export_call.c
if errorlevel 1 exit /b 1
findstr /C:"long long value = xl6_export_add__i64(1LL, 2LL);" release\tmp\export_call.c >nul
if errorlevel 1 (
    echo export_call.c did not use internal ABI symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_call.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_call.exe
if errorlevel 1 exit /b 1
release\tmp\export_call.exe
if errorlevel 1 exit /b 1

release\xl.exe abi release\examples\c_export.xl --emit release\tmp\example_c_export.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export app_add_i64" release\tmp\example_c_export.abi.txt >nul
if errorlevel 1 (
    echo release\examples\c_export.xl ABI dump missing export.
    exit /b 1
)

release\xl.exe cgen release\examples\class_c_export.xl --emit release\tmp\example_class_c_export.c --emit-header release\tmp\example_class_c_export.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t gauge_add(XL_Class_Gauge* self, int64_t delta);" release\tmp\example_class_c_export.h >nul
if errorlevel 1 (
    echo release\examples\class_c_export.xl header missing class method export.
    exit /b 1
)
release\xl.exe abi release\examples\class_c_export.xl --emit release\tmp\example_class_c_export.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"owner: Gauge" release\tmp\example_class_c_export.abi.txt >nul
if errorlevel 1 (
    echo release\examples\class_c_export.xl ABI dump missing class method owner.
    exit /b 1
)

release\xl.exe build test\cases\cgen_basic.xl >nul 2> release\tmp\build_missing_output.err.txt
if not errorlevel 1 (
    echo build without -o should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_duplicate.xl >nul 2> release\tmp\sem_duplicate.err.txt
if not errorlevel 1 (
    echo sem_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_global_func_value_conflict.xl >nul 2> release\tmp\sem_global_func_value_conflict.err.txt
if not errorlevel 1 (
    echo sem_global_func_value_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_const_assign.xl >nul 2> release\tmp\sem_const_assign.err.txt
if not errorlevel 1 (
    echo sem_const_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_safe_member_assign.xl >nul 2> release\tmp\sem_safe_member_assign.err.txt
if not errorlevel 1 (
    echo sem_safe_member_assign.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_unknown.xl >nul 2> release\tmp\sem_safe_member_unknown.err.txt
if not errorlevel 1 (
    echo sem_safe_member_unknown.xl should fail.
    exit /b 1
)
findstr /C:"safe member key receiver must be var or dynamic dict" release\tmp\sem_safe_member_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_unknown.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_unknown.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_unknown.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_typed_container.xl >nul 2> release\tmp\sem_safe_member_typed_container.err.txt
if not errorlevel 1 (
    echo sem_safe_member_typed_container.xl should fail.
    exit /b 1
)
findstr /C:"safe member length receiver must be var or dynamic container" release\tmp\sem_safe_member_typed_container.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_typed_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_typed_container.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_typed_container.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_non_container.xl >nul 2> release\tmp\sem_safe_member_non_container.err.txt
if not errorlevel 1 (
    echo sem_safe_member_non_container.xl should fail.
    exit /b 1
)
findstr /C:"safe member length receiver must be var or dynamic container" release\tmp\sem_safe_member_non_container.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_non_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_non_container.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_non_container.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_array.xl >nul 2> release\tmp\sem_member_key_array.err.txt
if not errorlevel 1 (
    echo sem_member_key_array.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_array.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_array.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_array.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_array.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_set.xl >nul 2> release\tmp\sem_member_key_set.err.txt
if not errorlevel 1 (
    echo sem_member_key_set.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_set.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_set.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_set.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_set.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_typed_dict.xl >nul 2> release\tmp\sem_member_key_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_member_key_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_typed_dict.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_typed_dict.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_typed_dict.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_length.xl >nul 2> release\tmp\sem_dynamic_member_assign_length.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl should fail.
    exit /b 1
)
findstr /C:"member length is read-only" release\tmp\sem_dynamic_member_assign_length.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_container.xl >nul 2> release\tmp\sem_dynamic_member_assign_container.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_dynamic_member_assign_container.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" release\tmp\sem_dynamic_member_assign_container.err.txt >nul
if not errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl should not report assignment target cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_typed_dict.xl >nul 2> release\tmp\sem_dynamic_member_assign_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_dynamic_member_assign_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" release\tmp\sem_dynamic_member_assign_typed_dict.err.txt >nul
if not errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl should not report assignment target cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_compound.xl >nul 2> release\tmp\sem_dynamic_member_assign_compound.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment only supports =" release\tmp\sem_dynamic_member_assign_compound.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_temporary.xl >nul 2> release\tmp\sem_dynamic_member_assign_temporary.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment receiver must be mutable" release\tmp\sem_dynamic_member_assign_temporary.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_array_key_type.xl >nul 2> release\tmp\sem_index_array_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_array_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_dict_key_type.xl >nul 2> release\tmp\sem_index_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_dict_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_index_dict_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_index_dict_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_container_index_key_type.xl >nul 2> release\tmp\sem_var_container_index_key_type.err.txt
if not errorlevel 1 (
    echo sem_var_container_index_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_outside_loop.xl >nul 2> release\tmp\sem_break_outside_loop.err.txt
if not errorlevel 1 (
    echo sem_break_outside_loop.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_continue_zero.xl >nul 2> release\tmp\sem_continue_zero.err.txt
if not errorlevel 1 (
    echo sem_continue_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_too_deep.xl >nul 2> release\tmp\sem_break_too_deep.err.txt
if not errorlevel 1 (
    echo sem_break_too_deep.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_if_condition_type.xl >nul 2> release\tmp\sem_if_condition_type.err.txt
if not errorlevel 1 (
    echo sem_if_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_while_condition_type.xl >nul 2> release\tmp\sem_while_condition_type.err.txt
if not errorlevel 1 (
    echo sem_while_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_conditional_condition_type.xl >nul 2> release\tmp\sem_conditional_condition_type.err.txt
if not errorlevel 1 (
    echo sem_conditional_condition_type.xl should fail.
    exit /b 1
)


echo DEMO6 suite ok: OOP ABI export and interop
exit /b 0

