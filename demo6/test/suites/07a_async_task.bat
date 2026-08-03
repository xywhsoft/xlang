@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: async task and async-generator...

call "%~dp007a_async_xrt_contract.bat"
if errorlevel 1 exit /b 1

call "%~dp007a_async_part_a.bat"
if errorlevel 1 exit /b 1

call "%~dp007a_async_part_b.bat"
if errorlevel 1 exit /b 1

call "%~dp007a_async_part_c.bat"
if errorlevel 1 exit /b 1

call "%~dp007a_async_class_methods.bat"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: async task and async-generator
exit /b 0
