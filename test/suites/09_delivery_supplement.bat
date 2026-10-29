@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

set "DELIVERY_OUT=build\test\delivery_%RANDOM%_%RANDOM%"
python test\test_delivery_case_matrix.py
if errorlevel 1 exit /b 1
python test\test_wiring_python_literals.py
if errorlevel 1 exit /b 1
python test\test_compiler_response.py
if errorlevel 1 exit /b 1
python test\test_delivery_native_fixtures.py
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_wiring_script_references.ps1
if errorlevel 1 exit /b 1
python tools\check_delivery_case_matrix.py --root . --out "%DELIVERY_OUT%\cases"
if errorlevel 1 exit /b 1
python tools\check_xrt_exports.py --root . --out "%DELIVERY_OUT%\exports"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_c_tool_selection.ps1 -Directory "%DELIVERY_OUT%\c-tools"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_companions.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_codec_dll_file.ps1
if errorlevel 1 exit /b 1

set "DELIVERY_PUBLIC=%XL6_REGRESSION_PUBLIC_DIRECTORY%"
if not defined DELIVERY_PUBLIC (
    set "DELIVERY_PUBLIC=%DELIVERY_OUT%\public"
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_xlir_public_source_driver.ps1 -Directory "%DELIVERY_OUT%\public" -Optimization O2 -RuntimeResource build\tmp\tcc_builtin_resources_runtime.c
    if errorlevel 1 exit /b 1
)
python tools\xlir_public_build_inputs.py --root . --public "%DELIVERY_PUBLIC%" --out "%DELIVERY_OUT%\source-receipt.json"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\stage_xlir_artifact_runtime_sources.ps1 -Directory "%DELIVERY_PUBLIC%"
if errorlevel 1 exit /b 1
python tools\check_delivery_native_fixtures.py --root . --public "%DELIVERY_PUBLIC%" --out "%DELIVERY_OUT%\native-fixtures"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_c_owned.ps1 -PublicDirectory "%DELIVERY_PUBLIC%" -Directory "%DELIVERY_OUT%\c-owned"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_native_contract.ps1 -PublicDirectory "%DELIVERY_PUBLIC%" -Directory "%DELIVERY_OUT%\native-contract"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_protocol_abi.ps1 -PublicSourceDirectory "%DELIVERY_PUBLIC%" -WorkDirectory "%DELIVERY_OUT%\protocol-abi"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_color.ps1 -PublicSourceDirectory "%DELIVERY_PUBLIC%" -WorkDirectory "%DELIVERY_OUT%\color"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_extlib_csv.ps1 -PublicSourceDirectory "%DELIVERY_PUBLIC%" -WorkDirectory "%DELIVERY_OUT%\csv"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_identity_fields.ps1 -PublicDirectory "%DELIVERY_PUBLIC%" -Directory "%DELIVERY_OUT%\identity"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_foreign_bridges.ps1 -PublicDriver "%DELIVERY_PUBLIC%\public_source.exe" -XrtObject "%DELIVERY_PUBLIC%\xrt_single_header_impl.o" -Directory "%DELIVERY_OUT%\foreign"
if errorlevel 1 exit /b 1

echo DEMO6 supplemental delivery regression checks passed.
exit /b 0
