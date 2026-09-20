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

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_standalone_compiler_vfs.ps1
if errorlevel 1 exit /b 1

rem HTTP/WebSocket non-fast applications are intentionally deferred to their later design review.
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_dev_projects.ps1 -Mode check -KeepGoing -ExcludeProjects "chatroom"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_generated_c_warnings.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_closure_basic_runtime.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xrt_product_trim.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xruntime_language_bridge.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen release\examples\hello.xl --emit build\test\trim_hello.c
if errorlevel 1 exit /b 1
findstr /C:"DynamicFields" build\test\trim_hello.c >nul
if not errorlevel 1 (
    echo trim_hello.c unexpectedly contains the dynamic-fields object model.
    exit /b 1
)

release\xl.exe build release\examples\hello.xl -o build\test\trim_hello.exe
if errorlevel 1 exit /b 1
build\test\trim_hello.exe
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_crypto.xl -o build\test\trim_stdlib_crypto.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_crypto.exe > build\test\trim_stdlib_crypto.out
if errorlevel 1 exit /b 1
findstr /C:"signature_guard=ok" build\test\trim_stdlib_crypto.out >nul
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

release\xl.exe build test\cases\stdlib_tcc_embed.xl -o build\test\trim_stdlib_tcc_embed.exe
if errorlevel 1 exit /b 1
build\test\trim_stdlib_tcc_embed.exe > build\test\trim_stdlib_tcc_embed.out
if errorlevel 1 exit /b 1
findstr /C:"stdlib tcc embed ok" build\test\trim_stdlib_tcc_embed.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\script_embed_xlang.xl -o build\test\trim_script_embed_xlang.exe
if errorlevel 1 exit /b 1
build\test\trim_script_embed_xlang.exe > build\test\trim_script_embed_xlang.out
if errorlevel 1 exit /b 1
findstr /C:"hello from nested xlang" build\test\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1
findstr /C:"script_embed_xlang=ok" build\test\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build test\fixtures\win_resource\main.xl --res test\fixtures\win_resource\app.res.json -o build\test\win_resource_cli.exe
if errorlevel 1 exit /b 1
build\test\win_resource_cli.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$v=[System.Diagnostics.FileVersionInfo]::GetVersionInfo('build\test\win_resource_cli.exe'); if($v.FileVersion -notlike '1.2.3.4*'){ Write-Host ('win_resource_cli.exe FileVersion mismatch: '+$v.FileVersion); exit 1 }; if($v.ProductName -ne 'xlang resource smoke'){ Write-Host ('win_resource_cli.exe ProductName mismatch: '+$v.ProductName); exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe build test\fixtures\win_resource\app.xlp -o build\test\win_resource_xlp.exe
if errorlevel 1 exit /b 1
build\test\win_resource_xlp.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$v=[System.Diagnostics.FileVersionInfo]::GetVersionInfo('build\test\win_resource_xlp.exe'); if($v.FileVersion -notlike '1.2.3.4*'){ Write-Host ('win_resource_xlp.exe FileVersion mismatch: '+$v.FileVersion); exit 1 }; if($v.ProductName -ne 'xlang resource smoke'){ Write-Host ('win_resource_xlp.exe ProductName mismatch: '+$v.ProductName); exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$left=@(Get-ChildItem -LiteralPath 'build\test' -Filter 'resource_*.obj' -File -Recurse -ErrorAction SilentlyContinue); if($left.Count -ne 0){$left | ForEach-Object { Write-Host ('unexpected resource object on disk: '+$_.FullName) }; exit 1}"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$items=@(Get-ChildItem -LiteralPath 'release\tmp' -Force); if($items.Count -ne 0){$items | ForEach-Object { Write-Host ('unexpected release/tmp artifact: ' + $_.FullName) }; exit 1}"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: artifact closure checks
exit /b 0
