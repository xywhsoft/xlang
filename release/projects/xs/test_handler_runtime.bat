@echo off
setlocal
cd /d "%~dp0\..\.."

xl.exe run projects\xs\src\handler_runtime_test.xl
if errorlevel 1 exit /b 1

xl.exe run projects\xs\src\handler_fault_test.xl
if errorlevel 1 exit /b 1

xl.exe run projects\xs\tests\handler_runtime_concurrency.c projects\xs\src\handler_runtime.c projects\xs\src\handler_generation.c --inc-path projects\xs\include --inc-path projects\xs\src
if errorlevel 1 exit /b 1

xl.exe run projects\xs\tests\module_lifecycle.c --inc-path include
if errorlevel 1 exit /b 1

echo xs2 handler runtime suite ok
exit /b 0
