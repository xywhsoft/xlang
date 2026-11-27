@echo off
setlocal
cd /d %~dp0

call release\win_x64\examples\feature_suite\run_feature_suite.bat
exit /b %errorlevel%
