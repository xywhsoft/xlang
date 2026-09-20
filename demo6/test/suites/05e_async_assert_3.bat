@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async cgen assertions 3/4...

if not "%XL6_ASYNC_CGEN_READY%"=="1" (
    release\xl.exe cgen test\cases\cgen_async.xl --emit build\test\cgen_async.c
    if errorlevel 1 exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_async.c',[System.Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [System.IO.File]::WriteAllText('build\test\cgen_async.assert.c',$text,(New-Object System.Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"pendingAsyncIntStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async int future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncString = xl_fn_slowGreetAsync(\"pending\");" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async string cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncString, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async string future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncStringValueA = xl6_xrt_string_dup(xl6_future_await_cstr_copy(pendingAsyncString" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first string await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncStringValueB = xl6_xrt_string_dup(xl6_future_await_cstr_copy(pendingAsyncString" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late string await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncStringStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async string future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncBool = xl_fn_slowFlipAsync(1);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async bool cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncBool, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async bool future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncBoolValueA = (*(int*)xl6_future_await_value(pendingAsyncBool" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first bool await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncBoolValueB = (*(int*)xl6_future_await_value(pendingAsyncBool" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late bool await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncBoolStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async bool future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncFloat = xl_fn_slowHalfAsync(1.5);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async float cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncFloat, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async float future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncFloatValueA = (*(double*)xl6_future_await_value(pendingAsyncFloat" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first float await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFloatValueB = (*(double*)xl6_future_await_value(pendingAsyncFloat" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late float await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncFloatStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async float future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncVoid = xl_fn_slowDoneAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async void cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncVoid, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async void future as borrowed.
    exit /b 1
)
findstr /C:"xl6_future_await_value(pendingAsyncVoid" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing void await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncVoidStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async void future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncVar = xl_fn_slowMakeVarAsync(150LL);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async var cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncVar, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async var future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncVarValueA = xl6_future_await_xvalue_copy(pendingAsyncVar" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first var await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncVarValueB = xl6_future_await_xvalue_copy(pendingAsyncVar" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late var await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncVarStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async var future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncFn = xl_fn_slowMakeAdderAsync(160LL);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async function cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncFn, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async function future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueA = (*(XL6_Function*)xl6_future_await_value(pendingAsyncFn" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first function await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueA.retain(pendingAsyncFnValueA.env)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first function await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncFnValueB = (*(XL6_Function*)xl6_future_await_value(pendingAsyncFn" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late function await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncFnStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async function future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncBox = xl_fn_slowMakeBoxAsync(170LL);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async class cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncBox, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async class future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncBoxValueA = xl6_future_await_xvalue_copy(pendingAsyncBox" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first class-handle await after pending async cancel.
    exit /b 1
)
findstr /C:"xrtValueRelease(pendingAsyncBoxValueA);" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first class-handle await cleanup.
    exit /b 1
)
findstr /C:"pendingAsyncBoxValueB = xl6_future_await_xvalue_copy(pendingAsyncBox" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late class-handle await after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncBoxStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async class future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncItems = xl_fn_slowMakeItemsAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed array cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncItems, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed array future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncItemsValueA = xl6_xrt_typed_array_copy(xl6TypeInt64(), (*(xtarray*)xl6_future_await_value(pendingAsyncItems" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed array await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncItemsValueB = xl6_xrt_typed_array_copy(xl6TypeInt64(), (*(xtarray*)xl6_future_await_value(pendingAsyncItems" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed array await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncItemsStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed array future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncList = xl_fn_slowMakeListAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed list cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncList, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed list future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncListValueA = xl6_xrt_typed_list_copy(xl6TypeInt64(), (*(xtlist*)xl6_future_await_value(pendingAsyncList" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed list await retain after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncListValueB = xl6_xrt_typed_list_copy(xl6TypeInt64(), (*(xtlist*)xl6_future_await_value(pendingAsyncList" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late typed list await retain after pending async worker completion.
    exit /b 1
)
findstr /C:"pendingAsyncListStateB == 42LL" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep pending async typed list future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingAsyncSet = xl_fn_slowMakeSetAsync();" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async typed set cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncSet, 0)" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async typed set future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncSetValueA = xl6_xrt_typed_set_copy(xl6TypeInt64(), (*(xtset*)xl6_future_await_value(pendingAsyncSet" build\test\cgen_async.assert.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first typed set await retain after pending async cancel.
    exit /b 1
)

echo DEMO6 suite ok: async cgen assertions 3/4
exit /b 0
