@echo off
setlocal
cd /d %~dp0

call build_gcc.bat
exit /b %errorlevel%
