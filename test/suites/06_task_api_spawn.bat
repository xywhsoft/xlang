@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: task API and task spawn...

call "%~dp006a_future_task_wait.bat"
if errorlevel 1 exit /b 1

call "%~dp006b_task_api_prepare.bat"
if errorlevel 1 exit /b 1
set "XL6_TASK_API_CGEN_READY=1"
for %%S in (1 2 3 4) do (
    call "%~dp006c_task_api_assert_%%S.bat"
    if errorlevel 1 exit /b 1
)

call "%~dp006d_task_spawn_expressions.bat"
if errorlevel 1 exit /b 1

call "%~dp006e_task_spawn_prepare.bat"
if errorlevel 1 exit /b 1
set "XL6_TASK_SPAWN_CGEN_READY=1"
for %%S in (1 2 3 4) do (
    call "%~dp006f_task_spawn_assert_%%S.bat"
    if errorlevel 1 exit /b 1
)

echo DEMO6 suite ok: task API and task spawn
exit /b 0

