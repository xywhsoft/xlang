@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn cgen assertions 2/4...

if not "%XL6_TASK_SPAWN_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_spawn.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c
    if errorlevel 1 exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_task_spawn.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1

findstr /C:"nullFirstSpawnAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.all assertion over task.spawn.
    exit /b 1
)
findstr /C:"nullFirstSpawnRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing null-first task.race assertion over task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnStringFuture = 0;" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnStringFuture, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed string null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnStringAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string task.race null source assertion.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.all fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnStringFuture, 0, xl6_task_spawn_values(greetJob, xrtTypeString(), sizeof(const char*)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed string null source before owned task.spawn.
    exit /b 1
)
findstr /C:"null-first-all" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source after null source for task.all.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.race fixture over owned task.spawn.
    exit /b 1
)
findstr /C:"null-first-race" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned string task.spawn source after null source for task.race.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.all assertion over task.spawn.
    exit /b 1
)
findstr /C:"nullFirstSpawnStringRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing string null-first task.race assertion over task.spawn.
    exit /b 1
)
findstr /C:"missingSpawnBoolFuture = 0;" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(flipJob, xrtTypeBool32(), sizeof(int)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned bool task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnBoolFuture, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed bool null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnBoolRace = xl6_future_await_i32_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnBoolAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnBoolRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing bool task.race null source assertion.
    exit /b 1
)
findstr /C:"missingSpawnFloatFuture = 0;" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float null task.spawn aggregation source fixture.
    exit /b 1
)
findstr /C:"xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"xl6_task_spawn_values(halfJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing owned float task.spawn source for null aggregation fixture.
    exit /b 1
)
findstr /C:"missingSpawnFloatFuture, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass borrowed float null source to task aggregation.
    exit /b 1
)
findstr /C:"nullSpawnFloatRace = xl6_future_await_f64_release(xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race null source fixture over task.spawn.
    exit /b 1
)
findstr /C:"nullSpawnFloatAllState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.all null source assertion.
    exit /b 1
)
findstr /C:"nullSpawnFloatRaceState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing float task.race null source assertion.
    exit /b 1
)
findstr /C:"failedStringValue = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_spawn_values(throwingStringJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn temporary future await.
    exit /b 1
)
findstr /C:"repeatedFailedStringSpawn = xl6_task_spawn_values(throwingStringJob, xrtTypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedString = xrtCopyStr(xl6_future_await_cstr_copy(repeatedFailedStringSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed string task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedStringSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed string task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedString = xrtCopyStr(xl6_future_await_cstr_copy(repeatedFailedStringSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed string task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"failedBoolValue = xl6_future_await_i32_release(xl6_task_spawn_values(throwingBoolJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn temporary future await.
    exit /b 1
)
findstr /C:"repeatedFailedBoolSpawn = xl6_task_spawn_values(throwingBoolJob, xrtTypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedBool = (*(int*)xl6_future_await_value(repeatedFailedBoolSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed bool task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedBoolSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed bool task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedBool = (*(int*)xl6_future_await_value(repeatedFailedBoolSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed bool task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"failedFloatValue = xl6_future_await_f64_release(xl6_task_spawn_values(throwingFloatJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn temporary future await.
    exit /b 1
)
findstr /C:"repeatedFailedFloatSpawn = xl6_task_spawn_values(throwingFloatJob, xrtTypeFloat()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn future fixture.
    exit /b 1
)
findstr /C:"firstFailedFloat = (*(double*)xl6_future_await_value(repeatedFailedFloatSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed float task.spawn named future first await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(repeatedFailedFloatSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass failed float task.spawn future cancel as borrowed.
    exit /b 1
)
findstr /C:"postCancelFailedFloat = (*(double*)xl6_future_await_value(repeatedFailedFloatSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep failed float task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"namedGreetFuture = xl6_task_spawn_values(greetJob" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named string task.spawn future initialization.
    exit /b 1
)
findstr /C:"namedGreetA = xrtCopyStr(xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn string first await copy.
    exit /b 1
)
findstr /C:"namedGreetB = xrtCopyStr(xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named task.spawn string repeated await copy.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedGreetFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed string task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedGreet = xrtCopyStr(xl6_future_await_cstr_copy(namedGreetFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed string task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"pendingStringSpawn = xl6_task_spawn_values(slowGreetJob, xrtTypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingStringSpawnValueA = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingStringSpawnValueB = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingStringSpawnStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingStringAllGroup = xl6_task_all_values(xrtTypeString()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingStringAllLeft, 0, pendingStringAllRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending string task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringAllGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringAllGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending string task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingStringAllLeftValueA = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingStringAllLeftValueB = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingStringAllLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingStringRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending string task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingStringRaceLeft, 0, pendingStringRaceRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending string task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingStringRaceGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending string task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingStringRaceGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending string task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftValueA = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first string task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftValueB = xrtCopyStr(xl6_future_await_cstr_copy(pendingStringRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late string task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingStringRaceLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending string task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'flippedValue\s*=\s*xl6_future_await_i32_release\(xl6_task_spawn_values\(flipJob,\s*xrtTypeBool32\(\),\s*sizeof\(int\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateBool\(1\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool temporary future bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedFlipFuture\s*=\s*xl6_task_spawn_values\(flipJob,\s*xrtTypeBool32\(\),\s*sizeof\(int\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateBool\(0\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool named future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedFlipA\s*=\s*\(\*\(int\*\)xl6_future_await_value\(namedFlipFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(int\)\)' -or $text -notmatch 'namedFlipB\s*=\s*\(\*\(int\*\)xl6_future_await_value\(namedFlipFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(int\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn bool repeated named future await copies.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedFlipFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed bool task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedFlip = (*(int*)xl6_future_await_value(namedFlipFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed bool task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"pendingBoolSpawn = xl6_task_spawn_values(slowFlipJob, xrtTypeBool32(), sizeof(int)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolSpawnValueA = (*(int*)xl6_future_await_value(pendingBoolSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingBoolSpawnValueB = (*(int*)xl6_future_await_value(pendingBoolSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingBoolSpawnStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingBoolAllGroup = xl6_task_all_values(xrtTypeBool32()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingBoolAllLeft, 0, pendingBoolAllRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending bool task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolAllGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolAllGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending bool task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftValueA = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftValueB = (*(int*)xl6_future_await_value(pendingBoolAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingBoolAllLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingBoolRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending bool task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeft, 0, pendingBoolRaceRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending bool task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingBoolRaceGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending bool task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingBoolRaceGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending bool task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftValueA = (*(int*)xl6_future_await_value(pendingBoolRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first bool task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftValueB = (*(int*)xl6_future_await_value(pendingBoolRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late bool task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingBoolRaceLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending bool task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'halfValue\s*=\s*xl6_future_await_f64_release\(xl6_task_spawn_values\(halfJob,\s*xrtTypeFloat\(\),\s*sizeof\(double\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateFloat\(\(double\)\(1\.5\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float temporary future bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedHalfFuture\s*=\s*xl6_task_spawn_values\(halfJob,\s*xrtTypeFloat\(\),\s*sizeof\(double\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateFloat\(\(double\)\(2\.5\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float named future initialization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'namedHalfA\s*=\s*\(\*\(double\*\)xl6_future_await_value\(namedHalfFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(double\)\)' -or $text -notmatch 'namedHalfB\s*=\s*\(\*\(double\*\)xl6_future_await_value\(namedHalfFuture,\s*\"\",\s*\d+,\s*\d+,\s*1,\s*sizeof\(double\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn float repeated named future await copies.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(namedHalfFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed float task.spawn cancel guard.
    exit /b 1
)
findstr /C:"postCancelNamedHalf = (*(double*)xl6_future_await_value(namedHalfFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep completed float task.spawn future awaitable after cancel.
    exit /b 1
)
findstr /C:"pendingFloatSpawn = xl6_task_spawn_values(slowHalfJob, xrtTypeFloat(), sizeof(double)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.spawn cancel fixture.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.spawn future as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatSpawnValueA = (*(double*)xl6_future_await_value(pendingFloatSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float await after pending task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingFloatSpawnValueB = (*(double*)xl6_future_await_value(pendingFloatSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float await after pending task.spawn worker completion.
    exit /b 1
)
findstr /C:"pendingFloatSpawnStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.spawn future canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingFloatAllGroup = xl6_task_all_values(xrtTypeFloat()" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.all over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingFloatAllLeft, 0, pendingFloatAllRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending float task.all sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatAllGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.all group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatAllGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending float task.all group as canceled.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftValueA = (*(double*)xl6_future_await_value(pendingFloatAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float task.all source await after group cancel.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftValueB = (*(double*)xl6_future_await_value(pendingFloatAllLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float task.all source await after worker completion.
    exit /b 1
)
findstr /C:"pendingFloatAllLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.all source canceled after worker completes.
    exit /b 1
)
findstr /C:"pendingFloatRaceGroup = xl6_task_race_values" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending float task.race over task.spawn group fixture.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeft, 0, pendingFloatRaceRight, 0" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should pass pending float task.race sources as borrowed futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingFloatRaceGroup, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel pending float task.race group as borrowed.
    exit /b 1
)
findstr /C:"pendingFloatRaceGroupState == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should await pending float task.race group as canceled.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftValueA = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first float task.race source await after group cancel.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftValueB = (*(double*)xl6_future_await_value(pendingFloatRaceLeft" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late float task.race source await after worker completion.
    exit /b 1
)
findstr /C:"pendingFloatRaceLeftStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending float task.race source canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(10LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn variadic non-empty argv bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicEmptyValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*1,\s*xvoCreateInt\(\(long long\)\(10LL\)\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn empty array vararg argv bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultJob\s*=\s*xl6_fnvalue_xl_fn_totalDefault_sig[0-9A-F]+_arity0_vararg_make\(\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing zero-fixed variadic optional adapter task.spawn case.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultEmptyValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*0\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn zero-fixed vararg adapter empty argv bridge.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultExtraValue\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing task.spawn zero-fixed vararg adapter extra argv bridge.
    exit /b 1
)
findstr /C:"variadicFuture = xl6_task_spawn_values(variadicJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicFuture\s*=\s*xl6_task_spawn_values\(variadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(10LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"variadicFutureA = (*(long long*)xl6_future_await_value(variadicFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn first await.
    exit /b 1
)
findstr /C:"variadicFutureB = (*(long long*)xl6_future_await_value(variadicFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named variadic task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed variadic task.spawn cancel guard.
    exit /b 1
)
findstr /C:"variadicDefaultFuture = xl6_task_spawn_values(variadicDefaultJob, xrtTypeInt(), sizeof(long long)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn future.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'variadicDefaultFuture\s*=\s*xl6_task_spawn_values\(variadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"variadicDefaultFutureA = (*(long long*)xl6_future_await_value(variadicDefaultFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn first await.
    exit /b 1
)
findstr /C:"variadicDefaultFutureB = (*(long long*)xl6_future_await_value(variadicDefaultFuture" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing named zero-fixed variadic task.spawn repeated await.
    exit /b 1
)
findstr /C:"!xl6_task_cancel_value(variadicDefaultFuture, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing completed zero-fixed variadic task.spawn cancel guard.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[Text.Encoding]::UTF8); if($text -notmatch 'slowVariadicJob\s*=\s*xl6_fnvalue_xl_fn_slowSumVariadic_sig[0-9A-F]+_make\(\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn function value.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingVariadicSpawn\s*=\s*xl6_task_spawn_values\(slowVariadicJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*4,\s*xvoCreateInt\(\(long long\)\(300LL\)\),\s*xvoCreateInt\(\(long long\)\(1LL\)\),\s*xvoCreateInt\(\(long long\)\(2LL\)\),\s*xvoCreateInt\(\(long long\)\(3LL\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn argv bridge.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(pendingVariadicSpawn, 0)" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending variadic task.spawn cancel.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnValueA = (*(long long*)xl6_future_await_value(pendingVariadicSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled pending variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnValueB = (*(long long*)xl6_future_await_value(pendingVariadicSpawn" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing late await for canceled pending variadic task.spawn.
    exit /b 1
)
findstr /C:"pendingVariadicSpawnStateB == 42LL" build\test\cgen_task_spawn.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep pending variadic task.spawn canceled after worker completes.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[Text.Encoding]::UTF8); if($text -notmatch 'slowVariadicDefaultJob\s*=\s*xl6_fnvalue_xl_fn_slowTotalDefault_sig[0-9A-F]+_arity0_vararg_make\(\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn adapter.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingVariadicDefaultSpawn\s*=\s*xl6_task_spawn_values\(slowVariadicDefaultJob,\s*xrtTypeInt\(\),\s*sizeof\(long long\),\s*\"\",\s*\d+,\s*\d+,\s*1,\s*2,\s*xvoCreateText\(\(void\*\)\(\"a\"\),\s*0,\s*false\),\s*xvoCreateText\(\(void\*\)\(\"b\"\),\s*0,\s*false\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending zero-fixed variadic task.spawn argv bridge.
    exit /b 1
)

echo DEMO6 suite ok: task spawn cgen assertions 2/4
exit /b 0

