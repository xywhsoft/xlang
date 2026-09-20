@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 5/10...

rem Preserve all original programs; assert verified IR and actual behavior.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_core_call_value_contract.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_evaluation_lifetime.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_owned_expression_discard.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_function_value_ownership.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_callsite_binding.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_product_result.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_auto_inference_matrix.xl --emit build\test\cgen_auto_inference_matrix.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_var_arg_exprs.xl --emit build\test\cgen_call_var_arg_exprs.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_call_scalar_arg_exprs.xl --emit build\test\cgen_call_scalar_arg_exprs.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_return_exprs.xl --emit build\test\cgen_var_return_exprs.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_expr_statement_discard.xl --emit build\test\cgen_expr_statement_discard.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_array_literal_expr_elements.xl --emit build\test\cgen_array_literal_expr_elements.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_values.xl --emit build\test\cgen_var_values.c
if errorlevel 1 exit /b 1

for %%F in (
    cgen_auto_inference_matrix
    cgen_call_var_arg_exprs
    cgen_call_scalar_arg_exprs
    cgen_var_return_exprs
    cgen_expr_statement_discard
    cgen_array_literal_expr_elements
    cgen_var_values
) do (
    "%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/%%F.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.strict.exe
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl test\cases\cgen_class_host.c
    if errorlevel 1 exit /b 1
    release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_host.c -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

rem Both original dedicated C hosts remain independent ABI checks.
release\xl.exe cgen test\cases\cgen_call_var_arg_exprs.xl --emit build\test\cgen_call_var_arg_exprs_xlir_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_call_var_arg_exprs_host.c %XL6_CGEN_LINKS% -o build\test\xlir_call_var_arg_exprs_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_call_var_arg_exprs_host.exe
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_var_return_exprs.xl --emit build\test\xlir_var_return_exprs_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_var_return_exprs_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\xlir_var_return_exprs_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_var_return_exprs_host.exe
if errorlevel 1 exit /b 1

rem Keep the original recursive allocation workload and 10-second ceiling.
release\xl.exe check test\cases\perf_var_recursive_alloc.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\perf_var_recursive_alloc.xl --emit build\test\perf_var_recursive_alloc.c
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\perf_var_recursive_alloc.xl -o build\test\perf_var_recursive_alloc.exe
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "foreach($case in @(@('array 18','2584'),@('dict 23','28657'))){ $psi=[Diagnostics.ProcessStartInfo]::new(); $psi.FileName=(Resolve-Path 'build\test\perf_var_recursive_alloc.exe').Path; $psi.Arguments=$case[0]; $psi.UseShellExecute=$false; $psi.RedirectStandardOutput=$true; $psi.RedirectStandardError=$true; $p=[Diagnostics.Process]::Start($psi); if(-not $p.WaitForExit(10000)){ try{$p.Kill()}catch{}; throw 'Recursive var allocation exceeded the original 10-second limit.' }; $out=$p.StandardOutput.ReadToEnd().Trim(); $err=$p.StandardError.ReadToEnd(); if($p.ExitCode -ne 0 -or $out -ne $case[1] -or $err.Length -ne 0){ throw ('Recursive var allocation mismatch: '+$out+' '+$err) } }"
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: core C generation and runtime baseline 5/10
exit /b 0
