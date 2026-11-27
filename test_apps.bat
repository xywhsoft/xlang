@echo off
setlocal
cd /d %~dp0

call release\win_x64\examples\apps\run_apps.bat
if errorlevel 1 exit /b 1

call release\win_x64\examples\games\run_games.bat
exit /b %errorlevel%
