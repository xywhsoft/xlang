@echo off
setlocal
cd /d %~dp0

call release\examples\apps\run_apps.bat
exit /b %errorlevel%
