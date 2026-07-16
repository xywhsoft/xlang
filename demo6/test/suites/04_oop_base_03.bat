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
findstr /C:"xl_class_Counter__isZero(((XL_Class_Counter*)xl6_class_data(counter" build\test\cgen_native_class_helper.c >nul
if errorlevel 1 (
    echo cgen_native_class_helper.c missing native-backed class helper call.
    exit /b 1
)
findstr /C:"xl_class_Counter__plus(((XL_Class_Counter*)xl6_class_data(counter" build\test\cgen_native_class_helper.c >nul
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
findstr /C:"box = xl6_box_XL_Class_BorrowedBox((XL_Class_BorrowedBox){make_borrowed_box(55LL)});" build\test\cgen_native_class_bind_point_basic.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point_basic.c missing managed point binding.
    exit /b 1
)
findstr /C:"if (box != 0)" build\test\cgen_native_class_bind_point_basic.c >nul
if errorlevel 1 (
    echo cgen_native_class_bind_point_basic.c missing managed native-backed cleanup.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_native_class_bind_point_basic.c test\cases\cgen_native_class_bind_point_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_native_class_bind_point_basic.exe
if errorlevel 1 exit /b 1
build\test\cgen_native_class_bind_point_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_native_class_ownership.xl --emit build\test\cgen_native_class_ownership.c
if errorlevel 1 exit /b 1
findstr /C:"box = xl6_box_XL_Class_OpenOnly((XL_Class_OpenOnly){make_borrowed_box(66LL)});" build\test\cgen_native_class_ownership.c >nul
if errorlevel 1 (
    echo cgen_native_class_ownership.c missing managed create-only native-backed class construction.
    exit /b 1
)
findstr /C:"box = xl6_box_XL_Class_ViewOnly((XL_Class_ViewOnly){handle});" build\test\cgen_native_class_ownership.c >nul
if errorlevel 1 (
    echo cgen_native_class_ownership.c missing managed view-only point binding.
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
findstr /C:"xvoCreateObject(xl6_type_desc_XL_Class_Point())" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing managed Point construction.
    exit /b 1
)
findstr /C:"xvoCreateObject(xl6_type_desc_XL_Class_Point3())" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing managed derived class construction.
    exit /b 1
)
findstr /C:")).base.x = 10LL;" build\test\cgen_class.c >nul
if errorlevel 1 (
    echo cgen_class.c missing managed inherited field assignment.
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
findstr /C:"xl_class_AutoBase__create(&(*__xl6_object_data_" build\test\cgen_base_create.c >nul
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
findstr /C:"xl_class_Base__create__void(&(*__xl6_object_data_" build\test\cgen_base_create_overloads.c >nul
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
findstr /C:"xl_class_Point__create(&(*__xl6_object_data_" build\test\cgen_class_methods.c >nul
if errorlevel 1 (
    echo cgen_class_methods.c missing create call.
    exit /b 1
)
findstr /C:"value = xl_class_Point__length2(((XL_Class_Point*)xl6_class_data(p" build\test\cgen_class_methods.c >nul
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
findstr /C:"xl_class_Pairer__pair(((XL_Class_Pairer*)xl6_class_data(pairer" build\test\cgen_class_method_multi_return.c >nul
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
findstr /C:"xl_class_object______setField____(&((XL_Class_Actor*)xl6_class_data(a" build\test\cgen_object_protocol.c >nul
if errorlevel 1 (
    echo cgen_object_protocol.c missing object field protocol setter call.
    exit /b 1
)
findstr /C:"xl_class_object______setItem____(&((XL_Class_Actor*)xl6_class_data(a" build\test\cgen_object_protocol.c >nul
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
findstr /C:"xl_class_Counter______add____(((XL_Class_Counter*)xl6_class_data(c" build\test\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing operator protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______contains____(((XL_Class_Counter*)xl6_class_data(c" build\test\cgen_protocol_ops.c >nul
if errorlevel 1 (
    echo cgen_protocol_ops.c missing contains protocol call.
    exit /b 1
)
findstr /C:"xl_class_Counter______call____(((XL_Class_Counter*)xl6_class_data(c" build\test\cgen_protocol_ops.c >nul
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
findstr /C:"xl_class_Meter______add______int(((XL_Class_Meter*)xl6_class_data(value" build\test\cgen_protocol_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_protocol_compound_assign.c missing class += protocol call.
    exit /b 1
)
findstr /C:"xvalue __xl6_class_arg_" build\test\cgen_protocol_compound_assign.c >nul
if errorlevel 1 (
    echo cgen_protocol_compound_assign.c missing inline class argument temporary.
    exit /b 1
)
findstr /C:"xl_class_Meter______add__________class__handle_of_Meter(((XL_Class_Meter*)xl6_class_data(value" build\test\cgen_protocol_compound_assign.c >nul
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
findstr /C:"xl_class_Player______get______hp__int__int(((XL_Class_Player*)xl6_class_data(player" build\test\cgen_class_property_get_let.c >nul
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
findstr /C:"xl_class_Score______add____(((XL_Class_Score*)xl6_class_data(__xl6_property_current_" build\test\cgen_class_property_compound_managed.c >nul
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
findstr /C:"direct = xvoCreateObject(xl6_type_desc_XL_Class_Ray()" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing managed nested global construction.
    exit /b 1
)
findstr /C:".value1 = xvoCreateObject(xl6_type_desc_XL_Class_Ray()" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing managed multi-return class construction.
    exit /b 1
)
findstr /C:"boxed = xvoCreateObject(xl6_type_desc_XL_Class_Box" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing managed generic class nested construction.
    exit /b 1
)
findstr /C:"xvalue __xl6_async_return_" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing managed async class return storage.
    exit /b 1
)
findstr /R /C:"__xl6_async_return_[0-9][0-9]* = xl6_xrt_move_to_shared(__xl6_async_return_" build\test\cgen_nested_class_constructor.c >nul
if errorlevel 1 (
    echo cgen_nested_class_constructor.c missing shared async class publication.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_nested_class_constructor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_nested_class_constructor.exe
if errorlevel 1 exit /b 1
build\test\cgen_nested_class_constructor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_stdlib_namespace_shadow.xl --emit build\test\cgen_stdlib_namespace_shadow.c
if errorlevel 1 exit /b 1
findstr /C:"xl_class_Vec__nearZero(((XL_Class_Vec*)xl6_class_data(dir" build\test\cgen_stdlib_namespace_shadow.c >nul
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
findstr /C:"xl_class_Sized______len____(((XL_Class_Sized*)xl6_class_data(value" build\test\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing named receiver len protocol call.
    exit /b 1
)
findstr /C:"xl_protocol_tmp_Sized______len____" build\test\cgen_protocol_len.c >nul
if errorlevel 1 (
    echo cgen_protocol_len.c missing temporary receiver len protocol wrapper.
    exit /b 1
)
findstr /C:"xl6_class_data(withField" build\test\cgen_protocol_len.c >nul
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
findstr /C:"xl_class_CounterIter______next____((XL_Class_CounterIter*)xl6_class_data(__xl6_class_iter_" build\test\cgen_protocol_iter.c >nul
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
findstr /C:"static void xl6_type_clone_XL_Class_Packet" build\test\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c missing XRT class clone adapter.
    exit /b 1
)
findstr /C:"xl6_type_clone_XL_Class_Packet" build\test\cgen_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_copy_clone.c does not register the class clone adapter.
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

