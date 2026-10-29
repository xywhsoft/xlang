@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1

if not defined XL6_FOUNDATION_OUT set "XL6_FOUNDATION_OUT=build\test\foundation-%RANDOM%-%RANDOM%"
if not defined XL6_FOUNDATION_JOBS set "XL6_FOUNDATION_JOBS=2"
python tools\check_stdlib_foundation.py --root . --out "%XL6_FOUNDATION_OUT%" --jobs "%XL6_FOUNDATION_JOBS%"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: foundation contracts and examples
exit /b 0
