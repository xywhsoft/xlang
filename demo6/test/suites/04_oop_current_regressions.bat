@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: current OOP and parameter-mode regressions...

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
findstr /C:"xl_class_Box__int__take(" build\test\cgen_generic_multi_return.c >nul
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
rem class: default reference semantics, polymorphism and parameter modes.
rem ------------------------------------------------------------------
for %%F in (
    cgen_class_polymorphism
    cgen_class_null_identity
    cgen_param_modes_class
    cgen_param_modes_container
    cgen_param_modes_container_matrix
    cgen_struct_value_semantics
    cgen_out_definite_assignment
    cgen_function_param_modes
) do (
    release\xl.exe build test\cases\%%F.xl -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe cgen test\cases\cgen_function_param_modes.xl --emit build\test\cgen_function_param_modes.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_fn_increment(long long* value);" build\test\cgen_function_param_modes.c >nul
if errorlevel 1 (
    echo cgen_function_param_modes.c lost the byref pointer ABI.
    exit /b 1
)
findstr /C:"incrementer.invoke)(incrementer.env, &(number))" build\test\cgen_function_param_modes.c >nul
if errorlevel 1 (
    echo cgen_function_param_modes.c lost byref function-value address passing.
    exit /b 1
)
findstr /C:"producer.invoke)(producer.env, &(result))" build\test\cgen_function_param_modes.c >nul
if errorlevel 1 (
    echo cgen_function_param_modes.c lost out function-value address passing.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_polymorphism.xl --emit build\test\cgen_class_polymorphism.c
if errorlevel 1 exit /b 1
findstr /C:"xvoCreateRecordMove(xl6_type_desc_XL_Class_RefDerived()" build\test\cgen_class_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_polymorphism.c missing inline derived class boxing.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_array_append_value_release(values," build\test\cgen_class_polymorphism.c >nul
if errorlevel 1 (
    echo cgen_class_polymorphism.c missing inline class container append.
    exit /b 1
)

for %%F in (
    cgen_class_handle_lifetime_overload
    cgen_class_generic_handle
    cgen_class_copy
    cgen_class_fresh_return
) do (
    release\xl.exe build test\cases\%%F.xl test\cases\cgen_class_destroy_host.c -o build\test\%%F.exe
    if errorlevel 1 exit /b 1
    build\test\%%F.exe
    if errorlevel 1 exit /b 1
)

release\xl.exe build test\cases\cgen_class_null_dereference.xl -o build\test\cgen_class_null_dereference.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_null_dereference.exe >nul 2>nul
if errorlevel 5 (
    echo cgen_class_null_dereference.exe returned an unexpected code above 4.
    exit /b 1
)
if not errorlevel 4 (
    echo cgen_class_null_dereference.exe should fail with NullReferenceError code 4.
    exit /b 1
)

release\xl.exe abi test\cases\export_class_handle.xl --json --emit build\test\export_class_handle.abi.json
if errorlevel 1 exit /b 1
findstr /C:"fn:app.makeHandle(int) as ExportHandleBase" build\test\export_class_handle.abi.json >nul
if errorlevel 1 (
    echo export_class_handle.abi.json lost the canonical class return type.
    exit /b 1
)
findstr /C:"fn:app.readHandle(ExportHandleBase) as int" build\test\export_class_handle.abi.json >nul
if errorlevel 1 (
    echo export_class_handle.abi.json lost the canonical class parameter type.
    exit /b 1
)
release\xl.exe cgen test\cases\export_class_handle.xl --emit build\test\export_class_handle.c --emit-header build\test\export_class_handle.h
if errorlevel 1 exit /b 1
findstr /C:"xvalue xl6_class_make(int64_t value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_class_read(xvalue value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"void xl6_mode_increment(int64_t* value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"void xl6_mode_make(xvalue* value);" build\test\export_class_handle.h >nul
if errorlevel 1 exit /b 1
findstr /C:"fn:app.increment(byref int) as ()" build\test\export_class_handle.abi.json >nul
if errorlevel 1 exit /b 1
findstr /C:"fn:app.makeOut(out ExportHandleBase) as ()" build\test\export_class_handle.abi.json >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\export_class_handle.xl test\cases\export_class_handle_header_host.c -o build\test\export_class_handle_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_handle_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_struct_param_modes.xl --emit build\test\export_struct_param_modes.c --emit-header build\test\export_struct_param_modes.h
if errorlevel 1 exit /b 1
findstr /C:"XL_Struct_AbiPair xl6_pair_shift(XL_Struct_AbiPair value);" build\test\export_struct_param_modes.h >nul
if errorlevel 1 exit /b 1
findstr /C:"void xl6_pair_reset(XL_Struct_AbiPair* value);" build\test\export_struct_param_modes.h >nul
if errorlevel 1 exit /b 1
findstr /C:"void xl6_pair_make(XL_Struct_AbiPair* value);" build\test\export_struct_param_modes.h >nul
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\export_struct_param_modes.xl test\cases\export_struct_param_modes_host.c -o build\test\export_struct_param_modes_host.exe
if errorlevel 1 exit /b 1
build\test\export_struct_param_modes_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_package_class_handle_use.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_package_class_handle_use.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_package_class_handle_use.xl -o build\test\import_package_class_handle_use.exe
if errorlevel 1 exit /b 1
build\test\import_package_class_handle_use.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_class_recursive_object.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_class_recursive_object.xl -o build\test\cgen_class_recursive_object.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_recursive_object.exe
if errorlevel 1 exit /b 1

for %%F in (
    sem_closure_capture_destroy_class
    sem_closure_capture_native_class
    sem_closure_capture_native_class_field
    sem_noncopyable_class_copy_init
    sem_noncopyable_assign
    sem_noncopyable_native_class_param
    sem_typed_container_noncopyable_native_class
    sem_typed_container_noncopyable_class
    sem_typed_container_noncopyable_nested
) do (
    release\xl.exe check test\cases\%%F.xl
    if errorlevel 1 exit /b 1
)

release\xl.exe check test\cases\sem_param_modes_invalid.xl > build\test\sem_param_modes_invalid.err.txt 2>&1
if not errorlevel 1 (
    echo sem_param_modes_invalid.xl should fail.
    exit /b 1
)
findstr /C:"byref" /C:"out" build\test\sem_param_modes_invalid.err.txt >nul
if errorlevel 1 (
    echo sem_param_modes_invalid.xl missing parameter-mode diagnostic.
    exit /b 1
)

echo DEMO6 suite ok: current OOP and parameter-mode regressions
exit /b 0
