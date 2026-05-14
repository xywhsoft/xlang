@echo off
setlocal

node "%~dp0fake_redis_smoke.js"
exit /b %ERRORLEVEL%
