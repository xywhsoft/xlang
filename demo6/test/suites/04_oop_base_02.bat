@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 2/8...

release\xl.exe cgen test\cases\cgen_dynamic_member_access.xl --emit build\test\cgen_dynamic_member_access.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_record_field_get_owned(__xl6_user_global_obj," build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing borrowed dynamic member access.
    exit /b 1
)
findstr /C:"xl6_record_field_get_owned(xl_fn_makeObj()," build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing temporary dynamic member retain-release helper.
    exit /b 1
)
findstr /C:"xl6_record_field_get_owned(xl6_record_field_get_owned(xl_fn_makeObj()," build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing nested temporary dynamic member retain-release helper.
    exit /b 1
)
findstr /C:"({ xvalue* __xl6_member_values_" build\test\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should not use GNU statement expressions for temporary dynamic members.
    exit /b 1
)
findstr /C:"xvalue* __xl6_member_assign_" build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing dynamic member assignment temporary.
    exit /b 1
)
findstr /C:"xl6_record_field_set_borrow(__xl6_user_global_obj," build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing direct dynamic member assignment lowering.
    exit /b 1
)
findstr /C:"xl6_record_field_set_borrow(xl6_record_field_get_owned(__xl6_user_global_nested, \"inner\", 0)," build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing nested dynamic member reference assignment lowering.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue* __xl6_nc_" build\test\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long __xl6_global_init_" build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing declaration-level dynamic member scalar temporary.
    exit /b 1
)
findstr /C:"declaredSafeMissing = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" build\test\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should route declaration-level dynamic member through scalar temporary.
    exit /b 1
)
findstr /C:"__xl6_user_global_declaredSafeMissing = __xl6_global_init_" build\test\cgen_dynamic_member_access.c >nul
if errorlevel 1 (
    echo cgen_dynamic_member_access.c missing declaration-level dynamic member scalar handoff.
    exit /b 1
)
findstr /C:"declaredSafeMissing = ((long long)xl6_xrt_value_to_i64_release(({" build\test\cgen_dynamic_member_access.c >nul
if not errorlevel 1 (
    echo cgen_dynamic_member_access.c should not use statement expressions for declaration-level dynamic member fallback.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_dynamic_member_access.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1
build\test\cgen_dynamic_member_access.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_dynamic_container_param.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_dynamic_container_param.xl --emit build\test\cgen_dynamic_container_param.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* xl_fn_take(xvalue* values)" build\test\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array parameter lowering.
    exit /b 1
)
findstr /C:"xvalue* xl_fn_choose(xvalue* values)" build\test\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic dict parameter lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_index_get_i64(values, (long long)(0LL))" build\test\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array index return.
    exit /b 1
)
findstr /C:"xl_fn_take(xl6_xrt_array_varargs(2," build\test\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic array literal argument.
    exit /b 1
)
findstr /C:"xl_fn_choose(xl6_xrt_dict_varargs(1," build\test\cgen_dynamic_container_param.c >nul
if errorlevel 1 (
    echo cgen_dynamic_container_param.c missing dynamic dict literal argument.
    exit /b 1
)
release\xl.exe build test\cases\cgen_dynamic_container_param.xl -o build\test\cgen_dynamic_container_param.exe
if errorlevel 1 exit /b 1
build\test\cgen_dynamic_container_param.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_member.xl --emit build\test\cgen_safe_member.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_safe_member.c' -Raw; if($t -notmatch '__xl6_user_global_valuesLength = xl6_nullable_box_[0-9a-f]+\(.*xl6TypedArrayCount\('){exit 1}"
if errorlevel 1 (
    echo cgen_safe_member.c missing typed array safe member lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_safe_member.c' -Raw; if($t -notmatch '__xl6_user_global_tableLength = xl6_nullable_box_[0-9a-f]+\(.*xl6TypedDictCount\('){exit 1}"
if errorlevel 1 (
    echo cgen_safe_member.c missing typed dict safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length(__xl6_user_global_dynamicValues)" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing dynamic array safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length(__xl6_user_global_missing)" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing null safe member lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length_release(xl_fn_makeArray())" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary array safe member release helper.
    exit /b 1
)
findstr /C:"xl6_xrt_member_safe_length_release(xl_fn_makeDynArray())" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing temporary dynamic safe member release helper.
    exit /b 1
)
findstr /C:"({ xvalue* __xl6_member_values_" build\test\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should not use GNU statement expressions for temporary safe members.
    exit /b 1
)
findstr /C:"expect_i64(((long long)xl6_xrt_value_to_i64_release(({ xvalue* __xl6_nc_" build\test\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should pre-evaluate null coalesce scalar arguments.
    exit /b 1
)
findstr /C:"long long __xl6_global_init_" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing declaration-level safe member scalar temporary.
    exit /b 1
)
findstr /C:"declaredLengthFallback = (long long)xl6_xrt_value_to_i64_release(__xl6_call_value_" build\test\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should route declaration-level safe member through scalar temporary.
    exit /b 1
)
findstr /C:"__xl6_user_global_declaredLengthFallback = __xl6_global_init_" build\test\cgen_safe_member.c >nul
if errorlevel 1 (
    echo cgen_safe_member.c missing declaration-level safe member scalar handoff.
    exit /b 1
)
findstr /C:"__xl6_user_global_declaredLengthFallback = ((long long)xl6_xrt_value_to_i64_release(({" build\test\cgen_safe_member.c >nul
if not errorlevel 1 (
    echo cgen_safe_member.c should not use statement expressions for declaration-level safe member fallback.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_safe_member.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_safe_member.exe
if errorlevel 1 exit /b 1
build\test\cgen_safe_member.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_varargs.xl --emit build\test\cgen_overload_varargs.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_fn_pick__string(\"x\"), 1LL);" build\test\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing fixed overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_fn_pick__string__array_u003C_var_u003E_(\"x\", xl6_xrt_array_varargs(1" build\test\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing vararg overload call.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(1" build\test\cgen_overload_varargs.c >nul
if errorlevel 1 (
    echo cgen_overload_varargs.c missing dict vararg overload call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_overload_varargs.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1
build\test\cgen_overload_varargs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\control_flow_inline_if.xl --emit build\test\control_flow_inline_if.c
if errorlevel 1 exit /b 1
findstr /C:"if ((value > 0LL))" build\test\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing first inline if.
    exit /b 1
)
findstr /C:"else {" build\test\control_flow_inline_if.c >nul
if errorlevel 1 (
    echo control_flow_inline_if.c missing inline else.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\control_flow_inline_if.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\control_flow_inline_if.exe
if errorlevel 1 exit /b 1
build\test\control_flow_inline_if.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_assign_ops.xl --emit build\test\cgen_assign_ops.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_user_global_a += 2LL;" build\test\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing compound assignment.
    exit /b 1
)
findstr /C:"__xl6_user_global_selected = (1 ? __xl6_user_global_inverted : 1LL);" build\test\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing ternary expression.
    exit /b 1
)
findstr /C:"__xl6_user_global_logicAnd = (1 && (!0));" build\test\cgen_assign_ops.c >nul
if errorlevel 1 (
    echo cgen_assign_ops.c missing logical expression.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_assign_ops.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_assign_ops.exe
if errorlevel 1 exit /b 1
build\test\cgen_assign_ops.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_null_coalesce.xl --emit build\test\cgen_null_coalesce.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* __xl6_global_init_" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce statement lowering.
    exit /b 1
)
findstr /C:"__xl6_global_init_" build\test\cgen_null_coalesce.c | findstr /C:"xrtValueInt((long long)(9LL));" >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce fallback assignment.
    exit /b 1
)
findstr /C:"__xl6_user_global_probed = __xl6_global_init_" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing single-evaluation null coalesce lowering.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -LiteralPath 'build\test\cgen_null_coalesce.c' -Raw -Encoding UTF8; if($t -notmatch 'xvalue\* __xl6_nc_\d+ = 0;' -or ([regex]::Matches($t, '__xl6_nc_\d+ = nc_probe_next_var\(\);')).Count -ne 1){exit 1}"
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null coalesce left temporary.
    exit /b 1
)
findstr /C:"long long __xl6_global_init_" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static single-evaluation null coalesce lowering.
    exit /b 1
)
findstr /C:"__xl6_global_init_" build\test\cgen_null_coalesce.c | findstr /C:"= 44LL;" >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce fallback assignment.
    exit /b 1
)
findstr /C:"XL6_Nullable_" build\test\cgen_null_coalesce.c | findstr /C:"__xl6_coalesce_" >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing typed nullable call argument slot.
    exit /b 1
)
findstr /C:"expect_i64(((__xl6_coalesce_" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing direct static null coalesce call argument.
    exit /b 1
)
findstr /C:".hasValue ? __xl6_coalesce_" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce value branch.
    exit /b 1
)
findstr /C:"__xl6_user_global_fallback = ({ xvalue* __xl6_nc_" build\test\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses var null coalesce statement expression.
    exit /b 1
)
findstr /C:"__xl6_user_global_probed = ({ xvalue* __xl6_nc_" build\test\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses probed null coalesce statement expression.
    exit /b 1
)
findstr /C:"__xl6_user_global_staticProbed = ({ long long __xl6_nc_" build\test\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses static null coalesce statement expression.
    exit /b 1
)
findstr /C:"expect_i64(({ long long __xl6_nc_" build\test\cgen_null_coalesce.c >nul
if not errorlevel 1 (
    echo cgen_null_coalesce.c still uses call argument null coalesce statement expression.
    exit /b 1
)
findstr /C:"__xl6_user_global_literal = xrtValueInt((long long)(7LL));" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing literal null coalesce boxing.
    exit /b 1
)
findstr /C:"__xl6_user_global_staticInt = 5LL;" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing static null coalesce inference.
    exit /b 1
)
findstr /C:"if (xl6_xrt_value_is_null(__xl6_user_global_assigned))" build\test\cgen_null_coalesce.c >nul
if errorlevel 1 (
    echo cgen_null_coalesce.c missing var null assignment lowering.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_null_coalesce.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1
build\test\cgen_null_coalesce.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_pow.xl --emit build\test\cgen_pow.c
if errorlevel 1 exit /b 1
findstr /C:"static long long xl6_int_pow_i64" build\test\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing int pow helper.
    exit /b 1
)
findstr /C:"__xl6_user_global_rightAssoc = xl6_int_pow_i64(2LL, xl6_int_pow_i64(3LL, 2LL));" build\test\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing right-associative pow lowering.
    exit /b 1
)
findstr /C:"__xl6_user_global_value = xl6_int_pow_i64(__xl6_user_global_value, 4LL);" build\test\cgen_pow.c >nul
if errorlevel 1 (
    echo cgen_pow.c missing pow assignment lowering.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_pow.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_pow.exe
if errorlevel 1 exit /b 1
build\test\cgen_pow.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_lifecycle.xl --emit build\test\cgen_native_class_lifecycle.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_xrt_type_value_id(rejectedCopy, false)" build\test\cgen_native_class_lifecycle.c >nul
if errorlevel 1 (
    echo cgen_native_class_lifecycle.c must query var type at runtime.
    exit /b 1
)
findstr /C:"struct XL_Class_Counter {" build\test\cgen_native_class_lifecycle.c >nul
if errorlevel 1 (
    echo cgen_native_class_lifecycle.c missing native-backed class handle struct.
    exit /b 1
)
findstr /C:"counter = xl6_box_XL_Class_Counter((XL_Class_Counter){counter_open(40LL)});" build\test\cgen_native_class_lifecycle.c >nul
if errorlevel 1 (
    echo cgen_native_class_lifecycle.c missing native-backed class direct create call.
    exit /b 1
)
findstr /C:"long long afterAdd = xl_class_Counter__add(((XL_Class_Counter*)xl6_class_data(counter" build\test\cgen_native_class_lifecycle.c >nul
if errorlevel 1 (
    echo cgen_native_class_lifecycle.c missing native-backed class direct method call.
    exit /b 1
)
findstr /C:"counter_close(*handle);" build\test\cgen_native_class_lifecycle.c >nul
if errorlevel 1 (
    echo cgen_native_class_lifecycle.c missing native-backed class direct destroy call.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_native_class_lifecycle.c test\cases\cgen_native_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_native_class_lifecycle.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_lifecycle.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class.xl --emit build\test\cgen_native_class.c
if errorlevel 1 exit /b 1
findstr /C:"void* counter_open(long long seed);" build\test\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native create C prototype.
    exit /b 1
)
findstr /C:"self->handle = counter_open(seed);" build\test\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native create wrapper.
    exit /b 1
)
findstr /C:"long long __xl6_return = counter_add(self->handle, value);" build\test\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native method wrapper.
    exit /b 1
)
findstr /C:"counter_close(self->handle);" build\test\cgen_native_class.c >nul
if errorlevel 1 (
    echo cgen_native_class.c missing native destroy wrapper.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_native_class.c test\cases\cgen_native_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_native_class.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_bind_point.xl --emit build\test\cgen_native_class_bind_point.c
if errorlevel 1 exit /b 1
findstr /C:"#define XL_RUNTIME_NEED_POINT_MOVE 1" build\test\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing native point move runtime gate.
    exit /b 1
)
findstr /C:"box = xl6_box_XL_Class_BorrowedBox((XL_Class_BorrowedBox){xl6_point_take((void**)&(raw))});" build\test\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing owning native point transfer.
    exit /b 1
)
findstr /C:"empty = xl6_box_XL_Class_BorrowedBox((XL_Class_BorrowedBox){xl6_point_take((void**)&(raw))});" build\test\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing repeated empty point transfer.
    exit /b 1
)
powershell -NoProfile -Command "$count=(Select-String -Path 'build\test\cgen_native_class_bind_point.c' -SimpleMatch 'xl6_point_take((void**)&(raw))').Count; if ($count -ne 3) { exit 1 }"
if errorlevel 1 (
    echo cgen_native_class_bind_point.c must transfer each point exactly once.
    exit /b 1
)
powershell -NoProfile -Command "$t=Get-Content 'build\test\cgen_native_class_bind_point.c' -Raw; if($t -notmatch 'xl_fn_consume\(xl6_xrt_bind_type\(__xl6_call_arg_[0-9]+,') { exit 1 }"
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing owned class argument move.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(__xl6_call_arg_" build\test\cgen_native_class_bind_point.c >nul
if not errorlevel 1 (
    echo cgen_native_class_bind_point.c retained an owned class argument temporary.
    exit /b 1
)
findstr /C:"if (empty != 0)" build\test\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing null-safe destroy guard.
    exit /b 1
)
findstr /C:"borrowed_box_close(self->handle);" build\test\cgen_native_class_bind_point.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point.c missing native destroy wrapper.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_native_class_bind_point.c test\cases\cgen_native_class_bind_point_host.c %XL6_CGEN_LINKS% -o build\test\cgen_native_class_bind_point.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_bind_point.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_field.xl --emit build\test\cgen_native_class_field.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_NativeBox__value(((XL_Class_NativeBox*)xl6_class_data(self->box" build\test\cgen_native_class_field.c >nul
if errorlevel 1 (
    echo cgen_native_class_field.c missing managed native class field method call.
    exit /b 1
)
findstr /C:"if (self->box != 0) { xrtValueRelease(self->box); self->box = 0; }" build\test\cgen_native_class_field.c >nul
if errorlevel 1 (
    echo cgen_native_class_field.c missing managed native class field cleanup.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_native_class_field.c test\cases\cgen_class_native_field_host.c %XL6_CGEN_LINKS% -o build\test\cgen_native_class_field.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_return_move.xl --emit build\test\cgen_native_class_return_move.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue* __xl6_return_" build\test\cgen_native_class_return_move.c >nul
if errorlevel 1 (
    echo cgen_native_class_return_move.c missing managed class return value.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(box)" build\test\cgen_native_class_return_move.c >nul
if errorlevel 1 (
    echo cgen_native_class_return_move.c missing retained managed class return.
    exit /b 1
)
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% build\test\cgen_native_class_return_move.c test\cases\cgen_class_native_field_host.c %XL6_CGEN_LINKS% -o build\test\cgen_native_class_return_move.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_return_move.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 2/8
exit /b 0
