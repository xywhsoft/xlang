@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 3/8...

release\xl.exe cgen test\cases\cgen_native_class_property.xl --emit build\test\cgen_native_class_property.c
if errorlevel 1 exit /b 1
findstr /C:"long long native_property_get_value(void* self);" build\test\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern get C prototype.
    exit /b 1
)
findstr /C:"void native_property_set_at(void* self, long long index, long long value);" build\test\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern let C prototype.
    exit /b 1
)
findstr /C:"return native_property_get_value(self->handle);" build\test\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern get wrapper.
    exit /b 1
)
findstr /C:"native_property_set_at(self->handle, index, value);" build\test\cgen_native_class_property.c >nul
if errorlevel 1 (
    echo cgen_native_class_property.c missing extern let wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_property.c test\cases\cgen_native_class_property_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_property.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_property.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_helper.xl --emit build\test\cgen_native_class_helper.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Counter__plus(XL_Class_Counter* self, long long extra);" build\test\cgen_native_class_helper.c >nul
if errorlevel 1 (
    echo cgen_native_class_helper.c missing native-backed class helper prototype.
    exit /b 1
)
findstr /C:"counter_value(self->handle)" build\test\cgen_native_class_helper.c >nul
if errorlevel 1 (
    echo cgen_native_class_helper.c missing native-backed class helper self handle call.
    exit /b 1
)
findstr /C:"xl_class_Counter__isZero(&counter)" build\test\cgen_native_class_helper.c >nul
if errorlevel 1 (
    echo cgen_native_class_helper.c missing native-backed class helper call.
    exit /b 1
)
findstr /C:"xl_class_Counter__plus(&counter, 7LL)" build\test\cgen_native_class_helper.c >nul
if errorlevel 1 (
    echo cgen_native_class_helper.c missing native-backed class helper call args.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_helper.c test\cases\cgen_native_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_helper.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_helper.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_bind_point_basic.xl --emit build\test\cgen_native_class_bind_point_basic.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = make_borrowed_box(55LL);" build\test\cgen_native_class_bind_point_basic.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point_basic.c missing point binding.
    exit /b 1
)
findstr /C:"borrowed_box_close(box.handle);" build\test\cgen_native_class_bind_point_basic.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point_basic.c missing direct borrowed destroy.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_bind_point_basic.c test\cases\cgen_native_class_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_bind_point_basic.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_bind_point_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_ownership.xl --emit build\test\cgen_native_class_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"box.handle = make_borrowed_box(66LL);" build\test\cgen_native_class_ownership.c >nul
if errorlevel 1 (
    echo cgen_native_class_ownership.c missing create-only native-backed class direct construction.
    exit /b 1
)
findstr /C:"box.handle = handle;" build\test\cgen_native_class_ownership.c >nul
if errorlevel 1 (
    echo cgen_native_class_ownership.c missing view-only point binding.
    exit /b 1
)
findstr /C:"xl6_point_take((void**)&(handle))" build\test\cgen_native_class_ownership.c >nul
if not errorlevel 1 (
    echo cgen_native_class_ownership.c must not consume a non-owning view handle.
    exit /b 1
)
findstr /C:"borrowed_box_close" build\test\cgen_native_class_ownership.c >nul
if not errorlevel 1 (
    echo cgen_native_class_ownership.c should not emit destroy without destroy mapping.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_ownership.c test\cases\cgen_native_class_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_ownership.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_ownership.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_null_destroy.xl --emit build\test\cgen_native_class_null_destroy.c
if errorlevel 1 exit /b 1
findstr /C:"if (self->handle != 0)" build\test\cgen_native_class_null_destroy.c >nul
if errorlevel 1 (
    echo cgen_native_class_null_destroy.c missing null-handle destroy guard.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_null_destroy.c test\cases\cgen_native_class_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_null_destroy.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_null_destroy.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class.xl --emit build\test\cgen_class.c
if errorlevel 1 exit /b 1
findstr /C:"struct XL_Class_Point {" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing class struct.
    exit /b 1
)
findstr /C:"XL_Class_Point base;" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing base field.
    exit /b 1
)
findstr /C:"p.x = 3LL;" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing field default.
    exit /b 1
)
findstr /C:"q.base.x = 10LL;" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing inherited field assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class.exe
if errorlevel 1 exit /b 1
build\test\cgen_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create.xl --emit build\test\cgen_base_create.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create(&self->base, seed);" build\test\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing explicit base create call.
    exit /b 1
)
findstr /C:"xl_class_AutoBase__create(&autoChild.base);" build\test\cgen_base_create.c >nul
if errorlevel 1 (
    echo cgen_base_create.c missing auto base create call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_base_create.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_base_create.exe
if errorlevel 1 exit /b 1
build\test\cgen_base_create.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_base_create_overloads.xl --emit build\test\cgen_base_create_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Base__create__void(&child.base);" build\test\cgen_base_create_overloads.c >nul
if errorlevel 1 (
    echo cgen_base_create_overloads.c missing auto no-arg base create overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_base_create_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1
build\test\cgen_base_create_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_methods.xl --emit build\test\cgen_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Point__create(XL_Class_Point* self, long long x, long long y);" build\test\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Point__length2(XL_Class_Point* self);" build\test\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method prototype.
    exit /b 1
)
findstr /C:"xl_class_Point__create(&p, 3LL, 4LL);" build\test\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create call.
    exit /b 1
)
findstr /C:"value = xl_class_Point__length2(&p);" build\test\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_methods.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_class_method_multi_return.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_class_method_multi_return.xl --emit build\test\cgen_class_method_multi_return.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL6_Function_Return_" build\test\cgen_class_method_multi_return.c >nul
if errorlevel 1 (
    echo cgen_class_method_multi_return.c missing class method multi-return struct.
    exit /b 1
)
findstr /C:"xl_class_Pairer__pair(&pairer, 3LL)" build\test\cgen_class_method_multi_return.c >nul
if errorlevel 1 (
    echo cgen_class_method_multi_return.c missing class method multi-return call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_method_multi_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_method_multi_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_method_multi_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_object_protocol.xl --emit build\test\cgen_object_protocol.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_object______setField____(&a.base" build\test\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing object field protocol setter call.
    exit /b 1
)
findstr /C:"xl_class_object______setItem____(&a.base" build\test\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing object item protocol setter call.
    exit /b 1
)
findstr /C:"xvoUnref(value);" build\test\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing callee-owned protocol value cleanup.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_protocol_value_" build\test\cgen_object_protocol.c >nul
if not errorlevel 1 (
    echo cgen_object_protocol.c releases a transferred protocol value twice.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_object_protocol.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_object_protocol.exe
if errorlevel 1 exit /b 1
build\test\cgen_object_protocol.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_ops.xl --emit build\test\cgen_protocol_ops.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Counter______add____(&c" build\test\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing operator protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______contains____(&c" build\test\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing contains protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______call____(&c" build\test\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing call protocol call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_ops.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_ops.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_ops.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_compound_assign.xl --emit build\test\cgen_protocol_compound_assign.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Meter______add______int(&value, 5LL)" build\test\cgen_protocol_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_protocol_compound_assign.c missing class += protocol call.
    exit /b 1
)
findstr /C:"XL_Class_Meter __xl6_class_arg_" build\test\cgen_protocol_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_protocol_compound_assign.c missing inline class argument temporary.
    exit /b 1
)
findstr /C:"xl_class_Meter______add______Meter(&value, __xl6_class_arg_" build\test\cgen_protocol_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_protocol_compound_assign.c missing inline class += overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_compound_assign.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_compound_assign.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_compound_assign.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_item_typed_array_return.xl --emit build\test\cgen_protocol_item_typed_array_return.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_item_typed_array_return.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_item_typed_array_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_item_typed_array_return.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_derive_string_hash_equals.xl --emit build\test\cgen_derive_string_hash_equals.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_DeriveRecord______toString____" build\test\cgen_derive_string_hash_equals.c >nul
if errorlevel 1 (
    echo cgen_derive_string_hash_equals.c missing derived string protocol.
    exit /b 1
)
findstr /C:"xl_class_DeriveRecord______hash____" build\test\cgen_derive_string_hash_equals.c >nul
if errorlevel 1 (
    echo cgen_derive_string_hash_equals.c missing derived hash protocol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_derive_string_hash_equals.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_derive_string_hash_equals.exe
if errorlevel 1 exit /b 1
build\test\cgen_derive_string_hash_equals.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_overload_index.xl --emit build\test\cgen_protocol_overload_index.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Grid______item______int__int" build\test\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing two-dimensional item overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______setItem______int__int__int" build\test\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing two-dimensional setItem overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______call______string" build\test\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string call overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______contains______string" build\test\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string contains overload symbol.
    exit /b 1
)
findstr /C:"xl_class_Grid______add______string" build\test\cgen_protocol_overload_index.c >nul
if errorlevel 1 (
    echo cgen_protocol_overload_index.c missing string add overload symbol.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_overload_index.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_overload_index.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_overload_index.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_property_get_let.xl --emit build\test\cgen_class_property_get_let.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Player______get______hp__void" build\test\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing zero-dimensional property getter symbol.
    exit /b 1
)
findstr /C:"xl_class_Player______let______hp__int__int__int" build\test\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing multi-dimensional property setter symbol.
    exit /b 1
)
findstr /C:"xl_class_Player______get______hp__int__int(&player, 2LL, 4LL)" build\test\cgen_class_property_get_let.c >nul
if errorlevel 1 (
    echo cgen_class_property_get_let.c missing multi-dimensional property getter call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_property_get_let.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_property_get_let.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_property_get_let.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_property_compound_managed.xl --emit build\test\cgen_class_property_compound_managed.c
if errorlevel 1 exit /b 1
findstr /C:"__xl6_property_current_" build\test\cgen_class_property_compound_managed.c >nul
if errorlevel 1 (
    echo cgen_class_property_compound_managed.c missing property read-modify-write temporary.
    exit /b 1
)
findstr /C:"xl_class_Score______add____(&__xl6_property_current_" build\test\cgen_class_property_compound_managed.c >nul
if errorlevel 1 (
    echo cgen_class_property_compound_managed.c missing class protocol compound call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_property_compound_managed.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_property_compound_managed.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_property_compound_managed.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_nested_class_constructor.xl --emit build\test\cgen_nested_class_constructor.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Ray__create(&direct, __xl6_class_arg_" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing nested global constructor temporaries.
    exit /b 1
)
findstr /C:".value1 = (XL_Class_Ray){0};" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing multi-return class construction.
    exit /b 1
)
findstr /C:"xl_class_Box__Vec3__create(&boxed, __xl6_class_arg_" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing generic class nested constructor lowering.
    exit /b 1
)
findstr /C:"XL_Class_Ray __xl6_async_return_" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing async class constructor lowering.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_nested_class_constructor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_nested_class_constructor.exe
if errorlevel 1 exit /b 1
build\test\cgen_nested_class_constructor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_stdlib_namespace_shadow.xl --emit build\test\cgen_stdlib_namespace_shadow.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Vec__nearZero(&dir)" build\test\cgen_stdlib_namespace_shadow.c >nul
if errorlevel 1 (
    echo cgen_stdlib_namespace_shadow.c did not preserve the local dir receiver.
    exit /b 1
)
findstr /C:"__xl_std_path_join" build\test\cgen_stdlib_namespace_shadow.c >nul
if errorlevel 1 (
    echo cgen_stdlib_namespace_shadow.c lost the unshadowed stdlib namespace call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_stdlib_namespace_shadow.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_stdlib_namespace_shadow.exe
if errorlevel 1 exit /b 1
build\test\cgen_stdlib_namespace_shadow.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_complex_receiver.xl --emit build\test\cgen_protocol_complex_receiver.c
if errorlevel 1 exit /b 1
findstr /C:"xl_protocol_tmp_Counter______add____" build\test\cgen_protocol_complex_receiver.c >nul
if errorlevel 1 (
    echo cgen_protocol_complex_receiver.c missing value receiver protocol wrapper.
    exit /b 1
)
findstr /C:"xl_protocol_tmp_Bag______item____" build\test\cgen_protocol_complex_receiver.c >nul
if errorlevel 1 (
    echo cgen_protocol_complex_receiver.c missing item protocol wrapper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_complex_receiver.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_complex_receiver.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_complex_receiver.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_protocol_len.xl --emit build\test\cgen_protocol_len.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Sized______len____(&value)" build\test\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing named receiver len protocol call.
    exit /b 1
)
findstr /C:"xl_protocol_tmp_Sized______len____" build\test\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing temporary receiver len protocol wrapper.
    exit /b 1
)
findstr /C:"withField.length = 4LL;" build\test\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing static length field assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_len.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_len.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_len.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\cgen_protocol_iter.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_protocol_iter.xl --emit build\test\cgen_protocol_iter.c
if errorlevel 1 exit /b 1
findstr /C:"xl_protocol_tmp_Counter______iter______void" build\test\cgen_protocol_iter.c >nul
if errorlevel 1 (
    echo cgen_protocol_iter.c missing temporary receiver iter protocol wrapper.
    exit /b 1
)
findstr /C:"xl_class_CounterIter______next____(&__xl6_class_iter_" build\test\cgen_protocol_iter.c >nul
if errorlevel 1 (
    echo cgen_protocol_iter.c missing iterator next protocol call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_protocol_iter.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_protocol_iter.exe
if errorlevel 1 exit /b 1
build\test\cgen_protocol_iter.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_copy_clone.xl --emit build\test\cgen_copy_clone.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Packet______copy____" build\test\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing derived copy protocol.
    exit /b 1
)
findstr /C:"xl_class_Packet______clone____" build\test\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing derived clone protocol.
    exit /b 1
)
findstr /C:"xvoDeepCopy(value.values)" build\test\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing deep xvalue field copy.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_copy_clone.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_copy_clone.exe
if errorlevel 1 exit /b 1
build\test\cgen_copy_clone.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_deepcopy_removed.xl --emit build\test\cgen_deepcopy_removed.c > build\test\cgen_deepcopy_removed.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_deepcopy_removed.xl should fail because deepcopy is not a builtin alias.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$bytes=[System.IO.File]::ReadAllBytes('build\test\cgen_deepcopy_removed.err.txt'); $enc=New-Object System.Text.UTF8Encoding($false,$true); $text=$enc.GetString($bytes); $expected=([char]0x6682)+([char]0x4E0D)+([char]0x652F)+([char]0x6301); if($text.IndexOf($expected) -lt 0){ Write-Host 'cgen_deepcopy_removed.err.txt missing unsupported diagnostic.'; exit 1 }"
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 3/8
exit /b 0
