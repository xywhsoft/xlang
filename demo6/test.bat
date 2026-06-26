@echo off
setlocal
cd /d %~dp0

call tools\xl6_toolchain_env.bat
if errorlevel 1 exit /b 1

call build.bat
if errorlevel 1 exit /b 1

echo Running DEMO6 smoke tests...

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_docs_index.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_cgenrule_docs.ps1
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

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_source_encoding.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -Command "$dir = Join-Path 'src' 'cgen'; $m = Get-ChildItem -LiteralPath $dir -Filter '*.c' | Select-String -SimpleMatch 'code generation error'; if ($m) { Write-Host 'src cgen contains generic cgen diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_toolchain_paths.ps1
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

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_c_fixture_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_package_fixtures.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_coverage.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_test_wiring.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_dll_symbols.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_negative_diagnostics.ps1
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_stdlib_examples.ps1
if errorlevel 1 exit /b 1

call examples\feature_suite\run_feature_suite.bat
if errorlevel 1 exit /b 1

release\xl.exe run examples\concurrency.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe run examples\union.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe --help > release\tmp\help.stdout.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\help.stdout.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); if($text.IndexOf('Usage:') -lt 0 -or $text.IndexOf('xl build') -lt 0){ Write-Host 'help.stdout.txt missing UTF-8 CLI help output.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe lex examples\hello.xl > release\tmp\hello.tokens.txt
if errorlevel 1 exit /b 1
release\xl.exe lex examples\hello.xl --emit release\tmp\hello.tokens.emit.txt
if errorlevel 1 exit /b 1
findstr /C:"IDENT" release\tmp\hello.tokens.emit.txt >nul
if errorlevel 1 (
    echo hello.tokens.emit.txt missing lexer emit output.
    exit /b 1
)
release\xl.exe lex examples\hello.xl --format json --emit release\tmp\hello.tokens.json
if errorlevel 1 exit /b 1
findstr /C:"KW_FUNC" release\tmp\hello.tokens.json >nul
if errorlevel 1 (
    echo hello.tokens.json missing JSON token.
    exit /b 1
)

release\xl.exe parse examples\hello.xl > release\tmp\hello.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe parse examples\hello.xl --emit release\tmp\hello.ast.emit.txt
if errorlevel 1 exit /b 1
findstr /C:"add" release\tmp\hello.ast.emit.txt >nul
if errorlevel 1 (
    echo hello.ast.emit.txt missing parse emit output.
    exit /b 1
)
release\xl.exe parse examples\hello.xl --format json --emit release\tmp\hello.ast.json
if errorlevel 1 exit /b 1
findstr /C:"kind" release\tmp\hello.ast.json >nul
if errorlevel 1 (
    echo hello.ast.json missing JSON kind field.
    exit /b 1
)
findstr /C:"add" release\tmp\hello.ast.json >nul
if errorlevel 1 (
    echo hello.ast.json missing function name.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$dir = Join-Path (Resolve-Path 'release\tmp') (([char]0x4E2D)+([char]0x6587)+'_path'); [System.IO.Directory]::CreateDirectory($dir) | Out-Null; $file = Join-Path $dir (([char]0x5165)+([char]0x53E3)+'.xl'); $out = Join-Path $dir 'out.ast.txt'; $utf8 = New-Object System.Text.UTF8Encoding($false); $src = 'func add(int a, int b) -> int' + [Environment]::NewLine + '    return a + b' + [Environment]::NewLine + 'end' + [Environment]::NewLine; [System.IO.File]::WriteAllText($file, $src, $utf8); & .\release\xl.exe parse $file --emit $out; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $text = [System.IO.File]::ReadAllText($out, [System.Text.Encoding]::UTF8); if ($text -notmatch 'add') { exit 2 }; exit 0"
if errorlevel 1 (
    echo unicode path parse/emit smoke test failed.
    exit /b 1
)

release\xl.exe check examples\hello.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\hello.xl --json > release\tmp\hello.check.json
if errorlevel 1 exit /b 1
findstr /C:"diagnostics" release\tmp\hello.check.json >nul
if errorlevel 1 (
    echo hello.check.json missing diagnostics field.
    exit /b 1
)
findstr /C:"true" release\tmp\hello.check.json >nul
if errorlevel 1 (
    echo hello.check.json missing success flag.
    exit /b 1
)

release\xl.exe check test\cases\sem_diag_utf8.xl >nul 2> release\tmp\sem_diag_utf8.err.txt
if not errorlevel 1 (
    echo sem_diag_utf8.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\sem_diag_utf8.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected='return '+([char]0x8868)+([char]0x8FBE)+([char]0x5F0F)+([char]0x7C7B)+([char]0x578B)+([char]0x548C)+([char]0x51FD)+([char]0x6570)+([char]0x8FD4)+([char]0x56DE)+([char]0x7C7B)+([char]0x578B)+([char]0x4E0D)+([char]0x5339)+([char]0x914D); if($text.IndexOf($expected) -lt 0){ Write-Host 'sem_diag_utf8.err.txt missing UTF-8 semantic diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$name=([char]0x7F3A)+([char]0x5931)+'_'+([char]0x6587)+([char]0x4EF6)+'.xl'; $missing=Join-Path 'release\tmp' $name; $err='release\tmp\cli_lex_missing_utf8.err.txt'; $out='release\tmp\cli_lex_missing_utf8.out.txt'; $p=Start-Process -FilePath 'release\xl.exe' -ArgumentList @('lex',$missing) -NoNewWindow -PassThru -Wait -RedirectStandardOutput $out -RedirectStandardError $err; if($p.ExitCode -eq 0){ Write-Host 'lex missing unicode path should fail.'; exit 1 }; $bytes=[System.IO.File]::ReadAllBytes($err); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); if($text.IndexOf('cannot read file:') -lt 0 -or $text.IndexOf($name) -lt 0){ Write-Host 'cli_lex_missing_utf8.err.txt missing UTF-8 path diagnostic.'; exit 2 }"
if errorlevel 1 exit /b 1

release\xl.exe cgen examples\hello.xl --dump-dir release\tmp\dump_hello
if errorlevel 1 exit /b 1
findstr /C:"KW_FUNC" release\tmp\dump_hello\hello.tokens.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing lexer dump.
    exit /b 1
)
findstr /C:"kind" release\tmp\dump_hello\hello.ast.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing AST dump.
    exit /b 1
)
findstr /C:"func add" release\tmp\dump_hello\hello.xlir >nul
if errorlevel 1 (
    echo cgen dump-dir missing IR dump.
    exit /b 1
)
findstr /C:"abiVersion" release\tmp\dump_hello\hello.abi.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing ABI dump.
    exit /b 1
)
findstr /C:"xlang-xapi-1" release\tmp\dump_hello\hello.xapi.json >nul
if errorlevel 1 (
    echo cgen dump-dir missing XAPI dump.
    exit /b 1
)
findstr /C:"Generated by xlang DEMO6 cgen." release\tmp\dump_hello\hello.c >nul
if errorlevel 1 (
    echo cgen dump-dir missing C dump.
    exit /b 1
)
findstr /C:"#ifndef XL6_HELLO_H" release\tmp\dump_hello\hello.h >nul
if errorlevel 1 (
    echo cgen dump-dir missing header dump.
    exit /b 1
)

release\xl.exe check test\cases\import_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_metadata.xl --emit release\tmp\import_metadata.xlir
if errorlevel 1 exit /b 1
findstr /C:"import counter" release\tmp\import_metadata.xlir >nul
if errorlevel 1 (
    echo import_metadata.xlir missing package import.
    exit /b 1
)
findstr /C:"import \"local/math\"" release\tmp\import_metadata.xlir >nul
if errorlevel 1 (
    echo import_metadata.xlir missing string import.
    exit /b 1
)
release\xl.exe xapi test\cases\import_metadata.xl --emit release\tmp\import_metadata.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"\"imports\"" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing imports array.
    exit /b 1
)
findstr /C:"counter" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing counter import.
    exit /b 1
)
findstr /C:"local/math" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing local path import.
    exit /b 1
)
findstr /C:"\"resolved\":true" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing resolved package import.
    exit /b 1
)
findstr /C:"xlang.package.json" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing resolved package manifest.
    exit /b 1
)
release\xl.exe cgen test\cases\import_metadata.xl --emit release\tmp\import_metadata.c
if errorlevel 1 exit /b 1
findstr /C:"int main(" release\tmp\import_metadata.c >nul
if not errorlevel 1 (
    echo import_metadata.c should not emit main.
    exit /b 1
)

release\xl.exe check test\cases\import_path_no_ext.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_path_no_ext.xl --emit release\tmp\import_path_no_ext.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_path_no_ext_value\" func __xl6_import_probe_value" release\tmp\import_path_no_ext.xlir >nul
if errorlevel 1 (
    echo import_path_no_ext.xlir missing generated extern.
    exit /b 1
)
findstr /C:"call __xl6_import_probe_value" release\tmp\import_path_no_ext.xlir >nul
if errorlevel 1 (
    echo import_path_no_ext.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_path_no_ext.xl --emit release\tmp\import_path_no_ext.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_path_no_ext_value" release\tmp\import_path_no_ext.c >nul
if errorlevel 1 (
    echo import_path_no_ext.c missing imported C symbol.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=(Resolve-Path '.').Path; $work=Join-Path $root 'test\cases\import_relative_scope\root'; $case='test\cases\import_relative_scope\root\source\import_relative_scope.xl'; $src=Join-Path $root $case; $correct=Join-Path $root 'test\cases\import_relative_scope\root\source\pkg.xapi.json'; $wrong=Join-Path $root 'test\cases\import_relative_scope\root\pkg.xapi.json'; $out=Join-Path $root 'release\tmp\import_relative_scope.xlir'; foreach($file in @($src,$correct,$wrong)){ if(!(Test-Path -LiteralPath $file)){ exit 10 } }; Push-Location $work; & (Join-Path $root 'release\xl.exe') ir 'source\import_relative_scope.xl' --emit $out; $code=$LASTEXITCODE; Pop-Location; if($code -ne 0){ exit $code }; $text=[System.IO.File]::ReadAllText($out,[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_import_relative_local_value'){ exit 11 }; if($text -notmatch '__xl6_import_pkg_value'){ exit 12 }; if($text -match 'xl6_import_relative_wrong_value'){ exit 13 }"
if errorlevel 1 (
    echo import_relative_scope resolver check failed.
    exit /b 1
)

release\xl.exe check test\cases\import_lib_search.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_lib_search.xl --emit release\tmp\import_lib_search.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_libprobe_value\" func __xl6_import_libprobe_value" release\tmp\import_lib_search.xlir >nul
if errorlevel 1 (
    echo import_lib_search.xlir missing generated extern.
    exit /b 1
)
findstr /C:"call __xl6_import_libprobe_value" release\tmp\import_lib_search.xlir >nul
if errorlevel 1 (
    echo import_lib_search.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_lib_search.xl --emit release\tmp\import_lib_search.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_libprobe_value" release\tmp\import_lib_search.c >nul
if errorlevel 1 (
    echo import_lib_search.c missing imported C symbol.
    exit /b 1
)

release\xl.exe check test\cases\import_xapi_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_use.xl --emit release\tmp\import_xapi_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_import_counter_add\" func __xl6_import_counter_counterAdd" release\tmp\import_xapi_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_use.xlir missing injected extern function.
    exit /b 1
)
findstr /C:"call __xl6_import_counter_counterAdd" release\tmp\import_xapi_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_use.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_use.xl --emit release\tmp\import_xapi_use.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_import_counter_add" release\tmp\import_xapi_use.c >nul
if errorlevel 1 (
    echo import_xapi_use.c missing imported C symbol.
    exit /b 1
)
release\xl.exe run test\cases\import_xapi_use.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_xapi_use.xl -o release\tmp\import_xapi_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_union_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_union_use.xl --emit release\tmp\import_xapi_type_union_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_union_api_make\" func __xl6_import_union_api_makePayload" release\tmp\import_xapi_type_union_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.xlir missing imported union factory.
    exit /b 1
)
findstr /C:"__xl6_import_union_api_Payload" release\tmp\import_xapi_type_union_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.xlir missing internal imported union type.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_union_use.xl --emit release\tmp\import_xapi_type_union_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Union_____xl6__import__union__api__Payload" release\tmp\import_xapi_type_union_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.c missing generated internal union C type.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_union_use.xl -o release\tmp\import_xapi_type_union_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_union_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_union_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\import_xapi_type_union_var_use.xl --emit release\tmp\import_xapi_type_union_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern XL_Union_____xl6__import__union__api__Payload xl6_union_api_default;" release\tmp\import_xapi_type_union_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_var_use.c missing imported union extern variable.
    exit /b 1
)
findstr /C:"xl6_union_api_default.i" release\tmp\import_xapi_type_union_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_var_use.c missing imported union variable field access.
    exit /b 1
)
findstr /C:"__xl6_import_union_api_defaultPayload" release\tmp\import_xapi_type_union_var_use.c >nul
if not errorlevel 1 (
    echo import_xapi_type_union_var_use.c leaked internal union variable name.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_union_var_use.xl -o release\tmp\import_xapi_type_union_var_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_union_var_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\import_xapi_type_class_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_class_use.xl --emit release\tmp\import_xapi_type_class_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_class_api_make\" func __xl6_import_class_api_makePoint" release\tmp\import_xapi_type_class_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.xlir missing imported class factory.
    exit /b 1
)
findstr /C:"__xl6_import_class_api_Point" release\tmp\import_xapi_type_class_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.xlir missing internal imported class type.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_class_use.xl --emit release\tmp\import_xapi_type_class_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_____xl6__import__class__api__Point" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing generated internal class C type.
    exit /b 1
)
findstr /C:"long long x;" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing imported class field x.
    exit /b 1
)
findstr /C:"long long y;" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing imported class field y.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_use.xl -o release\tmp\import_xapi_type_class_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\import_xapi_type_class_var_use.xl --emit release\tmp\import_xapi_type_class_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern XL_Class_____xl6__import__class__api__Point xl6_class_api_origin;" release\tmp\import_xapi_type_class_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_var_use.c missing imported class extern variable.
    exit /b 1
)
findstr /C:"xl6_class_api_origin.x" release\tmp\import_xapi_type_class_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_var_use.c missing imported class variable field access.
    exit /b 1
)
findstr /C:"__xl6_import_class_api_origin" release\tmp\import_xapi_type_class_var_use.c >nul
if not errorlevel 1 (
    echo import_xapi_type_class_var_use.c leaked internal class variable name.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_var_use.xl -o release\tmp\import_xapi_type_class_var_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_var_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_class_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_class_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\import_xapi_type_class_inherit_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_class_inherit_use.xl --emit release\tmp\import_xapi_type_class_inherit_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"class __xl6_import_class_inherit_api_NamedPoint extends __xl6_import_class_inherit_api_Entity" release\tmp\import_xapi_type_class_inherit_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.xlir missing rewritten class base.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_class_inherit_use.xl --emit release\tmp\import_xapi_type_class_inherit_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_____xl6__import__class__inherit__api__Entity base;" release\tmp\import_xapi_type_class_inherit_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.c missing generated class base layout.
    exit /b 1
)
findstr /C:"point.base.id" release\tmp\import_xapi_type_class_inherit_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.c missing inherited field access.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_inherit_use.xl -o release\tmp\import_xapi_type_class_inherit_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_inherit_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_inherit_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_class_inherit_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_class_inherit_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe build test\cases\import_no_global_pollution.xl -o release\tmp\import_no_global_pollution.exe >nul 2> release\tmp\import_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_no_global_pollution.xl should fail.
    exit /b 1
)

