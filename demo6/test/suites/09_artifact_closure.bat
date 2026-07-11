@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: artifact closure checks...

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_negative_diagnostics.ps1 -Artifacts
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_tmp_artifacts.ps1 -MinTextArtifacts 541
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_tcc_export_environment.ps1
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\hello.xl -o build\test\trim_hello.exe
if errorlevel 1 exit /b 1
build\test\trim_hello.exe
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_crypto.xl -o build\test\trim_stdlib_crypto.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_crypto.exe > build\test\trim_stdlib_crypto.out
if errorlevel 1 exit /b 1
findstr /C:"rsa_guard=ok" build\test\trim_stdlib_crypto.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_regex.xl -o build\test\trim_stdlib_regex.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_regex.exe > build\test\trim_stdlib_regex.out
if errorlevel 1 exit /b 1
findstr /C:"set=ok" build\test\trim_stdlib_regex.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_json.xl -o build\test\trim_stdlib_json.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_json.exe > build\test\trim_stdlib_json.out
if errorlevel 1 exit /b 1
findstr /C:"json=ok" build\test\trim_stdlib_json.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_path_file.xl -o build\test\trim_stdlib_path_file.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_path_file.exe > build\test\trim_stdlib_path_file.out
if errorlevel 1 exit /b 1
findstr /C:"dir=ok" build\test\trim_stdlib_path_file.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\script_embed_tcc.xl -o build\test\trim_script_embed_tcc.exe
if errorlevel 1 exit /b 1
build\test\trim_script_embed_tcc.exe > build\test\trim_script_embed_tcc.out
if errorlevel 1 exit /b 1
findstr /C:"hello from embedded C" build\test\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1
findstr /C:"hello from dynamic VFS" build\test\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1
findstr /C:"script_embed_tcc=ok" build\test\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\script_embed_xlang.xl -o build\test\trim_script_embed_xlang.exe
if errorlevel 1 exit /b 1
build\test\trim_script_embed_xlang.exe > build\test\trim_script_embed_xlang.out
if errorlevel 1 exit /b 1
findstr /C:"hello from nested xlang" build\test\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1
findstr /C:"script_embed_xlang=ok" build\test\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$items=@(Get-ChildItem -LiteralPath 'release\tmp' -Force); if($items.Count -ne 0){$items | ForEach-Object { Write-Host ('unexpected release/tmp artifact: ' + $_.FullName) }; exit 1}"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: artifact closure checks
exit /b 0
