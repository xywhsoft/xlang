@echo off
setlocal

if not "%XL_REDIS_REAL%"=="1" (
	echo Skipping x.redis real smoke. Set XL_REDIS_REAL=1 to run against Redis.
	exit /b 0
)

pushd "%~dp0..\..\..\demo5" >nul
set "OUT=%TEMP%\x_redis_smoke_%RANDOM%_%RANDOM%.log"
release\xl.exe run release\examples\redis\redis_basic.xl > "%OUT%" 2>&1
set RC=%ERRORLEVEL%
type "%OUT%"
for /f "tokens=3 delims== " %%A in ('findstr /c:"RunResult exitCode=" "%OUT%"') do set "RC=%%A"
del "%OUT%" >nul 2>nul
popd >nul
exit /b %RC%