release\xl.exe run test\cases\import_dependency_private.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_private.xl -o release\tmp\import_dependency_private.exe
if errorlevel 1 exit /b 1
release\tmp\import_dependency_private.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_dependency_versions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_versions.xl -o release\tmp\import_dependency_versions.exe
if errorlevel 1 exit /b 1
release\tmp\import_dependency_versions.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'packages\metadata_combo\package.xapi.json' -Raw | ConvertFrom-Json; if($j.links[0] -ne '../../release/tmp/metadata_combo_link.a'){exit 1}; if($j.loadlibs[0] -ne '../../release/tmp/metadata_combo_loadlib.dll'){exit 2}; if($j.cSources[0] -ne 'metadata_combo_native.c'){exit 3}; if(-not $j.native.singleton){exit 4}; if($j.native.loadName -ne 'metadata_combo_runtime.dll'){exit 5}; if($j.native.version -ne '1.0.0'){exit 6}"
if errorlevel 1 (
    echo metadata_combo package.xapi.json missing combined dependency metadata.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\metadata_combo_link_host.c -o release\tmp\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs release\tmp\metadata_combo_link.a release\tmp\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\metadata_combo_loadlib_host.c -o release\tmp\metadata_combo_loadlib.dll
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_metadata_combo.xl -o release\tmp\import_metadata_combo.exe
if errorlevel 1 exit /b 1
release\tmp\import_metadata_combo.exe
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\import_deps_snapshot_host.c src\compile\xl6_imports.c src\package\xl6_package.c src\abi\xl6_xapi_load.c src\abi\xl6_xapi_dump.c src\abi\xl6_callable.c src\type\xl6_type_model.c src\common\xl6_console.c src\common\xl6_output.c src\parser\xl6_parser_api.c src\parser\xl6_parser.c src\lexer\xl6_lexer_api.c src\lexer\xl6_lexer_gen.c src\lexer\xl6_token.c src\ast\xl6_ast.c src\diag\xl6_diag.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\import_deps_snapshot_host.exe
if errorlevel 1 exit /b 1
release\tmp\import_deps_snapshot_host.exe test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\import_dependency_no_pollution.xl -o release\tmp\import_dependency_no_pollution.exe >nul 2> release\tmp\import_dependency_no_pollution.err.txt
if not errorlevel 1 (
    echo import_dependency_no_pollution.xl should fail.
    exit /b 1
)
findstr /C:"private package dependency is not exported" release\tmp\import_dependency_no_pollution.err.txt >nul
if errorlevel 1 (
    echo import_dependency_no_pollution.err.txt missing private dependency diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_dependency_version_mismatch.xl >nul 2> release\tmp\import_dependency_version_mismatch.err.txt
if not errorlevel 1 (
    echo import_dependency_version_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"package dependency version mismatch" release\tmp\import_dependency_version_mismatch.err.txt >nul
if errorlevel 1 (
    echo import_dependency_version_mismatch.err.txt missing version mismatch diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_xapi_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_var_use.xl --emit release\tmp\import_xapi_var_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_imported_counter\" var __xl6_import_settings_importedCounter : int" release\tmp\import_xapi_var_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_var_use.xlir missing injected extern variable.
    exit /b 1
)
findstr /C:"return __xl6_import_settings_importedCounter" release\tmp\import_xapi_var_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_var_use.xlir missing namespace variable rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_var_use.xl --emit release\tmp\import_xapi_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t xl6_imported_counter;" release\tmp\import_xapi_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_var_use.c missing imported C variable.
    exit /b 1
)

release\xl.exe check test\cases\import_missing_xapi.xl >nul 2> release\tmp\import_missing_xapi.err.txt
if not errorlevel 1 (
    echo import_missing_xapi.xl should fail.
    exit /b 1
)
findstr /C:"package.xapi.json" release\tmp\import_missing_xapi.err.txt >nul
if errorlevel 1 (
    echo import_missing_xapi.err.txt missing interface file diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_broken_xapi.xl >nul 2> release\tmp\import_broken_xapi.err.txt
if not errorlevel 1 (
    echo import_broken_xapi.xl should fail.
    exit /b 1
)
findstr /C:"xlang-xapi-1" release\tmp\import_broken_xapi.err.txt >nul
if errorlevel 1 (
    echo import_broken_xapi.err.txt missing format diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_invalid_decl_xapi.xl >nul 2> release\tmp\import_invalid_decl_xapi.err.txt
if not errorlevel 1 (
    echo import_invalid_decl_xapi.xl should fail.
    exit /b 1
)
findstr /C:"XAPI import generated invalid extern declarations" release\tmp\import_invalid_decl_xapi.err.txt >nul
if errorlevel 1 (
    echo import_invalid_decl_xapi.err.txt missing generated declaration diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_unsupported_xapi_use.xl >nul 2> release\tmp\import_unsupported_xapi_use.err.txt
if not errorlevel 1 (
    echo import_unsupported_xapi_use.xl should fail.
    exit /b 1
)
findstr /C:"not supported by current C ABI bridge" release\tmp\import_unsupported_xapi_use.err.txt >nul
if errorlevel 1 (
    echo import_unsupported_xapi_use.err.txt missing unsupported import diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_native_singleton_conflict.xl >nul 2> release\tmp\import_native_singleton_conflict.err.txt
if not errorlevel 1 (
    echo import_native_singleton_conflict.xl should fail.
    exit /b 1
)
findstr /C:"native singleton package conflict" release\tmp\import_native_singleton_conflict.err.txt >nul
if errorlevel 1 (
    echo import_native_singleton_conflict.err.txt missing singleton conflict diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_outside_func.xl --json > release\tmp\sem_return_outside_func.check.json
if not errorlevel 1 (
    echo sem_return_outside_func.xl should fail in JSON check.
    exit /b 1
)
findstr /C:"false" release\tmp\sem_return_outside_func.check.json >nul
if errorlevel 1 (
    echo sem_return_outside_func.check.json missing failure flag.
    exit /b 1
)
findstr /C:"message" release\tmp\sem_return_outside_func.check.json >nul
if errorlevel 1 (
    echo sem_return_outside_func.check.json missing diagnostic message.
    exit /b 1
)

release\xl.exe lex examples\literals.xl > release\tmp\literals.tokens.txt
if errorlevel 1 exit /b 1

release\xl.exe parse examples\literals.xl > release\tmp\literals.ast.txt
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_numeric_literals.xl --emit release\tmp\cgen_numeric_literals.c
if errorlevel 1 exit /b 1
findstr /C:"long long dec = 1000000LL;" release\tmp\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing decimal literal normalization.
    exit /b 1
)
findstr /C:"long long hexB = 42LL;" release\tmp\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing &H literal normalization.
    exit /b 1
)
findstr /C:"double sci = 12.5e1;" release\tmp\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing float underscore normalization.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_numeric_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_numeric_literals.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_numeric_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_int_literal_overflow.xl --emit release\tmp\cgen_int_literal_overflow.c >nul 2> release\tmp\cgen_int_literal_overflow.err.txt
if not errorlevel 1 (
    echo cgen_int_literal_overflow.xl should fail.
    exit /b 1
)
findstr /C:"integer literal is too large or invalid" release\tmp\cgen_int_literal_overflow.err.txt >nul
if errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt missing integer literal overflow diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\cgen_int_literal_overflow.err.txt >nul
if not errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check examples\containers.xl
if errorlevel 1 exit /b 1

release\xl.exe run examples\containers.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\layout.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\comments.xl
if errorlevel 1 exit /b 1

release\xl.exe lex test\cases\sem_unclosed_comment.xl > release\tmp\sem_unclosed_comment.tokens.txt 2> release\tmp\sem_unclosed_comment.err.txt
if not errorlevel 1 (
    echo sem_unclosed_comment.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" release\tmp\sem_unclosed_comment.tokens.txt >nul
if errorlevel 1 (
    echo sem_unclosed_comment.tokens.txt missing ERROR token.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\sem_unclosed_comment.tokens.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x672A)+([char]0x95ED)+([char]0x5408)+([char]0x7684)+([char]0x591A)+([char]0x884C)+([char]0x6CE8)+([char]0x91CA); if($text.IndexOf($expected) -lt 0){ Write-Host 'sem_unclosed_comment.tokens.txt missing unclosed-comment diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_case_sensitive.xl --emit release\tmp\cgen_case_sensitive.c
if errorlevel 1 exit /b 1
findstr /C:"long long Name = 1LL;" release\tmp\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing upper-case Name.
    exit /b 1
)
findstr /C:"long long name = 2LL;" release\tmp\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing lower-case name.
    exit /b 1
)
findstr /C:"long long NAME = 3LL;" release\tmp\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing all-caps NAME.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_case_sensitive.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_case_sensitive.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_case_sensitive.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_context_keywords.xl --emit release\tmp\cgen_context_keywords.c
if errorlevel 1 exit /b 1
findstr /C:"long long self = 1LL;" release\tmp\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary self variable.
    exit /b 1
)
findstr /C:"long long base = 2LL;" release\tmp\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary base variable.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_context_keywords.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_context_keywords.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_context_keywords.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_unicode_identifiers.xl --emit release\tmp\cgen_unicode_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"xl_fn__u52A0__u4E00___int" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode function name.
    exit /b 1
)
findstr /C:"long long _u6570__u503C_" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode parameter name.
    exit /b 1
)
findstr /C:"long long _u7ED3__u679C_" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode variable name.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_unicode_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_args.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_defaults.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\varargs.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_named_vararg.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\containers.xl > release\tmp\containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_containers.xl > release\tmp\cgen_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_in_containers.xl > release\tmp\cgen_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_in_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_for_in_containers.xl > release\tmp\cgen_for_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_for_in_containers.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow.xl > release\tmp\control_flow.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_do_while.xl > release\tmp\control_flow_do_while.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_do_while.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_inline_if.xl > release\tmp\control_flow_inline_if.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_inline_if.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\operators.xl > release\tmp\operators.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\operators.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_null_coalesce.xl > release\tmp\cgen_null_coalesce.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_null_coalesce.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\member_index.xl > release\tmp\member_index.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\member_index.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generic_types.xl > release\tmp\generic_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generic_types.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\type_values.xl > release\tmp\type_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_values.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_type_name_identifiers.xl --emit release\tmp\cgen_type_name_identifiers.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_type_name_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_type_name_identifiers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_type_name_identifiers.exe
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\type_aliases.xl > release\tmp\type_aliases.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_aliases.xl
if errorlevel 1 exit /b 1
release\xl.exe xapi test\cases\export_alias.xl --emit release\tmp\export_alias.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"\"type\":\"int\"" release\tmp\export_alias.xapi.json >nul
if errorlevel 1 (
    echo export_alias.xapi.json missing canonical alias type.
    exit /b 1
)
findstr /C:"\"callingConvention\":\"cdecl\"" release\tmp\export_alias.xapi.json >nul
if errorlevel 1 (
    echo export_alias.xapi.json missing callable convention.
    exit /b 1
)
findstr /C:"\"varargMode\":\"none\"" release\tmp\export_alias.xapi.json >nul
if errorlevel 1 (
    echo export_alias.xapi.json missing callable vararg mode.
    exit /b 1
)

release\xl.exe parse test\cases\cgen_string_concat.xl > release\tmp\cgen_string_concat.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_concat.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_compare.xl > release\tmp\cgen_string_compare.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_compare.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_literals.xl > release\tmp\cgen_string_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_time_literals.xl > release\tmp\cgen_time_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_time_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_auto_expressions.xl > release\tmp\cgen_auto_expressions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_auto_expressions.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\invalid_numeric_operator.xl > release\tmp\invalid_numeric_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_numeric_operator.xl should fail.
    exit /b 1
)
findstr /C:"numeric operator operands must be numeric" release\tmp\invalid_numeric_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_numeric_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_integer_operator.xl > release\tmp\invalid_integer_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_integer_operator.xl should fail.
    exit /b 1
)
findstr /C:"integer operator operands must be integer" release\tmp\invalid_integer_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_integer_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_float.xl > release\tmp\sem_pow_float.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_float.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" release\tmp\sem_pow_float.err.txt >nul
if errorlevel 1 (
    echo sem_pow_float.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_var.xl > release\tmp\sem_pow_var.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_var.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" release\tmp\sem_pow_var.err.txt >nul
if errorlevel 1 (
    echo sem_pow_var.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_division_by_zero.xl > release\tmp\invalid_division_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_division_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" release\tmp\invalid_division_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_division_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_modulo_by_zero.xl > release\tmp\invalid_modulo_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_modulo_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" release\tmp\invalid_modulo_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_modulo_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_var_target.xl > release\tmp\sem_compound_var_target.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_var_target.xl should fail.
    exit /b 1
)
findstr /C:"var compound assignment target is not supported" release\tmp\sem_compound_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_compound_var_target.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_numeric_type.xl > release\tmp\sem_compound_numeric_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_numeric_type.xl should fail.
    exit /b 1
)
findstr /C:"numeric compound assignment operands must be numeric" release\tmp\sem_compound_numeric_type.err.txt >nul
if errorlevel 1 (
    echo sem_compound_numeric_type.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_float_to_int.xl > release\tmp\sem_compound_float_to_int.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_float_to_int.xl should fail.
    exit /b 1
)
findstr /C:"compound assignment result type does not match target type" release\tmp\sem_compound_float_to_int.err.txt >nul
if errorlevel 1 (
    echo sem_compound_float_to_int.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_div_zero.xl > release\tmp\sem_compound_div_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_div_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" release\tmp\sem_compound_div_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_div_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_mod_zero.xl > release\tmp\sem_compound_mod_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_mod_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" release\tmp\sem_compound_mod_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_mod_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_unary_operator.xl > release\tmp\invalid_unary_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_unary_operator.xl should fail.
    exit /b 1
)
findstr /C:"bitwise not operand must be integer" release\tmp\invalid_unary_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_unary_operator.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\cgen_var_values.xl > release\tmp\cgen_var_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_var_values.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_type_conversions.xl > release\tmp\cgen_type_conversions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_type_conversions.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_implicit_methods.xl > release\tmp\cgen_implicit_methods.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_implicit_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generics.xl > release\tmp\generics.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\generic_identity.xl
if errorlevel 1 exit /b 1

release\xl.exe check examples\custom_counter.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\overloads.xl > release\tmp\overloads.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\overload_varargs.xl > release\tmp\overload_varargs.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overload_varargs.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\function_types.xl > release\tmp\function_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\function_types.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\anonymous_functions.xl > release\tmp\anonymous_functions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\anonymous_functions.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\anonymous_functions.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\class_custom.xl > release\tmp\class_custom.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\class_custom.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\class_custom.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\class_point.xl
if errorlevel 1 exit /b 1
release\xl.exe check examples\second_stage.xl
if errorlevel 1 exit /b 1
release\xl.exe run examples\second_stage.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe ir examples\hello.xl > release\tmp\hello.xlir
if errorlevel 1 exit /b 1
findstr /C:"func add(a: int, b: int) -> int" release\tmp\hello.xlir >nul
if errorlevel 1 (
    echo hello.xlir missing function signature.
    exit /b 1
)
release\xl.exe ir examples\hello.xl --json --emit release\tmp\hello.ir.json
if errorlevel 1 exit /b 1
findstr /C:"stage" release\tmp\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing stage field.
    exit /b 1
)
findstr /C:"add" release\tmp\hello.ir.json >nul
if errorlevel 1 (
    echo hello.ir.json missing function name.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow.xl --emit release\tmp\control_flow.xlir
if errorlevel 1 exit /b 1
findstr /C:"for index, value in call range(1, 10, 2)" release\tmp\control_flow.xlir >nul
if errorlevel 1 (
    echo control_flow.xlir missing range for loop.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_do_while.xl --emit release\tmp\control_flow_do_while.xlir
if errorlevel 1 exit /b 1
findstr /C:"do" release\tmp\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing do block.
    exit /b 1
)
findstr /C:"while (value < 3)" release\tmp\control_flow_do_while.xlir >nul
if errorlevel 1 (
    echo control_flow_do_while.xlir missing while condition.
    exit /b 1
)

release\xl.exe ir test\cases\control_flow_inline_if.xl --emit release\tmp\control_flow_inline_if.xlir
if errorlevel 1 exit /b 1
findstr /C:"return 1" release\tmp\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline return.
    exit /b 1
)
findstr /C:"else" release\tmp\control_flow_inline_if.xlir >nul
if errorlevel 1 (
    echo control_flow_inline_if.xlir missing inline else.
    exit /b 1
)

release\xl.exe ir test\cases\operators.xl --emit release\tmp\operators.xlir
if errorlevel 1 exit /b 1
findstr /C:"a += 2" release\tmp\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing compound assignment.
    exit /b 1
)
findstr /C:"null ?? 9" release\tmp\operators.xlir >nul
if errorlevel 1 (
    echo operators.xlir missing null coalesce expression.
    exit /b 1
)

release\xl.exe ir test\cases\member_index.xl --emit release\tmp\member_index.xlir
if errorlevel 1 exit /b 1
findstr /C:"obj.name" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing member access.
    exit /b 1
)
findstr /C:"obj?.name" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe member access.
    exit /b 1
)
findstr /C:"obj[0]" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing index access.
    exit /b 1
)
findstr /C:"obj?[1]" release\tmp\member_index.xlir >nul
if errorlevel 1 (
    echo member_index.xlir missing safe index access.
    exit /b 1
)

release\xl.exe ir test\cases\function_types.xl --emit release\tmp\function_types.xlir
if errorlevel 1 exit /b 1
findstr /C:"var binary : function(int, int) -> int" release\tmp\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing function type signature.
    exit /b 1
)
findstr /C:"var parser : function(string) -> (bool, int)" release\tmp\function_types.xlir >nul
if errorlevel 1 (
    echo function_types.xlir missing multi-return function type.
    exit /b 1
)

release\xl.exe ir test\cases\anonymous_functions.xl --emit release\tmp\anonymous_functions.xlir
if errorlevel 1 exit /b 1
findstr /C:"var inc : auto = func(value: int) -> int <body>" release\tmp\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing arrow anonymous function.
    exit /b 1
)
findstr /C:"return func(value: int) -> int <body>" release\tmp\anonymous_functions.xlir >nul
if errorlevel 1 (
    echo anonymous_functions.xlir missing captured anonymous function return.
    exit /b 1
)

release\xl.exe ir test\cases\class_custom.xl --emit release\tmp\class_custom.xlir
if errorlevel 1 exit /b 1
findstr /C:"class Point" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing class declaration.
    exit /b 1
)
findstr /C:"field x : int = 0" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing class field.
    exit /b 1
)
findstr /C:"extern \"bag_count\" func count() -> int" release\tmp\class_custom.xlir >nul
if errorlevel 1 (
    echo class_custom.xlir missing custom extern method.
    exit /b 1
)

release\xl.exe ir test\cases\generics.xl --emit release\tmp\generics.xlir
if errorlevel 1 exit /b 1
findstr /C:"func identity<T>(value: T) -> T" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic function.
    exit /b 1
)
findstr /C:"class Box<T>" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing generic class.
    exit /b 1
)
findstr /C:"identity::<int>" release\tmp\generics.xlir >nul
if errorlevel 1 (
    echo generics.xlir missing explicit generic instantiation.
    exit /b 1
)

release\xl.exe ir test\cases\overloads.xl --emit release\tmp\overloads.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: int) -> int" release\tmp\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing int overload.
    exit /b 1
)
findstr /C:"func pick(value: string) -> string" release\tmp\overloads.xlir >nul
if errorlevel 1 (
    echo overloads.xlir missing string overload.
    exit /b 1
)

