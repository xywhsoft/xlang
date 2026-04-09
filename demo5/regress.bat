@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d %~dp0

set TRACE_DIR=release\trace\tests
set LOG_DIR=%TRACE_DIR%\logs
set DIFF_DIR=%TRACE_DIR%\diff
set FAILED=0

call build_gcc.bat
if errorlevel 1 exit /b 1

if not exist "%TRACE_DIR%" mkdir "%TRACE_DIR%"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
if not exist "%DIFF_DIR%" mkdir "%DIFF_DIR%"

call :run_case all "" "test\cases\all\baseline.txt"
call :run_case lexer lexer "test\cases\lexer\baseline.txt"
call :run_case parser parser "test\cases\parser\baseline.txt"
call :run_case sem sem "test\cases\sem\baseline.txt"
call :run_case lower lower "test\cases\lower\baseline.txt"
call :run_case cgen cgen "test\cases\codegen\baseline.txt"
call :run_case runtime runtime "test\cases\runtime\baseline.txt"

if not "%FAILED%"=="0" (
	echo Regression failed. See "%TRACE_DIR%".
	exit /b 1
)

echo Regression passed. See "%TRACE_DIR%".
exit /b 0

:run_case
set NAME=%~1
set SELECTOR=%~2
set BASELINE=%~3
set OUT_FILE=%LOG_DIR%\%NAME%.txt
set DIFF_FILE=%DIFF_DIR%\%NAME%.txt

if "%SELECTOR%"=="" (
	release\bin\xl5.exe test > "%OUT_FILE%"
) else (
	release\bin\xl5_test.exe %SELECTOR% > "%OUT_FILE%"
)
if errorlevel 1 (
	echo [FAIL] %NAME% execution
	set FAILED=1
	exit /b 0
)

fc /n "%BASELINE%" "%OUT_FILE%" > "%DIFF_FILE%"
if errorlevel 1 (
	echo [FAIL] %NAME% baseline mismatch
	set FAILED=1
	exit /b 0
)

echo [PASS] %NAME%
del /q "%DIFF_FILE%" >nul 2>nul
exit /b 0
