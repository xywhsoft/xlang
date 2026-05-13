@echo off
setlocal EnableExtensions

set ROOT=%~dp0..\..\..
cd /d "%ROOT%\demo5"

set XL=release\xldbg.exe
if not exist "%XL%" (
	echo Missing %XL%. Build demo5 first.
	exit /b 1
)

call :run release\examples\xle_official_native\use_x_uia_version.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_smoke.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_timeout.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_focused.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_tree.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_lifetime.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_shutdown.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_snapshot.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_patterns.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_events.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_event_callback.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_cached.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_locator.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_ui_auto_smoke.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_test_window_value.xl || exit /b 1
call :run release\examples\xle_official_native\use_x_uia_test_window_click.xl || exit /b 1

echo x.uia smoke passed.
exit /b 0

:run
echo [x.uia] %~1
set OUT=release\tmp\x_uia_smoke_%RANDOM%_%RANDOM%.txt
"%XL%" run "%~1" > "%OUT%" 2>&1
type "%OUT%"
if errorlevel 1 (
	echo [x.uia][FAIL] %~1
	del /f /q "%OUT%" >nul 2>nul
	exit /b 1
)
findstr /C:"RunResult exitCode=0" "%OUT%" >nul
if errorlevel 1 (
	echo [x.uia][FAIL] %~1 did not return RunResult exitCode=0
	del /f /q "%OUT%" >nul 2>nul
	exit /b 1
)
del /f /q "%OUT%" >nul 2>nul
exit /b 0
