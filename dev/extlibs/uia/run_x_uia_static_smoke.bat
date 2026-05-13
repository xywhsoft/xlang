@echo off
setlocal EnableExtensions

set ROOT=%~dp0..\..\..
cd /d "%ROOT%\demo5"

set XL=release\xldbg.exe
if not exist "%XL%" (
	echo Missing %XL%. Build demo5 first.
	exit /b 1
)

echo [x.uia][sem] release\examples\xle_official_native\use_x_uia_version.xl
"%XL%" sem release\examples\xle_official_native\use_x_uia_version.xl >nul
if errorlevel 1 (
	echo [x.uia][FAIL] sem use_x_uia_version.xl
	exit /b 1
)

echo [x.ui.auto][sem] release\examples\xle_official_native\use_x_ui_auto_smoke.xl
"%XL%" sem release\examples\xle_official_native\use_x_ui_auto_smoke.xl >nul
if errorlevel 1 (
	echo [x.ui.auto][FAIL] sem use_x_ui_auto_smoke.xl
	exit /b 1
)

echo [x.uia][sem] release\examples\xle_official_native\use_x_uia_event_callback.xl
"%XL%" sem release\examples\xle_official_native\use_x_uia_event_callback.xl >nul
if errorlevel 1 (
	echo [x.uia][FAIL] sem use_x_uia_event_callback.xl
	exit /b 1
)

echo [x.uia][run] release\examples\xle_official_native\use_x_uia_version.xl
set OUT=release\tmp\x_uia_static_smoke_%RANDOM%_%RANDOM%.txt
"%XL%" run release\examples\xle_official_native\use_x_uia_version.xl > "%OUT%" 2>&1
type "%OUT%"
if errorlevel 1 (
	echo [x.uia][FAIL] run use_x_uia_version.xl
	del /f /q "%OUT%" >nul 2>nul
	exit /b 1
)
findstr /C:"RunResult exitCode=0" "%OUT%" >nul
if errorlevel 1 (
	echo [x.uia][FAIL] use_x_uia_version.xl did not return RunResult exitCode=0
	del /f /q "%OUT%" >nul 2>nul
	exit /b 1
)
del /f /q "%OUT%" >nul 2>nul

echo x.uia static smoke passed.
exit /b 0
