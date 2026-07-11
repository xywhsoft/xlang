@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API cgen assertions 4/4...

if not "%XL6_TASK_API_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_api.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
    if errorlevel 1 exit /b 1
)

findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeString(), (*(xtarray*)xl6_future_await_value(repeatedFailedStringAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed string task.all awaitable after cancel.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedStringAllJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array string task.all await.
    exit /b 1
)
findstr /C:"failedStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedStringRace = xl6_task_race_values" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race named future fixture.
    exit /b 1
)
findstr /C:"firstFailedStringRace = xrtCopyStr(xl6_future_await_cstr_copy(repeatedFailedStringRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed string task.race first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedStringRace = xl6_task_cancel_value(repeatedFailedStringRace, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed string task.race cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedStringRace = xrtCopyStr(xl6_future_await_cstr_copy(repeatedFailedStringRace" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should keep failed string task.race awaitable after cancel.
    exit /b 1
)
findstr /C:"failedTypedStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(failedStringRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array string task.race await.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedBoolAllJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array bool task.all await.
    exit /b 1
)
findstr /C:"failedTypedBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(failedBoolRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array bool task.race await.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedFloatAllJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array float task.all await.
    exit /b 1
)
findstr /C:"failedTypedFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(failedFloatRaceJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed typed-array float task.race await.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedBoolAll = xl6_task_all_values(xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all named future fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(repeatedFailedBoolAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed bool task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedBoolAll = xl6_task_cancel_value(repeatedFailedBoolAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed bool task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(repeatedFailedBoolAll" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all temporary release await.
    exit /b 1
)
findstr /C:"repeatedFailedFloatAll = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all named future fixture.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(repeatedFailedFloatAll" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing failed float task.all first await copy.
    exit /b 1
)
findstr /C:"cancelRepeatedFailedFloatAll = xl6_task_cancel_value(repeatedFailedFloatAll, 0)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should pass failed float task.all cancel as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(repeatedFailedFloatAll" build\test\cgen_task_api.c >nul
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
findstr /C:"defaultJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize typed future array as an empty array.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(emptyStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty string typed-array path.
    exit /b 1
)
findstr /C:"emptyStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(emptyStringJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty string typed-array path.
    exit /b 1
)
findstr /C:"defaultStringJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize string future array as empty.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default string typed-array path.
    exit /b 1
)
findstr /C:"defaultStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(defaultStringJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default string typed-array path.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(emptyBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
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
findstr /C:"defaultBoolJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize bool future array as empty.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all default bool typed-array path.
    exit /b 1
)
findstr /C:"defaultBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(defaultBoolJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race default bool typed-array path.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(emptyFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.all empty float typed-array path.
    exit /b 1
)
findstr /C:"emptyFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(emptyFloatJobs)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing task.race empty float typed-array path.
    exit /b 1
)
findstr /C:"defaultFloatJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize float future array as empty.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(emptyTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
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
findstr /C:"defaultTaskBoxJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize class future array as empty.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(emptyTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
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
findstr /C:"defaultTaskFuncJobs = xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should default-initialize function future array as empty.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(defaultTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
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
findstr /C:"missingStringFuture = 0;" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string null future fixture.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeFuture(), XRT_OBJMODE_SHARED)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed future array creation for null source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(nullStringJobs), xrtTypeString()" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed string task.all null source path.
    exit /b 1
)
findstr /C:"nullStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(nullStringJobs)" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(nullBoolJobs), xrtTypeBool()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(nullFloatJobs), xrtTypeFloat()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(nullTaskBoxJobs), xl6_type_desc_XL_Class_TaskBox()" build\test\cgen_task_api.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(nullTaskFuncJobs), xrtTypeFunction()" build\test\cgen_task_api.c >nul
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
findstr /C:"typedMixedAllSource = xl_fn_addAsync(160LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array mixed task.all source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedMixedAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
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
findstr /C:"typedMixedRaceSource = xl_fn_addAsync(170LL, 1LL)" build\test\cgen_task_api.c >nul
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
findstr /C:"nullFirstAllSource = xl_fn_addAsync(180LL, 1LL)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array null-first task.all source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(nullFirstAllJobs), xrtTypeInt()" build\test\cgen_task_api.c >nul
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
findstr /C:"nullFirstRaceSource = xl_fn_addAsync(190LL, 1LL)" build\test\cgen_task_api.c >nul
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
findstr /C:"xl_fn_addAsync(140LL, 1LL), 1, missingFuture, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release owned task.all literal input before null future failure.
    exit /b 1
)
findstr /C:"xl_fn_addAsync(150LL, 1LL), 1, missingFuture, 0" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c should release owned task.race literal input before null future failure.
    exit /b 1
)
findstr /C:"static xfuture* xl6_future_add_ref_if_needed(xfuture* pFuture)" build\test\cgen_task_api.c >nul
if errorlevel 1 (
    echo cgen_task_api.c missing safe future add-ref helper for conditional future arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalCancelBorrowed\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? xl6_future_add_ref_if_needed\(conditionalCancelSource\) : xl_fn_addAsync\(210LL, 1LL\)\), 1\)' -or $text -notmatch 'conditionalCancelOwned\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? xl6_future_add_ref_if_needed\(conditionalCancelSource\) : xl_fn_addAsync\(220LL, 1LL\)\), 1\)' -or $text -notmatch 'conditionalCancelMissing\s*=\s*xl6_task_cancel_value\(\(chooseConditionalFuture \? xl_fn_addAsync\(230LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalMissingFuture\)\), 1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional future task.cancel ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?\(chooseConditionalAll \? xl6_future_add_ref_if_needed\(conditionalAllSource\) : xl_fn_addAsync\(250LL, 1LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalAllSource\) : xl_fn_addAsync\(260LL, 1LL\)\), 1' -or $text -notmatch 'xl6_future_await_i64_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalRace \? xl6_future_add_ref_if_needed\(conditionalRaceSource\) : xl_fn_addAsync\(280LL, 1LL\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional future task.all/race ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalAll \? xl6_future_add_ref_if_needed\(typedConditionalAllSource\) : xl_fn_addAsync\(320LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalAllSource\) : xl_fn_addAsync\(330LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'chooseTypedConditionalRace \? xl6_future_add_ref_if_needed\(typedConditionalRaceSource\) : xl_fn_addAsync\(350LL, 1LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_i64_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(!xrtTypedArrayAppend\([^,]+, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);' -or $text -notmatch 'if \(!xrtTypedArrayAppend\([^,]+, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalBoolAll \? xl6_future_add_ref_if_needed\(typedConditionalBoolAllSource\) : xl_fn_flipAsync\(1\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalBoolAllSource\) : xl_fn_flipAsync\(0\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalBoolAllJobs\), xrtTypeBool\(\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalBoolRace \? xl6_future_add_ref_if_needed\(typedConditionalBoolRaceSource\) : xl_fn_flipAsync\(1\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_i32_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalBoolRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional bool future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalFloatAll \? xl6_future_add_ref_if_needed\(typedConditionalFloatAllSource\) : xl_fn_addHalfAsync\(420\.5\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalFloatAllSource\) : xl_fn_addHalfAsync\(430\.5\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalFloatAllJobs\), xrtTypeFloat\(\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalFloatRace \? xl6_future_add_ref_if_needed\(typedConditionalFloatRaceSource\) : xl_fn_addHalfAsync\(450\.5\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_f64_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalFloatRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional float future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(!xrtTypedArrayAppend\([^,]+, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array scalar future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalStringAll \? xl6_future_add_ref_if_needed\(typedConditionalStringAllSource\) : xl_fn_greetAsync\([^)]*typed-cond-string-all-alt[^)]*\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalStringAllSource\) : xl_fn_greetAsync\([^)]*typed-cond-string-all-owned[^)]*\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalStringAllJobs\), xrtTypeString\(\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalStringRace \? xl6_future_add_ref_if_needed\(typedConditionalStringRaceSource\) : xl_fn_greetAsync\([^)]*typed-cond-string-race-owned[^)]*\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_cstr_release\(xl6_task_race_from_array\(\(xtarray\)\(typedConditionalStringRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional string future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalTaskBoxAll \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxAllSource\) : xl_fn_makeTaskBox\(720LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxAllSource\) : xl_fn_makeTaskBox\(730LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalTaskBoxAllJobs\), xl6_type_desc_XL_Class_TaskBox\(\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalTaskBoxRace \? xl6_future_add_ref_if_needed\(typedConditionalTaskBoxRaceSource\) : xl_fn_makeTaskBox\(750LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_task_race_from_array\(\(xtarray\)\(typedConditionalTaskBoxRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional class future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalTaskFuncAll \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncAllSource\) : xl_fn_makeTaskAdder\(770LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch '0 \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncAllSource\) : xl_fn_makeTaskAdder\(780LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_from_array\(\(xtarray\)\(typedConditionalTaskFuncAllJobs\), xrtTypeFunction\(\)' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'chooseTypedConditionalTaskFuncRace \? xl6_future_add_ref_if_needed\(typedConditionalTaskFuncRaceSource\) : xl_fn_makeTaskAdder\(800LL\)[\s\S]*?xrtTypedArrayAppend\([^,]+,[\s\S]*?xFutureRelease\(__xl6_typed_future_' -or $text -notmatch 'xl6_task_race_from_array\(\(xtarray\)\(typedConditionalTaskFuncRaceJobs\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array conditional function future ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'if \(!xrtTypedArrayAppend\([^,]+, &(__xl6_typed_future_\d+)\)\) \{[\s\S]*?xFutureRelease\(\1\);[\s\S]*?xl6_runtime_fatal\(3\);[\s\S]*?\} else \{[\s\S]*?xFutureRelease\(\1\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing typed-array resource future append failure cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?\(chooseConditionalNull \? xl_fn_addAsync\(290LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalNullLiteralFuture\)\), 1' -or $text -notmatch 'xl6_future_await_i64_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalNull \? xl_fn_addAsync\(300LL, 1LL\) : xl6_future_add_ref_if_needed\(conditionalNullLiteralFuture\)\), 1'){ exit 1 }"
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); $checks=@('xl6_future_await_handle_release(xl6_task_all_values(xrtTypeString()', '(chooseConditionalStringAll ? xl6_future_add_ref_if_needed(conditionalStringAllSource) : xl_fn_greetAsync(', '(0 ? xl6_future_add_ref_if_needed(conditionalStringAllSource) : xl_fn_greetAsync(', 'xl6_future_await_cstr_release(xl6_task_race_values(', '(chooseConditionalStringRace ? xl6_future_add_ref_if_needed(conditionalStringRaceSource) : xl_fn_greetAsync('); foreach($s in $checks){ if(-not $text.Contains($s)){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional string task.all/race resource ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'conditionalTaskBoxAllFuture\s*=\s*xl6_task_all_values\(xl6_type_desc_XL_Class_TaskBox\(\),[\s\S]*?\(chooseConditionalTaskBoxAll \? xl6_future_add_ref_if_needed\(conditionalTaskBoxAllSource\) : xl_fn_makeTaskBox\(620LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalTaskBoxAllSource\) : xl_fn_makeTaskBox\(630LL\)\), 1' -or $text -notmatch 'xl6_task_race_values\([\s\S]*?\(chooseConditionalTaskBoxRace \? xl6_future_add_ref_if_needed\(conditionalTaskBoxRaceSource\) : xl_fn_makeTaskBox\(650LL\)\), 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_api.c missing conditional class task.all/race resource ownership normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_api.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_handle_release\(xl6_task_all_values\(xrtTypeFunction\(\),[\s\S]*?\(chooseConditionalTaskFuncAll \? xl6_future_add_ref_if_needed\(conditionalTaskFuncAllSource\) : xl_fn_makeTaskAdder\(670LL\)\), 1,[\s\S]*?\(0 \? xl6_future_add_ref_if_needed\(conditionalTaskFuncAllSource\) : xl_fn_makeTaskAdder\(680LL\)\), 1' -or $text -notmatch 'xl6_future_await_function_release\(xl6_task_race_values\([\s\S]*?\(chooseConditionalTaskFuncRace \? xl6_future_add_ref_if_needed\(conditionalTaskFuncRaceSource\) : xl_fn_makeTaskAdder\(700LL\)\), 1'){ exit 1 }"
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

echo DEMO6 suite ok: task API cgen assertions 4/4
exit /b 0

