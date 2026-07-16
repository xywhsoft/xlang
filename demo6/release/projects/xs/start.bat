@echo off
setlocal

set "XS_DIR=%~dp0"
set "XL_EXE=%XS_DIR%..\..\xl.exe"
if not exist "%XL_EXE%" set "XL_EXE=xl"

"%XL_EXE%" run "%XS_DIR%xs.xl" -- %*
exit /b %errorlevel%
