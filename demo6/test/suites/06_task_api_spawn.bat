@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API and task spawn...

release\xl.exe check test\cases\cgen_future_task_wait.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_future_task_wait.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_future_task_wait.xl -o build\test\cgen_future_task_wait.exe
if errorlevel 1 exit /b 1

build\test\cgen_future_task_wait.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_future_task_wait.xl --emit build\test\cgen_future_task_wait.c
if errorlevel 1 exit /b 1

findstr /C:"xFutureWaitTimeout(pending" build\test\cgen_future_task_wait.c >nul
if errorlevel 1 (
    echo cgen_future_task_wait.c missing future.waitTimeout lowering.
    exit /b 1
)
findstr /C:"xl6_future_error_text(failed)" build\test\cgen_future_task_wait.c >nul
if errorlevel 1 (
    echo cgen_future_task_wait.c missing future.error lowering.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(cancelMe, 0)" build\test\cgen_future_task_wait.c >nul
if errorlevel 1 (
    echo cgen_future_task_wait.c missing borrowed future.cancel lowering.
    exit /b 1
)
findstr /C:"xl6_task_when_any_values" build\test\cgen_future_task_wait.c >nul
if errorlevel 1 (
    echo cgen_future_task_wait.c missing task.whenAny literal lowering.
    exit /b 1
)
findstr /C:"xl6_task_when_any_from_array((xtarray)(jobs)" build\test\cgen_future_task_wait.c >nul
if errorlevel 1 (
    echo cgen_future_task_wait.c missing task.whenAny typed array lowering.
    exit /b 1
)

release\xl.exe check test\cases\cgen_task_api.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_function_call_argv_borrow" build\test\cgen_task_api.c >nul
if not errorlevel 1 (
    echo cgen_task_api.c should not emit task.spawn borrowed argv helper.
    exit /b 1
)
findstr /C:"xl6_task_all_start" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all runtime helper.
    exit /b 1
)
findstr /C:"status = xl6_async_result_error(pOut, error != 0 ? error :" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should copy task.all wait error before releasing group future.
    exit /b 1
)
findstr /C:"status = xl6_async_result_error(pOut, error);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should copy task.all completed error before releasing group future.
    exit /b 1
)
findstr /C:"unsigned char* owned;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task literal future ownership flags.
    exit /b 1
)
findstr /C:"owned[i] = (unsigned char)va_arg(ap, int);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task literal future ownership varargs.
    exit /b 1
)
findstr /C:"if ( owned[i] && futures[i] != 0 ) { xFutureRelease(futures[i]); }" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing owned task literal future release.
    exit /b 1
)
findstr /C:"one, 0, two, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named task.all futures as borrowed.
    exit /b 1
)
findstr /C:"addAsync(10LL, 1LL), 1, addAsync(20LL, 2LL), 1" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass temporary task.all futures as owned.
    exit /b 1
)
findstr /C:"addAsync(9LL, 1LL), 1, addAsync(20LL, 2LL), 1" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass temporary task.race futures as owned.
    exit /b 1
)
findstr /C:"raceCancelValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race loser-cancel fixture.
    exit /b 1
)
findstr /C:"raceCancelWinner, 0, raceCancelLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named task.race loser-cancel inputs as borrowed.
    exit /b 1
)
findstr /C:"raceLoserValue = (*(long long*)xl6_future_await_value(raceCancelLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing loser future await after task.race.
    exit /b 1
)
findstr /C:"future cancel requested." build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race loser cancel diagnostic check.
    exit /b 1
)
findstr /C:"raceWinnerStillAvailable = (*(long long*)xl6_future_await_value(raceCancelWinner" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep task.race winner future awaitable.
    exit /b 1
)
findstr /C:"multiRaceWinner, 0, multiRaceLoserA, 0, multiRaceLoserB, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass three-way task.race named inputs as borrowed.
    exit /b 1
)
findstr /C:"multiRaceLoserAValue = (*(long long*)xl6_future_await_value(multiRaceLoserA" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing first loser await after three-way task.race.
    exit /b 1
)
findstr /C:"multiRaceLoserBValue = (*(long long*)xl6_future_await_value(multiRaceLoserB" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing second loser await after three-way task.race.
    exit /b 1
)
findstr /C:"nestedRaceLeft, 0, nestedRaceRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass nested task.race inputs to task.all as borrowed.
    exit /b 1
)
findstr /C:"nestedRaceFast, 0, nestedRaceSlow, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass nested task.race inputs to task.race as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(nestedRaceGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed nested task.race cancel fixture.
    exit /b 1
)
findstr /C:"nestedRaceSlowLateValue = (*(long long*)xl6_future_await_value(nestedRaceSlow" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing nested task.race loser late await fixture.
    exit /b 1
)
findstr /C:"sourceCanceledAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing source-canceled task.all fixture.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeft, 0, sourceCanceledAllRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass source-canceled task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledAllRequest = xl6_task_cancel_value(sourceCanceledAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel task.all source future directly.
    exit /b 1
)
findstr /C:"cancelMissingArray = xl6_task_cancel_value(missingCancelArrayFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed array future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingList = xl6_task_cancel_value(missingCancelListFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed list future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingSet = xl6_task_cancel_value(missingCancelSetFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed set future as borrowed.
    exit /b 1
)
findstr /C:"cancelMissingDict = xl6_task_cancel_value(missingCancelDictFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel null typed dict future as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledAllRightCancel = xl6_task_cancel_value(sourceCanceledAllRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other task.all source completed after source cancel.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeftLateValue = (*(long long*)xl6_future_await_value(sourceCanceledAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for directly canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"sourceCanceledAllLeftAgain = xl6_task_cancel_value(sourceCanceledAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledAllLeftAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing directly canceled task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllGroup = xl6_task_all_from_array((xtarray)(sourceCanceledTypedAllJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.all fixture.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllRequest = xl6_task_cancel_value(sourceCanceledTypedAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array task.all source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllRightCancel = xl6_task_cancel_value(sourceCanceledTypedAllRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other typed-array task.all source completed after source cancel.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for directly canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source canceled.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftLateValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for directly canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"sourceCanceledTypedAllLeftAgain = xl6_task_cancel_value(sourceCanceledTypedAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep directly canceled typed-array task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedAllLeftAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing directly canceled typed-array task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"sourceCanceledRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing source-canceled task.race fixture.
    exit /b 1
)
findstr /C:"sourceCanceledRaceWinner, 0, sourceCanceledRaceLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass source-canceled task.race inputs as borrowed.
    exit /b 1
)
findstr /C:"sourceCanceledRaceRequest = xl6_task_cancel_value(sourceCanceledRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel task.race source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceGroup = xl6_task_race_from_array((xtarray)(sourceCanceledTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.race fixture.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceRequest = xl6_task_cancel_value(sourceCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array task.race source future directly.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceLoserLateValue = (*(long long*)xl6_future_await_value(sourceCanceledTypedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late typed-array source-canceled task.race loser await.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedRaceLoserLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array source-canceled task.race loser canceled.
    exit /b 1
)
findstr /C:"sourceCanceledTypedRaceWinnerAgain = xl6_task_cancel_value(sourceCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array source-canceled task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(sourceCanceledTypedRaceWinnerAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array source-canceled task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.all source fixture.
    exit /b 1
)
findstr /C:"completedCanceledAllLeft, 0, completedCanceledAllRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed-canceled task.all source as borrowed.
    exit /b 1
)
findstr /C:"completedCanceledAllRightCancel = xl6_task_cancel_value(completedCanceledAllRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep other source completed after completed-canceled task.all source.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftValue = (*(long long*)xl6_future_await_value(completedCanceledAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for completed-canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source canceled.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllGroup = xl6_task_all_from_array((xtarray)(completedCanceledTypedAllJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.all source fixture.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllRequest = xl6_task_cancel_value(completedCanceledTypedAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed-canceled task.all source before grouping.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllRightCancel = xl6_task_cancel_value(completedCanceledTypedAllRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed-canceled task.all sibling completed.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftValue = (*(long long*)xl6_future_await_value(completedCanceledTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await for completed-canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source canceled.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftLateValue = (*(long long*)xl6_future_await_value(completedCanceledAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for completed-canceled task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledAllLeftAgain = xl6_task_cancel_value(completedCanceledAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledAllLeftAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftLateValue = (*(long long*)xl6_future_await_value(completedCanceledTypedAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late await for completed-canceled typed-array task.all source.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledTypedAllLeftAgain = xl6_task_cancel_value(completedCanceledTypedAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedAllLeftAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.all source repeated cancel assertion.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all failure-before-cancel fixture.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelLeft, 0, allFailedBeforeCancelRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failure-before-cancel task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"allFailedBeforeCancelRightRequest = xl6_task_cancel_value(allFailedBeforeCancelRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel second source in failure-before-cancel task.all fixture.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel-before-failure fixture.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailLeft, 0, allCanceledBeforeFailRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancel-before-failure task.all inputs as borrowed.
    exit /b 1
)
findstr /C:"allCanceledBeforeFailLeftRequest = xl6_task_cancel_value(allCanceledBeforeFailLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel first source in cancel-before-failure task.all fixture.
    exit /b 1
)
findstr /C:"typedAllFailedBeforeCancelGroup = xl6_task_all_from_array((xtarray)(typedAllFailedBeforeCancelJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array failure-before-cancel task.all fixture.
    exit /b 1
)
findstr /C:"typedAllFailedBeforeCancelRightRequest = xl6_task_cancel_value(typedAllFailedBeforeCancelRight, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array failure-before-cancel task.all source.
    exit /b 1
)
findstr /C:"completedCanceledRaceRequest = xl6_task_cancel_value(completedCanceledRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should build completed-canceled task.race source fixture.
    exit /b 1
)
findstr /C:"completedCanceledRaceWinner, 0, completedCanceledRaceLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed-canceled task.race source as borrowed.
    exit /b 1
)
findstr /C:"completedCanceledRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedCanceledRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late loser await for completed-canceled task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledRaceLoserLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.race loser canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledRaceWinnerAgain = xl6_task_cancel_value(completedCanceledRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledRaceWinnerAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceGroup = xl6_task_race_from_array((xtarray)(completedCanceledTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.race fixture.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceRequest = xl6_task_cancel_value(completedCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed-canceled task.race winner before grouping.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceLoserValue = (*(long long*)xl6_future_await_value(completedCanceledTypedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing loser await for completed-canceled typed-array task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceLoserState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel completed-canceled typed-array task.race loser.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedCanceledTypedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late loser await for completed-canceled typed-array task.race.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceLoserLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.race loser canceled after late completion.
    exit /b 1
)
findstr /C:"completedCanceledTypedRaceWinnerAgain = xl6_task_cancel_value(completedCanceledTypedRaceWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed-canceled typed-array task.race winner cancel idempotent.
    exit /b 1
)
findstr /C:"expect_i64(completedCanceledTypedRaceWinnerAgain, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-canceled typed-array task.race winner repeated cancel assertion.
    exit /b 1
)
findstr /C:"slowRaceAsync(401LL), 1, fastRaceAsync(402LL), 1" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep task.all literal input order with slow first.
    exit /b 1
)
findstr /C:"typedAllInputOrderValues = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedAllInputOrderJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all input order fixture.
    exit /b 1
)
findstr /C:"duplicateAllSource, 0, duplicateAllSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateRaceSource, 0, duplicateRaceSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateRaceSourceCancel = xl6_task_cancel_value(duplicateRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate task.race source completed after race.
    exit /b 1
)
findstr /C:"duplicatePendingAllSource, 0, duplicatePendingAllSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate pending task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicatePendingAllSourceCancel = xl6_task_cancel_value(duplicatePendingAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate pending task.all source completed after all.
    exit /b 1
)
findstr /C:"duplicatePendingRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate pending task.race source fixture.
    exit /b 1
)
findstr /C:"duplicatePendingRaceSource, 0, duplicatePendingRaceSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate pending task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicatePendingRaceSourceCancel = xl6_task_cancel_value(duplicatePendingRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate pending task.race source completed after race.
    exit /b 1
)
findstr /C:"duplicateCanceledAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate canceled task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateCanceledAllSource, 0, duplicateCanceledAllSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate canceled task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateCanceledAllRequest = xl6_task_cancel_value(duplicateCanceledAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.all source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateCanceledAllSourceCancelAgain = xl6_task_cancel_value(duplicateCanceledAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate canceled task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceSource, 0, duplicateCanceledRaceSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate canceled task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceRequest = xl6_task_cancel_value(duplicateCanceledRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.race source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateCanceledRaceSourceCancelAgain = xl6_task_cancel_value(duplicateCanceledRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate canceled task.race source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllGroup = xl6_task_all_from_array((xtarray)(duplicateTypedCanceledAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate canceled task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllRequest = xl6_task_cancel_value(duplicateTypedCanceledAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.all source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledAllSourceCancelAgain = xl6_task_cancel_value(duplicateTypedCanceledAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceGroup = xl6_task_race_from_array((xtarray)(duplicateTypedCanceledRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate canceled task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceRequest = xl6_task_cancel_value(duplicateTypedCanceledRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array duplicate task.race source before awaiting group.
    exit /b 1
)
findstr /C:"duplicateTypedCanceledRaceSourceCancelAgain = xl6_task_cancel_value(duplicateTypedCanceledRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate canceled task.race source cancel idempotent.
    exit /b 1
)
findstr /C:"duplicateFailedAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate failed task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateFailedAllSource, 0, duplicateFailedAllSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate failed task.all source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateFailedAllSourceCancel = xl6_task_cancel_value(duplicateFailedAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate failed task.all source failed after all.
    exit /b 1
)
findstr /C:"duplicateFailedRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing duplicate failed task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateFailedRaceSource, 0, duplicateFailedRaceSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate failed task.race source as borrowed twice.
    exit /b 1
)
findstr /C:"duplicateFailedRaceSourceCancel = xl6_task_cancel_value(duplicateFailedRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep duplicate failed task.race source failed after race.
    exit /b 1
)
findstr /C:"duplicateTypedFailedAllGroup = xl6_task_all_from_array((xtarray)(duplicateTypedFailedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate failed task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedFailedAllSourceCancel = xl6_task_cancel_value(duplicateTypedFailedAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate failed task.all source failed after all.
    exit /b 1
)
findstr /C:"duplicateTypedFailedRaceGroup = xl6_task_race_from_array((xtarray)(duplicateTypedFailedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate failed task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedFailedRaceSourceCancel = xl6_task_cancel_value(duplicateTypedFailedRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate failed task.race source failed after race.
    exit /b 1
)
findstr /C:"duplicateTypedAllValues = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(duplicateTypedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedRaceValue = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(duplicateTypedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedRaceSourceCancel = xl6_task_cancel_value(duplicateTypedRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate task.race source completed after race.
    exit /b 1
)
findstr /C:"duplicateTypedPendingAllValues = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(duplicateTypedPendingAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate pending task.all source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedPendingAllSourceCancel = xl6_task_cancel_value(duplicateTypedPendingAllSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate pending task.all source completed after all.
    exit /b 1
)
findstr /C:"duplicateTypedPendingRaceValue = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(duplicateTypedPendingRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array duplicate pending task.race source fixture.
    exit /b 1
)
findstr /C:"duplicateTypedPendingRaceSourceCancel = xl6_task_cancel_value(duplicateTypedPendingRaceSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array duplicate pending task.race source completed after race.
    exit /b 1
)
findstr /C:"groupFuture = xFutureAddRef(arg->groupFuture)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel forwarding target addref.
    exit /b 1
)
findstr /C:"xFutureForwardCancelTo(future, groupFuture)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all cancel forwarding hook.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); $start=$text.IndexOf('static xfuture* xl6_task_all_start('); if($start -lt 0){exit 1}; $end=$text.IndexOf('static xfuture* xl6_task_race_start(', $start); if($end -lt 0){exit 1}; $slice=$text.Substring($start,$end-$start); $checks=@('xfuture* groupFuture;','groupFuture = xFutureAddRef(arg->groupFuture);','if ( future == 0 ) {','xFutureRelease(groupFuture);','else if ( !xFutureForwardCancelTo(future, groupFuture) )','xFutureRequestCancel(future)','xFutureRelease(future)','} else {'); foreach($c in $checks){ if($slice.IndexOf($c) -lt 0){ exit 1 } }; if($slice -notmatch 'if \( arg->groupFuture == 0 \) \{[\s\S]*?xl6_task_all_arg_release\(arg\);[\s\S]*?task\.all start failed'){ exit 1 }; $idx=$slice.IndexOf('else if ( !xFutureForwardCancelTo(future, groupFuture) )'); $ret=$slice.IndexOf('return 0;', $idx); if($idx -lt 0 -or $ret -lt 0){exit 1}; $fail=$slice.Substring($idx,$ret-$idx); if($fail.IndexOf('xl6_task_all_arg_release(arg)') -ge 0){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing task.all start/cancel-forwarding lifecycle guard.
    exit /b 1
)
findstr /C:"cancelAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable task.all group fixture.
    exit /b 1
)
findstr /C:"cancelAllLeft, 0, cancelAllRight, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancelable task.all sources as borrowed.
    exit /b 1
)
findstr /C:"cancelAllRequested = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel named pending task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelAllRequestedAgain = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group cancel idempotent.
    exit /b 1
)
findstr /C:"cancelAllRequestedAfterAwait = xl6_task_cancel_value(cancelAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group cancel idempotent after await.
    exit /b 1
)
findstr /C:"cancelAllGroupStateAgain = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group repeated await canceled.
    exit /b 1
)
findstr /C:"cancelAllGroupStateAfterCancel = 42LL" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all group post-cancel await canceled.
    exit /b 1
)
findstr /C:"cancelAllLeftValue = (*(long long*)xl6_future_await_value(cancelAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.all left source await.
    exit /b 1
)
findstr /C:"cancelAllLeftCancelAgain = xl6_task_cancel_value(cancelAllLeft, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep canceled task.all source cancel idempotent.
    exit /b 1
)
findstr /C:"cancelAllLeftLateValue = (*(long long*)xl6_future_await_value(cancelAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled task.all left source await.
    exit /b 1
)
findstr /C:"expect_i64(cancelAllLeftLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late canceled task.all left source await canceled.
    exit /b 1
)
findstr /C:"cancelAllRightValue = (*(long long*)xl6_future_await_value(cancelAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled task.all right source await.
    exit /b 1
)
findstr /C:"cancelDuplicateAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing cancelable duplicate task.all group fixture.
    exit /b 1
)
findstr /C:"cancelDuplicateAllSource, 0, cancelDuplicateAllSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass duplicate task.all cancel sources as borrowed twice.
    exit /b 1
)
findstr /C:"cancelDuplicateAllRequested = xl6_task_cancel_value(cancelDuplicateAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel duplicate task.all group as borrowed.
    exit /b 1
)
findstr /C:"cancelDuplicateAllSourceLate = (*(long long*)xl6_future_await_value(cancelDuplicateAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late duplicate task.all source await after group cancel.
    exit /b 1
)
findstr /C:"temporaryDuplicateAllRequested = xl6_task_cancel_value(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing temporary duplicate task.all cancel fixture.
    exit /b 1
)
findstr /C:"temporaryDuplicateAllSource, 0, temporaryDuplicateAllSource, 0), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary duplicate task.all group as owned with borrowed sources.
    exit /b 1
)
findstr /C:"temporaryDuplicateAllSourceLate = (*(long long*)xl6_future_await_value(temporaryDuplicateAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late temporary duplicate task.all source await.
    exit /b 1
)
findstr /C:"repeatCancelFuture = slowRaceAsync(206LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing direct async repeated cancel fixture.
    exit /b 1
)
findstr /C:"repeatCancelSecond = xl6_task_cancel_value(repeatCancelFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep direct async canceled future cancel idempotent.
    exit /b 1
)
findstr /C:"repeatCancelValue = (*(long long*)xl6_future_await_value(repeatCancelFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing direct async canceled future await.
    exit /b 1
)
findstr /C:"repeatCancelAfterAwait = xl6_task_cancel_value(repeatCancelFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep direct async canceled future cancel idempotent after await.
    exit /b 1
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
findstr /C:"pendingStringAllLeftValue = xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.all left source await.
    exit /b 1
)
findstr /C:"pendingStringAllRightValue = xl6_future_await_cstr_copy(pendingStringAllRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.all right source await.
    exit /b 1
)
findstr /C:"pendingStringAllLateValue = xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_api.c >nul
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
findstr /C:"pendingStringRaceLeftValue = xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.race left source await.
    exit /b 1
)
findstr /C:"pendingStringRaceRightValue = xl6_future_await_cstr_copy(pendingStringRaceRight" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing canceled typed-array string task.race right source await.
    exit /b 1
)
findstr /C:"pendingStringRaceLateValue = xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing late canceled typed-array string task.race source await.
    exit /b 1
)
findstr /C:"expect_i64(pendingStringRaceLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep late typed-array string task.race source canceled.
    exit /b 1
)
findstr /C:"pendingBoolAllGroup = xl6_task_all_from_array((xtarray)(pendingBoolAllJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
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
findstr /C:"taskBoxes = xl6_future_await_handle_release(xl6_task_all_values(xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all class completion path.
    exit /b 1
)
findstr /C:"racedTaskBoxFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.race class future path.
    exit /b 1
)
findstr /C:"taskFuncs = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
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
findstr /C:"namedTaskBoxAllFuture = xl6_task_all_values(xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named class task.all future.
    exit /b 1
)
findstr /C:"namedTaskBoxAllB = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(namedTaskBoxAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed class task.all future.
    exit /b 1
)
findstr /C:"cancelNamedTaskBoxAllCompleted = xl6_task_cancel_value(namedTaskBoxAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed class task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedTaskBoxAll = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(namedTaskBoxAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"namedTaskBoxRaceCompletedFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named class task.race future.
    exit /b 1
)
findstr /C:"namedTaskBoxRaceB = (*(XL_Class_TaskBox*)xl6_future_await_value(namedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed class task.race future.
    exit /b 1
)
findstr /C:"cancelNamedTaskBoxRaceCompleted = xl6_task_cancel_value(namedTaskBoxRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed class task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(namedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed class task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"namedTaskFuncAllFuture = xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named function task.all future.
    exit /b 1
)
findstr /C:"namedTaskFuncAllB = xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(namedTaskFuncAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for completed function task.all future.
    exit /b 1
)
findstr /C:"cancelNamedTaskFuncAllCompleted = xl6_task_cancel_value(namedTaskFuncAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed function task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedTaskFuncAll = xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(namedTaskFuncAllFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"typedTaskBoxes = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all class completion path.
    exit /b 1
)
findstr /C:"typedRacedTaskBoxFuture = xl6_task_race_from_array((xtarray)(typedTaskBoxRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.race class future path.
    exit /b 1
)
findstr /C:"typedTaskFuncs = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
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
findstr /C:"typedTaskBoxAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedTaskBoxAllCompletedJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed class task.all future path.
    exit /b 1
)
findstr /C:"typedTaskBoxAllCompletedB = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(typedTaskBoxAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed class task.all future.
    exit /b 1
)
findstr /C:"cancelTypedTaskBoxAllCompleted = xl6_task_cancel_value(typedTaskBoxAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed class task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedTaskBoxAllCompleted = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(typedTaskBoxAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedTaskBoxRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedTaskBoxRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed class task.race future path.
    exit /b 1
)
findstr /C:"typedTaskBoxRaceCompletedB = (*(XL_Class_TaskBox*)xl6_future_await_value(typedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed class task.race future.
    exit /b 1
)
findstr /C:"cancelTypedTaskBoxRaceCompleted = xl6_task_cancel_value(typedTaskBoxRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed class task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedTaskBoxRaceCompleted = (*(XL_Class_TaskBox*)xl6_future_await_value(typedTaskBoxRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed class task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"typedTaskFuncAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedTaskFuncAllCompletedJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed function task.all future path.
    exit /b 1
)
findstr /C:"typedTaskFuncAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(typedTaskFuncAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed function task.all future.
    exit /b 1
)
findstr /C:"cancelTypedTaskFuncAllCompleted = xl6_task_cancel_value(typedTaskFuncAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed function task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedTaskFuncAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(typedTaskFuncAllCompletedFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"pendingTaskBoxAllGroup = xl6_task_all_from_array((xtarray)(pendingTaskBoxAllJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing pending typed-array class task.all group.
    exit /b 1
)
findstr /C:"pendingTaskBoxAllRequested = xl6_task_cancel_value(pendingTaskBoxAllGroup, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel pending typed-array class task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingTaskBoxAllLateValue = (*(XL_Class_TaskBox*)xl6_future_await_value(pendingTaskBoxAllRight" build\test\cgen_task_api.c >nul
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
findstr /C:"pendingTaskBoxRaceLateValue = (*(XL_Class_TaskBox*)xl6_future_await_value(pendingTaskBoxRaceLeft" build\test\cgen_task_api.c >nul
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
findstr /C:"cancelTemporaryAsync = xl6_task_cancel_value(addAsync(50LL, 6LL), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary async future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary task.all future through ownership helper.
    exit /b 1
)
findstr /C:"addAsync(60LL, 1LL), 1, addAsync(70LL, 2LL), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary task.race future through ownership helper.
    exit /b 1
)
findstr /C:"addAsync(80LL, 1LL), 1, addAsync(90LL, 2LL), 1), 1)" build\test\cgen_task_api.c >nul
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
findstr /C:"cancelTemporaryBoolAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary bool task.all future through ownership helper.
    exit /b 1
)
findstr /C:"flipAsync(1), 1, flipAsync(0), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary bool task.all/race source futures after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryBoolRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary bool task.race future through ownership helper.
    exit /b 1
)
findstr /C:"cancelTypedBoolAll = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(cancelBoolAllJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array bool task.all future through ownership helper.
    exit /b 1
)
findstr /C:"cancelTypedBoolRace = xl6_task_cancel_value(xl6_task_race_from_array((xtarray)(cancelBoolRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array bool task.race future through ownership helper.
    exit /b 1
)
findstr /C:"cancelTemporaryFloatAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary float task.all future through ownership helper.
    exit /b 1
)
findstr /C:"addHalfAsync(9.5), 1, addHalfAsync(10.5), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary float task.all source futures after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryFloatRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary float task.race future through ownership helper.
    exit /b 1
)
findstr /C:"addHalfAsync(11.5), 1, addHalfAsync(12.5), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary float task.race source futures after task.cancel.
    exit /b 1
)
findstr /C:"cancelTypedFloatAll = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(cancelFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array float task.all future through ownership helper.
    exit /b 1
)
findstr /C:"cancelTypedFloatRace = xl6_task_cancel_value(xl6_task_race_from_array((xtarray)(cancelFloatRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary typed-array float task.race future through ownership helper.
    exit /b 1
)
findstr /C:"xrtTypeFuture()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed future descriptor.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xfuture\* (__xl6_typed_future_\d+) = addAsync\(11LL, 1LL\);\s*if \(!xrtTypedArrayAppend\(raceJobs, &\1\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);' -or $text -notmatch 'xfuture\* (__xl6_typed_future_\d+) = addAsync\(40LL, 1LL\);\s*if \(!xrtTypedArrayAppend\(typedAllCompletedJobs, &\1\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing direct-call typed future append cleanup.
    exit /b 1
)
findstr /C:"xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing string descriptor for task.all string results.
    exit /b 1
)
findstr /C:"xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing bool descriptor for task.all bool results.
    exit /b 1
)
findstr /C:"xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing float descriptor for task.all float results.
    exit /b 1
)
findstr /C:"stringAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all string aggregation path.
    exit /b 1
)
findstr /C:"stringRace = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race string await path.
    exit /b 1
)
findstr /C:"boolAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all bool aggregation path.
    exit /b 1
)
findstr /C:"namedBoolAllFuture = xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all future fixture.
    exit /b 1
)
findstr /C:"namedBoolAllA = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all first await copy.
    exit /b 1
)
findstr /C:"namedBoolAllB = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all second await copy.
    exit /b 1
)
findstr /C:"typedBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedBoolAllJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed bool array path.
    exit /b 1
)
findstr /C:"boolRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race bool await path.
    exit /b 1
)
findstr /C:"typedBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(boolRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race typed bool array path.
    exit /b 1
)
findstr /C:"floatAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all float aggregation path.
    exit /b 1
)
findstr /C:"namedFloatAllFuture = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all future fixture.
    exit /b 1
)
findstr /C:"namedFloatAllA = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all first await copy.
    exit /b 1
)
findstr /C:"namedFloatAllB = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all second await copy.
    exit /b 1
)
findstr /C:"typedFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed float array path.
    exit /b 1
)
findstr /C:"floatRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race float await path.
    exit /b 1
)
findstr /C:"typedFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(floatRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race typed float array path.
    exit /b 1
)
findstr /C:"namedAllFuture = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.all future fixture.
    exit /b 1
)
findstr /C:"namedAllA = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.all first await copy.
    exit /b 1
)
findstr /C:"namedAllB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.all second await copy.
    exit /b 1
)
findstr /C:"repeatedFailedAll = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed task.all future fixture.
    exit /b 1
)
findstr /C:"repeatedAllB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(repeatedFailedAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed task.all future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedAll = xl6_task_cancel_value(repeatedFailedAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelRepeatedAll = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(repeatedFailedAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedStringAllFuture = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all future fixture.
    exit /b 1
)
findstr /C:"namedStringAllA = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all first await copy.
    exit /b 1
)
findstr /C:"namedStringAllB = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all second await copy.
    exit /b 1
)
findstr /C:"typedStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed string array path.
    exit /b 1
)
findstr /C:"cancelNamedAllCompleted = xl6_task_cancel_value(namedAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedAll = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedRaceFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named task.race future fixture.
    exit /b 1
)
findstr /C:"cancelNamedRaceCompleted = xl6_task_cancel_value(namedRaceFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named task.race future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedRaceValue = (*(long long*)xl6_future_await_value(namedRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"typedAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedAllCompletedJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed int task.all future path.
    exit /b 1
)
findstr /C:"typedAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed int task.all future.
    exit /b 1
)
findstr /C:"cancelTypedAllCompleted = xl6_task_cancel_value(typedAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed int task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed int task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed int task.race future path.
    exit /b 1
)
findstr /C:"typedRaceCompletedB = (*(long long*)xl6_future_await_value(typedRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed int task.race future.
    exit /b 1
)
findstr /C:"cancelTypedRaceCompleted = xl6_task_cancel_value(typedRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed int task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedRaceCompleted = (*(long long*)xl6_future_await_value(typedRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed int task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"cancelNamedStringAllCompleted = xl6_task_cancel_value(namedStringAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named string task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedStringAll = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named string task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedStringRaceFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named string task.race future fixture.
    exit /b 1
)
findstr /C:"cancelNamedStringRaceCompleted = xl6_task_cancel_value(namedStringRaceFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named string task.race future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedStringRaceValue = xl6_future_await_cstr_copy(namedStringRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named string task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"typedStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(stringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race typed string array path.
    exit /b 1
)
findstr /C:"typedStringAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedStringAllCompletedJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed string task.all future path.
    exit /b 1
)
findstr /C:"typedStringAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(typedStringAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed string task.all future.
    exit /b 1
)
findstr /C:"cancelTypedStringAllCompleted = xl6_task_cancel_value(typedStringAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed string task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedStringAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(typedStringAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed string task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedStringRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedStringRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed string task.race future path.
    exit /b 1
)
findstr /C:"typedStringRaceCompletedB = xl6_future_await_cstr_copy(typedStringRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed string task.race future.
    exit /b 1
)
findstr /C:"cancelTypedStringRaceCompleted = xl6_task_cancel_value(typedStringRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed string task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedStringRaceCompleted = xl6_future_await_cstr_copy(typedStringRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed string task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"typedBoolAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedBoolAllCompletedJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed bool task.all future path.
    exit /b 1
)
findstr /C:"typedBoolAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(typedBoolAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed bool task.all future.
    exit /b 1
)
findstr /C:"cancelTypedBoolAllCompleted = xl6_task_cancel_value(typedBoolAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed bool task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedBoolAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(typedBoolAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed bool task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedBoolRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedBoolRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed bool task.race future path.
    exit /b 1
)
findstr /C:"typedBoolRaceCompletedB = (*(int*)xl6_future_await_value(typedBoolRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed bool task.race future.
    exit /b 1
)
findstr /C:"cancelTypedBoolRaceCompleted = xl6_task_cancel_value(typedBoolRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed bool task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedBoolRaceCompleted = (*(int*)xl6_future_await_value(typedBoolRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed bool task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"typedFloatAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedFloatAllCompletedJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed float task.all future path.
    exit /b 1
)
findstr /C:"typedFloatAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(typedFloatAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed float task.all future.
    exit /b 1
)
findstr /C:"cancelTypedFloatAllCompleted = xl6_task_cancel_value(typedFloatAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed float task.all future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedFloatAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(typedFloatAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed float task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedFloatRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedFloatRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed float task.race future path.
    exit /b 1
)
findstr /C:"typedFloatRaceCompletedB = (*(double*)xl6_future_await_value(typedFloatRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed float task.race future.
    exit /b 1
)
findstr /C:"cancelTypedFloatRaceCompleted = xl6_task_cancel_value(typedFloatRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed float task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedFloatRaceCompleted = (*(double*)xl6_future_await_value(typedFloatRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed float task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"cancelNamedBoolAllCompleted = xl6_task_cancel_value(namedBoolAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named bool task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedBoolAll = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named bool task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedBoolRaceFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named bool task.race future fixture.
    exit /b 1
)
findstr /C:"cancelNamedBoolRaceCompleted = xl6_task_cancel_value(namedBoolRaceFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named bool task.race future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedBoolRaceValue = (*(int*)xl6_future_await_value(namedBoolRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named bool task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"cancelNamedFloatAllCompleted = xl6_task_cancel_value(namedFloatAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named float task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedFloatAll = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named float task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedFloatRaceFuture = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed named float task.race future fixture.
    exit /b 1
)
findstr /C:"cancelNamedFloatRaceCompleted = xl6_task_cancel_value(namedFloatRaceFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named float task.race future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelNamedFloatRaceValue = (*(double*)xl6_future_await_value(namedFloatRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named float task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"allFailureGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all fast-failure slow-source fixture.
    exit /b 1
)
findstr /C:"failAsync(), 1, allFailureSlowSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass task.all failed temporary as owned and slow source as borrowed.
    exit /b 1
)
findstr /C:"allFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(allFailureGroup" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all fast-failure group await.
    exit /b 1
)
findstr /C:"allFailureSlowValue = (*(long long*)xl6_future_await_value(allFailureSlowSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep task.all slow source awaitable after group failure.
    exit /b 1
)
findstr /C:"typedAllFailureGroup = xl6_task_all_from_array((xtarray)(typedAllFailureJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all fast-failure slow-source fixture.
    exit /b 1
)
findstr /C:"typedAllFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedAllFailureGroup" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array task.all fast-failure group await.
    exit /b 1
)
findstr /C:"typedAllFailureSlowValue = (*(long long*)xl6_future_await_value(typedAllFailureSlowSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array task.all slow source awaitable after group failure.
    exit /b 1
)
findstr /C:"typedAllFailureSlowCancel = xl6_task_cancel_value(typedAllFailureSlowSource, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array task.all slow source completed after group failure.
    exit /b 1
)
findstr /C:"raceFailureGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race fast-failure slow-loser fixture.
    exit /b 1
)
findstr /C:"failAsync(), 1, raceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass task.race failed temporary as owned and slow loser as borrowed.
    exit /b 1
)
findstr /C:"raceFailureLoserValueA = (*(long long*)xl6_future_await_value(raceFailureSlowLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race failed-loser first await.
    exit /b 1
)
findstr /C:"raceFailureLoserValueB = (*(long long*)xl6_future_await_value(raceFailureSlowLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race failed-loser repeated await after worker exit.
    exit /b 1
)
findstr /C:"expect_i64(raceFailureLoserStateB, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve task.race failed-loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"completedFailedRaceGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-failed task.race source fixture.
    exit /b 1
)
findstr /C:"completedFailedRaceWinner, 0, completedFailedRaceLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed-failed task.race source as borrowed.
    exit /b 1
)
findstr /C:"completedFailedRaceLoserValue = (*(long long*)xl6_future_await_value(completedFailedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-failed task.race loser await.
    exit /b 1
)
findstr /C:"completedFailedRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedFailedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing completed-failed task.race loser late await.
    exit /b 1
)
findstr /C:"expect_i64(completedFailedRaceLoserLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve completed-failed task.race loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"completedTypedFailedRaceGroup = xl6_task_race_from_array((xtarray)(completedTypedFailedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed-failed task.race source fixture.
    exit /b 1
)
findstr /C:"completedTypedFailedRaceLoserValue = (*(long long*)xl6_future_await_value(completedTypedFailedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed-failed task.race loser await.
    exit /b 1
)
findstr /C:"expect_i64(completedTypedFailedRaceLoserState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve typed-array completed-failed task.race loser first cancel state.
    exit /b 1
)
findstr /C:"completedTypedFailedRaceLoserLateValue = (*(long long*)xl6_future_await_value(completedTypedFailedRaceLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed-failed task.race loser late await.
    exit /b 1
)
findstr /C:"expect_i64(completedTypedFailedRaceLoserLateState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve typed-array completed-failed task.race loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"raceFailedBeforeCancelGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race failure-before-cancel fixture.
    exit /b 1
)
findstr /C:"raceFailedBeforeCancelWinner, 0, raceFailedBeforeCancelLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failure-before-cancel task.race inputs as borrowed.
    exit /b 1
)
findstr /C:"raceFailedBeforeCancelLoserRequest = xl6_task_cancel_value(raceFailedBeforeCancelLoser, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel second source in failure-before-cancel task.race fixture.
    exit /b 1
)
findstr /C:"raceCanceledBeforeFailGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race cancel-before-failure fixture.
    exit /b 1
)
findstr /C:"raceCanceledBeforeFailWinner, 0, raceCanceledBeforeFailLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass cancel-before-failure task.race inputs as borrowed.
    exit /b 1
)
findstr /C:"raceCanceledBeforeFailWinnerRequest = xl6_task_cancel_value(raceCanceledBeforeFailWinner, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel first source in cancel-before-failure task.race fixture.
    exit /b 1
)
findstr /C:"typedRaceFailedBeforeCancelGroup = xl6_task_race_from_array((xtarray)(typedRaceFailedBeforeCancelJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array failure-before-cancel task.race fixture.
    exit /b 1
)
findstr /C:"typedRaceFailedBeforeCancelLoserRequest = xl6_task_cancel_value(typedRaceFailedBeforeCancelLoser, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array failure-before-cancel task.race source.
    exit /b 1
)
findstr /C:"failedTaskBoxAll = xl6_future_await_handle_release(xl6_task_all_values(xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class task.all await.
    exit /b 1
)
findstr /C:"failedTypedTaskBoxAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedTaskBoxAllJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.all await.
    exit /b 1
)
findstr /C:"taskBoxRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class task.race group.
    exit /b 1
)
findstr /C:"failTaskBoxAsync(), 1, taskBoxRaceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed class race temporary as owned and loser as borrowed.
    exit /b 1
)
findstr /C:"taskBoxRaceFailureLoserValueB = (*(XL_Class_TaskBox*)xl6_future_await_value(taskBoxRaceFailureSlowLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class race loser repeated await.
    exit /b 1
)
findstr /C:"expect_i64(taskBoxRaceFailureLoserStateB, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve failed class race loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"failedTaskFuncAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed function task.all await.
    exit /b 1
)
findstr /C:"failedTypedTaskFuncAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedTaskFuncAllJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array function task.all await.
    exit /b 1
)
findstr /C:"taskFuncRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed function task.race group.
    exit /b 1
)
findstr /C:"failTaskAdderAsync(), 1, taskFuncRaceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed function race temporary as owned and loser as borrowed.
    exit /b 1
)
findstr /C:"taskFuncRaceFailureLoserValueB = (*(XL6_Function*)xl6_future_await_value(taskFuncRaceFailureSlowLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed function race loser repeated await.
    exit /b 1
)
findstr /C:"expect_i64(taskFuncRaceFailureLoserStateB, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve failed function race loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"repeatedFailedTaskBoxAll = xl6_task_all_values(xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed class task.all future.
    exit /b 1
)
findstr /C:"secondFailedTaskBoxAll = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(repeatedFailedTaskBoxAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed class task.all future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedTaskBoxAll = xl6_task_cancel_value(repeatedFailedTaskBoxAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed class task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedTaskBoxAll = xl6_xrt_typed_array_copy_mode(xl6_type_desc_XL_Class_TaskBox(), (*(xtarray*)xl6_future_await_value(repeatedFailedTaskBoxAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"repeatedFailedTaskBoxRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed class task.race future.
    exit /b 1
)
findstr /C:"secondFailedTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(repeatedFailedTaskBoxRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed class task.race future.
    exit /b 1
)
findstr /C:"failedTypedTaskBoxRaceFuture = xl6_task_race_from_array((xtarray)(failedTaskBoxRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.race future.
    exit /b 1
)
findstr /C:"failedTypedTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(failedTypedTaskBoxRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.race await.
    exit /b 1
)
findstr /C:"repeatedFailedTaskFuncAll = xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed function task.all future.
    exit /b 1
)
findstr /C:"secondFailedTaskFuncAll = xl6_xrt_typed_array_copy_mode(xrtTypeFunction(), (*(xtarray*)xl6_future_await_value(repeatedFailedTaskFuncAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed function task.all future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedTaskFuncAll = xl6_task_cancel_value(repeatedFailedTaskFuncAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed function task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"repeatedFailedTaskFuncRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed function task.race future.
    exit /b 1
)
findstr /C:"secondFailedTaskFuncRace = (*(XL6_Function*)xl6_future_await_value(repeatedFailedTaskFuncRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed function task.race future.
    exit /b 1
)
findstr /C:"failedTypedTaskFuncRace = xl6_future_await_function_release(xl6_task_race_from_array((xtarray)(failedTaskFuncRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array function task.race await.
    exit /b 1
)
findstr /C:"repeatedFailedRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed task.race future fixture.
    exit /b 1
)
findstr /C:"long long repeatedRaceB = (*(long long*)xl6_future_await_value(repeatedFailedRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed task.race future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedRace = xl6_task_cancel_value(repeatedFailedRace, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed task.race future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelRepeatedRace = (*(long long*)xl6_future_await_value(repeatedFailedRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"failedStringAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedStringAll = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all named future fixture.
    exit /b 1
)
findstr /C:"firstFailedStringAll = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(repeatedFailedStringAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedStringAll = xl6_task_cancel_value(repeatedFailedStringAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed string task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedStringAll = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(repeatedFailedStringAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed string task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"failedTypedStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array string task.all await.
    exit /b 1
)
findstr /C:"failedStringRace = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedStringRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race named future fixture.
    exit /b 1
)
findstr /C:"firstFailedStringRace = xl6_future_await_cstr_copy(repeatedFailedStringRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedStringRace = xl6_task_cancel_value(repeatedFailedStringRace, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed string task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedStringRace = xl6_future_await_cstr_copy(repeatedFailedStringRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed string task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"failedTypedStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(failedStringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array string task.race await.
    exit /b 1
)
findstr /C:"failedTypedBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedBoolAllJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array bool task.all await.
    exit /b 1
)
findstr /C:"failedTypedBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(failedBoolRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array bool task.race await.
    exit /b 1
)
findstr /C:"failedTypedFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array float task.all await.
    exit /b 1
)
findstr /C:"failedTypedFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(failedFloatRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array float task.race await.
    exit /b 1
)
findstr /C:"failedBoolAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedBoolAll = xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all named future fixture.
    exit /b 1
)
findstr /C:"firstFailedBoolAll = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(repeatedFailedBoolAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedBoolAll = xl6_task_cancel_value(repeatedFailedBoolAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed bool task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedBoolAll = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(repeatedFailedBoolAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed bool task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"failedBoolRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.race temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedBoolRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.race named future fixture.
    exit /b 1
)
findstr /C:"firstFailedBoolRace = (*(int*)xl6_future_await_value(repeatedFailedBoolRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.race first await borrow.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedBoolRace = xl6_task_cancel_value(repeatedFailedBoolRace, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed bool task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedBoolRace = (*(int*)xl6_future_await_value(repeatedFailedBoolRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed bool task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"failedFloatAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedFloatAll = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all named future fixture.
    exit /b 1
)
findstr /C:"firstFailedFloatAll = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(repeatedFailedFloatAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedFloatAll = xl6_task_cancel_value(repeatedFailedFloatAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed float task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedFloatAll = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(repeatedFailedFloatAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed float task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"failedFloatRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.race temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedFloatRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.race named future fixture.
    exit /b 1
)
findstr /C:"firstFailedFloatRace = (*(double*)xl6_future_await_value(repeatedFailedFloatRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.race first await borrow.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedFloatRace = xl6_task_cancel_value(repeatedFailedFloatRace, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed float task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedFloatRace = (*(double*)xl6_future_await_value(repeatedFailedFloatRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed float task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"task.all requires a non-empty future array" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty-array guard.
    exit /b 1
)
findstr /C:"task.race requires a non-empty future array" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty-array guard.
    exit /b 1
)
findstr /C:"task.all source array is null" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing defensive task.all null source array guard.
    exit /b 1
)
findstr /C:"task.race source array is null" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing defensive task.race null source array guard.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static void xl6_task_future_array_release\(xfuture\*\* futures, int count\)[\s\S]*?if \( futures\[i\] != 0 \) \{ xFutureRelease\(futures\[i\]\); \}[\s\S]*?xrtFree\(futures\);' -or $text -notmatch 'if \( futures\[i\] == 0 \) \{[\s\S]*?xl6_task_future_array_release\(arg->futures, i\);[\s\S]*?xrtFree\(arg\);[\s\S]*?task\.all source future is null'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing task.all null-source retained-prefix cleanup.
    exit /b 1
)
findstr /C:"defaultJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize typed future array as an empty array.
    exit /b 1
)
findstr /C:"defaultArrayAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default typed-array empty path.
    exit /b 1
)
findstr /C:"expect_i64(defaultArrayAllState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default typed-array empty assertion.
    exit /b 1
)
findstr /C:"defaultArrayRace = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(defaultJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default typed-array empty path.
    exit /b 1
)
findstr /C:"expect_i64(defaultArrayRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default typed-array empty assertion.
    exit /b 1
)
findstr /C:"emptyStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(emptyStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty string typed-array path.
    exit /b 1
)
findstr /C:"emptyStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(emptyStringJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty string typed-array path.
    exit /b 1
)
findstr /C:"defaultStringJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize string future array as empty.
    exit /b 1
)
findstr /C:"defaultStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default string typed-array path.
    exit /b 1
)
findstr /C:"defaultStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(defaultStringJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default string typed-array path.
    exit /b 1
)
findstr /C:"emptyBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(emptyBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty bool typed-array path.
    exit /b 1
)
findstr /C:"emptyBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(emptyBoolJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty bool typed-array path.
    exit /b 1
)
findstr /C:"expect_i64(emptyBoolRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty bool assertion.
    exit /b 1
)
findstr /C:"defaultBoolJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize bool future array as empty.
    exit /b 1
)
findstr /C:"defaultBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default bool typed-array path.
    exit /b 1
)
findstr /C:"defaultBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(defaultBoolJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default bool typed-array path.
    exit /b 1
)
findstr /C:"emptyFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(emptyFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty float typed-array path.
    exit /b 1
)
findstr /C:"emptyFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(emptyFloatJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty float typed-array path.
    exit /b 1
)
findstr /C:"defaultFloatJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize float future array as empty.
    exit /b 1
)
findstr /C:"defaultFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default float typed-array path.
    exit /b 1
)
findstr /C:"defaultFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(defaultFloatJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default float typed-array path.
    exit /b 1
)
findstr /C:"expect_i64(defaultFloatRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default float assertion.
    exit /b 1
)
findstr /C:"emptyTaskBoxAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(emptyTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty class typed-array path.
    exit /b 1
)
findstr /C:"emptyTaskBoxRaceFuture = xl6_task_race_from_array((xtarray)(emptyTaskBoxJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty class future path.
    exit /b 1
)
findstr /C:"emptyTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(emptyTaskBoxRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty class await path.
    exit /b 1
)
findstr /C:"defaultTaskBoxJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize class future array as empty.
    exit /b 1
)
findstr /C:"defaultTaskBoxAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default class typed-array path.
    exit /b 1
)
findstr /C:"defaultTaskBoxRaceFuture = xl6_task_race_from_array((xtarray)(defaultTaskBoxJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default class future path.
    exit /b 1
)
findstr /C:"defaultTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(defaultTaskBoxRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default class await path.
    exit /b 1
)
findstr /C:"expect_i64(defaultTaskBoxRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default class assertion.
    exit /b 1
)
findstr /C:"emptyTaskFuncAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(emptyTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty function typed-array path.
    exit /b 1
)
findstr /C:"emptyTaskFuncRaceFuture = xl6_task_race_from_array((xtarray)(emptyTaskFuncJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty function future path.
    exit /b 1
)
findstr /C:"emptyTaskFuncRace = (*(XL6_Function*)xl6_future_await_value(emptyTaskFuncRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty function await path.
    exit /b 1
)
findstr /C:"defaultTaskFuncJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize function future array as empty.
    exit /b 1
)
findstr /C:"defaultTaskFuncAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(defaultTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default function typed-array path.
    exit /b 1
)
findstr /C:"defaultTaskFuncRaceFuture = xl6_task_race_from_array((xtarray)(defaultTaskFuncJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default function future path.
    exit /b 1
)
findstr /C:"defaultTaskFuncRace = (*(XL6_Function*)xl6_future_await_value(defaultTaskFuncRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default function await path.
    exit /b 1
)
findstr /C:"expect_i64(defaultTaskFuncRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default function assertion.
    exit /b 1
)
findstr /C:"task.all source future is null" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all null source future guard.
    exit /b 1
)
findstr /C:"task.race source future is null" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race null source future guard.
    exit /b 1
)
findstr /C:"xl6_task_future_array_release(arg->futures, i);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release only retained task.all source prefix after null source failure.
    exit /b 1
)
findstr /C:"missingFuture, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named null future literal input as borrowed.
    exit /b 1
)
findstr /C:"xfuture* missingStringFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string null future fixture.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed future array creation for null source fixture.
    exit /b 1
)
findstr /C:"nullStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string task.all null source path.
    exit /b 1
)
findstr /C:"nullStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(nullStringJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string task.race null source path.
    exit /b 1
)
findstr /C:"expect_i64(nullStringRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string task.race null source assertion.
    exit /b 1
)
findstr /C:"missingBoolFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed bool null future fixture.
    exit /b 1
)
findstr /C:"nullBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed bool task.all null source path.
    exit /b 1
)
findstr /C:"nullBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(nullBoolJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed bool task.race null source path.
    exit /b 1
)
findstr /C:"expect_i64(nullBoolRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed bool task.race null source assertion.
    exit /b 1
)
findstr /C:"missingFloatFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed float null future fixture.
    exit /b 1
)
findstr /C:"nullFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed float task.all null source path.
    exit /b 1
)
findstr /C:"nullFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(nullFloatJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed float task.race null source path.
    exit /b 1
)
findstr /C:"expect_i64(nullFloatRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed float task.race null source assertion.
    exit /b 1
)
findstr /C:"missingTaskBoxFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed class null future fixture.
    exit /b 1
)
findstr /C:"nullTaskBoxAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed class task.all null source path.
    exit /b 1
)
findstr /C:"nullTaskBoxRaceFuture = xl6_task_race_from_array((xtarray)(nullTaskBoxJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed class task.race null source future path.
    exit /b 1
)
findstr /C:"nullTaskBoxRace = (*(XL_Class_TaskBox*)xl6_future_await_value(nullTaskBoxRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed class task.race named future await path.
    exit /b 1
)
findstr /C:"expect_i64(nullTaskBoxRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed class task.race null source assertion.
    exit /b 1
)
findstr /C:"missingTaskFuncFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed function null future fixture.
    exit /b 1
)
findstr /C:"nullTaskFuncAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed function task.all null source path.
    exit /b 1
)
findstr /C:"nullTaskFuncRaceFuture = xl6_task_race_from_array((xtarray)(nullTaskFuncJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed function task.race null source future path.
    exit /b 1
)
findstr /C:"nullTaskFuncRace = (*(XL6_Function*)xl6_future_await_value(nullTaskFuncRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed function task.race named future await path.
    exit /b 1
)
findstr /C:"expect_i64(nullTaskFuncRaceState, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed function task.race null source assertion.
    exit /b 1
)
findstr /C:"typedMixedAllSource = addAsync(160LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.all source fixture.
    exit /b 1
)
findstr /C:"typedMixedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedMixedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.all null source path.
    exit /b 1
)
findstr /C:"typedMixedAllSourceValue = (*(long long*)xl6_future_await_value(typedMixedAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array mixed task.all source awaitable after null failure.
    exit /b 1
)
findstr /C:"expect_i64(typedMixedAllSourceValue, 161LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.all source value assertion.
    exit /b 1
)
findstr /C:"typedMixedRaceSource = addAsync(170LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.race source fixture.
    exit /b 1
)
findstr /C:"typedMixedRace = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(typedMixedRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.race null source path.
    exit /b 1
)
findstr /C:"typedMixedRaceSourceValue = (*(long long*)xl6_future_await_value(typedMixedRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array mixed task.race source awaitable after null failure.
    exit /b 1
)
findstr /C:"expect_i64(typedMixedRaceSourceValue, 171LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.race source value assertion.
    exit /b 1
)
findstr /C:"nullFirstAllSource = addAsync(180LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.all source fixture.
    exit /b 1
)
findstr /C:"nullFirstAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(nullFirstAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.all null source path.
    exit /b 1
)
findstr /C:"nullFirstAllSourceValue = (*(long long*)xl6_future_await_value(nullFirstAllSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array null-first task.all source awaitable after null failure.
    exit /b 1
)
findstr /C:"expect_i64(nullFirstAllSourceValue, 181LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.all source value assertion.
    exit /b 1
)
findstr /C:"nullFirstRaceSource = addAsync(190LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.race source fixture.
    exit /b 1
)
findstr /C:"nullFirstRace = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(nullFirstRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.race null source path.
    exit /b 1
)
findstr /C:"nullFirstRaceSourceValue = (*(long long*)xl6_future_await_value(nullFirstRaceSource" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array null-first task.race source awaitable after null failure.
    exit /b 1
)
findstr /C:"expect_i64(nullFirstRaceSourceValue, 191LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.race source value assertion.
    exit /b 1
)
findstr /C:"addAsync(140LL, 1LL), 1, missingFuture, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release owned task.all literal input before null future failure.
    exit /b 1
)
findstr /C:"addAsync(150LL, 1LL), 1, missingFuture, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release owned task.race literal input before null future failure.
    exit /b 1
)
findstr /C:"static xfuture* xl6_future_add_ref_if_needed(xfuture* pFuture)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing safe future add-ref helper for conditional future arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalCancelBorrowed\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? xl6_future_add_ref_if_needed\(conditionalCancelSource\) : addAsync\(210LL, 1LL\)\), 1\)' -or $text -notmatch 'conditionalCancelOwned\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? xl6_future_add_ref_if_needed\(conditionalCancelSource\) : addAsync\(220LL, 1LL\)\), 1\)' -or $text -notmatch 'conditionalCancelMissing\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? addAsync\(230LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalMissingFuture\)\), 1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional future task.cancel ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?\(chooseConditionalAll \? xl6_future_add_ref_if_needed\(conditionalAllSource\) : addAsync\(250LL, 1LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalAllSource\) : addAsync\(260LL, 1LL\)\), 1' -or $text -notmatch 'conditionalRaceValue\s*=\s*xl6_future_await_i64_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalRace \? xl6_future_add_ref_if_needed\(conditionalRaceSource\) : addAsync\(280LL, 1LL\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional future task.all/race ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalAll \? xl6_future_add_ref_if_needed\(typedConditionalAllSource\) : addAsync\(320LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalAllSource\) : addAsync\(330LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'typedConditionalRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalRace \? xl6_future_add_ref_if_needed\(typedConditionalRaceSource\) : addAsync\(350LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalRaceValue\s*=\s*xl6_future_await_i64_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(!xrtTypedArrayAppend\(typedConditionalAllJobs, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);' -or $text -notmatch 'if \(!xrtTypedArrayAppend\(typedConditionalRaceJobs, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalBoolAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalBoolAll \? xl6_future_add_ref_if_needed\(typedConditionalBoolAllSource\) : flipAsync\(1\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalBoolAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalBoolAllSource\) : flipAsync\(0\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalBoolAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalBoolAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalBoolAllJobs\), xrtTypeBool\(\)' -or $text -notmatch 'typedConditionalBoolRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalBoolRace \? xl6_future_add_ref_if_needed\(typedConditionalBoolRaceSource\) : flipAsync\(1\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalBoolRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalBoolRaceValue\s*=\s*xl6_future_await_i32_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalBoolRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional bool future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalFloatAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalFloatAll \? xl6_future_add_ref_if_needed\(typedConditionalFloatAllSource\) : addHalfAsync\(420\.5\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalFloatAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalFloatAllSource\) : addHalfAsync\(430\.5\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalFloatAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalFloatAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalFloatAllJobs\), xrtTypeFloat\(\)' -or $text -notmatch 'typedConditionalFloatRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalFloatRace \? xl6_future_add_ref_if_needed\(typedConditionalFloatRaceSource\) : addHalfAsync\(450\.5\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalFloatRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalFloatRaceValue\s*=\s*xl6_future_await_f64_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalFloatRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional float future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); foreach($name in @('typedConditionalBoolAllJobs','typedConditionalBoolRaceJobs','typedConditionalFloatAllJobs','typedConditionalFloatRaceJobs')){ $pattern='if \(!xrtTypedArrayAppend\(' + [regex]::Escape($name) + ', &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'; if($text -notmatch $pattern){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array scalar future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalStringAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalStringAll \? xl6_future_add_ref_if_needed\(typedConditionalStringAllSource\) : greetAsync\([^)]*typed-cond-string-all-alt[^)]*\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalStringAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalStringAllSource\) : greetAsync\([^)]*typed-cond-string-all-owned[^)]*\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalStringAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalStringAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalStringAllJobs\), xrtTypeString\(\)' -or $text -notmatch 'typedConditionalStringRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalStringRace \? xl6_future_add_ref_if_needed\(typedConditionalStringRaceSource\) : greetAsync\([^)]*typed-cond-string-race-owned[^)]*\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalStringRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalStringRaceValue\s*=\s*xl6_future_await_cstr_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalStringRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional string future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalTaskBoxAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalTaskBoxAll \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxAllSource\) : makeTaskBox\(720LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskBoxAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxAllSource\) : makeTaskBox\(730LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskBoxAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalTaskBoxAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalTaskBoxAllJobs\), xl6_type_desc_XL_Class_TaskBox\(\)' -or $text -notmatch 'typedConditionalTaskBoxRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalTaskBoxRace \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxRaceSource\) : makeTaskBox\(750LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskBoxRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalTaskBoxRaceFuture\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedConditionalTaskBoxRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional class future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalTaskFuncAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalTaskFuncAll \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncAllSource\) : makeTaskAdder\(770LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskFuncAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncAllSource\) : makeTaskAdder\(780LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskFuncAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalTaskFuncAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalTaskFuncAllJobs\), xrtTypeFunction\(\)' -or $text -notmatch 'typedConditionalTaskFuncRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalTaskFuncRace \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncRaceSource\) : makeTaskAdder\(800LL\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalTaskFuncRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalTaskFuncRaceFuture\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedConditionalTaskFuncRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional function future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); foreach($name in @('typedConditionalStringAllJobs','typedConditionalStringRaceJobs','typedConditionalTaskBoxAllJobs','typedConditionalTaskBoxRaceJobs','typedConditionalTaskFuncAllJobs','typedConditionalTaskFuncRaceJobs')){ $pattern='if \(!xrtTypedArrayAppend\(' + [regex]::Escape($name) + ', &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'; if($text -notmatch $pattern){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array resource future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalNullAll\s*=\s*xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?\(chooseConditionalNull \? addAsync\(290LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalNullLiteralFuture\)\), 1' -or $text -notmatch 'conditionalNullRace\s*=\s*xl6_future_await_i64_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalNull \? addAsync\(300LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalNullLiteralFuture\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional null future literal normalization.
    exit /b 1
)
findstr /C:"bool xFutureValueIsOwned(xfuture* pFuture);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing future value ownership query declaration.
    exit /b 1
)
findstr /C:"valueOwned = xFutureValueIsOwned(pFuture);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing future await release ownership query.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalStringAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeString\(\),[\s\S]*?\(chooseConditionalStringAll \? xl6_future_add_ref_if_needed\(conditionalStringAllSource\) : greetAsync\([\s\S]*?\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalStringAllSource\) : greetAsync\([\s\S]*?\), 1' -or $text -notmatch 'conditionalStringRaceValue\s*=\s*xl6_future_await_cstr_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalStringRace \? xl6_future_add_ref_if_needed\(conditionalStringRaceSource\) : greetAsync\([\s\S]*?\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional string task.all/race resource ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalTaskBoxAllFuture\s*=\s*xl6_task_all_values\(xl6_type_desc_XL_Class_TaskBox\(\),[\s\S]*?\(chooseConditionalTaskBoxAll \? xl6_future_add_ref_if_needed\(conditionalTaskBoxAllSource\) : makeTaskBox\(620LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalTaskBoxAllSource\) : makeTaskBox\(630LL\)\), 1' -or $text -notmatch 'conditionalTaskBoxRaceFuture\s*=\s*xl6_task_race_values\([\s\S]*?\(chooseConditionalTaskBoxRace \? xl6_future_add_ref_if_needed\(conditionalTaskBoxRaceSource\) : makeTaskBox\(650LL\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional class task.all/race resource ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalTaskFuncAllValues\s*=\s*xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeFunction\(\),[\s\S]*?\(chooseConditionalTaskFuncAll \? xl6_future_add_ref_if_needed\(conditionalTaskFuncAllSource\) : makeTaskAdder\(670LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalTaskFuncAllSource\) : makeTaskAdder\(680LL\)\), 1' -or $text -notmatch 'conditionalTaskFuncRaceValue\s*=\s*xl6_future_await_function_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalTaskFuncRace \? xl6_future_add_ref_if_needed\(conditionalTaskFuncRaceSource\) : makeTaskAdder\(700LL\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional function task.all/race resource ownership normalization.
    exit /b 1
)
findstr /C:"futureArg = va_arg(ap, xfuture*);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task all/race vararg failure cleanup future read.
    exit /b 1
)
findstr /C:"ownedArg = va_arg(ap, int);" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task all/race vararg failure cleanup ownership read.
    exit /b 1
)
findstr /C:"if ( ownedArg && futureArg != 0 ) { xFutureRelease(futureArg); }" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task all/race vararg failure cleanup release.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); $checks=@('static xfuture\* xl6_task_all_values\([\s\S]*?if \( futures == 0 \) \{[\s\S]*?futureArg = va_arg\(ap, xfuture\*\);[\s\S]*?ownedArg = va_arg\(ap, int\);[\s\S]*?if \( ownedArg && futureArg != 0 \) \{ xFutureRelease\(futureArg\); \}[\s\S]*?out of memory while starting task\.all','static xfuture\* xl6_task_all_values\([\s\S]*?if \( owned == 0 \) \{[\s\S]*?futureArg = va_arg\(ap, xfuture\*\);[\s\S]*?ownedArg = va_arg\(ap, int\);[\s\S]*?if \( ownedArg && futureArg != 0 \) \{ xFutureRelease\(futureArg\); \}[\s\S]*?xrtFree\(futures\);[\s\S]*?out of memory while starting task\.all','static xfuture\* xl6_task_race_values\([\s\S]*?if \( futures == 0 \) \{[\s\S]*?futureArg = va_arg\(ap, xfuture\*\);[\s\S]*?ownedArg = va_arg\(ap, int\);[\s\S]*?if \( ownedArg && futureArg != 0 \) \{ xFutureRelease\(futureArg\); \}[\s\S]*?out of memory while starting task\.race','static xfuture\* xl6_task_race_values\([\s\S]*?if \( owned == 0 \) \{[\s\S]*?futureArg = va_arg\(ap, xfuture\*\);[\s\S]*?ownedArg = va_arg\(ap, int\);[\s\S]*?if \( ownedArg && futureArg != 0 \) \{ xFutureRelease\(futureArg\); \}[\s\S]*?xrtFree\(futures\);[\s\S]*?out of memory while starting task\.race'); foreach($p in $checks){ if($text -notmatch $p){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_api.c missing task all/race vararg allocation failure cleanup shape.
    exit /b 1
)
findstr /C:"if ( pFuture == 0 )" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.cancel null future guard.
    exit /b 1
)
findstr /C:"cancelMissing = xl6_task_cancel_value(missingCancelFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass named null cancel future as borrowed.
    exit /b 1
)
findstr /C:"missingCancelStringFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing string null cancel future fixture.
    exit /b 1
)
findstr /C:"cancelMissingString = xl6_task_cancel_value(missingCancelStringFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass string null cancel future as borrowed.
    exit /b 1
)
findstr /C:"expect_i64(cancelMissingString, 0LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing string null cancel assertion.
    exit /b 1
)
findstr /C:"missingCancelBoolFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing bool null cancel future fixture.
    exit /b 1
)
findstr /C:"cancelMissingBool = xl6_task_cancel_value(missingCancelBoolFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass bool null cancel future as borrowed.
    exit /b 1
)
findstr /C:"expect_i64(cancelMissingBool, 0LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing bool null cancel assertion.
    exit /b 1
)
findstr /C:"missingCancelFloatFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing float null cancel future fixture.
    exit /b 1
)
findstr /C:"cancelMissingFloat = xl6_task_cancel_value(missingCancelFloatFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass float null cancel future as borrowed.
    exit /b 1
)
findstr /C:"expect_i64(cancelMissingFloat, 0LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing float null cancel assertion.
    exit /b 1
)
findstr /C:"missingCancelTaskBoxFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing class null cancel future fixture.
    exit /b 1
)
findstr /C:"cancelMissingTaskBox = xl6_task_cancel_value(missingCancelTaskBoxFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass class null cancel future as borrowed.
    exit /b 1
)
findstr /C:"expect_i64(cancelMissingTaskBox, 0LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing class null cancel assertion.
    exit /b 1
)
findstr /C:"missingCancelTaskFuncFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing function null cancel future fixture.
    exit /b 1
)
findstr /C:"cancelMissingTaskFunc = xl6_task_cancel_value(missingCancelTaskFuncFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass function null cancel future as borrowed.
    exit /b 1
)
findstr /C:"expect_i64(cancelMissingTaskFunc, 0LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing function null cancel assertion.
    exit /b 1
)
findstr /C:"if ( !xl6_error_has() )" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing await null-future error preservation guard.
    exit /b 1
)

