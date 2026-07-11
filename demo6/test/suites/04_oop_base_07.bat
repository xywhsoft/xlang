@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 7/8...

release\xl.exe cgen test\cases\export_c_abi_types.xl --emit build\test\export_c_abi_types.c --emit-header build\test\export_c_abi_types.h
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_echo_c_size(size_t value);" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_size export declaration.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_echo_c_ssize(xl6_ssize_t value);" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ssize export declaration.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_echo_c_ptrdiff(ptrdiff_t value);" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_ptrdiff export declaration.
    exit /b 1
)
findstr /C:"wchar_t xl6_echo_c_wchar(wchar_t value);" build\test\export_c_abi_types.h >nul
if errorlevel 1 (
    echo export_c_abi_types.h missing c_wchar export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_c_abi_types.c test\cases\export_c_abi_types_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_c_abi_types_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe abi test\cases\export_alias.xl --emit build\test\export_alias.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.aliasAdd(int,int) as int" build\test\export_alias.abi.txt >nul
if errorlevel 1 (
    echo export_alias.abi.txt missing canonical alias signature.
    exit /b 1
)
release\xl.exe abi test\cases\export_alias.xl --json --emit build\test\export_alias.abi.json
if errorlevel 1 exit /b 1
findstr /C:"abiVersion" build\test\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing ABI version.
    exit /b 1
)
findstr /C:"fn:app.aliasAdd(int,int) as int" build\test\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing canonical alias signature.
    exit /b 1
)
findstr /C:"xl6_alias_add" build\test\export_alias.abi.json >nul
if errorlevel 1 (
    echo export_alias.abi.json missing C export symbol.
    exit /b 1
)
release\xl.exe abi test\cases\extern_puts.xl --json --emit build\test\extern_puts.abi.json
if errorlevel 1 exit /b 1
findstr /C:"cSymbol" build\test\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing extern C symbol.
    exit /b 1
)
findstr /C:"puts" build\test\extern_puts.abi.json >nul
if errorlevel 1 (
    echo extern_puts.abi.json missing puts symbol.
    exit /b 1
)
release\xl.exe cgen test\cases\export_alias.xl --emit build\test\export_alias.c --emit-header build\test\export_alias.h
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_aliasAdd(long long a, long long b);" build\test\export_alias.c >nul
if errorlevel 1 (
    echo export_alias.c missing expanded alias prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_alias_add(int64_t a, int64_t b);" build\test\export_alias.h >nul
if errorlevel 1 (
    echo export_alias.h missing expanded alias header declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_alias.c test\cases\export_alias_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_alias_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_alias_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe check release\examples\c_variables.xl
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_vars.xl --emit build\test\export_vars.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_counter\" int counter = 1" build\test\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export var declaration.
    exit /b 1
)
findstr /C:"export \"xl6_version\" const int version = 7" build\test\export_vars.xlir >nul
if errorlevel 1 (
    echo export_vars.xlir missing export const declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_vars.xl --emit build\test\export_vars.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_counter" build\test\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export var symbol.
    exit /b 1
)
findstr /C:"kind: const" build\test\export_vars.abi.txt >nul
if errorlevel 1 (
    echo export_vars.abi.txt missing export const kind.
    exit /b 1
)

release\xl.exe cgen test\cases\export_vars.xl --emit build\test\export_vars.c --emit-header build\test\export_vars.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_counter = 1LL;" build\test\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var definition.
    exit /b 1
)
findstr /C:"const int64_t xl6_version = 7LL;" build\test\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export const definition.
    exit /b 1
)
findstr /C:"= (xl6_counter + xl6_version);" build\test\export_vars.c >nul
if errorlevel 1 (
    echo export_vars.c missing export var read inside wrapper target.
    exit /b 1
)
findstr /C:"extern int64_t xl6_counter;" build\test\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export var declaration.
    exit /b 1
)
findstr /C:"extern const int64_t xl6_version;" build\test\export_vars.h >nul
if errorlevel 1 (
    echo export_vars.h missing export const declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_vars.c test\cases\export_vars_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_vars_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_vars_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\export_package_vars.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\export_package_vars.xl --emit build\test\export_package_vars.c --emit-header build\test\export_package_vars.h
if errorlevel 1 exit /b 1
findstr /C:"packageCounter" build\test\export_package_vars.h >nul
if not errorlevel 1 (
    echo export_package_vars.h should not expose package-only export variable.
    exit /b 1
)
findstr /C:"xl6_xrt_external_string_copy(native_shared_text(" build\test\cgen_extern_string_return.c >nul
if errorlevel 1 (
    echo cgen_extern_string_return.c missing stable extern string copy.
    exit /b 1
)
findstr /C:"xl6_xrt_external_string_copy(native_text_shared(" build\test\cgen_extern_string_return.c >nul
if errorlevel 1 (
    echo cgen_extern_string_return.c missing stable native member string copy.
    exit /b 1
)

release\xl.exe cgen test\cases\export_call.xl --emit build\test\export_call.c
if errorlevel 1 exit /b 1
findstr /C:"value = xl6_export_add__i64(1LL, 2LL);" build\test\export_call.c >nul
if errorlevel 1 (
    echo export_call.c did not use internal ABI symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_call.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_call.exe
if errorlevel 1 exit /b 1
build\test\export_call.exe
if errorlevel 1 exit /b 1

release\xl.exe abi release\examples\c_export.xl --emit build\test\example_c_export.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export app_add_i64" build\test\example_c_export.abi.txt >nul
if errorlevel 1 (
    echo release\examples\c_export.xl ABI dump missing export.
    exit /b 1
)

release\xl.exe cgen release\examples\class_c_export.xl --emit build\test\example_class_c_export.c --emit-header build\test\example_class_c_export.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t gauge_add(XL_Class_Gauge* self, int64_t delta);" build\test\example_class_c_export.h >nul
if errorlevel 1 (
    echo release\examples\class_c_export.xl header missing class method export.
    exit /b 1
)
release\xl.exe abi release\examples\class_c_export.xl --emit build\test\example_class_c_export.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"owner: Gauge" build\test\example_class_c_export.abi.txt >nul
if errorlevel 1 (
    echo release\examples\class_c_export.xl ABI dump missing class method owner.
    exit /b 1
)

if exist cgen_basic.exe del /f /q cgen_basic.exe
release\xl.exe build test\cases\cgen_basic.xl >nul
if errorlevel 1 (
    echo build without -o should use default output path.
    exit /b 1
)
if not exist cgen_basic.exe (
    echo build without -o did not create default output executable.
    exit /b 1
)
cgen_basic.exe
if errorlevel 1 exit /b 1
del /f /q cgen_basic.exe

release\xl.exe check test\cases\sem_duplicate.xl >nul 2> build\test\sem_duplicate.err.txt
if not errorlevel 1 (
    echo sem_duplicate.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_global_func_value_conflict.xl >nul 2> build\test\sem_global_func_value_conflict.err.txt
if not errorlevel 1 (
    echo sem_global_func_value_conflict.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_const_assign.xl >nul 2> build\test\sem_const_assign.err.txt
if not errorlevel 1 (
    echo sem_const_assign.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_safe_member_assign.xl >nul 2> build\test\sem_safe_member_assign.err.txt
if not errorlevel 1 (
    echo sem_safe_member_assign.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_unknown.xl >nul 2> build\test\sem_safe_member_unknown.err.txt
if not errorlevel 1 (
    echo sem_safe_member_unknown.xl should fail.
    exit /b 1
)
findstr /C:"safe member key receiver must be var or dynamic dict" build\test\sem_safe_member_unknown.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_unknown.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_safe_member_unknown.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_unknown.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_typed_container.xl >nul
if errorlevel 1 (
    echo sem_safe_member_typed_container.xl should pass.
    exit /b 1
)
release\xl.exe check test\cases\sem_safe_member_non_container.xl >nul 2> build\test\sem_safe_member_non_container.err.txt
if not errorlevel 1 (
    echo sem_safe_member_non_container.xl should fail.
    exit /b 1
)
findstr /C:"safe member length receiver must be a container" build\test\sem_safe_member_non_container.err.txt >nul
if errorlevel 1 (
    echo sem_safe_member_non_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_safe_member_non_container.err.txt >nul
if not errorlevel 1 (
    echo sem_safe_member_non_container.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_array.xl >nul 2> build\test\sem_member_key_array.err.txt
if not errorlevel 1 (
    echo sem_member_key_array.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" build\test\sem_member_key_array.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_array.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_member_key_array.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_array.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_set.xl >nul 2> build\test\sem_member_key_set.err.txt
if not errorlevel 1 (
    echo sem_member_key_set.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" build\test\sem_member_key_set.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_set.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_member_key_set.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_set.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_member_key_typed_dict.xl >nul 2> build\test\sem_member_key_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_member_key_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" build\test\sem_member_key_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_member_key_typed_dict.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_member_key_typed_dict.err.txt >nul
if not errorlevel 1 (
    echo sem_member_key_typed_dict.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_length.xl >nul 2> build\test\sem_dynamic_member_assign_length.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl should fail.
    exit /b 1
)
findstr /C:"member length is read-only" build\test\sem_dynamic_member_assign_length.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_length.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_container.xl >nul 2> build\test\sem_dynamic_member_assign_container.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" build\test\sem_dynamic_member_assign_container.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" build\test\sem_dynamic_member_assign_container.err.txt >nul
if not errorlevel 1 (
    echo sem_dynamic_member_assign_container.xl should not report assignment target cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_typed_dict.xl >nul 2> build\test\sem_dynamic_member_assign_typed_dict.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl should fail.
    exit /b 1
)
findstr /C:"member key receiver must be var or dynamic dict" build\test\sem_dynamic_member_assign_typed_dict.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" build\test\sem_dynamic_member_assign_typed_dict.err.txt >nul
if not errorlevel 1 (
    echo sem_dynamic_member_assign_typed_dict.xl should not report assignment target cascade.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_compound.xl >nul 2> build\test\sem_dynamic_member_assign_compound.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment only supports =" build\test\sem_dynamic_member_assign_compound.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_compound.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_dynamic_member_assign_temporary.xl >nul 2> build\test\sem_dynamic_member_assign_temporary.err.txt
if not errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl should fail.
    exit /b 1
)
findstr /C:"dynamic member assignment receiver must be mutable" build\test\sem_dynamic_member_assign_temporary.err.txt >nul
if errorlevel 1 (
    echo sem_dynamic_member_assign_temporary.xl missing expected diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_array_key_type.xl >nul 2> build\test\sem_index_array_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_array_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_index_dict_key_type.xl >nul 2> build\test\sem_index_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_index_dict_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_index_dict_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_index_dict_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_var_container_index_key_type.xl >nul 2> build\test\sem_var_container_index_key_type.err.txt
if not errorlevel 1 (
    echo sem_var_container_index_key_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_outside_loop.xl >nul 2> build\test\sem_break_outside_loop.err.txt
if not errorlevel 1 (
    echo sem_break_outside_loop.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_continue_zero.xl >nul 2> build\test\sem_continue_zero.err.txt
if not errorlevel 1 (
    echo sem_continue_zero.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_break_too_deep.xl >nul 2> build\test\sem_break_too_deep.err.txt
if not errorlevel 1 (
    echo sem_break_too_deep.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_if_condition_type.xl >nul 2> build\test\sem_if_condition_type.err.txt
if not errorlevel 1 (
    echo sem_if_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_while_condition_type.xl >nul 2> build\test\sem_while_condition_type.err.txt
if not errorlevel 1 (
    echo sem_while_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_conditional_condition_type.xl >nul 2> build\test\sem_conditional_condition_type.err.txt
if not errorlevel 1 (
    echo sem_conditional_condition_type.xl should fail.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_implicit_upcast.xl >nul 2>build\test\sem_class_implicit_upcast.err.txt
if not errorlevel 1 (
    echo sem_class_implicit_upcast.xl should fail.
    exit /b 1
)
powershell -NoProfile -Command "$count=(Select-String -Path 'build\test\sem_class_implicit_upcast.err.txt' -SimpleMatch 'target UpcastBase, value UpcastDerived').Count; if ($count -ne 4) { exit 1 }"
if errorlevel 1 (
    echo sem_class_implicit_upcast.err.txt should contain four slicing diagnostics.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_class_virtual_return_move.xl --emit build\test\cgen_class_virtual_return_move.c
if errorlevel 1 exit /b 1
findstr /C:"return __xl6_return_" build\test\cgen_class_virtual_return_move.c >nul
if errorlevel 1 (
    echo cgen_class_virtual_return_move.c missing materialized class return.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_virtual_return_move.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_virtual_return_move.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_virtual_return_move.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\sem_class_equality_without_protocol.xl >nul 2> build\test\sem_class_equality_without_protocol.err.txt
if not errorlevel 1 (
    echo sem_class_equality_without_protocol.xl should fail.
    exit /b 1
)
findstr /C:"class equality requires __eq__/__ne__ protocol" build\test\sem_class_equality_without_protocol.err.txt >nul
if errorlevel 1 (
    echo sem_class_equality_without_protocol.err.txt missing class equality diagnostic.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_class_equality_without_protocol.err.txt >nul
if not errorlevel 1 (
    echo sem_class_equality_without_protocol.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)


echo DEMO6 suite ok: OOP ABI export and interop baseline 7/8
exit /b 0

