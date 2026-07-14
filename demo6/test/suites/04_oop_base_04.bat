@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 4/8...

release\xl.exe cgen test\cases\cgen_derive_persistence.xl --emit build\test\cgen_derive_persistence.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Player______toValue____" build\test\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing derived toValue protocol.
    exit /b 1
)
findstr /C:"xl_class_Player______fromValue____" build\test\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing derived fromValue protocol.
    exit /b 1
)
findstr /C:"__xl6__json__stringify__value(const char* (*stringify_fn)(xvalue, int)" build\test\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing typed JSON class stringify helper.
    exit /b 1
)
findstr /C:"__xl6__xson__stringify__value(const char* (*stringify_fn)(xvalue)" build\test\cgen_derive_persistence.c >nul
if errorlevel 1 (
    echo cgen_derive_persistence.c missing typed XSON class stringify helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_derive_persistence.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_derive_persistence.exe
if errorlevel 1 exit /b 1
build\test\cgen_derive_persistence.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_field_persistence_attrs.xl --emit build\test\cgen_field_persistence_attrs.c
if errorlevel 1 exit /b 1
findstr /C:"display_name" build\test\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing json field alias.
    exit /b 1
)
findstr /C:"displayNameX" build\test\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing xson field alias.
    exit /b 1
)
findstr /C:"secret_name" build\test\cgen_field_persistence_attrs.c >nul
if errorlevel 1 (
    echo cgen_field_persistence_attrs.c missing included private field alias.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_field_persistence_attrs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_field_persistence_attrs.exe
if errorlevel 1 exit /b 1
build\test\cgen_field_persistence_attrs.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_overloads.xl --emit build\test\cgen_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Pair__create__void(XL_Class_Pair* self);" build\test\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing no-arg create overload symbol.
    exit /b 1
)
findstr /C:"void xl_class_Pair__create__int(XL_Class_Pair* self, long long value);" build\test\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing int create overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__int(XL_Class_Ops* self, long long value);" build\test\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class int method overload symbol.
    exit /b 1
)
findstr /C:"long long xl_class_Ops__pick__string(XL_Class_Ops* self, const char* text);" build\test\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing class string method overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Base__pick(&((XL_Class_Child*)xl6_class_data(child" build\test\cgen_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_class_overloads.c missing inherited method base receiver path.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_overloads.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\cgen_unicode_identifiers.xl --emit build\test\cgen_unicode_identifiers.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"symbol: xl_fn__u52A0__u4E00___int" build\test\cgen_unicode_identifiers.abi.txt >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.abi.txt missing encoded func symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_unicode_identifiers.xl --emit build\test\cgen_unicode_identifiers.c
if errorlevel 1 exit /b 1
findstr /C:"struct XL_Class__u70B9_ {" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class type.
    exit /b 1
)
findstr /C:"long long _u6A2A_;" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class field.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00_(" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function symbol.
    exit /b 1
)
findstr /C:"long long xl_fn__u52A0__u4E00_(long long _u6570__u503C_)" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded function parameter.
    exit /b 1
)
findstr /C:"long long xl_class__u70B9____u6C42__u548C_(XL_Class__u70B9_* self);" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class method symbol.
    exit /b 1
)
findstr /C:"_u7ED3__u679C_ = xl_fn__u52A0__u4E00_(41LL);" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded local variable.
    exit /b 1
)
findstr /C:"xvalue _u5750__u6807_;" build\test\cgen_unicode_identifiers.c >nul
if errorlevel 1 (
    echo cgen_unicode_identifiers.c missing encoded class variable.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_unicode_identifiers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1
build\test\cgen_unicode_identifiers.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_destroy.xl --emit build\test\cgen_class_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Tracker__destroy(XL_Class_Tracker* self);" build\test\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing destroy prototype.
    exit /b 1
)
findstr /C:"xvoUnref(tracker);" build\test\cgen_class_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_destroy.c missing scope destroy call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_destroy.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_cleanup_order.xl --emit build\test\cgen_cleanup_order.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\cgen_cleanup_order.c'; $second=($lines | Select-String -SimpleMatch 'xvoUnref(second);' | Select-Object -Last 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'xvoUnref(first);' | Select-Object -Last 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_cleanup_order.c should destroy later local before earlier local.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_cleanup_order.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1
build\test\cgen_cleanup_order.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_inherit_destroy.xl --emit build\test\cgen_class_inherit_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_AutoChild__destroy(XL_Class_AutoChild* self);" build\test\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing inherited auto destroy prototype.
    exit /b 1
)
findstr /C:"xl_class_Base__destroy(&self->base);" build\test\cgen_class_inherit_destroy.c >nul
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c missing base destroy tail call.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\cgen_class_inherit_destroy.c'; $body=($lines | Select-String -SimpleMatch 'record_destroy(self->value);' | Select-Object -First 1).LineNumber; $base=($lines | Select-String -SimpleMatch 'xl_class_Base__destroy(&self->base);' | Select-Object -First 1).LineNumber; if(-not $body -or -not $base -or $body -ge $base){exit 1}"
if errorlevel 1 (
    echo cgen_class_inherit_destroy.c should run child destroy body before base destroy tail call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_inherit_destroy.c test\cases\cgen_class_destroy_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_inherit_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_native_field.xl --emit build\test\cgen_class_native_field.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Holder__destroy(XL_Class_Holder* self);" build\test\cgen_class_native_field.c >nul
if errorlevel 1 (
    echo cgen_class_native_field.c missing auto destroy prototype.
    exit /b 1
)
findstr /C:"xl6_box_XL_Class_NativeBox((XL_Class_NativeBox){native_box_open(10LL)})" build\test\cgen_class_native_field.c >nul
if errorlevel 1 (
    echo cgen_class_native_field.c missing first native-backed field create.
    exit /b 1
)
findstr /C:"xvoUnref(self->second);" build\test\cgen_class_native_field.c >nul
if errorlevel 1 (
    echo cgen_class_native_field.c missing second native-backed field cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(self->first);" build\test\cgen_class_native_field.c >nul
if errorlevel 1 (
    echo cgen_class_native_field.c missing first native-backed field cleanup.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$lines=Get-Content 'build\test\cgen_class_native_field.c'; $second=($lines | Select-String -SimpleMatch 'xvoUnref(self->second);' | Select-Object -Last 1).LineNumber; $first=($lines | Select-String -SimpleMatch 'xvoUnref(self->first);' | Select-Object -Last 1).LineNumber; if(-not $second -or -not $first -or $second -ge $first){exit 1}"
if errorlevel 1 (
    echo cgen_class_native_field.c should destroy native-backed fields in reverse declaration order.
    exit /b 1
)
findstr /C:"xvoUnref(holder);" build\test\cgen_class_native_field.c >nul
if errorlevel 1 (
    echo cgen_class_native_field.c missing holder scope cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_native_field.c test\cases\cgen_class_native_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_native_field.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_native_field.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_field_method.xl --emit build\test\cgen_native_class_field_method.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_NativeBox__value(((XL_Class_NativeBox*)xl6_class_data(__xl6_call_receiver_" build\test\cgen_native_class_field_method.c >nul
if errorlevel 1 (
    echo cgen_native_class_field_method.c missing native-backed class field method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_field_method.c test\cases\cgen_class_native_field_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_field_method.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_field_method.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_native_class_overloads.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_native_class_overloads.xl --emit build\test\cgen_native_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_NativeScore__add__int(XL_Class_NativeScore* self, long long value)" build\test\cgen_native_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_native_class_overloads.c missing int native-backed overload ABI wrapper.
    exit /b 1
)
findstr /C:"xl_class_NativeScore__add__int(((XL_Class_NativeScore*)xl6_class_data(score" build\test\cgen_native_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_native_class_overloads.c missing int native-backed direct overload call.
    exit /b 1
)
findstr /C:"long long xl_class_NativeScore__add__string(XL_Class_NativeScore* self, const char* text)" build\test\cgen_native_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_native_class_overloads.c missing string native-backed overload ABI wrapper.
    exit /b 1
)
findstr /C:"xl_class_NativeScore__add__string(((XL_Class_NativeScore*)xl6_class_data(score" build\test\cgen_native_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_native_class_overloads.c missing string native-backed direct overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_overloads.c test\cases\cgen_native_class_overloads_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_overloads_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_overloads_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overloads.xl --emit build\test\cgen_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_fn_pick__int(long long value);" build\test\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing int overload symbol.
    exit /b 1
)
findstr /C:"const char* xl_fn_pick__string(const char* value);" build\test\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing string overload symbol.
    exit /b 1
)
findstr /C:"chosen = xl_fn_pick__int(41LL);" build\test\cgen_overloads.c >nul
if errorlevel 1 (
    echo cgen_overloads.c missing resolved overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_overloads.exe
if errorlevel 1 exit /b 1
build\test\cgen_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_overload_costs.xl --emit build\test\cgen_overload_costs.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_overload_costs.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_overload_costs.exe
if errorlevel 1 exit /b 1
build\test\cgen_overload_costs.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_priority.xl --emit build\test\cgen_generic_overload_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_fn_choose(1LL), 10LL);" build\test\cgen_generic_overload_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_priority.c should prefer exact non-generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_overload_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_overload_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_overload_var_priority.xl --emit build\test\cgen_generic_overload_var_priority.c
if errorlevel 1 exit /b 1
findstr /C:"expect_i64(xl_gen_choose__int(1LL), 20LL);" build\test\cgen_generic_overload_var_priority.c >nul
if errorlevel 1 (
    echo cgen_generic_overload_var_priority.c should prefer generic overload over var boxing.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_overload_var_priority.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_overload_var_priority.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_explicit_overload.xl --emit build\test\cgen_generic_explicit_overload.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_pick__int__sig__int(long long value);" build\test\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing scalar explicit generic overload instance.
    exit /b 1
)
findstr /C:"long long xl_gen_pick__int__sig__array_of_int(xtarray values);" build\test\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c missing array explicit generic overload instance.
    exit /b 1
)
findstr /C:"= xl_gen_pick__int__sig__array_of_int(__xl6_call_arg_" build\test\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose array explicit generic overload.
    exit /b 1
)
findstr /C:"scalarValue = xl_gen_pick__int__sig__int(1LL);" build\test\cgen_generic_explicit_overload.c >nul
if errorlevel 1 (
    echo cgen_generic_explicit_overload.c should choose scalar explicit generic overload.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_explicit_overload.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_explicit_overload.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_signature_suffix.xl --emit build\test\cgen_generic_signature_suffix.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content -Raw -Encoding UTF8 'build\test\cgen_generic_signature_suffix.c'; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\);', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__int__string\(long long left, const char\* right\)\s*\{', 'Multiline')).Count -ne 1){exit 3}; if(([regex]::Matches($t, '^long long xl_gen_order__int__string__sig__string__int\(const char\* left, long long right\)\s*\{', 'Multiline')).Count -ne 1){exit 4}; if($t -notmatch 'first = xl_gen_order__int__string__sig__int__string\(1LL, \"x\"\);'){exit 5}; if($t -notmatch 'second = xl_gen_order__int__string__sig__string__int\(\"x\", 1LL\);'){exit 6}"
