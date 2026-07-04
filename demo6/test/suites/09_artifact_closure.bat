@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: artifact closure checks...

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_negative_diagnostics.ps1 -Artifacts
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_tmp_artifacts.ps1 -MinTextArtifacts 541
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: artifact closure checks
exit /b 0

