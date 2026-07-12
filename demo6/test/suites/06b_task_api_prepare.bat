@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task API prepare and execution...

release\xl.exe check test\cases\cgen_task_api.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_task_api.xl --emit build\test\cgen_task_api.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$size=(Get-Item 'build\test\cgen_task_api.c').Length; if($size -gt 8MB){ Write-Host ('cgen_task_api.c is too large: '+$size+' bytes'); exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_task_api.xl test\cases\cgen_class_host.c -o build\test\cgen_task_api.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$exe=(Resolve-Path 'build\test\cgen_task_api.exe').Path; for($i=1;$i -le 4;$i++){ $psi=[System.Diagnostics.ProcessStartInfo]::new(); $psi.FileName=$exe; $psi.WorkingDirectory=(Get-Location).Path; $psi.UseShellExecute=$false; $p=[System.Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(120000)){ try{$p.Kill()}catch{}; Write-Host ('cgen_task_api.exe run '+$i+' timed out.'); exit 124 }; $code=$p.ExitCode; $p.Dispose(); if($code -ne 0){ Write-Host ('cgen_task_api.exe run '+$i+' failed.'); exit $code } }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_error_cleanup_milestone.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_error_cleanup_milestone.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_error_cleanup_milestone.xl -o build\test\cgen_error_cleanup_milestone.exe
if errorlevel 1 exit /b 1
build\test\cgen_error_cleanup_milestone.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: task API prepare and execution
exit /b 0