if errorlevel 1 (
    echo cgen_generic_signature_suffix.c missing distinct generic signature suffix instances.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_signature_suffix.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_signature_suffix.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_function.xl --emit build\test\cgen_generic_function.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" build\test\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic instance prototype.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" build\test\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic instance prototype.
    exit /b 1
)
findstr /C:"number = xl_gen_identity__int(41LL);" build\test\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing int generic call.
    exit /b 1
)
findstr /C:"text = xrtCopyStr(xl_gen_identity__string(\"ok\"), 0u);" build\test\cgen_generic_function.c >nul
if errorlevel 1 (
    echo cgen_generic_function.c missing string generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_function.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_function.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_function.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_dedupe.xl --emit build\test\cgen_generic_dedupe.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_generic_dedupe.c' -Raw; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\);', 'Multiline')).Count -ne 1){exit 1}; if(([regex]::Matches($t, '^long long xl_gen_identity__int\(long long value\)\s*\{', 'Multiline')).Count -ne 1){exit 2}; if(([regex]::Matches($t, 'xl_gen_identity__int\(')).Count -ne 5){exit 3}"
if errorlevel 1 (
    echo cgen_generic_dedupe.c duplicated or lost generic function instance.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_dedupe.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_dedupe.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_infer.xl --emit build\test\cgen_generic_infer.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int identity instance.
    exit /b 1
)
findstr /C:"const char* xl_gen_identity__string(const char* value);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string identity instance.
    exit /b 1
)
findstr /C:"long long xl_gen_choose__int(long long left, long long right);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countArray__int(xtarray values);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred array generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptList__string(xtlist values);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred list generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_acceptSet__int(xtset values);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred set generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countDict__int(xtdict values);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred dict generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_countNested__int(xtarray values);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred nested generic instance.
    exit /b 1
)
findstr /C:"xtarray xl_gen_identity__array_of_dict_of_int(xtarray value);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic instance.
    exit /b 1
)
findstr /C:"long long xl_gen_apply__int(XL6_Function op, long long value);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred function-param generic instance.
    exit /b 1
)
findstr /C:"number = xl_gen_identity__int(41LL);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred int call.
    exit /b 1
)
findstr /C:"text = xrtCopyStr(xl_gen_identity__string(\"ok\"), 0u);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred string call.
    exit /b 1
)
findstr /C:"chosen = xl_gen_choose__int(1LL, 2LL);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred choose call.
    exit /b 1
)
findstr /C:"copied = xl_gen_identity__int(seed);" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing inferred call from auto variable.
    exit /b 1
)
findstr /C:"explicitNested = __xl6_global_init_" build\test\cgen_generic_infer.c >nul
if errorlevel 1 (
    echo cgen_generic_infer.c missing explicit nested generic call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_infer.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_infer.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_infer.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_alias.xl --emit build\test\cgen_generic_alias.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_gen_identity__int(long long value);" build\test\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical int generic instance.
    exit /b 1
)
findstr /C:"xl_gen_identity__MyInt" build\test\cgen_generic_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_alias.c should not mangle transparent alias into generic symbol.
    exit /b 1
)
findstr /C:"explicitAlias = xl_gen_identity__int(41LL);" build\test\cgen_generic_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_alias.c missing canonical explicit alias call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_alias.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_alias.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_alias.xl --emit build\test\cgen_generic_class_alias.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" build\test\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing canonical generic class layout.
    exit /b 1
)
findstr /C:"XL_Class_Box__MyInt" build\test\cgen_generic_class_alias.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_alias.c should not mangle transparent alias into class symbol.
    exit /b 1
)
findstr /C:"xvalue aliasBox;" build\test\cgen_generic_class_alias.c >nul
if errorlevel 1 (
    echo cgen_generic_class_alias.c missing managed canonical alias variable.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_alias.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_alias.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 4/8
exit /b 0
