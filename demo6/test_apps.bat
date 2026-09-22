@echo off
setlocal
cd /d %~dp0

call release\examples\apps\run_apps.bat
if errorlevel 1 exit /b 1

call release\examples\games\run_games.bat
exit /b %errorlevel%
