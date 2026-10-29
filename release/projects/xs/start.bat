@echo off
setlocal

set "XS_DIR=%~dp0"
set "XS_EXE=%XS_DIR%xs.exe"

if not exist "%XS_EXE%" (
    echo xs.exe is missing. Build it with: xl build projects\xs\xs.xl -o projects\xs\xs.exe
    exit /b 1
)

"%XS_EXE%" %*
exit /b %errorlevel%
