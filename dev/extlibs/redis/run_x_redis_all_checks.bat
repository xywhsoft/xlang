@echo off
setlocal

pushd "%~dp0..\..\.." >nul
if errorlevel 1 exit /b 1

node dev\extlibs\redis\check_x_redis_exports.js
if errorlevel 1 (
	popd >nul
	exit /b 1
)

pushd demo5 >nul
if errorlevel 1 (
	popd >nul
	exit /b 1
)

release\xl.exe build release\examples\redis\redis_basic.xl --emit release\tmp\redis_basic.exe
if errorlevel 1 (
	popd >nul
	popd >nul
	exit /b 1
)

set "OUT=%TEMP%\x_redis_offline_%RANDOM%_%RANDOM%.log"
release\xl.exe run release\examples\redis\redis_offline_codec.xl > "%OUT%" 2>&1
set "RC=%ERRORLEVEL%"
type "%OUT%"
for /f "tokens=3 delims== " %%A in ('findstr /c:"RunResult exitCode=" "%OUT%"') do set "RC=%%A"
del "%OUT%" >nul 2>nul
if not "%RC%"=="0" (
	popd >nul
	popd >nul
	exit /b %RC%
)

popd >nul

demo5\release\xl5_test.exe runtime.redis_offline_codec
if errorlevel 1 (
	popd >nul
	exit /b 1
)

cmd /c dev\extlibs\redis\run_x_redis_fake_smoke.bat
set "RC=%ERRORLEVEL%"
popd >nul
exit /b %RC%