release\xl.exe cgen test\cases\cgen_generic_class_copy_clone.xl --emit build\test\cgen_generic_class_copy_clone.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_clone_XL_Class_Packet__int" build\test\cgen_generic_class_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_generic_class_copy_clone.c missing generic XRT class clone adapter.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_copy_clone.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_copy_clone.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_copy_clone.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_class_custom_copy_clone.xl --emit build\test\cgen_class_custom_copy_clone.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue protocolValue = xl_class_Packet______copy____" build\test\cgen_class_custom_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_class_custom_copy_clone.c does not dispatch custom copy through XRT.
    exit /b 1
)
findstr /C:"xvalue protocolValue = xl_class_Packet______clone____" build\test\cgen_class_custom_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_class_custom_copy_clone.c does not dispatch custom clone through XRT.
    exit /b 1
)
findstr /C:"______xl6__copy__value" build\test\cgen_class_custom_copy_clone.c >nul
if not errorlevel 1 (
    echo cgen_class_custom_copy_clone.c still emits the removed payload-value copy helper.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_class_custom_copy_clone.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_class_custom_copy_clone.exe
if errorlevel 1 exit /b 1
build\test\cgen_class_custom_copy_clone.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_custom_copy_clone.xl --emit build\test\cgen_generic_class_custom_copy_clone.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue xl_class_Packet__int______copy____" build\test\cgen_generic_class_custom_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_generic_class_custom_copy_clone.c missing generic custom copy prototype.
    exit /b 1
)
findstr /C:"xvalue protocolValue = xl_class_Packet__int______clone____" build\test\cgen_generic_class_custom_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_generic_class_custom_copy_clone.c does not dispatch generic custom clone.
    exit /b 1
)
findstr /C:"xvoShare(xvoGetRecordOwner(self))" build\test\cgen_generic_class_custom_copy_clone.c >nul
if errorlevel 1 (
    echo cgen_generic_class_custom_copy_clone.c cannot recover the managed self handle.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_custom_copy_clone.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_custom_copy_clone.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_custom_copy_clone.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_class_owner_symbol_run.xl > build\test\cgen_class_owner_symbol_run.out.txt 2> build\test\cgen_class_owner_symbol_run.err.txt
if errorlevel 1 (
    type build\test\cgen_class_owner_symbol_run.err.txt
    echo cgen_class_owner_symbol_run.xl failed in memory-run mode.
    exit /b 1
)
findstr /C:"class owner symbol run ok" build\test\cgen_class_owner_symbol_run.out.txt >nul
if errorlevel 1 (
    echo cgen_class_owner_symbol_run.xl missing success marker.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_deepcopy_removed.xl --emit build\test\cgen_deepcopy_removed.c > build\test\cgen_deepcopy_removed.err.txt 2>&1
if not errorlevel 1 (
    echo cgen_deepcopy_removed.xl should fail because deepcopy is not a builtin alias.
    exit /b 1
)
findstr /C:"undeclared identifier 'deepcopy'" build\test\cgen_deepcopy_removed.err.txt >nul
if errorlevel 1 (
    echo cgen_deepcopy_removed.err.txt missing removed deepcopy diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_object_var_field_lifetime.xl --emit build\test\cgen_object_var_field_lifetime.c
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_object_var_field_lifetime.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_object_var_field_lifetime.exe
if errorlevel 1 exit /b 1
build\test\cgen_object_var_field_lifetime.exe
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 3/8
exit /b 0