release\xl.exe build test\cases\cgen_task_api.xl test\cases\cgen_class_host.c -o build\test\cgen_task_api.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$exe=(Resolve-Path 'build\test\cgen_task_api.exe').Path; for($i=1;$i -le 4;$i++){ $psi=[System.Diagnostics.ProcessStartInfo]::new(); $psi.FileName=$exe; $psi.WorkingDirectory=(Get-Location).Path; $psi.UseShellExecute=$false; $p=[System.Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(120000)){ try{$p.Kill()}catch{}; Write-Host ('cgen_task_api.exe run '+$i+' timed out.'); exit 124 }; $code=$p.ExitCode; $p.Dispose(); if($code -ne 0){ Write-Host ('cgen_task_api.exe run '+$i+' failed.'); exit $code } }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_task_spawn_range_contains.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_task_spawn_range_contains.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_spawn_range_contains.xl --emit build\test\cgen_task_spawn_range_contains.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_range_contains_i64" build\test\cgen_task_spawn_range_contains.c >nul
if errorlevel 1 (
    echo cgen_task_spawn_range_contains.c missing range contains helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn_range_contains.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(0LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'makeSpawnValue\(2LL\)[\s\S]*?makeSpawnValue\(1LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'xl6_xrt_retain\(emptySpawnArg\)[\s\S]*?makeSpawnValue\(0LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'rangeContainsDynamic\s*=\s*xl6_range_contains_i64'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn_range_contains.c missing lowered range-contains task.spawn arguments.
    exit /b 1
)

release\xl.exe check test\cases\cgen_task_spawn_fstring.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_task_spawn_fstring.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_spawn_fstring.xl --emit build\test\cgen_task_spawn_fstring.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_fstr_join" build\test\cgen_task_spawn_fstring.c >nul
if errorlevel 1 (
    echo cgen_task_spawn_fstring.c missing f-string join helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn_fstring.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(40LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'fstringDynamicSpawn\s*=\s*xl6_xrt_fstr_join'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn_fstring.c missing lowered f-string task.spawn argument.
    exit /b 1
)

release\xl.exe check test\cases\cgen_task_spawn.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_task_spawn_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn runtime helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_argv_borrow(XL6_Function fn, int argc, xvalue* argv)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed argv function bridge.
    exit /b 1
)
findstr /C:"value = xl6_function_call_argv_borrow(arg->fn, arg->argc, arg->argv);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should call task.spawn functions through borrowed argv bridge.
    exit /b 1
)
findstr /C:"void xrtTypeDropValue(const xrt_type_desc* pType, void* pObj);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing XRT typed value drop declaration.
    exit /b 1
)
findstr /C:"static void* xl6_async_typed_box_alloc" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box allocator.
    exit /b 1
)
findstr /C:"xrtTypeDropValue(box->type, pValue);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box drop.
    exit /b 1
)
findstr /C:"box = xl6_async_typed_box_alloc(arg->resultType, arg->resultSize);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box allocation.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_typed_value_box;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed result box free hook.
    exit /b 1
)
findstr /C:"value = va_arg(ap, xvalue);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn vararg failure cleanup value read.
    exit /b 1
)
findstr /C:"if ( value != 0 ) { xvoUnref(value); }" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn vararg failure cleanup unref.
    exit /b 1
)
findstr /C:"arg->fn.invoke_var == 0 && arg->fn.invoke_xcall == 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should accept invoke_xcall-only function adapters.
    exit /b 1
)
findstr /C:"static xfuture* xl6_task_spawn_start(XL6_Function fn, int retainBorrow, int argc, xvalue* argv" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn retainBorrow start signature.
    exit /b 1
)
findstr /C:"static xfuture* xl6_task_spawn_values(XL6_Function fn, const xrt_type_desc* resultType, size_t resultSize, const char* file, int line, int column, int retainBorrow, int argc, ...)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn retainBorrow values signature.
    exit /b 1
)
findstr /C:"if ( retainBorrow && fn.retain != 0 && fn.env != 0 )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed task.spawn function retain.
    exit /b 1
)
findstr /C:"if ( !retainBorrow && fn.release != 0 && fn.env != 0 ) { fn.release(fn.env); }" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn function failure release.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('static xfuture* xl6_task_spawn_values('); if($idx -lt 0){exit 1}; $end=$text.IndexOf('future = xl6_task_spawn_start', $idx); if($end -lt 0){exit 1}; $slice=$text.Substring($idx,$end-$idx); if($slice -match 'fn\.retain\(fn\.env\)'){exit 1}"
if errorlevel 1 (
    echo cgen_task_spawn.c should not retain task.spawn functions before ownership classification.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_task_spawn_values\([\s\S]*?if \( argv == 0 \) \{[\s\S]*?va_start\(ap, argc\);[\s\S]*?value = va_arg\(ap, xvalue\);[\s\S]*?if \( value != 0 \) \{ xvoUnref\(value\); \}[\s\S]*?va_end\(ap\);[\s\S]*?if \( !retainBorrow && fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while preparing task\.spawn arguments'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn argv allocation failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( arg == 0 \) \{[\s\S]*?if \( fn\.release != 0 && fn\.env != 0 \) \{ fn\.release\(fn\.env\); \}[\s\S]*?out of memory while starting task\.spawn' -or $text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( arg->argv == 0 \) \{[\s\S]*?xl6_task_spawn_arg_release\(arg\);[\s\S]*?out of memory while copying task\.spawn arguments' -or $text -notmatch 'static xfuture\* xl6_task_spawn_start\([\s\S]*?if \( future == 0 \) \{[\s\S]*?xl6_task_spawn_arg_release\(arg\);[\s\S]*?return xl6_async_require_future\(future, file, line, column\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn start failure cleanup.
    exit /b 1
)
findstr /C:"if ( arg->resultSize == 0 )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn success branch.
    exit /b 1
)
findstr /C:"voidSpawnFuture = xl6_task_spawn_values(voidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn future fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_value(voidSpawnFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.spawn void await.
    exit /b 1
)
findstr /C:"cancelTemporaryVoidSpawn = xl6_task_cancel_value(xl6_task_spawn_values(voidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary no-return task.spawn cancel fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_cancel_value\(\(chooseConditionalVoidCancel \? xl6_future_add_ref_if_needed\(conditionalVoidCancelSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1\)' -or $text -notmatch 'conditionalVoidOwnedCancel\s*=\s*xl6_task_cancel_value\(\(chooseConditionalVoidCancel \? xl6_future_add_ref_if_needed\(conditionalVoidCancelSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional no-return task.cancel ownership normalization.
    exit /b 1
)
findstr /C:"if ( arg->itemType == xrtTypeNull() )" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing void task.all runtime branch.
    exit /b 1
)
findstr /C:"voidAllFuture = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.all future fixture.
    exit /b 1
)
findstr /C:"voidRaceFuture = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing no-return task.race future fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_void_release\(xl6_task_all_values\(xrtTypeNull\(\),[\s\S]*?\(chooseConditionalVoidAll \? xl6_future_add_ref_if_needed\(conditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1' -or $text -notmatch 'xl6_future_await_void_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalVoidRace \? xl6_future_add_ref_if_needed\(conditionalVoidRaceSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional no-return task.all/race ownership normalization.
    exit /b 1
)
findstr /C:"typedVoidAllFuture = xl6_task_all_from_array((xtarray)(typedVoidAllJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array no-return task.all fixture.
    exit /b 1
)
findstr /C:"typedVoidRaceFuture = xl6_task_race_from_array((xtarray)(typedVoidRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array no-return task.race fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedConditionalVoidAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalVoidAll \? xl6_future_add_ref_if_needed\(typedConditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalVoidAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalVoidAllSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalVoidAllJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalVoidAllFuture\s*=\s*xl6_task_all_from_array\(\(xtarray\)\(typedConditionalVoidAllJobs\), xrtTypeNull\(\)' -or $text -notmatch 'typedConditionalVoidRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'chooseTypedConditionalVoidRace \? xl6_future_add_ref_if_needed\(typedConditionalVoidRaceSource\) : xl6_task_spawn_values\(voidJob, xrtTypeNull\(\), 0,[\s\S]*?\)[\s\S]*?xrtTypedArrayAppend\(typedConditionalVoidRaceJobs,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'typedConditionalVoidRaceFuture\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedConditionalVoidRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array conditional no-return future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); foreach($name in @('typedConditionalVoidAllJobs','typedConditionalVoidRaceJobs')){ $pattern='if \(!xrtTypedArrayAppend\(' + [regex]::Escape($name) + ', &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'; if($text -notmatch $pattern){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array conditional no-return append failure cleanup.
    exit /b 1
)
findstr /C:"emptyVoidJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return future array fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_all_from_array((xtarray)(emptyVoidJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return task.all typed-array await path.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_race_from_array((xtarray)(emptyVoidJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty no-return task.race typed-array await path.
    exit /b 1
)
findstr /C:"defaultVoidJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return future array fixture.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_all_from_array((xtarray)(defaultVoidJobs), xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return task.all typed-array await path.
    exit /b 1
)
findstr /C:"xl6_future_await_void_release(xl6_task_race_from_array((xtarray)(defaultVoidJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing default no-return task.race typed-array await path.
    exit /b 1
)
findstr /C:"pendingVoidSpawn = xl6_task_spawn_values(slowVoidTask, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.spawn fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingVoidSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending no-return task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingVoidAllGroup = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.all fixture.
    exit /b 1
)
findstr /C:"pendingVoidRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending no-return task.race fixture.
    exit /b 1
)
findstr /C:"failedVoidSpawn = xl6_task_spawn_values(throwingVoidJob, xrtTypeNull(), 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.spawn fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(failedVoidSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed no-return task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"failedVoidAll = xl6_task_all_values(xrtTypeNull()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.all fixture.
    exit /b 1
)
findstr /C:"failedVoidRace = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed no-return task.race fixture.
    exit /b 1
)
findstr /C:"XL6_Function defaultJob = xl6_fnvalue_addDefault_arity1_make();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing optional-parameter adapter task.spawn case.
    exit /b 1
)
findstr /C:"xfuture* defaultFuture = xl6_task_spawn_values(defaultJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'defaultFuture\s*=\s*xl6_task_spawn_values\(defaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(41LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn argv bridge.
    exit /b 1
)
findstr /C:"defaultFutureA = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn first await.
    exit /b 1
)
findstr /C:"defaultFutureB = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named optional adapter task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(defaultFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed optional adapter task.spawn cancel guard.
    exit /b 1
)
findstr /C:"defaultFutureAfterCancel = (*(long long*)xl6_future_await_value(defaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing post-cancel optional adapter task.spawn await.
    exit /b 1
)
findstr /C:"XL6_Function missingJob = {0};" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingJobValue = xl6_future_await_i64_release(xl6_task_spawn_values(missingJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn call.
    exit /b 1
)
findstr /C:"task.spawn requires a callable func value" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty function value task.spawn diagnostic.
    exit /b 1
)
findstr /C:"XL6_Function missingStringJob = {0};" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingStringJobValue = xl6_future_await_cstr_release(xl6_task_spawn_values(missingStringJob, xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn call.
    exit /b 1
)
findstr /C:"missingStringJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty string function value task.spawn assertion.
    exit /b 1
)
findstr /C:"XL6_Function missingBoolJob = {0};" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingBoolJobValue = xl6_future_await_i32_release(xl6_task_spawn_values(missingBoolJob, xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn call.
    exit /b 1
)
findstr /C:"missingBoolJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty bool function value task.spawn assertion.
    exit /b 1
)
findstr /C:"XL6_Function missingFloatJob = {0};" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn fixture.
    exit /b 1
)
findstr /C:"missingFloatJobValue = xl6_future_await_f64_release(xl6_task_spawn_values(missingFloatJob, xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn call.
    exit /b 1
)
findstr /C:"missingFloatJobState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing empty float function value task.spawn assertion.
    exit /b 1
)
findstr /C:"xfuture* repeatedSpawn = xl6_task_spawn_values(addJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 27, 1, 2'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed named function values for task.spawn.
    exit /b 1
)
findstr /C:"long long repeatedB = (*(long long*)xl6_future_await_value(repeatedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for named task.spawn future.
    exit /b 1
)
findstr /C:"postCancelRepeatedSpawn = (*(long long*)xl6_future_await_value(repeatedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed task.spawn int future awaitable after cancel.
    exit /b 1
)
findstr /C:"directClosureValue = xl6_future_await_i64_release(xl6_task_spawn_values(makeAdder(40LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure expression task.spawn await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(makeAdder\(40LL\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 37, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned direct closure function values for task.spawn.
    exit /b 1
)
findstr /C:"directClosureFuture = xl6_task_spawn_values(makeAdder(41LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure expression named future.
    exit /b 1
)
findstr /C:"directClosureA = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first repeated await for direct closure future.
    exit /b 1
)
findstr /C:"directClosureB = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing second repeated await for direct closure future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(directClosureFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel completed direct closure future as borrowed.
    exit /b 1
)
findstr /C:"directClosureAfterCancel = (*(long long*)xl6_future_await_value(directClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed direct closure future awaitable after cancel.
    exit /b 1
)
findstr /C:"directClosureAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for direct closure task.all.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(makeAdder(50LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first direct closure task.all source.
    exit /b 1
)
findstr /C:"xrtTypedArrayAppend(directClosureAllJobs, &__xl6_typed_future_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array append for direct closure task.all.
    exit /b 1
)
findstr /C:"xFutureRelease(__xl6_typed_future_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary typed futures after direct closure task.all append.
    exit /b 1
)
findstr /C:"directClosureAllValues = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(directClosureAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all await for direct closure futures.
    exit /b 1
)
findstr /C:"directClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for direct closure future.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(makeAdder(70LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing direct closure task.race source.
    exit /b 1
)
findstr /C:"awaitedClosureValue = xl6_future_await_i64_release(xl6_task_spawn_values(xl6_future_await_function_release(makeAsyncAdder(80LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure expression task.spawn await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(xl6_future_await_function_release\(makeAsyncAdder\(80LL\), [^,]*, \d+, 44\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 38, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned awaited closure function values for task.spawn.
    exit /b 1
)
findstr /C:"awaitedClosureFuture = xl6_task_spawn_values(xl6_future_await_function_release(makeAsyncAdder(81LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure expression named future.
    exit /b 1
)
findstr /C:"awaitedClosureA = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first repeated await for awaited closure future.
    exit /b 1
)
findstr /C:"awaitedClosureB = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing second repeated await for awaited closure future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(awaitedClosureFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel completed awaited closure future as borrowed.
    exit /b 1
)
findstr /C:"awaitedClosureAfterCancel = (*(long long*)xl6_future_await_value(awaitedClosureFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed awaited closure future awaitable after cancel.
    exit /b 1
)
findstr /C:"awaitedClosureAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for awaited closure task.all.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl6_future_await_function_release(makeAsyncAdder(90LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first awaited closure task.all source.
    exit /b 1
)
findstr /C:"xrtTypedArrayAppend(awaitedClosureAllJobs, &__xl6_typed_future_" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array append for awaited closure task.all.
    exit /b 1
)
findstr /C:"awaitedClosureAllValues = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(awaitedClosureAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all await for awaited closure futures.
    exit /b 1
)
findstr /C:"awaitedClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for awaited closure future.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(xl6_future_await_function_release(makeAsyncAdder(110LL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing awaited closure task.race source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(1 \? makeAdder\(150LL\) : makeAdder\(160LL\)\)' -or $text -notmatch 'conditionalClosureFuture\s*=\s*xl6_task_spawn_values\(\(chooseFirstClosure \? makeAdder\(150LL\) : makeAdder\(160LL\)\)' -or $text -notmatch 'conditionalClosureAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'conditionalClosureRaceValue\s*=\s*xl6_future_await_i64_release\(xl6_task_race_values[\s\S]*?xl6_task_spawn_values\(\(1 \? makeAdder\(210LL\) : makeAdder\(220LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing conditional closure task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(1 \? makeAdder\(150LL\) : makeAdder\(160LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 42, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseFirstClosure \? makeAdder\(150LL\) : makeAdder\(160LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 45, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned conditional closure function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'borrowedConditionalClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\)' -or $text -notmatch 'borrowedConditionalClosureFuture\s*=\s*xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing borrowed conditional closure task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 50, 1, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseBorrowedClosure \? borrowedClosureLeft : borrowedClosureRight\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 53, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed conditional closure function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'mixedTaskOwnedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : makeAdder\(270LL\)\)' -or $text -notmatch 'mixedTaskBorrowedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : makeAdder\(280LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing mixed conditional closure branch copy for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : makeAdder\(270LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 45, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTaskClosure \? xl6_function_copy_value\(mixedTaskClosureSource\) : makeAdder\(280LL\)\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 48, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume normalized mixed conditional function values for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'memberClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(memberClosureBox\.op,' -or $text -notmatch 'memberClosureFuture\s*=\s*xl6_task_spawn_values\(memberClosureBox\.op,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing class function field task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_spawn_values\(memberClosureBox\.op, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 37, 1, 1' -or $text -notmatch 'xl6_task_spawn_values\(memberClosureBox\.op, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 40, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed class function fields for task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'borrowedMemberConditionalValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(\(chooseMemberClosure \? memberClosureBox\.op : memberClosureAlt\.op\)' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMemberClosure \? memberClosureBox\.op : memberClosureAlt\.op\), xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 49, 1, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should retain borrowed conditional class function fields for task.spawn.
    exit /b 1
)
findstr /C:"indexedClosureFuncs = xrtTypedArrayCreate(xrtTypeFunction(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed function array for indexed closure task.spawn.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'indexedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(xl6_function_from_callable\(\*\(xrt_callable\*\*\).*indexedClosureFuncs' -or $text -notmatch 'indexedClosureFuture\s*=\s*xl6_task_spawn_values\(xl6_function_from_callable\(\*\(xrt_callable\*\*\).*indexedClosureFuncs'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing indexed function item task.spawn source.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'indexedClosureValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(xl6_function_from_callable\([\s\S]*?indexedClosureFuncs[\s\S]*?, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 38, 0, 1' -or $text -notmatch 'indexedClosureFuture\s*=\s*xl6_task_spawn_values\(xl6_function_from_callable\([\s\S]*?indexedClosureFuncs[\s\S]*?, xrtTypeInt\(\), sizeof\(long long\), [^,]*, \d+, 41, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should consume owned indexed callable conversions for task.spawn.
    exit /b 1
)
findstr /C:"indexedClosureAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed future array for indexed closure task.all.
    exit /b 1
)
findstr /C:"indexedClosureRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing literal task.race await for indexed closure future.
    exit /b 1
)
findstr /C:"xfuture* repeatedFailedSpawn = xl6_task_spawn_values(throwingJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated failed task.spawn future fixture.
    exit /b 1
)
findstr /C:"long long failedSpawnB = (*(long long*)xl6_future_await_value(repeatedFailedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for failed task.spawn future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedSpawn = xl6_task_cancel_value(repeatedFailedSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedSpawn = (*(long long*)xl6_future_await_value(repeatedFailedSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnAllFailureGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnAllFailureSlowSource, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow task.spawn source as borrowed to task.all fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnAllFailureSlowValue = (*(long long*)xl6_future_await_value(spawnAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep slow task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"typedSpawnAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnAllFailureJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"typedSpawnAllFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedSpawnAllFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.all fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnAllFailureSlowValue = (*(long long*)xl6_future_await_value(typedSpawnAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array slow task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(typedSpawnAllFailureSlowSource, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array task.all slow task.spawn source completed after group failure.
    exit /b 1
)
findstr /C:"spawnRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnRaceFailureSlowLoser, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow task.spawn loser as borrowed to task.race fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnRaceFailureLoserValueB = (*(long long*)xl6_future_await_value(spawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled task.spawn race loser.
    exit /b 1
)
findstr /C:"spawnRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureGroup = xl6_task_race_from_array((xtarray)(typedSpawnRaceFailureJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureValue = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array task.race fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserValueA = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserValueB = (*(long long*)xl6_future_await_value(typedSpawnRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"xfuture* missingSpawnFuture = 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"nullSpawnAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(140LL)), xvoCreateInt((long long)(1LL))), 1, missingSpawnFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass owned task.spawn then borrowed null source to task.all.
    exit /b 1
)
findstr /C:"nullSpawnRace = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(150LL)), xvoCreateInt((long long)(1LL))), 1, missingSpawnFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass owned task.spawn then borrowed null source to task.race.
    exit /b 1
)
findstr /C:"nullFirstSpawnAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.all fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnFuture, 0, xl6_task_spawn_values(addJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed null source before owned task.spawn to task.all/race.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(160LL)), xvoCreateInt((long long)(1LL))), 1" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn source after null source for task.all.
    exit /b 1
)
findstr /C:"nullFirstSpawnRace = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.race fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(170LL)), xvoCreateInt((long long)(1LL))), 1" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned task.spawn source after null source for task.race.
    exit /b 1
)
findstr /C:"nullFirstSpawnAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.all assertion over task.spawn.
    exit /b 1
)
findstr /C:"nullFirstSpawnRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.race assertion over task.spawn.
    exit /b 1
)
findstr /C:"xfuture* missingSpawnStringFuture = 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"nullSpawnStringAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnStringFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed string null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnStringRace = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnStringAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race null source assertion.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.all fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnStringFuture, 0, xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed string null source before owned task.spawn.
    exit /b 1
)
findstr /C:"null-first-all" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source after null source for task.all.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringRace = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.race fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"null-first-race" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source after null source for task.race.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.all assertion over task.spawn.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.race assertion over task.spawn.
    exit /b 1
)
findstr /C:"xfuture* missingSpawnBoolFuture = 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"nullSpawnBoolAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(flipJob, xrtTypeBool(), sizeof(int)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned bool task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnBoolFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed bool null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnBoolRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnBoolAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnBoolRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race null source assertion.
    exit /b 1
)
findstr /C:"xfuture* missingSpawnFloatFuture = 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"nullSpawnFloatAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(halfJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned float task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnFloatFuture, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed float null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnFloatRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnFloatAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnFloatRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race null source assertion.
    exit /b 1
)
findstr /C:"failedStringValue = xl6_future_await_cstr_release(xl6_task_spawn_values(throwingStringJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn temporary future await.
    exit /b 1
)
findstr /C:"xfuture* repeatedFailedStringSpawn = xl6_task_spawn_values(throwingStringJob, xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedString = xl6_future_await_cstr_copy(repeatedFailedStringSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedStringSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed string task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedString = xl6_future_await_cstr_copy(repeatedFailedStringSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed string task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"failedBoolValue = xl6_future_await_i32_release(xl6_task_spawn_values(throwingBoolJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn temporary future await.
    exit /b 1
)
findstr /C:"xfuture* repeatedFailedBoolSpawn = xl6_task_spawn_values(throwingBoolJob, xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedBool = (*(int*)xl6_future_await_value(repeatedFailedBoolSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedBoolSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed bool task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedBool = (*(int*)xl6_future_await_value(repeatedFailedBoolSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed bool task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"failedFloatValue = xl6_future_await_f64_release(xl6_task_spawn_values(throwingFloatJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn temporary future await.
    exit /b 1
)
findstr /C:"xfuture* repeatedFailedFloatSpawn = xl6_task_spawn_values(throwingFloatJob, xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedFloat = (*(double*)xl6_future_await_value(repeatedFailedFloatSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedFloatSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed float task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedFloat = (*(double*)xl6_future_await_value(repeatedFailedFloatSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed float task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* namedGreetFuture = xl6_task_spawn_values(greetJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named string task.spawn future initialization.
    exit /b 1
)
findstr /C:"namedGreetA = xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn string first await copy.
    exit /b 1
)
findstr /C:"namedGreetB = xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn string repeated await copy.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedGreetFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed string task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedGreet = xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed string task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* pendingStringSpawn = xl6_task_spawn_values(slowGreetJob, xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingStringSpawnValueA = xl6_future_await_cstr_copy(pendingStringSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingStringSpawnValueB = xl6_future_await_cstr_copy(pendingStringSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingStringSpawnStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingStringAllGroup = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingStringAllLeft, 0, pendingStringAllRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending string task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringAllGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringAllGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending string task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingStringAllLeftValueA = xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingStringAllLeftValueB = xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingStringAllLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingStringRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingStringRaceLeft, 0, pendingStringRaceRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending string task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringRaceGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringRaceGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending string task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftValueA = xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftValueB = xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'flippedValue\s*=\s*xl6_future_await_i32_release\(xl6_task_spawn_values\(flipJob,\s*xrtTypeBool\(\),\s*sizeof\(int\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateBool\(1\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool temporary future bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedFlipFuture\s*=\s*xl6_task_spawn_values\(flipJob,\s*xrtTypeBool\(\),\s*sizeof\(int\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateBool\(0\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool named future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedFlipA\s*=\s*\(\*\(int\*\)xl6_future_await_value\(namedFlipFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(int\)\)' -or $text -notmatch 'namedFlipB\s*=\s*\(\*\(int\*\)xl6_future_await_value\(namedFlipFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(int\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool repeated named future await copies.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedFlipFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed bool task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedFlip = (*(int*)xl6_future_await_value(namedFlipFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed bool task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* pendingBoolSpawn = xl6_task_spawn_values(slowFlipJob, xrtTypeBool(), sizeof(int)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolSpawnValueA = (*(int*)xl6_future_await_value(pendingBoolSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingBoolSpawnValueB = (*(int*)xl6_future_await_value(pendingBoolSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingBoolSpawnStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingBoolAllGroup = xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingBoolAllLeft, 0, pendingBoolAllRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending bool task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolAllGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolAllGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending bool task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftValueA = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftValueB = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingBoolRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeft, 0, pendingBoolRaceRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending bool task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolRaceGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolRaceGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending bool task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftValueA = (*(int*)xl6_future_await_value(pendingBoolRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftValueB = (*(int*)xl6_future_await_value(pendingBoolRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'halfValue\s*=\s*xl6_future_await_f64_release\(xl6_task_spawn_values\(halfJob,\s*xrtTypeFloat\(\),\s*sizeof\(double\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateFloat\(\(double\)\(1\.5\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float temporary future bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedHalfFuture\s*=\s*xl6_task_spawn_values\(halfJob,\s*xrtTypeFloat\(\),\s*sizeof\(double\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateFloat\(\(double\)\(2\.5\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float named future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedHalfA\s*=\s*\(\*\(double\*\)xl6_future_await_value\(namedHalfFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(double\)\)' -or $text -notmatch 'namedHalfB\s*=\s*\(\*\(double\*\)xl6_future_await_value\(namedHalfFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(double\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float repeated named future await copies.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedHalfFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed float task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedHalf = (*(double*)xl6_future_await_value(namedHalfFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed float task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"xfuture* pendingFloatSpawn = xl6_task_spawn_values(slowHalfJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatSpawnValueA = (*(double*)xl6_future_await_value(pendingFloatSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingFloatSpawnValueB = (*(double*)xl6_future_await_value(pendingFloatSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingFloatSpawnStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingFloatAllGroup = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingFloatAllLeft, 0, pendingFloatAllRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending float task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatAllGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatAllGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending float task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftValueA = (*(double*)xl6_future_await_value(pendingFloatAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftValueB = (*(double*)xl6_future_await_value(pendingFloatAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingFloatRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeft, 0, pendingFloatRaceRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending float task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatRaceGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatRaceGroupState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending float task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftValueA = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftValueB = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(10LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn variadic non-empty argv bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicEmptyValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(10LL\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn empty array vararg argv bridge.
    exit /b 1
)
findstr /C:"XL6_Function variadicDefaultJob = xl6_fnvalue_totalDefault_arity0_vararg_make();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing zero-fixed variadic optional adapter task.spawn case.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultEmptyValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*0\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn zero-fixed vararg adapter empty argv bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultExtraValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn zero-fixed vararg adapter extra argv bridge.
    exit /b 1
)
findstr /C:"xfuture* variadicFuture = xl6_task_spawn_values(variadicJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicFuture\s*=\s*xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(10LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"variadicFutureA = (*(long long*)xl6_future_await_value(variadicFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn first await.
    exit /b 1
)
findstr /C:"variadicFutureB = (*(long long*)xl6_future_await_value(variadicFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.spawn cancel guard.
    exit /b 1
)
findstr /C:"xfuture* variadicDefaultFuture = xl6_task_spawn_values(variadicDefaultJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultFuture\s*=\s*xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"variadicDefaultFutureA = (*(long long*)xl6_future_await_value(variadicDefaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn first await.
    exit /b 1
)
findstr /C:"variadicDefaultFutureB = (*(long long*)xl6_future_await_value(variadicDefaultFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicDefaultFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed zero-fixed variadic task.spawn cancel guard.
    exit /b 1
)
findstr /C:"XL6_Function slowVariadicJob = xl6_fnvalue_slowSumVariadic_make();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn function value.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingVariadicSpawn\s*=\s*xl6_task_spawn_values\(slowVariadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(300LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingVariadicSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnValueA = (*(long long*)xl6_future_await_value(pendingVariadicSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled pending variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnValueB = (*(long long*)xl6_future_await_value(pendingVariadicSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await for canceled pending variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending variadic task.spawn canceled after worker completes.
    exit /b 1
)
findstr /C:"XL6_Function slowVariadicDefaultJob = xl6_fnvalue_slowTotalDefault_arity0_vararg_make();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn adapter.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingVariadicDefaultSpawn\s*=\s*xl6_task_spawn_values\(slowVariadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingVariadicDefaultSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnValueA = (*(long long*)xl6_future_await_value(pendingVariadicDefaultSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled pending zero-fixed variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnValueB = (*(long long*)xl6_future_await_value(pendingVariadicDefaultSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await for canceled pending zero-fixed variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending zero-fixed variadic task.spawn canceled after worker completes.
    exit /b 1
)
findstr /C:"variadicAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array for variadic task.spawn task.all.
    exit /b 1
)
findstr /C:"xfuture* variadicAllFuture = xl6_task_all_from_array((xtarray)(variadicAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all over variadic task.spawn typed array.
    exit /b 1
)
findstr /C:"variadicAllA = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(variadicAllFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing copied await for variadic task.all future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicAllFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.all cancel guard.
    exit /b 1
)
findstr /C:"variadicRaceJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array for variadic task.spawn task.race.
    exit /b 1
)
findstr /C:"xfuture* variadicRaceFuture = xl6_task_race_from_array((xtarray)(variadicRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race over variadic task.spawn typed array.
    exit /b 1
)
findstr /C:"variadicRaceA = (*(long long*)xl6_future_await_value(variadicRaceFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing await for variadic task.race future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicRaceFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.race cancel guard.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicSpawn = xl6_task_cancel_value(xl6_task_spawn_values(variadicJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary variadic task.spawn cancel ownership.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary task.all over variadic task.spawn ownership.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary task.race over variadic task.spawn ownership.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedVarArgSpawn\s*=\s*xl6_task_spawn_values\(varArgJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xl6_xrt_retain\(dynamicSpawnArg\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn var argument retain bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'literalVarArgValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(41LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn scalar literal to var argument bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'varStringValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varStringJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xl6_xrt_retain\(dynamicStringSpawnArg\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn string var argument retain bridge.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed task.spawn cancel guard.
    exit /b 1
)
findstr /C:"cancelTemporarySpawn = xl6_task_cancel_value(xl6_task_spawn_values(addJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.spawn future through ownership helper.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(2LL))), 1);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn future after task.cancel.
    exit /b 1
)
findstr /C:"xfuture* pendingSpawn = xl6_task_spawn_values(slowAddJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawn, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnValueA = (*(long long*)xl6_future_await_value(pendingSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingSpawnValueB = (*(long long*)xl6_future_await_value(pendingSpawn" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingSpawnCancelStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingSpawnAllGroup = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeft, 0, pendingSpawnAllRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending task.spawn all sources as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawnAllGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.all over task.spawn group as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeftValueA = (*(long long*)xl6_future_await_value(pendingSpawnAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all task.spawn left source await.
    exit /b 1
)
findstr /C:"pendingSpawnAllRightValueA = (*(long long*)xl6_future_await_value(pendingSpawnAllRight" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all task.spawn right source await.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeftValueB = (*(long long*)xl6_future_await_value(pendingSpawnAllLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.all task.spawn source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingSpawnRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeft, 0, pendingSpawnRaceRight, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending task.spawn race sources as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawnRaceGroup, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.race over task.spawn group as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeftValueA = (*(long long*)xl6_future_await_value(pendingSpawnRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race task.spawn left source await.
    exit /b 1
)
findstr /C:"pendingSpawnRaceRightValueA = (*(long long*)xl6_future_await_value(pendingSpawnRaceRight" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race task.spawn right source await.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeftValueB = (*(long long*)xl6_future_await_value(pendingSpawnRaceLeft" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.race task.spawn source canceled after worker completes.
    exit /b 1
)
findstr /C:"cancelTemporarySpawnAll = xl6_task_cancel_value(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.all over task.spawn futures through ownership helper.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(4LL))), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn inputs inside task.cancel task.all.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(5LL))), 1), 1);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporarySpawnRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.race over task.spawn futures through ownership helper.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(7LL))), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn inputs inside task.cancel task.race.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(8LL))), 1), 1);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.race future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryDefaultSpawn = xl6_task_cancel_value(xl6_task_spawn_values(defaultJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary optional adapter task.spawn cancel ownership.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTemporaryDefaultSpawn\s*=\s*xl6_task_cancel_value\(xl6_task_spawn_values\(defaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(42LL\)\)\),\s*1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary optional adapter task.spawn future after task.cancel.
    exit /b 1
)
findstr /C:"spawnAllValues = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass temporary task.spawn race futures as owned.
    exit /b 1
)
findstr /C:"spawnStringAllValues = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnStringRaceValue = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolAllValues = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(flipJob, xrtTypeBool(), sizeof(int)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnBoolRaceValue = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatAllValues = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(halfJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnFloatRaceValue = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"failedStringSpawnAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingStringJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedStringSpawnRace = xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"failedBoolSpawnAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingBoolJob, xrtTypeBool(), sizeof(int)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedBoolSpawnRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"failedFloatSpawnAll = xl6_future_await_handle_release(xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingFloatJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedFloatSpawnRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(spawnStringAllJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringTypedRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(spawnStringRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(spawnBoolAllJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolTypedRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(spawnBoolRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(spawnFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatTypedRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(spawnFloatRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(180LL)), xvoCreateInt((long long)(1LL))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all task.spawn source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedNullFirstSpawnAllJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnAllJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first int task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first int task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllTail = (*(long long*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnAllJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first int task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllTail == 181LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first int task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(190LL)), xvoCreateInt((long long)(1LL))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race task.spawn source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRace = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(typedNullFirstSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnRaceJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first int task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first int task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceTail = (*(long long*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnRaceJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first int task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceTail == 191LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first int task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.spawn source.
    exit /b 1
)
findstr /C:"typed-null-first-all" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all payload.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedNullFirstSpawnStringAllJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnStringAllJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first string task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first string task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllTail = xl6_future_await_cstr_copy((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnStringAllJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first string task.all source.
    exit /b 1
)
findstr /C:"typed array string task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first string task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typed-null-first-race" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race payload.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(typedNullFirstSpawnStringRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnStringRaceJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first string task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first string task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceTail = xl6_future_await_cstr_copy((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnStringRaceJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first string task.race source.
    exit /b 1
)
findstr /C:"typed array string task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first string task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedNullFirstSpawnBoolAllJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnBoolAllJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first bool task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first bool task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllTail = (*(int*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnBoolAllJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first bool task.all source.
    exit /b 1
)
findstr /C:"typed array bool task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first bool task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(typedNullFirstSpawnBoolRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnBoolRaceJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first bool task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first bool task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceTail = (*(int*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnBoolRaceJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first bool task.race source.
    exit /b 1
)
findstr /C:"typed array bool task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first bool task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(typedNullFirstSpawnFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnFloatAllJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first float task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first float task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllTail = (*(double*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnFloatAllJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first float task.all source.
    exit /b 1
)
findstr /C:"typed array float task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first float task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_LOCAL);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(typedNullFirstSpawnFloatRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedNullFirstSpawnFloatRaceJobs);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first float task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceState == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first float task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceTail = (*(double*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnFloatRaceJobs, (unsigned int)(1LL))))" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first float task.race source.
    exit /b 1
)
findstr /C:"typed array float task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first float task.race source.
    exit /b 1
)
findstr /C:"spawnIntAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnIntAllCompletedJobs), xrtTypeInt()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed int task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnIntAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(spawnIntAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed int task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnIntAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed int task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnIntAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(spawnIntAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed int task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnIntRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnIntRaceCompletedJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed int task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnIntRaceCompletedB = (*(long long*)xl6_future_await_value(spawnIntRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed int task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnIntRaceCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed int task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnIntRaceCompleted = (*(long long*)xl6_future_await_value(spawnIntRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed int task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnStringAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnStringAllCompletedJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(spawnStringAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnStringAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed string task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnStringAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(spawnStringAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed string task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnStringRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnStringRaceCompletedJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringRaceCompletedB = xl6_future_await_cstr_copy(spawnStringRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnStringRaceCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed string task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnStringRaceCompleted = xl6_future_await_cstr_copy(spawnStringRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed string task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnBoolAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnBoolAllCompletedJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(spawnBoolAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnBoolAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed bool task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnBoolAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(spawnBoolAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed bool task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnBoolRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnBoolRaceCompletedJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolRaceCompletedB = (*(int*)xl6_future_await_value(spawnBoolRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnBoolRaceCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed bool task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnBoolRaceCompleted = (*(int*)xl6_future_await_value(spawnBoolRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed bool task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnFloatAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnFloatAllCompletedJobs), xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatAllCompletedB = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(spawnFloatAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnFloatAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed float task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnFloatAllCompleted = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(spawnFloatAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed float task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnFloatRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnFloatRaceCompletedJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatRaceCompletedB = (*(double*)xl6_future_await_value(spawnFloatRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnFloatRaceCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed float task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnFloatRaceCompleted = (*(double*)xl6_future_await_value(spawnFloatRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed float task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"failedStringSpawnTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedStringSpawnAllJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"failedStringSpawnTypedRace = xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(failedStringSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringAllFailureGroup = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnStringAllFailureSlowSource, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow string task.spawn source as borrowed to task.all fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnStringAllFailureSlowValue = xl6_future_await_cstr_copy(spawnStringAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep slow string task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"typedSpawnStringAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnStringAllFailureJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"typedSpawnStringAllFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(typedSpawnStringAllFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnStringAllFailureSlowValue = xl6_future_await_cstr_copy(typedSpawnStringAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array slow string task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(typedSpawnStringAllFailureSlowSource, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array task.all slow string task.spawn source completed after group failure.
    exit /b 1
)
findstr /C:"spawnStringRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"spawnStringRaceFailureSlowLoser, 0" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass slow string task.spawn loser as borrowed to task.race fast-failure fixture.
    exit /b 1
)
findstr /C:"spawnStringRaceFailureLoserValueB = xl6_future_await_cstr_copy(spawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled string task.spawn race loser.
    exit /b 1
)
findstr /C:"spawnStringRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled string task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureGroup = xl6_task_race_from_array((xtarray)(typedSpawnStringRaceFailureJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureValue = xl6_future_await_cstr_copy(typedSpawnStringRaceFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserValueA = xl6_future_await_cstr_copy(typedSpawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array string task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserValueB = xl6_future_await_cstr_copy(typedSpawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array string task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array string task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"failedBoolSpawnTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedBoolSpawnAllJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"failedBoolSpawnTypedRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(failedBoolSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"failedFloatSpawnTypedAll = xl6_future_await_handle_release(xl6_task_all_from_array((xtarray)(failedFloatSpawnAllJobs), xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"failedFloatSpawnTypedRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(failedFloatSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array float task.race over task.spawn futures.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnBoolAllFailureJobs), xrtTypeBool()')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.all fast-failure over task.spawn fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolAllFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(typedSpawnBoolAllFailureGroup')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.all fast-failure group await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolAllFailureSlowValue = (*(int*)xl6_future_await_value(typedSpawnBoolAllFailureSlowSource')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing slow-source await after typed-array bool task.all fast failure.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('!xl6_task_cancel_value(typedSpawnBoolAllFailureSlowSource, 0)')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed bool task.all slow source non-cancelable.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolRaceFailureGroup = xl6_task_race_from_array((xtarray)(typedSpawnBoolRaceFailureJobs)')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.race fast-failure over task.spawn fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolRaceFailureValue = (*(int*)xl6_future_await_value(typedSpawnBoolRaceFailureGroup')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.race fast-failure group await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolRaceFailureLoserValueA = (*(int*)xl6_future_await_value(typedSpawnBoolRaceFailureSlowLoser')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array bool task.spawn race loser.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolRaceFailureLoserValueB = (*(int*)xl6_future_await_value(typedSpawnBoolRaceFailureSlowLoser')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array bool task.spawn race loser.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolRaceFailureLoserStateB == 42LL')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array bool task.spawn race loser after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnFloatAllFailureJobs), xrtTypeFloat()')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.all fast-failure over task.spawn fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatAllFailureValues = xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(typedSpawnFloatAllFailureGroup')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.all fast-failure group await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatAllFailureSlowValue = (*(double*)xl6_future_await_value(typedSpawnFloatAllFailureSlowSource')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing slow-source await after typed-array float task.all fast failure.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('!xl6_task_cancel_value(typedSpawnFloatAllFailureSlowSource, 0)')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed float task.all slow source non-cancelable.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatRaceFailureGroup = xl6_task_race_from_array((xtarray)(typedSpawnFloatRaceFailureJobs)')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.race fast-failure over task.spawn fixture.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatRaceFailureValue = (*(double*)xl6_future_await_value(typedSpawnFloatRaceFailureGroup')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.race fast-failure group await.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatRaceFailureLoserValueA = (*(double*)xl6_future_await_value(typedSpawnFloatRaceFailureSlowLoser')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array float task.spawn race loser.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatRaceFailureLoserValueB = (*(double*)xl6_future_await_value(typedSpawnFloatRaceFailureSlowLoser')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array float task.spawn race loser.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatRaceFailureLoserStateB == 42LL')){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array float task.spawn race loser after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnStringTypedAll\s*=\s*xl6_task_cancel_value\(xl6_task_all_from_array\(\(xtarray\)\(cancelSpawnStringAllJobs\),\s*xrtTypeString\(\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array string task.all over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnStringTypedRace\s*=\s*xl6_task_cancel_value\(xl6_task_race_from_array\(\(xtarray\)\(cancelSpawnStringRaceJobs\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array string task.race over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnBoolTypedAll\s*=\s*xl6_task_cancel_value\(xl6_task_all_from_array\(\(xtarray\)\(cancelSpawnBoolAllJobs\),\s*xrtTypeBool\(\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array bool task.all over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnBoolTypedRace\s*=\s*xl6_task_cancel_value\(xl6_task_race_from_array\(\(xtarray\)\(cancelSpawnBoolRaceJobs\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array bool task.race over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnFloatTypedAll\s*=\s*xl6_task_cancel_value\(xl6_task_all_from_array\(\(xtarray\)\(cancelSpawnFloatAllJobs\),\s*xrtTypeFloat\(\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array float task.all over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelSpawnFloatTypedRace\s*=\s*xl6_task_cancel_value\(xl6_task_race_from_array\(\(xtarray\)\(cancelSpawnFloatRaceJobs\),\s*[^;]+?\),\s*1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array float task.race over task.spawn futures as owned.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalVarArgValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered conditional var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'binaryVarArgValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered binary var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'nullCoalesceVarArgValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered null-coalesce var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'nestedCallArgValue\s*=\s*scoreSpawnResult\(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered positional call-argument task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'nestedNamedCallArgValue\s*=\s*combineSpawnResult\(1LL,\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered named call-argument task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionDynamicSpawnCount[\s\S]*?__xl6_task_arg_[\s\S]*?if \(\(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered if-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'whileDynamicSpawnCount[\s\S]*?while \(1\)[\s\S]*?__xl6_task_arg_[\s\S]*?if \(!\(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered while-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'doDynamicSpawnCount[\s\S]*?while \(1\)[\s\S]*?__xl6_task_arg_[\s\S]*?if \(!\(\(doDynamicSpawnCount < 2LL\) && \(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered do-while-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicExprTailValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered array-vararg task.spawn tail argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'spawnCancelReturnDynamic[\s\S]*?__xl6_task_arg_[\s\S]*?xl6_task_cancel_value\(xl6_task_spawn_values\(job,'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered return-statement task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'assignedDynamicSpawn\s*=\s*xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered assignment task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('multiDeclDynamicSpawnLeft = __xl6_multi_'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-2500); $slice=$text.Substring($start,$idx-$start); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'pairSpawnResult'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered multi-declaration task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('multiAssignDynamicSpawnLeft = __xl6_multi_'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-2500); $slice=$text.Substring($start,$idx-$start); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'pairSpawnResult'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered multi-assignment task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_task_cancel_value\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered expression-statement task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(221LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_[\s\S]*?task\.spawn dynamic var argument should lower inside assert conditions'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered assert-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(401LL\)[\s\S]*?xl6_error_raise\(\x22AssertError\x22,[\s\S]*?xl6_task_spawn_values\(varMessageJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered assert-message task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(301LL\)[\s\S]*?xl6_error_raise\(\x22Error\x22,[\s\S]*?xl6_task_spawn_values\(varMessageJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered throw-expression task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_gen_worker_dynamicSpawnStream[\s\S]*?__xl6_task_arg_[\s\S]*?xl6_task_spawn_values\(job,[\s\S]*?__xl6_task_arg_[\s\S]*?xl6_generator_yield_copy'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered yield-expression task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('rangeDynamicSpawnTotal'); if($idx -lt 0){exit 1}; $rangeIdx=$text.IndexOf('long long __xl6_range_start_', $idx); if($rangeIdx -lt 0){exit 1}; $start=[Math]::Max($idx, $rangeIdx-4000); $slice=$text.Substring($start,[Math]::Min(40000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_[\s\S]*?makeSpawnValue\(0LL\)[\s\S]*?long long __xl6_range_start_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'makeSpawnValue\(2LL\)[\s\S]*?makeSpawnValue\(1LL\)[\s\S]*?long long __xl6_range_stop_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_retain\(emptySpawnArg\)[\s\S]*?makeSpawnValue\(0LL\)[\s\S]*?long long __xl6_range_step_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'for \(long long __xl6_range_value_\d+ = __xl6_range_start_\d+;[\s\S]*?rangeDynamicSpawnTotal = \(rangeDynamicSpawnTotal \+ i\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered range-boundary task.spawn arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('indexDynamicSpawnRead'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered index-read task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('safeIndexDynamicSpawnRead'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_box_or_null\(xrtTypeInt\(\), xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_value_to_i64_release'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered safe-index-read task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('safeIndexDynamicSpawnFallback'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(7000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_box_or_null\(xrtTypeInt\(\), xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xvoIsNull\(__xl6_nc_' -or $slice -notmatch 'xvoCreateInt\(\(long long\)\(88LL\)\)' -or $slice -notmatch 'xl6_xrt_value_to_i64_release\(__xl6_call_value_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered safe-index fallback task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('iterableDynamicSpawnArrayTotal'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(120000,$text.Length-$idx)); $checks=@('__xl6_task_arg_','makeSpawnValue(2LL)','makeDynamicSpawnArray(xl6_future_await_i64_release(xl6_task_spawn_values(varArgJob','xtarray __xl6_typed_iter_'); foreach($p in $checks){ if($slice.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-container iterable task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('iterableDynamicSpawnGeneratorTotal'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(120000,$text.Length-$idx)); $checks=@('__xl6_task_arg_','xl6_xrt_basic_binary','makeDynamicSpawnGenerator(xl6_future_await_i64_release(xl6_task_spawn_values(varArgJob','XL6_Generator* __xl6_gen_iter_'); foreach($p in $checks){ if($slice.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered generator iterable task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl6_xrt_typed_array_set_at(indexDynamicSpawnValues'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-2000); $slice=$text.Substring($start,[Math]::Min(6000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xvoCreateInt\(\(long long\)\(77LL\)\)' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch '__xl6_index_assign_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered index-write-target task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedDynamicSpawnAllJobs[\s\S]*?__xl6_task_arg_[\s\S]*?xrtTypedArrayAppend\(typedDynamicSpawnAllJobs'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-array task.all task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedDynamicSpawnRaceJobs[\s\S]*?__xl6_task_arg_[\s\S]*?xrtTypedArrayAppend\(typedDynamicSpawnRaceJobs'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-array task.race task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_async_worker_makeTypedDynamicSpawnAllValues'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xrtTypedArrayAppendValue\(__xl6_async_return_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered async typed-array return task.spawn arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_async_worker_makeTypedDynamicSpawnAwaitValues'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(5000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xrtTypedArrayAppendValue\(__xl6_async_return_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered async typed-array return awaited task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'mixedTailBorrowedFuture\s*=\s*xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : makeAdder\(520LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'mixedTailOwnedFuture\s*=\s*xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : makeAdder\(530LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'mixedTailAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : makeAdder\(540LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(0 \? xl6_function_copy_value\(mixedTailClosureSource\) : makeAdder\(550LL\)\)[\s\S]*?, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing mixed conditional future/all callable normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureFuture\s*=\s*xl6_task_spawn_values\(pendingClosureSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureFuture, 0\)' -or $text -notmatch 'pendingClosureStateA = 42LL' -or $text -notmatch 'pendingClosureStateB = 42LL' -or $text -notmatch 'pendingClosureSource\.signature[\s\S]*?== 600LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.spawn cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureAllLeft\s*=\s*xl6_task_spawn_values\(pendingClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'pendingClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?pendingClosureAllLeft, 0, xl6_task_spawn_values\(makeSleepyAdder\(620LL\), xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureAllGroup, 0\)' -or $text -notmatch 'pendingClosureAllGroupState = 42LL' -or $text -notmatch 'pendingClosureAllLeftStateA = 42LL' -or $text -notmatch 'pendingClosureAllLeftStateB = 42LL' -or $text -notmatch 'pendingClosureAllSource\.signature[\s\S]*?== 610LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.all cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'failureClosureAllSlow\s*=\s*xl6_task_spawn_values\(failureClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'failureClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?xl6_task_spawn_values\(throwingJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 0\), 1, failureClosureAllSlow, 0' -or $text -notmatch 'failureClosureAllGroupState = 42LL' -or $text -notmatch 'failureClosureAllGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(failureClosureAllGroup, 0\)' -or $text -notmatch 'failureClosureAllGroupStateAfterCancel = 42LL' -or $text -notmatch 'failureClosureAllSlowValue == 890LL' -or $text -notmatch 'failureClosureAllSource\.signature[\s\S]*?== 670LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedFailureClosureAllSlow\s*=\s*xl6_task_spawn_values\(typedFailureClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'typedFailureClosureAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'typedFailureClosureAllGroup\s*=\s*xl6_task_all_from_array\(\(xtarray\)\(typedFailureClosureAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'typedFailureClosureAllGroupState = 42LL' -or $text -notmatch 'typedFailureClosureAllGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(typedFailureClosureAllGroup, 0\)' -or $text -notmatch 'typedFailureClosureAllGroupStateAfterCancel = 42LL' -or $text -notmatch 'typedFailureClosureAllSlowValue == 900LL' -or $text -notmatch 'typedFailureClosureAllSource\.signature[\s\S]*?== 680LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'successClosureAllSlow\s*=\s*xl6_task_spawn_values\(successClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'successClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?\), 1, successClosureAllSlow, 0' -or $text -notmatch 'successClosureAllValuesAgain\s*=\s*xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(successClosureAllGroup' -or $text -notmatch '!xl6_task_cancel_value\(successClosureAllGroup, 0\)' -or $text -notmatch 'successClosureAllValuesAfterCancel\s*=\s*xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(successClosureAllGroup' -or $text -notmatch 'successClosureAllSlowAgain == 915LL' -or $text -notmatch 'successClosureAllSource\.signature[\s\S]*?== 695LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedSuccessClosureAllSlow\s*=\s*xl6_task_spawn_values\(typedSuccessClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'typedSuccessClosureAllJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'typedSuccessClosureAllGroup\s*=\s*xl6_task_all_from_array\(\(xtarray\)\(typedSuccessClosureAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'typedSuccessClosureAllValuesAgain\s*=\s*xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(typedSuccessClosureAllGroup' -or $text -notmatch '!xl6_task_cancel_value\(typedSuccessClosureAllGroup, 0\)' -or $text -notmatch 'typedSuccessClosureAllValuesAfterCancel\s*=\s*xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(typedSuccessClosureAllGroup' -or $text -notmatch 'typedSuccessClosureAllSlowAgain == 920LL' -or $text -notmatch 'typedSuccessClosureAllSource\.signature[\s\S]*?== 700LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful typed-array task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureRaceLeft\s*=\s*xl6_task_spawn_values\(pendingClosureRaceSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'pendingClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?pendingClosureRaceLeft, 0, xl6_task_spawn_values\(makeSleepyAdder\(640LL\), xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureRaceGroup, 0\)' -or $text -notmatch 'pendingClosureRaceGroupState = 42LL' -or $text -notmatch 'pendingClosureRaceLeftStateA = 42LL' -or $text -notmatch 'pendingClosureRaceLeftStateB = 42LL' -or $text -notmatch 'pendingClosureRaceSource\.signature[\s\S]*?== 630LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.race cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'successClosureRaceLoser\s*=\s*xl6_task_spawn_values\(successClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'successClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 2,[\s\S]*?\), 1, successClosureRaceLoser, 0' -or $text -notmatch 'successClosureRaceValue = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAgain = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAgain == 651LL' -or $text -notmatch '!xl6_task_cancel_value\(successClosureRaceGroup, 0\)' -or $text -notmatch 'successClosureRaceValueAfterCancel = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAfterCancel == 651LL' -or $text -notmatch 'successClosureRaceLoserStateA = 42LL' -or $text -notmatch 'successClosureRaceLoserStateB = 42LL' -or $text -notmatch 'successClosureRaceLoserSource\.signature[\s\S]*?== 650LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedSuccessClosureRaceLoser\s*=\s*xl6_task_spawn_values\(typedSuccessClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'typedSuccessClosureRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'typedSuccessClosureRaceGroup\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedSuccessClosureRaceJobs\)' -or $text -notmatch 'typedSuccessClosureRaceValue == 711LL' -or $text -notmatch 'typedSuccessClosureRaceValueAgain = \(\*\(long long\*\)xl6_future_await_value\(typedSuccessClosureRaceGroup' -or $text -notmatch 'typedSuccessClosureRaceValueAgain == 711LL' -or $text -notmatch '!xl6_task_cancel_value\(typedSuccessClosureRaceGroup, 0\)' -or $text -notmatch 'typedSuccessClosureRaceValueAfterCancel = \(\*\(long long\*\)xl6_future_await_value\(typedSuccessClosureRaceGroup' -or $text -notmatch 'typedSuccessClosureRaceValueAfterCancel == 711LL' -or $text -notmatch 'typedSuccessClosureRaceLoserStateA = 42LL' -or $text -notmatch 'typedSuccessClosureRaceLoserStateB = 42LL' -or $text -notmatch 'typedSuccessClosureRaceLoserSource\.signature[\s\S]*?== 710LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful typed-array task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'failureClosureRaceLoser\s*=\s*xl6_task_spawn_values\(failureClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'failureClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?xl6_task_spawn_values\(throwingJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 0\), 1, failureClosureRaceLoser, 0' -or $text -notmatch 'failureClosureRaceGroupState = 42LL' -or $text -notmatch 'failureClosureRaceGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(failureClosureRaceGroup, 0\)' -or $text -notmatch 'failureClosureRaceGroupStateAfterCancel = 42LL' -or $text -notmatch 'failureClosureRaceLoserStateA = 42LL' -or $text -notmatch 'failureClosureRaceLoserStateB = 42LL' -or $text -notmatch 'failureClosureRaceLoserSource\.signature[\s\S]*?== 660LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedFailureClosureRaceLoser\s*=\s*xl6_task_spawn_values\(typedFailureClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'typedFailureClosureRaceJobs\s*=\s*xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_LOCAL\)' -or $text -notmatch 'typedFailureClosureRaceGroup\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedFailureClosureRaceJobs\)' -or $text -notmatch 'typedFailureClosureRaceGroupState = 42LL' -or $text -notmatch 'typedFailureClosureRaceGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(typedFailureClosureRaceGroup, 0\)' -or $text -notmatch 'typedFailureClosureRaceGroupStateAfterCancel = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserStateA = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserStateB = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserSource\.signature[\s\S]*?== 690LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_task_fn_" build\test\cgen_task_spawn.c >nul
if not errorlevel 1 (
    echo cgen_task_spawn.c should not emit task.spawn statement-expression temporaries.
    exit /b 1
)
findstr /C:"XL6_Error* error = xl6_error_take();" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn active error transfer.
    exit /b 1
)
findstr /C:"xl6_async_result_error(pOut, message);" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn future error propagation.
    exit /b 1
)
findstr /C:"static xl6_win_tls_dword xl6__active_error_tls_index = XL6_WIN_TLS_OUT_OF_INDEXES;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should allocate Windows TLS for active error state.
    exit /b 1
)
findstr /C:"return xl6_error_active_get() != 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should read active error state through TLS accessor.
    exit /b 1
)

release\xl.exe build test\cases\cgen_task_spawn.xl -o build\test\cgen_task_spawn.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$exe=(Resolve-Path 'build\test\cgen_task_spawn.exe').Path; for($i=1;$i -le 4;$i++){ $psi=[System.Diagnostics.ProcessStartInfo]::new(); $psi.FileName=$exe; $psi.WorkingDirectory=(Get-Location).Path; $psi.UseShellExecute=$false; $p=[System.Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(120000)){ try{$p.Kill()}catch{}; Write-Host ('cgen_task_spawn.exe run '+$i+' timed out.'); exit 124 }; $code=$p.ExitCode; $p.Dispose(); if($code -ne 0){ Write-Host ('cgen_task_spawn.exe run '+$i+' failed.'); exit $code } }"
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: task API and task spawn
exit /b 0

