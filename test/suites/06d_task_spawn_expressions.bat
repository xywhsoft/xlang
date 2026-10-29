@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn range and f-string expressions...

release\xl.exe check test\cases\cgen_task_spawn_range_contains.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_task_spawn_range_contains.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_spawn_range_contains.xl --emit build\test\cgen_task_spawn_range_contains.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn_range_contains.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_task_spawn_range_contains.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1
findstr /C:"xl6_range_contains_i64" build\test\cgen_task_spawn_range_contains.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn_range_contains.c missing range contains helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn_range_contains.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?xl_fn_makeSpawnValue\(0LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'xl_fn_makeSpawnValue\(2LL\)[\s\S]*?xl_fn_makeSpawnValue\(1LL\)[\s\S]*?xl6_xrt_basic_binary[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'xl6_xrt_retain\(emptySpawnArg\)[\s\S]*?xl_fn_makeSpawnValue\(0LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'xl6_range_contains_i64'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn_range_contains.c missing lowered range-contains task.spawn arguments.
    exit /b 1
)

release\xl.exe check test\cases\cgen_task_literal_single_eval.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_task_literal_single_eval.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_literal_single_eval.xl --emit build\test\cgen_task_literal_single_eval.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_literal_single_eval.c',[Text.Encoding]::UTF8); foreach($value in 91001..91004){ $needle='xrtValueInt\(\(long long\)\('+$value+'LL\)\)'; if(([regex]::Matches($text,$needle)).Count -ne 1){ exit 1 } }; if($text -match 'xl6TypedArrayCreate\(xl6TypeFuture\(\)\)'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_literal_single_eval.c should evaluate each task literal element exactly once without a dead temporary array.
    exit /b 1
)

release\xl.exe check test\cases\cgen_task_spawn_fstring.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_task_spawn_fstring.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_task_spawn_fstring.xl --emit build\test\cgen_task_spawn_fstring.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\cgen_task_spawn_fstring.c',[Text.Encoding]::UTF8).Replace('__xl6_user_global_',''); [IO.File]::WriteAllText('build\test\cgen_task_spawn_fstring.assert.c',$text,(New-Object Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_fstr_join" build\test\cgen_task_spawn_fstring.assert.c >nul
if errorlevel 1 (
    echo cgen_task_spawn_fstring.c missing f-string join helper.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn_fstring.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch '__xl6_task_arg_[\s\S]*?xl_fn_makeSpawnValue\(40LL\)[\s\S]*?xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -notmatch 'xl6_xrt_fstr_join'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn_fstring.c missing lowered f-string task.spawn argument.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_task_spawn_fstring.assert.c',[System.Text.Encoding]::UTF8); if($text -notmatch 'xtarray\s+(__xl6_call_arg_\d+)\s*=\s*0;[\s\S]*?sumSpawnItems\(\1\)[\s\S]*?xl6TypedArrayRelease\(\1\)' -or $text -notmatch 'xl6_task_spawn_values\(varArgJob,[\s\S]*?__xl6_task_arg_' -or $text -match 'xvalue\*\s+__xl6_call_arg_'){ exit 1 }"
if errorlevel 1 (
    echo cgen_task_spawn_fstring.c missing nested ordinary/task.spawn replacement composition.
    exit /b 1
)

echo DEMO6 suite ok: task spawn range and f-string expressions
exit /b 0
