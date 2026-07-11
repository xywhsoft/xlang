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
findstr /C:"signed char i8" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int8 C type.
    exit /b 1
)
findstr /C:"unsigned char u8" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint8 C type.
    exit /b 1
)
findstr /C:"short i16" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int16 C type.
    exit /b 1
)
findstr /C:"unsigned short u16" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint16 C type.
    exit /b 1
)
findstr /C:"int i32" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int32 C type.
    exit /b 1
)
findstr /C:"unsigned int u32" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint32 C type.
    exit /b 1
)
findstr /C:"long long i64" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing int64 C type.
    exit /b 1
)
findstr /C:"unsigned long long u64" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing uint64 C type.
    exit /b 1
)
findstr /C:"float f32" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float32 C type.
    exit /b 1
)
findstr /C:"double f64" build\test\cgen_primitive_widths.c >nul
if errorlevel 1 (
    echo cgen_primitive_widths.c missing float64 C type.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_primitive_widths.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1
build\test\cgen_primitive_widths.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_safe_widening.xl --emit build\test\cgen_safe_widening.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_safe_widening.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_safe_widening.exe
if errorlevel 1 exit /b 1
build\test\cgen_safe_widening.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_containers.xl --emit build\test\cgen_containers.c
if errorlevel 1 exit /b 1
findstr /C:"emptyArray = xvoCreateArray();" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing array default init.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_global_init_" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing statement-level array literal append.
    exit /b 1
)
findstr /C:"xl6_xrt_list_literal(2" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing list literal lowering.
    exit /b 1
)
findstr /C:"xrtTypedSetCreate(xrtTypeInt()" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing inferred typed set literal lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictCreate(xrtTypeInt()" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing inferred typed dict literal lowering.
    exit /b 1
)
findstr /C:"xrtTypedDictSetValue(__xl6_global_init_" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing inferred typed dict item lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_set_literal(0" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty set lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_dict_varargs(0" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing explicit empty dict lowering.
    exit /b 1
)
findstr /C:"xvoArrayAppendValue(__xl6_global_init_" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing prefixed array statement-level append.
    exit /b 1
)
findstr /C:"prefixedDict = xl6_xrt_move_to_shared(xl6_xrt_dict_varargs(1" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing shared prefixed dict publication.
    exit /b 1
)
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed array lowering.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed dict iteration.
    exit /b 1
)
findstr /C:"xrtTypedArrayDestroy(typedArray)" build\test\cgen_containers.c >nul
if errorlevel 1 (
    echo cgen_containers.c missing typed container cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_containers.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_container_ops.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_container_ops.xl --emit build\test\cgen_container_ops.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayConcat(" build\test\cgen_container_ops.c >nul
if errorlevel 1 (
    echo cgen_container_ops.c missing typed array concat lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_list_concat(" build\test\cgen_container_ops.c >nul
if errorlevel 1 (
    echo cgen_container_ops.c missing typed list concat lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_dict_concat(" build\test\cgen_container_ops.c >nul
if errorlevel 1 (
    echo cgen_container_ops.c missing typed dict concat lowering.
    exit /b 1
)
findstr /C:"xrtTypedSetUnion(" build\test\cgen_container_ops.c >nul
if errorlevel 1 (
    echo cgen_container_ops.c missing typed set union lowering.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_set_relation(" build\test\cgen_container_ops.c >nul
if errorlevel 1 (
    echo cgen_container_ops.c missing typed set relation lowering.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_static_typed_containers.xl --emit build\test\cgen_static_typed_containers.c
if errorlevel 1 exit /b 1
findstr /C:"xrtTypedArrayCreate(xrtTypeInt()" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed array create.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(values" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed array index.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xrtTypedListGet(names" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed list index.
    exit /b 1
)
findstr /C:"xl6_xrt_typed_require_item(xrtTypedDictGet(table" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing guarded typed dict index.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(flags" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed array read width normalization.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xrtTypedListGet(checks" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed list read width normalization.
    exit /b 1
)
findstr /C:"((int)(*(bool*)xl6_xrt_typed_require_item(xrtTypedDictGet(states" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed dict read width normalization.
    exit /b 1
)
findstr /C:"xrtTypedListItemAt(" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed list iteration.
    exit /b 1
)
findstr /C:"xrtTypedSetItemAt(" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed set iteration.
    exit /b 1
)
findstr /C:"xrtTypedDictItemAt(" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict iteration.
    exit /b 1
)
findstr /C:"int flag = ((int)(*(bool*)__xl6_typed_item_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed array for-in normalization.
    exit /b 1
)
findstr /C:"int check = ((int)(*(bool*)__xl6_typed_item_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed list for-in normalization.
    exit /b 1
)
findstr /C:"int item = ((int)(*(bool*)__xl6_typed_item_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed set for-in normalization.
    exit /b 1
)
findstr /C:"int state = ((int)(*(bool*)__xl6_typed_item_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing language bool typed dict for-in normalization.
    exit /b 1
)
findstr /C:"const char* key = xrtCopyStr(__xl6_typed_key_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key value copy.
    exit /b 1
)
findstr /C:"__xl6_typed_key_len_" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key length copy.
    exit /b 1
)
findstr /C:"xrtFree((void*)key);" build\test\cgen_static_typed_containers.c >nul
if errorlevel 1 (
    echo cgen_static_typed_containers.c missing typed dict key cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_static_typed_containers.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1
build\test\cgen_static_typed_containers.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: core C generation and runtime baseline 2/10
exit /b 0

