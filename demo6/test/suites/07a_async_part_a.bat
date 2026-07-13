@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async task and async-generator part 1/3...

release\xl.exe check test\cases\cgen_async_class_conditional_await.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async_class_conditional_await.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_async_class_conditional_await.xl --emit build\test\cgen_async_class_conditional_await.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_future_await_xvalue_release" build\test\cgen_async_class_conditional_await.c >nul
if errorlevel 1 (
    echo cgen_async_class_conditional_await.c missing managed class await release helper.
    exit /b 1
)
findstr /C:"xl6_future_add_ref_if_needed(missingConditionalAwaitBox)" build\test\cgen_async_class_conditional_await.c >nul
if errorlevel 1 (
    echo cgen_async_class_conditional_await.c missing borrowed/null future add-ref guard.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async_exception_propagation.xl -o build\test\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

build\test\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

for /L %%I in (1,1,5) do (
    build\test\cgen_async_exception_propagation.exe
    if errorlevel 1 (
        echo cgen_async_exception_propagation.exe repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe cgen test\cases\cgen_async_exception_propagation.xl --emit build\test\cgen_async_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"async boom\"" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async throw raise.
    exit /b 1
)

findstr /C:"xl6_future_await_value" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing await helper.
    exit /b 1
)

findstr /C:"xl6_future_await_value(failed" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing borrowed named future await.
    exit /b 1
)

findstr /C:"long long secondValue = (*(long long*)xl6_future_await_value(failed" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing repeated await for failed named future.
    exit /b 1
)

findstr /C:"cancelFailed = xl6_task_cancel_value(failed, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed future cancel as borrowed.
    exit /b 1
)

findstr /C:"xl6_future_await_i64_release(xl_fn_failAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing owned temporary future await release.
    exit /b 1
)

findstr /C:"xl6_future_await_value(failedVoid" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed void named future await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedVoid, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed void future cancel as borrowed.
    exit /b 1
)

findstr /C:"xl6_future_await_void_release(xl_fn_failVoidAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed void await release.
    exit /b 1
)

findstr /C:"failedStringValueA = xrtCopyStr(xl6_future_await_cstr_copy(failedString" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed string first await copy.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedString, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed string future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedStringValueB = xrtCopyStr(xl6_future_await_cstr_copy(failedString" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed string repeated await copy.
    exit /b 1
)

findstr /C:"failedBoolValueA = (*(int*)xl6_future_await_value(failedBool" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed bool first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedBool, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed bool future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedBoolValueB = (*(int*)xl6_future_await_value(failedBool" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed bool repeated await.
    exit /b 1
)

findstr /C:"failedFloatValueA = (*(double*)xl6_future_await_value(failedFloat" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed float first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedFloat, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed float future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedFloatValueB = (*(double*)xl6_future_await_value(failedFloat" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed float repeated await.
    exit /b 1
)

findstr /C:"failedVarValueA = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedVar" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed var first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedVar, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed var future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedVarValueB = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedVar" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed var repeated await retain.
    exit /b 1
)

findstr /C:"failedFunctionValueA = (*(XL6_Function*)xl6_future_await_value(failedFunction" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function first await.
    exit /b 1
)

findstr /C:"failedFunctionValueA.retain(failedFunctionValueA.env)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedFunction, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed function future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedFunctionValueB = (*(XL6_Function*)xl6_future_await_value(failedFunction" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function repeated await.
    exit /b 1
)

findstr /C:"failedFunctionValueB.retain(failedFunctionValueB.env)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed function repeated await retain.
    exit /b 1
)

findstr /C:"failedBoxValueA = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedBox" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed class first await.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedBox, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed class future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedBoxValueB = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedBox" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed class repeated await.
    exit /b 1
)

findstr /C:"failedItemsValueA = xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(failedItems" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed array first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedItems, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed array future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedItemsValueB = xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(failedItems" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed array repeated await retain.
    exit /b 1
)

