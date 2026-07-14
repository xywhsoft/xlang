@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API cgen assertions 2/4...

if not "%XL6_TASK_API_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_api.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
    if errorlevel 1 exit /b 1
)

findstr /C:"cancelRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable task.race group fixture.
    exit /b 1
)
findstr /C:"cancelRaceLeft, 0, cancelRaceRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancelable task.race sources as borrowed.
    exit /b 1
)
findstr /C:"cancelRaceRequested = xl6_task_cancel_value(cancelRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending task.race group as borrowed.
    exit /b 1
)
findstr /C:"cancelRaceRequestedAgain = xl6_task_cancel_value(cancelRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.race group cancel idempotent.
    exit /b 1
)
findstr /C:"cancelRaceGroupState = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.race group state assertion.
    exit /b 1
)
findstr /C:"cancelRaceRequestedAfterAwait = xl6_task_cancel_value(cancelRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.race group cancel idempotent after await.
    exit /b 1
)
findstr /C:"cancelRaceGroupStateAgain = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.race group repeated await canceled.
    exit /b 1
)
findstr /C:"cancelRaceGroupStateAfterCancel = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.race group post-cancel await canceled.
    exit /b 1
)
findstr /C:"cancelRaceLeftValue = (*(long long*)xl6_future_await_value(cancelRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.race left source await.
    exit /b 1
)
findstr /C:"cancelRaceLeftCancelAgain = xl6_task_cancel_value(cancelRaceLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.race source cancel idempotent.
    exit /b 1
)
findstr /C:"cancelRaceLeftLateValue = (*(long long*)xl6_future_await_value(cancelRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled task.race left source await.
    exit /b 1
)
findstr /C:"expect_i64(cancelRaceLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late canceled task.race left source await canceled.
    exit /b 1
)
findstr /C:"cancelRaceRightValue = (*(long long*)xl6_future_await_value(cancelRaceRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.race right source await.
    exit /b 1
)
findstr /C:"cancelDuplicateRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable duplicate task.race group fixture.
    exit /b 1
)
findstr /C:"cancelDuplicateRaceSource, 0, cancelDuplicateRaceSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.race cancel sources as borrowed twice.
    exit /b 1
)
findstr /C:"cancelDuplicateRaceRequested = xl6_task_cancel_value(cancelDuplicateRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.race group as borrowed.
    exit /b 1
)
findstr /C:"cancelDuplicateRaceSourceLate = (*(long long*)xl6_future_await_value(cancelDuplicateRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late duplicate task.race source await after group cancel.
    exit /b 1
)
findstr /C:"temporaryDuplicateRaceRequested = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing temporary duplicate task.race cancel fixture.
    exit /b 1
)
findstr /C:"temporaryDuplicateRaceSource, 0, temporaryDuplicateRaceSource, 0), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary duplicate task.race group as owned with borrowed sources.
    exit /b 1
)
findstr /C:"temporaryDuplicateRaceSourceLate = (*(long long*)xl6_future_await_value(temporaryDuplicateRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late temporary duplicate task.race source await.
    exit /b 1
)
findstr /C:"cancelTypedAllGroup = xl6_task_all_from_array((xtarray)(cancelTypedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array task.all group fixture.
    exit /b 1
)
findstr /C:"cancelTypedAllRequested = xl6_task_cancel_value(cancelTypedAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelTypedAllRequestedAgain = xl6_task_cancel_value(cancelTypedAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.all group cancel idempotent.
    exit /b 1
)
findstr /C:"cancelTypedAllGroupStateAgain = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.all group repeated await canceled.
    exit /b 1
)
findstr /C:"cancelTypedAllRequestedAfterAwait = xl6_task_cancel_value(cancelTypedAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.all group cancel idempotent after await.
    exit /b 1
)
findstr /C:"cancelTypedAllGroupStateAfterCancel = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.all group post-cancel await canceled.
    exit /b 1
)
findstr /C:"cancelTypedAllLeftValue = (*(long long*)xl6_future_await_value(cancelTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array task.all left source await.
    exit /b 1
)
findstr /C:"cancelTypedAllLeftLateValue = (*(long long*)xl6_future_await_value(cancelTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array task.all left source await.
    exit /b 1
)
findstr /C:"expect_i64(cancelTypedAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late canceled typed-array task.all left source await canceled.
    exit /b 1
)
findstr /C:"cancelTypedAllRightValue = (*(long long*)xl6_future_await_value(cancelTypedAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array task.all right source await.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedAllGroup = xl6_task_all_from_array((xtarray)(cancelDuplicateTypedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array duplicate task.all group fixture.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedAllRequested = xl6_task_cancel_value(cancelDuplicateTypedAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedAllSourceLate = (*(long long*)xl6_future_await_value(cancelDuplicateTypedAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late typed-array duplicate task.all source await after group cancel.
    exit /b 1
)
findstr /C:"temporaryDuplicateTypedAllRequested = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(temporaryDuplicateTypedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing temporary typed-array duplicate task.all cancel fixture.
    exit /b 1
)
findstr /C:"temporaryDuplicateTypedAllSourceLate = (*(long long*)xl6_future_await_value(temporaryDuplicateTypedAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late temporary typed-array duplicate task.all source await.
    exit /b 1
)
findstr /C:"cancelTypedRaceGroup = xl6_task_race_from_array((xtarray)(cancelTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array task.race group fixture.
    exit /b 1
)
findstr /C:"cancelTypedRaceRequested = xl6_task_cancel_value(cancelTypedRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array task.race group as borrowed.
    exit /b 1
)
findstr /C:"cancelTypedRaceRequestedAgain = xl6_task_cancel_value(cancelTypedRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.race group cancel idempotent.
    exit /b 1
)
findstr /C:"cancelTypedRaceGroupStateAgain = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.race group repeated await canceled.
    exit /b 1
)
findstr /C:"cancelTypedRaceRequestedAfterAwait = xl6_task_cancel_value(cancelTypedRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.race group cancel idempotent after await.
    exit /b 1
)
findstr /C:"cancelTypedRaceGroupStateAfterCancel = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled typed-array task.race group post-cancel await canceled.
    exit /b 1
)
findstr /C:"cancelTypedRaceLeftValue = (*(long long*)xl6_future_await_value(cancelTypedRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array task.race left source await.
    exit /b 1
)
findstr /C:"cancelTypedRaceLeftLateValue = (*(long long*)xl6_future_await_value(cancelTypedRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array task.race left source await.
    exit /b 1
)
findstr /C:"expect_i64(cancelTypedRaceLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late canceled typed-array task.race left source await canceled.
    exit /b 1
)
findstr /C:"cancelTypedRaceRightValue = (*(long long*)xl6_future_await_value(cancelTypedRaceRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array task.race right source await.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedRaceGroup = xl6_task_race_from_array((xtarray)(cancelDuplicateTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array duplicate task.race group fixture.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedRaceRequested = xl6_task_cancel_value(cancelDuplicateTypedRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.race group as borrowed.
    exit /b 1
)
findstr /C:"cancelDuplicateTypedRaceSourceLate = (*(long long*)xl6_future_await_value(cancelDuplicateTypedRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late typed-array duplicate task.race source await after group cancel.
    exit /b 1
)
findstr /C:"temporaryDuplicateTypedRaceRequested = xl6_task_cancel_value(xl6_task_race_from_array((xtarray)(temporaryDuplicateTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing temporary typed-array duplicate task.race cancel fixture.
    exit /b 1
)
findstr /C:"temporaryDuplicateTypedRaceSourceLate = (*(long long*)xl6_future_await_value(temporaryDuplicateTypedRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late temporary typed-array duplicate task.race source await.
    exit /b 1
)
findstr /C:"pendingStringAllGroup = xl6_task_all_from_array((xtarray)(pendingStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array string task.all group fixture.
    exit /b 1
)
findstr /C:"pendingStringAllRequested = xl6_task_cancel_value(pendingStringAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array string task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringAllLeftValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.all left source await.
    exit /b 1
)
findstr /C:"pendingStringAllRightValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.all right source await.
    exit /b 1
)
findstr /C:"pendingStringAllLateValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array string task.all source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingStringAllLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array string task.all source canceled.
    exit /b 1
)
findstr /C:"pendingStringRaceGroup = xl6_task_race_from_array((xtarray)(pendingStringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array string task.race group fixture.
    exit /b 1
)
findstr /C:"pendingStringRaceRequested = xl6_task_cancel_value(pendingStringRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array string task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.race left source await.
    exit /b 1
)
findstr /C:"pendingStringRaceRightValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringRaceRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.race right source await.
    exit /b 1
)
findstr /C:"pendingStringRaceLateValue = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array string task.race source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingStringRaceLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array string task.race source canceled.
    exit /b 1
)
findstr /C:"pendingBoolAllGroup = xl6_task_all_from_array((xtarray)(pendingBoolAllJobs), xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array bool task.all group fixture.
    exit /b 1
)
findstr /C:"pendingBoolAllRequested = xl6_task_cancel_value(pendingBoolAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array bool task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftValue = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array bool task.all left source await.
    exit /b 1
)
findstr /C:"pendingBoolAllLateValue = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array bool task.all source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingBoolAllLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array bool task.all source canceled.
    exit /b 1
)
findstr /C:"pendingBoolRaceGroup = xl6_task_race_from_array((xtarray)(pendingBoolRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array bool task.race group fixture.
    exit /b 1
)
findstr /C:"pendingBoolRaceRequested = xl6_task_cancel_value(pendingBoolRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array bool task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolRaceRightValue = (*(int*)xl6_future_await_value(pendingBoolRaceRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array bool task.race right source await.
    exit /b 1
)
findstr /C:"pendingBoolRaceLateValue = (*(int*)xl6_future_await_value(pendingBoolRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array bool task.race source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingBoolRaceLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array bool task.race source canceled.
    exit /b 1
)
findstr /C:"pendingFloatAllGroup = xl6_task_all_from_array((xtarray)(pendingFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array float task.all group fixture.
    exit /b 1
)
findstr /C:"pendingFloatAllRequested = xl6_task_cancel_value(pendingFloatAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array float task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatAllRightValue = (*(double*)xl6_future_await_value(pendingFloatAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array float task.all right source await.
    exit /b 1
)
findstr /C:"pendingFloatAllLateValue = (*(double*)xl6_future_await_value(pendingFloatAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array float task.all source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingFloatAllLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array float task.all source canceled.
    exit /b 1
)
findstr /C:"pendingFloatRaceGroup = xl6_task_race_from_array((xtarray)(pendingFloatRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable typed-array float task.race group fixture.
    exit /b 1
)
findstr /C:"pendingFloatRaceRequested = xl6_task_cancel_value(pendingFloatRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending typed-array float task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftValue = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array float task.race left source await.
    exit /b 1
)
findstr /C:"pendingFloatRaceLateValue = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array float task.race source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingFloatRaceLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array float task.race source canceled.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_array_value" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed array result free hook.
    exit /b 1
)
findstr /C:"xl6_task_all_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all literal helper.
    exit /b 1
)
findstr /C:"xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race literal helper.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed array path.
    exit /b 1
)
findstr /C:"xl6_task_race_from_array" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race typed array path.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all class completion path.
    exit /b 1
)
findstr /C:"racedTaskBoxFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.race class future path.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all function completion path.
    exit /b 1
)
findstr /C:"xvalue fnValue = xl6_function_to_xvalue(*(XL6_Function*)item);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all function xvalue bridge.
    exit /b 1
)
findstr /C:"taskFuncA = xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all function callable extraction.
    exit /b 1
)
findstr /C:"racedTaskFunc = (*(XL6_Function*)xl6_future_await_value(racedTaskFuncFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.race function await path.
    exit /b 1
)
findstr /C:"namedTaskBoxAllFuture = xl6_task_all_values(xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named class task.all future.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_runtime_type_desc_" build\test\cgen_task_api.c | findstr /C:"xl6_future_await_value(namedTaskBoxAllFuture" >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed class task.all future.
    exit /b 1
)
findstr /C:"cancelNamedTaskBoxAllCompleted = xl6_task_cancel_value(namedTaskBoxAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed class task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_runtime_type_desc_" build\test\cgen_task_api.c | findstr /C:"xl6_future_await_value(namedTaskBoxAllFuture" >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"namedTaskBoxRaceCompletedFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named class task.race future.
    exit /b 1
)
findstr /C:"namedTaskBoxRaceB = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(namedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed class task.race future.
    exit /b 1
)
findstr /C:"cancelNamedTaskBoxRaceCompleted = xl6_task_cancel_value(namedTaskBoxRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed class task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedTaskBoxRace = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(namedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed class task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"namedTaskFuncAllFuture = xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named function task.all future.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(namedTaskFuncAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed function task.all future.
    exit /b 1
)
findstr /C:"cancelNamedTaskFuncAllCompleted = xl6_task_cancel_value(namedTaskFuncAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed function task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(namedTaskFuncAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed function task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"namedTaskFuncRaceCompletedFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named function task.race future.
    exit /b 1
)
findstr /C:"namedTaskFuncRaceB = (*(XL6_Function*)xl6_future_await_value(namedTaskFuncRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed function task.race future.
    exit /b 1
)
findstr /C:"cancelNamedTaskFuncRaceCompleted = xl6_task_cancel_value(namedTaskFuncRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed function task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedTaskFuncRace = (*(XL6_Function*)xl6_future_await_value(namedTaskFuncRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed function task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedTaskBoxJobs), xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all class completion path.
    exit /b 1
)
findstr /C:"typedRacedTaskBoxFuture = xl6_task_race_from_array((xtarray)(typedTaskBoxRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.race class future path.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all function completion path.
    exit /b 1
)
findstr /C:"typedTaskFuncA = xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all function callable extraction.
    exit /b 1
)
findstr /C:"typedRacedTaskFuncFuture = xl6_task_race_from_array((xtarray)(typedTaskFuncRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.race function future path.
    exit /b 1
)
findstr /C:"typedRacedTaskFunc = (*(XL6_Function*)xl6_future_await_value(typedRacedTaskFuncFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.race function await path.
    exit /b 1
)
findstr /C:"typedTaskBoxAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedTaskBoxAllCompletedJobs), xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed class task.all future path.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_runtime_type_desc_" build\test\cgen_task_api.c | findstr /C:"xl6_future_await_value(typedTaskBoxAllCompletedFuture" >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed class task.all future.
    exit /b 1
)
findstr /C:"cancelTypedTaskBoxAllCompleted = xl6_task_cancel_value(typedTaskBoxAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed class task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xl6_runtime_type_desc_" build\test\cgen_task_api.c | findstr /C:"xl6_future_await_value(typedTaskBoxAllCompletedFuture" >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedTaskBoxRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedTaskBoxRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed class task.race future path.
    exit /b 1
)
findstr /C:"typedTaskBoxRaceCompletedB = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(typedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed class task.race future.
    exit /b 1
)
findstr /C:"cancelTypedTaskBoxRaceCompleted = xl6_task_cancel_value(typedTaskBoxRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed class task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedTaskBoxRaceCompleted = xl6_xrt_move_to_shared(xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(typedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed class task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"typedTaskFuncAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedTaskFuncAllCompletedJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed function task.all future path.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(typedTaskFuncAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed function task.all future.
    exit /b 1
)
findstr /C:"cancelTypedTaskFuncAllCompleted = xl6_task_cancel_value(typedTaskFuncAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed function task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(typedTaskFuncAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed function task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedTaskFuncRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedTaskFuncRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed function task.race future path.
    exit /b 1
)
findstr /C:"typedTaskFuncRaceCompletedB = (*(XL6_Function*)xl6_future_await_value(typedTaskFuncRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed function task.race future.
    exit /b 1
)
findstr /C:"cancelTypedTaskFuncRaceCompleted = xl6_task_cancel_value(typedTaskFuncRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed function task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedTaskFuncRaceCompleted = (*(XL6_Function*)xl6_future_await_value(typedTaskFuncRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed function task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"pendingTaskBoxAllGroup = xl6_task_all_from_array((xtarray)(pendingTaskBoxAllJobs), xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing pending typed-array class task.all group.
    exit /b 1
)
findstr /C:"pendingTaskBoxAllRequested = xl6_task_cancel_value(pendingTaskBoxAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel pending typed-array class task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingTaskBoxAllLateValue = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(pendingTaskBoxAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array class task.all source await.
    exit /b 1
)
findstr /C:"pendingTaskBoxRaceGroup = xl6_task_race_from_array((xtarray)(pendingTaskBoxRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing pending typed-array class task.race group.
    exit /b 1
)
findstr /C:"pendingTaskBoxRaceRequested = xl6_task_cancel_value(pendingTaskBoxRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel pending typed-array class task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingTaskBoxRaceLateValue = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(pendingTaskBoxRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array class task.race source await.
    exit /b 1
)
findstr /C:"pendingTaskFuncAllGroup = xl6_task_all_from_array((xtarray)(pendingTaskFuncAllJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing pending typed-array function task.all group.
    exit /b 1
)
findstr /C:"pendingTaskFuncAllRequested = xl6_task_cancel_value(pendingTaskFuncAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel pending typed-array function task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingTaskFuncAllLateValue = (*(XL6_Function*)xl6_future_await_value(pendingTaskFuncAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array function task.all source await.
    exit /b 1
)
findstr /C:"pendingTaskFuncRaceGroup = xl6_task_race_from_array((xtarray)(pendingTaskFuncRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing pending typed-array function task.race group.
    exit /b 1
)
findstr /C:"pendingTaskFuncRaceRequested = xl6_task_cancel_value(pendingTaskFuncRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel pending typed-array function task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingTaskFuncRaceLateValue = (*(XL6_Function*)xl6_future_await_value(pendingTaskFuncRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array function task.race source await.
    exit /b 1
)
findstr /C:"xl6_task_race_start" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race runtime helper.
    exit /b 1
)
findstr /C:"static bool xl6_task_cancel_value" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.cancel ownership helper.
    exit /b 1
)
findstr /C:"cancelRequested = xl6_task_cancel_value(cancelTarget, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named task.cancel future as borrowed.
    exit /b 1
)
findstr /C:"cancelTemporaryAsync = xl6_task_cancel_value(xl_fn_addAsync(50LL, 6LL), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary async future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary task.all future through ownership helper.
    exit /b 1
)
findstr /C:"xl_fn_addAsync(60LL, 1LL), 1, xl_fn_addAsync(70LL, 2LL), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary task.race future through ownership helper.
    exit /b 1
)
findstr /C:"xl_fn_addAsync(80LL, 1LL), 1, xl_fn_addAsync(90LL, 2LL), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary task.race future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTypedAll = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(cancelAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array task.all future through ownership helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTypedAll\s*=\s*xl6_task_cancel_value\(xl6_task_all_from_array\(\(xtarray\)\(cancelAllJobs\),\s*xrtTypeInt\(\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c should release temporary typed-array task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTypedRace = xl6_task_cancel_value(xl6_task_race_from_array((xtarray)(cancelRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array task.race future through ownership helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTypedRace\s*=\s*xl6_task_cancel_value\(xl6_task_race_from_array\(\(xtarray\)\(cancelRaceJobs\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c should release temporary typed-array task.race future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTypedStringAll = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(cancelStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array string task.all future through ownership helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTypedStringAll\s*=\s*xl6_task_cancel_value\(xl6_task_all_from_array\(\(xtarray\)\(cancelStringAllJobs\),\s*xrtTypeString\(\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c should release temporary typed-array string task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTypedStringRace = xl6_task_cancel_value(xl6_task_race_from_array((xtarray)(cancelStringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array string task.race future through ownership helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTypedStringRace\s*=\s*xl6_task_cancel_value\(xl6_task_race_from_array\(\(xtarray\)\(cancelStringRaceJobs\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c should release temporary typed-array string task.race future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryBoolAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary bool task.all future through ownership helper.
    exit /b 1
)

echo DEMO6 suite ok: task API cgen assertions 2/4
exit /b 0
