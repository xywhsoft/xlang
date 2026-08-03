@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: enum and match...

for %%F in (
    enum_numeric_runtime
    enum_match_matrix
    enum_nested_generic
    enum_recursive_container
    enum_tagged_lifecycle
    enum_async_closure
    cgen_generic_enum_match_top_level
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
    release\xl.exe run test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

for %%F in (
    sem_enum_backing_type_error
    sem_enum_value_range_error
    sem_enum_payload_arity_error
    sem_enum_recursive_direct_error
    sem_enum_recursive_mutual_error
    sem_enum_recursive_generic_error
    sem_enum_match_branch_type_error
    sem_enum_match_duplicate_pattern_error
    sem_enum_match_else_order_error
    sem_enum_match_non_exhaustive_error
    sem_enum_match_wrong_scrutinee_error
) do (
    release\xl.exe check test\cases\%%F.xl > build\test\%%F.err.txt 2>&1
    if not errorlevel 1 (
        echo %%F.xl should fail.
        exit /b 1
    )
    findstr /C:"[sem]" build\test\%%F.err.txt >nul
    if errorlevel 1 (
        echo %%F.xl missing semantic diagnostic.
        type build\test\%%F.err.txt
        exit /b 1
    )
)

release\xl.exe check test\cases\import_enum_namespace.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_enum_namespace.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_enum_selective.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_enum_selective.xl
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_enum_abi.xl --emit build\test\export_enum_abi.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"numeric_enum HttpStatus" build\test\export_enum_abi.abi.txt >nul
if errorlevel 1 exit /b 1
findstr /C:"tagged_enum Packet" build\test\export_enum_abi.abi.txt >nul
if errorlevel 1 exit /b 1
findstr /C:"variant failure(code:int,message:string)" build\test\export_enum_abi.abi.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_enum_abi.xl --json --emit build\test\export_enum_abi.abi.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"numeric_enum\"" build\test\export_enum_abi.abi.json >nul
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"tagged_enum\"" build\test\export_enum_abi.abi.json >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_enum_abi.xl --emit build\test\export_enum_abi.c --emit-header build\test\export_enum_abi.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Enum_Packet__int" build\test\export_enum_abi.h >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_enum_make_XL_Enum_Packet__string__failure" build\test\export_enum_abi.h >nul
if errorlevel 1 exit /b 1
findstr /C:"xtarray __xl6_cabi_arg_0 = xrtTypedArrayClone(values);" build\test\export_enum_abi.c >nul
if errorlevel 1 exit /b 1

"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_enum_abi.c test\cases\export_enum_abi_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_enum_abi_host.exe
if errorlevel 1 exit /b 1
build\test\export_enum_abi_host.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\export_enum_abi.xl -shared -o build\test\export_enum_abi.dll --emit-header build\test\export_enum_abi_shared.h
if errorlevel 1 exit /b 1
release\xl.exe impdef build\test\export_enum_abi.dll -o build\test\export_enum_abi.def
if errorlevel 1 exit /b 1
findstr /C:"enum_make_packet" build\test\export_enum_abi.def >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_enum_copy_XL_Enum_Packet__int" build\test\export_enum_abi.def >nul
if errorlevel 1 exit /b 1
findstr /C:"xl6_enum_type_XL_Enum_Packet__int" build\test\export_enum_abi.def >nul
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: enum and match
exit /b 0
