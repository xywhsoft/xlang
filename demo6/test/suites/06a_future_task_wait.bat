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

echo DEMO6 suite ok: future task wait
exit /b 0

