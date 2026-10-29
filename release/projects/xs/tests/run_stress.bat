@echo off
setlocal
cd /d "%~dp0\..\..\.."

xl.exe run projects\xs\tests\handler_runtime_concurrency.c projects\xs\src\handler_runtime.c projects\xs\src\handler_generation.c --inc-path projects\xs\include --inc-path projects\xs\src || exit /b 1
xl.exe run projects\xs\src\http_stress_test.xl || exit /b 1
xl.exe run projects\xs\src\socket_service_test.xl || exit /b 1

echo xs2 stress suite ok
exit /b 0
