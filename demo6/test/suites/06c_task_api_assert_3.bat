@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API cgen assertions 3/4...

if not "%XL6_TASK_API_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_api.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
    if errorlevel 1 exit /b 1
)

findstr /C:"xl_fn_flipAsync(1), 1, xl_fn_flipAsync(0), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary bool task.all/race source futures after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryBoolRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary bool task.race future through ownership helper.
    exit /b 1
)
findstr /C:"cancelTypedBoolAll = xl6_task_cancel_value(xl6_task_all_from_array((xtarray)(cancelBoolAllJobs), xrtTypeBool32()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl_fn_addHalfAsync(9.5), 1, xl_fn_addHalfAsync(10.5), 1), 1)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release temporary float task.all source futures after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryFloatRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel temporary float task.race future through ownership helper.
    exit /b 1
)
findstr /C:"xl_fn_addHalfAsync(11.5), 1, xl_fn_addHalfAsync(12.5), 1), 1)" build\test\cgen_task_api.c >nul
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xfuture\* (__xl6_typed_future_\d+) = xl_fn_addAsync\(11LL, 1LL\);\s*if \(!xrtTypedArrayAppend\([^,]+, &\1\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);' -or $text -notmatch 'xfuture\* (__xl6_typed_future_\d+) = xl_fn_addAsync\(40LL, 1LL\);\s*if \(!xrtTypedArrayAppend\([^,]+, &\1\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing direct-call typed future append cleanup.
    exit /b 1
)
findstr /C:"xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing string descriptor for task.all string results.
    exit /b 1
)
findstr /C:"xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing bool descriptor for task.all bool results.
    exit /b 1
)
findstr /C:"xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing float descriptor for task.all float results.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all string aggregation path.
    exit /b 1
)
findstr /C:"stringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race string await path.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all bool aggregation path.
    exit /b 1
)
findstr /C:"namedBoolAllFuture = xl6_task_all_values(xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all future fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool32(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all first await copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool32(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named bool task.all second await copy.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedBoolAllJobs), xrtTypeBool32()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all float aggregation path.
    exit /b 1
)
findstr /C:"namedFloatAllFuture = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all future fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all first await copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named float task.all second await copy.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.all first await copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named task.all second await copy.
    exit /b 1
)
findstr /C:"repeatedFailedAll = xl6_task_all_values(xrtTypeInt()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed task.all future fixture.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed task.all future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedAll = xl6_task_cancel_value(repeatedFailedAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed task.all future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedStringAllFuture = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all future fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all first await copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing named string task.all second await copy.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all typed string array path.
    exit /b 1
)
findstr /C:"cancelNamedAllCompleted = xl6_task_cancel_value(namedAllFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass completed named task.all future cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(namedAllFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed int task.all future.
    exit /b 1
)
findstr /C:"cancelTypedAllCompleted = xl6_task_cancel_value(typedAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed int task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeInt(), (*(xtarray*)xl6_future_await_value(typedAllCompletedFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(namedStringAllFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"postCancelNamedStringRaceValue = xrtCopyStr(xl6_future_await_cstr_copy(namedStringRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep completed named string task.race future awaitable after cancel.
    exit /b 1
)
findstr /C:"typedStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(stringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race typed string array path.
    exit /b 1
)
findstr /C:"typedStringAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedStringAllCompletedJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed string task.all future path.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(typedStringAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed string task.all future.
    exit /b 1
)
findstr /C:"cancelTypedStringAllCompleted = xl6_task_cancel_value(typedStringAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed string task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(typedStringAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed string task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"typedStringRaceCompletedFuture = xl6_task_race_from_array((xtarray)(typedStringRaceCompletedJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed string task.race future path.
    exit /b 1
)
findstr /C:"typedStringRaceCompletedB = xrtCopyStr(xl6_future_await_cstr_copy(typedStringRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed string task.race future.
    exit /b 1
)
findstr /C:"cancelTypedStringRaceCompleted = xl6_task_cancel_value(typedStringRaceCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed string task.race future as borrowed.
    exit /b 1
)
findstr /C:"postCancelTypedStringRaceCompleted = xrtCopyStr(xl6_future_await_cstr_copy(typedStringRaceCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep typed-array completed string task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"typedBoolAllCompletedFuture = xl6_task_all_from_array((xtarray)(typedBoolAllCompletedJobs), xrtTypeBool32()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array completed bool task.all future path.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool32(), (*(xtarray*)xl6_future_await_value(typedBoolAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed bool task.all future.
    exit /b 1
)
findstr /C:"cancelTypedBoolAllCompleted = xl6_task_cancel_value(typedBoolAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed bool task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool32(), (*(xtarray*)xl6_future_await_value(typedBoolAllCompletedFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(typedFloatAllCompletedFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for typed-array completed float task.all future.
    exit /b 1
)
findstr /C:"cancelTypedFloatAllCompleted = xl6_task_cancel_value(typedFloatAllCompletedFuture, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should cancel typed-array completed float task.all future as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(typedFloatAllCompletedFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool32(), (*(xtarray*)xl6_future_await_value(namedBoolAllFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(namedFloatAllFuture" build\test\cgen_task_api.c >nul
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
findstr /C:"xl_fn_failAsync(), 1, allFailureSlowSource, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass task.all failed temporary as owned and slow source as borrowed.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(allFailureGroup" build\test\cgen_task_api.c >nul
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
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(typedAllFailureGroup" build\test\cgen_task_api.c >nul
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
findstr /C:"xl_fn_failAsync(), 1, raceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_values(xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class task.all await.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedTaskBoxAllJobs), xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.all await.
    exit /b 1
)
findstr /C:"taskBoxRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class task.race group.
    exit /b 1
)
findstr /C:"xl_fn_failTaskBoxAsync(), 1, taskBoxRaceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed class race temporary as owned and loser as borrowed.
    exit /b 1
)
findstr /C:"taskBoxRaceFailureLoserValueB = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(taskBoxRaceFailureSlowLoser" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed class race loser repeated await.
    exit /b 1
)
findstr /C:"expect_i64(taskBoxRaceFailureLoserStateB, 42LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should preserve failed class race loser cancel state after worker exit.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed function task.all await.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedTaskFuncAllJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array function task.all await.
    exit /b 1
)
findstr /C:"taskFuncRaceFailureGroup = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed function task.race group.
    exit /b 1
)
findstr /C:"xl_fn_failTaskAdderAsync(), 1, taskFuncRaceFailureSlowLoser, 0" build\test\cgen_task_api.c >nul
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
findstr /C:"repeatedFailedTaskBoxAll = xl6_task_all_values(xl6_runtime_type_desc_" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed class task.all future.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedTaskBoxAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed class task.all future.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedTaskBoxAll = xl6_task_cancel_value(repeatedFailedTaskBoxAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed class task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedTaskBoxAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed class task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"repeatedFailedTaskBoxRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed class task.race future.
    exit /b 1
)
findstr /C:"secondFailedTaskBoxRace = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(repeatedFailedTaskBoxRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated await for failed class task.race future.
    exit /b 1
)
findstr /C:"failedTypedTaskBoxRaceFuture = xl6_task_race_from_array((xtarray)(failedTaskBoxRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.race future.
    exit /b 1
)
findstr /C:"failedTypedTaskBoxRace = xl6_xrt_bind_type(xl6_xrt_retain((*(xvalue*)xl6_future_await_value(failedTypedTaskBoxRaceFuture" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array class task.race await.
    exit /b 1
)
findstr /C:"repeatedFailedTaskFuncAll = xl6_task_all_values(xrtTypeFunction()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing repeated failed function task.all future.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedTaskFuncAll" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedStringAll = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all named future fixture.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(repeatedFailedStringAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedStringAll = xl6_task_cancel_value(repeatedFailedStringAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed string task.all cancel as borrowed.
    exit /b 1
)

echo DEMO6 suite ok: task API cgen assertions 3/4
exit /b 0
