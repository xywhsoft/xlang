@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: generator...

release\xl.exe check test\cases\sem_for_await_non_generator.xl >nul 2> build\test\sem_for_await_non_generator.err.txt
if not errorlevel 1 (
    echo sem_for_await_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"for await requires generator<T>" build\test\sem_for_await_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_for_await_non_generator.xl missing for-await diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_type_non_generator.xl >nul 2> build\test\sem_async_type_non_generator.err.txt
if not errorlevel 1 (
    echo sem_async_type_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"async type marker is only supported on generator<T>" build\test\sem_async_type_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_async_type_non_generator.xl missing async type marker diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_generator_return_value.xl >nul 2> build\test\sem_async_generator_return_value.err.txt
if not errorlevel 1 (
    echo sem_async_generator_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" build\test\sem_async_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" build\test\sem_async_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_generator_type_marker_return_value.xl >nul 2> build\test\sem_async_generator_type_marker_return_value.err.txt
if not errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" build\test\sem_async_generator_type_marker_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" build\test\sem_async_generator_type_marker_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_return_value.xl >nul 2> build\test\sem_generator_return_value.err.txt
if not errorlevel 1 (
    echo sem_generator_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" build\test\sem_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_generator_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" build\test\sem_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_generator_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_generator.xl -o build\test\cgen_generator.exe
if errorlevel 1 exit /b 1

build\test\cgen_generator.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\cgen_generator.exe
    if errorlevel 1 (
        echo cgen_generator.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_generator.xl --emit build\test\cgen_generator.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_generator.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_generator.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"xrtCoCreate" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing xrtCoCreate.
    exit /b 1
)
findstr /C:"xl6_generator_yield_copy" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator yield helper.
    exit /b 1
)
findstr /C:"xl6_generator_next" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator next helper.
    exit /b 1
)
findstr /C:"xl6_generator_destroy" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator cleanup helper.
    exit /b 1
)
findstr /C:"xl6_generator_store_active_error" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator stored error helper.
    exit /b 1
)
findstr /C:"xl6_generator_raise_stored_error" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator stored error rethrow helper.
    exit /b 1
)
findstr /C:"xl6_generator_store_active_error(__xl6_gen)" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator worker error capture.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_brokenEarly" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator early-error worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_emptyNums" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing empty generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_toggles" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing bool generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_weights" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing float generator worker.
    exit /b 1
)
findstr /C:"12:34:56" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing time generator literal lowering.
    exit /b 1
)
findstr /C:"generator boom before yield" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator early-error diagnostic.
    exit /b 1
)
findstr /C:"iterate null generator" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator null diagnostic.
    exit /b 1
)
findstr /C:"savedBreakGen = xl_fn_nums(3LL);" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing saved break generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedBreakGen = xl_fn_nums\(3LL\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBreakGen;\s*savedBreakGen = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long index = __xl6_gen_index_\1;[\s\S]*?long long item = 0;[\s\S]*?if \(\(index == 0LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?savedBreakTotal = \(\(savedBreakTotal \+ \(index \* 100LL\)\) \+ item\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;[\s\S]*?__xl6_continue_\1:[\s\S]*?__xl6_gen_index_\1 \+= 1;[\s\S]*?savedBreakTotal == 102LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved generator continue/break cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBreakGen;\s*savedBreakGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved generator break second-pass rejection.
    exit /b 1
)
findstr /C:"consumedGen = xl_fn_nums(1LL);" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedGen;\s*consumedGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?consumedFirstTotal = \(consumedFirstTotal \+ item\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator first-pass move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedGen;\s*consumedGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrCompare\(xrtStrView\(xl6_error_message\(e, 0\)\), xrtStrView\(\x22iterate null generator\x22\)\)[\s\S]*?consumedAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator second-pass null rejection.
    exit /b 1
)
findstr /C:"missingGen = 0;" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing null generator initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = missingGen;\s*missingGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrCompare\(xrtStrView\(xl6_error_message\(e, 0\)\), xrtStrView\(\x22iterate null generator\x22\)\)[\s\S]*?missingGenState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing null generator rejection.
    exit /b 1
)
findstr /C:"XL6_Generator* xl_fn_returnEarlyNums(void)" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing return-early generator function.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_xl_fn_returnEarlyNums[\s\S]*?xl6_generator_yield_copy[\s\S]*?\{5LL\}[\s\S]*?return;[\s\S]*?\{50LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing return-early generator worker shape.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnEarlyNums = xl_fn_returnEarlyNums\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarlyNums;\s*savedReturnEarlyNums = 0;[\s\S]*?savedReturnEarlyCount = \(savedReturnEarlyCount \+ 1LL\);[\s\S]*?savedReturnEarlyTotal = \(savedReturnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnEarlyCount == 1LL[\s\S]*?savedReturnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved return-early generator move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnEarlyAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarlyNums;\s*savedReturnEarlyNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnEarlyAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved return-early generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'returnEarlyCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = xl_fn_returnEarlyNums\(\);[\s\S]*?returnEarlyCount = \(returnEarlyCount \+ 1LL\);[\s\S]*?returnEarlyTotal = \(returnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnEarlyCount == 1LL[\s\S]*?returnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing temporary return-early generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* xl_fn_returnBeforeYieldNums(void)" build\test\cgen_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing pre-yield return generator function.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_xl_fn_returnBeforeYieldNums[\s\S]*?\{\s*XL6_Generator\* __xl6_gen = \(XL6_Generator\*\)__xl6_arg;[\s\S]*?xl6_temp_scope_end\(&__xl6_temp_scope_\d+\);\s*return;[\s\S]*?\{60LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing pre-yield return generator worker shape.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnBeforeYieldNums = xl_fn_returnBeforeYieldNums\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYieldNums;\s*savedReturnBeforeYieldNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedReturnBeforeYieldCount = \(savedReturnBeforeYieldCount \+ 1LL\);[\s\S]*?savedReturnBeforeYieldTotal = \(savedReturnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnBeforeYieldCount == 0LL[\s\S]*?savedReturnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved pre-yield return generator move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnBeforeYieldAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYieldNums;\s*savedReturnBeforeYieldNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnBeforeYieldAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved pre-yield return generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'returnBeforeYieldCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = xl_fn_returnBeforeYieldNums\(\);[\s\S]*?returnBeforeYieldCount = \(returnBeforeYieldCount \+ 1LL\);[\s\S]*?returnBeforeYieldTotal = \(returnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnBeforeYieldCount == 0LL[\s\S]*?returnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing temporary pre-yield return generator lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator_two_targets.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generator_two_targets.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_generator_two_targets.xl -o build\test\cgen_generator_two_targets.exe
if errorlevel 1 exit /b 1

build\test\cgen_generator_two_targets.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    build\test\cgen_generator_two_targets.exe
    if errorlevel 1 (
        echo cgen_generator_two_targets.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_generator_two_targets.xl --emit build\test\cgen_generator_two_targets.c
if errorlevel 1 exit /b 1

findstr /C:"long long __xl6_gen_index_" build\test\cgen_generator_two_targets.c >nul
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index counter.
    exit /b 1
)
findstr /C:"long long index = __xl6_gen_index_" build\test\cgen_generator_two_targets.c >nul
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index target.
    exit /b 1
)
powershell -NoProfile -Command "if (-not (Select-String -Path 'build\test\cgen_generator_two_targets.c' -Pattern '__xl6_gen_index_[0-9]+ \+= 1;' -Quiet)) { exit 1 }"
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index increment.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_outside_generator.xl >nul 2> build\test\sem_yield_outside_generator.err.txt
if not errorlevel 1 (
    echo sem_yield_outside_generator.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_sync_func.xl >nul 2> build\test\sem_yield_sync_func.err.txt
if not errorlevel 1 (
    echo sem_yield_sync_func.xl should fail.
    exit /b 1
)
findstr /C:"yield" build\test\sem_yield_sync_func.err.txt >nul
if errorlevel 1 (
    echo sem_yield_sync_func.xl missing yield outside generator diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_async_non_generator.xl >nul 2> build\test\sem_yield_async_non_generator.err.txt
if not errorlevel 1 (
    echo sem_yield_async_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"yield" build\test\sem_yield_async_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_yield_async_non_generator.xl missing async non-generator yield diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator_string_resource.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_generator_string_resource.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generator_string_resource.xl --emit build\test\cgen_generator_string_resource.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_frame->prefix = xl6_xrt_string_dup(prefix != 0 ? prefix : \"\", 0u);" build\test\cgen_generator_string_resource.c >nul
if errorlevel 1 (
    echo cgen_generator_string_resource.c missing owned generator string frame copy.
    exit /b 1
)
findstr /C:"xl_gen_frame_free_xl_fn_messages" build\test\cgen_generator_string_resource.c >nul
if errorlevel 1 (
    echo cgen_generator_string_resource.c missing generator frame destructor.
    exit /b 1
)
findstr /C:"if ( xrtCoCancel(gen->co) ) { (void)xrtCoResume(gen->co); }" build\test\cgen_generator_string_resource.c >nul
if errorlevel 1 (
    echo cgen_generator_string_resource.c missing suspended generator cancellation.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -DXL6_USE_XRT_LIFECYCLE build\test\cgen_generator_string_resource.c %XL6_CGEN_LINKS% -o build\test\cgen_generator_string_resource.exe
if errorlevel 1 exit /b 1
build\test\cgen_generator_string_resource.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_generator_var_element.xl >nul 2> build\test\sem_generator_var_element.err.txt
if not errorlevel 1 (
    echo sem_generator_var_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" build\test\sem_generator_var_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_element.xl missing generator var element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_typed_array_element.xl >nul 2> build\test\sem_generator_typed_array_element.err.txt
if not errorlevel 1 (
    echo sem_generator_typed_array_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" build\test\sem_generator_typed_array_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_element.xl missing generator typed array element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_class_element.xl >nul 2> build\test\sem_generator_class_element.err.txt
if not errorlevel 1 (
    echo sem_generator_class_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" build\test\sem_generator_class_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_class_element.xl missing generator element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_function_element.xl >nul 2> build\test\sem_generator_function_element.err.txt
if not errorlevel 1 (
    echo sem_generator_function_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" build\test\sem_generator_function_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_function_element.xl missing generator element diagnostic.
    exit /b 1
)

rem Class references now use retained continuation-frame ownership.
release\xl.exe run test\cases\sem_generator_class_param.xl
if errorlevel 1 (
    echo sem_generator_class_param.xl failed retained class-frame execution.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_var_param.xl >nul 2> build\test\sem_generator_var_param.err.txt
if not errorlevel 1 (
    echo sem_generator_var_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" build\test\sem_generator_var_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_param.xl missing generator var parameter diagnostic.
    exit /b 1
)
findstr /C:"frame" build\test\sem_generator_var_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_param.xl missing generator var parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_typed_array_param.xl >nul 2> build\test\sem_generator_typed_array_param.err.txt
if not errorlevel 1 (
    echo sem_generator_typed_array_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" build\test\sem_generator_typed_array_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_param.xl missing generator typed array parameter diagnostic.
    exit /b 1
)
findstr /C:"frame" build\test\sem_generator_typed_array_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_param.xl missing generator typed array parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_function_param.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_generator_function_param.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_generator_yield_type.xl >nul 2> build\test\sem_generator_yield_type.err.txt
if not errorlevel 1 (
    echo sem_generator_yield_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value float" build\test\sem_generator_yield_type.err.txt >nul
if errorlevel 1 (
    echo sem_generator_yield_type.xl missing yield type context.
    exit /b 1
)

echo DEMO6 suite ok: generator
exit /b 0
