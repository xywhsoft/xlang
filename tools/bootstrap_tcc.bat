@echo off
setlocal
cd /d %~dp0\..

set XL6_ROOT=%CD%
call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

if not exist build mkdir build
if not exist build\tmp mkdir build\tmp

if exist "%XL6_TCC%" (
    exit /b 0
)

for %%I in ("%XL6_TCC%") do set "XL6_TCC_ABSOLUTE=%%~fI"
for %%I in ("%XL6_ROOT%\build\tmp\tcc_bootstrap.exe") do set "XL6_TCC_DEFAULT=%%~fI"
if /I not "%XL6_TCC_ABSOLUTE%"=="%XL6_TCC_DEFAULT%" (
    echo DEMO6 TCC bootstrap failed: custom XL6_TCC does not exist: "%XL6_TCC%".
    exit /b 1
)
set "XL6_TCC=%XL6_TCC_ABSOLUTE%"

if exist "%XL6_BOOTSTRAP_CC%" goto XL6_BOOTSTRAP_CC_OK
where %XL6_BOOTSTRAP_CC% >nul 2>nul
if errorlevel 1 (
    echo DEMO6 TCC bootstrap failed: MinGW compiler "%XL6_BOOTSTRAP_CC%" was not found in PATH.
    echo Please install MinGW or set XL6_BOOTSTRAP_CC to a working C compiler.
    exit /b 1
)
:XL6_BOOTSTRAP_CC_OK

echo Bootstrapping DEMO6 local TCC...
"%XL6_BOOTSTRAP_CC%" -std=c11 -O2 -D_CRT_SECURE_NO_WARNINGS -Ilib\tcc -Ilib\lzma -o "%XL6_TCC%" ^
    lib\tcc\tcc.c ^
    lib\tcc\tcc_builtin_vfs.c ^
    lib\tcc\tcc_utf8_io.c ^
    lib\lzma\LzmaDec.c ^
    tools\tcc_empty_builtin_resources.c ^
    -lws2_32

if errorlevel 1 (
    echo DEMO6 TCC bootstrap failed.
    exit /b 1
)

exit /b 0
