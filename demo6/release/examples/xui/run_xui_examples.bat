@echo off
setlocal
cd /d %~dp0\..\..\..

rem Wires: release/examples/xui/*.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_xui_examples.ps1
exit /b %errorlevel%
