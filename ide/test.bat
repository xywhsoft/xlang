@echo off
setlocal

call "%~dp0build.bat"
if errorlevel 1 exit /b 1

"%~dp0release\xlang_ide.exe" --check-language-rules
if errorlevel 1 exit /b 1

"%~dp0release\xlang_ide.exe" --check-workspace
if errorlevel 1 exit /b 1

"%~dp0release\xlang_ide.exe" --check-workspace-file "%~dp0test\fixtures\workspace\Workspace1.xlw"
if errorlevel 1 exit /b 1

for %%L in (zh en ru ja fr es) do (
    "%~dp0release\xlang_ide.exe" --frames 1 --lang %%L
    if errorlevel 1 exit /b 1
)

exit /b 0
