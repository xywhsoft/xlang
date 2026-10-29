@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: XRT async ownership contract...

"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% test\cases\xrt_promise_owned_contract.c -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\xrt_promise_owned_contract.exe
if errorlevel 1 exit /b 1
build\test\xrt_promise_owned_contract.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: XRT async ownership contract
exit /b 0
