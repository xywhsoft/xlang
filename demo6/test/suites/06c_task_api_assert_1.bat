@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API cgen assertions 1/4...

if not "%XL6_TASK_API_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_api.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
    if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_api.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_task_api.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"xl6_function_call_argv_borrow" build\test\cgen_task_api.assert.c >nul
if not errorlevel 1 (
    echo cgen_task_api.c should not emit task.spawn borrowed argv helper.
    exit /b 1
)
findstr /C:"xl6_task_all_start" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all runtime helper.
    exit /b 1
)
findstr /C:"static int xl6_task_all_continue" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all continuation mapper.
    exit /b 1
)
findstr /C:"pOut->iStatus = XL6_FUTURE_STATUS_CANCELLED;" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve task.all source cancellation.
    exit /b 1
)
findstr /C:"pOut->iStatus = XL6_FUTURE_STATUS_CLOSED;" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve task.all source closure.
    exit /b 1
)
findstr /C:"return error != 0 ? xl6_async_result_native_error(pOut, error) : xl6_async_result_error(pOut, \"task.all source failed\");" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve structured task.all source failures.
    exit /b 1
)
findstr /C:"unsigned char* owned;" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task literal future ownership flags.
    exit /b 1
)
findstr /C:"owned[i] = (unsigned char)va_arg(ap, int);" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task literal future ownership varargs.
    exit /b 1
)
findstr /C:"if ( owned[i] && futures[i] != 0 ) { xl6FutureDestroy(futures[i]); }" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing owned task literal future release.
    exit /b 1
)
findstr /C:"one, 0, two, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named task.all futures as borrowed.
    exit /b 1
)
findstr /C:"xl_fn_addAsync(10LL, 1LL), 1, xl_fn_addAsync(20LL, 2LL), 1" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass temporary task.all futures as owned.
    exit /b 1
)
findstr /C:"xl_fn_addAsync(9LL, 1LL), 1, xl_fn_addAsync(20LL, 2LL), 1" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass temporary task.race futures as owned.
    exit /b 1
)
findstr /C:"= xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race loser-cancel fixture.
    exit /b 1
)
findstr /C:"raceCancelWinner, 0, raceCancelLoser, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named task.race loser-cancel inputs as borrowed.
    exit /b 1
)
findstr /C:"raceLoserValue = (*(long long*)xl6_future_await_value(raceCancelLoser" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing loser future await after task.race.
    exit /b 1
)
findstr /C:"future cancel requested." build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race loser cancel diagnostic check.
    exit /b 1
)
findstr /C:"raceWinnerStillAvailable = (*(long long*)xl6_future_await_value(raceCancelWinner" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep task.race winner future awaitable.
    exit /b 1
)
findstr /C:"multiRaceWinner, 0, multiRaceLoserA, 0, multiRaceLoserB, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass three-way task.race named inputs as borrowed.
    exit /b 1
)
findstr /C:"multiRaceLoserAValue = (*(long long*)xl6_future_await_value(multiRaceLoserA" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing first loser await after three-way task.race.
    exit /b 1
)
findstr /C:"multiRaceLoserBValue = (*(long long*)xl6_future_await_value(multiRaceLoserB" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing second loser await after three-way task.race.
    exit /b 1
)
findstr /C:"nestedRaceLeft, 0, nestedRaceRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass nested task.race inputs to task.all as borrowed.
    exit /b 1
)
findstr /C:"nestedRaceFast, 0, nestedRaceSlow, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass nested task.race inputs to task.race as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(nestedRaceGroup, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed nested task.race cancel fixture.
    exit /b 1
)
findstr /C:"nestedRaceSlowLateValue = (*(long long*)xl6_future_await_value(nestedRaceSlow" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing nested task.race loser late await fixture.
    exit /b 1
)
findstr /C:"sourceCanceledAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing source-canceled task.all fixture.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeft, 0, sourceCanceledAllRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass source-canceled task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledAllRequest = xl6_task_cancel_value(sourceCanceledAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel task.all source future directly.
    exit /b 1
)
findstr /C:"cancelMissingArray = xl6_task_cancel_value(missingCancelArrayFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed array future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingList = xl6_task_cancel_value(missingCancelListFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed list future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingSet = xl6_task_cancel_value(missingCancelSetFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed set future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingDict = xl6_task_cancel_value(missingCancelDictFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed dict future as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledAllRightCancel = xl6_task_cancel_value(sourceCanceledAllRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other task.all source completed after source cancel.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeftLateValue = (*(long long*)xl6_future_await_value(sourceCanceledAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for directly canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledAllLeftLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeftAgain = xl6_task_cancel_value(sourceCanceledAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledAllLeftAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing directly canceled task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllGroup = xl6_task_all_from_array((xtarray)(sourceCanceledTypedAllJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.all fixture.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllRequest = xl6_task_cancel_value(sourceCanceledTypedAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array task.all source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllRightCancel = xl6_task_cancel_value(sourceCanceledTypedAllRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other typed-array task.all source completed after source cancel.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for directly canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source canceled.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftLateValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for directly canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftAgain = xl6_task_cancel_value(sourceCanceledTypedAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing directly canceled typed-array task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"sourceCanceledRaceGroup = xl6_task_race_values" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing source-canceled task.race fixture.
    exit /b 1
)
findstr /C:"sourceCanceledRaceWinner, 0, sourceCanceledRaceLoser, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass source-canceled task.race inputs as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledRaceRequest = xl6_task_cancel_value(sourceCanceledRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel task.race source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceGroup = xl6_task_race_from_array((xtarray)(sourceCanceledTypedRaceJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.race fixture.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceRequest = xl6_task_cancel_value(sourceCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array task.race source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceLoserLateValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedRaceLoser" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late typed-array source-canceled task.race loser await.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedRaceLoserLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array source-canceled task.race loser canceled.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceWinnerAgain = xl6_task_cancel_value(sourceCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array source-canceled task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedRaceWinnerAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.all source fixture.
    exit /b 1
)
findstr /C:"completedCanceledAllLeft, 0, completedCanceledAllRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed-canceled task.all source as borrowed.
    exit /b 1
)
findstr /C:"completedCanceledAllRightCancel = xl6_task_cancel_value(completedCanceledAllRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other source completed after completed-canceled task.all source.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftValue = (*(long long*)xl6_future_await_value(completedCanceledAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for completed-canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source canceled.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllGroup = xl6_task_all_from_array((xtarray)(completedCanceledTypedAllJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.all source fixture.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllRequest = xl6_task_cancel_value(completedCanceledTypedAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed-canceled task.all source before grouping.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllRightCancel = xl6_task_cancel_value(completedCanceledTypedAllRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed-canceled task.all sibling completed.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftValue = (*(long long*)xl6_future_await_value(completedCanceledTypedAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for completed-canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source canceled.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftLateValue = (*(long long*)xl6_future_await_value(completedCanceledAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for completed-canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftAgain = xl6_task_cancel_value(completedCanceledAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftLateValue = (*(long long*)xl6_future_await_value(completedCanceledTypedAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for completed-canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftAgain = xl6_task_cancel_value(completedCanceledTypedAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all failure-before-cancel fixture.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelLeft, 0, allFailedBeforeCancelRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failure-before-cancel task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelRightRequest = xl6_task_cancel_value(allFailedBeforeCancelRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel second source in failure-before-cancel task.all fixture.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel-before-failure fixture.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailLeft, 0, allCanceledBeforeFailRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancel-before-failure task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailLeftRequest = xl6_task_cancel_value(allCanceledBeforeFailLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel first source in cancel-before-failure task.all fixture.
    exit /b 1
)
findstr /C:"typedAllFailedBeforeCancelGroup = xl6_task_all_from_array((xtarray)(typedAllFailedBeforeCancelJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array failure-before-cancel task.all fixture.
    exit /b 1
)
findstr /C:"typedAllFailedBeforeCancelRightRequest = xl6_task_cancel_value(typedAllFailedBeforeCancelRight, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array failure-before-cancel task.all source.
    exit /b 1
)
findstr /C:"completedCanceledRaceRequest = xl6_task_cancel_value(completedCanceledRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should build completed-canceled task.race source fixture.
    exit /b 1
)
findstr /C:"completedCanceledRaceWinner, 0, completedCanceledRaceLoser, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed-canceled task.race source as borrowed.
    exit /b 1
)
findstr /C:"completedCanceledRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedCanceledRaceLoser" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late loser await for completed-canceled task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledRaceLoserLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.race loser canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledRaceWinnerAgain = xl6_task_cancel_value(completedCanceledRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledRaceWinnerAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceGroup = xl6_task_race_from_array((xtarray)(completedCanceledTypedRaceJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.race fixture.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceRequest = xl6_task_cancel_value(completedCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed-canceled task.race winner before grouping.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceLoserValue = (*(long long*)xl6_future_await_value(completedCanceledTypedRaceLoser" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing loser await for completed-canceled typed-array task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceLoserState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel completed-canceled typed-array task.race loser.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedCanceledTypedRaceLoser" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late loser await for completed-canceled typed-array task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceLoserLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.race loser canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceWinnerAgain = xl6_task_cancel_value(completedCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceWinnerAgain, 0LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"xl_fn_slowRaceAsync(401LL), 1, xl_fn_fastRaceAsync(402LL), 1" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep task.all literal input order with slow first.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedAllInputOrderJobs), xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all input order fixture.
    exit /b 1
)
findstr /C:"duplicateAllSource, 0, duplicateAllSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateRaceSource, 0, duplicateRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateRaceSource, 0, duplicateRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateRaceSourceCancel = xl6_task_cancel_value(duplicateRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate task.race source completed after race.
    exit /b 1
)
findstr /C:"duplicatePendingAllSource, 0, duplicatePendingAllSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate pending task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicatePendingAllSourceCancel = xl6_task_cancel_value(duplicatePendingAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate pending task.all source completed after all.
    exit /b 1
)
findstr /C:"duplicatePendingRaceSource, 0, duplicatePendingRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate pending task.race source fixture.
    exit /b 1
)
findstr /C:"duplicatePendingRaceSource, 0, duplicatePendingRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate pending task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicatePendingRaceSourceCancel = xl6_task_cancel_value(duplicatePendingRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate pending task.race source completed after race.
    exit /b 1
)
findstr /C:"duplicateCanceledAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate canceled task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateCanceledAllSource, 0, duplicateCanceledAllSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate canceled task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateCanceledAllRequest = xl6_task_cancel_value(duplicateCanceledAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.all source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateCanceledAllSourceCancelAgain = xl6_task_cancel_value(duplicateCanceledAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceGroup = xl6_task_race_values" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate canceled task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceSource, 0, duplicateCanceledRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate canceled task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceRequest = xl6_task_cancel_value(duplicateCanceledRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.race source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceSourceCancelAgain = xl6_task_cancel_value(duplicateCanceledRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate canceled task.race source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllGroup = xl6_task_all_from_array((xtarray)(duplicateTypedCanceledAllJobs), xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate canceled task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllRequest = xl6_task_cancel_value(duplicateTypedCanceledAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.all source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllSourceCancelAgain = xl6_task_cancel_value(duplicateTypedCanceledAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceGroup = xl6_task_race_from_array((xtarray)(duplicateTypedCanceledRaceJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate canceled task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceRequest = xl6_task_cancel_value(duplicateTypedCanceledRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.race source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceSourceCancelAgain = xl6_task_cancel_value(duplicateTypedCanceledRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate canceled task.race source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateFailedAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate failed task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateFailedAllSource, 0, duplicateFailedAllSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate failed task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateFailedAllSourceCancel = xl6_task_cancel_value(duplicateFailedAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate failed task.all source failed after all.
    exit /b 1
)
findstr /C:"duplicateFailedRaceGroup = xl6_task_race_values" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate failed task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateFailedRaceSource, 0, duplicateFailedRaceSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate failed task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateFailedRaceSourceCancel = xl6_task_cancel_value(duplicateFailedRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate failed task.race source failed after race.
    exit /b 1
)
findstr /C:"duplicateTypedFailedAllGroup = xl6_task_all_from_array((xtarray)(duplicateTypedFailedAllJobs), xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate failed task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedFailedAllSourceCancel = xl6_task_cancel_value(duplicateTypedFailedAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate failed task.all source failed after all.
    exit /b 1
)
findstr /C:"duplicateTypedFailedRaceGroup = xl6_task_race_from_array((xtarray)(duplicateTypedFailedRaceJobs)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate failed task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedFailedRaceSourceCancel = xl6_task_cancel_value(duplicateTypedFailedRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate failed task.race source failed after race.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(duplicateTypedAllJobs), xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate task.all source fixture.
    exit /b 1
)
findstr /C:"= xl6TypedArrayRetain(duplicateTypedRaceJobs);" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedRaceSourceCancel = xl6_task_cancel_value(duplicateTypedRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate task.race source completed after race.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(duplicateTypedPendingAllJobs), xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate pending task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedPendingAllSourceCancel = xl6_task_cancel_value(duplicateTypedPendingAllSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate pending task.all source completed after all.
    exit /b 1
)
findstr /C:"= xl6TypedArrayRetain(duplicateTypedPendingRaceJobs);" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate pending task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedPendingRaceSourceCancel = xl6_task_cancel_value(duplicateTypedPendingRaceSource, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate pending task.race source completed after race.
    exit /b 1
)
findstr /C:"groupFuture = xl6FutureRef(arg->groupFuture)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel forwarding target addref.
    exit /b 1
)
findstr /C:"xl6FutureLinkCancel(future, groupFuture)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel forwarding hook.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.assert.c',[System.Text.Encoding]::UTF8); $start=$text.IndexOf('static xfuture* xl6_task_all_start('); if($start -lt 0){exit 1}; $end=$text.IndexOf('static xfuture* xl6_task_race_start(', $start); if($end -lt 0){exit 1}; $slice=$text.Substring($start,$end-$start); $checks=@('xfuture* groupFuture;','groupFuture = xl6FutureRef(arg->groupFuture);','if ( future == 0 ) {','xl6FutureDestroy(groupFuture);','else if ( !xl6FutureLinkCancel(future, groupFuture) )','xl6FutureCancel(future)','xl6FutureDestroy(future)','} else {'); foreach($c in $checks){ if($slice.IndexOf($c) -lt 0){ exit 1 } }; if($slice -notmatch 'if \( arg->groupFuture == 0 \) \{[\s\S]*?xl6_task_all_arg_release\(arg\);[\s\S]*?task\.all start failed'){ exit 1 }; $idx=$slice.IndexOf('else if ( !xl6FutureLinkCancel(future, groupFuture) )'); $ret=$slice.IndexOf('return 0;', $idx); if($idx -lt 0 -or $ret -lt 0){exit 1}; $fail=$slice.Substring($idx,$ret-$idx); if($fail.IndexOf('xl6_task_all_arg_release(arg)') -ge 0){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing task.all start/cancel-forwarding lifecycle guard.
    exit /b 1
)
findstr /C:"cancelAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable task.all group fixture.
    exit /b 1
)
findstr /C:"cancelAllLeft, 0, cancelAllRight, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancelable task.all sources as borrowed.
    exit /b 1
)
findstr /C:"cancelAllRequested = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelAllRequestedAgain = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group cancel idempotent.
    exit /b 1
)
findstr /C:"cancelAllRequestedAfterAwait = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group cancel idempotent after await.
    exit /b 1
)
findstr /C:"cancelAllGroupStateAgain = 42LL" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group repeated await canceled.
    exit /b 1
)
findstr /C:"cancelAllGroupStateAfterCancel = 42LL" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group post-cancel await canceled.
    exit /b 1
)
findstr /C:"cancelAllLeftValue = (*(long long*)xl6_future_await_value(cancelAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.all left source await.
    exit /b 1
)
findstr /C:"cancelAllLeftCancelAgain = xl6_task_cancel_value(cancelAllLeft, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"cancelAllLeftLateValue = (*(long long*)xl6_future_await_value(cancelAllLeft" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled task.all left source await.
    exit /b 1
)
findstr /C:"expect_i64(cancelAllLeftLateState, 42LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late canceled task.all left source await canceled.
    exit /b 1
)
findstr /C:"cancelAllRightValue = (*(long long*)xl6_future_await_value(cancelAllRight" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.all right source await.
    exit /b 1
)
findstr /C:"cancelDuplicateAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable duplicate task.all group fixture.
    exit /b 1
)
findstr /C:"cancelDuplicateAllSource, 0, cancelDuplicateAllSource, 0" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.all cancel sources as borrowed twice.
    exit /b 1
)
findstr /C:"cancelDuplicateAllRequested = xl6_task_cancel_value(cancelDuplicateAllGroup, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelDuplicateAllSourceLate = (*(long long*)xl6_future_await_value(cancelDuplicateAllSource" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late duplicate task.all source await after group cancel.
    exit /b 1
)
findstr /C:"= xl6_task_cancel_value(xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing temporary duplicate task.all cancel fixture.
    exit /b 1
)
findstr /C:"temporaryDuplicateAllSource, 0, temporaryDuplicateAllSource, 0), 1)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary duplicate task.all group as owned with borrowed sources.
    exit /b 1
)
findstr /C:"temporaryDuplicateAllSourceLate = (*(long long*)xl6_future_await_value(temporaryDuplicateAllSource" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late temporary duplicate task.all source await.
    exit /b 1
)
findstr /C:"repeatCancelFuture = xl_fn_slowRaceAsync(206LL)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing direct async repeated cancel fixture.
    exit /b 1
)
findstr /C:"repeatCancelSecond = xl6_task_cancel_value(repeatCancelFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep direct async canceled future cancel idempotent.
    exit /b 1
)
findstr /C:"repeatCancelValue = (*(long long*)xl6_future_await_value(repeatCancelFuture" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing direct async canceled future await.
    exit /b 1
)
findstr /C:"repeatCancelAfterAwait = xl6_task_cancel_value(repeatCancelFuture, 0)" build\test\cgen_task_api.assert.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep direct async canceled future cancel idempotent after await.
    exit /b 1
)

echo DEMO6 suite ok: task API cgen assertions 1/4
exit /b 0

