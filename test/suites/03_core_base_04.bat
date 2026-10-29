@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
set "XL6_SUITE_PREPARED=1"
echo Running DEMO6 suite: core C generation and runtime baseline 4/10...

rem Replace legacy C-local spelling snapshots with verified IR contracts.
rem All original language fixtures and behavior assertions remain below.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_core_expression_contract.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_compare_contract.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_special_f64_constant_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_index_write_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_typed_container_index_assign_objects_functions_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_multi_assign_index.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_index_compound_assign.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_type_value_abi.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_interpolated_string.ps1
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_time_approx_abi.ps1
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_typed_container_index_assign.xl --emit build\test\cgen_typed_container_index_assign.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_index_assign_objects_functions.xl --emit build\test\cgen_typed_container_index_assign_objects_functions.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_multi_assign_index.xl --emit build\test\cgen_typed_container_multi_assign_index.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_typed_container_index_compound_assign.xl --emit build\test\cgen_typed_container_index_compound_assign.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\type_values.xl --emit build\test\type_values.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_minimal_runtime.xl --emit build\test\cgen_minimal_runtime.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\type_aliases.xl --emit build\test\type_aliases.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_concat.xl --emit build\test\cgen_string_concat.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_compare.xl --emit build\test\cgen_string_compare.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_literals.xl --emit build\test\cgen_string_literals.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_embedded_nul.xl --emit build\test\cgen_string_embedded_nul.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_string_persistence_default_nul.xl --emit build\test\cgen_string_persistence_default_nul.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_fstring.xl --emit build\test\cgen_fstring.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_time_literals.xl --emit build\test\cgen_time_literals.c
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_auto_expressions.xl --emit build\test\cgen_auto_expressions.c
if errorlevel 1 exit /b 1

for %%F in (
    cgen_typed_container_index_assign
    cgen_typed_container_index_assign_objects_functions
    cgen_typed_container_multi_assign_index
    cgen_typed_container_index_compound_assign
    type_values
    cgen_minimal_runtime
    type_aliases
    cgen_string_concat
    cgen_string_compare
    cgen_string_literals
    cgen_string_embedded_nul
    cgen_string_persistence_default_nul
    cgen_fstring
    cgen_time_literals
    cgen_auto_expressions
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

rem User entry is a separate contract from module initialization.
release\xl.exe cgen test\cases\cgen_top_level_before_main.xl --emit build\test\cgen_top_level_before_main.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE_USER_ENTRY -DXLIR_GENERATED_MODULE=\"build/test/cgen_top_level_before_main.c\" test\cases\xlir_generated_module_host.c %XL6_CGEN_LINKS% -o build\test\cgen_top_level_before_main.strict.exe
if errorlevel 1 exit /b 1
build\test\cgen_top_level_before_main.strict.exe > build\test\cgen_top_level_before_main.strict.out.txt 2> build\test\cgen_top_level_before_main.strict.err.txt
if not "%errorlevel%"=="7" (
    echo cgen_top_level_before_main.strict.exe should return 7.
    exit /b 1
)
release\xl.exe run test\cases\cgen_top_level_before_main.xl > build\test\cgen_top_level_before_main.out.txt 2> build\test\cgen_top_level_before_main.err.txt
if not "%errorlevel%"=="7" (
    echo cgen_top_level_before_main.xl run should return 7.
    exit /b 1
)
release\xl.exe build test\cases\cgen_top_level_before_main.xl -o build\test\cgen_top_level_before_main.exe
if errorlevel 1 exit /b 1
build\test\cgen_top_level_before_main.exe > build\test\cgen_top_level_before_main.exe.out.txt 2> build\test\cgen_top_level_before_main.exe.err.txt
if not "%errorlevel%"=="7" (
    echo cgen_top_level_before_main.exe should return 7.
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$prefix='build\test\cgen_top_level_before_main'; foreach($suffix in @('','.strict','.exe')){ $out=[IO.File]::ReadAllText($prefix+$suffix+'.out.txt'); $err=[IO.File]::ReadAllBytes($prefix+$suffix+'.err.txt'); if($out -notmatch '\Atop:main\r?\n\z' -or $err.Length -ne 0){ throw 'User main/top-level ordering or error output changed.' } }"
if errorlevel 1 exit /b 1

rem Keep the original dedicated string-comparison host independently wired.
release\xl.exe cgen test\cases\cgen_string_compare.xl --backend xlir --emit build\test\xlir_string_compare_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_string_compare_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\xlir_string_compare_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_string_compare_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_auto_inference_matrix.xl
if errorlevel 1 exit /b 1
echo DEMO6 suite ok: core C generation and runtime baseline 4/10
exit /b 0
