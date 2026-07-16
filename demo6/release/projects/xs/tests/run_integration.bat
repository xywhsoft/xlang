@echo off
setlocal
cd /d "%~dp0\..\..\.."

xl.exe run projects\xs\src\http_service_test.xl || exit /b 1
xl.exe run projects\xs\tests\xrt_owned_tls.c || exit /b 1
xl.exe run projects\xs\tests\xrt_owned_tls.xl || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File projects\xs\tests\check_http_cgen_abi.ps1 || exit /b 1
xl.exe run projects\xs\src\http_fault_test.xl || exit /b 1
xl.exe run projects\xs\src\socket_service_test.xl || exit /b 1
xl.exe run projects\xs\src\socket_framing_test.xl || exit /b 1
xl.exe run projects\xs\src\service_runtime_test.xl || exit /b 1
xl.exe run projects\xs\src\atomic_reload_test.xl || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File projects\xs\test_smoke.ps1 || exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File projects\xs\blog\tests\smoke.ps1 || exit /b 1

echo xs2 integration suite ok
exit /b 0