release\xl.exe ir test\cases\overload_varargs.xl --emit release\tmp\overload_varargs.xlir
if errorlevel 1 exit /b 1
findstr /C:"func pick(value: string) -> int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing fixed overload.
    exit /b 1
)
findstr /C:"func pick(value: string, arg: array<var>) -> int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing array vararg overload.
    exit /b 1
)
findstr /C:"func options(path: string, arg: dict<var>) -> int" release\tmp\overload_varargs.xlir >nul
if errorlevel 1 (
    echo overload_varargs.xlir missing dict vararg overload.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_basic.xl --emit release\tmp\cgen_basic.c
if errorlevel 1 exit /b 1
findstr /C:"long long add(long long a, long long b);" release\tmp\cgen_basic.c >nul
if errorlevel 1 (
    echo cgen_basic.c missing add prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_basic.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_basic.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_default_init.xl --emit release\tmp\cgen_default_init.c
if errorlevel 1 exit /b 1
findstr /C:"long long count = 0;" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing int default init.
    exit /b 1
)
findstr /C:"const char* name = \"\";" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing string default init.
    exit /b 1
)
findstr /C:"void* handle = 0;" release\tmp\cgen_default_init.c >nul
if errorlevel 1 (
    echo cgen_default_init.c missing point default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_default_init.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_default_init.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_default_init.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union.xl --emit release\tmp\cgen_union.c
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing top-level union typedef.
    exit /b 1
)
findstr /C:"union {" release\tmp\cgen_union.c >nul
if errorlevel 1 (
    echo cgen_union.c missing inline class union.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_union.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_union.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_union.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_union_var_box.xl --emit release\tmp\cgen_union_var_box.c
if errorlevel 1 exit /b 1
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union XRT type descriptor.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Union_Payload" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union hash type op.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(p)" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing union var boxing helper call.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xl6_type_desc_XL_Union_Payload()" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array descriptor.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xl6_type_desc_XL_Union_Payload(), copied)" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union array boxing.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xl6_type_desc_XL_Union_Payload()" release\tmp\cgen_union_var_box.c >nul
if errorlevel 1 (
    echo cgen_union_var_box.c missing typed union set descriptor.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_union_var_box.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_union_var_box.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_union_var_box.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_inline_union_generic_class_field.xl --emit release\tmp\cgen_inline_union_generic_class_field.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing generic class dependency.
    exit /b 1
)
findstr /C:"XL_Class_Box__int boxed;" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic class field.
    exit /b 1
)
findstr /C:"XL_Class_Box__T" release\tmp\cgen_inline_union_generic_class_field.c >nul
if not errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c leaked unresolved generic class field.
    exit /b 1
)
findstr /C:"packet.boxed.value = 41LL;" release\tmp\cgen_inline_union_generic_class_field.c >nul
if errorlevel 1 (
    echo cgen_inline_union_generic_class_field.c missing inline union generic member write.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_inline_union_generic_class_field.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_inline_union_generic_class_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union.xl --dump-dir release\tmp\dump_export_union
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload p" release\tmp\dump_export_union\export_union.h >nul
if errorlevel 1 (
    echo export_union.h missing union parameter.
    exit /b 1
)
findstr /C:"\"kind\":\"union\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union type metadata.
    exit /b 1
)
findstr /C:"\"fields\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union fields metadata.
    exit /b 1
)
findstr /C:"\"name\":\"i\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union field i metadata.
    exit /b 1
)
findstr /C:"\"type\":\"c_double\"" release\tmp\dump_export_union\export_union.xapi.json >nul
if errorlevel 1 (
    echo export_union.xapi.json missing union c_double field type.
    exit /b 1
)
release\xl.exe xapi test\cases\xapi_type_fields.xl --emit release\tmp\xapi_type_fields.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"class\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class type metadata.
    exit /b 1
)
findstr /C:"\"name\":\"tag\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class field tag metadata.
    exit /b 1
)
findstr /C:"Packet tag field." release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class field doc metadata.
    exit /b 1
)
findstr /C:"\"kind\":\"desc\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing structured class field doc metadata.
    exit /b 1
)
findstr /C:"\"base\":\"Packet\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing class base metadata.
    exit /b 1
)
findstr /C:"\"typeParams\":[\"T\"]" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing generic class type parameter metadata.
    exit /b 1
)
findstr /C:"\"storage\":\"inline-union\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing inline union field storage metadata.
    exit /b 1
)
findstr /C:"\"name\":\"code\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing inline union field code metadata.
    exit /b 1
)
findstr /C:"\"name\":\"ClassPayload\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing union class field owner metadata.
    exit /b 1
)
findstr /C:"\"type\":\"Point\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing union class field type metadata.
    exit /b 1
)
findstr /C:"\"kind\":\"custom\"" release\tmp\xapi_type_fields.json >nul
if errorlevel 1 (
    echo xapi_type_fields.json missing custom type metadata.
    exit /b 1
)
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\xapi_load_types_host.c src\abi\xl6_xapi_load.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\xapi_load_types_host.exe
if errorlevel 1 exit /b 1
release\tmp\xapi_load_types_host.exe release\tmp\xapi_type_fields.json
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\xapi_loader_defaults_host.c src\abi\xl6_xapi_load.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\xapi_loader_defaults_host.exe
if errorlevel 1 exit /b 1
release\tmp\xapi_loader_defaults_host.exe legacy test\cases\xapi_legacy_defaults.xapi.json
if errorlevel 1 exit /b 1
release\tmp\xapi_loader_defaults_host.exe unknowns test\cases\xapi_unknown_fields.xapi.json
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_union_top_anon.xl >nul 2> release\tmp\sem_union_top_anon.err.txt
if not errorlevel 1 (
    echo sem_union_top_anon.xl should fail.
    exit /b 1
)
findstr /C:"top-level union must have a name" release\tmp\sem_union_top_anon.err.txt >nul
if errorlevel 1 (
    echo sem_union_top_anon.err.txt missing named top-level union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_empty.xl >nul 2> release\tmp\sem_union_empty.err.txt
if not errorlevel 1 (
    echo sem_union_empty.xl should fail.
    exit /b 1
)
findstr /C:"union must contain at least one field" release\tmp\sem_union_empty.err.txt >nul
if errorlevel 1 (
    echo sem_union_empty.err.txt missing empty union diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_default.xl >nul 2> release\tmp\sem_union_default.err.txt
if not errorlevel 1 (
    echo sem_union_default.xl should fail.
    exit /b 1
)
findstr /C:"union fields do not support default values" release\tmp\sem_union_default.err.txt >nul
if errorlevel 1 (
    echo sem_union_default.err.txt missing union default diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_duplicate_field.xl >nul 2> release\tmp\sem_union_duplicate_field.err.txt
if not errorlevel 1 (
    echo sem_union_duplicate_field.xl should fail.
    exit /b 1
)
findstr /C:"duplicate union field name" release\tmp\sem_union_duplicate_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_duplicate_field.err.txt missing duplicate union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_anonymous_conflict.xl >nul 2> release\tmp\sem_union_anonymous_conflict.err.txt
if not errorlevel 1 (
    echo sem_union_anonymous_conflict.xl should fail.
    exit /b 1
)
findstr /C:"class member layout name conflicts with an existing field or union" release\tmp\sem_union_anonymous_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_union_anonymous_conflict.err.txt missing inline union conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_named_inline.xl >nul 2> release\tmp\sem_union_named_inline.err.txt
if not errorlevel 1 (
    echo sem_union_named_inline.xl should fail.
    exit /b 1
)
findstr /C:"class inline union must be anonymous in this phase" release\tmp\sem_union_named_inline.err.txt >nul
if errorlevel 1 (
    echo sem_union_named_inline.err.txt missing named inline union diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_unknown_field.xl >nul 2> release\tmp\sem_class_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_unknown_field.err.txt >nul
if not errorlevel 1 (
    echo sem_class_unknown_field.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_unknown_field.xl >nul 2> release\tmp\sem_union_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_union_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"unknown class/union field" release\tmp\sem_union_unknown_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_unknown_field.err.txt missing unknown union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_function_field.xl >nul 2> release\tmp\sem_union_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_function_field.err.txt missing union function field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_destroy_class_field.xl >nul 2> release\tmp\sem_union_destroy_class_field.err.txt
if not errorlevel 1 (
    echo sem_union_destroy_class_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_destroy_class_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_destroy_class_field.err.txt missing union non-plain class field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_generic_inline_function_field.xl >nul 2> release\tmp\sem_union_generic_inline_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_generic_inline_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" release\tmp\sem_union_generic_inline_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_generic_inline_function_field.err.txt missing generic inline union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_member_assign_type.xl >nul 2> release\tmp\sem_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_member_assign_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_nested_member_assign_type.xl >nul 2> release\tmp\sem_nested_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_nested_member_assign_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_multi_member_assign_type.xl >nul 2> release\tmp\sem_multi_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_multi_member_assign_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_array_assign_type.xl >nul 2> release\tmp\sem_typed_array_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_array_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_array_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_type.xl >nul 2> release\tmp\sem_typed_dict_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_dict_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_key_type.xl >nul 2> release\tmp\sem_typed_dict_assign_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_typed_dict_assign_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_safe_dict_key_type.xl >nul 2> release\tmp\sem_typed_safe_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_safe_dict_key_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_string_index_compound_type.xl >nul 2> release\tmp\sem_typed_string_index_compound_type.err.txt
if not errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl should fail.
    exit /b 1
)
findstr /C:"string compound assignment operands must both be string" release\tmp\sem_typed_string_index_compound_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_primitive_widths.xl --emit release\tmp\cgen_primitive_widths.c
if errorlevel 1 exit /b 1
findstr /C:"signed char i8" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int8 C type.
    exit /b 1
)
findstr /C:"unsigned char u8" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint8 C type.
    exit /b 1
)
findstr /C:"short i16" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int16 C type.
    exit /b 1
)
findstr /C:"unsigned short u16" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint16 C type.
    exit /b 1
)
findstr /C:"int i32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int32 C type.
    exit /b 1
)
findstr /C:"unsigned int u32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint32 C type.
    exit /b 1
)
findstr /C:"long long i64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int64 C type.
    exit /b 1
)
findstr /C:"unsigned long long u64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint64 C type.
    exit /b 1
)
findstr /C:"float f32" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float32 C type.
    exit /b 1
)
findstr /C:"double f64" release\tmp\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float64 C type.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_primitive_widths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_containers.xl --emit release\tmp\cgen_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue emptyArray = xvoCreateArray();" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing array default init.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing array literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_list_literal(2" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing list literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_literal(3" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing set literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing dict literal lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_literal(0" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty set lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(0" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty dict lowering.
    exit /b 1
)
findstr /C:"xvalue prefixedArray = xl6_xrt_array_varargs(2" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing prefixed array expression fallback.
    exit /b 1
)
findstr /C:"xvalue prefixedDict = xl6_xrt_dict_varargs(1" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing prefixed dict expression fallback.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed array lowering.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed dict iteration.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedArray)" release\tmp\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed container cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_static_typed_containers.xl --emit release\tmp\cgen_static_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed array create.
    exit /b 1
)
findstr /C:"xrtTypedArrayGet(values" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed array index.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedSetItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed set iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(" release\tmp\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict iteration.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_static_typed_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_typed_container_for_in_returned.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_for_in_returned.xl --emit release\tmp\cgen_typed_container_for_in_returned.c
if errorlevel 1 exit /b 1
findstr /C:"xtarray makeValues(void)" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array return ABI.
    exit /b 1
)
findstr /C:"xvalue makeValues(void)" release\tmp\cgen_typed_container_for_in_returned.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c regressed to xvalue array return.
    exit /b 1
)
findstr /C:"xrtTypedArrayGet(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed array for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing typed dict for-in lowering.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(__xl6_typed_iter_" release\tmp\cgen_typed_container_for_in_returned.c >nul
if errorlevel 1 (
    echo cgen_typed_container_for_in_returned.c missing returned typed array cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_for_in_returned.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_for_in_returned.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_box_to_var.xl --emit release\tmp\cgen_typed_container_box_to_var.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed array boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed list boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed set boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing typed dict boxing helper.
    exit /b 1
)
findstr /C:"xl6_xrt_bind_type(xl6_xrt_typed_array_to_xvalue" release\tmp\cgen_typed_container_box_to_var.c >nul
if errorlevel 1 (
    echo cgen_typed_container_box_to_var.c missing runtime type binding for boxed typed containers.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_box_to_var.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_box_to_var.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_type_desc_dedupe.xl --emit release\tmp\cgen_type_desc_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_type_desc_dedupe.c' -Raw; if(([regex]::Matches($t, 'static const xrt_type_desc\* xl6_runtime_type_desc_[0-9A-F]+\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static const xrt_generic_type generic')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_type_desc_dedupe.c duplicated runtime type desc for same typed container.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_type_desc_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_type_desc_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_assign.xl --emit release\tmp\cgen_typed_container_copy_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set copy helper.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_copy" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict copy helper.
    exit /b 1
)
findstr /C:"xrtTypedArrayCount(valuesCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed container length lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_to_xvalue(xrtTypeInt(), idsCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed set in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_to_xvalue(xrtTypeInt(), values)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed array in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_to_xvalue(xrtTypeString(), names)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed list in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_to_xvalue(xrtTypeInt(), scoresCopy)" release\tmp\cgen_typed_container_copy_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_assign.c missing typed dict in lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_copy_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_copy_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_copy_objects_functions.xl --emit release\tmp\cgen_typed_container_copy_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_array_copy(xrtTypeFunction(), funcs)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed function array copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6_type_desc_XL_Class_FnBox(), boxes)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class array copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_copy(xl6_type_desc_XL_Class_FnBox(), named)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed class dict copy.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_copy(xl6_type_desc_XL_Custom_Native(), natives)" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing typed custom array copy.
    exit /b 1
)
findstr /C:"(*(XL_Class_FnBox*)dst).op.retain" release\tmp\cgen_typed_container_copy_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_copy_objects_functions.c missing class copy function retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_copy_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_copy_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_functions.xl --emit release\tmp\cgen_typed_container_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeFunction()" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function array create.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xrtTypeFunction()" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function set create.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xrtTypedArrayGet" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing callable to function bridge.
    exit /b 1
)
findstr /C:"xl6_function_call_var(first" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing function fallback call.
    exit /b 1
)
findstr /C:"XL6_Function each = xl6_function_from_callable" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration bridge.
    exit /b 1
)
findstr /C:"each.release(each.env);" release\tmp\cgen_typed_container_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_functions.c missing typed function iteration cleanup.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_objects.xl --emit release\tmp\cgen_typed_container_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_type_unbox_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Class_Pair" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing class hash type op.
    exit /b 1
)
findstr /C:"xl6_type_unbox_XL_Custom_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing custom unbox type op.
    exit /b 1
)
findstr /C:"static int xl6_type_compare_XL_Custom_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing custom compare type op.
    exit /b 1
)
findstr /C:"static unsigned long long xl6_type_hash_XL_Custom_Native" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing custom hash type op.
    exit /b 1
)
findstr /C:"xrtTypedSetAddValue(pairSet" release\tmp\cgen_typed_container_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_objects.c missing typed class set fill.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_function_field_objects.xl --emit release\tmp\cgen_typed_container_function_field_objects.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_copy_XL_Class_FnBox" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class type copy helper.
    exit /b 1
)
findstr /C:"__xl6_function_field_assign_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing function field assignment lowering.
    exit /b 1
)
findstr /C:"__xl6_class_assign_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing class assignment retain path.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing member function call lowering.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing var-call fallback runtime.
    exit /b 1
)
findstr /C:"fromArray.op.retain(fromArray.op.env);" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed index class retain.
    exit /b 1
)
findstr /C:"item.op.retain(item.op.env);" release\tmp\cgen_typed_container_function_field_objects.c >nul
if errorlevel 1 (
    echo cgen_typed_container_function_field_objects.c missing typed iteration class retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_function_field_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_function_field_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign.xl --emit release\tmp\cgen_typed_container_index_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArraySetValue(values" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed array index assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(names" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed list index assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(scores" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing typed dict index assignment.
    exit /b 1
)
findstr /C:"static void xl6_runtime_fatal(int code)" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime fatal helper.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(3);" release\tmp\cgen_typed_container_index_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign.c missing centralized runtime fatal call.
    exit /b 1
)
findstr /C:"exit(3);" release\tmp\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" release\tmp\cgen_typed_container_index_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_assign.c still contains direct runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign_objects_functions.xl --emit release\tmp\cgen_typed_container_index_assign_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedListSetValue(pairList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(pairDict" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(pairDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed class dynamic dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedArraySetValue(payloadArray" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(payloadList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(payloadDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed union dynamic dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedArraySetValue(nativeArray" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed custom array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(nativeList" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed custom list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(nativeDict" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed custom dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(nativeDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed custom dynamic dict assignment.
    exit /b 1
)
findstr /C:"xrtTypedArraySetValue(funcs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function array assignment.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(listedFuncs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function list assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(table, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict assignment.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(next)" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing function boxing for typed assignment.
    exit /b 1
)
findstr /C:"xl6_function_from_callable(*(xrt_callable**)xrtTypedListGet(listedFuncs" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function list get lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictGet(table, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_index_assign_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_assign_objects_functions.c missing typed function dynamic dict get lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_assign_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_assign_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_multi_assign_index.xl --emit release\tmp\cgen_typed_container_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArraySetValue(values" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed array multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(scores" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed dict multi assignment.
    exit /b 1
)
findstr /C:"xl6_box_XL_Class_Pair(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing class multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing union multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Custom_Native(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing custom multi assignment boxing.
    exit /b 1
)
findstr /C:"xl6_function_to_xvalue(__xl6_multi_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function multi assignment boxing.
    exit /b 1
)
findstr /C:"xrtTypedListSetValue(payloads" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing typed list multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(natives, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing custom dynamic dict multi assignment.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(table, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing function dynamic dict multi assignment.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_typed_container_multi_assign_index.c' -Raw; $simple=[regex]::Matches($t,'xvalue (__xl6_multi_index_\d+) = (?:xvoCreateInt|xl6_box_XL_Class_Pair|xl6_box_XL_Union_Payload|xl6_function_to_xvalue)[^;]*;\s*if \(!xrtTyped(?:Array|List|Dict)SetValue\([^\n]*\1\)\) \{\s*xl6_runtime_fatal\(3\);\s*\}\s*xvoUnref\(\1\);',[Text.RegularExpressions.RegexOptions]::Singleline); if($simple.Count -lt 6){exit 1}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = xl6_box_XL_Custom_Native\(__xl6_multi_[^;]+;[\s\S]*?xrtTypedDictSetValue\(natives,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 2}; if($t -notmatch 'xvalue (__xl6_multi_index_\d+) = xl6_function_to_xvalue\(__xl6_multi_[^;]+;[\s\S]*?xrtTypedDictSetValue\(table,[\s\S]*?\1[\s\S]*?xvoUnref\(\1\);'){exit 3}"
if errorlevel 1 (
    echo cgen_typed_container_multi_assign_index.c missing multi-return typed index temp cleanup ordering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_compound_assign.xl --emit release\tmp\cgen_typed_container_index_compound_assign.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_typed_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index compound temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_str_concat" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing string compound helper.
    exit /b 1
)
findstr /C:"xvoCreateInt(xl6_int_pow_i64" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index pow compound lowering.
    exit /b 1
)
findstr /C:"xvoCreateFloat(__xl6_typed_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c missing typed index float compound lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_current" release\tmp\cgen_typed_container_index_compound_assign.c >nul
if not errorlevel 1 (
    echo cgen_typed_container_index_compound_assign.c should not use xvalue current for typed index compound assignment.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_index_compound_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_index_compound_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\type_values.xl --emit release\tmp\type_values.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned long long valueType = 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type value declaration.
    exit /b 1
)
findstr /C:"int intOk = (valueType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing type literal comparison.
    exit /b 1
)
findstr /C:"int classOk = (boxType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing class type literal comparison.
    exit /b 1
)
findstr /C:"int arrayOk = (arrayType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing array type literal comparison.
    exit /b 1
)
findstr /C:"int dictOk = (dictType == 0x" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing dict type literal comparison.
    exit /b 1
)
findstr /C:"static const xrt_generic_type generic" release\tmp\type_values.c >nul
if errorlevel 1 (
    echo type_values.c missing XRT generic type metadata.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\type_values.exe
if errorlevel 1 exit /b 1
release\tmp\type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_minimal_runtime.xl --emit release\tmp\cgen_minimal_runtime.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_minimal_runtime.c' -Raw; if($t -match 'typedef struct XL6_Function'){exit 1}; if($t -match '__xl_std_'){exit 2}; if($t -match 'xl6_xrt_array_varargs'){exit 3}; if($t -match '\bxvalue\b'){exit 4}; if($t -notmatch 'int main\(int argc, char\*\* argv\)'){exit 5}"
if errorlevel 1 (
    echo cgen_minimal_runtime.c runtime needs negative snapshot failed.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_minimal_runtime.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_minimal_runtime.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\type_aliases.xl --emit release\tmp\type_aliases.xlir
if errorlevel 1 exit /b 1
findstr /C:"typedef MyInt = int" release\tmp\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing typedef alias.
    exit /b 1
)
findstr /C:"extern type HANDLE = point" release\tmp\type_aliases.xlir >nul
if errorlevel 1 (
    echo type_aliases.xlir missing extern type alias.
    exit /b 1
)
release\xl.exe cgen test\cases\type_aliases.xl --emit release\tmp\type_aliases.c
if errorlevel 1 exit /b 1
findstr /C:"long long value = 41LL;" release\tmp\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing int alias lowering.
    exit /b 1
)
findstr /C:"void* handle = 0;" release\tmp\type_aliases.c >nul
if errorlevel 1 (
    echo type_aliases.c missing point alias lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\type_aliases.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\type_aliases.exe
if errorlevel 1 exit /b 1
release\tmp\type_aliases.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_concat.xl --emit release\tmp\cgen_string_concat.c
if errorlevel 1 exit /b 1
findstr /C:"static const char* xl6_xrt_str_concat" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string concat helper.
    exit /b 1
)
findstr /C:"const char* joined = xl6_xrt_str_concat" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string concat declaration.
    exit /b 1
)
findstr /C:"joined = xl6_xrt_str_concat(joined, \"!\");" release\tmp\cgen_string_concat.c >nul
if errorlevel 1 (
    echo cgen_string_concat.c missing string += lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_concat.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_concat.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_concat.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_compare.xl --emit release\tmp\cgen_string_compare.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(left, same, 0, false) == 0" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string equality lowering.
    exit /b 1
)
findstr /C:"xrtStrComp(left, right, 0, false) < 0" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string ordered comparison lowering.
    exit /b 1
)
findstr /C:"xrtStrApprox(left, 0, same, 0)" release\tmp\cgen_string_compare.c >nul
if errorlevel 1 (
    echo cgen_string_compare.c missing string approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_compare.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_compare.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_compare.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_string_literals.xl --emit release\tmp\cgen_string_literals.c
if errorlevel 1 exit /b 1
findstr /C:"const char* single =" release\tmp\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing single quote string lowering.
    exit /b 1
)
findstr /C:"const char* rawPath =" release\tmp\cgen_string_literals.c >nul
if errorlevel 1 (
    echo cgen_string_literals.c missing raw string lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_string_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_string_literals.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_string_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_time_literals.xl --emit release\tmp\cgen_time_literals.c
if errorlevel 1 exit /b 1
findstr /C:"long long dateOnly = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing date literal lowering.
    exit /b 1
)
findstr /C:"long long timeOnly = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time literal lowering.
    exit /b 1
)
findstr /C:"long long both = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing datetime literal lowering.
    exit /b 1
)
findstr /C:"long long inferred = xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time auto inference.
    exit /b 1
)
findstr /C:"xrtTimeApprox(both, xrtStrToTime(" release\tmp\cgen_time_literals.c >nul
if errorlevel 1 (
    echo cgen_time_literals.c missing time approximate lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_time_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_time_literals.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_time_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_auto_expressions.xl --emit release\tmp\cgen_auto_expressions.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue dynSum =" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto dynamic expression lowering.
    exit /b 1
)
findstr /C:"long long sum =" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto integer expression inference.
    exit /b 1
)
findstr /C:"double floatSum =" release\tmp\cgen_auto_expressions.c >nul
if errorlevel 1 (
    echo cgen_auto_expressions.c missing auto float expression inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_auto_expressions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_auto_expressions.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_auto_inference_matrix.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_auto_inference_matrix.xl --emit release\tmp\cgen_auto_inference_matrix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_auto_inference_matrix.c'; $checks=@('long long literalInt =','double literalFloat =','int literalBool =','const char* literalString =','xvalue dynamicArray =','xvalue dynamicList =','xvalue dynamicDict =','long long typedArrayItem = (*(long long*)xrtTypedArrayGet(typedArray','const char* typedListItem = (*(const char**)xrtTypedListGet(typedList','long long typedDictItem = (*(long long*)xrtTypedDictGet(typedDict','xvalue missingTypedItem = xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedArrayGet(typedArray','xvalue dynamicArrayItem = xl6_xrt_retain(xvoArrayGetValue(dynamicArray','xvalue dynamicListItem = xl6_xrt_retain(xvoListGetValue(dynamicList','xvalue dynamicDictItem = xl6_xrt_retain(xvoTableGetValue(dynamicDict','XL_Class_MatrixBox box = {0};','box = makeBox(7LL,','long long fieldValue = box.value;','const char* fieldText = box.label;','long long methodValue = xl_class_MatrixBox__getValue(&box);','XL6_Function fn = makeAdder(10LL);','long long fnResult =','long long callResult = add(fieldValue, methodValue);','xfuture* futureValue = doubleAsync(6LL);','long long awaitedValue =','xchannel autoCh = xrtChannelCreate(1LL);','int sent = xl6_channel_send_value(autoCh','int received = xl6_channel_recv_into(autoCh'); foreach($p in $checks){ if($t.IndexOf($p) -lt 0){ Write-Host ('cgen_auto_inference_matrix.c missing '+$p); exit 1 } }"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_auto_inference_matrix.xl test\cases\cgen_class_host.c -o release\tmp\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_auto_inference_matrix.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_var_values.xl --emit release\tmp\cgen_var_values.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue empty = xvoCreateNull();" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var null initialization.
    exit /b 1
)
findstr /C:"xvalue number = xvoCreateInt((long long)(123LL));" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing int var boxing.
    exit /b 1
)
findstr /C:"xvalue moment = xvoCreateTime((long long)(xrtStrToTime(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing time var boxing.
    exit /b 1
)
findstr /C:"xvalue __xl6_assign_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var assignment ownership handoff.
    exit /b 1
)
findstr /C:"xvoCreateTime((long long)(xrtStrToTime(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing datetime vararg boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic var type helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_equal(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value equality helper.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_order(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value ordering helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_binary(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic helper.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_bin_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value arithmetic cleanup.
    exit /b 1
)
findstr /C:"xvalue __xl6_unary_value_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic unary temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_unary(__xl6_unary_value_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic unary helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_truthy(__xl6_not_value_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic truthy helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_logic_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical temporaries.
    exit /b 1
)
findstr /C:"__xl6_logic_right_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic logical right operand path.
    exit /b 1
)
findstr /C:"xvalue __xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate temporaries.
    exit /b 1
)
findstr /C:"xl6_xrt_basic_approx(__xl6_approx_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic approximate helper.
    exit /b 1
)
findstr /C:"xvalue __xl6_cond_result_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic conditional result path.
    exit /b 1
)
findstr /C:"xvalue __xl6_truth_value_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic condition truthy path.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_cmp_left_" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing dynamic value compare cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(number);" release\tmp\cgen_var_values.c >nul
if errorlevel 1 (
    echo cgen_var_values.c missing var cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_var_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_var_values.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_var_values.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_var_runtime_objects.xl --emit release\tmp\cgen_var_runtime_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xvoCreateCallable(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable xvalue boxing.
    exit /b 1
)
findstr /C:"xrtCallableCreate(fn.sig" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable signature binding.
    exit /b 1
)
findstr /C:"box->fn.invoke_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing function xcall dispatch.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 1u" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing multi-return xcall result.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 4u" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing overflow multi-return xcall result.
    exit /b 1
)
findstr /C:"xl6_fnvalue_pairDefault_arity1_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional multi-return adapter xcall.
    exit /b 1
)
findstr /C:"xl6_fnvalue_totalDefault_arity0_vararg_xcall" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional vararg adapter xcall.
    exit /b 1
)
findstr /C:"xrtFuncSigTypeId(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing precise function type query.
    exit /b 1
)
findstr /C:"xvoCreateRecord(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record boxing.
    exit /b 1
)
findstr /C:"xvoCreateHandle(" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing custom handle boxing.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Class_Pair" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record type ops.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Custom_Native" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing custom handle type ops.
    exit /b 1
)
findstr /C:"xl6_var_call(boxedFn" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var function call.
    exit /b 1
)
findstr /C:"xl6_var_call_kwargs(boxedOptions" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var kwargs call.
    exit /b 1
)
findstr /C:"frame.Kwargs = kwargs" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing kwargs call frame binding.
    exit /b 1
)
findstr /C:"xvoTableRemove(__xl6_vararg, \"path\", 0)" release\tmp\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing fixed named parameter filtering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_var_runtime_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_type_conversions.xl --emit release\tmp\cgen_type_conversions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrToI64(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to int conversion.
    exit /b 1
)
findstr /C:"xrtStrToNum(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to float conversion.
    exit /b 1
)
findstr /C:"xrtStrToTime(" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to time conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing int to string helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(boxedInt)" release\tmp\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing var to string conversion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_type_conversions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_type_conversions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_type_conversions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_implicit_methods.xl --emit release\tmp\cgen_implicit_methods.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_value_to_string_release" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing implicit toString lowering.
    exit /b 1
)
findstr /C:"xvoToString(value, 0)" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing XRT xvoToString bridge.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(123LL))" release\tmp\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing literal toString boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_implicit_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure.xl --emit release\tmp\cgen_closure.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function runtime.
    exit /b 1
)
findstr /C:"typedef struct XL6_Closure_" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure environment.
    exit /b 1
)
findstr /C:"void (*retain)(void* env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function retain field.
    exit /b 1
)
findstr /C:"__env->ref_count = 1;" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure ref count init.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_closure.c >nul
if not errorlevel 1 (
    echo cgen_closure.c still contains CRT malloc.
    exit /b 1
)
findstr /C:"free(" release\tmp\cgen_closure.c >nul
if not errorlevel 1 (
    echo cgen_closure.c still contains CRT free.
    exit /b 1
)
findstr /C:"copy.retain(copy.env);" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function copy retain.
    exit /b 1
)
findstr /C:"long long value = ((add10.signature == " release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function signature guard.
    exit /b 1
)
findstr /C:"((long long (*)(void*, long long))add10.invoke)(add10.env, 5LL)" release\tmp\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function value invoke.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_closure.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_closure.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_closure.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure_resource_capture.xl --emit release\tmp\cgen_closure_resource_capture.c
if errorlevel 1 exit /b 1
findstr /C:"__env->values = xl6_xrt_retain(values);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture retain.
    exit /b 1
)
findstr /C:"xvoUnref(__env->values);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture release.
    exit /b 1
)
findstr /C:"__env->inner.retain(__env->inner.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture retain.
    exit /b 1
)
findstr /C:"__env->inner.release(__env->inner.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture release.
    exit /b 1
)
findstr /C:"current.release(current.env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function assignment release.
    exit /b 1
)
findstr /C:".retain(__xl6_function_assign_" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing borrowed function assignment retain.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" release\tmp\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_closure_resource_capture.c >nul
if not errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT malloc.
    exit /b 1
)
findstr /C:"free(" release\tmp\cgen_closure_resource_capture.c >nul
if not errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT free.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_closure_resource_capture.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_bare_call.xl --emit release\tmp\cgen_function_bare_call.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue (*invoke_var)(void* env, int argc, xvalue* argv);" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function invoke slot.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var(XL6_Function fn, int argc, ...)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function call helper.
    exit /b 1
)
findstr /C:"xvoType(__xl6_argv[0]) != XL6_XVO_DT_INT" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function argument type check.
    exit /b 1
)
findstr /C:"xvalue value = xl6_function_call_var(add, 2" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing bare function call lowering.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg arity check.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)" release\tmp\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg packing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_bare_call.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_named_value.xl --emit release\tmp\cgen_function_named_value.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_fnvalue_add_invoke(void* __xl6_env, long long left, long long right)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function typed wrapper.
    exit /b 1
)
findstr /C:"static xvalue xl6_fnvalue_add_invoke_var(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function erased wrapper.
    exit /b 1
)
findstr /C:"XL6_Function typed = xl6_fnvalue_add_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function value init.
    exit /b 1
)
findstr /C:"XL6_Function erased = xl6_fnvalue_add_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing bare function value init.
    exit /b 1
)
findstr /C:"XL6_Function oneDefault = xl6_fnvalue_addDefault_arity1_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional parameter function adapter.
    exit /b 1
)
findstr /C:"XL6_Function variadicDefault = xl6_fnvalue_totalDefault_arity0_vararg_make();" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional vararg function adapter.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1 || __xl6_argc > 2)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing erased optional arity check.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function array vararg lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function dict vararg lowering.
    exit /b 1
)
findstr /C:"copy = (char*)xrtMalloc(len + 1u);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for function fallback string.
    exit /b 1
)
findstr /C:"copy = (char*)malloc(len + 1u);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still uses CRT malloc for function fallback string.
    exit /b 1
)
findstr /C:"box = (XL6_Function_Callable_Env*)xrtMalloc(sizeof(XL6_Function_Callable_Env));" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for callable env.
    exit /b 1
)
findstr /C:"xrtFree(box);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for callable env.
    exit /b 1
)
findstr /C:"argv = (xvalue*)xrtMalloc(sizeof(xvalue) * (size_t)argc);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for erased call argv.
    exit /b 1
)
findstr /C:"xrtFree(argv);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for erased call argv.
    exit /b 1
)
findstr /C:"malloc(" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT malloc.
    exit /b 1
)
findstr /C:"free(" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT free.
    exit /b 1
)
findstr /C:"static void xl6_function_fatal(int code)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal helper.
    exit /b 1
)
findstr /C:"static void xl6_runtime_fatal(int code)" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized runtime fatal helper.
    exit /b 1
)
findstr /C:"xl6_function_fatal(3);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal call.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(code);" release\tmp\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function fatal delegation.
    exit /b 1
)
findstr /C:"exit(3);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" release\tmp\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_named_value.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_named_value.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_named_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_function_value.xl --emit release\tmp\cgen_extern_function_value.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_fnvalue_cadd_invoke(void* __xl6_env, long long left, long long right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function typed wrapper.
    exit /b 1
)
findstr /C:"native_add_i64(left, right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing native extern target call.
    exit /b 1
)
findstr /C:"xl6_fnvalue_cadd_xcall" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing extern function xcall adapter.
    exit /b 1
)
findstr /C:"xl6_export_exportedAdd(left, right)" release\tmp\cgen_extern_function_value.c >nul
if errorlevel 1 (
    echo cgen_extern_function_value.c missing export function internal target call.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_extern_function_value.c test\cases\cgen_extern_function_value_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_extern_function_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_multi_return.xl --emit release\tmp\cgen_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function_Return_" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing function multi-return struct.
    exit /b 1
)
findstr /C:"XL6_Function pairFn = xl6_fnvalue_pair_make();" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return named function value.
    exit /b 1
)
findstr /C:"XL6_Function defaultFn = xl6_fnvalue_pairDefault_arity1_make();" release\tmp\cgen_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_function_multi_return.c missing multi-return optional adapter.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_member_function_multi_return.xl --emit release\tmp\cgen_member_function_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"XL6_Function __xl6_member_fn_multi_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return function lowering.
    exit /b 1
)
findstr /C:"XL6_Function_Return_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing member multi-return struct.
    exit /b 1
)
findstr /C:".op.retain != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing class return function-field retain.
    exit /b 1
)
findstr /C:"__xl6_class_assign_" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing borrowed class assignment temporary.
    exit /b 1
)
findstr /C:"returned.op.retain" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for class rvalue initialization.
    exit /b 1
)
findstr /C:"assignedBox.op.retain" release\tmp\cgen_member_function_multi_return.c >nul
if not errorlevel 1 (
    echo cgen_member_function_multi_return.c should not retain function fields for multi-return rvalue assignment.
    exit /b 1
)
findstr /C:"assignedBox.op.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing class multi-assignment release.
    exit /b 1
)
findstr /C:"assignedOp.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing function multi-assignment release.
    exit /b 1
)
findstr /C:".value0.op.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded class multi-return release.
    exit /b 1
)
findstr /C:".value1.release != 0" release\tmp\cgen_member_function_multi_return.c >nul
if errorlevel 1 (
    echo cgen_member_function_multi_return.c missing discarded function multi-return release.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_member_function_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_member_function_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_default.xl --emit release\tmp\cgen_function_default.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" release\tmp\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function runtime.
    exit /b 1
)
findstr /C:"XL6_Function fn = {0};" release\tmp\cgen_function_default.c >nul
if errorlevel 1 (
    echo cgen_function_default.c missing function default init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_default.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_default.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_default.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_adapter_dedupe.xl --emit release\tmp\cgen_function_adapter_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_function_adapter_dedupe.c' -Raw; if(([regex]::Matches($t, 'static XL6_Function xl6_fnvalue_addDefault_arity1_make\(void\)\s*\{')).Count -ne 1){exit 1}; if(([regex]::Matches($t, 'static long long xl6_fnvalue_addDefault_arity1_invoke\(')).Count -ne 1){exit 2}"
if errorlevel 1 (
    echo cgen_function_adapter_dedupe.c duplicated default-arg adapter.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_function_adapter_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_function_adapter_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\call_defaults.xl --emit release\tmp\call_defaults.c
if errorlevel 1 exit /b 1
findstr /C:"long long first = add(1LL, 1LL, 2LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing default arguments.
    exit /b 1
)
findstr /C:"long long second = add(1LL, 1LL, 4LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing named argument reorder.
    exit /b 1
)
findstr /C:"long long third = add(1LL, 2LL, 3LL);" release\tmp\call_defaults.c >nul
if errorlevel 1 (
    echo call_defaults.c missing all named arguments.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\call_defaults.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\call_defaults.exe
if errorlevel 1 exit /b 1
release\tmp\call_defaults.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range.xl --emit release\tmp\cgen_range.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" release\tmp\cgen_range.c >nul
if errorlevel 1 (
    echo cgen_range.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_assert.xl --emit release\tmp\cgen_range_assert.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_range_step_" release\tmp\cgen_range_assert.c >nul
if errorlevel 1 (
    echo cgen_range_assert.c missing range lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range_assert.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range_assert.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range_assert.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_range_step_runtime_zero.xl --emit release\tmp\cgen_range_step_runtime_zero.c
if errorlevel 1 exit /b 1
findstr /C:"RangeError" release\tmp\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing range step runtime error guard.
    exit /b 1
)
findstr /C:"xl6_error_print_uncaught" release\tmp\cgen_range_step_runtime_zero.c >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.c missing uncaught runtime error printer.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_range_step_runtime_zero.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_range_step_runtime_zero.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_range_step_runtime_zero.exe >nul 2> release\tmp\cgen_range_step_runtime_zero.err.txt
if not errorlevel 1 (
    echo cgen_range_step_runtime_zero.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught RangeError: range step cannot be 0" release\tmp\cgen_range_step_runtime_zero.err.txt >nul
if errorlevel 1 (
    echo cgen_range_step_runtime_zero.err.txt missing uncaught RangeError diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_in_range.xl --emit release\tmp\cgen_in_range.c
if errorlevel 1 exit /b 1
findstr /C:"static int xl6_range_contains_i64" release\tmp\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains helper.
    exit /b 1
)
findstr /C:"expect_i64(xl6_range_contains_i64(3LL, 0, 5LL, 1), 1LL);" release\tmp\cgen_in_range.c >nul
if errorlevel 1 (
    echo cgen_in_range.c missing range contains call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_in_range.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_in_range.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_in_range.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_in_containers.xl --emit release\tmp\cgen_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtFindStr(text, 0, \"pha\", 0, false) != 0" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing string in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_array_contains(values" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing array in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_list_contains(sparse" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing list in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_contains(unique" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing set in lowering.
    exit /b 1
)
findstr /C:"xvoTableExists(table, \"a\", 0)" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing dict in lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_value_contains(dynArray" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing dynamic var in lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container for in lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_in_values_" release\tmp\cgen_in_containers.c >nul
if errorlevel 1 (
    echo cgen_in_containers.c missing temporary RHS container release.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_in_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_for_in_containers.xl --emit release\tmp\cgen_for_in_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArrayItemCount(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing array for-in lowering.
    exit /b 1
)
findstr /C:"xvoGetList(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing list for-in lowering.
    exit /b 1
)
findstr /C:"xvoSetGetValueAt(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing set for-in lowering.
    exit /b 1
)
findstr /C:"xvoGetTable(__xl6_iter_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing dict for-in lowering.
    exit /b 1
)
findstr /C:"xrtAVLTB_IterEnd(__xl6_avl_" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing AVL iterator cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_for_in_containers.c'; $list=[regex]::Matches($text,'xavltbase (__xl6_avl_\d+) = \(xavltbase\)xvoGetList\(__xl6_iter_\d+\);[\s\S]*?xrtAVLTB_IterBegin\(\1\);[\s\S]*?xrtAVLTB_IterNext\(\1\)[\s\S]*?xrtAVLTB_IterEnd\(\1\);'); $dict=[regex]::Matches($text,'xavltbase (__xl6_avl_\d+) = \(xavltbase\)xvoGetTable\(__xl6_iter_\d+\);[\s\S]*?xrtAVLTB_IterBegin\(\1\);[\s\S]*?xrtAVLTB_IterNext\(\1\)[\s\S]*?xrtAVLTB_IterEnd\(\1\);'); if ($list.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing list AVL begin/next/end cleanup sequence.'; exit 1 }; if ($dict.Count -lt 1) { Write-Host 'cgen_for_in_containers.c missing dict AVL begin/next/end cleanup sequence.'; exit 1 }"
if errorlevel 1 exit /b 1
findstr /C:"xrtStrComp(xl6_xrt_value_to_string(key), \"b\", 0, false)" release\tmp\cgen_for_in_containers.c >nul
if errorlevel 1 (
    echo cgen_for_in_containers.c missing var-to-string comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_for_in_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_for_in_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_approx.xl --emit release\tmp\cgen_approx.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtIntApprox(long long a, long long b);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing xrtIntApprox prototype.
    exit /b 1
)
findstr /C:"expect_i64(xrtIntApprox(a, b), 1LL);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing int approx call.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(x, y), 1LL);" release\tmp\cgen_approx.c >nul
if errorlevel 1 (
    echo cgen_approx.c missing num approx call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_approx.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_approx.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_approx.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_math.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_math.xl --emit release\tmp\stdlib_math.c
if errorlevel 1 exit /b 1
findstr /C:"double sqrt(double x);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing sqrt extern prototype.
    exit /b 1
)
findstr /C:"double __xl_std_math_round(double x);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper.
    exit /b 1
)
findstr /C:"expect_i64(xrtNumApprox(__xl_std_math_round(1.6), 2.0), 1LL);" release\tmp\stdlib_math.c >nul
if errorlevel 1 (
    echo stdlib_math.c missing round helper call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_math.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_math.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_math.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_string.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_string.xl --emit release\tmp\stdlib_string.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing strlen c_size extern prototype.
    exit /b 1
)
findstr /C:"int xrtStrComp" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrComp extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_string_compare" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing compare helper.
    exit /b 1
)
findstr /C:"unsigned int xrtInStr" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtInStr extern prototype.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_starts_with" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string startsWith runtime helper.
    exit /b 1
)
findstr /C:"bool xl6_xrt_string_ends_with" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string endsWith runtime helper.
    exit /b 1
)
findstr /C:"void* xrtCheckStr" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtCheckStr extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_string_has_any" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.hasAny helper.
    exit /b 1
)
findstr /C:"double xrtStrSim" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtStrSim extern prototype.
    exit /b 1
)
findstr /C:"void* xrtIntFormat" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtIntFormat extern prototype.
    exit /b 1
)
findstr /C:"void* xrtNumFormat" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtNumFormat extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_int" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatInt helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_string_format_float" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.formatFloat helper.
    exit /b 1
)
findstr /C:"char** xrtSplit" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing xrtSplit extern prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_string_split" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string split runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_string_split" release\tmp\stdlib_string.c >nul
if errorlevel 1 (
    echo stdlib_string.c missing string.split helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_string.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_string.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_string.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time.xl --emit release\tmp\stdlib_time.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtDateAdd" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtDateAdd extern prototype.
    exit /b 1
)
findstr /C:"long long __xl_std_time_from_datetime" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing fromDateTime helper.
    exit /b 1
)
findstr /C:"int64_t xrtStrToTime" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtStrToTime extern prototype.
    exit /b 1
)
findstr /C:"void* xrtTimeFormat" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeFormat extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtTimeParse" release\tmp\stdlib_time.c >nul
if errorlevel 1 (
    echo stdlib_time.c missing xrtTimeParse extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_extra.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_extra.xl --emit release\tmp\stdlib_time_extra.c
if errorlevel 1 exit /b 1
findstr /C:"double xrtTimer" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimer extern prototype.
    exit /b 1
)
findstr /C:"void xrtSleep" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtSleep extern prototype.
    exit /b 1
)
findstr /C:"bool xrtTimeRangeOverlap" release\tmp\stdlib_time_extra.c >nul
if errorlevel 1 (
    echo stdlib_time_extra.c missing xrtTimeRangeOverlap extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time_extra.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time_extra.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time_extra.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_time_calendar.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_time_calendar.xl --emit release\tmp\stdlib_time_calendar.c
if errorlevel 1 exit /b 1
findstr /C:"int64_t xrtToUnixTime" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtToUnixTime extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFirstDayOfMonth" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtFirstDayOfMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtWeekOfYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtWeekOfYear extern prototype.
    exit /b 1
)
findstr /C:"bool xrtIsLeapYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtIsLeapYear extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInMonth" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInMonth extern prototype.
    exit /b 1
)
findstr /C:"int xrtDaysInYear" release\tmp\stdlib_time_calendar.c >nul
if errorlevel 1 (
    echo stdlib_time_calendar.c missing xrtDaysInYear extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_time_calendar.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_time_calendar.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_random.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_random.xl --emit release\tmp\stdlib_random.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned int xrtRand32" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRand32 extern prototype.
    exit /b 1
)
findstr /C:"int xrtRandRange" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandRange extern prototype.
    exit /b 1
)
findstr /C:"void* xrtRandStr" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing xrtRandStr extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_random_string" release\tmp\stdlib_random.c >nul
if errorlevel 1 (
    echo stdlib_random.c missing random.string helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_random.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_random.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_random.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_path_file.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_path_file.xl --emit release\tmp\stdlib_path_file.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPathExists" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathExists extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathRandom" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathRandom extern prototype.
    exit /b 1
)
findstr /C:"void* xrtPathJoin" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtPathJoin extern prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_path_join" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join runtime helper.
    exit /b 1
)
findstr /C:"const char* __xl_std_path_join" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing path.join helper.
    exit /b 1
)
findstr /C:"int xrtDirDelete" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirDelete extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirCopy" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirCopy extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirMove" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirMove extern prototype.
    exit /b 1
)
findstr /C:"int __xl_std_dir_delete" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.delete helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_copy" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.copy helper.
    exit /b 1
)
findstr /C:"int __xl_std_dir_move" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.move helper.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xl6_xrt_dir_list" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.list runtime helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_files" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.files helper.
    exit /b 1
)
findstr /C:"xvalue __xl_std_dir_dirs" release\tmp\stdlib_path_file.c >nul
if errorlevel 1 (
    echo stdlib_path_file.c missing dir.dirs helper.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_path_file.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_path_file.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_path_file.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_file_meta.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_file_meta.xl --emit release\tmp\stdlib_file_meta.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtFileSetSize" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileSetSize extern prototype.
    exit /b 1
)
findstr /C:"int xrtFileGetAttr" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAttr extern prototype.
    exit /b 1
)
findstr /C:"int64_t xrtFileGetAccessTime" release\tmp\stdlib_file_meta.c >nul
if errorlevel 1 (
    echo stdlib_file_meta.c missing xrtFileGetAccessTime extern prototype.
    exit /b 1
)
"%XL6_TCC%" -DXL6_USE_XRT_LIFECYCLE %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_file_meta.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_file_meta.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_file_meta.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\control_flow_do_while.xl --emit release\tmp\control_flow_do_while.c
if errorlevel 1 exit /b 1
findstr /C:"do {" release\tmp\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing do loop.
    exit /b 1
)
findstr /C:"} while ((value < 3LL));" release\tmp\control_flow_do_while.c >nul
if errorlevel 1 (
    echo control_flow_do_while.c missing while condition.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\control_flow_do_while.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\control_flow_do_while.exe
if errorlevel 1 exit /b 1
release\tmp\control_flow_do_while.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_levels.xl --emit release\tmp\cgen_loop_levels.c
if errorlevel 1 exit /b 1
findstr /C:"goto __xl6_break_" release\tmp\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level break jump.
    exit /b 1
)
findstr /C:"goto __xl6_continue_" release\tmp\cgen_loop_levels.c >nul
if errorlevel 1 (
    echo cgen_loop_levels.c missing multi-level continue jump.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_loop_levels.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_loop_levels.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_loop_levels.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_named_args.xl --emit release\tmp\cgen_named_args.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(add(1LL, 1LL, 2LL), 4LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing default argument lowering.
    exit /b 1
)
findstr /C:"expect_i64(add(1LL, 1LL, 4LL), 6LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named default lowering.
    exit /b 1
)
findstr /C:"xl_class_Box__create(&box, 6LL, 4LL);" release\tmp\cgen_named_args.c >nul
if errorlevel 1 (
    echo cgen_named_args.c missing named class constructor lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_named_args.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_named_args.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_named_args.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return.xl --emit release\tmp\cgen_multi_return.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ __xl6_multi_'){exit 2}"
if errorlevel 1 (
    echo cgen_multi_return.c missing signature return struct or temp.
    exit /b 1
)
findstr /C:"left = __xl6_multi_" release\tmp\cgen_multi_return.c >nul
if errorlevel 1 (
    echo cgen_multi_return.c missing multi-assign lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_return_struct_dedupe.xl --emit release\tmp\cgen_multi_return_struct_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_multi_return_struct_dedupe.c' -Raw; if(([regex]::Matches($t, 'typedef struct XL6_Function_Return_[0-9A-F]+')).Count -ne 1){exit 1}; if($t -match 'XL6_Return_leftPair'){exit 2}; if($t -match 'XL6_Return_rightPair'){exit 3}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ leftPair\(void\);')).Count -ne 1){exit 4}; if(([regex]::Matches($t, 'XL6_Function_Return_[0-9A-F]+ rightPair\(void\);')).Count -ne 1){exit 5}"
