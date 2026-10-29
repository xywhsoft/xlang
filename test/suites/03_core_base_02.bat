@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: core C generation and runtime baseline 2/10...

release\xl.exe check test\cases\sem_union_anonymous_conflict.xl >nul 2> build\test\sem_union_anonymous_conflict.err.txt
if not errorlevel 1 (
    echo sem_union_anonymous_conflict.xl should fail.
    exit /b 1
)
findstr /C:"class member layout name conflicts with an existing field or union" build\test\sem_union_anonymous_conflict.err.txt >nul
if errorlevel 1 (
    echo sem_union_anonymous_conflict.err.txt missing inline union conflict diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_named_inline.xl >nul 2> build\test\sem_union_named_inline.err.txt
if not errorlevel 1 (
    echo sem_union_named_inline.xl should fail.
    exit /b 1
)
findstr /C:"class inline union must be anonymous in this phase" build\test\sem_union_named_inline.err.txt >nul
if errorlevel 1 (
    echo sem_union_named_inline.err.txt missing named inline union diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_class_unknown_field.xl >nul 2> build\test\sem_class_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_class_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"unknown class/union field" build\test\sem_class_unknown_field.err.txt >nul
if errorlevel 1 (
    echo sem_class_unknown_field.err.txt missing unknown field diagnostic.
    exit /b 1
)
findstr /C:"member assignment receiver must be class/union field, var or dynamic dict" build\test\sem_class_unknown_field.err.txt >nul
if not errorlevel 1 (
    echo sem_class_unknown_field.err.txt should not report assignment target cascade.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_class_unknown_field.err.txt >nul
if not errorlevel 1 (
    echo sem_class_unknown_field.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_unknown_field.xl >nul 2> build\test\sem_union_unknown_field.err.txt
if not errorlevel 1 (
    echo sem_union_unknown_field.xl should fail.
    exit /b 1
)
findstr /C:"unknown class/union field" build\test\sem_union_unknown_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_unknown_field.err.txt missing unknown union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_function_field.xl >nul 2> build\test\sem_union_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" build\test\sem_union_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_function_field.err.txt missing union function field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_destroy_class_field.xl >nul 2> build\test\sem_union_destroy_class_field.err.txt
if not errorlevel 1 (
    echo sem_union_destroy_class_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" build\test\sem_union_destroy_class_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_destroy_class_field.err.txt missing union non-plain class field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_union_generic_inline_function_field.xl >nul 2> build\test\sem_union_generic_inline_function_field.err.txt
if not errorlevel 1 (
    echo sem_union_generic_inline_function_field.xl should fail.
    exit /b 1
)
findstr /C:"union field type must be a plain C-compatible value type" build\test\sem_union_generic_inline_function_field.err.txt >nul
if errorlevel 1 (
    echo sem_union_generic_inline_function_field.err.txt missing generic inline union field diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_assign_type_mismatch.xl >nul 2> build\test\sem_assign_type_mismatch.err.txt
if not errorlevel 1 (
    echo sem_assign_type_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_assign_type_mismatch.err.txt >nul
if errorlevel 1 (
    echo sem_assign_type_mismatch.xl missing variable assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_narrowing_int.xl >nul 2> build\test\sem_numeric_narrowing_int.err.txt
if not errorlevel 1 (
    echo sem_numeric_narrowing_int.xl should fail.
    exit /b 1
)
findstr /C:"target int8, value int64" build\test\sem_numeric_narrowing_int.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_narrowing_int.xl missing numeric narrowing type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_signed_to_unsigned.xl >nul 2> build\test\sem_numeric_signed_to_unsigned.err.txt
if not errorlevel 1 (
    echo sem_numeric_signed_to_unsigned.xl should fail.
    exit /b 1
)
findstr /C:"target uint16, value int8" build\test\sem_numeric_signed_to_unsigned.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_signed_to_unsigned.xl missing signed to unsigned type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_int_to_float_implicit.xl >nul 2> build\test\sem_numeric_int_to_float_implicit.err.txt
if not errorlevel 1 (
    echo sem_numeric_int_to_float_implicit.xl should fail.
    exit /b 1
)
findstr /C:"target float, value int" build\test\sem_numeric_int_to_float_implicit.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_int_to_float_implicit.xl missing int to float type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_numeric_float_narrowing.xl >nul 2> build\test\sem_numeric_float_narrowing.err.txt
if not errorlevel 1 (
    echo sem_numeric_float_narrowing.xl should fail.
    exit /b 1
)
findstr /C:"target float32, value float64" build\test\sem_numeric_float_narrowing.err.txt >nul
if errorlevel 1 (
    echo sem_numeric_float_narrowing.xl missing float narrowing type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_member_assign_type.xl >nul 2> build\test\sem_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" build\test\sem_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_assign_type.xl missing member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_member_assign_type.xl missing member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_nested_member_assign_type.xl >nul 2> build\test\sem_nested_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_nested_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" build\test\sem_nested_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_nested_member_assign_type.xl missing nested member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_nested_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_nested_member_assign_type.xl missing nested member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_multi_member_assign_type.xl >nul 2> build\test\sem_multi_member_assign_type.err.txt
if not errorlevel 1 (
    echo sem_multi_member_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"multi-return assignment item 1 type mismatch" build\test\sem_multi_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_multi_member_assign_type.xl missing multi-return member assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, return string" build\test\sem_multi_member_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_multi_member_assign_type.xl missing multi-return member assignment type context.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_array_assign_type.xl >nul 2> build\test\sem_typed_array_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_array_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" build\test\sem_typed_array_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt missing typed array assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_typed_array_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt missing typed array assignment type context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_typed_array_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_array_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_type.xl >nul 2> build\test\sem_typed_dict_assign_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.xl should fail.
    exit /b 1
)
findstr /C:"assignment expression type does not match member/index target type" build\test\sem_typed_dict_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt missing typed dict assignment mismatch diagnostic.
    exit /b 1
)
findstr /C:"target int, value string" build\test\sem_typed_dict_assign_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt missing typed dict assignment type context.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_typed_dict_assign_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_dict_assign_key_type.xl >nul 2> build\test\sem_typed_dict_assign_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.xl should fail.
    exit /b 1
)
findstr /C:"code generation error" build\test\sem_typed_dict_assign_key_type.err.txt >nul
if not errorlevel 1 (
    echo sem_typed_dict_assign_key_type.err.txt regressed to generic cgen diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_safe_dict_key_type.xl >nul 2> build\test\sem_typed_safe_dict_key_type.err.txt
if not errorlevel 1 (
    echo sem_typed_safe_dict_key_type.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\sem_typed_string_index_compound_type.xl >nul 2> build\test\sem_typed_string_index_compound_type.err.txt
if not errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl should fail.
    exit /b 1
)
findstr /C:"string compound assignment operands must both be string" build\test\sem_typed_string_index_compound_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl missing expected diagnostic.
    exit /b 1
)
findstr /C:"op +=, left string, right int" build\test\sem_typed_string_index_compound_type.err.txt >nul
if errorlevel 1 (
    echo sem_typed_string_index_compound_type.xl missing compound diagnostic context.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_primitive_widths.xl --emit build\test\cgen_primitive_widths.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_container_contract.ps1 -Name cgen_primitive_widths
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_primitive_widths.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1
build\test\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_widening.xl --emit build\test\cgen_safe_widening.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_safe_widening.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_safe_widening.exe
if errorlevel 1 exit /b 1
build\test\cgen_safe_widening.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_mixed_numeric_binary_infer.xl --emit build\test\cgen_mixed_numeric_binary_infer.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_mixed_numeric_binary_infer.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_mixed_numeric_binary_infer.exe
if errorlevel 1 exit /b 1
build\test\cgen_mixed_numeric_binary_infer.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\cgen_mixed_numeric_binary_infer.xl --lowered --emit build\test\xlir_mixed_numeric_binary.xlir
if errorlevel 1 exit /b 1
findstr /C:"scalar-convert" build\test\xlir_mixed_numeric_binary.xlir >nul
if errorlevel 1 (
    echo xlir_mixed_numeric_binary.xlir missing explicit i64-to-f64 execution promotion.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_mixed_numeric_binary_infer.xl --backend xlir --emit build\test\xlir_mixed_numeric_binary_generated.c
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror test\cases\xlir_mixed_numeric_binary_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\xlir_mixed_numeric_binary_host.exe
if errorlevel 1 exit /b 1
build\test\xlir_mixed_numeric_binary_host.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_var_numeric_convert.xl > build\test\cgen_var_numeric_convert.out.txt
if errorlevel 1 exit /b 1
findstr /C:"var numeric conversion ok" build\test\cgen_var_numeric_convert.out.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_containers.xl --emit build\test\cgen_containers.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_container_contract.ps1 -Name cgen_containers
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_containers.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_container_ops.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_typed_shared_copy_probe.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_container_ops.xl --emit build\test\cgen_container_ops.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_container_contract.ps1 -Name cgen_container_ops
if errorlevel 1 exit /b 1

powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_dynamic_integer_index_write.ps1
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cgen_dynamic_container_reference.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_dynamic_container_reference.xl --emit build\test\cgen_dynamic_container_reference.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_container_contract.ps1 -Name cgen_dynamic_container_reference
if errorlevel 1 exit /b 1
findstr /C:"return xrtValueRetain(value);" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h missing managed-reference retain path.
    exit /b 1
)
findstr /C:"bool (*Clone)(ptr pTarget, const void* pSource, const xrttype* pType);" release\include\xruntime_decl.h >nul
if errorlevel 1 (
    echo xruntime_decl.h is missing the runtime clone type operation.
    exit /b 1
)
findstr /C:"static inline xvalue* xl6ValueRecordOwnerRef(const ptr pData);" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h is missing the managed record owner lookup.
    exit /b 1
)
findstr /C:"XRT_API int32 xrtRefRetain(volatile int32* pCount);" release\include\xruntime_decl.h >nul
if errorlevel 1 (
    echo xruntime_decl.h is missing the reference retain API.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_static_typed_containers.xl --emit build\test\cgen_static_typed_containers.c
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\check_xlir_scalar_container_contract.ps1 -Name cgen_static_typed_containers
if errorlevel 1 exit /b 1
"%XL6_CGEN_CC%" %XL6_CGEN_FLAGS% %XL6_CGEN_INCLUDES% -Werror -DXLIR_GENERATED_MODULE=\"build/test/cgen_static_typed_containers.c\" test\cases\xlir_generated_module_host.c test\cases\cgen_class_host.c %XL6_CGEN_LINKS% -o build\test\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 2/10
exit /b 0
