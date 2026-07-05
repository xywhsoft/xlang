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

release\xl.exe build release\examples\hello.xl -o release\tmp\trim_hello.exe
if errorlevel 1 exit /b 1
release\tmp\trim_hello.exe
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_crypto.xl -o release\tmp\trim_stdlib_crypto.exe
if errorlevel 1 exit /b 1
release\tmp\trim_stdlib_crypto.exe > release\tmp\trim_stdlib_crypto.out
if errorlevel 1 exit /b 1
findstr /C:"rsa_guard=ok" release\tmp\trim_stdlib_crypto.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_regex.xl -o release\tmp\trim_stdlib_regex.exe
if errorlevel 1 exit /b 1
release\tmp\trim_stdlib_regex.exe > release\tmp\trim_stdlib_regex.out
if errorlevel 1 exit /b 1
findstr /C:"set=ok" release\tmp\trim_stdlib_regex.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_json.xl -o release\tmp\trim_stdlib_json.exe
if errorlevel 1 exit /b 1
release\tmp\trim_stdlib_json.exe > release\tmp\trim_stdlib_json.out
if errorlevel 1 exit /b 1
findstr /C:"json=ok" release\tmp\trim_stdlib_json.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\stdlib_path_file.xl -o release\tmp\trim_stdlib_path_file.exe
if errorlevel 1 exit /b 1
release\tmp\trim_stdlib_path_file.exe > release\tmp\trim_stdlib_path_file.out
if errorlevel 1 exit /b 1
findstr /C:"dir=ok" release\tmp\trim_stdlib_path_file.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\script_embed_tcc.xl -o release\tmp\trim_script_embed_tcc.exe
if errorlevel 1 exit /b 1
release\tmp\trim_script_embed_tcc.exe > release\tmp\trim_script_embed_tcc.out
if errorlevel 1 exit /b 1
findstr /C:"hello from embedded C" release\tmp\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1
findstr /C:"hello from dynamic VFS" release\tmp\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1
findstr /C:"script_embed_tcc=ok" release\tmp\trim_script_embed_tcc.out >nul
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\script_embed_xlang.xl -o release\tmp\trim_script_embed_xlang.exe
if errorlevel 1 exit /b 1
release\tmp\trim_script_embed_xlang.exe > release\tmp\trim_script_embed_xlang.out
if errorlevel 1 exit /b 1
findstr /C:"hello from nested xlang" release\tmp\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1
findstr /C:"script_embed_xlang=ok" release\tmp\trim_script_embed_xlang.out >nul
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: artifact closure checks
exit /b 0
