@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async generator channel docs f-string negatives...

release\xl.exe check test\cases\cgen_async_class_conditional_await.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async_class_conditional_await.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_async_class_conditional_await.xl --emit release\tmp\cgen_async_class_conditional_await.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_future_await_class_conditionalAwaitBoxAsync_release" release\tmp\cgen_async_class_conditional_await.c >nul
if errorlevel 1 (
    echo cgen_async_class_conditional_await.c missing class await release helper.
    exit /b 1
)
findstr /C:"xl6_future_add_ref_if_needed(missingConditionalAwaitBox)" release\tmp\cgen_async_class_conditional_await.c >nul
if errorlevel 1 (
    echo cgen_async_class_conditional_await.c missing borrowed/null future add-ref guard.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async_exception_propagation.xl -o release\tmp\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

for /L %%I in (1,1,5) do (
    release\tmp\cgen_async_exception_propagation.exe
    if errorlevel 1 (
        echo cgen_async_exception_propagation.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_async_exception_propagation.xl --emit release\tmp\cgen_async_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"async boom\"" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async throw raise.
    exit /b 1
)

findstr /C:"xl6_future_await_value" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing await helper.
    exit /b 1
)

findstr /C:"xl6_future_await_value(failed" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing borrowed named future await.
    exit /b 1
)

findstr /C:"long long secondValue = (*(long long*)xl6_future_await_value(failed" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing repeated await for failed named future.
    exit /b 1
)

findstr /C:"cancelFailed = xl6_task_cancel_value(failed, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed future cancel as borrowed.
    exit /b 1
)

findstr /C:"xl6_future_await_i64_release(failAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing owned temporary future await release.
    exit /b 1
)

findstr /C:"xl6_future_await_value(failedVoid" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed void named future await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedVoid, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed void future cancel as borrowed.
    exit /b 1
)

findstr /C:"xl6_future_await_void_release(failVoidAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed void await release.
    exit /b 1
)

findstr /C:"failedStringValueA = xl6_future_await_cstr_copy(failedString" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed string first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedString, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed string future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedStringValueB = xl6_future_await_cstr_copy(failedString" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed string repeated await copy.
    exit /b 1
)

findstr /C:"failedBoolValueA = (*(int*)xl6_future_await_value(failedBool" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed bool first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedBool, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed bool future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedBoolValueB = (*(int*)xl6_future_await_value(failedBool" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed bool repeated await.
    exit /b 1
)

findstr /C:"failedFloatValueA = (*(double*)xl6_future_await_value(failedFloat" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed float first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedFloat, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed float future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedFloatValueB = (*(double*)xl6_future_await_value(failedFloat" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed float repeated await.
    exit /b 1
)

findstr /C:"failedVarValueA = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedVar" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed var first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedVar, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed var future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedVarValueB = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedVar" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed var repeated await retain.
    exit /b 1
)

findstr /C:"failedFunctionValueA = (*(XL6_Function*)xl6_future_await_value(failedFunction" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function first await.
    exit /b 1
)

findstr /C:"failedFunctionValueA.retain(failedFunctionValueA.env)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedFunction, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed function future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedFunctionValueB = (*(XL6_Function*)xl6_future_await_value(failedFunction" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function repeated await.
    exit /b 1
)

findstr /C:"failedFunctionValueB.retain(failedFunctionValueB.env)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function repeated await retain.
    exit /b 1
)

findstr /C:"failedBoxValueA = (*(XL_Class_FailedAsyncBox*)xl6_future_await_value(failedBox" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed class first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedBox, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed class future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedBoxValueB = (*(XL_Class_FailedAsyncBox*)xl6_future_await_value(failedBox" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed class repeated await.
    exit /b 1
)

findstr /C:"failedItemsValueA = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(failedItems" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed array first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedItems, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed array future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedItemsValueB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(failedItems" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed array repeated await copy.
    exit /b 1
)

findstr /C:"failedListValueA = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(failedList" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed list first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedList, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed list future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedListValueB = xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(failedList" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed list repeated await copy.
    exit /b 1
)

findstr /C:"failedSetValueA = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(failedSet" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed set first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedSet, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed set future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedSetValueB = xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(failedSet" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed set repeated await copy.
    exit /b 1
)

findstr /C:"failedDictValueA = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(failedDict" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed dict first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedDict, 0)" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed dict future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedDictValueB = xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(failedDict" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed dict repeated await copy.
    exit /b 1
)

findstr /C:"tempStringValue = xl6_future_await_cstr_release(failStringAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed string await release.
    exit /b 1
)

findstr /C:"tempItemsValue = xl6_future_await_handle_release(failItemsAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed array await release.
    exit /b 1
)

findstr /C:"tempListValue = xl6_future_await_handle_release(failListAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed list await release.
    exit /b 1
)

findstr /C:"tempSetValue = xl6_future_await_handle_release(failSetAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed set await release.
    exit /b 1
)

findstr /C:"tempDictValue = xl6_future_await_handle_release(failDictAsync()" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed dict await release.
    exit /b 1
)

findstr /C:"static int xl_async_worker_asyncFinallyCatchAsync" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally worker.
    exit /b 1
)

findstr /C:"goto __xl6_try_finally_" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally control path.
    exit /b 1
)

findstr /C:"xl6_error_raise(\"Error\", \"async finally override boom\"" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally override error.
    exit /b 1
)

findstr /C:"xl6_error_raise(\"Error\", \"async finally void override boom\"" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally override error.
    exit /b 1
)

findstr /C:"long long finallyCatchRepeated = (*(long long*)xl6_future_await_value(finallyCatchFuture" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally repeated await.
    exit /b 1
)

findstr /C:"finallyOverrideState = 84LL" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally repeated error state.
    exit /b 1
)

findstr /C:"xl6_future_await_value(finallyVoidFuture" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally await.
    exit /b 1
)

