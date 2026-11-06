@echo off
setlocal
for %%P in ("%~dp0.") do set "XLANG_UI_RUNTIME=%%~fP"
:find_runtime
if exist "%XLANG_UI_RUNTIME%\xl.exe" goto run_gallery
if exist "%XLANG_UI_RUNTIME%\release\win_x64\xl.exe" (
    set "XLANG_UI_RUNTIME=%XLANG_UI_RUNTIME%\release\win_x64"
    goto run_gallery
)
for %%P in ("%XLANG_UI_RUNTIME%\..") do set "XLANG_UI_PARENT=%%~fP"
if /I "%XLANG_UI_PARENT%"=="%XLANG_UI_RUNTIME%" (
    echo Cannot find the matched xlang Windows runtime.
    exit /b 2
)
set "XLANG_UI_RUNTIME=%XLANG_UI_PARENT%"
goto find_runtime
:run_gallery
if not defined XLANG_PACKAGE_PATH set "XLANG_PACKAGE_PATH=%XLANG_UI_RUNTIME%\extlibs"
pushd "%XLANG_UI_RUNTIME%"
"%XLANG_UI_RUNTIME%\xl.exe" run "%~dp0all_controls.xl"
set "XLANG_UI_EXIT=%ERRORLEVEL%"
popd
exit /b %XLANG_UI_EXIT%
