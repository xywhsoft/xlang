@echo off
setlocal
cd /d %~dp0

call test.bat
exit /b %errorlevel%