if errorlevel 1 (
    echo cgen_multi_return_struct_dedupe.c should reuse one signature return struct.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_return_struct_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_return_struct_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_multi_assign_index.xl --emit release\tmp\cgen_multi_assign_index.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArraySetValue(values, (unsigned int)(0LL), __xl6_multi_index_" release\tmp\cgen_multi_assign_index.c >nul
if errorlevel 1 (
    echo cgen_multi_assign_index.c missing index target lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_multi_assign_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_multi_assign_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_varargs.xl --emit release\tmp\cgen_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue arg" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing xvalue arg.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array varargs lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict varargs lowering.
    exit /b 1
)
findstr /C:"xvoArrayItemCount(arg)" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing array length lowering.
    exit /b 1
)
findstr /C:"xvoTableItemCount(arg)" release\tmp\cgen_varargs.c >nul
if errorlevel 1 (
    echo cgen_varargs.c missing dict length lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_varargs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_varargs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_index_access.xl --emit release\tmp\cgen_index_access.c
if errorlevel 1 exit /b 1
findstr /C:"xvoArrayGetValue(values, (unsigned int)(1LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array index lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_values_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing temporary index receiver capture.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_index_values_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing temporary index receiver release.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(xvoArrayGetValue(__xl6_index_values_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing retained index result for temporary receiver.
    exit /b 1
)
findstr /C:"xvoListGetValue(sparse, (long long)(2LL))" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing list index lowering.
    exit /b 1
)
findstr /C:"xvoTableGetValue(table, \"count\", 0)" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict index lowering.
    exit /b 1
)
findstr /C:"xvoTableGetValue(arg, \"retry\", 0)" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict vararg index lowering.
    exit /b 1
)
findstr /C:"xvoArraySetValue(values, (unsigned int)(1LL), __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing array index assignment lowering.
    exit /b 1
)
findstr /C:"xvoListSetValue(sparse, (long long)(2LL), __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing list index assignment lowering.
    exit /b 1
)
findstr /C:"xvoTableSetValue(table, \"count\", 0, __xl6_index_assign_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing dict index assignment lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index compound current temp.
    exit /b 1
)
findstr /C:"xvoCreateInt(xvoGetInt(__xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index compound int lowering.
    exit /b 1
)
findstr /C:"nc_probe_next_var()" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing var-returning compound RHS probe.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_conv_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing compound RHS temporary xvalue release.
    exit /b 1
)
findstr /C:"if (xvoIsNull(__xl6_index_current_" release\tmp\cgen_index_access.c >nul
if errorlevel 1 (
    echo cgen_index_access.c missing index null assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_index_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_index_access.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_index_access.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_index.xl --emit release\tmp\cgen_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_index_safe_get_i64(values, (long long)(1LL))" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_key(table, \"count\")" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing dict safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_safe_get_i64(missing, (long long)(0LL))" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c missing null safe index lowering.
    exit /b 1
)
findstr /C:"return xvoArrayGetValue(0, 0u);" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c should return borrowed null for numeric safe index misses.
    exit /b 1
)
findstr /C:"return xvoTableGetValue(0, key, 0);" release\tmp\cgen_safe_index.c >nul
if errorlevel 1 (
    echo cgen_safe_index.c should return borrowed null for keyed safe index misses.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_safe_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index.xl --emit release\tmp\cgen_typed_container_safe_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedArrayGet(values" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed array safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed list safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeValue = xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedArrayGet(values" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed array safe index as xvalue.
    exit /b 1
)
findstr /C:"xvalue missingName = xl6_xrt_typed_box_or_null(xrtTypeString(), xrtTypedListGet(names" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed list safe index as xvalue.
    exit /b 1
)
findstr /C:"xvalue missingScore = xl6_xrt_typed_box_or_null(xrtTypeInt(), xrtTypedDictGet(scores" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing auto typed dict safe index as xvalue.
    exit /b 1
)
findstr /C:"xvalue __xl6_conv_" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue conversion lowering.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_conv_" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue conversion release.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string_release(xl6_xrt_typed_box_or_null" release\tmp\cgen_typed_container_safe_index.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index.c missing temporary xvalue string conversion release.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_safe_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_safe_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_safe_index_objects_functions.xl --emit release\tmp\cgen_typed_container_safe_index_objects_functions.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue maybePair = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Pair(), xrtTypedArrayGet(pairs" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybePayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedArrayGet(payloads" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Custom_Native(), xrtTypedArrayGet(natives" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing custom typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeFn = xl6_xrt_typed_box_or_null(xrtTypeFunction(), xrtTypedArrayGet(funcs" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed array safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListPair = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Class_Pair(), xrtTypedListGet(pairList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListPayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedListGet(payloadList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Custom_Native(), xrtTypedListGet(nativeList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing custom typed list safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeListFn = xl6_xrt_typed_box_or_null(xrtTypeFunction(), xrtTypedListGet(funcList" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed list safe index lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictGet(pairDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing class typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeDictPayload = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Union_Payload(), xrtTypedDictGet(payloadDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing union typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xvalue maybeDictNative = xl6_xrt_typed_box_or_null(xl6_type_desc_XL_Custom_Native(), xrtTypedDictGet(nativeDict" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing custom typed dict safe index lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictGet(funcDict, xvoGetText(__xl6_dict_key_" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function typed dict dynamic safe index lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeListFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_xrt_type_value_id(maybeDictFn, false) == fnType" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function signature type preservation.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeListFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing list function safe index var call lowering.
    exit /b 1
)
findstr /C:"xl6_var_call(maybeDictFn, 1" release\tmp\cgen_typed_container_safe_index_objects_functions.c >nul
if errorlevel 1 (
    echo cgen_typed_container_safe_index_objects_functions.c missing dict function safe index var call lowering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_typed_container_safe_index_objects_functions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_typed_container_safe_index_objects_functions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_dynamic_member_access.xl --emit release\tmp\cgen_dynamic_member_access.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_index_safe_get_key(obj," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing borrowed dynamic member access.
    exit /b 1
)
findstr /C:"xvalue __xl6_member_values_" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing temporary dynamic member receiver capture.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(xl6_xrt_index_safe_get_key(__xl6_member_values_" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing temporary dynamic member retain.
    exit /b 1
)
findstr /C:"xvalue __xl6_member_assign_" release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing dynamic member assignment temporary.
    exit /b 1
)
findstr /C:"xvoTableSetValue(obj," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing direct dynamic member assignment lowering.
    exit /b 1
)
findstr /C:"xvoTableSetValue(xl6_xrt_index_safe_get_key(nested," release\tmp\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing nested dynamic member assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_dynamic_member_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_member.xl --emit release\tmp\cgen_safe_member.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_member_safe_length(values)" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing array safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length(missing)" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing null safe member lowering.
    exit /b 1
)
findstr /C:"xvalue __xl6_member_values_" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary safe member receiver capture.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_member_values_" release\tmp\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary safe member receiver release.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_safe_member.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_safe_member.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_safe_member.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_varargs.xl --emit release\tmp\cgen_overload_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_fn_pick__string(\"x\"), 1LL);" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing fixed overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_fn_pick__string__array_u003C_var_u003E_(\"x\", xl6_xrt_array_varargs(1" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing vararg overload call.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(1" release\tmp\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing dict vararg overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overload_varargs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\control_flow_inline_if.xl --emit release\tmp\control_flow_inline_if.c
if errorlevel 1 exit /b 1
findstr /C:"if ((value > 0LL))" release\tmp\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing first inline if.
    exit /b 1
)
findstr /C:"else {" release\tmp\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing inline else.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\control_flow_inline_if.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\control_flow_inline_if.exe
if errorlevel 1 exit /b 1
release\tmp\control_flow_inline_if.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_assign_ops.xl --emit release\tmp\cgen_assign_ops.c
if errorlevel 1 exit /b 1
findstr /C:"a += 2LL;" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing compound assignment.
    exit /b 1
)
findstr /C:"long long selected = (1 ? inverted : 1LL);" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing ternary expression.
    exit /b 1
)
findstr /C:"int logicAnd = (1 && (!0));" release\tmp\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing logical expression.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_assign_ops.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_assign_ops.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_assign_ops.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_null_coalesce.xl --emit release\tmp\cgen_null_coalesce.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue fallback = ({ xvalue __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce lowering.
    exit /b 1
)
findstr /C:"xvalue probed = ({ xvalue __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing single-evaluation null coalesce lowering.
    exit /b 1
)
findstr /C:"long long staticProbed = ({ long long __xl6_nc_" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static single-evaluation null coalesce lowering.
    exit /b 1
)
findstr /C:"xvalue literal = xvoCreateInt((long long)(7LL));" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing literal null coalesce boxing.
    exit /b 1
)
findstr /C:"long long staticInt = 5LL;" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce inference.
    exit /b 1
)
findstr /C:"if (xvoIsNull(assigned))" release\tmp\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_null_coalesce.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_pow.xl --emit release\tmp\cgen_pow.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_int_pow_i64" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing int pow helper.
    exit /b 1
)
findstr /C:"long long rightAssoc = xl6_int_pow_i64(2LL, xl6_int_pow_i64(3LL, 2LL));" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing right-associative pow lowering.
    exit /b 1
)
findstr /C:"value = xl6_int_pow_i64(value, 4LL);" release\tmp\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing pow assignment lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_pow.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_pow.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_pow.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom.xl --emit release\tmp\cgen_custom.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Custom_Counter { void* handle; } XL_Custom_Counter;" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing custom handle struct.
    exit /b 1
)
findstr /C:"counter.handle = counter_open(40LL);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing custom create call.
    exit /b 1
)
findstr /C:"long long afterAdd = counter_add(counter.handle, 2LL);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing custom method call.
    exit /b 1
)
findstr /C:"counter_close(counter.handle);" release\tmp\cgen_custom.c >nul
if errorlevel 1 (
    echo cgen_custom.c missing custom destroy call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom.c test\cases\cgen_custom_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_helper.xl --emit release\tmp\cgen_custom_helper.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_custom_Counter__plus(XL_Custom_Counter* self, long long extra);" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing custom helper prototype.
    exit /b 1
)
findstr /C:"counter_value(self->handle)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing custom helper self handle call.
    exit /b 1
)
findstr /C:"xl_custom_Counter__isZero(&counter)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing custom helper call.
    exit /b 1
)
findstr /C:"xl_custom_Counter__plus(&counter, 7LL)" release\tmp\cgen_custom_helper.c >nul
if errorlevel 1 (
    echo cgen_custom_helper.c missing custom helper call args.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_helper.c test\cases\cgen_custom_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_helper.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_helper.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_bind_point.xl --emit release\tmp\cgen_custom_bind_point.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = make_borrowed_box(55LL);" release\tmp\cgen_custom_bind_point.c >nul
if errorlevel 1 (
    echo cgen_custom_bind_point.c missing point binding.
    exit /b 1
)
findstr /C:"borrowed_box_close(box.handle);" release\tmp\cgen_custom_bind_point.c >nul
if errorlevel 1 (
    echo cgen_custom_bind_point.c missing borrowed destroy.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_bind_point.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_bind_point.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_bind_point.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_ownership.xl --emit release\tmp\cgen_custom_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = make_borrowed_box(66LL);" release\tmp\cgen_custom_ownership.c >nul
if errorlevel 1 (
    echo cgen_custom_ownership.c missing create-only custom construction.
    exit /b 1
)
findstr /C:"box.handle = handle;" release\tmp\cgen_custom_ownership.c >nul
if errorlevel 1 (
    echo cgen_custom_ownership.c missing view-only point binding.
    exit /b 1
)
findstr /C:"borrowed_box_close" release\tmp\cgen_custom_ownership.c >nul
if not errorlevel 1 (
    echo cgen_custom_ownership.c should not emit destroy without destroy mapping.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_ownership.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_ownership.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_null_destroy.xl --emit release\tmp\cgen_custom_null_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"if (box.handle != 0)" release\tmp\cgen_custom_null_destroy.c >nul
if errorlevel 1 (
    echo cgen_custom_null_destroy.c missing null-handle destroy guard.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_null_destroy.c test\cases\cgen_custom_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_null_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_null_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class.xl --emit release\tmp\cgen_class.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point {" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing class struct.
    exit /b 1
)
findstr /C:"XL_Class_Point base;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing base field.
    exit /b 1
)
findstr /C:"p.x = 3LL;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing field default.
    exit /b 1
)
findstr /C:"q.base.x = 10LL;" release\tmp\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing inherited field assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create.xl --emit release\tmp\cgen_base_create.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create(&self->base, seed);" release\tmp\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing explicit base create call.
    exit /b 1
)
findstr /C:"xl_class_AutoBase__create(&autoChild.base);" release\tmp\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing auto base create call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_base_create.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_base_create.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_base_create.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create_overloads.xl --emit release\tmp\cgen_base_create_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create__void(&child.base);" release\tmp\cgen_base_create_overloads.c >nul
if errorlevel 1 (
    echo cgen_base_create_overloads.c missing auto no-arg base create overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_base_create_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_methods.xl --emit release\tmp\cgen_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Point__create(XL_Class_Point* self, long long x, long long y);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Point__length2(XL_Class_Point* self);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method prototype.
    exit /b 1
)
findstr /C:"xl_class_Point__create(&p, 3LL, 4LL);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Point__length2(&p);" release\tmp\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_overloads.xl --emit release\tmp\cgen_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Pair__create__void(XL_Class_Pair* self);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing no-arg create overload symbol.
    exit /b 1
)
findstr /C:"void xl_class_Pair__create__int(XL_Class_Pair* self, long long value);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing int create overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__int(XL_Class_Ops* self, long long value);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class int method overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__string(XL_Class_Ops* self, const char* text);" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class string method overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Base__pick(&child.base, 7LL)" release\tmp\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing inherited method base receiver path.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\cgen_unicode_identifiers.xl --emit release\tmp\cgen_unicode_identifiers.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"symbol: xl_fn__u52A0__u4E00___int" release\tmp\cgen_unicode_identifiers.abi.txt >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.abi.txt missing encoded function symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_unicode_identifiers.xl --emit release\tmp\cgen_unicode_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class__u70B9_ {" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class type.
    exit /b 1
)
findstr /C:"long long _u6A2A_;" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class field.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00___int(" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function symbol.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00___int(long long _u6570__u503C_)" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function parameter.
    exit /b 1
)
findstr /C:"long long xl_class__u70B9____u6C42__u548C_(XL_Class__u70B9_* self);" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class method symbol.
    exit /b 1
)
findstr /C:"long long _u7ED3__u679C_ = xl_fn__u52A0__u4E00___int(41LL);" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded local variable.
    exit /b 1
)
findstr /C:"XL_Class__u70B9_ _u5750__u6807_ = {0};" release\tmp\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class variable.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_unicode_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_destroy.xl --emit release\tmp\cgen_class_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Tracker__destroy(XL_Class_Tracker* self);" release\tmp\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_Tracker__destroy(&tracker);" release\tmp\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing scope destroy call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_cleanup_order.xl --emit release\tmp\cgen_cleanup_order.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_cleanup_order.c'; $second=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&second);' | Select-Object -First 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'xl_class_Tracker__destroy(&first);' | Select-Object -First 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_cleanup_order.c should destroy later local before earlier local.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_cleanup_order.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_inherit_destroy.xl --emit release\tmp\cgen_class_inherit_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_AutoChild__destroy(XL_Class_AutoChild* self);" release\tmp\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing inherited auto destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_Base__destroy(&self->base);" release\tmp\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing base destroy tail call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_class_inherit_destroy.c'; $body=($lines | Select-String -SimpleMatch 'record_destroy(self->value);' | Select-Object -First 1).LineNumber; $base=($lines | Select-String -SimpleMatch 'xl_class_Base__destroy(&self->base);' | Select-Object -First 1).LineNumber; if(-not $body -or -not $base -or $body -ge $base){exit 1}"
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c should run child destroy body before base destroy tail call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_inherit_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_custom_field.xl --emit release\tmp\cgen_class_custom_field.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Holder__destroy(XL_Class_Holder* self);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing auto destroy prototype.
    exit /b 1
)
findstr /C:"self->first.handle = native_box_open(10LL);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing first custom field create.
    exit /b 1
)
findstr /C:"native_box_close(self->second.handle);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing second custom field cleanup.
    exit /b 1
)
findstr /C:"native_box_close(self->first.handle);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing first custom field cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'release\tmp\cgen_class_custom_field.c'; $second=($lines | Select-String -SimpleMatch 'native_box_close(self->second.handle);' | Select-Object -First 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'native_box_close(self->first.handle);' | Select-Object -First 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_class_custom_field.c should destroy custom fields in reverse declaration order.
    exit /b 1
)
findstr /C:"xl_class_Holder__destroy(&holder);" release\tmp\cgen_class_custom_field.c >nul
if errorlevel 1 (
    echo cgen_class_custom_field.c missing holder scope cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_class_custom_field.c test\cases\cgen_class_custom_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_class_custom_field.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_class_custom_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_custom_field_method.xl --emit release\tmp\cgen_custom_field_method.c
if errorlevel 1 exit /b 1
findstr /C:"long long __xl6_return_0 = native_box_value(self->box.handle);" release\tmp\cgen_custom_field_method.c >nul
if errorlevel 1 (
    echo cgen_custom_field_method.c missing custom field method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_field_method.c test\cases\cgen_class_custom_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_field_method.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_field_method.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_custom_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_custom_overloads.xl --emit release\tmp\cgen_custom_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"score_add_i(score.handle, 5LL)" release\tmp\cgen_custom_overloads.c >nul
if errorlevel 1 (
    echo cgen_custom_overloads.c missing int custom overload call.
    exit /b 1
)
findstr /C:"score_add_s(score.handle" release\tmp\cgen_custom_overloads.c >nul
if errorlevel 1 (
    echo cgen_custom_overloads.c missing string custom overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_custom_overloads.c test\cases\cgen_custom_overloads_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_custom_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_custom_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overloads.xl --emit release\tmp\cgen_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_fn_pick__int(long long value);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing int overload symbol.
    exit /b 1
)
findstr /C:"const char* xl_fn_pick__string(const char* value);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing string overload symbol.
    exit /b 1
)
findstr /C:"long long chosen = xl_fn_pick__int(41LL);" release\tmp\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing resolved overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_costs.xl --emit release\tmp\cgen_overload_costs.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_overload_costs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_overload_costs.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_overload_costs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_priority.xl --emit release\tmp\cgen_generic_overload_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(choose(1LL), 10LL);" release\tmp\cgen_generic_overload_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_priority.c should prefer exact non-generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_overload_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_var_priority.xl --emit release\tmp\cgen_generic_overload_var_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_gen_choose__int(1LL), 20LL);" release\tmp\cgen_generic_overload_var_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_var_priority.c should prefer generic overload over var boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_overload_var_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_explicit_overload.xl --emit release\tmp\cgen_generic_explicit_overload.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_pick__int__sig__int(long long value);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing scalar explicit generic overload instance.
    exit /b 1
)
findstr /C:"long long xl_gen_pick__int__sig__array_of_int(xvalue values);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing array explicit generic overload instance.
    exit /b 1
)
findstr /C:"long long arrayValue = xl_gen_pick__int__sig__array_of_int(" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose array explicit generic overload.
    exit /b 1
)
findstr /C:"long long scalarValue = xl_gen_pick__int__sig__int(1LL);" release\tmp\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose scalar explicit generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_explicit_overload.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_signature_suffix.xl --emit release\tmp\cgen_generic_signature_suffix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'release\tmp\cgen_generic_signature_suffix.c'; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\);', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\)\s*\{', 'Multiline')).Count -ne 1){exit 3}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\)\s*\{', 'Multiline')).Count -ne 1){exit 4}; if($t -notmatch 'long long first = xl_gen_order__int__string__sig__int__string\(1LL, \"x\"\);'){exit 5}; if($t -notmatch 'long long second = xl_gen_order__int__string__sig__string__int\(\"x\", 1LL\);'){exit 6}"
if errorlevel 1 (
    echo cgen_generic_signature_suffix.c missing distinct generic signature suffix instances.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_signature_suffix.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_function.xl --emit release\tmp\cgen_generic_function.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic instance prototype.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic instance prototype.
    exit /b 1
)
findstr /C:"long long number = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic call.
    exit /b 1
)
findstr /C:"const char* text = xl_gen_identity__string(\"ok\");" release\tmp\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_function.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_function.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_function.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_dedupe.xl --emit release\tmp\cgen_generic_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_generic_dedupe.c' -Raw; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\)\s*\{', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, 'xl_gen_identity__int\(')).Count -ne 5){exit 3}"
if errorlevel 1 (
    echo cgen_generic_dedupe.c duplicated or lost generic function instance.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_infer.xl --emit release\tmp\cgen_generic_infer.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int identity instance.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string identity instance.
    exit /b 1
)
findstr /C:"long long xl_gen_choose__int(long long left, long long right);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countArray__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred array generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptList__string(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred list generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptSet__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred set generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countDict__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred dict generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countNested__int(xvalue values);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred nested generic instance.
    exit /b 1
)
findstr /C:"xvalue xl_gen_identity__array_of_dict_of_int(xvalue value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_apply__int(XL6_Function op, long long value);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred function-param generic instance.
    exit /b 1
)
findstr /C:"long long number = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int call.
    exit /b 1
)
findstr /C:"const char* text = xl_gen_identity__string(\"ok\");" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string call.
    exit /b 1
)
findstr /C:"long long chosen = xl_gen_choose__int(1LL, 2LL);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose call.
    exit /b 1
)
findstr /C:"long long copied = xl_gen_identity__int(seed);" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred call from auto variable.
    exit /b 1
)
findstr /C:"xvalue explicitNested = xl6_xrt_bind_type(xl_gen_identity__array_of_dict_of_int(" release\tmp\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_infer.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_infer.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_infer.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_alias.xl --emit release\tmp\cgen_generic_alias.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" release\tmp\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical int generic instance.
    exit /b 1
)
findstr /C:"xl_gen_identity__MyInt" release\tmp\cgen_generic_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_alias.c should not mangle transparent alias into generic symbol.
    exit /b 1
)
findstr /C:"long long explicitAlias = xl_gen_identity__int(41LL);" release\tmp\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical explicit alias call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_alias.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_alias.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_alias.xl --emit release\tmp\cgen_generic_class_alias.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing canonical generic class layout.
    exit /b 1
)
findstr /C:"XL_Class_Box__MyInt" release\tmp\cgen_generic_class_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_alias.c should not mangle transparent alias into class symbol.
    exit /b 1
)
findstr /C:"XL_Class_Box__int aliasBox = {0};" release\tmp\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing canonical alias variable type.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class.xl --emit release\tmp\cgen_generic_class.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int class instance.
    exit /b 1
)
findstr /C:"long long value;" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int field substitution.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Box__string {" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string class instance.
    exit /b 1
)
findstr /C:"const char* value;" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string field substitution.
    exit /b 1
)
findstr /C:"XL_Class_Box__int intBox = {0};" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int variable declaration.
    exit /b 1
)
findstr /C:"XL_Class_Box__string stringBox = {0};" release\tmp\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string variable declaration.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_fields.xl --emit release\tmp\cgen_generic_class_fields.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing int class instance.
    exit /b 1
)
findstr /C:"intBox.value = 123LL;" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field assignment.
    exit /b 1
)
findstr /C:"expect_i64(intBox.value, 123LL);" release\tmp\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field read.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_function_fields.xl --emit release\tmp\cgen_generic_class_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_copy_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type copy helper.
    exit /b 1
)
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type desc helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_box_XL_Class_Mapper__int" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class box helper.
    exit /b 1
)
findstr /C:"XL6_Function __xl6_member_fn_" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic member function call lowering.
    exit /b 1
)
findstr /C:"fromArray.op.retain(fromArray.op.env);" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed index retain.
    exit /b 1
)
findstr /C:"item.op.retain(item.op.env);" release\tmp\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_nested_function_fields.xl --emit release\tmp\cgen_generic_class_nested_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"xl_class_Mapper__destroy" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not call unresolved Mapper destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Mapper__int {" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested Mapper int instance.
    exit /b 1
)
findstr /C:"XL_Class_Mapper__int inner;" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing substituted nested class field.
    exit /b 1
)
findstr /C:"xl_class_Mapper__int__destroy(&self->inner);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing generic nested destroy call.
    exit /b 1
)
findstr /C:"fromArray.inner.op.retain(fromArray.inner.op.env);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed index retain.
    exit /b 1
)
findstr /C:"item.inner.op.retain(item.inner.op.env);" release\tmp\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_nested_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_deep_function_fields.xl --emit release\tmp\cgen_generic_class_deep_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"XL_Class_Middle__T" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Middle generic type.
    exit /b 1
)
findstr /C:"xl_class_Middle__destroy" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not call unresolved Middle destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Middle__int {" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing nested Middle int instance.
    exit /b 1
)
findstr /C:"XL_Class_Mapper__int mapper;" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Mapper field.
    exit /b 1
)
findstr /C:"XL_Class_Middle__int middle;" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Middle field.
    exit /b 1
)
findstr /C:"xl_class_Middle__int__destroy(&self->middle);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Middle destroy call.
    exit /b 1
)
findstr /C:"xl_class_Mapper__int__destroy(&self->mapper);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Mapper destroy call.
    exit /b 1
)
findstr /C:"fromArray.middle.mapper.op.retain(fromArray.middle.mapper.op.env);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed index retain.
    exit /b 1
)
findstr /C:"item.middle.mapper.op.retain(item.middle.mapper.op.env);" release\tmp\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_deep_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_methods.xl --emit release\tmp\cgen_generic_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__valueOf(XL_Class_Box__int* self);" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method prototype.
    exit /b 1
)
findstr /C:"long long __xl6_return_0 = self->value;" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method body.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_auto.xl --emit release\tmp\cgen_generic_class_auto.c
if errorlevel 1 exit /b 1
findstr /C:"long long fromField = intBox.value;" release\tmp\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic field auto inference.
    exit /b 1
)
findstr /C:"long long fromMethod = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic method return auto inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_auto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_overloads.xl --emit release\tmp\cgen_generic_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__pick__int(XL_Class_Box__int* self, long long value);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Box__int__pick__string(XL_Class_Box__int* self, const char* text);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload prototype.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__int(&intBox, 1LL), 10LL);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__string(&intBox, \"x\"), 20LL);" release\tmp\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_ctor.xl --emit release\tmp\cgen_generic_class_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Box__int__create(XL_Class_Box__int* self, long long value);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create prototype.
    exit /b 1
)
findstr /C:"self->value = value;" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create body.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&intBox, 123LL);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class method call after create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_explicit_ctor.xl --emit release\tmp\cgen_generic_class_explicit_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Box__int intBox = {0};" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class variable declaration.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&intBox, 123LL);" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class create call.
    exit /b 1
)
findstr /C:"long long value = xl_class_Box__int__valueOf(&intBox);" release\tmp\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing method call after explicit create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_explicit_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_type_values.xl --emit release\tmp\cgen_generic_class_type_values.c
if errorlevel 1 exit /b 1
findstr /C:"unsigned long long leftType = 0x" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing generic class type value.
    exit /b 1
)
findstr /C:"expect_i64((leftType == rightType), 1LL);" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing equal generic class type comparison.
    exit /b 1
)
findstr /C:"expect_i64((leftType == textType), 0LL);" release\tmp\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing distinct generic class type comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_generic_class_type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_basic.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_basic.xl -o release\tmp\xl6_build_basic.exe
if errorlevel 1 exit /b 1
release\tmp\xl6_build_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_range.xl -o release\tmp\xl6_build_range.exe
if errorlevel 1 exit /b 1
release\tmp\xl6_build_range.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_main_entry.xl -o release\tmp\cgen_main_entry.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_main_entry.exe alpha beta
if not errorlevel 3 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)
if errorlevel 4 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)

