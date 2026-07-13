@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 7/10...

release\xl.exe cgen test\cases\cgen_runtime_type_baseline.xl --emit build\test\cgen_runtime_type_baseline.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_type_value_id(boxedChild" build\test\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing boxed class runtime type query.
    exit /b 1
)
findstr /C:"xvoCreateObject(xl6_type_desc_XL_Class_Actor()" build\test\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing object-derived class heap allocation.
    exit /b 1
)
findstr /C:"xvoCreateHandle(xl6_type_desc_XL_Class_Native()" build\test\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing native-backed class handle runtime type boxing.
    exit /b 1
)
findstr /C:"xl6_box_XL_Union_Payload(payload)" build\test\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing union runtime type boxing.
    exit /b 1
)
findstr /C:"xvoCreateCallable(" build\test\cgen_runtime_type_baseline.c >nul
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing callable runtime type boxing.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_runtime_type_baseline.c' -Raw; if($t -notmatch 'xrtTypedArrayCreate\(xl6_runtime_type_desc_([0-9A-F]+)\(\),[\s\S]*?xl6_xrt_typed_array_to_xvalue\(xl6_runtime_type_desc_\1\(\), children\)'){exit 1}"
if errorlevel 1 (
    echo cgen_runtime_type_baseline.c missing typed class array runtime descriptor binding.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_runtime_type_baseline.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_runtime_type_baseline.exe
if errorlevel 1 exit /b 1
build\test\cgen_runtime_type_baseline.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_is_as.xl --emit build\test\cgen_runtime_is_as.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_runtime_value_is(" build\test\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime is helper.
    exit /b 1
)
findstr /C:"xl6_runtime_value_as_record(" build\test\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime record downcast helper.
    exit /b 1
)
findstr /C:"xl6_runtime_type_is(actual" build\test\cgen_runtime_is_as.c >nul
if errorlevel 1 (
    echo cgen_runtime_is_as.c missing runtime class relation helper.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_runtime_is_as.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_runtime_is_as.exe
if errorlevel 1 exit /b 1
build\test\cgen_runtime_is_as.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_runtime_as_fail.xl --emit build\test\cgen_runtime_as_fail.c
if errorlevel 1 (
    echo cgen_runtime_as_fail.xl should generate runtime failure path.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(4);" build\test\cgen_runtime_as_fail.c >nul
if errorlevel 1 (
    echo cgen_runtime_as_fail.c missing runtime as failure branch.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_as_container_target.xl >nul 2> build\test\sem_runtime_as_container_target.err.txt
if not errorlevel 1 (
    echo sem_runtime_as_container_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem] as" build\test\sem_runtime_as_container_target.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_as_container_target.err.txt missing as target diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_as_static_mismatch.xl >nul 2> build\test\sem_runtime_as_static_mismatch.err.txt
if not errorlevel 1 (
    echo sem_runtime_as_static_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"[sem] as" build\test\sem_runtime_as_static_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_as_static_mismatch.err.txt missing as source diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_runtime_is_var_target.xl >nul 2> build\test\sem_runtime_is_var_target.err.txt
if not errorlevel 1 (
    echo sem_runtime_is_var_target.xl should fail.
    exit /b 1
)
findstr /C:"[sem] is" build\test\sem_runtime_is_var_target.err.txt >nul
if errorlevel 1 (
    echo sem_runtime_is_var_target.err.txt missing is target diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_type_conversions.xl --emit build\test\cgen_type_conversions.c
if errorlevel 1 exit /b 1
findstr /C:"xrtStrToI64(" build\test\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to int conversion.
    exit /b 1
)
findstr /C:"xrtStrToNum(" build\test\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to float conversion.
    exit /b 1
)
findstr /C:"xrtStrToTime(" build\test\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing string to time conversion.
    exit /b 1
)
findstr /C:"xl6_xrt_i64_to_string" build\test\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing int to string helper.
    exit /b 1
)
findstr /C:"xl6_xrt_value_to_string(boxedInt)" build\test\cgen_type_conversions.c >nul
if errorlevel 1 (
    echo cgen_type_conversions.c missing var to string conversion.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_type_conversions.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_type_conversions.exe
if errorlevel 1 exit /b 1
build\test\cgen_type_conversions.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_implicit_methods.xl --emit build\test\cgen_implicit_methods.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_value_to_string_release" build\test\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing implicit toString lowering.
    exit /b 1
)
findstr /C:"xvoToString(value, 0)" build\test\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing XRT xvoToString bridge.
    exit /b 1
)
findstr /C:"xvoCreateInt((long long)(123LL))" build\test\cgen_implicit_methods.c >nul
if errorlevel 1 (
    echo cgen_implicit_methods.c missing literal toString boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_implicit_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1
build\test\cgen_implicit_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure.xl --emit build\test\cgen_closure.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function runtime.
    exit /b 1
)
findstr /C:"typedef struct XL6_Closure_" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure environment.
    exit /b 1
)
findstr /C:"void (*retain)(void* env);" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function retain field.
    exit /b 1
)
findstr /C:"__env->ref_count = 1;" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing closure ref count init.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" build\test\cgen_closure.c >nul
if not errorlevel 1 (
    echo cgen_closure.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'build\test\cgen_closure.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_closure.c still contains CRT free.
    exit /b 1
)
findstr /C:"copy.retain(copy.env);" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function copy retain.
    exit /b 1
)
findstr /C:"value = ((add10.signature == " build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function signature guard.
    exit /b 1
)
findstr /C:"((long long (*)(void*, long long))add10.invoke)(add10.env, 5LL)" build\test\cgen_closure.c >nul
if errorlevel 1 (
    echo cgen_closure.c missing function value invoke.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_closure.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_closure.exe
if errorlevel 1 exit /b 1
build\test\cgen_closure.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_closure_class_ctor_scope.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_closure_class_ctor_scope.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_closure_class_ctor_scope.xl --emit build\test\cgen_closure_class_ctor_scope.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Box__create(&(*__xl6_object_data_" build\test\cgen_closure_class_ctor_scope.c >nul
if errorlevel 1 (
    echo cgen_closure_class_ctor_scope.c should construct the heap object payload inside the closure invoke body.
    exit /b 1
)
findstr /C:"box = xvoCreateObject(xl6_type_desc_XL_Class_Box()" build\test\cgen_closure_class_ctor_scope.c >nul
if errorlevel 1 (
    echo cgen_closure_class_ctor_scope.c should publish the closure-local class handle.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_closure_class_ctor_scope.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_closure_class_ctor_scope.exe
if errorlevel 1 exit /b 1
build\test\cgen_closure_class_ctor_scope.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_closure_resource_capture.xl --emit build\test\cgen_closure_resource_capture.c
if errorlevel 1 exit /b 1
findstr /C:"__env->values = xl6_xrt_retain(values);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture retain.
    exit /b 1
)
findstr /C:"xvoUnref(__env->values);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing xvalue capture release.
    exit /b 1
)
findstr /C:"__env->inner.retain(__env->inner.env);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture retain.
    exit /b 1
)
findstr /C:"__env->inner.release(__env->inner.env);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function capture release.
    exit /b 1
)
findstr /C:"current.release(current.env);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing function assignment release.
    exit /b 1
)
findstr /C:".retain(__xl6_function_assign_" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c missing borrowed function assignment retain.
    exit /b 1
)
findstr /C:"xrtMalloc(sizeof(XL6_Closure_" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT allocation for closure env.
    exit /b 1
)
findstr /C:"xrtFree(__env);" build\test\cgen_closure_resource_capture.c >nul
if errorlevel 1 (
    echo cgen_closure_resource_capture.c should use XRT free for closure env.
    exit /b 1
)
findstr /C:"malloc(" build\test\cgen_closure_resource_capture.c >nul
if not errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'build\test\cgen_closure_resource_capture.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_closure_resource_capture.c still contains CRT free.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_closure_resource_capture.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1
build\test\cgen_closure_resource_capture.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_bare_call.xl --emit build\test\cgen_function_bare_call.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue (*invoke_var)(void* env, int argc, xvalue* argv);" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function invoke slot.
    exit /b 1
)
findstr /C:"static xvalue xl6_function_call_var(XL6_Function fn, int argc, ...)" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function call helper.
    exit /b 1
)
findstr /C:"xvoType(__xl6_argv[0]) != XL6_XVO_DT_INT" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased function argument type check.
    exit /b 1
)
findstr /C:"value = xl6_xrt_move_to_shared(xl6_function_call_var(add, 2" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing bare function call lowering.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1)" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg arity check.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_vararg, __xl6_argv[__xl6_i], false)" build\test\cgen_function_bare_call.c >nul
if errorlevel 1 (
    echo cgen_function_bare_call.c missing erased array vararg packing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_function_bare_call.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_bare_call.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_function_named_value.xl --emit build\test\cgen_function_named_value.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_fnvalue_xl_fn_add_invoke(void* __xl6_env, long long left, long long right)" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function typed wrapper.
    exit /b 1
)
findstr /C:"static xvalue xl6_fnvalue_xl_fn_add_invoke_var(void* __xl6_env, int __xl6_argc, xvalue* __xl6_argv)" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing named function erased wrapper.
    exit /b 1
)
findstr /C:"typed = xl6_fnvalue_xl_fn_add_make();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function value init.
    exit /b 1
)
findstr /C:"erased = xl6_fnvalue_xl_fn_add_make();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing bare function value init.
    exit /b 1
)
findstr /C:"oneDefault = xl6_fnvalue_xl_fn_addDefault_arity1_make();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional parameter function adapter.
    exit /b 1
)
findstr /C:"variadicDefault = xl6_fnvalue_xl_fn_totalDefault_arity0_vararg_make();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional vararg function adapter.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$text=[System.IO.File]::ReadAllText('build\test\cgen_function_named_value.c',[System.Text.Encoding]::UTF8); if($text.IndexOf('mixedPicker = (chooseMixedPicker ? xl6_function_copy_value(borrowedMixedPicker) : xl_fn_makePicker(200LL));') -lt 0 -or $text.IndexOf('XL6_Function __xl6_function_assign_') -lt 0 -or $text.IndexOf('= (chooseMixedPicker ? xl6_function_copy_value(borrowedMixedPicker) : xl_fn_makePicker(300LL));') -lt 0){ exit 1 }"
if errorlevel 1 (
    echo cgen_function_named_value.c missing mixed conditional function value branch copy.
    exit /b 1
)
findstr /C:"if (__xl6_argc < 1 || __xl6_argc > 2)" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing erased optional arity check.
    exit /b 1
)
findstr /C:"xl6_xrt_array_varargs(3" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function array vararg lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(2" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing typed function dict vararg lowering.
    exit /b 1
)
findstr /C:"static xrt_func_param __xl6_sig_params[2u];" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature parameter metadata array.
    exit /b 1
)
findstr /C:"static const struct xrt_type_desc* __xl6_sig_returns[1u];" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature return type metadata array.
    exit /b 1
)
findstr /C:"__xl6_sig.Params = __xl6_sig_params;" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature Params binding.
    exit /b 1
)
findstr /C:"__xl6_sig.ReturnTypes = __xl6_sig_returns;" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function signature ReturnTypes binding.
    exit /b 1
)
findstr /C:"__xl6_sig_params[1].Flags = 0x00000001U;" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing optional parameter metadata flag.
    exit /b 1
)
findstr /C:"__xl6_sig_params[0].Type = xrtTypeArray();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing array vararg metadata type.
    exit /b 1
)
findstr /C:"__xl6_sig_params[1].Type = xrtTypeDict();" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing dict vararg metadata type.
    exit /b 1
)
findstr /C:"__xl6_sig.Flags = 0x00000001U;" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing array vararg signature flag.
    exit /b 1
)
findstr /C:"__xl6_sig.Flags = 0x00000002U;" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing dict vararg signature flag.
    exit /b 1
)
findstr /C:"copy = (char*)xrtMalloc(len + 1u);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for function fallback string.
    exit /b 1
)
findstr /C:"copy = (char*)malloc(len + 1u);" build\test\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still uses CRT malloc for function fallback string.
    exit /b 1
)
findstr /C:"box = (XL6_Function_Callable_Env*)xrtMalloc(sizeof(XL6_Function_Callable_Env));" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for callable env.
    exit /b 1
)
findstr /C:"xrtFree(box);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for callable env.
    exit /b 1
)
findstr /C:"argv = (xvalue*)xrtMalloc(sizeof(xvalue) * (size_t)argc);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT allocation for erased call argv.
    exit /b 1
)
findstr /C:"xrtFree(argv);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c should use XRT free for erased call argv.
    exit /b 1
)
findstr /C:"malloc(" build\test\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT malloc.
    exit /b 1
)
powershell -NoProfile -Command "if (Select-String -Path 'build\test\cgen_function_named_value.c' -Pattern '(?<![A-Za-z0-9_])free\s*\(' -Quiet) { exit 1 }"
if errorlevel 1 (
    echo cgen_function_named_value.c still contains CRT free.
    exit /b 1
)
findstr /C:"static void xl6_function_fatal(int code)" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal helper.
    exit /b 1
)
findstr /C:"#include <__xl_runtime/xl_runtime.h>" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized runtime header.
    exit /b 1
)
findstr /C:"xl6_function_fatal(3);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing centralized function fatal call.
    exit /b 1
)
findstr /C:"xl6_runtime_fatal(code);" build\test\cgen_function_named_value.c >nul
if errorlevel 1 (
    echo cgen_function_named_value.c missing function fatal delegation.
    exit /b 1
)
findstr /C:"exit(3);" build\test\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 3.
    exit /b 1
)
findstr /C:"exit(2);" build\test\cgen_function_named_value.c >nul
if not errorlevel 1 (
    echo cgen_function_named_value.c still contains direct function runtime exit 2.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_function_named_value.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_function_named_value.exe
if errorlevel 1 exit /b 1
build\test\cgen_function_named_value.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_named_function_argument.xl --emit build\test\cgen_named_function_argument.c
if errorlevel 1 exit /b 1
findstr /C:"xl_fn_apply(5LL, xl6_fnvalue_xl_fn_doubleIt_make())" build\test\cgen_named_function_argument.c >nul
if errorlevel 1 (
    echo cgen_named_function_argument.c missing direct named function argument lowering.
    exit /b 1
)
findstr /C:"xl_fn_apply(5LL, xl6_fnvalue_xl_fn_withDefault_arity1_make())" build\test\cgen_named_function_argument.c >nul
if errorlevel 1 (
    echo cgen_named_function_argument.c missing optional parameter adapter argument lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_named_function_argument.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_named_function_argument.exe
if errorlevel 1 exit /b 1
build\test\cgen_named_function_argument.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 7/10
exit /b 0
