@echo off
setlocal
cd /d %~dp0\..\..\..

rem Wires: release/examples/xge/*.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_xge_examples.ps1
exit /b %errorlevel%
