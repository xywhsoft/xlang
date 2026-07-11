@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: current OOP ref generic regressions...

release\xl.exe check test\cases\cgen_generic_multi_return.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generic_multi_return.xl --emit build\test\cgen_generic_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"xl_gen_pair__int(41LL)" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing inferred generic multi-return call.
    exit /b 1
)
findstr /C:"xl_gen_pair__string(\"ready\")" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing explicit generic multi-return call.
    exit /b 1
)
findstr /C:"xl_class_Box__int__take(&box)" build\test\cgen_generic_multi_return.c >nul
if errorlevel 1 (
    echo cgen_generic_multi_return.c missing generic class multi-return call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_multi_return.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_field_module_const.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_field_module_const.xl --emit build\test\cgen_class_field_module_const.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_class_field_module_const.c' -Raw; $intDecl=$t.IndexOf('extern long long DEFAULT_SCORE;'); $textDecl=$t.IndexOf('extern const char* DEFAULT_NAME;'); $intUse=$t.IndexOf('= DEFAULT_SCORE', $intDecl + 1); $textUse=$t.IndexOf('DEFAULT_NAME, 0u', $textDecl + 1); if($intDecl -lt 0 -or $textDecl -lt 0 -or $intUse -lt 0 -or $textUse -lt 0 -or $intDecl -ge $intUse -or $textDecl -ge $textUse){exit 1}"
if errorlevel 1 (
    echo cgen_class_field_module_const.c has an invalid module constant declaration order.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_field_module_const.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_field_module_const.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_field_module_const.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_generic_self_method.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_generic_self_method.xl --emit build\test\cgen_generic_self_method.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Ring__int__reset(self, initial);" build\test\cgen_generic_self_method.c >nul
if errorlevel 1 (
    echo cgen_generic_self_method.c lost the concrete generic class symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_self_method.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_self_method.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_self_method.exe
if errorlevel 1 exit /b 1

rem ------------------------------------------------------------------
rem ref<T>: explicit polymorphic references keep class value semantics.
rem ------------------------------------------------------------------
for %%F in (
    cgen_class_ref_polymorphism
    cgen_ref_null_identity
) do (
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_ref_polymorphism.xl --emit build\test\cgen_class_ref_polymorphism.c
if errorlevel 1 exit /b 1
findstr /C:"xvoCreateRecordMove(xl6_type_desc_XL_Class_RefDerived()" build\test\cgen_class_ref_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_ref_polymorphism.c missing inline derived ref move boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_append_value_release(values, __xl6_call_arg_" build\test\cgen_class_ref_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_ref_polymorphism.c missing inline ref container append.
    exit /b 1
)

for %%F in (
    cgen_class_ref_lifetime_overload
    cgen_ref_generic_class
    cgen_ref_from_class_value
    cgen_ref_from_fresh_return
) do (
    release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_destroy_host.c -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe cgen test\cases\cgen_ref_generic_class.xl --emit build\test\cgen_ref_generic_class.c
if errorlevel 1 exit /b 1
findstr /C:"&xl6_runtime_ref_ops" build\test\cgen_ref_generic_class.c >nul
if errorlevel 1 (
    echo cgen_ref_generic_class.c missing dedicated ref type operations.
    exit /b 1
)
findstr /C:"XL6_XRT_TYPE_KIND_REF" build\test\cgen_ref_generic_class.c >nul
if errorlevel 1 (
    echo cgen_ref_generic_class.c missing ref runtime descriptor.
    exit /b 1
)

release\xl.exe build test\cases\cgen_ref_null_dereference.xl -o build\test\cgen_ref_null_dereference.exe
if errorlevel 1 exit /b 1
build\test\cgen_ref_null_dereference.exe >nul 2>nul
if errorlevel 5 (
    echo cgen_ref_null_dereference.exe returned an unexpected code above 4.
    exit /b 1
)
if not errorlevel 4 (
    echo cgen_ref_null_dereference.exe should fail with NullReferenceError code 4.
    exit /b 1
)

release\xl.exe check test\cases\sem_ref_non_class.xl >nul 2>build\test\sem_ref_non_class.err.txt
if not errorlevel 1 (
    echo sem_ref_non_class.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\sem_ref_non_class.err.txt',[Text.Encoding]::UTF8); $expected=-join @(0x72,0x65,0x66,0x3C,0x54,0x3E,0x20,0x7684,0x20,0x54,0x20,0x5FC5,0x987B,0x662F,0x20,0x63,0x6C,0x61,0x73,0x73,0x20,0x7C7B,0x578B | ForEach-Object {[char]$_}); if(-not $text.Contains($expected)){exit 1}"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_ref_native_class.xl >nul 2>build\test\sem_ref_native_class.err.txt
if not errorlevel 1 (
    echo sem_ref_native_class.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\sem_ref_native_class.err.txt',[Text.Encoding]::UTF8); $expected=-join @(0x72,0x65,0x66,0x3C,0x54,0x3E,0x20,0x5F53,0x524D,0x4E0D,0x63A5,0x6536,0x20,0x6E,0x61,0x74,0x69,0x76,0x65,0x2D,0x62,0x61,0x63,0x6B,0x65,0x64,0x20,0x63,0x6C,0x61,0x73,0x73 | ForEach-Object {[char]$_}); if(-not $text.Contains($expected)){exit 1}"
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_ref_noncopyable_existing.xl >nul 2>build\test\sem_ref_noncopyable_existing.err.txt
if not errorlevel 1 (
    echo sem_ref_noncopyable_existing.xl should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[IO.File]::ReadAllText('build\test\sem_ref_noncopyable_existing.err.txt',[Text.Encoding]::UTF8); $expected=-join @(0x4E0D,0x53EF,0x590D,0x5236,0x20,0x63,0x6C,0x61,0x73,0x73,0x20,0x53EA,0x80FD,0x7528,0x65B0,0x6784,0x9020,0x503C,0x521B,0x5EFA,0x20,0x72,0x65,0x66 | ForEach-Object {[char]$_}); if(-not $text.Contains($expected)){exit 1}"
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_ref.xl --json --emit build\test\export_ref.abi.json
if errorlevel 1 exit /b 1
findstr /C:"fn:app.makeRef(int) as ref<ExportRefBase>" build\test\export_ref.abi.json >nul
if errorlevel 1 (
    echo export_ref.abi.json lost the canonical ref return type.
    exit /b 1
)
findstr /C:"fn:app.readRef(ref<ExportRefBase>) as int" build\test\export_ref.abi.json >nul
if errorlevel 1 (
    echo export_ref.abi.json lost the canonical ref parameter type.
    exit /b 1
)
release\xl.exe cgen test\cases\export_ref.xl --emit build\test\export_ref.c --emit-header build\test\export_ref.h
if errorlevel 1 exit /b 1
findstr /C:"xvalue xl6_ref_make(int64_t value);" build\test\export_ref.h >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_ref_read(xvalue value);" build\test\export_ref.h >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\export_ref.xl test\cases\export_ref_header_host.c -o build\test\export_ref_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_ref_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_package_ref_use.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_package_ref_use.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_package_ref_use.xl -o build\test\import_package_ref_use.exe
if errorlevel 1 exit /b 1
build\test\import_package_ref_use.exe
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: current OOP ref generic regressions
exit /b 0