findstr /C:"failedListValueA = xrtTypedListRetain((*(xtlist*)xl6_future_await_value(failedList" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed list first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedList, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed list future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedListValueB = xrtTypedListRetain((*(xtlist*)xl6_future_await_value(failedList" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed list repeated await retain.
    exit /b 1
)

findstr /C:"failedSetValueA = xrtTypedSetRetain((*(xtset*)xl6_future_await_value(failedSet" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed set first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedSet, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed set future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedSetValueB = xrtTypedSetRetain((*(xtset*)xl6_future_await_value(failedSet" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed set repeated await retain.
    exit /b 1
)

findstr /C:"failedDictValueA = xrtTypedDictRetain((*(xtdict*)xl6_future_await_value(failedDict" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed dict first await retain.
    exit /b 1
)

findstr /C:"xl6_task_cancel_value(failedDict, 0)" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c should pass failed typed dict future cancel as borrowed.
    exit /b 1
)

findstr /C:"failedDictValueB = xrtTypedDictRetain((*(xtdict*)xl6_future_await_value(failedDict" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing failed typed dict repeated await retain.
    exit /b 1
)

findstr /C:"tempStringValue = xrtCopyStr(xl6_future_await_cstr_release(xl_fn_failStringAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed string await release.
    exit /b 1
)

findstr /C:"tempItemsValue = xl6_future_await_handle_release(xl_fn_failItemsAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed array await release.
    exit /b 1
)

findstr /C:"tempListValue = xl6_future_await_handle_release(xl_fn_failListAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed list await release.
    exit /b 1
)

findstr /C:"tempSetValue = xl6_future_await_handle_release(xl_fn_failSetAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed set await release.
    exit /b 1
)

findstr /C:"tempDictValue = xl6_future_await_handle_release(xl_fn_failDictAsync()" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing temporary failed typed dict await release.
    exit /b 1
)

findstr /C:"static int xl_async_worker_xl_fn_asyncFinallyCatchAsync" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally worker.
    exit /b 1
)

findstr /C:"goto __xl6_try_finally_" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally control path.
    exit /b 1
)

findstr /C:"xl6_error_raise(\"Error\", \"async finally override boom\"" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally override error.
    exit /b 1
)

findstr /C:"xl6_error_raise(\"Error\", \"async finally void override boom\"" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally override error.
    exit /b 1
)

findstr /C:"finallyCatchRepeated = (*(long long*)xl6_future_await_value(finallyCatchFuture" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally repeated await.
    exit /b 1
)

findstr /C:"finallyOverrideState = 84LL" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async finally repeated error state.
    exit /b 1
)

findstr /C:"xl6_future_await_value(finallyVoidFuture" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally await.
    exit /b 1
)

