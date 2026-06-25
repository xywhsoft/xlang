@echo off
setlocal
cd /d %~dp0

call build_dll.bat
if errorlevel 1 exit /b 1

release\xl_dll_null_context.exe
if errorlevel 1 exit /b 1

release\xl_dll_threaded_host.exe
if errorlevel 1 exit /b 1

release\xl_dll_embed_string.exe
if errorlevel 1 exit /b 1

release\tmp\xl_dll_embed_string_script.exe
if errorlevel 1 exit /b 1

release\tmp\xl_dll_embed_file_script.exe
if errorlevel 1 exit /b 1

if not exist release\tmp\xl_dll_link_host.o (
    echo xl_dll_link_host.o missing.
    exit /b 1
)
if not exist release\tmp\libxl6dlllink.a (
    echo libxl6dlllink.a missing.
    exit /b 1
)
if not exist release\tmp\xl_dll_loadlib_host.dll (
    echo xl_dll_loadlib_host.dll missing.
    exit /b 1
)
if not exist release\tmp\xl_dll_loadlib_host.def (
    echo xl_dll_loadlib_host.def missing.
    exit /b 1
)

echo DEMO6 xl.dll tests ok.
exit /b 0
