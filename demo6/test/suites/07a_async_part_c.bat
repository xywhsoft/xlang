@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async task and async-generator part 3/3...

release\xl.exe check test\cases\sem_task_spawn_return_array.xl >nul 2> build\test\sem_task_spawn_return_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_array.xl missing task.spawn array return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_list.xl >nul 2> build\test\sem_task_spawn_return_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_list.xl missing task.spawn list return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_set.xl >nul 2> build\test\sem_task_spawn_return_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_set.xl missing task.spawn set return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_dict.xl >nul 2> build\test\sem_task_spawn_return_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_dict.xl missing task.spawn dict return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_function.xl >nul 2> build\test\sem_task_spawn_return_function.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_function.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_function.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_function.xl missing task.spawn function return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_class.xl >nul 2> build\test\sem_task_spawn_return_class.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_class.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_class.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_class.xl missing task.spawn class return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_channel.xl >nul 2> build\test\sem_task_spawn_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_channel.xl missing task.spawn channel return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_future.xl >nul 2> build\test\sem_task_spawn_return_future.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_future.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_future.xl missing task.spawn future return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_generator.xl >nul 2> build\test\sem_task_spawn_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_generator.xl missing task.spawn generator return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_sync.xl >nul 2> build\test\sem_task_spawn_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_sync.xl missing task.spawn sync return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_thread.xl >nul 2> build\test\sem_task_spawn_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" build\test\sem_task_spawn_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_thread.xl missing task.spawn thread return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_array.xl >nul 2> build\test\sem_task_spawn_arg_typed_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_typed_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_array.xl missing task.spawn typed array argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_list.xl >nul 2> build\test\sem_task_spawn_arg_typed_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_typed_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_list.xl missing task.spawn typed list argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_set.xl >nul 2> build\test\sem_task_spawn_arg_typed_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_typed_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_set.xl missing task.spawn typed set argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_dict.xl >nul 2> build\test\sem_task_spawn_arg_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_dict.xl missing task.spawn typed dict argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_array.xl >nul 2> build\test\sem_task_spawn_arg_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_array.xl missing task.spawn array argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_list.xl >nul 2> build\test\sem_task_spawn_arg_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_list.xl missing task.spawn list argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_set.xl >nul 2> build\test\sem_task_spawn_arg_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_set.xl missing task.spawn set argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_dict.xl >nul 2> build\test\sem_task_spawn_arg_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_dict.xl missing task.spawn dict argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_function.xl >nul 2> build\test\sem_task_spawn_arg_function.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_function.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_function.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_function.xl missing task.spawn function argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_class.xl >nul 2> build\test\sem_task_spawn_arg_class.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_class.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_class.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_class.xl missing task.spawn class argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_channel.xl >nul 2> build\test\sem_task_spawn_arg_channel.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_channel.xl missing task.spawn channel argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_future.xl >nul 2> build\test\sem_task_spawn_arg_future.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_future.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_future.xl missing task.spawn future argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_generator.xl >nul 2> build\test\sem_task_spawn_arg_generator.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_generator.xl missing task.spawn generator argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_sync.xl >nul 2> build\test\sem_task_spawn_arg_sync.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_sync.xl missing task.spawn sync argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_thread.xl >nul 2> build\test\sem_task_spawn_arg_thread.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" build\test\sem_task_spawn_arg_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_thread.xl missing task.spawn thread argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async_generator.xl
if errorlevel 1 (
    echo cgen_async_generator.xl check failed.
    exit /b 1
)

release\xl.exe run test\cases\cgen_async_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async_generator.xl -o build\test\cgen_async_generator.exe
if errorlevel 1 exit /b 1

build\test\cgen_async_generator.exe
if errorlevel 1 exit /b 1

