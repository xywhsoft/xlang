@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

python -B tools\test_tcc_vfs_output_directory.py
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_file_runtime.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_extlib_construction.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_audit.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_smoke_process.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_sqlite.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_helper_boundaries.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_sdk_features.ps1
if errorlevel 1 exit /b 1

set "XL6_NATIVE_CONTRACT_DIR=build\test\extlib_native_contract_%RANDOM%_%RANDOM%"
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_xlir_public_source_driver.ps1 -Directory "%XL6_NATIVE_CONTRACT_DIR%\compiler"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\stage_xlir_artifact_runtime_sources.ps1 -Directory "%XL6_NATIVE_CONTRACT_DIR%\compiler"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_native_contract.ps1 -PublicDirectory "%XL6_NATIVE_CONTRACT_DIR%\compiler" -Directory "%XL6_NATIVE_CONTRACT_DIR%\checks"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_gzip.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_chardet.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_graph.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_phase1_extlibs.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_mqtt.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_serial.ps1
if errorlevel 1 exit /b 1

exit /b 0
