@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: semantic negatives exceptions sync thread async...

call "%~dp005a_semantic_negatives.bat"
if errorlevel 1 exit /b 1

call "%~dp005b_exceptions_sync.bat"
if errorlevel 1 exit /b 1

call "%~dp005c_thread_semantics.bat"
if errorlevel 1 exit /b 1

call "%~dp005d_async_prepare.bat"
if errorlevel 1 exit /b 1
set "XL6_ASYNC_CGEN_READY=1"

for %%S in (1 2 3 4) do (
    call "%~dp005e_async_assert_%%S.bat"
    if errorlevel 1 exit /b 1
)

echo DEMO6 suite ok: semantic negatives exceptions sync thread async
exit /b 0

