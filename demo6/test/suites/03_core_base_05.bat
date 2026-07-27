@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 5/10...

release\xl.exe cgen test\cases\cgen_auto_inference_matrix.xl --emit build\test\cgen_auto_inference_matrix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'build\test\cgen_auto_inference_matrix.c'; $checks=@('long long __xl6_user_global_literalInt;','double __xl6_user_global_literalFloat;','int __xl6_user_global_literalBool;','const char* __xl6_user_global_literalString;','xtarray __xl6_user_global_inferredArray;','xtlist __xl6_user_global_inferredList;','xtdict __xl6_user_global_inferredDict;','typedArrayItem = (*(long long*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(__xl6_user_global_typedArray','typedListItem = xrtCopyStr(xl6_xrt_str_dup((*(const char**)xl6_xrt_typed_require_item(xrtTypedListGet(__xl6_user_global_typedList','typedDictItem = (*(long long*)xl6_xrt_typed_require_item(xrtTypedDictGet(__xl6_user_global_typedDict','missingTypedItem = xl6_xrt_move_to_shared(xl6_xrt_typed_box_or_null(xrtTypeInt(), xl6_xrt_typed_array_get_at(__xl6_user_global_typedArray','inferredArrayItem = (*(long long*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(__xl6_user_global_inferredArray','inferredListItem = xrtCopyStr(xl6_xrt_str_dup((*(const char**)xl6_xrt_typed_require_item(xrtTypedListGet(__xl6_user_global_inferredList','inferredDictItem = (*(long long*)xl6_xrt_typed_require_item(xrtTypedDictGet(__xl6_user_global_inferredDict','xvalue __xl6_user_global_box;','box = xl6_xrt_move_to_shared(xl_fn_makeBox(7LL,','fieldValue = (*((XL_Class_MatrixBox*)xl6_class_data(__xl6_user_global_box','fieldText = xrtCopyStr((*((XL_Class_MatrixBox*)xl6_class_data(__xl6_user_global_box','methodValue = xl_class_MatrixBox__getValue(((XL_Class_MatrixBox*)xl6_class_data(__xl6_user_global_box','fn = xl_fn_makeAdder(10LL);','long long __xl6_user_global_fnResult;','callResult = xl_fn_add(__xl6_user_global_fieldValue, __xl6_user_global_methodValue);','futureValue = xl_fn_doubleAsync(6LL);','long long __xl6_user_global_awaitedValue;','autoCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','sent = xl6_channel_send_value(__xl6_user_global_autoCh','received = xl6_channel_recv_into(__xl6_user_global_autoCh'); foreach($p in $checks){ if($t.IndexOf($p) -lt 0){ Write-Host ('cgen_auto_inference_matrix.c missing '+$p); exit 1 } }"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_auto_inference_matrix.xl test\cases\cgen_class_host.c -o build\test\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1
build\test\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_call_var_arg_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_var_arg_exprs.xl --emit build\test\cgen_call_var_arg_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_call_arg_" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var call argument temporary.
    exit /b 1
)
findstr /C:"xl6_extcall_xl_fn_record(__xl6_call_arg_" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing owned extern call argument handoff.
    exit /b 1
)
findstr /C:"if (value != 0) { xvoUnref(value); }" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing borrowed C ABI wrapper cleanup.
    exit /b 1
)
findstr /C:"xl_fn_sumNamed(xl6_xrt_retain(__xl6_arg_0), xl6_xrt_retain(__xl6_arg_1))" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing dynamic call owned-parameter bridge.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" build\test\cgen_call_var_arg_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument binary lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_nc_" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument null-coalesce temporary.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" build\test\cgen_call_var_arg_exprs.c | findstr /C:"makeValue(7LL);" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var argument conditional branch.
    exit /b 1
)
findstr /C:"xl6_var_call(__xl6_user_global_dynamicSum, 2, __xl6_call_arg_" build\test\cgen_call_var_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var callable positional argument lowering.
    exit /b 1
)
findstr /C:"xl6_var_call_kwargs(__xl6_user_global_dynamicSum, xl6_xrt_dict_varargs(2," build\test\cgen_call_var_arg_exprs.c | findstr /C:"__xl6_call_arg_" >nul
if errorlevel 1 (
    echo cgen_call_var_arg_exprs.c missing var callable named argument lowering.
    exit /b 1
)
findstr /C:"({" build\test\cgen_call_var_arg_exprs.c >nul
if not errorlevel 1 (
    echo cgen_call_var_arg_exprs.c should not emit statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_call_var_arg_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_call_var_arg_exprs.exe
if errorlevel 1 exit /b 1
build\test\cgen_call_var_arg_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\perf_var_recursive_alloc.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\perf_var_recursive_alloc.xl --emit build\test\perf_var_recursive_alloc.c
if errorlevel 1 exit /b 1
findstr /C:"xvoUnref(parent);" build\test\perf_var_recursive_alloc.c >nul
if errorlevel 1 (
    echo perf_var_recursive_alloc.c missing fixed var parameter cleanup.
    exit /b 1
)
release\xl.exe build test\cases\perf_var_recursive_alloc.xl -o build\test\perf_var_recursive_alloc.exe
if errorlevel 1 exit /b 1
build\test\perf_var_recursive_alloc.exe array 18 >nul
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$psi=[Diagnostics.ProcessStartInfo]::new(); $psi.FileName=(Resolve-Path 'build\test\perf_var_recursive_alloc.exe').Path; $psi.Arguments='dict 23'; $psi.UseShellExecute=$false; $psi.RedirectStandardOutput=$true; $p=[Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(10000)){ try{$p.Kill()}catch{}; Write-Host 'recursive var dict benchmark regressed past 10 seconds.'; exit 124 }; $out=$p.StandardOutput.ReadToEnd().Trim(); if($p.ExitCode -ne 0 -or $out -ne '28657'){ Write-Host ('recursive var dict benchmark failed: '+$out); exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_call_scalar_arg_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_scalar_arg_exprs.xl --emit build\test\cgen_call_scalar_arg_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"long long __xl6_call_arg_" build\test\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing scalar call argument temporary.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_i64_release(__xl6_call_arg_" build\test\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing dynamic scalar argument conversion.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" build\test\cgen_call_scalar_arg_exprs.c >nul
if errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c missing call argument replacement.
    exit /b 1
)
findstr /C:"({" build\test\cgen_call_scalar_arg_exprs.c >nul
if not errorlevel 1 (
    echo cgen_call_scalar_arg_exprs.c should not emit statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_call_scalar_arg_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_call_scalar_arg_exprs.exe
if errorlevel 1 exit /b 1
build\test\cgen_call_scalar_arg_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_var_return_exprs.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_return_exprs.xl --emit build\test\cgen_var_return_exprs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_return_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing xvalue return temporary.
    exit /b 1
)
findstr /C:"int __xl6_return_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing scalar return temporary.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return binary lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_nested_value_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing nested xvalue statement temporary.
    exit /b 1
)
findstr /C:"xvalue __xl6_return_value_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return xvalue field temporary.
    exit /b 1
)
findstr /C:"value0 = __xl6_return_value_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return first field handoff.
    exit /b 1
)
findstr /C:"value1 = __xl6_return_value_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return second field handoff.
    exit /b 1
)
findstr /C:"__xl6_return_value_" build\test\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return dynamic binary lowering.
    exit /b 1
)
findstr /C:"__xl6_return_value_" build\test\cgen_var_return_exprs.c | findstr /C:"makeValue(14LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return conditional branch lowering.
    exit /b 1
)
findstr /C:"__xl6_return_value_" build\test\cgen_var_return_exprs.c | findstr /C:"makeValue(15LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return null-coalesce fallback lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar field temporary.
    exit /b 1
)
findstr /C:"value0 = __xl6_return_field_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return first scalar field handoff.
    exit /b 1
)
findstr /C:"value1 = __xl6_return_field_" build\test\cgen_var_return_exprs.c >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return second scalar field handoff.
    exit /b 1
)
findstr /C:"__xl6_return_field_" build\test\cgen_var_return_exprs.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar compare lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" build\test\cgen_var_return_exprs.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar approximate lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" build\test\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar logical lowering.
    exit /b 1
)
findstr /C:"__xl6_return_field_" build\test\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_to_i64_release(__xl6_call_arg_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing multi-return scalar conversion lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return compare lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return approximate lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return logical lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"makeValue(40LL);" >nul
if errorlevel 1 (
    echo cgen_var_return_exprs.c missing statement-level return conditional branch.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_var_return_exprs.c | findstr /C:"({" >nul
if not errorlevel 1 (
    echo cgen_var_return_exprs.c should not emit return statement expressions.
    exit /b 1
)
findstr /C:"({" build\test\cgen_var_return_exprs.c >nul
if not errorlevel 1 (
    echo cgen_var_return_exprs.c should not emit any statement expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_var_return_exprs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_var_return_exprs.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_return_exprs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_expr_statement_discard.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_expr_statement_discard.xl --emit build\test\cgen_expr_statement_discard.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing xvalue expression statement temporary.
    exit /b 1
)
findstr /C:"int __xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing scalar expression statement temporary.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing discarded xvalue expression cleanup.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level binary discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level compare discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level approximate discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level logical discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"xl6_xrt_array_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_expr_statement_discard.c missing statement-level membership discard lowering.
    exit /b 1
)
findstr /C:"__xl6_expr_stmt_" build\test\cgen_expr_statement_discard.c | findstr /C:"({" >nul
if not errorlevel 1 (
    echo cgen_expr_statement_discard.c should not emit expression statement statement-expressions.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_expr_statement_discard.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_expr_statement_discard.exe
if errorlevel 1 exit /b 1
build\test\cgen_expr_statement_discard.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_array_literal_expr_elements.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_array_literal_expr_elements.xl --emit build\test\cgen_array_literal_expr_elements.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue __xl6_return_" build\test\cgen_array_literal_expr_elements.c | findstr /C:"xvoCreateArray()" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal return creation.
    exit /b 1
)
findstr /C:"xvalue __xl6_global_init_" build\test\cgen_array_literal_expr_elements.c | findstr /C:"xvoCreateArray()" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal declaration creation.
    exit /b 1
)
findstr /C:"xvalue __xl6_expr_stmt_" build\test\cgen_array_literal_expr_elements.c | findstr /C:"xvoCreateArray()" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing statement-level array literal expression statement creation.
    exit /b 1
)
findstr /C:"xvalue __xl6_array_item_" build\test\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal item temporaries.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_return_" build\test\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal return append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_global_init_" build\test\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal declaration append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_global_init_" build\test\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing prefixed array declaration append.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_expr_stmt_" build\test\cgen_array_literal_expr_elements.c >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal expression statement append.
    exit /b 1
)
findstr /C:"__xl6_array_item_" build\test\cgen_array_literal_expr_elements.c | findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" >nul
if errorlevel 1 (
    echo cgen_array_literal_expr_elements.c missing array literal item binary lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3, ({" build\test\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not emit array varargs with statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(2, ({" build\test\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not emit prefixed array varargs with statement expressions.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_bin_left_" build\test\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should pre-evaluate array literal items.
    exit /b 1
)
findstr /C:"runtime_type_desc_" build\test\cgen_array_literal_expr_elements.c >nul
if not errorlevel 1 (
    echo cgen_array_literal_expr_elements.c should not bind dynamic array<var> runtime descriptors.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_array_literal_expr_elements.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_array_literal_expr_elements.exe
if errorlevel 1 exit /b 1
build\test\cgen_array_literal_expr_elements.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_var_values.xl --emit build\test\cgen_var_values.c
if errorlevel 1 exit /b 1
findstr /C:"empty = xvoCreateNull();" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var null initialization.
    exit /b 1
)
findstr /C:"number = xl6_xrt_move_to_shared(xvoCreateInt((long long)(123LL)));" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing int var boxing.
    exit /b 1
)
findstr /C:"moment = xl6_xrt_move_to_shared(xvoCreateTime((long long)(xrtStrToTime(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing time var boxing.
    exit /b 1
)
findstr /C:"xvalue __xl6_assign_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var assignment ownership handoff.
    exit /b 1
)
findstr /C:"xvoCreateTime((long long)(xrtStrToTime(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing datetime vararg boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic var type helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_equal(__xl6_cmp_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value equality helper.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_order(__xl6_cmp_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value ordering helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic helper.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic cleanup.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_sumNumber;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level sumNumber dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"= xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level sumNumber dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_sumNumber = ({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit sumNumber dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_joinedText;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level joinedText dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"= xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level joinedText dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_joinedText = ({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit joinedText dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedPlus;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedPlus dynamic arithmetic declaration.
    exit /b 1
)
findstr /C:"= xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedPlus dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedPlus = ({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedPlus dynamic arithmetic declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_conditionalPick;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalPick declaration.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(__xl6_user_global_lowNumber);" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalPick assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_conditionalPick = ({ xvalue __xl6_cond_result_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit conditionalPick declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_conditionalTemp;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalTemp declaration.
    exit /b 1
)
findstr /C:"makeValue(40LL);" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level conditionalTemp assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_conditionalTemp = ({ xvalue __xl6_cond_result_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit conditionalTemp declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedAuto;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedAuto declaration.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(12LL));" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedAuto assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedAuto = ({ xvalue __xl6_cond_result_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedAuto declaration statement expressions.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedConditional;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedConditional declaration.
    exit /b 1
)
findstr /C:"xvoCreateText((void*)(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level mixedConditional assignment.
    exit /b 1
)
findstr /C:"xvalue __xl6_user_global_mixedConditional = ({ xvalue __xl6_cond_result_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit mixedConditional declaration statement expressions.
    exit /b 1
)
findstr /C:"int __xl6_user_global_compareDecl;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level compareDecl declaration.
    exit /b 1
)
findstr /C:"compareDecl = __xl6_global_init_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level compareDecl assignment.
    exit /b 1
)
findstr /C:"int __xl6_user_global_compareDecl = ({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit compareDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"long long __xl6_user_global_orderDecl;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level orderDecl declaration.
    exit /b 1
)
findstr /C:"orderDecl = __xl6_global_init_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level orderDecl assignment.
    exit /b 1
)
findstr /C:"long long __xl6_user_global_orderDecl = ({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit orderDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int __xl6_user_global_autoCompareDecl;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoCompareDecl declaration.
    exit /b 1
)
findstr /C:"autoCompareDecl = __xl6_global_init_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoCompareDecl assignment.
    exit /b 1
)
findstr /C:"int __xl6_user_global_autoCompareDecl = ({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit autoCompareDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int __xl6_user_global_approxDecl;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level approxDecl declaration.
    exit /b 1
)
findstr /C:"approxDecl = __xl6_global_init_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level approxDecl assignment.
    exit /b 1
)
findstr /C:"int __xl6_user_global_approxDecl = ({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit approxDecl declaration statement expressions.
    exit /b 1
)
findstr /C:"int __xl6_user_global_autoApproxDecl;" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoApproxDecl declaration.
    exit /b 1
)
findstr /C:"autoApproxDecl = __xl6_global_init_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level autoApproxDecl assignment.
    exit /b 1
)
findstr /C:"int __xl6_user_global_autoApproxDecl = ({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit autoApproxDecl declaration statement expressions.
    exit /b 1
)
findstr /R /C:"__xl6_assign_[0-9][0-9]* = xl6_xrt_basic_binary(__xl6_bin_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignBinary dynamic arithmetic assignment.
    exit /b 1
)
findstr /C:"assignBinary = __xl6_assign_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing assignBinary target handoff.
    exit /b 1
)
findstr /R /C:"xvalue __xl6_assign_[0-9][0-9]* = ({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignBinary statement expression initializer.
    exit /b 1
)
findstr /C:"__xl6_assign_" build\test\cgen_var_values.c | findstr /C:"makeValue(60LL);" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignConditional false branch.
    exit /b 1
)
findstr /C:"assignConditional = __xl6_assign_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing assignConditional target handoff.
    exit /b 1
)
findstr /R /C:"xvalue __xl6_assign_[0-9][0-9]* = ({ xvalue __xl6_cond_result_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignConditional statement expression initializer.
    exit /b 1
)
findstr /C:"assignCompare = (int)(__xl6_cmp_result_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignCompare assignment.
    exit /b 1
)
findstr /C:"assignCompare = ({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignCompare statement expressions.
    exit /b 1
)
findstr /C:"assignOrder = (long long)(__xl6_cmp_result_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignOrder assignment.
    exit /b 1
)
findstr /C:"assignOrder = ({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignOrder statement expressions.
    exit /b 1
)
findstr /C:"assignApprox = (int)(__xl6_approx_result_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level assignApprox assignment.
    exit /b 1
)
findstr /C:"assignApprox = ({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit assignApprox statement expressions.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_unary_release(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic unary release helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_truthy_release(" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic truthy release helper.
    exit /b 1
)
findstr /C:"__xl6_unary_value_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic unary statement temporaries.
    exit /b 1
)
findstr /C:"__xl6_not_value_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic not statement temporaries.
    exit /b 1
)
findstr /C:"__xl6_truth_value_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic condition truthy statement temporaries.
    exit /b 1
)
findstr /C:"xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical temporaries.
    exit /b 1
)
findstr /C:"__xl6_logic_right_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical right operand path.
    exit /b 1
)
findstr /C:"xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_approx(__xl6_approx_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate helper.
    exit /b 1
)
findstr /C:"int __xl6_if_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic if condition temporaries.
    exit /b 1
)
findstr /C:"if (__xl6_if_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if condition handoff.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" build\test\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_if_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic if approximate condition lowering.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in if conditions.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in if conditions.
    exit /b 1
)
findstr /C:"if (({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in if conditions.
    exit /b 1
)
findstr /C:"int __xl6_elseif_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic elseif condition temporaries.
    exit /b 1
)
findstr /C:"if (__xl6_elseif_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif condition handoff.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" build\test\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_elseif_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic elseif approximate condition lowering.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"else if (({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in elseif conditions.
    exit /b 1
)
findstr /C:"int __xl6_while_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic while condition temporaries.
    exit /b 1
)
findstr /C:"if (!__xl6_while_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while condition break guard.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" build\test\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_while_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic while approximate condition lowering.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in while conditions.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in while conditions.
    exit /b 1
)
findstr /C:"while (({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in while conditions.
    exit /b 1
)
findstr /C:"int __xl6_do_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic do while condition temporaries.
    exit /b 1
)
findstr /C:"if (!__xl6_do_cond_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while condition break guard.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_cmp_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while compare condition lowering.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" build\test\cgen_var_values.c | findstr /C:"xl6_xrt_value_truthy(__xl6_logic_right_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while logical condition lowering.
    exit /b 1
)
findstr /C:"__xl6_do_cond_" build\test\cgen_var_values.c | findstr /C:"__xl6_approx_result_" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic do while approximate condition lowering.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"} while (({ xvalue __xl6_approx_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic approximate statement expressions in do while conditions.
    exit /b 1
)
findstr /C:"long long __xl6_call_arg_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level scalar call argument temporaries.
    exit /b 1
)
findstr /C:"__xl6_call_arg_" build\test\cgen_var_values.c | findstr /C:"makeValue(40LL);" >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level dynamic conditional call argument path.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing scalar call argument handoff.
    exit /b 1
)
findstr /C:"const char* __xl6_call_str_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing statement-level string call argument temporaries.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_cmp_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic compare statement expressions as direct call arguments.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_logic_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic logical statement expressions as direct call arguments.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic arithmetic statement expressions as direct scalar call arguments.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(({ xvalue __xl6_bin_left_" build\test\cgen_var_values.c >nul
if not errorlevel 1 (
    echo cgen_var_values.c should not emit dynamic arithmetic statement expressions as direct string call arguments.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_cmp_left_" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_user_global_number);" build\test\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_var_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_var_values.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_values.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 5/10
exit /b 0
