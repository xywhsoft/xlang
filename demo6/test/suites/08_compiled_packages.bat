@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: compiled-in package embedding...

set "XL6_PACKAGE_CONTRACT_DIR=build\test\compiled_packages_%RANDOM%_%RANDOM%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\build_xlir_public_source_driver.ps1 -Directory "%XL6_PACKAGE_CONTRACT_DIR%\compiler" -RuntimeResource build\tmp\tcc_builtin_resources_runtime.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_compiled_packages.ps1 -PublicDirectory "%XL6_PACKAGE_CONTRACT_DIR%\compiler" -RuntimeDirectory release -Directory "%XL6_PACKAGE_CONTRACT_DIR%\acceptance"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: compiled-in package embedding
exit /b 0