release\xl.exe run test\cases\xl_run_args.xl -- alpha "two words" "quote\"value" beta ""
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_main_entry.xl --emit release\tmp\cgen_main_entry.c
if errorlevel 1 exit /b 1
findstr /C:"static xvalue xl6_argv_to_arg(int argc, char** argv)" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv-to-arg helper.
    exit /b 1
)
findstr /C:"XL_InitOptions __xl6_options = xlInitOptions(argc, argv);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlInitOptions call.
    exit /b 1
)
findstr /C:"__xl6_arg = xl6_argv_to_arg(argc, argv);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv-to-arg call.
    exit /b 1
)
findstr /C:"__xl6_code = (int)xl_user_main(__xl6_arg);" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlang main dispatch.
    exit /b 1
)
findstr /C:"xlUnit();" release\tmp\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlUnit call.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_main_uncaught.xl --emit release\tmp\cgen_main_uncaught.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_print_uncaught();" release\tmp\cgen_main_uncaught.c >nul
if errorlevel 1 (
    echo cgen_main_uncaught.c missing uncaught main error printer.
    exit /b 1
)
release\xl.exe run test\cases\cgen_main_uncaught.xl >nul 2> release\tmp\cgen_main_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_main_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: main boom" release\tmp\cgen_main_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_main_uncaught.err.txt missing uncaught main exception diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_main_case_sensitive.xl --emit release\tmp\cgen_main_case_sensitive.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_main_case_sensitive.c' -Raw; if($t -match 'xl_user_main'){exit 1}; if($t -match 'int main\('){exit 2}; if($t -notmatch 'long long Main\(xvalue arg\)'){exit 3}"
if errorlevel 1 (
    echo cgen_main_case_sensitive.c should treat Main as a normal function.
    exit /b 1
)
release\xl.exe xapi test\cases\cgen_main_case_sensitive.xl --emit release\tmp\cgen_main_case_sensitive.xapi.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_case_sensitive.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.kind -ne 'none'){exit 1}"
if errorlevel 1 (
    echo cgen_main_case_sensitive.xapi.json should report entry none.
    exit /b 1
)
release\xl.exe check test\cases\cgen_main_case_sensitive_script.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_main_case_sensitive_script.xl --emit release\tmp\cgen_main_case_sensitive_script.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\cgen_main_case_sensitive_script.c' -Raw; if($t -match 'xl_user_main'){exit 1}; if($t -notmatch 'int main\('){exit 2}; if($t -notmatch 'long long Main\(xvalue arg\)'){exit 3}; if($t -notmatch 'expect_i64\(1LL, 1LL\);'){exit 4}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.c should treat Main as normal function and top-level code as script main.
    exit /b 1
)
release\xl.exe xapi test\cases\cgen_main_case_sensitive_script.xl --emit release\tmp\cgen_main_case_sensitive_script.xapi.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\cgen_main_case_sensitive_script.xapi.json' -Raw | ConvertFrom-Json; if($j.entry.kind -ne 'script-main'){exit 1}; if($j.entry.returnType -ne 'int'){exit 2}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.xapi.json should report script-main.
    exit /b 1
)
release\xl.exe build test\cases\cgen_main_case_sensitive_script.xl test\cases\cgen_class_host.c -o release\tmp\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1

