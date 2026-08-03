@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: static guards and CLI smoke...

echo Running DEMO6 smoke tests...

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_docs_index.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cgenrule_docs.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_risk_ledger.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_markdown_links.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_tool_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_batch_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cli_temp_cleanup.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_source_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_typed_hir_contract.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_source_encoding.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_frontend_fuzz.ps1 -Iterations 10000
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "$dir = Join-Path 'src' 'cgen'; $m = Get-ChildItem -LiteralPath $dir -Filter '*.c' | Select-String -SimpleMatch 'code generation error'; if ($m) { Write-Host 'src cgen contains generic cgen diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "$dir = Join-Path 'src' 'cgen'; $m = Get-ChildItem -LiteralPath $dir -Filter '*.c' | Select-String -SimpleMatch '__xl6_conv_'; if ($m) { Write-Host 'src cgen contains stale __xl6_conv_ fallback.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "$dir = Join-Path 'src' 'cgen'; $m = Get-ChildItem -LiteralPath $dir -Filter '*.c' | Select-String -SimpleMatch '({ XL6_Function'; if ($m) { Write-Host 'src cgen contains stale XL6_Function statement-expression fallback.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "$dir = Join-Path 'src' 'cgen'; $allowed = @('xl6_cgen_async_runtime.c', 'xl6_cgen_async_runtime_await.c', 'xl6_cgen_async_runtime_prelude.c'); $files = Get-ChildItem -LiteralPath $dir -Filter '*.c' | Where-Object { $allowed -notcontains $_.Name }; $m = $files | Select-String -SimpleMatch 'xFutureRequestCancel'; if ($m) { Write-Host 'src cgen emits direct future cancel outside async runtime helpers.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cgen_statement_expr_allowlist.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cgen_module_size.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_toolchain_paths.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_tcc_xrt_contract.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_c_string_safety.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_dll_api_boundary.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_test_case_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_test_data_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_example_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_xlang_output_markers.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_c_fixture_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_release_extlibs.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_xrt_symbols.ps1
if errorlevel 1 exit /b 1

call release\examples\extlibs\run_extlibs.bat
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_coverage.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_test_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_dll_symbols.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_negative_diagnostics.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_async_task_soak.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_examples.ps1
if errorlevel 1 exit /b 1

call release\examples\apps\run_apps.bat
if errorlevel 1 exit /b 1

call release\examples\feature_suite\run_feature_suite.bat
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\concurrency.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1
for /L %%I in (1,1,2) do (
    release\xl.exe run release\examples\concurrency.xl %XL6_XL_DEFAULT_LINK_ARGS%
    if errorlevel 1 (
        echo release\examples\concurrency.xl repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe run release\examples\task_api.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1
for /L %%I in (1,1,2) do (
    release\xl.exe run release\examples\task_api.xl %XL6_XL_DEFAULT_LINK_ARGS%
    if errorlevel 1 (
        echo release\examples\task_api.xl repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe run release\examples\async_task_wait.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\async_generator.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1
for /L %%I in (1,1,3) do (
    release\xl.exe run release\examples\async_generator.xl %XL6_XL_DEFAULT_LINK_ARGS%
    if errorlevel 1 (
        echo release\examples\async_generator.xl repeat run %%I failed.
        exit /b 1
    )
)

release\xl.exe run release\examples\union.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

cmd /c "set XL6_TCC=__missing_tcc__.exe&& release\xl.exe run release\examples\stdlib_print.xl > build\test\embedded_tcc_run.out.txt"
if errorlevel 1 (
    echo embedded libtcc run failed when external tcc is missing.
    exit /b 1
)
findstr /C:"xlang print 123 done" build\test\embedded_tcc_run.out.txt >nul
if errorlevel 1 (
    echo embedded_tcc_run.out.txt missing print output.
    exit /b 1
)
cmd /c "set XL6_TCC=__missing_tcc__.exe&& release\xl.exe build release\examples\stdlib_print.xl -o build\test\embedded_tcc_build.exe"
if errorlevel 1 (
    echo embedded libtcc build failed when external tcc is missing.
    exit /b 1
)
build\test\embedded_tcc_build.exe > build\test\embedded_tcc_build.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang print 123 done" build\test\embedded_tcc_build.out.txt >nul
if errorlevel 1 (
    echo embedded_tcc_build.out.txt missing print output.
    exit /b 1
)
cmd /c "set XL6_TCC=__missing_tcc__.exe&& set XL6_TCC_ROOT=__missing_root__&& set XL6_TCC_INCLUDE=__missing_include__&& set XL6_TCC_WIN_INCLUDE=__missing_win_include__&& set XL6_TCC_WINAPI_INCLUDE=__missing_winapi_include__&& set XL6_TCC_LIB=__missing_lib__&& set XL6_TCC_LIBTCC1=__missing_libtcc1.a&& set XL6_XRT_OBJ=__missing_xrt.o&& release\xl.exe run release\examples\stdlib_print.xl > build\test\embedded_vfs_run.out.txt"
if errorlevel 1 (
    echo embedded VFS run failed when external TCC resources are missing.
    exit /b 1
)
findstr /C:"xlang print 123 done" build\test\embedded_vfs_run.out.txt >nul
if errorlevel 1 (
    echo embedded_vfs_run.out.txt missing print output.
    exit /b 1
)
cmd /c "set XL6_TCC=__missing_tcc__.exe&& set XL6_TCC_ROOT=__missing_root__&& set XL6_TCC_INCLUDE=__missing_include__&& set XL6_TCC_WIN_INCLUDE=__missing_win_include__&& set XL6_TCC_WINAPI_INCLUDE=__missing_winapi_include__&& set XL6_TCC_LIB=__missing_lib__&& set XL6_TCC_LIBTCC1=__missing_libtcc1.a&& set XL6_XRT_OBJ=__missing_xrt.o&& release\xl.exe build release\examples\stdlib_print.xl -o build\test\embedded_vfs_build.exe"
if errorlevel 1 (
    echo embedded VFS build failed when external TCC resources are missing.
    exit /b 1
)
build\test\embedded_vfs_build.exe > build\test\embedded_vfs_build.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang print 123 done" build\test\embedded_vfs_build.out.txt >nul
if errorlevel 1 (
    echo embedded_vfs_build.out.txt missing print output.
    exit /b 1
)

if exist stdlib_print.exe del /Q stdlib_print.exe
release\xl.exe build release\examples\stdlib_print.xl
if errorlevel 1 (
    echo xl build default output for xlang failed.
    exit /b 1
)
if not exist stdlib_print.exe (
    echo xl build default output stdlib_print.exe missing.
    exit /b 1
)
stdlib_print.exe > build\test\default_xlang_build.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang print 123 done" build\test\default_xlang_build.out.txt >nul
if errorlevel 1 (
    echo default_xlang_build.out.txt missing print output.
    exit /b 1
)
del /Q stdlib_print.exe

powershell -NoProfile -ExecutionPolicy Bypass -Command "$utf8=New-Object System.Text.UTF8Encoding($false); [System.IO.File]::WriteAllText('build\test\default_c_build.c', '#include <stdio.h>' + [Environment]::NewLine + 'int main(void) { puts(\"c default build ok\"); return 0; }' + [Environment]::NewLine, $utf8)"
if errorlevel 1 exit /b 1
if exist default_c_build.exe del /Q default_c_build.exe
release\xl.exe build build\test\default_c_build.c
if errorlevel 1 (
    echo xl build default output for C failed.
    exit /b 1
)
if not exist default_c_build.exe (
    echo xl build default output default_c_build.exe missing.
    exit /b 1
)
default_c_build.exe > build\test\default_c_build.out.txt
if errorlevel 1 exit /b 1
findstr /C:"c default build ok" build\test\default_c_build.out.txt >nul
if errorlevel 1 (
    echo default_c_build.out.txt missing output.
    exit /b 1
)
del /Q default_c_build.exe

release\xl.exe build release\examples\stdlib_print.xl --shared >nul 2> build\test\default_shared_build.err.txt
if not errorlevel 1 (
    echo xl build --shared without -o should fail.
    exit /b 1
)
findstr /C:"build shared library requires -o" build\test\default_shared_build.err.txt >nul
if errorlevel 1 (
    echo default_shared_build.err.txt missing -o diagnostic.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$utf8=New-Object System.Text.UTF8Encoding($false); [System.IO.File]::WriteAllText('build\test\release_cwd_vfs.xl', 'print(\"release cwd vfs ok\")' + [Environment]::NewLine, $utf8)"
if errorlevel 1 exit /b 1
pushd release
xl.exe run ..\build\test\release_cwd_vfs.xl > ..\build\test\release_cwd_vfs.out.txt
set XL6_RELEASE_CWD_RC=%errorlevel%
popd
if not "%XL6_RELEASE_CWD_RC%"=="0" (
    echo embedded VFS run failed when current directory is release.
    exit /b 1
)
findstr /C:"release cwd vfs ok" build\test\release_cwd_vfs.out.txt >nul
if errorlevel 1 (
    echo release_cwd_vfs.out.txt missing print output.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$dir = Join-Path (Resolve-Path 'build\test') (([char]0x4E2D)+([char]0x6587)+'_embedded_tcc'); [System.IO.Directory]::CreateDirectory($dir) | Out-Null; $srcPath = Join-Path $dir (([char]0x811A)+([char]0x672C)+'.xl'); $exePath = Join-Path $dir (([char]0x7A0B)+([char]0x5E8F)+'.exe'); $outPath = Join-Path $dir 'out.txt'; $utf8 = New-Object System.Text.UTF8Encoding($false); $src = 'print(\"unicode embedded tcc ok\")' + [Environment]::NewLine; [System.IO.File]::WriteAllText($srcPath, $src, $utf8); & .\release\xl.exe build $srcPath -o $exePath; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; & $exePath | Out-File -Encoding utf8 $outPath; $text = [System.IO.File]::ReadAllText($outPath, [System.Text.Encoding]::UTF8); if ($text.IndexOf('unicode embedded tcc ok') -lt 0) { exit 2 }; exit 0"
if errorlevel 1 (
    echo embedded libtcc unicode path build failed.
    exit /b 1
)

release\xl.exe --help > build\test\help.stdout.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\help.stdout.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); if($text.IndexOf('Usage:') -lt 0 -or $text.IndexOf('xl build') -lt 0){ Write-Host 'help.stdout.txt missing UTF-8 CLI help output.'; exit 1 }"
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_debugger_not_released.ps1
if errorlevel 1 exit /b 1

release\xl.exe lex release\examples\hello.xl > build\test\hello.tokens.txt
if errorlevel 1 exit /b 1
release\xl.exe lex release\examples\hello.xl --emit build\test\hello.tokens.emit.txt
if errorlevel 1 exit /b 1
findstr /C:"IDENT" build\test\hello.tokens.emit.txt >nul
if errorlevel 1 (
    echo hello.tokens.emit.txt missing lexer emit output.
    exit /b 1
)
release\xl.exe lex release\examples\hello.xl --format json --emit build\test\hello.tokens.json
if errorlevel 1 exit /b 1
findstr /C:"KW_FUNC" build\test\hello.tokens.json >nul
if errorlevel 1 (
    echo hello.tokens.json missing JSON token.
    exit /b 1
)

release\xl.exe parse release\examples\hello.xl > build\test\hello.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe parse release\examples\hello.xl --emit build\test\hello.ast.emit.txt
if errorlevel 1 exit /b 1
findstr /C:"add" build\test\hello.ast.emit.txt >nul
if errorlevel 1 (
    echo hello.ast.emit.txt missing parse emit output.
    exit /b 1
)
release\xl.exe parse release\examples\hello.xl --format json --emit build\test\hello.ast.json
if errorlevel 1 exit /b 1
findstr /C:"kind" build\test\hello.ast.json >nul
if errorlevel 1 (
    echo hello.ast.json missing JSON kind field.
    exit /b 1
)
findstr /C:"add" build\test\hello.ast.json >nul
if errorlevel 1 (
    echo hello.ast.json missing function name.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$dir = Join-Path (Resolve-Path 'build\test') (([char]0x4E2D)+([char]0x6587)+'_path'); [System.IO.Directory]::CreateDirectory($dir) | Out-Null; $file = Join-Path $dir (([char]0x5165)+([char]0x53E3)+'.xl'); $out = Join-Path $dir 'out.ast.txt'; $utf8 = New-Object System.Text.UTF8Encoding($false); $src = 'func add(int a, int b) as int' + [Environment]::NewLine + '    return a + b' + [Environment]::NewLine + 'end' + [Environment]::NewLine; [System.IO.File]::WriteAllText($file, $src, $utf8); & .\release\xl.exe parse $file --emit $out; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $text = [System.IO.File]::ReadAllText($out, [System.Text.Encoding]::UTF8); if ($text -notmatch 'add') { exit 2 }; exit 0"
if errorlevel 1 (
    echo unicode path parse/emit smoke test failed.
    exit /b 1
)

release\xl.exe check release\examples\hello.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\hello.xl --json > build\test\hello.check.json
if errorlevel 1 exit /b 1
findstr /C:"diagnostics" build\test\hello.check.json >nul
if errorlevel 1 (
    echo hello.check.json missing diagnostics field.
    exit /b 1
)
findstr /C:"true" build\test\hello.check.json >nul
if errorlevel 1 (
    echo hello.check.json missing success flag.
    exit /b 1
)

release\xl.exe check test\cases\sem_diag_utf8.xl >nul 2> build\test\sem_diag_utf8.err.txt
if not errorlevel 1 (
    echo sem_diag_utf8.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\sem_diag_utf8.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected='return '+([char]0x8868)+([char]0x8FBE)+([char]0x5F0F)+([char]0x7C7B)+([char]0x578B)+([char]0x548C)+([char]0x51FD)+([char]0x6570)+([char]0x8FD4)+([char]0x56DE)+([char]0x7C7B)+([char]0x578B)+([char]0x4E0D)+([char]0x5339)+([char]0x914D); if($text.IndexOf($expected) -lt 0){ Write-Host 'sem_diag_utf8.err.txt missing UTF-8 semantic diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$name=([char]0x7F3A)+([char]0x5931)+'_'+([char]0x6587)+([char]0x4EF6)+'.xl'; $missing=Join-Path 'build\test' $name; $err='build\test\cli_lex_missing_utf8.err.txt'; $out='build\test\cli_lex_missing_utf8.out.txt'; $p=Start-Process -FilePath 'release\xl.exe' -ArgumentList @('lex',$missing) -NoNewWindow -PassThru -Wait -RedirectStandardOutput $out -RedirectStandardError $err; if($p.ExitCode -eq 0){ Write-Host 'lex missing unicode path should fail.'; exit 1 }; $bytes=[System.IO.File]::ReadAllBytes($err); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); if($text.IndexOf('cannot read file:') -lt 0 -or $text.IndexOf($name) -lt 0){ Write-Host 'cli_lex_missing_utf8.err.txt missing UTF-8 path diagnostic.'; exit 2 }"
if errorlevel 1 exit /b 1

release\xl.exe cgen release\examples\hello.xl --dump-dir build\test\dump_hello
if errorlevel 1 exit /b 1
findstr /C:"KW_FUNC" build\test\dump_hello\hello.tokens.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing lexer dump.
    exit /b 1
)
findstr /C:"kind" build\test\dump_hello\hello.ast.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing AST dump.
    exit /b 1
)
findstr /C:"func add" build\test\dump_hello\hello.xlir >nul
if errorlevel 1 (
    echo cgen dump-dir missing IR dump.
    exit /b 1
)
findstr /C:"abiVersion" build\test\dump_hello\hello.abi.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing ABI dump.
    exit /b 1
)
findstr /C:"Generated by xlang DEMO6 cgen." build\test\dump_hello\hello.c >nul
if errorlevel 1 (
    echo cgen dump-dir missing C dump.
    exit /b 1
)
findstr /C:"#ifndef XL6_HELLO_H" build\test\dump_hello\hello.h >nul
if errorlevel 1 (
    echo cgen dump-dir missing header dump.
    exit /b 1
)


echo DEMO6 suite ok: static guards and CLI smoke
exit /b 0

