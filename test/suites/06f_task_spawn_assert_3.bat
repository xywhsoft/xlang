@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn cgen assertions 3/4...

if not "%XL6_TASK_SPAWN_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_spawn.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c
    if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_task_spawn.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"xl6_task_cancel_value(pendingVariadicDefaultSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnValueA = (*(long long*)xl6_future_await_value(pendingVariadicDefaultSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled pending zero-fixed variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnValueB = (*(long long*)xl6_future_await_value(pendingVariadicDefaultSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await for canceled pending zero-fixed variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicDefaultSpawnStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending zero-fixed variadic task.spawn canceled after worker completes.
    exit /b 1
)
findstr /C:"variadicAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array for variadic task.spawn task.all.
    exit /b 1
)
findstr /C:"variadicAllFuture = xl6_task_all_from_array((xtarray)(variadicAllJobs), xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all over variadic task.spawn typed array.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6TypeInt64(), (*(xtarray*)xl6_future_await_value(variadicAllFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing copied await for variadic task.all future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicAllFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.all cancel guard.
    exit /b 1
)
findstr /C:"variadicRaceJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed array for variadic task.spawn task.race.
    exit /b 1
)
findstr /C:"variadicRaceFuture = xl6_task_race_from_array((xtarray)(variadicRaceJobs)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race over variadic task.spawn typed array.
    exit /b 1
)
findstr /C:"variadicRaceA = (*(long long*)xl6_future_await_value(variadicRaceFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing await for variadic task.race future.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicRaceFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.race cancel guard.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicSpawn = xl6_task_cancel_value(xl6_task_spawn_values(variadicJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary variadic task.spawn cancel ownership.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicAll = xl6_task_cancel_value(xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary task.all over variadic task.spawn ownership.
    exit /b 1
)
findstr /C:"cancelTemporaryVariadicRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary task.race over variadic task.spawn ownership.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedVarArgSpawn\s*=\s*xl6_task_spawn_values\(varArgJob,\s*xl6TypeInt64\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xl6_xrt_retain\(dynamicSpawnArg\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn var argument retain bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'literalVarArgValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,\s*xl6TypeInt64\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xrtValueInt\(\(long long\)\(41LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn scalar literal to var argument bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'varStringValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varStringJob,\s*xl6TypeInt64\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xl6_xrt_retain\(dynamicStringSpawnArg\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn string var argument retain bridge.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed task.spawn cancel guard.
    exit /b 1
)
findstr /C:"cancelTemporarySpawn = xl6_task_cancel_value(xl6_task_spawn_values(addJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.spawn future through ownership helper.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(2LL))), 1);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn future after task.cancel.
    exit /b 1
)
findstr /C:"pendingSpawn = xl6_task_spawn_values(slowAddJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnValueA = (*(long long*)xl6_future_await_value(pendingSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingSpawnValueB = (*(long long*)xl6_future_await_value(pendingSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingSpawnCancelStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingSpawnAllGroup = xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeft, 0, pendingSpawnAllRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending task.spawn all sources as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawnAllGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.all over task.spawn group as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeftValueA = (*(long long*)xl6_future_await_value(pendingSpawnAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all task.spawn left source await.
    exit /b 1
)
findstr /C:"pendingSpawnAllRightValueA = (*(long long*)xl6_future_await_value(pendingSpawnAllRight" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.all task.spawn right source await.
    exit /b 1
)
findstr /C:"pendingSpawnAllLeftValueB = (*(long long*)xl6_future_await_value(pendingSpawnAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.all task.spawn source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingSpawnRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeft, 0, pendingSpawnRaceRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending task.spawn race sources as borrowed.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingSpawnRaceGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending task.race over task.spawn group as borrowed.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeftValueA = (*(long long*)xl6_future_await_value(pendingSpawnRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race task.spawn left source await.
    exit /b 1
)
findstr /C:"pendingSpawnRaceRightValueA = (*(long long*)xl6_future_await_value(pendingSpawnRaceRight" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending task.race task.spawn right source await.
    exit /b 1
)
findstr /C:"pendingSpawnRaceLeftValueB = (*(long long*)xl6_future_await_value(pendingSpawnRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending task.race task.spawn source canceled after worker completes.
    exit /b 1
)
findstr /C:"cancelTemporarySpawnAll = xl6_task_cancel_value(xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.all over task.spawn futures through ownership helper.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(4LL))), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn inputs inside task.cancel task.all.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(5LL))), 1), 1);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.all future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporarySpawnRace = xl6_task_cancel_value(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel temporary task.race over task.spawn futures through ownership helper.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(7LL))), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.spawn inputs inside task.cancel task.race.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(8LL))), 1), 1);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary task.race future after task.cancel.
    exit /b 1
)
findstr /C:"cancelTemporaryDefaultSpawn = xl6_task_cancel_value(xl6_task_spawn_values(defaultJob, xl6TypeInt64(), sizeof(long long)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing temporary optional adapter task.spawn cancel ownership.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'cancelTemporaryDefaultSpawn\s*=\s*xl6_task_cancel_value\(xl6_task_spawn_values\(defaultJob,\s*xl6TypeInt64\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xrtValueInt\(\(long long\)\(42LL\)\)\),\s*1\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c should release temporary optional adapter task.spawn future after task.cancel.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnRaceValue = xl6_future_await_i64_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"), 1, xl6_task_spawn_values(defaultJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass temporary task.spawn race futures as owned.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xl6TypeString(), sizeof(const char*)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnStringRaceValue = xl6_xrt_string_dup(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(flipJob, xl6TypeBool32(), sizeof(int)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnBoolRaceValue = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeFloat64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(halfJob, xl6TypeFloat64(), sizeof(double)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"spawnFloatRaceValue = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingStringJob, xl6TypeString(), sizeof(const char*)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedStringSpawnRace = xl6_xrt_string_dup(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingBoolJob, xl6TypeBool32(), sizeof(int)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedBoolSpawnRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xl6TypeFloat64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(throwingFloatJob, xl6TypeFloat64(), sizeof(double)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn source for task aggregation.
    exit /b 1
)
findstr /C:"failedFloatSpawnRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(spawnStringAllJobs), xl6TypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"= xl6_xrt_string_dup(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringTypedRace = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race managed result assignment.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(spawnBoolAllJobs), xl6TypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"= xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnBoolTypedRace = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array bool task.race result assignment.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(spawnFloatAllJobs), xl6TypeFloat64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"= xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnFloatTypedRace = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array float task.race result assignment.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(180LL)), xrtValueInt((long long)(1LL))" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all task.spawn source.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedNullFirstSpawnAllJobs), xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnAllJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first int task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first int task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllTail = (*(long long*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnAllJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first int task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnAllTail == 181LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first int task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xrtValueInt((long long)(190LL)), xrtValueInt((long long)(1LL))" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race task.spawn source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRace = xl6_future_await_i64_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first int task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnRaceJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first int task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first int task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceTail = (*(long long*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnRaceJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first int task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnRaceTail == 191LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first int task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xl6TypeString(), sizeof(const char*)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.spawn source.
    exit /b 1
)
findstr /C:"typed-null-first-all" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all payload.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedNullFirstSpawnStringAllJobs), xl6TypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnStringAllJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first string task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first string task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringAllTail = xl6_xrt_string_dup(xl6_future_await_cstr_copy((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnStringAllJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first string task.all source.
    exit /b 1
)
findstr /C:"typed array string task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first string task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typed-null-first-race" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race payload.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRace = xl6_xrt_string_dup(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first string task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnStringRaceJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first string task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first string task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnStringRaceTail = xl6_xrt_string_dup(xl6_future_await_cstr_copy((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnStringRaceJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first string task.race source.
    exit /b 1
)
findstr /C:"typed array string task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first string task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedNullFirstSpawnBoolAllJobs), xl6TypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnBoolAllJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first bool task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first bool task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolAllTail = (*(int*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnBoolAllJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first bool task.all source.
    exit /b 1
)
findstr /C:"typed array bool task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first bool task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first bool task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnBoolRaceJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first bool task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first bool task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnBoolRaceTail = (*(int*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnBoolRaceJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first bool task.race source.
    exit /b 1
)
findstr /C:"typed array bool task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first bool task.race source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.all source array over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(typedNullFirstSpawnFloatAllJobs), xl6TypeFloat64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.all await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnFloatAllJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first float task.all jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first float task.all error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatAllTail = (*(double*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnFloatAllJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first float task.all source.
    exit /b 1
)
findstr /C:"typed array float task.all null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first float task.all source.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceJobs = __xl6_global_init_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.race source array over task.spawn futures.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRace = xl6_future_await_f64_release(xl6_task_race_from_array((xtarray)(__xl6_call_arg_" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array null-first float task.race await over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6TypedArrayRelease(typedNullFirstSpawnFloatRaceJobs);" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing cleanup for typed-array null-first float task.race jobs.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing assertion for typed-array null-first float task.race error path.
    exit /b 1
)
findstr /C:"typedNullFirstSpawnFloatRaceTail = (*(double*)xl6_future_await_value((*(xfuture**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(typedNullFirstSpawnFloatRaceJobs, (long long)(1LL)), xl6TypeFuture()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail await for typed-array null-first float task.race source.
    exit /b 1
)
findstr /C:"typed array float task.race null-first should leave task.spawn source awaitable" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing tail assertion for typed-array null-first float task.race source.
    exit /b 1
)
findstr /C:"spawnIntAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnIntAllCompletedJobs), xl6TypeInt64()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed int task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6TypeInt64(), (*(xtarray*)xl6_future_await_value(spawnIntAllCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed int task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnIntAllCompletedFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed int task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6TypeInt64(), (*(xtarray*)xl6_future_await_value(spawnIntAllCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed int task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnIntRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnIntRaceCompletedJobs)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed int task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnIntRaceCompletedB = (*(long long*)xl6_future_await_value(spawnIntRaceCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed int task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnIntRaceCompletedFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed int task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnIntRaceCompleted = (*(long long*)xl6_future_await_value(spawnIntRaceCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed int task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnStringAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnStringAllCompletedJobs), xl6TypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6TypeString(), (*(xtarray*)xl6_future_await_value(spawnStringAllCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnStringAllCompletedFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed string task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6TypeString(), (*(xtarray*)xl6_future_await_value(spawnStringAllCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed string task.all over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnStringRaceCompletedFuture = xl6_task_race_from_array((xtarray)(spawnStringRaceCompletedJobs)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed string task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"spawnStringRaceCompletedB = xl6_xrt_string_dup(xl6_future_await_cstr_copy(spawnStringRaceCompletedFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed string task.race over task.spawn futures.
    exit /b 1
)

echo DEMO6 suite ok: task spawn cgen assertions 3/4
exit /b 0

