@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: async class methods...

for %%F in (
    async_class_method
    async_class_receiver_lifetime
    async_generic_class_method
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\async_native_backed_class_method.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\async_native_backed_class_method.xl test\cases\cgen_native_class_host.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\async_native_backed_class_method.xl test\cases\cgen_native_class_host.c -o build\test\async_native_backed_class_method.exe
if errorlevel 1 exit /b 1
build\test\async_native_backed_class_method.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: async class methods
exit /b 0
