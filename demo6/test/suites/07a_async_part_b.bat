@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async task and async-generator part 2/3...

release\xl.exe check test\cases\sem_task_cancel_sync.xl >nul 2> build\test\sem_task_cancel_sync.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" build\test\sem_task_cancel_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_sync.xl missing task.cancel sync diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_thread.xl >nul 2> build\test\sem_task_cancel_thread.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" build\test\sem_task_cancel_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_thread.xl missing task.cancel thread diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_named_argument.xl >nul 2> build\test\sem_task_cancel_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_cancel_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_arg_count_none.xl >nul 2> build\test\sem_task_cancel_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_cancel_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_arg_count_none.xl missing task.cancel argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_arg_count_many.xl >nul 2> build\test\sem_task_cancel_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_cancel_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_arg_count_many.xl missing task.cancel too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_arg_count_none.xl >nul 2> build\test\sem_task_all_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_all_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_all_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_arg_count_none.xl missing task.all argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_arg_count_many.xl >nul 2> build\test\sem_task_all_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_all_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_all_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_arg_count_many.xl missing task.all too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_empty.xl >nul 2> build\test\sem_task_all_empty.err.txt
if not errorlevel 1 (
    echo sem_task_all_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_all_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_empty.xl missing task.all diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_non_future.xl >nul 2> build\test\sem_task_all_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_all_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_non_future.xl missing task.all non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_array_non_future.xl >nul 2> build\test\sem_task_all_array_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_array_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_all_array_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_array_non_future.xl missing task.all array non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_mixed.xl >nul 2> build\test\sem_task_all_mixed.err.txt
if not errorlevel 1 (
    echo sem_task_all_mixed.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_all_mixed.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_mixed.xl missing task.all diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_mixed.xl >nul 2> build\test\sem_task_race_mixed.err.txt
if not errorlevel 1 (
    echo sem_task_race_mixed.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_race_mixed.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_mixed.xl missing task.race diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_non_future.xl >nul 2> build\test\sem_task_race_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_race_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_race_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_non_future.xl missing task.race non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_array_non_future.xl >nul 2> build\test\sem_task_race_array_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_race_array_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_race_array_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_array_non_future.xl missing task.race array non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_named_argument.xl >nul 2> build\test\sem_task_all_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_all_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_all_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_var.xl >nul 2> build\test\sem_task_all_var.err.txt
if errorlevel 1 (
    echo sem_task_all_var.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_future.xl >nul 2> build\test\sem_task_all_return_future.err.txt
if errorlevel 1 (
    echo sem_task_all_return_future.xl should pass because future handles are reference counted.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_channel.xl >nul 2> build\test\sem_task_all_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_all_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_channel.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_generator.xl >nul 2> build\test\sem_task_all_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_all_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_generator.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_future.xl >nul 2> build\test\sem_task_all_typed_array_return_future.err.txt
if errorlevel 1 (
    echo sem_task_all_typed_array_return_future.xl should pass because future handles are reference counted.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_channel.xl >nul 2> build\test\sem_task_all_typed_array_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_all_typed_array_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_all_typed_array_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_typed_array_return_channel.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_generator.xl >nul 2> build\test\sem_task_all_typed_array_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_all_typed_array_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_all_typed_array_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_typed_array_return_generator.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_array.xl >nul 2> build\test\sem_task_all_return_typed_array.err.txt
if errorlevel 1 (
    echo sem_task_all_return_typed_array.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_list.xl >nul 2> build\test\sem_task_all_return_typed_list.err.txt
if errorlevel 1 (
    echo sem_task_all_return_typed_list.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_set.xl >nul 2> build\test\sem_task_all_return_typed_set.err.txt
if errorlevel 1 (
    echo sem_task_all_return_typed_set.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_dict.xl >nul 2> build\test\sem_task_all_return_typed_dict.err.txt
if errorlevel 1 (
    echo sem_task_all_return_typed_dict.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_empty.xl >nul 2> build\test\sem_task_race_empty.err.txt
if not errorlevel 1 (
    echo sem_task_race_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" build\test\sem_task_race_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_empty.xl missing task.race diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_var.xl >nul 2> build\test\sem_task_race_var.err.txt
if errorlevel 1 (
    echo sem_task_race_var.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_sync.xl >nul 2> build\test\sem_task_race_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_race_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_sync.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_thread.xl >nul 2> build\test\sem_task_race_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_race_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_thread.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_typed_array_return_sync.xl >nul 2> build\test\sem_task_race_typed_array_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_race_typed_array_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_race_typed_array_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_typed_array_return_sync.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_typed_array_return_thread.xl >nul 2> build\test\sem_task_race_typed_array_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_race_typed_array_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"move-only resource handles cannot be aggregated" build\test\sem_task_race_typed_array_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_typed_array_return_thread.xl missing move-only result diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_array.xl >nul 2> build\test\sem_task_race_return_typed_array.err.txt
if errorlevel 1 (
    echo sem_task_race_return_typed_array.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_list.xl >nul 2> build\test\sem_task_race_return_typed_list.err.txt
if errorlevel 1 (
    echo sem_task_race_return_typed_list.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_set.xl >nul 2> build\test\sem_task_race_return_typed_set.err.txt
if errorlevel 1 (
    echo sem_task_race_return_typed_set.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_dict.xl >nul 2> build\test\sem_task_race_return_typed_dict.err.txt
if errorlevel 1 (
    echo sem_task_race_return_typed_dict.xl should pass.
    exit /b 1
)

release\xl.exe run test\cases\cgen_task_aggregate_type_closure.xl > build\test\cgen_task_aggregate_type_closure.out.txt 2> build\test\cgen_task_aggregate_type_closure.err.txt
if errorlevel 1 (
    type build\test\cgen_task_aggregate_type_closure.err.txt
    echo cgen_task_aggregate_type_closure.xl run failed.
    exit /b 1
)
findstr /C:"task aggregate type closure ok" build\test\cgen_task_aggregate_type_closure.out.txt >nul
if errorlevel 1 (
    echo cgen_task_aggregate_type_closure.xl missing success marker.
    exit /b 1
)

release\xl.exe run test\cases\cgen_async_class_function_field.xl > build\test\cgen_async_class_function_field.out.txt 2> build\test\cgen_async_class_function_field.err.txt
if errorlevel 1 (
    type build\test\cgen_async_class_function_field.err.txt
    echo cgen_async_class_function_field.xl run failed.
    exit /b 1
)
findstr /C:"async class function aggregate ok" build\test\cgen_async_class_function_field.out.txt >nul
if errorlevel 1 (
    echo cgen_async_class_function_field.xl missing success marker.
    exit /b 1
)

release\xl.exe build test\cases\cgen_async_class_function_field.xl -o build\test\cgen_async_class_function_field.exe >nul 2> build\test\cgen_async_class_function_field_build.err.txt
if errorlevel 1 (
    type build\test\cgen_async_class_function_field_build.err.txt
    echo cgen_async_class_function_field.xl build failed.
    exit /b 1
)
build\test\cgen_async_class_function_field.exe > build\test\cgen_async_class_function_field_exe.out.txt 2> build\test\cgen_async_class_function_field_exe.err.txt
if errorlevel 1 (
    type build\test\cgen_async_class_function_field_exe.err.txt
    echo cgen_async_class_function_field.exe failed.
    exit /b 1
)

release\xl.exe build test\cases\cgen_task_aggregate_type_closure.xl -o build\test\cgen_task_aggregate_type_closure.exe >nul 2> build\test\cgen_task_aggregate_type_closure_build.err.txt
if errorlevel 1 (
    type build\test\cgen_task_aggregate_type_closure_build.err.txt
    echo cgen_task_aggregate_type_closure.xl build failed.
    exit /b 1
)
build\test\cgen_task_aggregate_type_closure.exe > build\test\cgen_task_aggregate_type_closure_exe.out.txt 2> build\test\cgen_task_aggregate_type_closure_exe.err.txt
if errorlevel 1 (
    type build\test\cgen_task_aggregate_type_closure_exe.err.txt
    echo cgen_task_aggregate_type_closure.exe failed.
    exit /b 1
)

release\xl.exe run test\cases\cgen_task_shared_closure_refcount.xl > build\test\cgen_task_shared_closure_refcount.out.txt 2> build\test\cgen_task_shared_closure_refcount.err.txt
if errorlevel 1 (
    type build\test\cgen_task_shared_closure_refcount.err.txt
    echo cgen_task_shared_closure_refcount.xl run failed.
    exit /b 1
)
findstr /C:"shared closure refcount ok" build\test\cgen_task_shared_closure_refcount.out.txt >nul
if errorlevel 1 (
    echo cgen_task_shared_closure_refcount.xl missing success marker.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_task_shared_closure_refcount.xl --emit build\test\cgen_task_shared_closure_refcount.c
if errorlevel 1 exit /b 1
findstr /C:"xrtRefRetain(&__env->ref_count)" build\test\cgen_task_shared_closure_refcount.c >nul
if errorlevel 1 (
    echo cgen_task_shared_closure_refcount.c missing atomic closure retain.
    exit /b 1
)
findstr /C:"xrtRefRelease(&__env->ref_count)" build\test\cgen_task_shared_closure_refcount.c >nul
if errorlevel 1 (
    echo cgen_task_shared_closure_refcount.c missing atomic closure release.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_arg_count_none.xl >nul 2> build\test\sem_task_race_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_race_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_race_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_arg_count_none.xl missing task.race argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_arg_count_many.xl >nul 2> build\test\sem_task_race_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_race_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_race_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_arg_count_many.xl missing task.race too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_temp_class_await.xl >nul 2> build\test\sem_task_race_temp_class_await.err.txt
if errorlevel 1 (
    echo sem_task_race_temp_class_await.xl should pass.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_named_argument.xl >nul 2> build\test\sem_task_race_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_race_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_race_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_empty.xl >nul 2> build\test\sem_task_when_any_empty.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.whenAny" build\test\sem_task_when_any_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_empty.xl missing task.whenAny empty diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_non_future.xl >nul 2> build\test\sem_task_when_any_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.whenAny" build\test\sem_task_when_any_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_non_future.xl missing task.whenAny non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_named_argument.xl >nul 2> build\test\sem_task_when_any_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" build\test\sem_task_when_any_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_bare_name.xl >nul 2> build\test\sem_task_spawn_bare_name.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_bare_name.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn first argument" build\test\sem_task_spawn_bare_name.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_bare_name.xl missing task.spawn first argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_first_arg_non_func.xl >nul 2> build\test\sem_task_spawn_first_arg_non_func.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_first_arg_non_func.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn first argument" build\test\sem_task_spawn_first_arg_non_func.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_first_arg_non_func.xl missing task.spawn non-func first argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_named_argument.xl >nul 2> build\test\sem_task_spawn_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn does not accept named arguments" build\test\sem_task_spawn_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_named_argument.xl missing task.spawn named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count_none.xl >nul 2> build\test\sem_task_spawn_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn requires at least a func argument" build\test\sem_task_spawn_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count_none.xl missing task.spawn empty argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count.xl >nul 2> build\test\sem_task_spawn_arg_count.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument count" build\test\sem_task_spawn_arg_count.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count.xl missing task.spawn count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count_many.xl >nul 2> build\test\sem_task_spawn_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument count" build\test\sem_task_spawn_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count_many.xl missing task.spawn too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_type_mismatch.xl >nul 2> build\test\sem_task_spawn_arg_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument type does not match func signature" build\test\sem_task_spawn_arg_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_type_mismatch.xl missing task.spawn argument type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_multi_return.xl >nul 2> build\test\sem_task_spawn_arg_multi_return.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_multi_return.xl should fail.
    exit /b 1
)
powershell -NoProfile -Command "$t=[IO.File]::ReadAllText('build\test\sem_task_spawn_arg_multi_return.err.txt',[Text.Encoding]::UTF8); $e=-join @(0x591A,0x8FD4,0x56DE,0x503C,0x51FD,0x6570,0x8C03,0x7528,0x4E0D,0x80FD,0x4F5C,0x4E3A,0x5355,0x4E2A,0x5B9E,0x53C2 | ForEach-Object {[char]$_}); if(-not $t.Contains($e)){exit 1}"
if errorlevel 1 (
    echo sem_task_spawn_arg_multi_return.xl missing unified multi-return argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_multi_return.xl >nul 2> build\test\sem_task_spawn_multi_return.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_multi_return.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn currently supports only no-return or single-return" build\test\sem_task_spawn_multi_return.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_multi_return.xl missing task.spawn no-return/single-return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_var.xl >nul
if errorlevel 1 (
    echo sem_task_spawn_return_var.xl should pass language checking.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_dict_vararg.xl >nul 2> build\test\sem_task_spawn_dict_vararg.err.txt
if errorlevel 1 (
    echo sem_task_spawn_dict_vararg.xl should pass language checking.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_array.xl >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_array.xl should pass language checking.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_list.xl >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_list.xl should pass language checking.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_set.xl >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_set.xl should pass language checking.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_dict.xl >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_dict.xl should pass language checking.
    exit /b 1
)


echo DEMO6 suite ok: async task and async-generator part 2/3
exit /b 0
