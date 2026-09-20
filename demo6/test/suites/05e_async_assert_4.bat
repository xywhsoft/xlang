@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async cgen assertions 4/4...

if not "%XL6_ASYNC_CGEN_READY%"=="1" (
    release\xl.exe cgen test\cases\cgen_async.xl --emit build\test\cgen_async.c
    if errorlevel 1 exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.c',[System.Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [System.IO.File]::WriteAllText('build\test\cgen_async.assert.c',$text,(New-Object System.Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"pendingAsyncSetValueB = xl6_xrt_typed_set_copy(xl6TypeInt64(), (*(xtset*)xl6_future_await_value(pendingAsyncSet" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed set await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncSetStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed set future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncDict = xl_fn_slowMakeDictAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed dict cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncDict, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed dict future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncDictValueA = xl6_xrt_typed_dict_copy(xl6TypeInt64(), (*(xtdict*)xl6_future_await_value(pendingAsyncDict" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed dict await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncDictValueB = xl6_xrt_typed_dict_copy(xl6TypeInt64(), (*(xtdict*)xl6_future_await_value(pendingAsyncDict" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed dict await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncDictStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed dict future canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.assert.c',[System.Text.Encoding]::UTF8); $hs=$text.IndexOf('static void* xl6_future_await_handle_release'); $he=$text.IndexOf('static int xl6_future_await_void_release',$hs); $ss=$text.IndexOf('static const char* xl6_future_await_cstr_release'); $se=$text.IndexOf('static const char* xl6_future_await_cstr_copy',$ss); if($hs -lt 0 -or $he -le $hs -or $ss -lt 0 -or $se -le $ss){exit 1}; $handle=$text.Substring($hs,$he-$hs); $string=$text.Substring($ss,$se-$ss); if(-not $handle.Contains('valueType->Ops->copy(&result, value)') -or -not $handle.Contains('return result;') -or -not $string.Contains('return xl6_temp_string_take((char*)result, 0);')){exit 1}"
if errorlevel 1 (
    echo cgen_async.c has invalid handle/string await ownership separation.
    exit /b 1
)
findstr /C:"__xl6_args->name = xl6_xrt_string_dup(name, 0u);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing owned string argument frame copy.
    exit /b 1
)
findstr /C:"const char* name = xl6_temp_string_take((char*)__xl6_args->name, 0u);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing worker string argument ownership transfer.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_string_value;" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing owned async string result destructor.
    exit /b 1
)
findstr /C:"result = result != 0 ? xl6_xrt_string_dup(result, 0) : \"\";" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
	echo cgen_async.c missing temporary string await copy.
    exit /b 1
)
findstr /C:"xl6_future_await_handle_release(xl_fn_makeItemsAsync()" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await typed container helper call.
    exit /b 1
)
findstr /C:"xtarray __xl6_async_return_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed container async return storage.
    exit /b 1
)
findstr /C:"xtlist __xl6_async_return_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed list async return storage.
    exit /b 1
)
findstr /C:"xtset __xl6_async_return_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed set async return storage.
    exit /b 1
)
findstr /C:"xtdict __xl6_async_return_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed dict async return storage.
    exit /b 1
)
findstr /C:"xl6TypedArrayCreate(xl6TypeInt64())" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing typed container async shared create.
    exit /b 1
)
findstr /C:"valueType->Ops->copy(&result, value)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
	echo cgen_async.c missing temporary typed container await copy.
    exit /b 1
)
findstr /R /C:"xtarray __xl6_async_return_[0-9][0-9]* = xl6TypedArrayRetain(values);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing constant-time typed container async retain.
    exit /b 1
)
findstr /C:"itemsFuture = xl_fn_makeItemsAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing named future typed container fixture.
    exit /b 1
)
findstr /C:"namedItemsA = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed container copy for first await.
    exit /b 1
)
findstr /C:"namedItemsB = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed container copy for second await.
    exit /b 1
)
findstr /C:"namedListA = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed list copy for first await.
    exit /b 1
)
findstr /C:"namedListB = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed list copy for second await.
    exit /b 1
)
findstr /C:"namedSetA = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed set copy for first await.
    exit /b 1
)
findstr /C:"namedSetB = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed set copy for second await.
    exit /b 1
)
findstr /C:"namedDictA = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed dict copy for first await.
    exit /b 1
)
findstr /C:"namedDictB = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing borrowed named future typed dict copy for second await.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(itemsFuture, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed array future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelItems = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed array future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(listFuture, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed list future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelList = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed list future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(setFuture, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed set future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelSet = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed set future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(dictFuture, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async typed dict future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelDict = __xl6_global_init_" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async typed dict future awaitable after cancel.
    exit /b 1
)
findstr /C:"long long temporaryValue = ({ xfuture*" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary scalar await.
    exit /b 1
)
findstr /C:"XL6_Function temporaryFn = ({ xfuture*" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary function await.
    exit /b 1
)
findstr /C:"({ xfuture* __xl6_await_future_" build\test\cgen_async.assert.c | findstr /C:"doneAsync" >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary void await.
    exit /b 1
)
findstr /C:"static xvalue* xl6_future_await_xvalue_release" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class-handle release helper.
    exit /b 1
)
findstr /C:"temporaryBox = xl6_future_await_xvalue_release(xl_fn_makeBoxAsync(110LL)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class-handle helper call.
    exit /b 1
)
findstr /C:"if ( result != 0 ) { xrtValueRetain(result); }" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await class-handle retain.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.assert.c',[System.Text.Encoding]::UTF8); $start=$text.IndexOf('static void* xl6_async_zero_value'); $end=$text.IndexOf('static void* xl6_future_await_value',$start); if($start -lt 0 -or $end -le $start){exit 1}; $helper=$text.Substring($start,$end-$start); if(-not $helper.Contains('xrtTemp(valueSize)') -or $helper.Contains('xrtMalloc(valueSize)')){exit 1}"
if errorlevel 1 (
    echo cgen_async.c await failure placeholder must use XRT temporary memory.
    exit /b 1
)
findstr /C:"xl6_future_release_if_needed(pFuture)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing temporary await helper future release.
    exit /b 1
)
findstr /C:"temporaryBox = ({ xfuture*" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary class await.
    exit /b 1
)
findstr /C:"xvalue temporaryVar = ({ xfuture*" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use statement expression for temporary xvalue await.
    exit /b 1
)
findstr /C:"__xl6_await_future_" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not use temporary await future block.
    exit /b 1
)
findstr /C:"temporaryFn.retain(temporaryFn.env)" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not double-retain temporary await function result.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(temporaryBox)" build\test\cgen_async.assert.c >nul
if not errorlevel 1 (
    echo cgen_async.c should not double-retain temporary await class-handle result.
    exit /b 1
)

echo DEMO6 suite ok: async cgen assertions 4/4
exit /b 0
