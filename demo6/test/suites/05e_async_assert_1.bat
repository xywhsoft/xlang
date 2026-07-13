@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async cgen assertions 1/4...

if not "%XL6_ASYNC_CGEN_READY%"=="1" (
    release\xl.exe cgen test\cases\cgen_async.xl --emit build\test\cgen_async.c
    if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_async_require_future\([\s\S]*?if \( pFuture != 0 \) \{ return pFuture; \}[\s\S]*?if \( xl6_error_has\(\) \) \{ return 0; \}[\s\S]*?async task start failed'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async.c missing async start active error preservation.
    exit /b 1
)
findstr /C:"xl6_future_await_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await helper.
    exit /b 1
)
findstr /C:"xFutureRelease" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing future cleanup.
    exit /b 1
)
findstr /C:"__xl6_async_return_" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async return temporary.
    exit /b 1
)
findstr /C:".op.retain != 0" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async class function-field retain.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_async_worker_xl_fn_makeBoxAsync[\s\S]*?pOut->pfnFreeValue = xl6_async_free_xvalue_value'){ exit 1 }"
if errorlevel 1 (
    echo cgen_async.c missing async class-handle xvalue free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_function_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async function result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_xvalue_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async xvalue result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_array_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed array result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_list_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed list result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_set_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed set result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_dict_value" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async typed dict result free hook.
    exit /b 1
)
findstr /C:"fnA.retain(fnA.env)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await function retain.
    exit /b 1
)
findstr /C:"secondBox = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(sharedBoxFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing repeated await class-handle retain.
    exit /b 1
)
findstr /C:"autoBox = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(autoBoxFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class-handle retain.
    exit /b 1
)
findstr /C:"xvoUnref(autoBox);" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class-handle cleanup.
    exit /b 1
)
findstr /C:"temporaryValue = xl6_future_await_i64_release(xl_fn_addAsync(8LL, 9LL)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await scalar release helper.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl_fn_doneAsync()" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await void release helper.
    exit /b 1
)
findstr /C:"temporaryFn = xl6_future_await_function_release(xl_fn_makeAdderAsync(60LL)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await function release helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_future_await_xvalue_release" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await xvalue release helper.
    exit /b 1
)
findstr /C:"awaitedVar = xl6_xrt_move_to_shared(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(varFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed await xvalue retain.
    exit /b 1
)
findstr /C:"temporaryVar = xl6_xrt_move_to_shared(xl6_future_await_xvalue_release(xl_fn_makeVarAsync(130LL)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await xvalue helper call.
    exit /b 1
)
findstr /C:"static xfuture* xl6_future_add_ref_if_needed" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await future add-ref helper.
    exit /b 1
)
findstr /C:"conditionalAwaitBorrowed = xl6_future_await_i64_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalAwaitSource) : xl_fn_addAsync(310LL, 1LL))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed scalar normalization.
    exit /b 1
)
findstr /C:"conditionalAwaitOwned = xl6_future_await_i64_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalAwaitSource) : xl_fn_addAsync(320LL, 1LL))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned scalar normalization.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalVoidSource) : xl_fn_doneAsync())" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await void normalization.
    exit /b 1
)
findstr /C:"conditionalVarBorrowed = xl6_xrt_move_to_shared(xl6_future_await_xvalue_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalVarSource) : xl_fn_makeVarAsync(340LL))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed var normalization.
    exit /b 1
)
findstr /C:"conditionalFnBorrowed = xl6_future_await_function_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalFnSource) : xl_fn_makeAdderAsync(370LL))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed function normalization.
    exit /b 1
)
findstr /C:"conditionalBoxBorrowed = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_future_await_xvalue_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalBoxSource)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed class-handle release helper.
    exit /b 1
)
findstr /C:"conditionalBoxOwned = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_future_await_xvalue_release((chooseConditionalAwait ? xl6_future_add_ref_if_needed(conditionalBoxSource) : xl_fn_makeBoxAsync(395LL))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned class-handle release helper.
    exit /b 1
)
findstr /C:"conditionalGreetingBorrowed = xrtCopyStr((chooseConditionalGreeting ? xl6_future_await_cstr_copy(conditionalGreetingSource" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed string copy.
    exit /b 1
)
findstr /C:": xl6_future_await_cstr_release(xl_fn_greetAsync(" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned string release.
    exit /b 1
)
findstr /C:"chooseConditionalItems ? xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(conditionalItemsSource" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed array copy.
    exit /b 1
)
findstr /C:": (xtarray)xl6_future_await_handle_release(xl_fn_slowMakeItemsAsync()" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed array release.
    exit /b 1
)
findstr /C:"chooseConditionalList ? xl6_xrt_typed_list_copy_mode(xrtTypeInt(), (*(xtlist*)xl6_future_await_value(conditionalListSource" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed list copy.
    exit /b 1
)
findstr /C:": (xtlist)xl6_future_await_handle_release(xl_fn_slowMakeListAsync()" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed list release.
    exit /b 1
)
findstr /C:"chooseConditionalSet ? xl6_xrt_typed_set_copy_mode(xrtTypeInt(), (*(xtset*)xl6_future_await_value(conditionalSetSource" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed set copy.
    exit /b 1
)
findstr /C:": (xtset)xl6_future_await_handle_release(xl_fn_slowMakeSetAsync()" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed set release.
    exit /b 1
)
findstr /C:"chooseConditionalDict ? xl6_xrt_typed_dict_copy_mode(xrtTypeInt(), (*(xtdict*)xl6_future_await_value(conditionalDictSource" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await borrowed typed dict copy.
    exit /b 1
)
findstr /C:": (xtdict)xl6_future_await_handle_release(xl_fn_slowMakeDictAsync()" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await owned typed dict release.
    exit /b 1
)
findstr /C:"conditionalMissingAwaitValue = xl6_future_await_i64_release((chooseConditionalMissingAwait ? xl_fn_addAsync(390LL, 1LL) : xl6_future_add_ref_if_needed(conditionalMissingAwait))" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing conditional await null branch normalization.
    exit /b 1
)
findstr /C:"static const char* xl6_future_await_cstr_copy" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing named string await copy helper.
    exit /b 1
)
findstr /C:"temporaryGreeting = xrtCopyStr(xl6_future_await_cstr_release(xl_fn_greetAsync(" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await string release helper call.
    exit /b 1
)
findstr /C:"namedGreetingA = xrtCopyStr(xl6_future_await_cstr_copy(greetingFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first named future string copy.
    exit /b 1
)
findstr /C:"namedGreetingB = xrtCopyStr(xl6_future_await_cstr_copy(greetingFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing second named future string copy.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(futureValue, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async int future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelValue = (*(long long*)xl6_future_await_value(futureValue" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async int future awaitable after cancel.
    exit /b 1
)

echo DEMO6 suite ok: async cgen assertions 1/4
exit /b 0
