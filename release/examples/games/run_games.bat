@echo off
setlocal
cd /d %~dp0\..\..\..

rem Wires: release/examples/games/*.xl
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_game_examples.ps1
exit /b %errorlevel%
