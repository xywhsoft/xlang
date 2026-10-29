@echo off
setlocal
cd /d "%~dp0"

set "XL=..\..\xl.exe"
if not exist "%XL%" (
    echo xl.exe not found: %XL%
    exit /b 1
)

if not exist build mkdir build
"%XL%" cgen callback_module.xl --emit build\callback_module.c --emit-header build\callback_module.h
if errorlevel 1 exit /b 1

"%XL%" build callback_module.xl host.c --inc-path build -o build\function_interop.exe --emit-header build\callback_module.h
if errorlevel 1 exit /b 1

build\function_interop.exe
exit /b %errorlevel%