release\xl.exe build examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c -o release\tmp\build_mixed_host.exe
if errorlevel 1 exit /b 1
release\tmp\build_mixed_host.exe
if errorlevel 1 exit /b 1

release\xl.exe help > release\tmp\xl_help.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\xl_help.txt' -SimpleMatch 'xl build')){exit 1}"
if errorlevel 1 (
    echo xl_help.txt missing build usage.
    exit /b 1
)

release\xl.exe xapi test\cases\export_add.xl > release\tmp\xapi_stdout.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\xapi_stdout.json' -SimpleMatch 'xlang-xapi-1')){exit 1}"
if errorlevel 1 (
    echo xapi stdout missing format marker.
    exit /b 1
)

release\xl.exe build test\cases\export_add.xl >nul 2> release\tmp\build_missing_output.err.txt
if not errorlevel 1 (
    echo build without -o should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'release\tmp\build_missing_output.err.txt' -SimpleMatch 'build requires -o')){exit 1}"
if errorlevel 1 (
    echo build missing -o diagnostic missing.
    exit /b 1
)

release\xl.exe build examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c -o release\tmp\build_mixed_two_c.exe
if errorlevel 1 exit /b 1
release\tmp\build_mixed_two_c.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$argsList=@('build','examples\feature_suite\09_c_export_abi.xl'); for($i=0;$i -lt 33;$i++){ $argsList += 'test\cases\build_mixed_extra.c' }; $argsList += @('-o','release\tmp\too_many_c.exe'); $p=Start-Process -FilePath 'release\xl.exe' -ArgumentList $argsList -NoNewWindow -PassThru -Wait -RedirectStandardOutput 'release\tmp\build_too_many_c.out.txt' -RedirectStandardError 'release\tmp\build_too_many_c.err.txt'; if($p.ExitCode -eq 0){exit 1}; if(-not (Select-String -Path 'release\tmp\build_too_many_c.err.txt' -SimpleMatch 'too many C source files')){exit 2}"
if errorlevel 1 (
    echo build too many C source diagnostic missing.
    exit /b 1
)

release\xl.exe build examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c --save-temps -o release\tmp\build_mixed_save_temps.exe > release\tmp\build_mixed_save_temps.log 2>&1
if errorlevel 1 exit /b 1
release\tmp\build_mixed_save_temps.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$log=Get-Content 'release\tmp\build_mixed_save_temps.log' -Raw; $m=[regex]::Match($log,'temporary files kept:\s*(.+)'); if(-not $m.Success){exit 1}; $dir=$m.Groups[1].Value.Trim(); if(-not (Test-Path -LiteralPath $dir -PathType Container)){exit 2}; foreach($f in @('csrc_0.c','csrc_1.c')){$p=Join-Path $dir $f; if(-not (Test-Path -LiteralPath $p)){exit 3}; $t=Get-Content -LiteralPath $p -Raw; if($t -notmatch 'lib/xrt/xrt\.h'){exit 4}; if($t -notmatch 'module\.h'){exit 5}}; $t0=Get-Content -LiteralPath (Join-Path $dir 'csrc_0.c') -Raw; $t1=Get-Content -LiteralPath (Join-Path $dir 'csrc_1.c') -Raw; if($t0 -notmatch 'build_mixed_host\.c'){exit 6}; if($t1 -notmatch 'build_mixed_extra\.c'){exit 7}; Remove-Item -LiteralPath $dir -Recurse -Force"
if errorlevel 1 (
    echo build save-temps wrapper check failed.
    exit /b 1
)

release\xl.exe build test\cases\cgen_main_entry.xl -o release\tmp\explicit_xrt_link.exe --link %XL6_XRT_OBJ%
if errorlevel 1 exit /b 1
release\tmp\explicit_xrt_link.exe one
if not errorlevel 2 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)
if errorlevel 3 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_build_*').Count; Set-Content 'release\tmp\build_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_main_entry.xl -shared -o release\tmp\bad_main_shared.dll >nul 2> release\tmp\bad_main_shared.err.txt
if not errorlevel 1 (
    echo shared build with xlang main should fail.
    exit /b 1
)
findstr /C:"shared build cannot contain xlang main" release\tmp\bad_main_shared.err.txt >nul
if errorlevel 1 (
    echo shared build error message missing.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'release\tmp\build_fail_dirs.before'); $after=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_build_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed build should clean temporary work directory.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_run_*').Count; Set-Content 'release\tmp\run_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_time_literal_bad.xl >nul 2> release\tmp\run_bad_script.err.txt
if not errorlevel 1 (
    echo bad script run should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'release\tmp\run_fail_dirs.before'); $after=@(Get-ChildItem 'release\tmp' -Directory -Filter 'xl6_run_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed run should clean temporary work directory.
    exit /b 1
)

"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\cli_link_host.c -o release\tmp\cli_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs release\tmp\libxl6clitest.a release\tmp\cli_link_host.o
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cli_link.xl --lib-path release\tmp --link xl6clitest
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cli_link.xl -o release\tmp\cli_link_build.exe --lib-path release\tmp --link xl6clitest
if errorlevel 1 exit /b 1
release\tmp\cli_link_build.exe
if errorlevel 1 exit /b 1

"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\cli_loadlib_host.c -o release\tmp\cli_loadlib_host.dll
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cli_loadlib.xl --loadlib release\tmp\cli_loadlib_host.dll
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_puts.xl --emit release\tmp\extern_puts.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"puts\" func puts(text: string) -> int32" release\tmp\extern_puts.xlir >nul
if errorlevel 1 (
    echo extern_puts.xlir missing extern declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_puts.xl --emit release\tmp\extern_puts.c
if errorlevel 1 exit /b 1
findstr /C:"int32_t puts(const char* text);" release\tmp\extern_puts.c >nul
if errorlevel 1 (
    echo extern_puts.c missing C prototype.
    exit /b 1
)

release\xl.exe run test\cases\extern_puts.xl > release\tmp\extern_puts.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang calls c" release\tmp\extern_puts.out.txt >nul
if errorlevel 1 (
    echo extern_puts.xl did not call C puts.
    exit /b 1
)

release\xl.exe run examples\c_hello.xl > release\tmp\example_c_hello.out.txt
if errorlevel 1 exit /b 1
findstr /C:"hello from c" release\tmp\example_c_hello.out.txt >nul
if errorlevel 1 (
    echo examples\c_hello.xl did not run.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_bool.xl --emit release\tmp\extern_bool.c
if errorlevel 1 exit /b 1
findstr /C:"bool native_flip_bool(bool value);" release\tmp\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool extern prototype.
    exit /b 1
)
findstr /C:"void expect_bool(bool actual, bool expected);" release\tmp\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool expect prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\extern_bool.c test\cases\extern_bool_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\extern_bool.exe
if errorlevel 1 exit /b 1
release\tmp\extern_bool.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\c_abi_types.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\c_abi_types.xl --emit release\tmp\c_abi_types.c
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_c_echo_size(size_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_size prototype.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_c_echo_ssize(xl6_ssize_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize prototype.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_c_echo_ptrdiff(ptrdiff_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ptrdiff prototype.
    exit /b 1
)
findstr /C:"wchar_t xl6_c_echo_wchar(wchar_t value);" release\tmp\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_wchar prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\c_abi_types.c test\cases\c_abi_types_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\c_abi_types.exe
if errorlevel 1 exit /b 1
release\tmp\c_abi_types.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_extern_string_return.xl --emit release\tmp\cgen_extern_string_return.c
if errorlevel 1 exit /b 1
findstr /C:"const char* native_text(void);" release\tmp\cgen_extern_string_return.c >nul
if errorlevel 1 (
    echo cgen_extern_string_return.c missing string return extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\cgen_extern_string_return.c test\cases\cgen_extern_string_return_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1
release\tmp\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_var.xl --emit release\tmp\extern_var.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"host_counter\" var counter : int" release\tmp\extern_var.xlir >nul
if errorlevel 1 (
    echo extern_var.xlir missing extern var declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_var.xl --emit release\tmp\extern_var.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t host_counter;" release\tmp\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable declaration.
    exit /b 1
)
findstr /C:"host_counter = (host_counter + 2LL);" release\tmp\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\extern_var.c test\cases\extern_var_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\extern_var.exe
if errorlevel 1 exit /b 1
release\tmp\extern_var.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_add.xl --emit release\tmp\export_add.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_add_i64\" func add_i64(a: int, b: int) -> int" release\tmp\export_add.xlir >nul
if errorlevel 1 (
    echo export_add.xlir missing export declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_add.xl --emit release\tmp\export_add.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_add_i64" release\tmp\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing C export symbol.
    exit /b 1
)
findstr /C:"symbol: xl6_export_add__i64" release\tmp\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing internal ABI symbol.
    exit /b 1
)

release\xl.exe cgen test\cases\export_add.xl --emit release\tmp\export_add.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_add__i64(long long a, long long b);" release\tmp\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing internal export prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" release\tmp\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing C export wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_add.c test\cases\export_add_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_add_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_add_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_add.xl --emit release\tmp\export_add_header.c --emit-header release\tmp\export_add.h
if errorlevel 1 exit /b 1
findstr /C:"#ifndef XL6_EXPORT_ADD_H" release\tmp\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing header guard.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" release\tmp\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing C export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_add_header.c test\cases\export_add_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_add_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_add_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class.xl --emit release\tmp\export_class.c --emit-header release\tmp\export_class.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class typedef.
    exit /b 1
)
findstr /C:"XL_Class_Point xl6_point_make(int64_t x, int64_t y);" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class return export declaration.
    exit /b 1
)
findstr /C:"int64_t xl6_point_sum(XL_Class_Point point);" release\tmp\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class parameter export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class.c test\cases\export_class_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_inherit.xl --emit release\tmp\export_class_inherit.c --emit-header release\tmp\export_class_inherit.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Entity" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing base class typedef.
    exit /b 1
)
findstr /C:"XL_Class_Entity base;" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited base field.
    exit /b 1
)
findstr /C:"XL_Class_NamedPoint xl6_named_point_make(int64_t id, int64_t x, int64_t y);" release\tmp\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited class return export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class_inherit.c test\cases\export_class_inherit_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_union_field.xl --emit release\tmp\export_class_union_field.c --emit-header release\tmp\export_class_union_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing dependent union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload payload;" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class union field.
    exit /b 1
)
findstr /C:"XL_Class_Box xl6_box_make(int64_t value);" release\tmp\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class-with-union export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_class_union_field.c test\cases\export_class_union_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union_class_field.xl --emit release\tmp\export_union_class_field.c --emit-header release\tmp\export_union_class_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing dependent class typedef.
    exit /b 1
)
findstr /C:"typedef union XL_Union_Payload" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union typedef.
    exit /b 1
)
findstr /C:"XL_Class_Point point;" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union class field.
    exit /b 1
)
findstr /C:"XL_Union_Payload xl6_payload_make(int64_t x, int64_t y);" release\tmp\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union-with-class export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_union_class_field.c test\cases\export_union_class_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\export_add.xl -shared -o release\tmp\xl6_export_add.dll --emit-header release\tmp\xl6_export_add_shared.h
if errorlevel 1 exit /b 1
if not exist release\tmp\xl6_export_add.dll (
    echo xl6_export_add.dll missing after build -shared.
    exit /b 1
)
if not exist release\tmp\xl6_export_add_shared.h (
    echo xl6_export_add_shared.h missing after build -shared.
    exit /b 1
)

release\xl.exe abi test\cases\export_multi_return.xl --emit release\tmp\export_multi_return.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.pair(int)->(int,int)" release\tmp\export_multi_return.abi.txt >nul
if errorlevel 1 (
    echo export_multi_return.abi.txt missing multi-return signature.
    exit /b 1
)

release\xl.exe cgen test\cases\export_multi_return.xl --emit release\tmp\export_multi_return.c --emit-header release\tmp\export_multi_return.h
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\export_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing internal return struct.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'release\tmp\export_multi_return.c' -Raw; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ xl6_export_pair\(long long seed\);'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing multi-return internal prototype.
    exit /b 1
)
findstr /C:"void xl6_pair_i64(int64_t seed, int64_t* out0, int64_t* out1);" release\tmp\export_multi_return.h >nul
if errorlevel 1 (
    echo export_multi_return.h missing out-param wrapper declaration.
    exit /b 1
)
findstr /C:"*out1 = (int64_t)__xl6_return.value1;" release\tmp\export_multi_return.c >nul
if errorlevel 1 (
    echo export_multi_return.c missing out-param assignment.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_multi_return.c test\cases\export_multi_return_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_bool.xl --emit release\tmp\export_bool.c --emit-header release\tmp\export_bool.h
if errorlevel 1 exit /b 1
findstr /C:"int xl6_export_isEven(long long value);" release\tmp\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing internal bool export prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" release\tmp\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing C bool wrapper prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" release\tmp\export_bool.h >nul
if errorlevel 1 (
    echo export_bool.h missing C bool wrapper declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_bool.c test\cases\export_bool_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_bool_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_bool_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_primitive_widths.xl --emit release\tmp\export_primitive_widths.c --emit-header release\tmp\export_primitive_widths.h
if errorlevel 1 exit /b 1
findstr /C:"uint64_t xl6_pick_u64(" release\tmp\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing uint64 return wrapper.
    exit /b 1
)
findstr /C:"int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64, float f32, double f64, bool ok, void* handle" release\tmp\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing primitive width parameter mapping.
    exit /b 1
)
findstr /C:"uint64_t xl6_pick_u64(" release\tmp\export_primitive_widths.c >nul
if errorlevel 1 (
    echo export_primitive_widths.c missing C wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" -c -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_primitive_widths.c -o release\tmp\export_primitive_widths.o
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_c_abi_types.xl --emit release\tmp\export_c_abi_types.c --emit-header release\tmp\export_c_abi_types.h
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_echo_c_size(size_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_size export declaration.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_echo_c_ssize(xl6_ssize_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize export declaration.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_echo_c_ptrdiff(ptrdiff_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ptrdiff export declaration.
    exit /b 1
)
findstr /C:"wchar_t xl6_echo_c_wchar(wchar_t value);" release\tmp\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_wchar export declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_c_abi_types.c test\cases\export_c_abi_types_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_alias.xl --emit release\tmp\export_alias.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.aliasAdd(int,int)->int" release\tmp\export_alias.abi.txt >nul
if errorlevel 1 (
    echo export_alias.abi.txt missing canonical alias signature.
    exit /b 1
)
release\xl.exe abi test\cases\export_alias.xl --json --emit release\tmp\export_alias.abi.json
if errorlevel 1 exit /b 1
findstr /C:"abiVersion" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing ABI version.
    exit /b 1
)
findstr /C:"fn:app.aliasAdd(int,int)->int" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing canonical alias signature.
    exit /b 1
)
findstr /C:"xl6_alias_add" release\tmp\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing C export symbol.
    exit /b 1
)
release\xl.exe abi test\cases\extern_puts.xl --json --emit release\tmp\extern_puts.abi.json
if errorlevel 1 exit /b 1
findstr /C:"cSymbol" release\tmp\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing extern C symbol.
    exit /b 1
)
findstr /C:"puts" release\tmp\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing puts symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\export_alias.xl --emit release\tmp\export_alias.c --emit-header release\tmp\export_alias.h
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_aliasAdd(long long a, long long b);" release\tmp\export_alias.c >nul
if errorlevel 1 (
    echo export_alias.c missing expanded alias prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_alias_add(int64_t a, int64_t b);" release\tmp\export_alias.h >nul
if errorlevel 1 (
    echo export_alias.h missing expanded alias header declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_alias.c test\cases\export_alias_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_alias_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_alias_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check examples\c_variables.xl
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_vars.xl --emit release\tmp\export_vars.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_counter\" var counter : int = 1" release\tmp\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export var declaration.
    exit /b 1
)
findstr /C:"export \"xl6_version\" const version : int = 7" release\tmp\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export const declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_vars.xl --emit release\tmp\export_vars.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_counter" release\tmp\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export var symbol.
    exit /b 1
)
findstr /C:"kind: const" release\tmp\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export const kind.
    exit /b 1
)

