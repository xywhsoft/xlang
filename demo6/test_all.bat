@echo off
setlocal
cd /d %~dp0

call test.bat
if errorlevel 1 exit /b 1

call examples\feature_suite\run_feature_suite.bat
if errorlevel 1 exit /b 1

exit /b 0
