@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: future task wait...

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

release\xl.exe check test\cases\stdlib_future_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_future_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_future_metadata.xl -o build\test\stdlib_future_metadata.exe
if errorlevel 1 exit /b 1
build\test\stdlib_future_metadata.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_task_group.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_task_group.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_task_group.xl -o build\test\stdlib_task_group.exe
if errorlevel 1 exit /b 1
build\test\stdlib_task_group.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_future_continuation.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\stdlib_future_continuation.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\stdlib_future_continuation.xl -o build\test\stdlib_future_continuation.exe
if errorlevel 1 exit /b 1
build\test\stdlib_future_continuation.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\stdlib_future_continuation.xl --emit build\test\stdlib_future_continuation.c
if errorlevel 1 exit /b 1

for %%S in (xFutureThenSourceEx xFutureCatchSourceEx xFutureFinallyResultSourceEx) do (
    findstr /C:"%%S" build\test\stdlib_future_continuation.c >nul
    if errorlevel 1 (
        echo stdlib_future_continuation.c missing %%S runtime bridge.
        exit /b 1
    )
)

release\xl.exe check test\cases\sem_future_then_arg_count.xl > build\test\sem_future_then_arg_count.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future method argument count mismatch" build\test\sem_future_then_arg_count.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_then_param_type.xl > build\test\sem_future_then_param_type.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.then named callback must have one matching parameter and one return value" build\test\sem_future_then_param_type.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_then_no_return.xl > build\test\sem_future_then_no_return.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.then named callback must have one matching parameter and one return value" build\test\sem_future_then_no_return.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_then_multi_return.xl > build\test\sem_future_then_multi_return.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.then named callback must have one matching parameter and one return value" build\test\sem_future_then_multi_return.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_catch_signature.xl > build\test\sem_future_catch_signature.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.catch named callback must match func(error) as T" build\test\sem_future_catch_signature.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_finally_param.xl > build\test\sem_future_finally_param.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.finally named callback must match func()" build\test\sem_future_finally_param.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_finally_return.xl > build\test\sem_future_finally_return.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future.finally named callback must match func()" build\test\sem_future_finally_return.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_future_continuation_named_argument.xl > build\test\sem_future_continuation_named_argument.err.txt 2>&1
if not errorlevel 1 exit /b 1
findstr /C:"future method does not accept named arguments" build\test\sem_future_continuation_named_argument.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\stdlib_future.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\stdlib_future_xlang.out.txt
if errorlevel 1 exit /b 1
"%XL6_TCC%" -std=c11 -Wall -Ilib\xrt %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\examples\stdlib_future_c.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\stdlib_future_c.exe
if errorlevel 1 exit /b 1
build\test\stdlib_future_c.exe > build\test\stdlib_future_c.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -Command "$a=[System.IO.File]::ReadAllLines('build\test\stdlib_future_xlang.out.txt'); $b=[System.IO.File]::ReadAllLines('build\test\stdlib_future_c.out.txt'); if (($a -join \"`n\") -ne ($b -join \"`n\")) { Write-Host 'stdlib future xlang/C output mismatch.'; Write-Host 'xlang:'; $a; Write-Host 'C:'; $b; exit 1 }"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: future task wait
exit /b 0
