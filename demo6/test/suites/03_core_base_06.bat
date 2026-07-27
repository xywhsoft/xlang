@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 6/10...

release\xl.exe cgen test\cases\cgen_var_runtime_objects.xl --emit build\test\cgen_var_runtime_objects.c
if errorlevel 1 exit /b 1
findstr /C:"xvoCreateCallable(" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable xvalue boxing.
    exit /b 1
)
findstr /C:"xrtCallableCreate(fn.sig" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing callable signature binding.
    exit /b 1
)
findstr /C:"box->fn.invoke_xcall" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing function xcall dispatch.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 1u" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing multi-return xcall result.
    exit /b 1
)
findstr /C:"xrtCallResultSetValue(__xl6_result, 4u" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing overflow multi-return xcall result.
    exit /b 1
)
findstr /C:"xl6_fnvalue_xl_fn_pairDefault_arity1_xcall" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional multi-return adapter xcall.
    exit /b 1
)
findstr /C:"xl6_fnvalue_xl_fn_totalDefault_arity0_vararg_xcall" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing optional vararg adapter xcall.
    exit /b 1
)
findstr /C:"xrtFuncSigTypeId(" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing precise function type query.
    exit /b 1
)
findstr /C:"xvoCreateRecord(" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record boxing.
    exit /b 1
)
findstr /C:"xvoCreateHandle(" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing native-backed class handle boxing.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Class_Pair" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing class record type ops.
    exit /b 1
)
findstr /C:"static const xrt_type_ops xl6_type_ops_XL_Class_Native" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing native-backed class handle type ops.
    exit /b 1
)
findstr /C:"xl6_var_call(__xl6_user_global_boxedFn" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var function call.
    exit /b 1
)
findstr /C:"xl6_var_call_kwargs(__xl6_user_global_boxedOptions" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing dynamic var kwargs call.
    exit /b 1
)
findstr /C:"frame.Kwargs = kwargs" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing kwargs call frame binding.
    exit /b 1
)
findstr /C:"xvoTableRemove(__xl6_vararg, \"path\", 0)" build\test\cgen_var_runtime_objects.c >nul
if errorlevel 1 (
    echo cgen_var_runtime_objects.c missing fixed named parameter filtering.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_var_runtime_objects.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1
build\test\cgen_var_runtime_objects.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 6/10
exit /b 0
