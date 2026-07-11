@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 9/10...

release\xl.exe cgen test\cases\cgen_in_range.xl --emit build\test\cgen_in_range.c
if errorlevel 1 exit /b 1
findstr /C:"static int xl6_range_contains_i64" build\test\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains helper.
    exit /b 1
)
findstr /C:" = xl6_range_contains_i64(3LL, 0, 5LL, 1);" build\test\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains call.
    exit /b 1
)
findstr /C:"expect_i64(__xl6_call_arg_" build\test\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains temporary argument.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_in_range.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_in_range.exe
if errorlevel 1 exit /b 1
build\test\cgen_in_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_in_containers.xl --emit build\test\cgen_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFindStr(text, 0, \"pha\", 0, false) != 0" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing string in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_contains_value_release(values" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing typed array in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_contains_value_release(sparse" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing typed list in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_contains_value_release(unique" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing typed set in lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictExists(table, \"a\", 0)" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing typed dict in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_value_contains(dynArray" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing dynamic var in lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_in_containers.c | findstr /C:"xl6_xrt_array_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level literal array in return lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" build\test\cgen_in_containers.c | findstr /C:"makeDynArray()" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic array in return temp.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_in_containers.c | findstr /C:"xl6_xrt_value_contains(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic container in return lowering.
    exit /b 1
)
findstr /C:"__xl6_return_" build\test\cgen_in_containers.c | findstr /C:"xvoTableExists(__xl6_in_values_" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level literal dict in return lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" build\test\cgen_in_containers.c | findstr /C:"makeDynTable()" >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing statement-level dynamic dict in return temp.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container for in lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_in_values_" build\test\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container release.
    exit /b 1
)
findstr /C:"expect_i64(({ xvalue __xl6_in_values_" build\test\cgen_in_containers.c >nul
if not errorlevel 1 (
    echo cgen_in_containers.c should pre-evaluate in arguments.
    exit /b 1
)
findstr /C:"({ xvalue __xl6_in_values_" build\test\cgen_in_containers.c >nul
if not errorlevel 1 (
    echo cgen_in_containers.c should not use statement expressions for in temporaries.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_in_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_for_in_containers.xl --emit build\test\cgen_for_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArrayItemCount(__xl6_iter_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing array for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_list_snapshot(__xl6_iter_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing list for-in lowering.
    exit /b 1
)
findstr /C:"xvoSetGetValueAt(__xl6_iter_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing set for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_snapshot(__xl6_iter_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing dict for-in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_iter_snapshot_free(__xl6_list_snapshot_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing list snapshot cleanup.
    exit /b 1
)
findstr /C:"xl6_xrt_iter_snapshot_free(__xl6_dict_snapshot_" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing dict snapshot cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'build\test\cgen_for_in_containers.c'; $bad=@('xrtAVLTB_Iter','__xl6_avl_','__xl6_list_data_','__xl6_dict_data_'); foreach($p in $bad){ if($text.IndexOf($p) -ge 0){ Write-Host ('cgen_for_in_containers.c regressed to AVL iterator shape: '+$p); exit 1 } }; $list=[regex]::Matches($text,'XL6_XrtIterSnapshot\* (__xl6_list_snapshot_\d+) = xl6_xrt_list_snapshot\(__xl6_iter_\d+\);[\s\S]*?for \(unsigned int __xl6_list_pos_\d+ = 0;[\s\S]*?XL6_XrtIterItem\* __xl6_list_item_\d+ = &\1->items\[[^\]]+\];[\s\S]*?xl6_xrt_iter_snapshot_free\(\1\);'); $dict=[regex]::Matches($text,'XL6_XrtIterSnapshot\* (__xl6_dict_snapshot_\d+) = xl6_xrt_dict_snapshot\(__xl6_iter_\d+\);[\s\S]*?for \(unsigned int __xl6_dict_pos_\d+ = 0;[\s\S]*?XL6_XrtIterItem\* __xl6_dict_item_\d+ = &\1->items\[[^\]]+\];[\s\S]*?xl6_xrt_iter_snapshot_free\(\1\);'); if ($list.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing list snapshot sequence.'; exit 1 }; if ($dict.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing dict snapshot sequence.'; exit 1 }"
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(xl6_xrt_value_to_string(key), \"b\", 0, false)" build\test\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing var-to-string comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_for_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_for_in_var_non_container.xl --emit build\test\cgen_for_in_var_non_container.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_raise(\"TypeError\", \"for in value must be array, list, set or dict\"" build\test\cgen_for_in_var_non_container.c >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.c missing var for-in TypeError guard.
    exit /b 1
)
findstr /C:"xl6_error_print_uncaught" build\test\cgen_for_in_var_non_container.c >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.c missing uncaught runtime error printer.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_for_in_var_non_container.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_for_in_var_non_container.exe
if errorlevel 1 exit /b 1
build\test\cgen_for_in_var_non_container.exe >nul 2> build\test\cgen_for_in_var_non_container.err.txt
if not errorlevel 1 (
    echo cgen_for_in_var_non_container.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught TypeError: for in value must be array, list, set or dict" build\test\cgen_for_in_var_non_container.err.txt >nul
if errorlevel 1 (
    echo cgen_for_in_var_non_container.err.txt missing uncaught TypeError diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_approx.xl --emit build\test\cgen_approx.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtIntApprox(long long a, long long b);" build\test\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing xrtIntApprox prototype.
    exit /b 1
)
findstr /C:"expect_i64(xrtIntApprox(a, b), 1LL);" build\test\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing int approx call.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(x, y), 1LL);" build\test\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing num approx call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_approx.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_approx.exe
if errorlevel 1 exit /b 1
build\test\cgen_approx.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_math.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_math.xl --emit build\test\stdlib_math.c
if errorlevel 1 exit /b 1
findstr /C:"double sqrt(double x);" build\test\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing sqrt extern prototype.
    exit /b 1
)
findstr /C:"double __xl_std_math_round(double x);" build\test\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(__xl_std_math_round(1.6), 2.0), 1LL);" build\test\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper call.
    exit /b 1
)
findstr /C:"double xrtMathClamp(double value, double minValue, double maxValue);" build\test\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing xrtMathClamp extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_math.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_math.exe
if errorlevel 1 exit /b 1
build\test\stdlib_math.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_math.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_math_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_math_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_math_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_math_c.exe > build\test\stdlib_math_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_math_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_math_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib math xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_string.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_string.xl --emit build\test\stdlib_string.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing strlen c_size extern prototype.
    exit /b 1
)
findstr /C:"int xrtStrComp" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrComp extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_string_compare" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing compare helper.
    exit /b 1
)
findstr /C:"unsigned int xrtInStr" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtInStr extern prototype.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_starts_with" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string startsWith runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_ends_with" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string endsWith runtime helper.
    exit /b 1
)
findstr /C:"void* xrtCheckStr" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtCheckStr extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_string_has_any" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.hasAny helper.
    exit /b 1
)
findstr /C:"double xrtStrSim" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrSim extern prototype.
    exit /b 1
)
findstr /C:"void* xrtIntFormat" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtIntFormat extern prototype.
    exit /b 1
)
findstr /C:"void* xrtNumFormat" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtNumFormat extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_int" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatInt helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_float" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatFloat helper.
    exit /b 1
)
findstr /C:"char** xrtSplit" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtSplit extern prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_string_split" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string split runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_string_split" build\test\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.split helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_string.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_string.exe
if errorlevel 1 exit /b 1
build\test\stdlib_string.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_string.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_string_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_string_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_string_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_string_c.exe > build\test\stdlib_string_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_string_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_string_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib string xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time.xl --emit build\test\stdlib_time.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtDateAdd" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtDateAdd extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_time_from_datetime" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing fromDateTime helper.
    exit /b 1
)
findstr /C:"int64_t xrtStrToTime" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtStrToTime extern prototype.
    exit /b 1
)
findstr /C:"void* xrtTimeFormat" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeFormat extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtTimeParse" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeParse extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTryStrToTime" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTryStrToTime extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTryTimeParse" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTryTimeParse extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtDateDiff" build\test\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtDateDiff extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_time.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_time.exe
if errorlevel 1 exit /b 1
build\test\stdlib_time.exe
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\stdlib_time.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_time_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_time_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_time_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_time_c.exe > build\test\stdlib_time_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_time_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_time_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib time xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_extra.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_extra.xl --emit build\test\stdlib_time_extra.c
if errorlevel 1 exit /b 1
findstr /C:"double xrtTimer" build\test\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimer extern prototype.
    exit /b 1
)
findstr /C:"void xrtSleep" build\test\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtSleep extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTimeRangeOverlap" build\test\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimeRangeOverlap extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRelativeTime" build\test\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtRelativeTime extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_time_extra.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_time_extra.exe
if errorlevel 1 exit /b 1
build\test\stdlib_time_extra.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_calendar.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_calendar.xl --emit build\test\stdlib_time_calendar.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtToUnixTime" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtToUnixTime extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFirstDayOfMonth" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtFirstDayOfMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtWeekOfYear" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtWeekOfYear extern prototype.
    exit /b 1
)
findstr /C:"bool xrtIsLeapYear" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtIsLeapYear extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInMonth" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInYear" build\test\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInYear extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_time_calendar.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1
build\test\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_random.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_random.xl --emit build\test\stdlib_random.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned int xrtRand32" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRand32 extern prototype.
    exit /b 1
)
findstr /C:"int xrtRandRange" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandRange extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandStr" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandStr extern prototype.
    exit /b 1
)
findstr /C:"void xrtRandSeedThread" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandSeedThread extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandCreate" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandCreate extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtRand32Obj" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRand32Obj extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandStrObj" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandStrObj extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_random_string" build\test\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing random.string helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\stdlib_random.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_random.exe
if errorlevel 1 exit /b 1
build\test\stdlib_random.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 9/10
exit /b 0