release\xl.exe cgen test\cases\export_vars.xl --emit release\tmp\export_vars.c --emit-header release\tmp\export_vars.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_counter = 1LL;" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var definition.
    exit /b 1
)
findstr /C:"const int64_t xl6_version = 7LL;" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export const definition.
    exit /b 1
)
findstr /C:"long long __xl6_return_0 = (xl6_counter + xl6_version);" release\tmp\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var read inside wrapper target.
    exit /b 1
)
findstr /C:"extern int64_t xl6_counter;" release\tmp\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export var declaration.
    exit /b 1
)
findstr /C:"extern const int64_t xl6_version;" release\tmp\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export const declaration.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tmp %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_vars.c test\cases\export_vars_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_vars_header_host.exe
if errorlevel 1 exit /b 1
release\tmp\export_vars_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_call.xl --emit release\tmp\export_call.c
if errorlevel 1 exit /b 1
findstr /C:"long long value = xl6_export_add__i64(1LL, 2LL);" release\tmp\export_call.c >nul
if errorlevel 1 (
    echo export_call.c did not use internal ABI symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\export_call.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\export_call.exe
if errorlevel 1 exit /b 1
release\tmp\export_call.exe
if errorlevel 1 exit /b 1

release\xl.exe abi examples\c_export.xl --emit release\tmp\example_c_export.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export app_add_i64" release\tmp\example_c_export.abi.txt >nul
if errorlevel 1 (
    echo examples\c_export.xl ABI dump missing export.
    exit /b 1
)

release\xl.exe build test\cases\cgen_basic.xl >nul 2> release\tmp\build_missing_output.err.txt
if not errorlevel 1 (
    echo build without -o should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_duplicate.xl >nul 2> release\tmp\sem_duplicate.err.txt
if not errorlevel 1 (
    echo sem_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_global_func_value_conflict.xl >nul 2> release\tmp\sem_global_func_value_conflict.err.txt
if not errorlevel 1 (
    echo sem_global_func_value_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_const_assign.xl >nul 2> release\tmp\sem_const_assign.err.txt
if not errorlevel 1 (
    echo sem_const_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_safe_member_assign.xl >nul 2> release\tmp\sem_safe_member_assign.err.txt
if not errorlevel 1 (
    echo sem_safe_member_assign.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_unknown.xl >nul 2> release\tmp\sem_safe_member_unknown.err.txt
if not errorlevel 1 (
    echo sem_safe_member_unknown.xl should fail.
    exit /b 1
)
findstr /C:"safe member key receiver must be var or dynamic dict" release\tmp\sem_safe_member_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_unknown.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_unknown.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_unknown.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_typed_container.xl >nul 2> release\tmp\sem_safe_member_typed_container.err.txt
if not errorlevel 1 (
    echo sem_safe_member_typed_container.xl should fail.
    exit /b 1
)
findstr /C:"safe member length receiver must be var or dynamic container" release\tmp\sem_safe_member_typed_container.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_typed_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_typed_container.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_typed_container.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_non_container.xl >nul 2> release\tmp\sem_safe_member_non_container.err.txt
if not errorlevel 1 (
    echo sem_safe_member_non_container.xl should fail.
    exit /b 1
)
findstr /C:"safe member length receiver must be var or dynamic container" release\tmp\sem_safe_member_non_container.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_non_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_safe_member_non_container.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_non_container.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_array.xl >nul 2> release\tmp\sem_member_key_array.err.txt
if not errorlevel 1 (
    echo sem_member_key_array.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_array.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_array.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_array.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_array.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_set.xl >nul 2> release\tmp\sem_member_key_set.err.txt
if not errorlevel 1 (
    echo sem_member_key_set.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_set.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_set.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_set.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_set.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_typed_dict.xl >nul 2> release\tmp\sem_member_key_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_member_key_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" release\tmp\sem_member_key_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_typed_dict.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_member_key_typed_dict.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_typed_dict.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_length.xl >nul 2> release\tmp\sem_dynamic_member_assign_length.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl should fail.
    exit /b 1
)
findstr /C:"member length is read-only" release\tmp\sem_dynamic_member_assign_length.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_container.xl >nul 2> release\tmp\sem_dynamic_member_assign_container.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl should fail.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" release\tmp\sem_dynamic_member_assign_container.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_typed_dict.xl >nul 2> release\tmp\sem_dynamic_member_assign_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" release\tmp\sem_dynamic_member_assign_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_compound.xl >nul 2> release\tmp\sem_dynamic_member_assign_compound.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment only supports =" release\tmp\sem_dynamic_member_assign_compound.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_temporary.xl >nul 2> release\tmp\sem_dynamic_member_assign_temporary.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment receiver must be mutable" release\tmp\sem_dynamic_member_assign_temporary.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_array_key_type.xl >nul 2> release\tmp\sem_index_array_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_array_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_dict_key_type.xl >nul 2> release\tmp\sem_index_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_dict_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_index_dict_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_index_dict_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_container_index_key_type.xl >nul 2> release\tmp\sem_var_container_index_key_type.err.txt
if not errorlevel 1 (
    echo sem_var_container_index_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_outside_loop.xl >nul 2> release\tmp\sem_break_outside_loop.err.txt
if not errorlevel 1 (
    echo sem_break_outside_loop.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_continue_zero.xl >nul 2> release\tmp\sem_continue_zero.err.txt
if not errorlevel 1 (
    echo sem_continue_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_too_deep.xl >nul 2> release\tmp\sem_break_too_deep.err.txt
if not errorlevel 1 (
    echo sem_break_too_deep.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_if_condition_type.xl >nul 2> release\tmp\sem_if_condition_type.err.txt
if not errorlevel 1 (
    echo sem_if_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_while_condition_type.xl >nul 2> release\tmp\sem_while_condition_type.err.txt
if not errorlevel 1 (
    echo sem_while_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_conditional_condition_type.xl >nul 2> release\tmp\sem_conditional_condition_type.err.txt
if not errorlevel 1 (
    echo sem_conditional_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_logical_operand_type.xl >nul 2> release\tmp\sem_logical_operand_type.err.txt
if not errorlevel 1 (
    echo sem_logical_operand_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_outside_func.xl >nul 2> release\tmp\sem_return_outside_func.err.txt
if not errorlevel 1 (
    echo sem_return_outside_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_value_in_no_return.xl >nul 2> release\tmp\sem_return_value_in_no_return.err.txt
if not errorlevel 1 (
    echo sem_return_value_in_no_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_missing_value.xl >nul 2> release\tmp\sem_return_missing_value.err.txt
if not errorlevel 1 (
    echo sem_return_missing_value.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_multi_count.xl >nul 2> release\tmp\sem_return_multi_count.err.txt
if not errorlevel 1 (
    echo sem_return_multi_count.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_count.xl >nul 2> release\tmp\sem_multi_decl_count.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_count.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_decl_rhs.xl >nul 2> release\tmp\sem_multi_decl_rhs.err.txt
if not errorlevel 1 (
    echo sem_multi_decl_rhs.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_assign_count.xl >nul 2> release\tmp\sem_multi_assign_count.err.txt
if not errorlevel 1 (
    echo sem_multi_assign_count.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_decl.xl >nul 2> release\tmp\sem_multi_single_decl.err.txt
if not errorlevel 1 (
    echo sem_multi_single_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_single_assign.xl >nul 2> release\tmp\sem_multi_single_assign.err.txt
if not errorlevel 1 (
    echo sem_multi_single_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_arg.xl >nul 2> release\tmp\sem_multi_return_arg.err.txt
if not errorlevel 1 (
    echo sem_multi_return_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_multi_return_single.xl >nul 2> release\tmp\sem_multi_return_single.err.txt
if not errorlevel 1 (
    echo sem_multi_return_single.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_named_order.xl >nul 2> release\tmp\sem_call_named_order.err.txt
if not errorlevel 1 (
    echo sem_call_named_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_duplicate_named.xl >nul 2> release\tmp\sem_call_duplicate_named.err.txt
if not errorlevel 1 (
    echo sem_call_duplicate_named.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_call_missing_required.xl >nul 2> release\tmp\sem_call_missing_required.err.txt
if not errorlevel 1 (
    echo sem_call_missing_required.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_call_missing_required.err.txt >nul
if not errorlevel 1 (
    echo sem_call_missing_required.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_order.xl >nul 2> release\tmp\sem_param_order.err.txt
if not errorlevel 1 (
    echo sem_param_order.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_position.xl >nul 2> release\tmp\sem_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_vararg_arg_redeclare.xl >nul 2> release\tmp\sem_vararg_arg_redeclare.err.txt
if not errorlevel 1 (
    echo sem_vararg_arg_redeclare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_bad_name.xl >nul 2> release\tmp\sem_function_type_bad_name.err.txt
if not errorlevel 1 (
    echo sem_function_type_bad_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_type_vararg_position.xl >nul 2> release\tmp\sem_function_type_vararg_position.err.txt
if not errorlevel 1 (
    echo sem_function_type_vararg_position.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_signature_mismatch.xl >nul 2> release\tmp\sem_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo sem_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\sem_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo sem_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_function_signature_mismatch.xl >nul 2> release\tmp\cgen_function_signature_mismatch.err.txt
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem]" release\tmp\cgen_function_signature_mismatch.err.txt >nul
if errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt missing semantic diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\cgen_function_signature_mismatch.err.txt >nul
if not errorlevel 1 (
    echo cgen_function_signature_mismatch.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_call_arg_type.xl >nul 2> release\tmp\sem_function_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_call_arg_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_function_field_call_arg_type.xl >nul 2> release\tmp\sem_member_function_field_call_arg_type.err.txt
if not errorlevel 1 (
    echo sem_member_function_field_call_arg_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_named_arg.xl >nul 2> release\tmp\sem_function_bare_named_arg.err.txt
if not errorlevel 1 (
    echo sem_function_bare_named_arg.xl should fail.
    exit /b 1
)
findstr /C:"function" release\tmp\sem_function_bare_named_arg.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt missing function diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_bare_named_arg.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_named_arg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_bare_overload_name.xl >nul 2> release\tmp\sem_function_bare_overload_name.err.txt
if not errorlevel 1 (
    echo sem_function_bare_overload_name.xl should fail.
    exit /b 1
)
findstr /C:"function" release\tmp\sem_function_bare_overload_name.err.txt >nul
if errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt missing function overload diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_function_bare_overload_name.err.txt >nul
if not errorlevel 1 (
    echo sem_function_bare_overload_name.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_function_multi_return_arg_type.xl >nul 2> release\tmp\sem_function_multi_return_arg_type.err.txt
if not errorlevel 1 (
    echo sem_function_multi_return_arg_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_type_mismatch.xl >nul 2> release\tmp\sem_return_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_return_type_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_param_default_type.xl >nul 2> release\tmp\sem_param_default_type.err.txt
if not errorlevel 1 (
    echo sem_param_default_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_anon_capture_assign.xl >nul 2> release\tmp\sem_anon_capture_assign.err.txt
if not errorlevel 1 (
    echo sem_anon_capture_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_destroy_class.xl >nul 2> release\tmp\sem_closure_capture_destroy_class.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_destroy_class.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_custom.xl >nul 2> release\tmp\sem_closure_capture_custom.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_custom.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_closure_capture_custom_field.xl >nul 2> release\tmp\sem_closure_capture_custom_field.err.txt
if not errorlevel 1 (
    echo sem_closure_capture_custom_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_class_copy_init.xl >nul 2> release\tmp\sem_noncopyable_class_copy_init.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_class_copy_init.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_assign.xl >nul 2> release\tmp\sem_noncopyable_assign.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_custom_param.xl >nul 2> release\tmp\sem_noncopyable_custom_param.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_custom_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_noncopyable_return.xl >nul 2> release\tmp\sem_noncopyable_return.err.txt
if not errorlevel 1 (
    echo sem_noncopyable_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_custom.xl >nul 2> release\tmp\sem_typed_container_noncopyable_custom.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_custom.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_custom.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_custom.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_class.xl >nul 2> release\tmp\sem_typed_container_noncopyable_class.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_class.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_class.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_class.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_container_noncopyable_nested.xl >nul 2> release\tmp\sem_typed_container_noncopyable_nested.err.txt
if not errorlevel 1 (
    echo sem_typed_container_noncopyable_nested.xl should fail.
    exit /b 1
)
findstr /C:"typed container item type must be copyable" release\tmp\sem_typed_container_noncopyable_nested.err.txt >nul
if errorlevel 1 (
    echo sem_typed_container_noncopyable_nested.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_unknown_base.xl >nul 2> release\tmp\sem_class_unknown_base.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_base.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_create_missing.xl >nul 2> release\tmp\sem_class_base_create_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_create_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_default_missing.xl >nul 2> release\tmp\sem_class_base_default_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_default_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_outside_create.xl >nul 2> release\tmp\sem_class_base_outside_create.err.txt
if not errorlevel 1 (
    echo sem_class_base_outside_create.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_base_overload_missing.xl >nul 2> release\tmp\sem_class_base_overload_missing.err.txt
if not errorlevel 1 (
    echo sem_class_base_overload_missing.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_ctor_arity.xl >nul 2> release\tmp\sem_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_class_ctor_arity.xl should fail.
    exit /b 1
)
findstr /C:"create" release\tmp\sem_class_ctor_arity.err.txt >nul
if errorlevel 1 (
    echo sem_class_ctor_arity.err.txt missing create diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_ctor_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_class_ctor_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_default_ctor_args.xl >nul 2> release\tmp\sem_class_default_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_class_default_ctor_args.xl should fail.
    exit /b 1
)
findstr /C:"create" release\tmp\sem_class_default_ctor_args.err.txt >nul
if errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt missing create diagnostic.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_default_ctor_args.err.txt >nul
if not errorlevel 1 (
    echo sem_class_default_ctor_args.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_destroy_params.xl >nul 2> release\tmp\sem_class_destroy_params.err.txt
if not errorlevel 1 (
    echo sem_class_destroy_params.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_conflict.xl >nul 2> release\tmp\sem_class_field_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_field_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_field_default_type.xl >nul 2> release\tmp\sem_class_field_default_type.err.txt
if not errorlevel 1 (
    echo sem_class_field_default_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_class_field_default_type.err.txt >nul
if not errorlevel 1 (
    echo sem_class_field_default_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_method_overload_conflict.xl >nul 2> release\tmp\sem_class_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_class_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_self_assign.xl >nul 2> release\tmp\sem_self_assign.err.txt
if not errorlevel 1 (
    echo sem_self_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_bad_special.xl >nul 2> release\tmp\sem_custom_bad_special.err.txt
if not errorlevel 1 (
    echo sem_custom_bad_special.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_duplicate_c_symbol.xl >nul 2> release\tmp\sem_custom_duplicate_c_symbol.err.txt
if not errorlevel 1 (
    echo sem_custom_duplicate_c_symbol.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_bind_non_point.xl >nul 2> release\tmp\sem_custom_bind_non_point.err.txt
if not errorlevel 1 (
    echo sem_custom_bind_non_point.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_create_point_bind_conflict.xl >nul 2> release\tmp\sem_custom_create_point_bind_conflict.err.txt
if not errorlevel 1 (
    echo sem_custom_create_point_bind_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_bad_type.xl >nul 2> release\tmp\sem_custom_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_bad_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_bad_type.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_bad_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_return_bad.xl >nul 2> release\tmp\sem_custom_extern_return_bad.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_return_bad.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_return_bad.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_return_bad.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_extern_vararg.xl >nul 2> release\tmp\sem_custom_extern_vararg.err.txt
if not errorlevel 1 (
    echo sem_custom_extern_vararg.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_custom_extern_vararg.err.txt >nul
if not errorlevel 1 (
    echo sem_custom_extern_vararg.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_custom_method_overload_conflict.xl >nul 2> release\tmp\sem_custom_method_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_custom_method_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_param.xl >nul 2> release\tmp\sem_auto_param.err.txt
if not errorlevel 1 (
    echo sem_auto_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_return.xl >nul 2> release\tmp\sem_auto_return.err.txt
if not errorlevel 1 (
    echo sem_auto_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_field.xl >nul 2> release\tmp\sem_auto_field.err.txt
if not errorlevel 1 (
    echo sem_auto_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_alias.xl >nul 2> release\tmp\sem_auto_alias.err.txt
if not errorlevel 1 (
    echo sem_auto_alias.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_auto_function_type.xl >nul 2> release\tmp\sem_auto_function_type.err.txt
if not errorlevel 1 (
    echo sem_auto_function_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_void_type.xl >nul 2> release\tmp\sem_void_type.err.txt
if not errorlevel 1 (
    echo sem_void_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_as_return.xl >nul 2> release\tmp\sem_as_return.err.txt
if not errorlevel 1 (
    echo sem_as_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_builtin_name.xl >nul 2> release\tmp\sem_type_builtin_name.err.txt
if not errorlevel 1 (
    echo sem_type_builtin_name.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_duplicate_decl.xl >nul 2> release\tmp\sem_type_duplicate_decl.err.txt
if not errorlevel 1 (
    echo sem_type_duplicate_decl.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_unknown_type.xl >nul 2> release\tmp\sem_unknown_type.err.txt
if not errorlevel 1 (
    echo sem_unknown_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_duplicate.xl >nul 2> release\tmp\sem_export_duplicate.err.txt
if not errorlevel 1 (
    echo sem_export_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_optional.xl >nul 2> release\tmp\sem_export_optional.err.txt
if not errorlevel 1 (
    echo sem_export_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_vararg.xl >nul 2> release\tmp\sem_export_vararg.err.txt
if not errorlevel 1 (
    echo sem_export_vararg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_string_return.xl >nul 2> release\tmp\sem_export_string_return.err.txt
if not errorlevel 1 (
    echo sem_export_string_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_export_generic.xl >nul 2> release\tmp\sem_export_generic.err.txt
if not errorlevel 1 (
    echo sem_export_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_arity.xl >nul 2> release\tmp\sem_generic_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_param_duplicate.xl >nul 2> release\tmp\sem_generic_param_duplicate.err.txt
if not errorlevel 1 (
    echo sem_generic_param_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_unknown_type_arg.xl >nul 2> release\tmp\sem_generic_unknown_type_arg.err.txt
if not errorlevel 1 (
    echo sem_generic_unknown_type_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_infer_conflict.xl >nul 2> release\tmp\sem_generic_infer_conflict.err.txt
if not errorlevel 1 (
    echo sem_generic_infer_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_func_inst_arity.xl >nul 2> release\tmp\sem_generic_func_inst_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_func_inst_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_old_call_syntax.xl >nul 2> release\tmp\sem_generic_old_call_syntax.err.txt
if not errorlevel 1 (
    echo sem_generic_old_call_syntax.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_arity.xl >nul 2> release\tmp\sem_generic_class_ctor_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_open_ctor.xl >nul 2> release\tmp\sem_generic_class_open_ctor.err.txt
if not errorlevel 1 (
    echo sem_generic_class_open_ctor.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_ctor_type_mismatch.xl >nul 2> release\tmp\sem_generic_class_ctor_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_generic_class_ctor_type_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_arity.xl >nul 2> release\tmp\sem_generic_class_arity.err.txt
if not errorlevel 1 (
    echo sem_generic_class_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generic_class_extends.xl >nul 2> release\tmp\sem_generic_class_extends.err.txt
if not errorlevel 1 (
    echo sem_generic_class_extends.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_return_only.xl >nul 2> release\tmp\sem_overload_return_only.err.txt
if not errorlevel 1 (
    echo sem_overload_return_only.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_optional_conflict.xl >nul 2> release\tmp\sem_overload_optional_conflict.err.txt
if not errorlevel 1 (
    echo sem_overload_optional_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_vararg_same_kind.xl >nul 2> release\tmp\sem_overload_vararg_same_kind.err.txt
if not errorlevel 1 (
    echo sem_overload_vararg_same_kind.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_overload_conflict.xl >nul 2> release\tmp\sem_alias_overload_conflict.err.txt
if not errorlevel 1 (
    echo sem_alias_overload_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_alias_cycle.xl >nul 2> release\tmp\sem_alias_cycle.err.txt
if not errorlevel 1 (
    echo sem_alias_cycle.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_mismatch.xl >nul 2> release\tmp\sem_extern_c_symbol_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_c_symbol_kind_mismatch.xl >nul 2> release\tmp\sem_extern_c_symbol_kind_mismatch.err.txt
if not errorlevel 1 (
    echo sem_extern_c_symbol_kind_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_bad_type.xl >nul 2> release\tmp\sem_extern_bad_type.err.txt
if not errorlevel 1 (
    echo sem_extern_bad_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_multi_return.xl >nul 2> release\tmp\sem_extern_multi_return.err.txt
if not errorlevel 1 (
    echo sem_extern_multi_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_optional.xl >nul 2> release\tmp\sem_extern_optional.err.txt
if not errorlevel 1 (
    echo sem_extern_optional.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_vararg.xl >nul 2> release\tmp\sem_extern_vararg.err.txt
if not errorlevel 1 (
    echo sem_extern_vararg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_extern_generic.xl >nul 2> release\tmp\sem_extern_generic.err.txt
if not errorlevel 1 (
    echo sem_extern_generic.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_concat_mismatch.xl >nul 2> release\tmp\sem_string_concat_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_concat_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_assign_mismatch.xl >nul 2> release\tmp\sem_string_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_assign_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_string_compare_mismatch.xl >nul 2> release\tmp\sem_string_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_string_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_bad.xl >nul 2> release\tmp\sem_time_literal_bad.err.txt
if not errorlevel 1 (
    echo sem_time_literal_bad.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_literal_newline.xl >nul 2> release\tmp\sem_time_literal_newline.err.txt
if not errorlevel 1 (
    echo sem_time_literal_newline.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_assign_mismatch.xl >nul 2> release\tmp\sem_time_assign_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_assign_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_time_compare_mismatch.xl >nul 2> release\tmp\sem_time_compare_mismatch.err.txt
if not errorlevel 1 (
    echo sem_time_compare_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_custom_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_custom_assign.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_custom_arg.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_class_return.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_function_init.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_var_destroy_class_init.xl >nul 2> release\tmp\sem_var_destroy_class_init.err.txt
if not errorlevel 1 (
    echo sem_var_destroy_class_init.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_destroy_class_return.xl >nul 2> release\tmp\sem_var_destroy_class_return.err.txt
if not errorlevel 1 (
    echo sem_var_destroy_class_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_overload_ambiguous.xl >nul 2> release\tmp\sem_overload_ambiguous.err.txt
if not errorlevel 1 (
    echo sem_overload_ambiguous.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_literal.xl >nul 2> release\tmp\sem_typed_literal.err.txt
if not errorlevel 1 (
    echo sem_typed_literal.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_typed_set_pair.xl >nul 2> release\tmp\sem_typed_set_pair.err.txt
if not errorlevel 1 (
    echo sem_typed_set_pair.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_value_mismatch.xl >nul 2> release\tmp\sem_type_value_mismatch.err.txt
if not errorlevel 1 (
    echo sem_type_value_mismatch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_compare_int.xl >nul 2> release\tmp\sem_type_compare_int.err.txt
if not errorlevel 1 (
    echo sem_type_compare_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_order_compare.xl >nul 2> release\tmp\sem_type_order_compare.err.txt
if not errorlevel 1 (
    echo sem_type_order_compare.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_type_call_arity.xl >nul 2> release\tmp\sem_type_call_arity.err.txt
if not errorlevel 1 (
    echo sem_type_call_arity.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_arity.xl >nul 2> release\tmp\sem_range_arity.err.txt
if not errorlevel 1 (
    echo sem_range_arity.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_range_arity.err.txt >nul
if not errorlevel 1 (
    echo sem_range_arity.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_range_step_zero.xl >nul 2> release\tmp\sem_range_step_zero.err.txt
if not errorlevel 1 (
    echo sem_range_step_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_for_in_three_targets.xl >nul 2> release\tmp\sem_for_in_three_targets.err.txt
if not errorlevel 1 (
    echo sem_for_in_three_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_for_in_three_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_for_in_three_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_exceptions.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_exceptions.xl -o release\tmp\cgen_exceptions.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_exceptions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_exceptions.xl --emit release\tmp\cgen_exceptions.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise" release\tmp\cgen_exceptions.c >nul
if errorlevel 1 (
    echo cgen_exceptions.c missing error runtime raise.
    exit /b 1
)

release\xl.exe check test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_loop_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_loop_exception_propagation.xl -o release\tmp\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_loop_exception_propagation.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_loop_exception_propagation.xl --emit release\tmp\cgen_loop_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"loop boom\"" release\tmp\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop throw raise.
    exit /b 1
)

findstr /C:"goto __xl6_try_finally_" release\tmp\cgen_loop_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_loop_exception_propagation.c missing loop exception try/finally jump.
    exit /b 1
)

release\xl.exe run test\cases\cgen_exception_uncaught.xl >nul 2> release\tmp\cgen_exception_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_exception_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: uncaught" release\tmp\cgen_exception_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_exception_uncaught.err.txt missing uncaught exception diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_sync.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_sync.xl -o release\tmp\cgen_sync.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_sync.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_sync.xl --emit release\tmp\cgen_sync.c
if errorlevel 1 exit /b 1

findstr /C:"xrtMutexCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtMutexCreate.
    exit /b 1
)
findstr /C:"xrtSemCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtSemCreate.
    exit /b 1
)
findstr /C:"xrtRWLockCreate" release\tmp\cgen_sync.c >nul
if errorlevel 1 (
    echo cgen_sync.c missing xrtRWLockCreate.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_ctor_args.xl >nul 2> release\tmp\sem_sync_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_sync_ctor_args.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_method_args.xl >nul 2> release\tmp\sem_sync_method_args.err.txt
if not errorlevel 1 (
    echo sem_sync_method_args.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_cond_mutex_arg.xl >nul 2> release\tmp\sem_sync_cond_mutex_arg.err.txt
if not errorlevel 1 (
    echo sem_sync_cond_mutex_arg.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_sync_copy.xl >nul 2> release\tmp\sem_sync_copy.err.txt
if not errorlevel 1 (
    echo sem_sync_copy.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread.xl -o release\tmp\cgen_thread.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_thread.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_thread.xl --emit release\tmp\cgen_thread.c
if errorlevel 1 exit /b 1

findstr /C:"xrtThreadCreate" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing xrtThreadCreate.
    exit /b 1
)
findstr /C:"xl6_thread_start" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread start helper.
    exit /b 1
)
findstr /C:"xl6_thread_release" release\tmp\cgen_thread.c >nul
if errorlevel 1 (
    echo cgen_thread.c missing thread release helper.
    exit /b 1
)

release\xl.exe check test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_thread_default.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_thread_default.xl -o release\tmp\cgen_thread_default.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_thread_default.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_thread_default.xl --emit release\tmp\cgen_thread_default.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_thread_release" release\tmp\cgen_thread_default.c >nul
if errorlevel 1 (
    echo cgen_thread_default.c missing thread release helper.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_ctor_args.xl >nul 2> release\tmp\sem_thread_ctor_args.err.txt
if not errorlevel 1 (
    echo sem_thread_ctor_args.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_entry_signature.xl >nul 2> release\tmp\sem_thread_entry_signature.err.txt
if not errorlevel 1 (
    echo sem_thread_entry_signature.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_method_args.xl >nul 2> release\tmp\sem_thread_method_args.err.txt
if not errorlevel 1 (
    echo sem_thread_method_args.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_thread_copy.xl >nul 2> release\tmp\sem_thread_copy.err.txt
if not errorlevel 1 (
    echo sem_thread_copy.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async.xl -o release\tmp\cgen_async.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_async.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_async.xl --emit release\tmp\cgen_async.c
if errorlevel 1 exit /b 1

findstr /C:"xTaskRunThread" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing xTaskRunThread.
    exit /b 1
)
findstr /C:"xl6_future_await_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await helper.
    exit /b 1
)
findstr /C:"xFutureRelease" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing future cleanup.
    exit /b 1
)
findstr /C:"__xl6_async_return_" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async return temporary.
    exit /b 1
)
findstr /C:".op.retain != 0" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async class function-field retain.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl_async_result_free_makeBoxAsync" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async class result free hook.
    exit /b 1
)
findstr /C:"pOut->pfnFreeValue = xl6_async_free_function_value" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing async function result free hook.
    exit /b 1
)
findstr /C:"fnA.retain(fnA.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing await function retain.
    exit /b 1
)
findstr /C:"secondBox.op.retain(secondBox.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing repeated await class retain.
    exit /b 1
)
findstr /C:"autoBox.op.retain(autoBox.op.env)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class retain.
    exit /b 1
)
findstr /C:"xl_class_AsyncBox__destroy(&autoBox)" release\tmp\cgen_async.c >nul
if errorlevel 1 (
    echo cgen_async.c missing auto await class cleanup.
    exit /b 1
)

release\xl.exe check test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_async_exception_propagation.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_async_exception_propagation.xl -o release\tmp\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_async_exception_propagation.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_async_exception_propagation.xl --emit release\tmp\cgen_async_exception_propagation.c
if errorlevel 1 exit /b 1

findstr /C:"xl6_error_raise(\"Error\", \"async boom\"" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing async throw raise.
    exit /b 1
)

findstr /C:"xl6_future_await_value" release\tmp\cgen_async_exception_propagation.c >nul
if errorlevel 1 (
    echo cgen_async_exception_propagation.c missing await helper.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_non_future.xl >nul 2> release\tmp\sem_await_non_future.err.txt
if not errorlevel 1 (
    echo sem_await_non_future.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_await_sync_func.xl >nul 2> release\tmp\sem_await_sync_func.err.txt
if not errorlevel 1 (
    echo sem_await_sync_func.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_return_future.xl >nul 2> release\tmp\sem_async_return_future.err.txt
if not errorlevel 1 (
    echo sem_async_return_future.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_multi_return.xl >nul 2> release\tmp\sem_async_multi_return.err.txt
if not errorlevel 1 (
    echo sem_async_multi_return.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_async_generator.xl >nul 2> release\tmp\sem_async_generator.err.txt
if not errorlevel 1 (
    echo sem_async_generator.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_generator.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_generator.xl -o release\tmp\cgen_generator.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_generator.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generator.xl --emit release\tmp\cgen_generator.c
if errorlevel 1 exit /b 1

findstr /C:"xrtCoCreate" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing xrtCoCreate.
    exit /b 1
)
findstr /C:"xl6_generator_yield_copy" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator yield helper.
    exit /b 1
)
findstr /C:"xl6_generator_next" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator next helper.
    exit /b 1
)
findstr /C:"xl6_generator_destroy" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing generator cleanup helper.
    exit /b 1
)
findstr /C:"xl_gen_worker_toggles" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing bool generator worker.
    exit /b 1
)
findstr /C:"xl_gen_worker_weights" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing float generator worker.
    exit /b 1
)
findstr /C:"12:34:56" release\tmp\cgen_generator.c >nul
if errorlevel 1 (
    echo cgen_generator.c missing time generator literal lowering.
    exit /b 1
)

release\xl.exe check test\cases\sem_yield_outside_generator.xl >nul 2> release\tmp\sem_yield_outside_generator.err.txt
if not errorlevel 1 (
    echo sem_yield_outside_generator.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_string_element.xl >nul 2> release\tmp\sem_generator_string_element.err.txt
if not errorlevel 1 (
    echo sem_generator_string_element.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_class_element.xl >nul 2> release\tmp\sem_generator_class_element.err.txt
if not errorlevel 1 (
    echo sem_generator_class_element.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_function_element.xl >nul 2> release\tmp\sem_generator_function_element.err.txt
if not errorlevel 1 (
    echo sem_generator_function_element.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_yield_type.xl >nul 2> release\tmp\sem_generator_yield_type.err.txt
if not errorlevel 1 (
    echo sem_generator_yield_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_generator_two_targets.xl >nul 2> release\tmp\sem_generator_two_targets.err.txt
if not errorlevel 1 (
    echo sem_generator_two_targets.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_generator_two_targets.err.txt >nul
if not errorlevel 1 (
    echo sem_generator_two_targets.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe run test\cases\cgen_channel.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_channel.xl -o release\tmp\cgen_channel.exe
if errorlevel 1 exit /b 1

release\tmp\cgen_channel.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_channel.xl --emit release\tmp\cgen_channel.c
if errorlevel 1 exit /b 1

findstr /C:"xrtChannelCreate" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing xrtChannelCreate.
    exit /b 1
)
findstr /C:"xl6_channel_send_value" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing send helper.
    exit /b 1
)
findstr /C:"xl6_channel_recv_into" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing recv helper.
    exit /b 1
)
findstr /C:"xrtChannelTryRecv" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing try recv path.
    exit /b 1
)
findstr /C:"12:34:56" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing time channel literal lowering.
    exit /b 1
)
findstr /C:"xl6_channel_send_value(closedCh" release\tmp\cgen_channel.c >nul
if errorlevel 1 (
    echo cgen_channel.c missing closed channel send assertion.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_string_element.xl >nul 2> release\tmp\sem_channel_string_element.err.txt
if not errorlevel 1 (
    echo sem_channel_string_element.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_send_type.xl >nul 2> release\tmp\sem_channel_send_type.err.txt
if not errorlevel 1 (
    echo sem_channel_send_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_recv_target_type.xl >nul 2> release\tmp\sem_channel_recv_target_type.err.txt
if not errorlevel 1 (
    echo sem_channel_recv_target_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_class_element.xl >nul 2> release\tmp\sem_channel_class_element.err.txt
if not errorlevel 1 (
    echo sem_channel_class_element.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_channel_function_element.xl >nul 2> release\tmp\sem_channel_function_element.err.txt
if not errorlevel 1 (
    echo sem_channel_function_element.xl should fail.
    exit /b 1
)

release\xl.exe xapi test\cases\xapi_doc.xl --emit release\tmp\xapi_doc.json
if errorlevel 1 exit /b 1

findstr /C:"\"doc\"" release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing doc object.
    exit /b 1
)

findstr /C:"Add two integers." release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing desc text.
    exit /b 1
)

findstr /C:"auto value = add(1, 2)" release\tmp\xapi_doc.json >nul
if errorlevel 1 (
    echo xapi_doc.json missing example text.
    exit /b 1
)

release\xl.exe xapi test\cases\xapi_json_escape.xl --emit release\tmp\xapi_json_escape.json
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'release\tmp\xapi_json_escape.json' -Raw | ConvertFrom-Json; $q=[char]34; $fields=$j.exports[0].doc.fields; $desc=($fields | Where-Object { $_.kind -eq 'desc' } | Select-Object -First 1).text; $example=($fields | Where-Object { $_.kind -eq 'example' } | Select-Object -First 1).text; if($desc -ne ('Quote '+$q+'demo'+$q+' and path c:\tmp\file.')){exit 1}; if($example -ne ('print('+$q+'hello\nworld'+$q+')')){exit 2}"
if errorlevel 1 (
    echo xapi_json_escape.json failed JSON escape roundtrip.
    exit /b 1
)

release\xl.exe check test\cases\sem_throw_int.xl >nul 2> release\tmp\sem_throw_int.err.txt
if not errorlevel 1 (
    echo sem_throw_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_rethrow_outside_catch.xl >nul 2> release\tmp\sem_rethrow_outside_catch.err.txt
if not errorlevel 1 (
    echo sem_rethrow_outside_catch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_assert_message_int.xl >nul 2> release\tmp\sem_assert_message_int.err.txt
if not errorlevel 1 (
    echo sem_assert_message_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_without_handler.xl >nul 2> release\tmp\sem_try_without_handler.err.txt
if not errorlevel 1 (
    echo sem_try_without_handler.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_error_unknown_member.xl >nul 2> release\tmp\sem_error_unknown_member.err.txt
if not errorlevel 1 (
    echo sem_error_unknown_member.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" release\tmp\sem_error_unknown_member.err.txt >nul
if not errorlevel 1 (
    echo sem_error_unknown_member.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_return_boundary.xl >nul 2> release\tmp\sem_try_return_boundary.err.txt
if not errorlevel 1 (
    echo sem_try_return_boundary.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_unknown_field.xl >nul 2> release\tmp\sem_doc_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_doc_unknown_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_naked_text.xl >nul 2> release\tmp\sem_doc_naked_text.err.txt
if not errorlevel 1 (
    echo sem_doc_naked_text.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_bad_param.xl >nul 2> release\tmp\sem_doc_bad_param.err.txt
if not errorlevel 1 (
    echo sem_doc_bad_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_duplicate_param.xl >nul 2> release\tmp\sem_doc_duplicate_param.err.txt
if not errorlevel 1 (
    echo sem_doc_duplicate_param.xl should fail.
    exit /b 1
)

release\xl.exe lex test\cases\invalid_range.xl > release\tmp\invalid_range.tokens.txt 2> release\tmp\invalid_range.err.txt
if not errorlevel 1 (
    echo invalid_range.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" release\tmp\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing ERROR token.
    exit /b 1
)
findstr /C:"range(...)" release\tmp\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing range diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\template_todo.xl > release\tmp\template_todo.ast.txt 2> release\tmp\template_todo.err.txt
if not errorlevel 1 (
    echo template_todo.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('release\tmp\template_todo.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x6A21)+([char]0x677F)+([char]0x5B57)+([char]0x7B26)+([char]0x4E32)+([char]0x7B2C)+([char]0x4E00)+([char]0x9636)+([char]0x6BB5)+([char]0x6682)+([char]0x672A)+([char]0x5B9E)+([char]0x73B0); if($text.IndexOf($expected) -lt 0){ Write-Host 'template_todo.err.txt missing template-string diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_json.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_json.xl --emit release\tmp\stdlib_json.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseJSON" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyJSON" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing xrtStringifyJSON extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_json_stringify" release\tmp\stdlib_json.c >nul
if errorlevel 1 (
    echo stdlib_json.c missing json.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_json.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_json.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_json.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_xson.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_xson.xl --emit release\tmp\stdlib_xson.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xrtParseXSON" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtParseXSON extern prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyXSON" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xrtStringifyXSON extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_xson_stringify" release\tmp\stdlib_xson.c >nul
if errorlevel 1 (
    echo stdlib_xson.c missing xson.stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_xson.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_xson.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_xson.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_print.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_print.xl --emit release\tmp\stdlib_print.c
if errorlevel 1 exit /b 1
findstr /C:"void xl6_xrt_print(xvalue values, bool newline)" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing print runtime helper.
    exit /b 1
)
findstr /C:"void* xrtStringifyJSON" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing xrtStringifyJSON runtime prototype.
    exit /b 1
)
findstr /C:"void* xrtStringifyXSON" release\tmp\stdlib_print.c >nul
if errorlevel 1 (
    echo stdlib_print.c missing xrtStringifyXSON runtime prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_print.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_print.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_print.exe > release\tmp\stdlib_print.out.txt
if errorlevel 1 exit /b 1
findstr /C:"null" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing null output.
    exit /b 1
)
findstr /C:"true false 123 -45" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing scalar output.
    exit /b 1
)
findstr /C:"[1,2,3]" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing array JSON output.
    exit /b 1
)
findstr /C:"name" release\tmp\stdlib_print.out.txt >nul
if errorlevel 1 (
    echo stdlib_print.out.txt missing dict JSON output.
    exit /b 1
)

release\xl.exe check test\cases\stdlib_error_paths.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_error_paths.xl --emit release\tmp\stdlib_error_paths.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtFileReadAll" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFileReadAll extern prototype.
    exit /b 1
)
findstr /C:"int xrtDirScanEx" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtDirScanEx runtime prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseJSON" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseJSON extern prototype.
    exit /b 1
)
findstr /C:"xvalue xrtParseXSON" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtParseXSON extern prototype.
    exit /b 1
)
findstr /C:"xregex* xrtRegexCreate" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtFormUrlEncodedDecodeTo" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtFormUrlEncodedDecodeTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtUrlResolveTo" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtUrlResolveTo runtime prototype.
    exit /b 1
)
findstr /C:"bool xrtExecCapture" release\tmp\stdlib_error_paths.c >nul
if errorlevel 1 (
    echo stdlib_error_paths.c missing xrtExecCapture runtime prototype.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text = Get-Content -Raw -Encoding UTF8 'release\tmp\stdlib_error_paths.c'; if ($text -notmatch 'xrtFormUrlEncodedDecodeTo\([^\)]*\) \) \{\s*xrtFree\(outText\);\s*return xl6_query_empty_string\(\);') { Write-Host 'stdlib_error_paths.c missing query decode failure cleanup.'; exit 1 }; if ($text -notmatch 'if \( !xl6_url_parse_view\(base, baseSize, &baseUrl\) \) \{ return xl6_url_empty_string\(\); \}') { Write-Host 'stdlib_error_paths.c missing url resolve invalid base fallback.'; exit 1 }; if ($text -notmatch 'if \( !xrtExecCapture\(&config, &result, timeout\) \) \{ xrtProcessResultUnit\(&result\); return xl6_process_empty_string\(\); \}') { Write-Host 'stdlib_error_paths.c missing process exec failure cleanup.'; exit 1 }; if ($text -notmatch 'xrtProcessResultUnit\(&result\);\s*return ok;') { Write-Host 'stdlib_error_paths.c missing process success result cleanup.'; exit 1 }"
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_error_paths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_error_paths.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_error_paths.exe > release\tmp\stdlib_error_paths.out.txt 2> release\tmp\stdlib_error_paths.err.txt
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_codec.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_codec.xl --emit release\tmp\stdlib_codec.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtHexEncode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtHexEncode extern prototype.
    exit /b 1
)
findstr /C:"void* xrtBase64Encode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing xrtBase64Encode extern prototype.
    exit /b 1
)
findstr /C:"const char* __xl_std_codec_base64_decode" release\tmp\stdlib_codec.c >nul
if errorlevel 1 (
    echo stdlib_codec.c missing codec.base64Decode helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_codec.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_codec.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_codec.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_hash.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_hash.xl --emit release\tmp\stdlib_hash.c
if errorlevel 1 exit /b 1
findstr /C:"size_t strlen(const char* text);" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing strlen extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32 extern prototype.
    exit /b 1
)
findstr /C:"unsigned int xrtHash32_WithSeed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash32_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64 extern prototype.
    exit /b 1
)
findstr /C:"unsigned long long xrtHash64_WithSeed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing xrtHash64_WithSeed extern prototype.
    exit /b 1
)
findstr /C:"unsigned int __xl_std_hash_u32_with_seed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u32WithSeed helper.
    exit /b 1
)
findstr /C:"unsigned long long __xl_std_hash_u64_with_seed" release\tmp\stdlib_hash.c >nul
if errorlevel 1 (
    echo stdlib_hash.c missing hash.u64WithSeed helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_hash.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_hash.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_hash.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_crypto.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_crypto.xl --emit release\tmp\stdlib_crypto.c
if errorlevel 1 exit /b 1
findstr /C:"void xrtSHA256" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing xrtSHA256 runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_sha512_hex" release\tmp\stdlib_crypto.c >nul
if errorlevel 1 (
    echo stdlib_crypto.c missing sha512 runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_crypto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_crypto.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_crypto.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_id_net.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_id_net.xl --emit release\tmp\stdlib_id_net.c
if errorlevel 1 exit /b 1
findstr /C:"void* xrtMakeXIDS" release\tmp\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtMakeXIDS extern prototype.
    exit /b 1
)
findstr /C:"void* xrtGetLocalName" release\tmp\stdlib_id_net.c >nul
if errorlevel 1 (
    echo stdlib_id_net.c missing xrtGetLocalName extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_id_net.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_id_net.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_id_net.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_url.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_url.xl --emit release\tmp\stdlib_url.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtUrlParseViewN" release\tmp\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing xrtUrlParseViewN runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_url_resolve" release\tmp\stdlib_url.c >nul
if errorlevel 1 (
    echo stdlib_url.c missing url resolve runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_url.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_url.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_url.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_query.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_query.xl --emit release\tmp\stdlib_query.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtPercentEncodeTo" release\tmp\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing xrtPercentEncodeTo runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_query_append" release\tmp\stdlib_query.c >nul
if errorlevel 1 (
    echo stdlib_query.c missing query append runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_query.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_query.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_query.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_regex.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_regex.xl --emit release\tmp\stdlib_regex.c
if errorlevel 1 exit /b 1
findstr /C:"xregex* xrtRegexCreate" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing xrtRegexCreate runtime prototype.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_xrt_regex_find" release\tmp\stdlib_regex.c >nul
if errorlevel 1 (
    echo stdlib_regex.c missing regex find runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_regex.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_regex.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_regex.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_process.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_process.xl --emit release\tmp\stdlib_process.c
if errorlevel 1 exit /b 1
findstr /C:"bool xrtExecCapture" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing xrtExecCapture runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_process_exec" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exec runtime helper.
    exit /b 1
)
findstr /C:"int xl6_xrt_process_exit_code" release\tmp\stdlib_process.c >nul
if errorlevel 1 (
    echo stdlib_process.c missing process exitCode runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_process.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_process.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_process.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\stdlib_http_server.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\stdlib_http_server.xl --emit release\tmp\stdlib_http_server.c
if errorlevel 1 exit /b 1
findstr /C:"xhttpdserver* xrtHttpdCreate" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing xrtHttpdCreate runtime prototype.
    exit /b 1
)
findstr /C:"void* xl6_xrt_http_server_create" release\tmp\stdlib_http_server.c >nul
if errorlevel 1 (
    echo stdlib_http_server.c missing http server runtime helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% release\tmp\stdlib_http_server.c test\cases\stdlib_http_server_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\stdlib_http_server.exe
if errorlevel 1 exit /b 1
release\tmp\stdlib_http_server.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -File tools\check_negative_diagnostics.ps1 -Artifacts
if errorlevel 1 exit /b 1

echo DEMO6 smoke tests ok.
exit /b 0
