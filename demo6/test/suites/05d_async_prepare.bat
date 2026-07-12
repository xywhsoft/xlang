@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async execution and generation...

release\xl.exe check test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async.xl -o build\test\cgen_async.exe
if errorlevel 1 exit /b 1

build\test\cgen_async.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    build\test\cgen_async.exe
    if errorlevel 1 (
        echo cgen_async.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe check test\cases\cgen_async_string_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_async_string_ownership.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_async_param_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_async_param_ownership.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_async_param_ownership.xl -o build\test\cgen_async_param_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_async_param_ownership.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_async_param_ownership.xl --emit build\test\cgen_async_param_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"xtarray values;" build\test\cgen_async_param_ownership.c >nul
if errorlevel 1 (
    echo cgen_async_param_ownership.c missing static typed-container frame storage.
    exit /b 1
)
findstr /C:"__xl6_args->dynamicValue = xl6_xrt_retain(dynamicValue);" build\test\cgen_async_param_ownership.c >nul
if errorlevel 1 (
    echo cgen_async_param_ownership.c missing owned var argument frame copy.
    exit /b 1
)
findstr /C:"__xl6_args->values = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), values, XRT_OBJMODE_SHARED);" build\test\cgen_async_param_ownership.c >nul
if errorlevel 1 (
    echo cgen_async_param_ownership.c missing owned typed-array argument frame copy.
    exit /b 1
)
findstr /C:"__xl6_args->box = xl_class_AsyncParamBox______xl6__copy__value(box, 1);" build\test\cgen_async_param_ownership.c >nul
if errorlevel 1 (
    echo cgen_async_param_ownership.c missing owned class argument frame copy.
    exit /b 1
)
findstr /C:"__xl6_typed_param_owned_values = 1;" build\test\cgen_async_param_ownership.c >nul
if errorlevel 1 (
    echo cgen_async_param_ownership.c missing worker typed-array ownership transfer.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_async.xl --emit build\test\cgen_async.c
if errorlevel 1 exit /b 1

findstr /C:"xTaskRunThread" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing xTaskRunThread.
    exit /b 1
)
findstr /C:"xl6_async_require_future(xl6_task_run_thread_tracked" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async no-arg task start guard.
    exit /b 1
)
findstr /C:"if ( future == 0 ) { (void)xl6_async_active_add(-1); xrtFree(task); }" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing tracked task start rollback.
    exit /b 1
)
findstr /C:"async task args allocation failed" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async args allocation diagnostic.
    exit /b 1
)
findstr /C:"async task start failed" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async task start diagnostic.
    exit /b 1
)

echo DEMO6 suite ok: async execution and generation
exit /b 0
