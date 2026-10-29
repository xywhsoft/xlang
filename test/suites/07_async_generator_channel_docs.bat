@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async generator channel docs f-string negatives...

call "%~dp007a_async_task.bat"
if errorlevel 1 exit /b 1
call "%~dp007b_generator.bat"
if errorlevel 1 exit /b 1
call "%~dp007c_channel.bat"
if errorlevel 1 exit /b 1
call "%~dp007d_errors_docs_fstring.bat"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: async generator channel docs f-string negatives
exit /b 0

