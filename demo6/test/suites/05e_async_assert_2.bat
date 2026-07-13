@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async cgen assertions 2/4...

if not "%XL6_ASYNC_CGEN_READY%"=="1" (
    release\xl.exe cgen test\cases\cgen_async.xl --emit build\test\cgen_async.c
    if errorlevel 1 exit /b 1
)

findstr /C:"xl6_task_cancel_value(voidFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async void future cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(boolFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async bool future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelBool = (*(int*)xl6_future_await_value(boolFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async bool future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(halfFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async float future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelHalf = (*(double*)xl6_future_await_value(halfFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async float future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(boxFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async class future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelBox = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(boxFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should retain the completed async class handle after cancel.
    exit /b 1
)
findstr /C:"xvoUnref(postCancelBox);" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing post-cancel async class-handle cleanup.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(fnFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async function future cancel as borrowed.
    exit /b 1
)
findstr /C:"fnC = (*(XL6_Function*)xl6_future_await_value(fnFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async function future awaitable after cancel.
    exit /b 1
)
findstr /C:"fnC.retain(fnC.env)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing post-cancel async function retain.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(varFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async var future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelVar = xl6_xrt_move_to_shared(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(varFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async var future awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(greetingFuture, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should pass completed async string future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelGreeting = xrtCopyStr(xl6_future_await_cstr_copy(greetingFuture" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should keep completed async string future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitInt;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null int future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitInt = 0;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null int future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitIntValue = (*(long long*)xl6_future_await_value(missingAwaitInt" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null int future await path.
    exit /b 1
)
findstr /C:"missingAwaitStringValue = xrtCopyStr(xl6_future_await_cstr_copy(missingAwaitString" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null string future await path.
    exit /b 1
)
findstr /C:"missingAwaitItemsValue = xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(missingAwaitItems" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed array future await path.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitBool;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitBoolValue = (*(int*)xl6_future_await_value(missingAwaitBool" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await path.
    exit /b 1
)
findstr /C:"missingAwaitBoolState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null bool future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitFloat;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitFloatValue = (*(double*)xl6_future_await_value(missingAwaitFloat" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await path.
    exit /b 1
)
findstr /C:"missingAwaitFloatState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null float future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitVar;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitVarValue = xl6_xrt_retain((*(xvalue*)xl6_future_await_value(missingAwaitVar" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await path.
    exit /b 1
)
findstr /C:"missingAwaitVarState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null var future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitFn;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitFnValue = (*(XL6_Function*)xl6_future_await_value(missingAwaitFn" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await path.
    exit /b 1
)
findstr /C:"missingAwaitFnState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null function future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitBox;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitBoxValue = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(missingAwaitBox" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class-handle future await path.
    exit /b 1
)
findstr /C:"missingAwaitBoxState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null class future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitList;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitListValue = xrtTypedListRetain((*(xtlist*)xl6_future_await_value(missingAwaitList" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await path.
    exit /b 1
)
findstr /C:"missingAwaitListState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed list future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitSet;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitSetValue = xrtTypedSetRetain((*(xtset*)xl6_future_await_value(missingAwaitSet" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await path.
    exit /b 1
)
findstr /C:"missingAwaitSetState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed set future await assertion.
    exit /b 1
)
findstr /C:"xfuture* missingAwaitDict;" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await fixture.
    exit /b 1
)
findstr /C:"missingAwaitDictValue = xrtTypedDictRetain((*(xtdict*)xl6_future_await_value(missingAwaitDict" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await path.
    exit /b 1
)
findstr /C:"missingAwaitDictState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed dict future await assertion.
    exit /b 1
)
findstr /C:"xrtStrComp(xl6_error_message(e), \"await on null future\"" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null future await diagnostic check.
    exit /b 1
)
findstr /C:"missingAwaitItemsState == 42LL" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing null typed array future await assertion.
    exit /b 1
)
findstr /C:"pendingAsyncInt = xl_fn_slowAddAsync(40LL, 2LL);" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing pending async int cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingAsyncInt, 0)" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c should cancel pending async int future as borrowed.
    exit /b 1
)
findstr /C:"pendingAsyncIntValueA = (*(long long*)xl6_future_await_value(pendingAsyncInt" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing first int await after pending async cancel.
    exit /b 1
)
findstr /C:"pendingAsyncIntValueB = (*(long long*)xl6_future_await_value(pendingAsyncInt" build\test\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing late int await after pending async worker completion.
    exit /b 1
)

echo DEMO6 suite ok: async cgen assertions 2/4
exit /b 0
