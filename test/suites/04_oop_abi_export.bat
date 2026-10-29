@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: OOP ABI export and interop...

call "%~dp004_oop_base_01.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_02.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_03.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_04.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_05.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_06.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_07.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_base_08.bat"
if errorlevel 1 exit /b 1

call "%~dp004_oop_current_regressions.bat"
if errorlevel 1 exit /b 1

call "%~dp004_user_protocol.bat"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: OOP ABI export and interop
exit /b 0
