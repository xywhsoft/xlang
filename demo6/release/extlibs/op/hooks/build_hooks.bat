@echo off
REM Build hook DLLs using TCC
REM Run from demo6 directory: release\extlibs\op\hooks\build_hooks.bat

set TCC=%~dp0..\..\..\..\build\tmp\tcc_bootstrap.exe
set TCCRES=%~dp0..\..\..\..\res\tcc\win64\x86_64
set HOOKS_DIR=%~dp0

echo Building op_d3d9_hook.dll ...
"%TCC%" -B "%TCCRES%" -shared -o "%HOOKS_DIR%op_d3d9_hook.dll" "%HOOKS_DIR%op_d3d9_hook.c" -luser32 -lgdi32 -ld3d9
if %errorlevel% neq 0 (echo FAILED: op_d3d9_hook.dll & exit /b 1)

echo Building op_d3d11_hook.dll ...
"%TCC%" -B "%TCCRES%" -shared -o "%HOOKS_DIR%op_d3d11_hook.dll" "%HOOKS_DIR%op_d3d11_hook.c" -luser32 -lgdi32 -ld3d11 -ldxgi
if %errorlevel% neq 0 (echo FAILED: op_d3d11_hook.dll & exit /b 1)

echo Building op_wgl_hook.dll ...
"%TCC%" -B "%TCCRES%" -shared -o "%HOOKS_DIR%op_wgl_hook.dll" "%HOOKS_DIR%op_wgl_hook.c" -luser32 -lgdi32 -lopengl32
if %errorlevel% neq 0 (echo FAILED: op_wgl_hook.dll & exit /b 1)

echo All hook DLLs built successfully.
