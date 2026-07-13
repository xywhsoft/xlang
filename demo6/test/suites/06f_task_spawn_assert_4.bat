@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn cgen assertions 4/4...

if not "%XL6_TASK_SPAWN_CGEN_READY%"=="1" (
    release\xl.exe check test\cases\cgen_task_spawn.xl
    if errorlevel 1 exit /b 1
    release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c
    if errorlevel 1 exit /b 1
)

findstr /C:"xl6_task_cancel_value(spawnStringRaceCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed string task.race over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"postCancelSpawnStringRaceCompleted = xrtCopyStr(xl6_future_await_cstr_copy(spawnStringRaceCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep typed-array completed string task.race over task.spawn futures awaitable after cancel.
    exit /b 1
)
findstr /C:"spawnBoolAllCompletedFuture = xl6_task_all_from_array((xtarray)(spawnBoolAllCompletedJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array completed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(spawnBoolAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnBoolAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed bool task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeBool(), (*(xtarray*)xl6_future_await_value(spawnBoolAllCompletedFuture" build\test\cgen_task_spawn.c >nul
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
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(spawnFloatAllCompletedFuture" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for typed-array completed float task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_cancel_value(spawnFloatAllCompletedFuture, 0)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should cancel typed-array completed float task.all over task.spawn futures as borrowed.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy_mode(xrtTypeFloat(), (*(xtarray*)xl6_future_await_value(spawnFloatAllCompletedFuture" build\test\cgen_task_spawn.c >nul
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
findstr /C:"xl6_task_all_from_array((xtarray)(failedStringSpawnAllJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array string task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"failedStringSpawnTypedRace = xrtCopyStr(xl6_future_await_cstr_release(xl6_task_race_from_array((xtarray)(failedStringSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
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
findstr /C:"spawnStringAllFailureSlowValue = xrtCopyStr(xl6_future_await_cstr_copy(spawnStringAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should keep slow string task.spawn source awaitable after task.all failure.
    exit /b 1
)
findstr /C:"typedSpawnStringAllFailureGroup = xl6_task_all_from_array((xtarray)(typedSpawnStringAllFailureJobs), xrtTypeString()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all fast-failure over task.spawn fixture.
    exit /b 1
)
findstr /C:"xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(typedSpawnStringAllFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.all fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnStringAllFailureSlowValue = xrtCopyStr(xl6_future_await_cstr_copy(typedSpawnStringAllFailureSlowSource" build\test\cgen_task_spawn.c >nul
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
findstr /C:"spawnStringRaceFailureLoserValueB = xrtCopyStr(xl6_future_await_cstr_copy(spawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
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
findstr /C:"typedSpawnStringRaceFailureValue = xrtCopyStr(xl6_future_await_cstr_copy(typedSpawnStringRaceFailureGroup" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing typed-array string task.race fast-failure group await.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserValueA = xrtCopyStr(xl6_future_await_cstr_copy(typedSpawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing first await for canceled typed-array string task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserValueB = xrtCopyStr(xl6_future_await_cstr_copy(typedSpawnStringRaceFailureSlowLoser" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing repeated await for canceled typed-array string task.spawn race loser.
    exit /b 1
)
findstr /C:"typedSpawnStringRaceFailureLoserStateB == 42LL" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should preserve canceled typed-array string task.spawn race loser after worker completes.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedBoolSpawnAllJobs), xrtTypeBool()" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array bool task.all over task.spawn futures.
    exit /b 1
)
findstr /C:"failedBoolSpawnTypedRace = xl6_future_await_i32_release(xl6_task_race_from_array((xtarray)(failedBoolSpawnRaceJobs)" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array bool task.race over task.spawn futures.
    exit /b 1
)
findstr /C:"xl6_task_all_from_array((xtarray)(failedFloatSpawnAllJobs), xrtTypeFloat()" build\test\cgen_task_spawn.c >nul
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnBoolAllFailureValues = xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(typedSpawnBoolAllFailureGroup')){ exit 1 }"
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if(-not $text.Contains('typedSpawnFloatAllFailureValues = xrtTypedArrayRetain((*(xtarray*)xl6_future_await_value(typedSpawnFloatAllFailureGroup')){ exit 1 }"
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered conditional var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered binary var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered null-coalesce var task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_fn_scoreSpawnResult\(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered positional call-argument task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl_fn_combineSpawnResult\(1LL,\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered named call-argument task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('conditionDynamicSpawnCount = 0LL;'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch 'xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?__xl6_if_cond_\d+ = \(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?\1\)[\s\S]*?if \(__xl6_if_cond_\d+\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered if-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('whileDynamicSpawnCount = 0LL;'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch 'while \(1\)[\s\S]*?xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?__xl6_while_cond_\d+ = \(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?\1\)[\s\S]*?if \(!__xl6_while_cond_\d+\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered while-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('doDynamicSpawnCount = 0LL;'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(250000,$text.Length-$idx)); if($slice -notmatch 'while \(1\)[\s\S]*?xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?__xl6_do_cond_\d+ = \(\(doDynamicSpawnCount < 2LL\) && \(xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?\1\)[\s\S]*?if \(!__xl6_do_cond_\d+\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered do-while-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_future_await_i64_release\(xl6_task_spawn_values\(variadicDefaultJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
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
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_fn_makeSpawnValue(221LL)'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-1000); $slice=$text.Substring($start,[Math]::Min(8000,$text.Length-$start)); if($slice -notmatch 'xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?xl_fn_makeSpawnValue\(221LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?\1[\s\S]*?task\.spawn dynamic var argument should lower inside assert conditions'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered assert-condition task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_fn_makeSpawnValue(401LL)'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-1000); $slice=$text.Substring($start,[Math]::Min(8000,$text.Length-$start)); if($slice -notmatch 'xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?xl_fn_makeSpawnValue\(401LL\)[\s\S]*?xl6_error_raise\(\x22AssertError\x22,[\s\S]*?xl6_task_spawn_values\(varMessageJob,[\s\S]*?\1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered assert-message task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_fn_makeSpawnValue(301LL)'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-1000); $slice=$text.Substring($start,[Math]::Min(8000,$text.Length-$start)); if($slice -notmatch 'xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?xl_fn_makeSpawnValue\(301LL\)[\s\S]*?xl6_error_raise\(\x22Error\x22,[\s\S]*?xl6_task_spawn_values\(varMessageJob,[\s\S]*?\1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered throw-expression task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('static void xl_gen_worker_xl_fn_dynamicSpawnStream('); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(20000,$text.Length-$idx)); if($slice -notmatch 'xvalue (__xl6_task_arg_\d+) = 0;[\s\S]*?xl6_generator_yield_copy\([^;]*xl6_task_spawn_values\(job,[\s\S]*?\1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered yield-expression task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('rangeDynamicSpawnTotal'); if($idx -lt 0){exit 1}; $rangeIdx=$text.IndexOf('long long __xl6_range_start_', $idx); if($rangeIdx -lt 0){exit 1}; $start=[Math]::Max($idx, $rangeIdx-4000); $slice=$text.Substring($start,[Math]::Min(40000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_[\s\S]*?xl_fn_makeSpawnValue\(0LL\)[\s\S]*?long long __xl6_range_start_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'xl_fn_makeSpawnValue\(2LL\)[\s\S]*?xl_fn_makeSpawnValue\(1LL\)[\s\S]*?long long __xl6_range_stop_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_retain\(emptySpawnArg\)[\s\S]*?xl_fn_makeSpawnValue\(0LL\)[\s\S]*?long long __xl6_range_step_\d+\s*=\s*xl6_future_await_i64_release\(xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'for \(long long __xl6_range_value_\d+ = __xl6_range_start_\d+;[\s\S]*?rangeDynamicSpawnTotal = \(rangeDynamicSpawnTotal \+ i\);'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered range-boundary task.spawn arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('indexDynamicSpawnRead = __xl6_global_init_'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-3000); $slice=$text.Substring($start,[Math]::Min(9000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered index-read task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('safeIndexDynamicSpawnRead = __xl6_global_init_'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-3000); $slice=$text.Substring($start,[Math]::Min(9000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_box_or_null\(xrtTypeInt\(\), xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_value_to_i64_release'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered safe-index-read task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('safeIndexDynamicSpawnFallback = __xl6_global_init_'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-3000); $slice=$text.Substring($start,[Math]::Min(10000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xl6_xrt_typed_box_or_null\(xrtTypeInt\(\), xl6_xrt_typed_array_get_at\(indexDynamicSpawnValues' -or $slice -notmatch 'xvoIsNull\(__xl6_nc_' -or $slice -notmatch 'xvoCreateInt\(\(long long\)\(88LL\)\)' -or $slice -notmatch 'xl6_xrt_value_to_i64_release\(__xl6_call_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered safe-index fallback task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('iterableDynamicSpawnArrayTotal = 0LL'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(120000,$text.Length-$idx)); $checks=@('__xl6_task_arg_','xl_fn_makeSpawnValue(2LL)','xl_fn_makeDynamicSpawnArray(xl6_future_await_i64_release(xl6_task_spawn_values(varArgJob','xtarray __xl6_typed_iter_'); foreach($p in $checks){ if($slice.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-container iterable task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('iterableDynamicSpawnGeneratorTotal = 0LL'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(120000,$text.Length-$idx)); $checks=@('__xl6_task_arg_','xl6_xrt_basic_binary','xl_fn_makeDynamicSpawnGenerator(xl6_future_await_i64_release(xl6_task_spawn_values(varArgJob','XL6_Generator* __xl6_gen_iter_'); foreach($p in $checks){ if($slice.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered generator iterable task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl6_xrt_typed_array_set_at(indexDynamicSpawnValues'); if($idx -lt 0){exit 1}; $start=[Math]::Max(0,$idx-2000); $slice=$text.Substring($start,[Math]::Min(6000,$text.Length-$start)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xvoCreateInt\(\(long long\)\(77LL\)\)' -or $slice -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $slice -notmatch '__xl6_index_assign_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered index-write-target task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $checks=@('__xl6_task_arg_','xrtTypedArrayAppend(__xl6_global_init_','typedDynamicSpawnAllJobs = __xl6_global_init_','xl6_task_all_from_array((xtarray)(typedDynamicSpawnAllJobs)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-array task.all task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $checks=@('__xl6_task_arg_','xrtTypedArrayAppend(__xl6_global_init_','typedDynamicSpawnRaceJobs = __xl6_global_init_','xl6_task_race_from_array((xtarray)(typedDynamicSpawnRaceJobs)'); foreach($p in $checks){ if($text.IndexOf($p) -lt 0){ exit 1 } }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered typed-array task.race task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_async_worker_xl_fn_makeTypedDynamicSpawnAllValues'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(6000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xrtTypedArrayAppendValue\(__xl6_async_return_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered async typed-array return task.spawn arguments.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); $idx=$text.IndexOf('xl_async_worker_xl_fn_makeTypedDynamicSpawnAwaitValues'); if($idx -lt 0){exit 1}; $slice=$text.Substring($idx,[Math]::Min(5000,$text.Length-$idx)); if($slice -notmatch '__xl6_task_arg_' -or $slice -notmatch 'xrtTypedArrayAppendValue\(__xl6_async_return_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing lowered async typed-array return awaited task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'mixedTailBorrowedFuture\s*=\s*xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : xl_fn_makeAdder\(520LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'mixedTailOwnedFuture\s*=\s*xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : xl_fn_makeAdder\(530LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'xl6_task_spawn_values\(\(chooseMixedTailClosure \? xl6_function_copy_value\(mixedTailClosureSource\) : xl_fn_makeAdder\(540LL\)\)[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_spawn_values\(\(0 \? xl6_function_copy_value\(mixedTailClosureSource\) : xl_fn_makeAdder\(550LL\)\)[\s\S]*?, 0, 1'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing mixed conditional future/all callable normalization.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureFuture\s*=\s*xl6_task_spawn_values\(pendingClosureSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureFuture, 0\)' -or $text -notmatch 'pendingClosureStateA = 42LL' -or $text -notmatch 'pendingClosureStateB = 42LL' -or $text -notmatch 'pendingClosureSource\.signature[\s\S]*?== 600LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.spawn cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureAllLeft\s*=\s*xl6_task_spawn_values\(pendingClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'pendingClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?pendingClosureAllLeft, 0, xl6_task_spawn_values\(xl_fn_makeSleepyAdder\(620LL\), xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureAllGroup, 0\)' -or $text -notmatch 'pendingClosureAllGroupState = 42LL' -or $text -notmatch 'pendingClosureAllLeftStateA = 42LL' -or $text -notmatch 'pendingClosureAllLeftStateB = 42LL' -or $text -notmatch 'pendingClosureAllSource\.signature[\s\S]*?== 610LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.all cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'failureClosureAllSlow\s*=\s*xl6_task_spawn_values\(failureClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'failureClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?xl6_task_spawn_values\(throwingJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 0\), 1, failureClosureAllSlow, 0' -or $text -notmatch 'failureClosureAllGroupState = 42LL' -or $text -notmatch 'failureClosureAllGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(failureClosureAllGroup, 0\)' -or $text -notmatch 'failureClosureAllGroupStateAfterCancel = 42LL' -or $text -notmatch 'failureClosureAllSlowValue == 890LL' -or $text -notmatch 'failureClosureAllSource\.signature[\s\S]*?== 670LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedFailureClosureAllSlow\s*=\s*xl6_task_spawn_values\(typedFailureClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'typedFailureClosureAllGroup\s*=\s*xl6_task_all_from_array\(\(xtarray\)\(typedFailureClosureAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'typedFailureClosureAllGroupState = 42LL' -or $text -notmatch 'typedFailureClosureAllGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(typedFailureClosureAllGroup, 0\)' -or $text -notmatch 'typedFailureClosureAllGroupStateAfterCancel = 42LL' -or $text -notmatch 'typedFailureClosureAllSlowValue == 900LL' -or $text -notmatch 'typedFailureClosureAllSource\.signature[\s\S]*?== 680LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed typed-array task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'successClosureAllSlow\s*=\s*xl6_task_spawn_values\(successClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'successClosureAllGroup\s*=\s*xl6_task_all_values\(xrtTypeInt\(\),[\s\S]*?xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?\), 1, successClosureAllSlow, 0' -or $text -notmatch 'xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(successClosureAllGroup' -or $text -notmatch '!xl6_task_cancel_value\(successClosureAllGroup, 0\)' -or $text -notmatch 'xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(successClosureAllGroup' -or $text -notmatch 'successClosureAllSlowAgain == 915LL' -or $text -notmatch 'successClosureAllSource\.signature[\s\S]*?== 695LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedSuccessClosureAllSlow\s*=\s*xl6_task_spawn_values\(typedSuccessClosureAllSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'typedSuccessClosureAllGroup\s*=\s*xl6_task_all_from_array\(\(xtarray\)\(typedSuccessClosureAllJobs\), xrtTypeInt\(\)' -or $text -notmatch 'xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(typedSuccessClosureAllGroup' -or $text -notmatch '!xl6_task_cancel_value\(typedSuccessClosureAllGroup, 0\)' -or $text -notmatch 'xl6_xrt_typed_array_copy_mode\(xrtTypeInt\(\), \(\*\(xtarray\*\)xl6_future_await_value\(typedSuccessClosureAllGroup' -or $text -notmatch 'typedSuccessClosureAllSlowAgain == 920LL' -or $text -notmatch 'typedSuccessClosureAllSource\.signature[\s\S]*?== 700LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful typed-array task.all closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'pendingClosureRaceLeft\s*=\s*xl6_task_spawn_values\(pendingClosureRaceSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'pendingClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?pendingClosureRaceLeft, 0, xl6_task_spawn_values\(xl_fn_makeSleepyAdder\(640LL\), xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 0, 1' -or $text -notmatch 'xl6_task_cancel_value\(pendingClosureRaceGroup, 0\)' -or $text -notmatch 'pendingClosureRaceGroupState = 42LL' -or $text -notmatch 'pendingClosureRaceLeftStateA = 42LL' -or $text -notmatch 'pendingClosureRaceLeftStateB = 42LL' -or $text -notmatch 'pendingClosureRaceSource\.signature[\s\S]*?== 630LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing pending closure task.race cancel/source-callable lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'successClosureRaceLoser\s*=\s*xl6_task_spawn_values\(successClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'successClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?xl6_task_spawn_values\(addJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 2,[\s\S]*?\), 1, successClosureRaceLoser, 0' -or $text -notmatch 'successClosureRaceValue = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAgain = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAgain == 651LL' -or $text -notmatch '!xl6_task_cancel_value\(successClosureRaceGroup, 0\)' -or $text -notmatch 'successClosureRaceValueAfterCancel = \(\*\(long long\*\)xl6_future_await_value\(successClosureRaceGroup' -or $text -notmatch 'successClosureRaceValueAfterCancel == 651LL' -or $text -notmatch 'successClosureRaceLoserStateA = 42LL' -or $text -notmatch 'successClosureRaceLoserStateB = 42LL' -or $text -notmatch 'successClosureRaceLoserSource\.signature[\s\S]*?== 650LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedSuccessClosureRaceLoser\s*=\s*xl6_task_spawn_values\(typedSuccessClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'typedSuccessClosureRaceGroup\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedSuccessClosureRaceJobs\)' -or $text -notmatch 'typedSuccessClosureRaceValue == 711LL' -or $text -notmatch 'typedSuccessClosureRaceValueAgain = \(\*\(long long\*\)xl6_future_await_value\(typedSuccessClosureRaceGroup' -or $text -notmatch 'typedSuccessClosureRaceValueAgain == 711LL' -or $text -notmatch '!xl6_task_cancel_value\(typedSuccessClosureRaceGroup, 0\)' -or $text -notmatch 'typedSuccessClosureRaceValueAfterCancel = \(\*\(long long\*\)xl6_future_await_value\(typedSuccessClosureRaceGroup' -or $text -notmatch 'typedSuccessClosureRaceValueAfterCancel == 711LL' -or $text -notmatch 'typedSuccessClosureRaceLoserStateA = 42LL' -or $text -notmatch 'typedSuccessClosureRaceLoserStateB = 42LL' -or $text -notmatch 'typedSuccessClosureRaceLoserSource\.signature[\s\S]*?== 710LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing successful typed-array task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'failureClosureRaceLoser\s*=\s*xl6_task_spawn_values\(failureClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'failureClosureRaceGroup\s*=\s*xl6_task_race_values\([\s\S]*?xl6_task_spawn_values\(throwingJob, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 0\), 1, failureClosureRaceLoser, 0' -or $text -notmatch 'failureClosureRaceGroupState = 42LL' -or $text -notmatch 'failureClosureRaceGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(failureClosureRaceGroup, 0\)' -or $text -notmatch 'failureClosureRaceGroupStateAfterCancel = 42LL' -or $text -notmatch 'failureClosureRaceLoserStateA = 42LL' -or $text -notmatch 'failureClosureRaceLoserStateB = 42LL' -or $text -notmatch 'failureClosureRaceLoserSource\.signature[\s\S]*?== 660LL'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn.c missing failed task.race closure group repeated/cancel/post-cancel lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'typedFailureClosureRaceLoser\s*=\s*xl6_task_spawn_values\(typedFailureClosureRaceLoserSource, xrtTypeInt\(\), sizeof\(long long\),[\s\S]*?, 1, 1, xvoCreateInt' -or $text -notmatch 'xrtTypedArrayCreate\(xrtTypeFuture\(\), XRT_OBJMODE_SHARED\)' -or $text -notmatch 'typedFailureClosureRaceGroup\s*=\s*xl6_task_race_from_array\(\(xtarray\)\(typedFailureClosureRaceJobs\)' -or $text -notmatch 'typedFailureClosureRaceGroupState = 42LL' -or $text -notmatch 'typedFailureClosureRaceGroupStateAgain = 42LL' -or $text -notmatch '!xl6_task_cancel_value\(typedFailureClosureRaceGroup, 0\)' -or $text -notmatch 'typedFailureClosureRaceGroupStateAfterCancel = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserStateA = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserStateB = 42LL' -or $text -notmatch 'typedFailureClosureRaceLoserSource\.signature[\s\S]*?== 690LL'){ exit 1 }"
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
findstr /C:"static DWORD xl6__active_error_tls_index = TLS_OUT_OF_INDEXES;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should allocate Windows TLS for active error state.
    exit /b 1
)
findstr /C:"return xl6_error_active_get() != 0;" build\test\cgen_task_spawn.c >nul
if errorlevel 1 (
    echo cgen_task_spawn.c should read active error state through TLS accessor.
    exit /b 1
)

echo DEMO6 suite ok: task spawn cgen assertions 4/4
exit /b 0
