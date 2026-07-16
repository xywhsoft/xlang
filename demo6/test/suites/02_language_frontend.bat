@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: language frontend and type semantics...

release\xl.exe lex release\examples\literals.xl > build\test\literals.tokens.txt
if errorlevel 1 exit /b 1

release\xl.exe parse release\examples\literals.xl > build\test\literals.ast.txt
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_numeric_literals.xl --emit build\test\cgen_numeric_literals.c
if errorlevel 1 exit /b 1
findstr /C:"long long dec;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing decimal global declaration.
    exit /b 1
)
findstr /C:"dec = 1000000LL;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing decimal literal normalization.
    exit /b 1
)
findstr /C:"hexA = 42LL;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing hex literal normalization.
    exit /b 1
)
findstr /C:"octA = 42LL;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing oct literal normalization.
    exit /b 1
)
findstr /C:"binA = 42LL;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing bin literal normalization.
    exit /b 1
)
findstr /C:"sci = 12.5e1;" build\test\cgen_numeric_literals.c >nul
if errorlevel 1 (
    echo cgen_numeric_literals.c missing float underscore normalization.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_numeric_literals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_numeric_literals.exe
if errorlevel 1 exit /b 1
build\test\cgen_numeric_literals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_int_literal_overflow.xl --emit build\test\cgen_int_literal_overflow.c >nul 2> build\test\cgen_int_literal_overflow.err.txt
if not errorlevel 1 (
    echo cgen_int_literal_overflow.xl should fail.
    exit /b 1
)
findstr /C:"integer literal is too large or invalid" build\test\cgen_int_literal_overflow.err.txt >nul
if errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt missing integer literal overflow diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\cgen_int_literal_overflow.err.txt >nul
if not errorlevel 1 (
    echo cgen_int_literal_overflow.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check release\examples\containers.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\containers.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\container_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\container_methods.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\container_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\container_methods.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\container_ops.xl
if errorlevel 1 exit /b 1

release\xl.exe run release\examples\container_ops.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\layout.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\comments.xl
if errorlevel 1 exit /b 1

release\xl.exe lex test\cases\sem_unclosed_comment.xl > build\test\sem_unclosed_comment.tokens.txt 2> build\test\sem_unclosed_comment.err.txt
if not errorlevel 1 (
    echo sem_unclosed_comment.xl should fail.
    exit /b 1
)
findstr /C:"ERROR" build\test\sem_unclosed_comment.tokens.txt >nul
if errorlevel 1 (
    echo sem_unclosed_comment.tokens.txt missing ERROR token.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\sem_unclosed_comment.tokens.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x672A)+([char]0x95ED)+([char]0x5408)+([char]0x7684)+([char]0x591A)+([char]0x884C)+([char]0x6CE8)+([char]0x91CA); if($text.IndexOf($expected) -lt 0){ Write-Host 'sem_unclosed_comment.tokens.txt missing unclosed-comment diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_case_sensitive.xl --emit build\test\cgen_case_sensitive.c
if errorlevel 1 exit /b 1
findstr /C:"long long Name;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing upper-case Name.
    exit /b 1
)
findstr /C:"Name = 1LL;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing upper-case Name init.
    exit /b 1
)
findstr /C:"long long name;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing lower-case name.
    exit /b 1
)
findstr /C:"name = 2LL;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing lower-case name init.
    exit /b 1
)
findstr /C:"long long NAME;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing all-caps NAME.
    exit /b 1
)
findstr /C:"NAME = 3LL;" build\test\cgen_case_sensitive.c >nul
if errorlevel 1 (
    echo cgen_case_sensitive.c missing all-caps NAME init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_case_sensitive.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_case_sensitive.exe
if errorlevel 1 exit /b 1
build\test\cgen_case_sensitive.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_context_keywords.xl --emit build\test\cgen_context_keywords.c
if errorlevel 1 exit /b 1
findstr /C:"long long self;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary self variable.
    exit /b 1
)
findstr /C:"self = 1LL;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary self init.
    exit /b 1
)
findstr /C:"long long base;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary base variable.
    exit /b 1
)
findstr /C:"base = 2LL;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing ordinary base init.
    exit /b 1
)
findstr /C:"long long xl_kw_static;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing escaped static context identifier.
    exit /b 1
)
findstr /C:"xl_kw_static = 8LL;" build\test\cgen_context_keywords.c >nul
if errorlevel 1 (
    echo cgen_context_keywords.c missing escaped static context identifier init.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_context_keywords.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_context_keywords.exe
if errorlevel 1 exit /b 1
build\test\cgen_context_keywords.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_context_keyword_function.xl --emit build\test\cgen_context_keyword_function.c
if errorlevel 1 exit /b 1
findstr /C:"xl_fn_static(void)" build\test\cgen_context_keyword_function.c >nul
if errorlevel 1 (
    echo cgen_context_keyword_function.c missing escaped static function symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_context_keyword_function.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_context_keyword_function.exe
if errorlevel 1 exit /b 1
build\test\cgen_context_keyword_function.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_unicode_identifiers.xl --emit build\test\cgen_unicode_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"xl_fn__u52A0__u4E00_(long long" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode function name.
    exit /b 1
)
findstr /C:"long long _u6570__u503C_" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode parameter name.
    exit /b 1
)
findstr /C:"long long _u7ED3__u679C_" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded unicode variable name.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_unicode_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_long_identifiers.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_long_identifiers.xl --emit build\test\cgen_long_identifiers.c --emit-header build\test\cgen_long_identifiers.h
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$c=Get-Content -Raw -Encoding UTF8 'build\test\cgen_long_identifiers.c'; $h=Get-Content -Raw -Encoding UTF8 'build\test\cgen_long_identifiers.h'; $hashes=[regex]::Matches($c, '_h[0-9A-F]{16}'); if($hashes.Count -lt 8){exit 1}; $unique=@($hashes.Value | Sort-Object -Unique); if($unique.Count -lt 8){exit 2}; $type=[regex]::Match($h, 'typedef struct (XL_Class_[A-Za-z0-9_]+)\s*\{').Groups[1].Value; if([string]::IsNullOrEmpty($type) -or $c.IndexOf('struct '+$type) -lt 0){exit 3}"
if errorlevel 1 (
    echo cgen_long_identifiers.c missing unique bounded identifier hashes.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_long_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_long_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_long_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_args.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_defaults.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\varargs.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\call_named_vararg.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\containers.xl > build\test\containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_containers.xl > build\test\cgen_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_in_containers.xl > build\test\cgen_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_in_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_for_in_containers.xl > build\test\cgen_for_in_containers.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_for_in_containers.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_for_in_var_non_container.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow.xl > build\test\control_flow.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_do_while.xl > build\test\control_flow_do_while.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_do_while.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\control_flow_inline_if.xl > build\test\control_flow_inline_if.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\control_flow_inline_if.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\operators.xl > build\test\operators.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\operators.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_null_coalesce.xl > build\test\cgen_null_coalesce.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_null_coalesce.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\member_index.xl > build\test\member_index.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\member_index.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generic_types.xl > build\test\generic_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generic_types.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_container_literal_target_types.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_container_literal_target_item_mismatch.xl > build\test\sem_container_literal_target_item_mismatch.err.txt 2>&1
if not errorlevel 1 (
    echo sem_container_literal_target_item_mismatch.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build/test/sem_container_literal_target_item_mismatch.err.txt'); $count=([regex]::Matches($text,'\[sem\].*target (array|list|set|dict)<[^>]+>, value')).Count; if($count -ne 4){ Write-Host ('expected 4 container literal item mismatch diagnostics, got '+$count); exit 1 }"
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\type_values.xl > build\test\type_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_values.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_type_name_identifiers.xl --emit build\test\cgen_type_name_identifiers.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_type_name_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_type_name_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_type_name_identifiers.exe
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\type_aliases.xl > build\test\type_aliases.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\type_aliases.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_string_concat.xl > build\test\cgen_string_concat.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_concat.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_compare.xl > build\test\cgen_string_compare.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_compare.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_string_literals.xl > build\test\cgen_string_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_string_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_fstring.xl > build\test\cgen_fstring.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_fstring.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_time_literals.xl > build\test\cgen_time_literals.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_time_literals.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\cgen_auto_expressions.xl > build\test\cgen_auto_expressions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_auto_expressions.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\invalid_numeric_operator.xl > build\test\invalid_numeric_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_numeric_operator.xl should fail.
    exit /b 1
)
findstr /C:"numeric operator operands must be numeric" build\test\invalid_numeric_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_numeric_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_integer_operator.xl > build\test\invalid_integer_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_integer_operator.xl should fail.
    exit /b 1
)
findstr /C:"integer operator operands must be integer" build\test\invalid_integer_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_integer_operator.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_float.xl > build\test\sem_pow_float.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_float.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" build\test\sem_pow_float.err.txt >nul
if errorlevel 1 (
    echo sem_pow_float.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_pow_var.xl > build\test\sem_pow_var.err.txt 2>&1
if not errorlevel 1 (
    echo sem_pow_var.xl should fail.
    exit /b 1
)
findstr /C:"power operator operands must be integer" build\test\sem_pow_var.err.txt >nul
if errorlevel 1 (
    echo sem_pow_var.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_division_by_zero.xl > build\test\invalid_division_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_division_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" build\test\invalid_division_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_division_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_modulo_by_zero.xl > build\test\invalid_modulo_by_zero.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_modulo_by_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" build\test\invalid_modulo_by_zero.diag.txt >nul
if errorlevel 1 (
    echo invalid_modulo_by_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_var_target.xl > build\test\sem_compound_var_target.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_var_target.xl should fail.
    exit /b 1
)
findstr /C:"var compound assignment target is not supported" build\test\sem_compound_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_compound_var_target.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left var, right int" build\test\sem_compound_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_compound_var_target.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_numeric_type.xl > build\test\sem_compound_numeric_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_numeric_type.xl should fail.
    exit /b 1
)
findstr /C:"numeric compound assignment operands must be numeric" build\test\sem_compound_numeric_type.err.txt >nul
if errorlevel 1 (
    echo sem_compound_numeric_type.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left int, right bool" build\test\sem_compound_numeric_type.err.txt >nul
if errorlevel 1 (
    echo sem_compound_numeric_type.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_float_to_int.xl > build\test\sem_compound_float_to_int.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_float_to_int.xl should fail.
    exit /b 1
)
findstr /C:"compound assignment result type does not match target type" build\test\sem_compound_float_to_int.err.txt >nul
if errorlevel 1 (
    echo sem_compound_float_to_int.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left int, right float" build\test\sem_compound_float_to_int.err.txt >nul
if errorlevel 1 (
    echo sem_compound_float_to_int.xl missing compound diagnostic context.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_div_zero.xl > build\test\sem_compound_div_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_div_zero.xl should fail.
    exit /b 1
)
findstr /C:"division by zero literal" build\test\sem_compound_div_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_div_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_compound_mod_zero.xl > build\test\sem_compound_mod_zero.err.txt 2>&1
if not errorlevel 1 (
    echo sem_compound_mod_zero.xl should fail.
    exit /b 1
)
findstr /C:"modulo by zero literal" build\test\sem_compound_mod_zero.err.txt >nul
if errorlevel 1 (
    echo sem_compound_mod_zero.xl missing expected diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\invalid_unary_operator.xl > build\test\invalid_unary_operator.diag.txt 2>&1
if not errorlevel 1 (
    echo invalid_unary_operator.xl should fail.
    exit /b 1
)
findstr /C:"bitwise not operand must be integer" build\test\invalid_unary_operator.diag.txt >nul
if errorlevel 1 (
    echo invalid_unary_operator.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\cgen_var_values.xl > build\test\cgen_var_values.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_var_values.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_type_conversions.xl > build\test\cgen_type_conversions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_type_conversions.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\cgen_implicit_methods.xl > build\test\cgen_implicit_methods.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\cgen_implicit_methods.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\generics.xl > build\test\generics.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\sem_generic_constraints.xl > build\test\sem_generic_constraints.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_generic_constraint_unsatisfied.xl > build\test\sem_generic_constraint_unsatisfied.err.txt 2>&1
if not errorlevel 1 (
    echo sem_generic_constraint_unsatisfied.xl should fail.
    exit /b 1
)
findstr /C:"generic type argument does not satisfy constraint" build\test\sem_generic_constraint_unsatisfied.err.txt >nul
if errorlevel 1 (
    echo sem_generic_constraint_unsatisfied.err.txt missing generic constraint diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_generic_constraint_unknown.xl > build\test\sem_generic_constraint_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_generic_constraint_unknown.xl should fail.
    exit /b 1
)
findstr /C:"unknown generic constraint" build\test\sem_generic_constraint_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_generic_constraint_unknown.err.txt missing unknown constraint diagnostic.
    exit /b 1
)
release\xl.exe check release\examples\generics.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\generic_identity.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\generic_constraints.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\generic_class_iter.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\runtime_type.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\runtime_var_bool_type.xl
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\native_counter.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\overloads.xl > build\test\overloads.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe parse test\cases\overload_varargs.xl > build\test\overload_varargs.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\overload_varargs.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_overload_var_requires_explicit.xl > build\test\sem_overload_var_requires_explicit.err.txt 2>&1
if not errorlevel 1 (
    echo sem_overload_var_requires_explicit.xl should fail.
    exit /b 1
)
findstr /C:"dynamic var argument requires explicit 'as Type' conversion before overload resolution" build\test\sem_overload_var_requires_explicit.err.txt >nul
if errorlevel 1 (
    echo sem_overload_var_requires_explicit.err.txt missing explicit conversion diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_multi_return_type_diagnostic.xl > build\test\sem_multi_return_type_diagnostic.err.txt 2>&1
if not errorlevel 1 (
    echo sem_multi_return_type_diagnostic.xl should fail.
    exit /b 1
)
findstr /C:"multi-return assignment item 2 type mismatch: target Color, return Ray; targets (bool, Color, Ray), returns (bool, Ray, Color)" build\test\sem_multi_return_type_diagnostic.err.txt >nul
if errorlevel 1 (
    echo sem_multi_return_type_diagnostic.err.txt missing ordered signature diagnostic.
    exit /b 1
)

release\xl.exe parse test\cases\function_types.xl > build\test\function_types.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\function_types.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\anonymous_functions.xl > build\test\anonymous_functions.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\anonymous_functions.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\anonymous_functions.xl
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\class_native_backed.xl > build\test\class_native_backed.ast.txt
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\class_native_backed.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_native_backed.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_native_bridge.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\class_point.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\object_protocol.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_protocol_overload.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_protocol_overload.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_property_get_let.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_property_get_let.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_static_access.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_static_access.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_protected_access.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_protected_access.xl
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\oop_virtual_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\oop_virtual_dispatch.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_static_access.xl --emit build\test\cgen_class_static_access.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl_class_Counter____static__total = 2LL;" build\test\cgen_class_static_access.c >nul
if errorlevel 1 (
    echo cgen_class_static_access.c missing static class field storage.
    exit /b 1
)
findstr /C:"long long xl_class_Counter__add(long long value);" build\test\cgen_class_static_access.c >nul
if errorlevel 1 (
    echo cgen_class_static_access.c missing static method prototype without self.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_static_access.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_static_access.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_static_access.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_virtual.xl --emit build\test\cgen_class_virtual.c
if errorlevel 1 exit /b 1
findstr /C:"struct XL6_VTable_Entity" build\test\cgen_class_virtual.c >nul
if errorlevel 1 (
    echo cgen_class_virtual.c missing class vtable type.
    exit /b 1
)
findstr /C:"xl6_vtable_Entity__Player" build\test\cgen_class_virtual.c >nul
if errorlevel 1 (
    echo cgen_class_virtual.c missing derived vtable object.
    exit /b 1
)
findstr /C:").base.__xl6_vt_Entity = &xl6_vtable_Entity__Player;" build\test\cgen_class_virtual.c >nul
if errorlevel 1 (
    echo cgen_class_virtual.c missing heap object derived vtable initialization.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_virtual.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_virtual.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_virtual.exe
if errorlevel 1 exit /b 1
release\xl.exe check release\examples\copy_clone.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\class_persistence.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\class_field_persistence_attrs.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_protocol_valid.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_protocol_unknown_reserved.xl > build\test\sem_protocol_unknown_reserved.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_unknown_reserved.xl should fail.
    exit /b 1
)
findstr /C:"__unknown__" build\test\sem_protocol_unknown_reserved.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_unknown_reserved.err.txt missing protocol diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_item_arity.xl > build\test\sem_protocol_item_arity.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_item_arity.xl should fail.
    exit /b 1
)
findstr /C:"__item__" build\test\sem_protocol_item_arity.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_item_arity.err.txt missing protocol arity diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_index_multi_container.xl > build\test\sem_index_multi_container.err.txt 2>&1
if not errorlevel 1 (
    echo sem_index_multi_container.xl should fail.
    exit /b 1
)
findstr /C:"exactly one key" build\test\sem_index_multi_container.err.txt >nul
if errorlevel 1 (
    echo sem_index_multi_container.err.txt missing multi-index container diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_readonly.xl > build\test\sem_class_property_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_readonly.xl should fail.
    exit /b 1
)
findstr /C:"class property assignment requires matching let" build\test\sem_class_property_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_readonly.err.txt missing readonly property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_readonly.xl > build\test\sem_class_property_compound_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_readonly.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching let" build\test\sem_class_property_compound_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_readonly.err.txt missing readonly compound property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_writeonly.xl > build\test\sem_class_property_compound_writeonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_writeonly.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching get" build\test\sem_class_property_compound_writeonly.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_writeonly.err.txt missing writeonly compound property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_setter_type.xl > build\test\sem_class_property_compound_setter_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_setter_type.xl should fail.
    exit /b 1
)
findstr /C:"class property compound assignment requires matching let" build\test\sem_class_property_compound_setter_type.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_setter_type.err.txt missing setter type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_null_coalesce_assign.xl > build\test\sem_class_property_null_coalesce_assign.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_null_coalesce_assign.xl should fail.
    exit /b 1
)
findstr /C:"class property does not support ?=" build\test\sem_class_property_null_coalesce_assign.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_null_coalesce_assign.err.txt missing null-coalescing property diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_class_property_compound_narrowing.xl > build\test\sem_class_property_compound_narrowing.err.txt 2>&1
if not errorlevel 1 (
    echo sem_class_property_compound_narrowing.xl should fail.
    exit /b 1
)
findstr /C:"compound assignment result type does not match target type" build\test\sem_class_property_compound_narrowing.err.txt >nul
if errorlevel 1 (
    echo sem_class_property_compound_narrowing.err.txt missing compound narrowing diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_field_key_type.xl > build\test\sem_protocol_field_key_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_field_key_type.xl should fail.
    exit /b 1
)
findstr /C:"__field__" build\test\sem_protocol_field_key_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_field_key_type.err.txt missing protocol field key diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_copyable_return.xl > build\test\sem_protocol_copyable_return.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_copyable_return.xl should fail.
    exit /b 1
)
findstr /C:"__copyable__" build\test\sem_protocol_copyable_return.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_copyable_return.err.txt missing protocol return diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_optional_param.xl > build\test\sem_protocol_optional_param.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_optional_param.xl should fail.
    exit /b 1
)
findstr /C:"__item__" build\test\sem_protocol_optional_param.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_optional_param.err.txt missing protocol optional parameter diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_operator_arg_type.xl > build\test\sem_protocol_operator_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_operator_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_operator_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_operator_arg_type.err.txt missing protocol operator type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_contains_arg_type.xl > build\test\sem_protocol_contains_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_contains_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_contains_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_contains_arg_type.err.txt missing protocol contains type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_call_arg_type.xl > build\test\sem_protocol_call_arg_type.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_call_arg_type.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_protocol_call_arg_type.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_call_arg_type.err.txt missing protocol call type diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_len_readonly.xl > build\test\sem_protocol_len_readonly.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_len_readonly.xl should fail.
    exit /b 1
)
findstr /C:"member length is read-only" build\test\sem_protocol_len_readonly.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_len_readonly.err.txt missing length readonly diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_protocol_next_returns.xl > build\test\sem_protocol_next_returns.err.txt 2>&1
if not errorlevel 1 (
    echo sem_protocol_next_returns.xl should fail.
    exit /b 1
)
findstr /C:"__next__" build\test\sem_protocol_next_returns.err.txt >nul
if errorlevel 1 (
    echo sem_protocol_next_returns.err.txt missing __next__ diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_valid.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\sem_derive_unknown.xl > build\test\sem_derive_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_unknown.xl should fail.
    exit /b 1
)
findstr /C:"derive" build\test\sem_derive_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_derive_unknown.err.txt missing derive diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_duplicate.xl > build\test\sem_derive_duplicate.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_duplicate.xl should fail.
    exit /b 1
)
findstr /C:"derive" build\test\sem_derive_duplicate.err.txt >nul
if errorlevel 1 (
    echo sem_derive_duplicate.err.txt missing derive duplicate diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_move_conflict.xl > build\test\sem_derive_move_conflict.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_move_conflict.xl should fail.
    exit /b 1
)
findstr /C:"moveOnly" build\test\sem_derive_move_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_derive_move_conflict.err.txt missing derive conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_copy_noncopyable_field.xl > build\test\sem_derive_copy_noncopyable_field.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_copy_noncopyable_field.xl should fail.
    exit /b 1
)
findstr /C:"derive copy/clone" build\test\sem_derive_copy_noncopyable_field.err.txt >nul
if errorlevel 1 (
    echo sem_derive_copy_noncopyable_field.err.txt missing copy field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_derive_value_native_class_field.xl > build\test\sem_derive_value_native_class_field.err.txt 2>&1
if not errorlevel 1 (
    echo sem_derive_value_native_class_field.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_derive_value_native_class_field.err.txt >nul
if errorlevel 1 (
    echo sem_derive_value_native_class_field.err.txt missing persistence field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_json_parse_without_derive.xl > build\test\sem_json_parse_without_derive.err.txt 2>&1
if not errorlevel 1 (
    echo sem_json_parse_without_derive.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_json_parse_without_derive.err.txt >nul
if errorlevel 1 (
    echo sem_json_parse_without_derive.err.txt missing persistence target diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_json_stringify_without_derive.xl > build\test\sem_json_stringify_without_derive.err.txt 2>&1
if not errorlevel 1 (
    echo sem_json_stringify_without_derive.xl should fail.
    exit /b 1
)
findstr /C:"derive value/json/xson" build\test\sem_json_stringify_without_derive.err.txt >nul
if errorlevel 1 (
    echo sem_json_stringify_without_derive.err.txt missing persistence stringify diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_unknown.xl > build\test\sem_field_attr_unknown.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_unknown.xl should fail.
    exit /b 1
)
findstr /C:"unknown field persistence annotation" build\test\sem_field_attr_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_unknown.err.txt missing unknown annotation diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_conflict.xl > build\test\sem_field_attr_conflict.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_conflict.xl should fail.
    exit /b 1
)
findstr /C:"skip conflicts with include" build\test\sem_field_attr_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_conflict.err.txt missing conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_static.xl > build\test\sem_field_attr_static.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_static.xl should fail.
    exit /b 1
)
findstr /C:"static field cannot use persistence annotation" build\test\sem_field_attr_static.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_static.err.txt missing static field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_field_attr_bad_name.xl > build\test\sem_field_attr_bad_name.err.txt 2>&1
if not errorlevel 1 (
    echo sem_field_attr_bad_name.xl should fail.
    exit /b 1
)
findstr /C:"name option must be a string" build\test\sem_field_attr_bad_name.err.txt >nul
if errorlevel 1 (
    echo sem_field_attr_bad_name.err.txt missing name option diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\cgen_contextual_identifiers.xl
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "[System.IO.File]::WriteAllText('build\test\source_without_final_newline.xl','println(1 + 2)',(New-Object System.Text.UTF8Encoding($false)))"
if errorlevel 1 exit /b 1
release\xl.exe check build\test\source_without_final_newline.xl
if errorlevel 1 (
    echo source without final newline should pass parser.
    exit /b 1
)
release\xl.exe run build\test\source_without_final_newline.xl > build\test\source_without_final_newline.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\source_without_final_newline.out.txt'); if($text.TrimEnd([char[]]@([char]13,[char]10)) -ne '3'){ exit 1 }"
if errorlevel 1 (
    echo source without final newline produced unexpected output.
    exit /b 1
)
release\xl.exe check release\examples\second_stage.xl
if errorlevel 1 exit /b 1
release\xl.exe run release\examples\second_stage.xl %XL6_XL_DEFAULT_LINK_ARGS%
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_undeclared_identifier.xl > build\test\sem_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_undeclared_identifier.xl:2:19: [sem] undeclared identifier 'missingValue'" build\test\sem_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_undeclared_identifier.err.txt missing source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_fstring_undeclared_identifier.xl > build\test\sem_fstring_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_fstring_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_fstring_undeclared_identifier.xl:2:28: [sem] undeclared identifier 'missingValue'" build\test\sem_fstring_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_fstring_undeclared_identifier.err.txt missing rebased source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_fstring_multiline_undeclared_identifier.xl > build\test\sem_fstring_multiline_undeclared_identifier.err.txt 2>&1
if not errorlevel 1 (
    echo sem_fstring_multiline_undeclared_identifier.xl should fail.
    exit /b 1
)
findstr /C:"test\cases\sem_fstring_multiline_undeclared_identifier.xl:3:8: [sem] undeclared identifier 'missingValue'" build\test\sem_fstring_multiline_undeclared_identifier.err.txt >nul
if errorlevel 1 (
    echo sem_fstring_multiline_undeclared_identifier.err.txt missing multiline rebased diagnostic.
    exit /b 1
)

release\xl.exe run release\examples\top_level_main.xl %XL6_XL_DEFAULT_LINK_ARGS% > build\test\top_level_main.out.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\top_level_main.out.txt'; if($lines.Count -lt 2 -or $lines[0] -ne 'top-level init' -or $lines[1] -ne 'main body'){ exit 1 }"
if errorlevel 1 (
    echo release\examples\top_level_main.xl output order failed.
    exit /b 1
)


echo DEMO6 suite ok: language frontend and type semantics
exit /b 0