findstr /C:"finallyVoidState = 84LL" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally repeated error state.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_non_future.xl >nul 2> release\tmp\sem_await_non_future.err.txt
if not errorlevel 1 (
    echo sem_await_non_future.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_receiver_expr.xl >nul 2> release\tmp\sem_future_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_future_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"future receiver" release\tmp\sem_future_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_future_receiver_expr.xl missing future receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_unknown_method.xl >nul 2> release\tmp\sem_future_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_future_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown future method" release\tmp\sem_future_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_future_unknown_method.xl missing future method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_wait_timeout_type.xl >nul 2> release\tmp\sem_future_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_future_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"future.waitTimeout" release\tmp\sem_future_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_future_wait_timeout_type.xl missing waitTimeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_sync_func.xl >nul 2> release\tmp\sem_await_sync_func.err.txt
if not errorlevel 1 (
    echo sem_await_sync_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_sync_generator.xl >nul 2> release\tmp\sem_await_sync_generator.err.txt
if not errorlevel 1 (
    echo sem_await_sync_generator.xl should fail.
    exit /b 1
)
findstr /C:"await can only be used" release\tmp\sem_await_sync_generator.err.txt >nul
if errorlevel 1 (
    echo sem_await_sync_generator.xl missing await sync generator diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_future.xl >nul 2> release\tmp\sem_async_return_future.err.txt
if not errorlevel 1 (
    echo sem_async_return_future.xl should fail.
    exit /b 1
)
findstr /C:"future/task" release\tmp\sem_async_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_future.xl should report future/task return diagnostic.
    exit /b 1
)
findstr /C:"target future" release\tmp\sem_async_return_future.err.txt >nul
if not errorlevel 1 (
    echo sem_async_return_future.xl should not report return mismatch cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_task.xl >nul 2> release\tmp\sem_async_return_task.err.txt
if not errorlevel 1 (
    echo sem_async_return_task.xl should fail.
    exit /b 1
)
findstr /C:"future/task" release\tmp\sem_async_return_task.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_task.xl should report future/task return diagnostic.
    exit /b 1
)
findstr /C:"target task" release\tmp\sem_async_return_task.err.txt >nul
if not errorlevel 1 (
    echo sem_async_return_task.xl should not report return mismatch cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_channel.xl >nul 2> release\tmp\sem_async_return_channel.err.txt
if not errorlevel 1 (
    echo sem_async_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_channel.xl missing async channel return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_sync.xl >nul 2> release\tmp\sem_async_return_sync.err.txt
if not errorlevel 1 (
    echo sem_async_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_sync.xl missing async sync return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_thread.xl >nul 2> release\tmp\sem_async_return_thread.err.txt
if not errorlevel 1 (
    echo sem_async_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_thread.xl missing async thread return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_future.xl >nul 2> release\tmp\sem_async_param_future.err.txt
if not errorlevel 1 (
    echo sem_async_param_future.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_param_future.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_future.xl missing async future parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_generator.xl >nul 2> release\tmp\sem_async_param_generator.err.txt
if not errorlevel 1 (
    echo sem_async_param_generator.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_param_generator.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_generator.xl missing async generator parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_channel.xl >nul 2> release\tmp\sem_async_param_channel.err.txt
if not errorlevel 1 (
    echo sem_async_param_channel.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_param_channel.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_channel.xl missing async channel parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_sync.xl >nul 2> release\tmp\sem_async_param_sync.err.txt
if not errorlevel 1 (
    echo sem_async_param_sync.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_param_sync.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_sync.xl missing async sync parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_thread.xl >nul 2> release\tmp\sem_async_param_thread.err.txt
if not errorlevel 1 (
    echo sem_async_param_thread.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_async_param_thread.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_thread.xl missing async thread parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_multi_return.xl >nul 2> release\tmp\sem_async_multi_return.err.txt
if not errorlevel 1 (
    echo sem_async_multi_return.xl should fail.
    exit /b 1
)
findstr /C:"multi-return" release\tmp\sem_async_multi_return.err.txt >nul
if errorlevel 1 (
    echo sem_async_multi_return.xl missing async multi-return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_unknown_member.xl >nul 2> release\tmp\sem_task_unknown_member.err.txt
if not errorlevel 1 (
    echo sem_task_unknown_member.xl should fail.
    exit /b 1
)
findstr /C:"unknown task API" release\tmp\sem_task_unknown_member.err.txt >nul
if errorlevel 1 (
    echo sem_task_unknown_member.xl missing unknown task API diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_non_future.xl >nul 2> release\tmp\sem_task_cancel_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" release\tmp\sem_task_cancel_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_non_future.xl missing task.cancel diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_channel.xl >nul 2> release\tmp\sem_task_cancel_channel.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" release\tmp\sem_task_cancel_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_channel.xl missing task.cancel channel diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_generator.xl >nul 2> release\tmp\sem_task_cancel_generator.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" release\tmp\sem_task_cancel_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_generator.xl missing task.cancel generator diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_sync.xl >nul 2> release\tmp\sem_task_cancel_sync.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" release\tmp\sem_task_cancel_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_sync.xl missing task.cancel sync diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_thread.xl >nul 2> release\tmp\sem_task_cancel_thread.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" release\tmp\sem_task_cancel_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_thread.xl missing task.cancel thread diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_named_argument.xl >nul 2> release\tmp\sem_task_cancel_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_cancel_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_arg_count_none.xl >nul 2> release\tmp\sem_task_cancel_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_cancel_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_arg_count_none.xl missing task.cancel argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_arg_count_many.xl >nul 2> release\tmp\sem_task_cancel_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_cancel_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_arg_count_many.xl missing task.cancel too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_arg_count_none.xl >nul 2> release\tmp\sem_task_all_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_all_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_all_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_arg_count_none.xl missing task.all argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_arg_count_many.xl >nul 2> release\tmp\sem_task_all_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_all_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_all_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_arg_count_many.xl missing task.all too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_empty.xl >nul 2> release\tmp\sem_task_all_empty.err.txt
if not errorlevel 1 (
    echo sem_task_all_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_all_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_empty.xl missing task.all diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_non_future.xl >nul 2> release\tmp\sem_task_all_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_all_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_non_future.xl missing task.all non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_array_non_future.xl >nul 2> release\tmp\sem_task_all_array_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_array_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_all_array_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_array_non_future.xl missing task.all array non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_mixed.xl >nul 2> release\tmp\sem_task_all_mixed.err.txt
if not errorlevel 1 (
    echo sem_task_all_mixed.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_all_mixed.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_mixed.xl missing task.all diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_mixed.xl >nul 2> release\tmp\sem_task_race_mixed.err.txt
if not errorlevel 1 (
    echo sem_task_race_mixed.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_race_mixed.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_mixed.xl missing task.race diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_non_future.xl >nul 2> release\tmp\sem_task_race_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_race_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_race_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_non_future.xl missing task.race non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_array_non_future.xl >nul 2> release\tmp\sem_task_race_array_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_race_array_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_race_array_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_array_non_future.xl missing task.race array non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_named_argument.xl >nul 2> release\tmp\sem_task_all_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_all_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_all_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_var.xl >nul 2> release\tmp\sem_task_all_var.err.txt
if not errorlevel 1 (
    echo sem_task_all_var.xl should fail.
    exit /b 1
)
findstr /C:"future<var>" release\tmp\sem_task_all_var.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_var.xl missing task.all future var diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_future.xl >nul 2> release\tmp\sem_task_all_return_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_future.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_future.xl missing task.all nested future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_channel.xl >nul 2> release\tmp\sem_task_all_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_channel.xl missing task.all channel future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_generator.xl >nul 2> release\tmp\sem_task_all_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_generator.xl missing task.all generator future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_future.xl >nul 2> release\tmp\sem_task_all_typed_array_return_future.err.txt
if not errorlevel 1 (
    echo sem_task_all_typed_array_return_future.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_typed_array_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_typed_array_return_future.xl missing task.all typed array nested future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_channel.xl >nul 2> release\tmp\sem_task_all_typed_array_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_all_typed_array_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_typed_array_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_typed_array_return_channel.xl missing task.all typed array channel future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_typed_array_return_generator.xl >nul 2> release\tmp\sem_task_all_typed_array_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_all_typed_array_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_all_typed_array_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_typed_array_return_generator.xl missing task.all typed array generator future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_array.xl >nul 2> release\tmp\sem_task_all_return_typed_array.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_typed_array.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_all_return_typed_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_typed_array.xl missing task.all container future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_list.xl >nul 2> release\tmp\sem_task_all_return_typed_list.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_typed_list.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_all_return_typed_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_typed_list.xl missing task.all list future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_set.xl >nul 2> release\tmp\sem_task_all_return_typed_set.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_typed_set.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_all_return_typed_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_typed_set.xl missing task.all set future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_all_return_typed_dict.xl >nul 2> release\tmp\sem_task_all_return_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_task_all_return_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_all_return_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_all_return_typed_dict.xl missing task.all dict future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_empty.xl >nul 2> release\tmp\sem_task_race_empty.err.txt
if not errorlevel 1 (
    echo sem_task_race_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.all/task.race" release\tmp\sem_task_race_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_empty.xl missing task.race diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_var.xl >nul 2> release\tmp\sem_task_race_var.err.txt
if not errorlevel 1 (
    echo sem_task_race_var.xl should fail.
    exit /b 1
)
findstr /C:"future<var>" release\tmp\sem_task_race_var.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_var.xl missing task.race future var diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_sync.xl >nul 2> release\tmp\sem_task_race_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_race_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_sync.xl missing task.race sync future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_thread.xl >nul 2> release\tmp\sem_task_race_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_race_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_thread.xl missing task.race thread future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_typed_array_return_sync.xl >nul 2> release\tmp\sem_task_race_typed_array_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_race_typed_array_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_race_typed_array_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_typed_array_return_sync.xl missing task.race typed array sync future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_typed_array_return_thread.xl >nul 2> release\tmp\sem_task_race_typed_array_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_race_typed_array_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"future resource handle result" release\tmp\sem_task_race_typed_array_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_typed_array_return_thread.xl missing task.race typed array thread future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_array.xl >nul 2> release\tmp\sem_task_race_return_typed_array.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_typed_array.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_race_return_typed_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_typed_array.xl missing task.race container future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_list.xl >nul 2> release\tmp\sem_task_race_return_typed_list.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_typed_list.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_race_return_typed_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_typed_list.xl missing task.race list future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_set.xl >nul 2> release\tmp\sem_task_race_return_typed_set.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_typed_set.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_race_return_typed_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_typed_set.xl missing task.race set future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_return_typed_dict.xl >nul 2> release\tmp\sem_task_race_return_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_task_race_return_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"future<array/list/set/dict>" release\tmp\sem_task_race_return_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_return_typed_dict.xl missing task.race dict future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_arg_count_none.xl >nul 2> release\tmp\sem_task_race_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_race_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_race_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_arg_count_none.xl missing task.race argument count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_arg_count_many.xl >nul 2> release\tmp\sem_task_race_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_race_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_race_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_arg_count_many.xl missing task.race too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_temp_class_await.xl >nul 2> release\tmp\sem_task_race_temp_class_await.err.txt
if not errorlevel 1 (
    echo sem_task_race_temp_class_await.xl should fail.
    exit /b 1
)
findstr /C:"temporary await of task.all/task.race class result" release\tmp\sem_task_race_temp_class_await.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_temp_class_await.xl missing temporary class race await diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_race_named_argument.xl >nul 2> release\tmp\sem_task_race_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_race_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_race_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_race_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_empty.xl >nul 2> release\tmp\sem_task_when_any_empty.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_empty.xl should fail.
    exit /b 1
)
findstr /C:"task.whenAny" release\tmp\sem_task_when_any_empty.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_empty.xl missing task.whenAny empty diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_non_future.xl >nul 2> release\tmp\sem_task_when_any_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.whenAny" release\tmp\sem_task_when_any_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_non_future.xl missing task.whenAny non-future diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_when_any_named_argument.xl >nul 2> release\tmp\sem_task_when_any_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_when_any_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task API" release\tmp\sem_task_when_any_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_when_any_named_argument.xl missing task API named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_bare_name.xl >nul 2> release\tmp\sem_task_spawn_bare_name.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_bare_name.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn first argument" release\tmp\sem_task_spawn_bare_name.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_bare_name.xl missing task.spawn first argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_first_arg_non_func.xl >nul 2> release\tmp\sem_task_spawn_first_arg_non_func.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_first_arg_non_func.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn first argument" release\tmp\sem_task_spawn_first_arg_non_func.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_first_arg_non_func.xl missing task.spawn non-func first argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_named_argument.xl >nul 2> release\tmp\sem_task_spawn_named_argument.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn does not accept named arguments" release\tmp\sem_task_spawn_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_named_argument.xl missing task.spawn named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count_none.xl >nul 2> release\tmp\sem_task_spawn_arg_count_none.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count_none.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn requires at least a func argument" release\tmp\sem_task_spawn_arg_count_none.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count_none.xl missing task.spawn empty argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count.xl >nul 2> release\tmp\sem_task_spawn_arg_count.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument count" release\tmp\sem_task_spawn_arg_count.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count.xl missing task.spawn count diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_count_many.xl >nul 2> release\tmp\sem_task_spawn_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_count_many.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument count" release\tmp\sem_task_spawn_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_count_many.xl missing task.spawn too many arguments diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_type_mismatch.xl >nul 2> release\tmp\sem_task_spawn_arg_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn argument type does not match func signature" release\tmp\sem_task_spawn_arg_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_type_mismatch.xl missing task.spawn argument type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_multi_return.xl >nul 2> release\tmp\sem_task_spawn_arg_multi_return.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_multi_return.xl should fail.
    exit /b 1
)
findstr /C:"multi-return call cannot be passed as one task.spawn argument" release\tmp\sem_task_spawn_arg_multi_return.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_multi_return.xl missing task.spawn multi-return argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_multi_return.xl >nul 2> release\tmp\sem_task_spawn_multi_return.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_multi_return.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn currently supports only no-return or single-return" release\tmp\sem_task_spawn_multi_return.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_multi_return.xl missing task.spawn no-return/single-return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_var.xl >nul 2> release\tmp\sem_task_spawn_return_var.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_var.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type cannot be var" release\tmp\sem_task_spawn_return_var.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_var.xl missing task.spawn return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_dict_vararg.xl >nul 2> release\tmp\sem_task_spawn_dict_vararg.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_dict_vararg.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn does not support ***" release\tmp\sem_task_spawn_dict_vararg.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_dict_vararg.xl missing task.spawn dict vararg diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_array.xl >nul 2> release\tmp\sem_task_spawn_return_typed_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_typed_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_typed_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_array.xl missing task.spawn typed array return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_list.xl >nul 2> release\tmp\sem_task_spawn_return_typed_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_typed_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_typed_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_list.xl missing task.spawn typed list return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_set.xl >nul 2> release\tmp\sem_task_spawn_return_typed_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_typed_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_typed_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_set.xl missing task.spawn typed set return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_typed_dict.xl >nul 2> release\tmp\sem_task_spawn_return_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_typed_dict.xl missing task.spawn typed dict return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_array.xl >nul 2> release\tmp\sem_task_spawn_return_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_array.xl missing task.spawn array return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_list.xl >nul 2> release\tmp\sem_task_spawn_return_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_list.xl missing task.spawn list return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_set.xl >nul 2> release\tmp\sem_task_spawn_return_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_set.xl missing task.spawn set return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_dict.xl >nul 2> release\tmp\sem_task_spawn_return_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_dict.xl missing task.spawn dict return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_function.xl >nul 2> release\tmp\sem_task_spawn_return_function.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_function.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_function.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_function.xl missing task.spawn function return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_class.xl >nul 2> release\tmp\sem_task_spawn_return_class.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_class.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_class.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_class.xl missing task.spawn class return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_channel.xl >nul 2> release\tmp\sem_task_spawn_return_channel.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_channel.xl missing task.spawn channel return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_future.xl >nul 2> release\tmp\sem_task_spawn_return_future.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_future.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_future.xl missing task.spawn future return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_generator.xl >nul 2> release\tmp\sem_task_spawn_return_generator.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_generator.xl missing task.spawn generator return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_sync.xl >nul 2> release\tmp\sem_task_spawn_return_sync.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_sync.xl missing task.spawn sync return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_return_thread.xl >nul 2> release\tmp\sem_task_spawn_return_thread.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn return type must be supported" release\tmp\sem_task_spawn_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_return_thread.xl missing task.spawn thread return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_array.xl >nul 2> release\tmp\sem_task_spawn_arg_typed_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_typed_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_array.xl missing task.spawn typed array argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_list.xl >nul 2> release\tmp\sem_task_spawn_arg_typed_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_typed_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_list.xl missing task.spawn typed list argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_set.xl >nul 2> release\tmp\sem_task_spawn_arg_typed_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_typed_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_set.xl missing task.spawn typed set argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_typed_dict.xl >nul 2> release\tmp\sem_task_spawn_arg_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_typed_dict.xl missing task.spawn typed dict argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_array.xl >nul 2> release\tmp\sem_task_spawn_arg_array.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_array.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_array.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_array.xl missing task.spawn array argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_list.xl >nul 2> release\tmp\sem_task_spawn_arg_list.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_list.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_list.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_list.xl missing task.spawn list argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_set.xl >nul 2> release\tmp\sem_task_spawn_arg_set.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_set.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_set.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_set.xl missing task.spawn set argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_dict.xl >nul 2> release\tmp\sem_task_spawn_arg_dict.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_dict.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_dict.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_dict.xl missing task.spawn dict argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_function.xl >nul 2> release\tmp\sem_task_spawn_arg_function.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_function.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_function.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_function.xl missing task.spawn function argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_class.xl >nul 2> release\tmp\sem_task_spawn_arg_class.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_class.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_class.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_class.xl missing task.spawn class argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_channel.xl >nul 2> release\tmp\sem_task_spawn_arg_channel.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_channel.xl missing task.spawn channel argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_future.xl >nul 2> release\tmp\sem_task_spawn_arg_future.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_future.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_future.xl missing task.spawn future argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_generator.xl >nul 2> release\tmp\sem_task_spawn_arg_generator.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_generator.xl missing task.spawn generator argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_sync.xl >nul 2> release\tmp\sem_task_spawn_arg_sync.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_sync.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_sync.err.txt >nul
if errorlevel 1 (
    echo sem_task_spawn_arg_sync.xl missing task.spawn sync argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_spawn_arg_thread.xl >nul 2> release\tmp\sem_task_spawn_arg_thread.err.txt
if not errorlevel 1 (
    echo sem_task_spawn_arg_thread.xl should fail.
    exit /b 1
)
findstr /C:"task.spawn parameter type must be supported" release\tmp\sem_task_spawn_arg_thread.err.txt >nul
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

release\xl.exe build test\cases\cgen_async_generator.xl -o release\tmp\cgen_async_generator.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_async_generator.exe
if errorlevel 1 exit /b 1

for /L %%I in (1,1,5) do (
    release\tmp\cgen_async_generator.exe
    if errorlevel 1 (
        echo cgen_async_generator.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_async_generator.xl --emit release\tmp\cgen_async_generator.c
if errorlevel 1 exit /b 1

findstr /C:"XL6_Generator* stream" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing generator return symbol for async func.
    exit /b 1
)
findstr /C:"xl_gen_worker_stream" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_brokenEarly" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator early-error worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_emptyStream" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator empty worker.
    exit /b 1
)
findstr /C:"xl6_future_await_i64_release" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing await lowering inside generator.
    exit /b 1
)
findstr /C:"xl6_generator_next" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await generator iteration.
    exit /b 1
)
findstr /C:"XL6_Generator* savedStream = stream(30LL);" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator value.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedStream;\s*savedStream = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &value,[\s\S]*?savedTotal = \(savedTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator move-to-iterator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* savedBroken = broken();" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator error source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBroken;\s*savedBroken = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""async generator boom""'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator error propagation lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* savedBrokenEarly = brokenEarly();" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator early-error source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBrokenEarly;\s*savedBrokenEarly = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""async generator boom before yield""'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator early-error propagation lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* savedEmpty = emptyStream();" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved empty async generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedEmpty;\s*savedEmpty = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedEmptyCount = \(savedEmptyCount \+ 1LL\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved empty async generator iterator cleanup lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* savedIndexed = indexedStream();" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator indexed source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexed;\s*savedIndexed = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long savedIndex = __xl6_gen_index_\1;[\s\S]*?long long savedValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &savedValue,[\s\S]*?if \(\(savedIndex == 1LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator break cleanup lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexed;\s*savedIndexed = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator second-pass rejection after break.
    exit /b 1
)
findstr /C:"XL6_Generator* savedIndexedContinue = indexedStream();" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator continue source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexedContinue;\s*savedIndexedContinue = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long savedContinueIndex = __xl6_gen_index_\1;[\s\S]*?long long savedContinueValue = 0;[\s\S]*?if \(\(savedContinueIndex == 1LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?if \(\(savedContinueIndex == 2LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator continue/break lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedContinueAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedIndexedContinue;\s*savedIndexedContinue = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedContinueAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved async generator second-pass rejection after continue/break.
    exit /b 1
)
findstr /C:"XL6_Generator* returnEarlyStream(void)" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing return-early async generator symbol.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_returnEarlyStream[\s\S]*?xl6_generator_yield_copy[\s\S]*?addAsync\(0LL, 5LL\)[\s\S]*?return;[\s\S]*?addAsync\(0LL, 50LL\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing return-early async generator worker lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedReturnEarly = returnEarlyStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarly;\s*savedReturnEarly = 0;[\s\S]*?savedReturnEarlyCount = \(savedReturnEarlyCount \+ 1LL\);[\s\S]*?savedReturnEarlyTotal = \(savedReturnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-early async generator consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedReturnEarlyAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarly;\s*savedReturnEarly = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnEarlyAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-early async generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long returnEarlyCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = returnEarlyStream\(\);[\s\S]*?returnEarlyCount = \(returnEarlyCount \+ 1LL\);[\s\S]*?returnEarlyTotal = \(returnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnEarlyCount == 1LL[\s\S]*?returnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing temporary return-early async generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* returnBeforeYieldStream(void)" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing return-before-yield async generator symbol.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_returnBeforeYieldStream[\s\S]*?\{\s*XL6_Generator\* __xl6_gen = \(XL6_Generator\*\)__xl6_arg;\s*return;[\s\S]*?addAsync\(0LL, 60LL\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing return-before-yield async generator worker lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedReturnBeforeYield = returnBeforeYieldStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYield;\s*savedReturnBeforeYield = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedReturnBeforeYieldCount = \(savedReturnBeforeYieldCount \+ 1LL\);[\s\S]*?savedReturnBeforeYieldTotal = \(savedReturnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnBeforeYieldCount == 0LL[\s\S]*?savedReturnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-before-yield async generator empty consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedReturnBeforeYieldAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYield;\s*savedReturnBeforeYield = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnBeforeYieldAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing saved return-before-yield async generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long returnBeforeYieldCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = returnBeforeYieldStream\(\);[\s\S]*?returnBeforeYieldCount = \(returnBeforeYieldCount \+ 1LL\);[\s\S]*?returnBeforeYieldTotal = \(returnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnBeforeYieldCount == 0LL[\s\S]*?returnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing temporary return-before-yield async generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* plainStream(void)" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing ordinary generator symbol for for-await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_plainStream[\s\S]*?xl6_generator_yield_copy[\s\S]*?\(long long\)\{7LL\}[\s\S]*?xl6_generator_yield_copy[\s\S]*?\(long long\)\{8LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing ordinary generator worker for for-await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long plainAwaitTotal = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = plainStream\(\);[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?plainAwaitTotal = \(plainAwaitTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?plainAwaitTotal == 15LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator temporary consumption lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedPlainAwait = plainStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwait;\s*savedPlainAwait = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long plainIndex = __xl6_gen_index_\1;[\s\S]*?long long plainValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &plainValue,[\s\S]*?savedPlainAwaitWeighted = \(\(savedPlainAwaitWeighted \+ \(plainIndex \* 100LL\)\) \+ plainValue\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedPlainAwaitWeighted == 115LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await saved ordinary generator index/value lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedPlainAwaitAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwait;\s*savedPlainAwait = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedPlainAwaitAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedPlainAwaitBreak = plainStream\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwaitBreak;\s*savedPlainAwaitBreak = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long plainBreakIndex = __xl6_gen_index_\1;[\s\S]*?long long plainBreakValue = 0;[\s\S]*?if \(\(plainBreakIndex == 0LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?savedPlainAwaitBreakTotal = \(\(savedPlainAwaitBreakTotal \+ \(plainBreakIndex \* 100LL\)\) \+ plainBreakValue\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;[\s\S]*?__xl6_continue_\1:[\s\S]*?__xl6_gen_index_\1 \+= 1;[\s\S]*?savedPlainAwaitBreakTotal == 108LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator continue/break cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedPlainAwaitBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedPlainAwaitBreak;\s*savedPlainAwaitBreak = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedPlainAwaitBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing for-await ordinary generator break second-pass rejection.
    exit /b 1
)
findstr /C:"iterate null generator" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator null diagnostic.
    exit /b 1
)
findstr /C:"XL6_Generator* consumedAsync = stream(40LL);" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedAsync;\s*consumedAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?consumedAsyncFirstTotal = \(consumedAsyncFirstTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator first-pass move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedAsync;\s*consumedAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?consumedAsyncAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing consumed async generator second-pass null rejection.
    exit /b 1
)
findstr /C:"XL6_Generator* missingAsync = 0;" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing null async generator initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = missingAsync;\s*missingAsync = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?missingAsyncState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing null async generator rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_gen_worker_indexedStream[\s\S]*?xl6_future_await_i64_release[\s\S]*?xl6_generator_yield_copy'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing indexed async generator await-yield worker.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long __xl6_gen_index_([0-9]+) = 0;[\s\S]*?long long asyncIndex = __xl6_gen_index_\1;[\s\S]*?long long asyncValue = 0;[\s\S]*?xl6_generator_value_into\(__xl6_gen_iter_\1, &asyncValue'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator two-target index/value lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_async_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(\(asyncIndex == 2LL\)\) \{[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_([0-9]+)\);[\s\S]*?goto __xl6_break_\1;'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator break cleanup.
    exit /b 1
)
findstr /C:"async generator boom before yield" release\tmp\cgen_async_generator.c >nul
if errorlevel 1 (
    echo cgen_async_generator.c missing async generator early-error diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_await_non_generator.xl >nul 2> release\tmp\sem_for_await_non_generator.err.txt
if not errorlevel 1 (
    echo sem_for_await_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"for await requires generator<T>" release\tmp\sem_for_await_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_for_await_non_generator.xl missing for-await diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_type_non_generator.xl >nul 2> release\tmp\sem_async_type_non_generator.err.txt
if not errorlevel 1 (
    echo sem_async_type_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"async type marker is only supported on generator<T>" release\tmp\sem_async_type_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_async_type_non_generator.xl missing async type marker diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_generator_return_value.xl >nul 2> release\tmp\sem_async_generator_return_value.err.txt
if not errorlevel 1 (
    echo sem_async_generator_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_async_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" release\tmp\sem_async_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_generator_type_marker_return_value.xl >nul 2> release\tmp\sem_async_generator_type_marker_return_value.err.txt
if not errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_async_generator_type_marker_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" release\tmp\sem_async_generator_type_marker_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_async_generator_type_marker_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_return_value.xl >nul 2> release\tmp\sem_generator_return_value.err.txt
if not errorlevel 1 (
    echo sem_generator_return_value.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_generator_return_value.xl missing generator diagnostic.
    exit /b 1
)
findstr /C:"return" release\tmp\sem_generator_return_value.err.txt >nul
if errorlevel 1 (
    echo sem_generator_return_value.xl missing return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_generator.xl -o release\tmp\cgen_generator.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_generator.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    release\tmp\cgen_generator.exe
    if errorlevel 1 (
        echo cgen_generator.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_generator.xl --emit release\tmp\cgen_generator.c
if errorlevel 1 exit /b 1

findstr /C:"xrtCoCreate" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing xrtCoCreate.
    exit /b 1
)
findstr /C:"xl6_generator_yield_copy" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator yield helper.
    exit /b 1
)
findstr /C:"xl6_generator_next" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator next helper.
    exit /b 1
)
findstr /C:"xl6_generator_destroy" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator cleanup helper.
    exit /b 1
)
findstr /C:"xl6_generator_store_active_error" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator stored error helper.
    exit /b 1
)
findstr /C:"xl6_generator_raise_stored_error" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator stored error rethrow helper.
    exit /b 1
)
findstr /C:"xl6_generator_store_active_error(__xl6_gen)" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator worker error capture.
    exit /b 1
)
findstr /C:"xl_gen_worker_brokenEarly" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator early-error worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_emptyNums" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing empty generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_toggles" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing bool generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_weights" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing float generator worker.
    exit /b 1
)
findstr /C:"12:34:56" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing time generator literal lowering.
    exit /b 1
)
findstr /C:"generator boom before yield" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator early-error diagnostic.
    exit /b 1
)
findstr /C:"iterate null generator" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator null diagnostic.
    exit /b 1
)
findstr /C:"XL6_Generator* savedBreakGen = nums(3LL);" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing saved break generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedBreakGen = nums\(3LL\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBreakGen;\s*savedBreakGen = 0;\s*long long __xl6_gen_index_\1 = 0;[\s\S]*?long long index = __xl6_gen_index_\1;[\s\S]*?long long item = 0;[\s\S]*?if \(\(index == 0LL\)\) \{[\s\S]*?goto __xl6_continue_\1;[\s\S]*?savedBreakTotal = \(\(savedBreakTotal \+ \(index \* 100LL\)\) \+ item\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\);[\s\S]*?goto __xl6_break_\1;[\s\S]*?__xl6_continue_\1:[\s\S]*?__xl6_gen_index_\1 \+= 1;[\s\S]*?savedBreakTotal == 102LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved generator continue/break cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedBreakAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedBreakGen;\s*savedBreakGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedBreakAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved generator break second-pass rejection.
    exit /b 1
)
findstr /C:"XL6_Generator* consumedGen = nums(1LL);" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedGen;\s*consumedGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?consumedFirstTotal = \(consumedFirstTotal \+ item\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator first-pass move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = consumedGen;\s*consumedGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?consumedAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing consumed generator second-pass null rejection.
    exit /b 1
)
findstr /C:"XL6_Generator* missingGen = 0;" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing null generator initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* __xl6_gen_iter_([0-9]+) = missingGen;\s*missingGen = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?xrtStrComp\(xl6_error_message\(e\), ""iterate null generator""[\s\S]*?missingGenState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing null generator rejection.
    exit /b 1
)
findstr /C:"XL6_Generator* returnEarlyNums(void)" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing return-early generator function.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_returnEarlyNums[\s\S]*?xl6_generator_yield_copy[\s\S]*?\{5LL\}[\s\S]*?return;[\s\S]*?\{50LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing return-early generator worker shape.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedReturnEarlyNums = returnEarlyNums\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarlyNums;\s*savedReturnEarlyNums = 0;[\s\S]*?savedReturnEarlyCount = \(savedReturnEarlyCount \+ 1LL\);[\s\S]*?savedReturnEarlyTotal = \(savedReturnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnEarlyCount == 1LL[\s\S]*?savedReturnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved return-early generator move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedReturnEarlyAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnEarlyNums;\s*savedReturnEarlyNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnEarlyAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved return-early generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long returnEarlyCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = returnEarlyNums\(\);[\s\S]*?returnEarlyCount = \(returnEarlyCount \+ 1LL\);[\s\S]*?returnEarlyTotal = \(returnEarlyTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnEarlyCount == 1LL[\s\S]*?returnEarlyTotal == 5LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing temporary return-early generator lowering.
    exit /b 1
)
findstr /C:"XL6_Generator* returnBeforeYieldNums(void)" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing pre-yield return generator function.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl_gen_worker_returnBeforeYieldNums[\s\S]*?\{\s*XL6_Generator\* __xl6_gen = \(XL6_Generator\*\)__xl6_arg;\s*return;[\s\S]*?\{60LL\}'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing pre-yield return generator worker shape.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'XL6_Generator\* savedReturnBeforeYieldNums = returnBeforeYieldNums\(\);[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYieldNums;\s*savedReturnBeforeYieldNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?savedReturnBeforeYieldCount = \(savedReturnBeforeYieldCount \+ 1LL\);[\s\S]*?savedReturnBeforeYieldTotal = \(savedReturnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?savedReturnBeforeYieldCount == 0LL[\s\S]*?savedReturnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved pre-yield return generator move lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long savedReturnBeforeYieldAgainState = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = savedReturnBeforeYieldNums;\s*savedReturnBeforeYieldNums = 0;[\s\S]*?while \(xl6_generator_next\(__xl6_gen_iter_\1,[\s\S]*?goto __xl6_try_catch_([0-9]+);[\s\S]*?__xl6_try_catch_\2:[\s\S]*?savedReturnBeforeYieldAgainState = 42LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing saved pre-yield return generator second-pass rejection.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_generator.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'long long returnBeforeYieldCount = 0LL;[\s\S]*?XL6_Generator\* __xl6_gen_iter_([0-9]+) = returnBeforeYieldNums\(\);[\s\S]*?returnBeforeYieldCount = \(returnBeforeYieldCount \+ 1LL\);[\s\S]*?returnBeforeYieldTotal = \(returnBeforeYieldTotal \+ value\);[\s\S]*?xl6_generator_destroy\(__xl6_gen_iter_\1\)[\s\S]*?returnBeforeYieldCount == 0LL[\s\S]*?returnBeforeYieldTotal == 0LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_generator.c missing temporary pre-yield return generator lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator_two_targets.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generator_two_targets.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_generator_two_targets.xl -o release\tmp\cgen_generator_two_targets.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_generator_two_targets.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    release\tmp\cgen_generator_two_targets.exe
    if errorlevel 1 (
        echo cgen_generator_two_targets.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_generator_two_targets.xl --emit release\tmp\cgen_generator_two_targets.c
if errorlevel 1 exit /b 1

findstr /C:"long long __xl6_gen_index_" release\tmp\cgen_generator_two_targets.c >nul
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index counter.
    exit /b 1
)
findstr /C:"long long index = __xl6_gen_index_" release\tmp\cgen_generator_two_targets.c >nul
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index target.
    exit /b 1
)
powershell -NoProfile -Command "if (-not (Select-String -Path 'release\tmp\cgen_generator_two_targets.c' -Pattern '__xl6_gen_index_[0-9]+ \+= 1;' -Quiet)) { exit 1 }"
if errorlevel 1 (
    echo cgen_generator_two_targets.c missing generator index increment.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_outside_generator.xl >nul 2> release\tmp\sem_yield_outside_generator.err.txt
if not errorlevel 1 (
    echo sem_yield_outside_generator.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_sync_func.xl >nul 2> release\tmp\sem_yield_sync_func.err.txt
if not errorlevel 1 (
    echo sem_yield_sync_func.xl should fail.
    exit /b 1
)
findstr /C:"yield" release\tmp\sem_yield_sync_func.err.txt >nul
if errorlevel 1 (
    echo sem_yield_sync_func.xl missing yield outside generator diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_async_non_generator.xl >nul 2> release\tmp\sem_yield_async_non_generator.err.txt
if not errorlevel 1 (
    echo sem_yield_async_non_generator.xl should fail.
    exit /b 1
)
findstr /C:"yield" release\tmp\sem_yield_async_non_generator.err.txt >nul
if errorlevel 1 (
    echo sem_yield_async_non_generator.xl missing async non-generator yield diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_string_element.xl >nul 2> release\tmp\sem_generator_string_element.err.txt
if not errorlevel 1 (
    echo sem_generator_string_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" release\tmp\sem_generator_string_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_string_element.xl missing generator element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_var_element.xl >nul 2> release\tmp\sem_generator_var_element.err.txt
if not errorlevel 1 (
    echo sem_generator_var_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" release\tmp\sem_generator_var_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_element.xl missing generator var element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_typed_array_element.xl >nul 2> release\tmp\sem_generator_typed_array_element.err.txt
if not errorlevel 1 (
    echo sem_generator_typed_array_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" release\tmp\sem_generator_typed_array_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_element.xl missing generator typed array element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_class_element.xl >nul 2> release\tmp\sem_generator_class_element.err.txt
if not errorlevel 1 (
    echo sem_generator_class_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" release\tmp\sem_generator_class_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_class_element.xl missing generator element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_function_element.xl >nul 2> release\tmp\sem_generator_function_element.err.txt
if not errorlevel 1 (
    echo sem_generator_function_element.xl should fail.
    exit /b 1
)
findstr /C:"generator<T>" release\tmp\sem_generator_function_element.err.txt >nul
if errorlevel 1 (
    echo sem_generator_function_element.xl missing generator element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_string_param.xl >nul 2> release\tmp\sem_generator_string_param.err.txt
if not errorlevel 1 (
    echo sem_generator_string_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_string_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_string_param.xl missing generator string parameter diagnostic.
    exit /b 1
)
findstr /C:"bool" release\tmp\sem_generator_string_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_string_param.xl missing generator string parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_class_param.xl >nul 2> release\tmp\sem_generator_class_param.err.txt
if not errorlevel 1 (
    echo sem_generator_class_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_class_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_class_param.xl missing generator parameter diagnostic.
    exit /b 1
)
findstr /C:"bool" release\tmp\sem_generator_class_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_class_param.xl missing generator parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_var_param.xl >nul 2> release\tmp\sem_generator_var_param.err.txt
if not errorlevel 1 (
    echo sem_generator_var_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_var_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_param.xl missing generator var parameter diagnostic.
    exit /b 1
)
findstr /C:"bool" release\tmp\sem_generator_var_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_var_param.xl missing generator var parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_typed_array_param.xl >nul 2> release\tmp\sem_generator_typed_array_param.err.txt
if not errorlevel 1 (
    echo sem_generator_typed_array_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_typed_array_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_param.xl missing generator typed array parameter diagnostic.
    exit /b 1
)
findstr /C:"bool" release\tmp\sem_generator_typed_array_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_typed_array_param.xl missing generator typed array parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_function_param.xl >nul 2> release\tmp\sem_generator_function_param.err.txt
if not errorlevel 1 (
    echo sem_generator_function_param.xl should fail.
    exit /b 1
)
findstr /C:"generator" release\tmp\sem_generator_function_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_function_param.xl missing generator parameter diagnostic.
    exit /b 1
)
findstr /C:"bool" release\tmp\sem_generator_function_param.err.txt >nul
if errorlevel 1 (
    echo sem_generator_function_param.xl missing generator parameter type boundary.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_yield_type.xl >nul 2> release\tmp\sem_generator_yield_type.err.txt
if not errorlevel 1 (
    echo sem_generator_yield_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value float" release\tmp\sem_generator_yield_type.err.txt >nul
if errorlevel 1 (
    echo sem_generator_yield_type.xl missing yield type context.
    exit /b 1
)

release\xl.exe run test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_channel.xl -o release\tmp\cgen_channel.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_channel.exe
if errorlevel 1 exit /b 1
for /L %%I in (1,1,5) do (
    release\tmp\cgen_channel.exe
    if errorlevel 1 (
        echo cgen_channel.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_channel.xl --emit release\tmp\cgen_channel.c
if errorlevel 1 exit /b 1

findstr /C:"xrtChannelCreate" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing xrtChannelCreate.
    exit /b 1
)
findstr /C:"xl6_channel_require_handle(xrtChannelCreate" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing channel constructor error guard.
    exit /b 1
)
findstr /C:"xl6_channel_send_value" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing send helper.
    exit /b 1
)
findstr /C:"xl6_channel_recv_into" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing recv helper.
    exit /b 1
)
findstr /C:"xrtChannelTryRecv" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing try recv path.
    exit /b 1
)
findstr /C:"12:34:56" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing time channel literal lowering.
    exit /b 1
)
findstr /C:"xl6_channel_send_value(closedCh" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing closed channel send assertion.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('xchannel defaultCh = xl6_channel_require_handle(xrtChannelCreate(0)','xrtChannelCount(defaultCh) == 2LL','xl6_channel_recv_into(defaultCh, &defaultFirst, sizeof(long long), 0','xrtChannelClose(defaultCh)','xrtChannelIsClosed(defaultCh)','xl6_channel_recv_into(defaultCh, &defaultSecond, sizeof(long long), 1','!xl6_channel_recv_into(defaultCh, &defaultSecond, sizeof(long long), 0','!xl6_channel_send_value(defaultCh, &(long long){3LL}, sizeof(long long)','xchannel fullCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_send_value(fullCh, &(long long){10LL}, sizeof(long long)','xl6_channel_send_value(fullCh, &(long long){11LL}, sizeof(long long)','!xl6_channel_send_value(fullCh, &(long long){12LL}, sizeof(long long)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing channel close-drain or capacity guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('xchannel crossThreadCh = xl6_channel_require_handle(xrtChannelCreate(4LL)','xchannel crossThreadCh;','xl6_channel_send_value(__env->crossThreadCh, &(long long){314LL}','xl6_channel_send_value(__env->crossThreadCh, &(long long){159LL}','xrtChannelClose(__env->crossThreadCh)','XL6_Function channelProducer = xl6_anon_','_make(crossThreadCh)','void* producerThread = xl6_thread_require_handle(xl6_thread_start(channelProducer, 1','xrtThreadWaitTimeout(producerThread, 2000LL) == 0','(int)xrtThreadGetExitCode(producerThread) == 17LL','xrtChannelIsClosed(crossThreadCh)','xl6_channel_recv_into(crossThreadCh, &crossFirst, sizeof(long long), 1','xl6_channel_recv_into(crossThreadCh, &crossSecond, sizeof(long long), 1','!xl6_channel_recv_into(crossThreadCh, &crossSecond, sizeof(long long), 0','!xl6_channel_send_value(crossThreadCh, &(long long){265LL}, sizeof(long long)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing cross-thread captured channel guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('xchannel delayedRecvCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_send_value(__env->delayedRecvCh, &(long long){271LL}','xl6_channel_recv_into(delayedRecvCh, &delayedRecvValue, sizeof(long long), 1','xrtThreadWaitTimeout(delayedRecvThread, 2000LL) == 0','(int)xrtThreadGetExitCode(delayedRecvThread) == 19LL','xchannel delayedConsumerCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_recv_into(__env->delayedConsumerCh, &got, sizeof(long long), 1','xl6_channel_send_value(delayedConsumerCh, &(long long){2718LL}','xrtThreadWaitTimeout(delayedConsumerThread, 2000LL) == 0','(int)xrtThreadGetExitCode(delayedConsumerThread) == 2718LL'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing blocking channel wakeup guard lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('release\tmp\cgen_channel.c',[System.Text.Encoding]::UTF8); $checks=@('xchannel nonblockingTryThreadCh = xl6_channel_require_handle(xrtChannelCreate(1LL)','xl6_channel_recv_into(__env->nonblockingTryThreadCh, &got, sizeof(long long), 0','(int)xrtThreadGetExitCode(nonblockingTryThread) == 31LL','xl6_channel_send_value(nonblockingTryThreadCh, &(long long){42LL}, sizeof(long long)','xl6_channel_recv_into(nonblockingTryThreadCh, &nonblockingTryLater, sizeof(long long), 1','xrtChannelClose(nonblockingTryThreadCh)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ Write-Host ('cgen_channel.c missing '+$p); exit 1 } }"
if errorlevel 1 (
    echo cgen_channel.c missing captured tryRecvInto nonblocking guard lowering.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_string_element.xl >nul 2> release\tmp\sem_channel_string_element.err.txt
if not errorlevel 1 (
    echo sem_channel_string_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_string_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_string_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_var_element.xl >nul 2> release\tmp\sem_channel_var_element.err.txt
if not errorlevel 1 (
    echo sem_channel_var_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_var_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_var_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_generator_element.xl >nul 2> release\tmp\sem_channel_generator_element.err.txt
if not errorlevel 1 (
    echo sem_channel_generator_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_generator_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_generator_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_typed_array_element.xl >nul 2> release\tmp\sem_channel_typed_array_element.err.txt
if not errorlevel 1 (
    echo sem_channel_typed_array_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_typed_array_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_typed_array_element.xl missing channel typed array element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_generic_arity.xl >nul 2> release\tmp\sem_channel_generic_arity.err.txt
if not errorlevel 1 (
    echo sem_channel_generic_arity.xl should fail.
    exit /b 1
)
findstr /C:"expected 1, actual 2" release\tmp\sem_channel_generic_arity.err.txt >nul
if errorlevel 1 (
    echo sem_channel_generic_arity.xl missing channel generic argument count context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_ctor_arg_count_many.xl >nul 2> release\tmp\sem_channel_ctor_arg_count_many.err.txt
if not errorlevel 1 (
    echo sem_channel_ctor_arg_count_many.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" release\tmp\sem_channel_ctor_arg_count_many.err.txt >nul
if errorlevel 1 (
    echo sem_channel_ctor_arg_count_many.xl missing channel constructor diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_ctor_named_argument.xl >nul 2> release\tmp\sem_channel_ctor_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_ctor_named_argument.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" release\tmp\sem_channel_ctor_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_ctor_named_argument.xl missing channel constructor named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_capacity_type.xl >nul 2> release\tmp\sem_channel_capacity_type.err.txt
if not errorlevel 1 (
    echo sem_channel_capacity_type.xl should fail.
    exit /b 1
)
findstr /L /C:"[sem] channel" release\tmp\sem_channel_capacity_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_capacity_type.xl missing channel capacity diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_receiver_expr.xl >nul 2> release\tmp\sem_channel_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_channel_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"channel receiver" release\tmp\sem_channel_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_channel_receiver_expr.xl missing named receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_send_named_argument.xl >nul 2> release\tmp\sem_channel_send_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_send_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.send" release\tmp\sem_channel_send_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_send_named_argument.xl missing channel send named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_named_argument.xl >nul 2> release\tmp\sem_channel_recv_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.recvInto" release\tmp\sem_channel_recv_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_named_argument.xl missing channel recvInto named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_named_argument.xl >nul 2> release\tmp\sem_channel_try_recv_named_argument.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_named_argument.xl should fail.
    exit /b 1
)
findstr /C:"channel.tryRecvInto" release\tmp\sem_channel_try_recv_named_argument.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_named_argument.xl missing channel tryRecvInto named argument diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_unknown_method.xl >nul 2> release\tmp\sem_channel_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_channel_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown channel method" release\tmp\sem_channel_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_channel_unknown_method.xl missing unknown channel method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_send_type.xl >nul 2> release\tmp\sem_channel_send_type.err.txt
if not errorlevel 1 (
    echo sem_channel_send_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" release\tmp\sem_channel_send_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_send_type.xl missing channel send type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_target_type.xl >nul 2> release\tmp\sem_channel_recv_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_target_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value float" release\tmp\sem_channel_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_target_type.xl missing channel recv target type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_target_type.xl >nul 2> release\tmp\sem_channel_try_recv_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl should fail.
    exit /b 1
)
findstr /C:"channel.tryRecvInto" release\tmp\sem_channel_try_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl missing channel tryRecvInto target type method.
    exit /b 1
)
findstr /C:"target int, value float" release\tmp\sem_channel_try_recv_target_type.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_target_type.xl missing channel tryRecvInto target type context.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_captured_target.xl >nul 2> release\tmp\sem_channel_recv_captured_target.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_captured_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_channel_recv_captured_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_captured_target.xl missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_captured_target.xl >nul 2> release\tmp\sem_channel_try_recv_captured_target.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_captured_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_channel_try_recv_captured_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_captured_target.xl missing semantic diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_const_target.xl >nul 2> release\tmp\sem_channel_recv_const_target.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_const_target.xl should fail.
    exit /b 1
)
findstr /C:"const" release\tmp\sem_channel_recv_const_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_recv_const_target.xl missing const target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_try_recv_const_target.xl >nul 2> release\tmp\sem_channel_try_recv_const_target.err.txt
if not errorlevel 1 (
    echo sem_channel_try_recv_const_target.xl should fail.
    exit /b 1
)
findstr /C:"const" release\tmp\sem_channel_try_recv_const_target.err.txt >nul
if errorlevel 1 (
    echo sem_channel_try_recv_const_target.xl missing const target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_class_element.xl >nul 2> release\tmp\sem_channel_class_element.err.txt
if not errorlevel 1 (
    echo sem_channel_class_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_class_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_class_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_function_element.xl >nul 2> release\tmp\sem_channel_function_element.err.txt
if not errorlevel 1 (
    echo sem_channel_function_element.xl should fail.
    exit /b 1
)
findstr /C:"channel<T>" release\tmp\sem_channel_function_element.err.txt >nul
if errorlevel 1 (
    echo sem_channel_function_element.xl missing channel element diagnostic.
    exit /b 1
)

release\xl.exe xapi test\cases\xapi_doc.xl --emit release\tmp\xapi_doc.json
if errorlevel 1 exit /b 1

findstr /C:"\"doc\"" release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing doc object.
    exit /b 1
)

findstr /C:"Add two integers." release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing desc text.
    exit /b 1
)

findstr /C:"auto value = add(1, 2)" release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing example text.
    exit /b 1
)

release\xl.exe xapi test\cases\xapi_json_escape.xl --emit release\tmp\xapi_json_escape.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\xapi_json_escape.json' -Raw | ConvertFrom-Json; $q=[char]34; $fields=$j.exports[0].doc.fields; $desc=($fields | Where-Object { $_.kind -eq 'desc' } | Select-Object -First 1).text; $example=($fields | Where-Object { $_.kind -eq 'example' } | Select-Object -First 1).text; if($desc -ne ('Quote '+$q+'demo'+$q+' and path c:\tmp\file.')){exit 1}; if($example -ne ('print('+$q+'hello\nworld'+$q+')')){exit 2}"
if errorlevel 1 (
    echo xapi_json_escape.json failed JSON escape roundtrip.
    exit /b 1
)

release\xl.exe check test\cases\sem_throw_int.xl >nul 2> release\tmp\sem_throw_int.err.txt
if not errorlevel 1 (
    echo sem_throw_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_rethrow_outside_catch.xl >nul 2> release\tmp\sem_rethrow_outside_catch.err.txt
if not errorlevel 1 (
    echo sem_rethrow_outside_catch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_assert_message_int.xl >nul 2> release\tmp\sem_assert_message_int.err.txt
if not errorlevel 1 (
    echo sem_assert_message_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_without_handler.xl >nul 2> release\tmp\sem_try_without_handler.err.txt
if not errorlevel 1 (
    echo sem_try_without_handler.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_error_unknown_member.xl >nul 2> release\tmp\sem_error_unknown_member.err.txt
if not errorlevel 1 (
    echo sem_error_unknown_member.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_error_unknown_member.err.txt >nul
if not errorlevel 1 (
    echo sem_error_unknown_member.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_return_boundary.xl >nul 2> release\tmp\sem_try_return_boundary.err.txt
if not errorlevel 1 (
    echo sem_try_return_boundary.xl should fail.
    exit /b 1
)
findstr /C:"return/break/continue" release\tmp\sem_try_return_boundary.err.txt >nul
if errorlevel 1 (
    echo sem_try_return_boundary.xl missing abrupt-control boundary diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_unknown_field.xl >nul 2> release\tmp\sem_doc_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_doc_unknown_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_naked_text.xl >nul 2> release\tmp\sem_doc_naked_text.err.txt
if not errorlevel 1 (
    echo sem_doc_naked_text.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_bad_param.xl >nul 2> release\tmp\sem_doc_bad_param.err.txt
if not errorlevel 1 (
    echo sem_doc_bad_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_duplicate_param.xl >nul 2> release\tmp\sem_doc_duplicate_param.err.txt
if not errorlevel 1 (
    echo sem_doc_duplicate_param.xl should fail.
    exit /b 1
)

release\xl.exe lex test\cases\invalid_range.xl > release\tmp\invalid_range.tokens.txt 2> release\tmp\invalid_range.err.txt
if not errorlevel 1 (
    echo invalid_range.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" release\tmp\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing ERROR token.
    exit /b 1
)
findstr /C:"range(...)" release\tmp\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing range diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_0X.xl >nul 2> release\tmp\parse_invalid_int_prefix_0X.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_0X.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_0X.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_0X.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_0O.xl >nul 2> release\tmp\parse_invalid_int_prefix_0O.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_0O.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_0O.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_0O.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_0B.xl >nul 2> release\tmp\parse_invalid_int_prefix_0B.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_0B.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_0B.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_0B.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_h_lower.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_h_lower.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_h_lower.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_h_lower.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_h_lower.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_H_upper.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_H_upper.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_H_upper.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_H_upper.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_H_upper.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_o_lower.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_o_lower.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_o_lower.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_o_lower.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_o_lower.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_O_upper.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_O_upper.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_O_upper.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_O_upper.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_O_upper.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_b_lower.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_b_lower.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_b_lower.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_b_lower.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_b_lower.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_int_prefix_amp_B_upper.xl >nul 2> release\tmp\parse_invalid_int_prefix_amp_B_upper.err.txt
if not errorlevel 1 (
    echo parse_invalid_int_prefix_amp_B_upper.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_invalid_int_prefix_amp_B_upper.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_int_prefix_amp_B_upper.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_function_arrow_removed.xl >nul 2> release\tmp\parse_function_arrow_removed.err.txt
if not errorlevel 1 (
    echo parse_function_arrow_removed.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" release\tmp\parse_function_arrow_removed.err.txt >nul
if errorlevel 1 (
    echo parse_function_arrow_removed.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\parse_fstring_bad_brace.xl > release\tmp\parse_fstring_bad_brace.ast.txt 2> release\tmp\parse_fstring_bad_brace.err.txt
if not errorlevel 1 (
    echo parse_fstring_bad_brace.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\parse_fstring_bad_brace.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x5355)+([char]0x72EC)+([char]0x7684)+' }'; if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_bad_brace.err.txt missing bad brace diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\parse_fstring_empty_expr.xl > release\tmp\parse_fstring_empty_expr.ast.txt 2> release\tmp\parse_fstring_empty_expr.err.txt
if not errorlevel 1 (
    echo parse_fstring_empty_expr.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\parse_fstring_empty_expr.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x4E0D)+([char]0x80FD)+([char]0x4E3A)+([char]0x7A7A); if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_empty_expr.err.txt missing empty expr diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\parse_fstring_unclosed.xl > release\tmp\parse_fstring_unclosed.ast.txt 2> release\tmp\parse_fstring_unclosed.err.txt
if not errorlevel 1 (
    echo parse_fstring_unclosed.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\parse_fstring_unclosed.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x672A)+([char]0x95ED)+([char]0x5408)+([char]0x7684)+' f-string '+([char]0x63D2)+([char]0x503C)+([char]0x8868)+([char]0x8FBE)+([char]0x5F0F); if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_unclosed.err.txt missing unclosed expr diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: async generator channel docs f-string negatives
exit /b 0

