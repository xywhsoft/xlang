@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: errors docs and f-string negatives...

release\xl.exe check test\cases\sem_throw_int.xl >nul 2> build\test\sem_throw_int.err.txt
if not errorlevel 1 (
    echo sem_throw_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_rethrow_outside_catch.xl >nul 2> build\test\sem_rethrow_outside_catch.err.txt
if not errorlevel 1 (
    echo sem_rethrow_outside_catch.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_assert_message_int.xl >nul 2> build\test\sem_assert_message_int.err.txt
if not errorlevel 1 (
    echo sem_assert_message_int.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_without_handler.xl >nul 2> build\test\sem_try_without_handler.err.txt
if not errorlevel 1 (
    echo sem_try_without_handler.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_error_unknown_member.xl >nul 2> build\test\sem_error_unknown_member.err.txt
if not errorlevel 1 (
    echo sem_error_unknown_member.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_error_unknown_member.err.txt >nul
if not errorlevel 1 (
    echo sem_error_unknown_member.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_try_return_boundary.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_try_finally_abrupt_boundary.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_try_finally_pending_control.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_try_finally_pending_control.xl -o build\test\cgen_try_finally_pending_control.exe
if errorlevel 1 exit /b 1
build\test\cgen_try_finally_pending_control.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_try_finally_pending_async.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_try_finally_pending_async.xl -o build\test\cgen_try_finally_pending_async.exe
if errorlevel 1 exit /b 1
build\test\cgen_try_finally_pending_async.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_try_finally_pending_control.xl --emit build\test\cgen_try_finally_pending_control.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_pending_kind_" build\test\cgen_try_finally_pending_control.c >nul
if errorlevel 1 (
    echo pending-control lowering state is missing.
    exit /b 1
)
findstr /C:"__xl6_try_finally_dispatch_" build\test\cgen_try_finally_pending_control.c >nul
if errorlevel 1 (
    echo pending-control finally dispatch label is missing.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_unknown_field.xl >nul 2> build\test\sem_doc_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_doc_unknown_field.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_naked_text.xl >nul 2> build\test\sem_doc_naked_text.err.txt
if not errorlevel 1 (
    echo sem_doc_naked_text.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_bad_param.xl >nul 2> build\test\sem_doc_bad_param.err.txt
if not errorlevel 1 (
    echo sem_doc_bad_param.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_doc_duplicate_param.xl >nul 2> build\test\sem_doc_duplicate_param.err.txt
if not errorlevel 1 (
    echo sem_doc_duplicate_param.xl should fail.
    exit /b 1
)

release\xl.exe lex test\cases\invalid_range.xl > build\test\invalid_range.tokens.txt 2> build\test\invalid_range.err.txt
if not errorlevel 1 (
    echo invalid_range.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" build\test\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing ERROR token.
    exit /b 1
)
findstr /C:"range(...)" build\test\invalid_range.tokens.txt >nul
if errorlevel 1 (
    echo invalid_range.tokens.txt missing range diagnostic.
    exit /b 1
)


release\xl.exe check test\cases\parse_invalid_extern_var_marker.xl >nul 2> build\test\parse_invalid_extern_var_marker.err.txt
if not errorlevel 1 (
    echo parse_invalid_extern_var_marker.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" build\test\parse_invalid_extern_var_marker.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_extern_var_marker.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\parse_invalid_export_var_marker.xl >nul 2> build\test\parse_invalid_export_var_marker.err.txt
if not errorlevel 1 (
    echo parse_invalid_export_var_marker.xl should fail.
    exit /b 1
)
findstr /L /C:"[parse]" build\test\parse_invalid_export_var_marker.err.txt >nul
if errorlevel 1 (
    echo parse_invalid_export_var_marker.err.txt missing parse diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\parse_fstring_bad_brace.xl > build\test\parse_fstring_bad_brace.ast.txt 2> build\test\parse_fstring_bad_brace.err.txt
if not errorlevel 1 (
    echo parse_fstring_bad_brace.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\parse_fstring_bad_brace.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x5355)+([char]0x72EC)+([char]0x7684)+' }'; if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_bad_brace.err.txt missing bad brace diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\parse_fstring_empty_expr.xl > build\test\parse_fstring_empty_expr.ast.txt 2> build\test\parse_fstring_empty_expr.err.txt
if not errorlevel 1 (
    echo parse_fstring_empty_expr.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\parse_fstring_empty_expr.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x4E0D)+([char]0x80FD)+([char]0x4E3A)+([char]0x7A7A); if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_empty_expr.err.txt missing empty expr diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\parse_fstring_unclosed.xl > build\test\parse_fstring_unclosed.ast.txt 2> build\test\parse_fstring_unclosed.err.txt
if not errorlevel 1 (
    echo parse_fstring_unclosed.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\parse_fstring_unclosed.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x672A)+([char]0x95ED)+([char]0x5408)+([char]0x7684)+' f-string '+([char]0x63D2)+([char]0x503C)+([char]0x8868)+([char]0x8FBE)+([char]0x5F0F); if($text.IndexOf($expected) -lt 0){ Write-Host 'parse_fstring_unclosed.err.txt missing unclosed expr diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: errors docs and f-string negatives
exit /b 0
