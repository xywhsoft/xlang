@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: task spawn prepare and execution soak...

release\xl.exe check test\cases\cgen_task_spawn.xl
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_task_spawn.xl --emit build\test\cgen_task_spawn.c

release\xl.exe build test\cases\cgen_task_spawn.xl -o build\test\cgen_task_spawn.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$exe=(Resolve-Path 'build\test\cgen_task_spawn.exe').Path; for($i=1;$i -le 4;$i++){ $psi=[System.Diagnostics.ProcessStartInfo]::new(); $psi.FileName=$exe; $psi.WorkingDirectory=(Get-Location).Path; $psi.UseShellExecute=$false; $p=[System.Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(120000)){ try{$p.Kill()}catch{}; Write-Host ('cgen_task_spawn.exe run '+$i+' timed out.'); exit 124 }; $code=$p.ExitCode; $p.Dispose(); if($code -ne 0){ Write-Host ('cgen_task_spawn.exe run '+$i+' failed.'); exit $code } }"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: task spawn prepare and execution soak
exit /b 0

