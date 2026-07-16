@echo off
setlocal

call "%~dp0run_fast.bat" || exit /b 1
call "%~dp0run_integration.bat" || exit /b 1
call "%~dp0run_stress.bat" || exit /b 1
call "%~dp0run_soak.bat" || exit /b 1
call "%~dp0run_memcheck.bat" || exit /b 1

echo xs2 all suites ok
exit /b 0