findstr /C:"finallyVoidState = 84LL" build\test\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async void finally repeated error state.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_non_future.xl >nul 2> build\test\sem_await_non_future.err.txt
if not errorlevel 1 (
    echo sem_await_non_future.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_receiver_expr.xl >nul 2> build\test\sem_future_receiver_expr.err.txt
if not errorlevel 1 (
    echo sem_future_receiver_expr.xl should fail.
    exit /b 1
)
findstr /C:"future receiver" build\test\sem_future_receiver_expr.err.txt >nul
if errorlevel 1 (
    echo sem_future_receiver_expr.xl missing future receiver diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_unknown_method.xl >nul 2> build\test\sem_future_unknown_method.err.txt
if not errorlevel 1 (
    echo sem_future_unknown_method.xl should fail.
    exit /b 1
)
findstr /C:"unknown future method" build\test\sem_future_unknown_method.err.txt >nul
if errorlevel 1 (
    echo sem_future_unknown_method.xl missing future method diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_future_wait_timeout_type.xl >nul 2> build\test\sem_future_wait_timeout_type.err.txt
if not errorlevel 1 (
    echo sem_future_wait_timeout_type.xl should fail.
    exit /b 1
)
findstr /C:"future.waitTimeout" build\test\sem_future_wait_timeout_type.err.txt >nul
if errorlevel 1 (
    echo sem_future_wait_timeout_type.xl missing waitTimeout type diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_sync_func.xl >nul 2> build\test\sem_await_sync_func.err.txt
if not errorlevel 1 (
    echo sem_await_sync_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_sync_generator.xl >nul 2> build\test\sem_await_sync_generator.err.txt
if not errorlevel 1 (
    echo sem_await_sync_generator.xl should fail.
    exit /b 1
)
findstr /C:"await can only be used" build\test\sem_await_sync_generator.err.txt >nul
if errorlevel 1 (
    echo sem_await_sync_generator.xl missing await sync generator diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_future.xl >nul 2> build\test\sem_async_return_future.err.txt
if not errorlevel 1 (
    echo sem_async_return_future.xl should fail.
    exit /b 1
)
findstr /C:"future/task" build\test\sem_async_return_future.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_future.xl should report future/task return diagnostic.
    exit /b 1
)
findstr /C:"target future" build\test\sem_async_return_future.err.txt >nul
if not errorlevel 1 (
    echo sem_async_return_future.xl should not report return mismatch cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_task.xl >nul 2> build\test\sem_async_return_task.err.txt
if not errorlevel 1 (
    echo sem_async_return_task.xl should fail.
    exit /b 1
)
findstr /C:"future/task" build\test\sem_async_return_task.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_task.xl should report future/task return diagnostic.
    exit /b 1
)
findstr /C:"target task" build\test\sem_async_return_task.err.txt >nul
if not errorlevel 1 (
    echo sem_async_return_task.xl should not report return mismatch cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_channel.xl >nul 2> build\test\sem_async_return_channel.err.txt
if not errorlevel 1 (
    echo sem_async_return_channel.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_return_channel.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_channel.xl missing async channel return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_sync.xl >nul 2> build\test\sem_async_return_sync.err.txt
if not errorlevel 1 (
    echo sem_async_return_sync.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_return_sync.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_sync.xl missing async sync return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_thread.xl >nul 2> build\test\sem_async_return_thread.err.txt
if not errorlevel 1 (
    echo sem_async_return_thread.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_return_thread.err.txt >nul
if errorlevel 1 (
    echo sem_async_return_thread.xl missing async thread return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_future.xl >nul 2> build\test\sem_async_param_future.err.txt
if not errorlevel 1 (
    echo sem_async_param_future.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_param_future.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_future.xl missing async future parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_generator.xl >nul 2> build\test\sem_async_param_generator.err.txt
if not errorlevel 1 (
    echo sem_async_param_generator.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_param_generator.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_generator.xl missing async generator parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_channel.xl >nul 2> build\test\sem_async_param_channel.err.txt
if not errorlevel 1 (
    echo sem_async_param_channel.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_param_channel.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_channel.xl missing async channel parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_sync.xl >nul 2> build\test\sem_async_param_sync.err.txt
if not errorlevel 1 (
    echo sem_async_param_sync.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_param_sync.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_sync.xl missing async sync parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_param_thread.xl >nul 2> build\test\sem_async_param_thread.err.txt
if not errorlevel 1 (
    echo sem_async_param_thread.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" build\test\sem_async_param_thread.err.txt >nul
if errorlevel 1 (
    echo sem_async_param_thread.xl missing async thread parameter diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_multi_return.xl >nul 2> build\test\sem_async_multi_return.err.txt
if not errorlevel 1 (
    echo sem_async_multi_return.xl should fail.
    exit /b 1
)
findstr /C:"multi-return" build\test\sem_async_multi_return.err.txt >nul
if errorlevel 1 (
    echo sem_async_multi_return.xl missing async multi-return diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_unknown_member.xl >nul 2> build\test\sem_task_unknown_member.err.txt
if not errorlevel 1 (
    echo sem_task_unknown_member.xl should fail.
    exit /b 1
)
findstr /C:"unknown task API" build\test\sem_task_unknown_member.err.txt >nul
if errorlevel 1 (
    echo sem_task_unknown_member.xl missing unknown task API diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_non_future.xl >nul 2> build\test\sem_task_cancel_non_future.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_non_future.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" build\test\sem_task_cancel_non_future.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_non_future.xl missing task.cancel diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_channel.xl >nul 2> build\test\sem_task_cancel_channel.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_channel.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" build\test\sem_task_cancel_channel.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_channel.xl missing task.cancel channel diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_task_cancel_generator.xl >nul 2> build\test\sem_task_cancel_generator.err.txt
if not errorlevel 1 (
    echo sem_task_cancel_generator.xl should fail.
    exit /b 1
)
findstr /C:"task.cancel" build\test\sem_task_cancel_generator.err.txt >nul
if errorlevel 1 (
    echo sem_task_cancel_generator.xl missing task.cancel generator diagnostic.
    exit /b 1
)


echo DEMO6 suite ok: async task and async-generator part 1/3
exit /b 0
