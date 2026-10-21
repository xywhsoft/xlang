@echo off
python -X utf8 "%~dp0tools\package_current.py" %*
exit /b %errorlevel%
