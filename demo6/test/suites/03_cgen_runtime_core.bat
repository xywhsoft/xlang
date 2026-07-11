@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime...

call "%~dp003_core_base_01.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_02.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_03.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_04.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_05.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_06.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_07.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_08.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_09.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_base_10.bat"
if errorlevel 1 exit /b 1

call "%~dp003_core_current_regressions.bat"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime
exit /b 0

