@echo off
setlocal
cd /d %~dp0

call test.bat
if errorlevel 1 exit /b 1

call test_tutorial.bat
if errorlevel 1 exit /b 1

call test_apps.bat
if errorlevel 1 exit /b 1

call test_dll.bat
if errorlevel 1 exit /b 1

exit /b 0
