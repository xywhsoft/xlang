@echo off
setlocal
cd /d %~dp0

call build_dll.bat
if errorlevel 1 exit /b 1

release\xl_dll_embed_string.exe
if errorlevel 1 exit /b 1

release\tmp\xl_dll_embed_string_script.exe
if errorlevel 1 exit /b 1

echo DEMO6 xl.dll tests ok.
exit /b 0
