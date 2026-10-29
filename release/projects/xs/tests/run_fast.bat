@echo off
setlocal
cd /d "%~dp0\..\..\.."

call projects\xs\start.bat --check || exit /b 1
xl.exe run projects\xs\src\config_test.xl || exit /b 1
xl.exe run projects\xs\src\config_reload_test.xl || exit /b 1
xl.exe run projects\xs\src\handler_runtime_test.xl || exit /b 1
xl.exe run projects\xs\src\handler_fault_test.xl || exit /b 1
xl.exe run projects\xs\src\logging_test.xl || exit /b 1
xl.exe run projects\xs\tests\module_lifecycle.c --inc-path include || exit /b 1
xl.exe run projects\xs\blog\tests\security_test.xl || exit /b 1
xl.exe run projects\xs\blog\tests\schema_test.xl || exit /b 1
xl.exe run projects\xs\blog\tests\repository_test.xl || exit /b 1
xl.exe run projects\xs\blog\tests\cookie_test.xl || exit /b 1
xl.exe run projects\xs\blog\tests\dynamic_cookie_test.xl || exit /b 1

echo xs2 fast suite ok
exit /b 0