for /L %%I in (1,1,5) do (
    build\test\cgen_async_generator.exe
    if errorlevel 1 (
        echo cgen_async_generator.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_async_generator.xl --emit build\test\cgen_async_generator.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_async_generator.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_async_generator.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"XL6_Generator* xl_fn_stream" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing generator return symbol for async func.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_stream" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_brokenEarly" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator early-error worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_xl_fn_emptyStream" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator empty worker.
    exit /b 1
)
findstr /C:"xl6_future_await_i64_release" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing await lowering inside generator.
    exit /b 1
)
findstr /C:"xl6_generator_next" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await generator iteration.
    exit /b 1
)
findstr /C:"savedStream = xl_fn_stream(30LL);" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator value.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedStream;\s*savedStream = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &value,[\s\S]*?savedTotal = \(savedTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator move-to-iterator lowering.
    exit /b 1
)
findstr /C:"savedBroken = xl_fn_broken();" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator error source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBroken;\s*savedBroken = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""async generator boom""'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator error propagation lowering.
    exit /b 1
)
findstr /C:"savedBrokenEarly = xl_fn_brokenEarly();" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator early-error source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBrokenEarly;\s*savedBrokenEarly = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""async generator boom before yield""'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator early-error propagation lowering.
    exit /b 1
)
findstr /C:"savedEmpty = xl_fn_emptyStream();" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved empty async generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedEmpty;\s*savedEmpty = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedEmptyCount = \(savedEmptyCount \+ 1LL\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved empty async generator iterator cleanup lowering.
    exit /b 1
)
findstr /C:"savedIndexed = xl_fn_indexedStream();" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator indexed source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexed;\s*savedIndexed = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long savedIndex = __xl6_gen_index_\1;[\s\S]*?long long savedValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &savedValue,[\s\S]*?if \(\(savedIndex == 1LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator break cleanup lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexed;\s*savedIndexed = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator second-pass rejection after break.
    exit /b 1
)
findstr /C:"savedIndexedContinue = xl_fn_indexedStream();" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator continue source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexedContinue;\s*savedIndexedContinue = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long savedContinueIndex = __xl6_gen_index_\1;[\s\S]*?long long savedContinueValue = 0;[\s\S]*?if \(\(savedContinueIndex == 1LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?if \(\(savedContinueIndex == 2LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator continue/break lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedContinueAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexedContinue;\s*savedIndexedContinue = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedContinueAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator second-pass rejection after continue/break.
    exit /b 1
)
findstr /C:"XL6_Generator* xl_fn_returnEarlyStream(void)" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing return-early async generator symbol.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_xl_fn_returnEarlyStream[\s\S]*?xl6_generator_yield_copy[\s\S]*?xl_fn_addAsync\(0LL, 5LL\)[\s\S]*?return;[\s\S]*?xl_fn_addAsync\(0LL, 50LL\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing return-early async generator worker lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnEarly = xl_fn_returnEarlyStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarly;\s*savedReturnEarly = 0;[\s\S]*?savedReturnEarlyCount = \(savedReturnEarlyCount \+ 1LL\);[\s\S]*?savedReturnEarlyTotal = \(savedReturnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-early async generator consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnEarlyAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarly;\s*savedReturnEarly = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnEarlyAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-early async generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'returnEarlyCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = xl_fn_returnEarlyStream\(\);[\s\S]*?returnEarlyCount = \(returnEarlyCount \+ 1LL\);[\s\S]*?returnEarlyTotal = \(returnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnEarlyCount == 1LL[\s\S]*?returnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing temporary return-early async generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* xl_fn_returnBeforeYieldStream(void)" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing return-before-yield async generator symbol.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_xl_fn_returnBeforeYieldStream[\s\S]*?\{\s*XL6_Generator\* __xl6_gen = \(XL6_Generator\*\)__xl6_arg;[\s\S]*?xl6_temp_scope_end\(&__xl6_temp_scope_\d+\);\s*return;[\s\S]*?xl_fn_addAsync\(0LL, 60LL\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing return-before-yield async generator worker lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnBeforeYield = xl_fn_returnBeforeYieldStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYield;\s*savedReturnBeforeYield = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedReturnBeforeYieldCount = \(savedReturnBeforeYieldCount \+ 1LL\);[\s\S]*?savedReturnBeforeYieldTotal = \(savedReturnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnBeforeYieldCount == 0LL[\s\S]*?savedReturnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-before-yield async generator empty consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedReturnBeforeYieldAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYield;\s*savedReturnBeforeYield = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnBeforeYieldAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-before-yield async generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'returnBeforeYieldCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = xl_fn_returnBeforeYieldStream\(\);[\s\S]*?returnBeforeYieldCount = \(returnBeforeYieldCount \+ 1LL\);[\s\S]*?returnBeforeYieldTotal = \(returnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnBeforeYieldCount == 0LL[\s\S]*?returnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing temporary return-before-yield async generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* xl_fn_plainStream(void)" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing ordinary generator symbol for for-await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_xl_fn_plainStream[\s\S]*?xl6_generator_yield_copy[\s\S]*?\(long long\)\{7LL\}[\s\S]*?xl6_generator_yield_copy[\s\S]*?\(long long\)\{8LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing ordinary generator worker for for-await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'plainAwaitTotal = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = xl_fn_plainStream\(\);[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?plainAwaitTotal = \(plainAwaitTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?plainAwaitTotal == 15LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator temporary consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedPlainAwait = xl_fn_plainStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwait;\s*savedPlainAwait = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long plainIndex = __xl6_gen_index_\1;[\s\S]*?long long plainValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &plainValue,[\s\S]*?savedPlainAwaitWeighted = \(\(savedPlainAwaitWeighted \+ \(plainIndex \* 100LL\)\) \+ plainValue\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedPlainAwaitWeighted == 115LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await saved ordinary generator index/value lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedPlainAwaitAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwait;\s*savedPlainAwait = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedPlainAwaitAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedPlainAwaitBreak = xl_fn_plainStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwaitBreak;\s*savedPlainAwaitBreak = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long plainBreakIndex = __xl6_gen_index_\1;[\s\S]*?long long plainBreakValue = 0;[\s\S]*?if \(\(plainBreakIndex == 0LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?savedPlainAwaitBreakTotal = \(\(savedPlainAwaitBreakTotal \+ \(plainBreakIndex \* 100LL\)\) \+ plainBreakValue\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;[\s\S]*?__xl6_continue_\1:[\s\S]*?__xl6_gen_index_\1 \+= 1;[\s\S]*?savedPlainAwaitBreakTotal == 108LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator continue/break cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'savedPlainAwaitBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwaitBreak;\s*savedPlainAwaitBreak = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedPlainAwaitBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator break second-pass rejection.
    exit /b 1
)
findstr /C:"iterate null generator" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator null diagnostic.
    exit /b 1
)
findstr /C:"consumedAsync = xl_fn_stream(40LL);" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedAsync;\s*consumedAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?consumedAsyncFirstTotal = \(consumedAsyncFirstTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator first-pass move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedAsync;\s*consumedAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?consumedAsyncAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator second-pass null rejection.
    exit /b 1
)
findstr /C:"missingAsync = 0;" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing null async generator initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = missingAsync;\s*missingAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?missingAsyncState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing null async generator rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_gen_worker_xl_fn_indexedStream[\s\S]*?xl6_future_await_i64_release[\s\S]*?xl6_generator_yield_copy'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing indexed async generator await-yield worker.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_gen_index_([0-9]+) = 0;[\s\S]*?long long asyncIndex = __xl6_gen_index_\1;[\s\S]*?long long asyncValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &asyncValue'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator two-target index/value lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async_generator.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(\(asyncIndex == 2LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_([0-9]+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator break cleanup.
    exit /b 1
)
findstr /C:"async generator boom before yield" build\test\cgen_async_generator.assert.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator early-error diagnostic.
    exit /b 1
)


echo DEMO6 suite ok: async task and async-generator part 3/3
exit /b 0
