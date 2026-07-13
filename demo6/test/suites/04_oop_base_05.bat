@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 5/8...

release\xl.exe cgen test\cases\cgen_generic_class.xl --emit build\test\cgen_generic_class.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int class instance.
    exit /b 1
)
findstr /C:"long long value;" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int field substitution.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Box__string {" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string class instance.
    exit /b 1
)
findstr /C:"const char* value;" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string field substitution.
    exit /b 1
)
findstr /C:"xvalue intBox;" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing int variable declaration.
    exit /b 1
)
findstr /C:"xvalue stringBox;" build\test\cgen_generic_class.c >nul
if errorlevel 1 (
    echo cgen_generic_class.c missing string variable declaration.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_fields.xl --emit build\test\cgen_generic_class_fields.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Box__int {" build\test\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing int class instance.
    exit /b 1
)
findstr /C:"(*((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field assignment.
    exit /b 1
)
findstr /C:")).value, 123LL);" build\test\cgen_generic_class_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_fields.c missing generic class field read.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_function_fields.xl --emit build\test\cgen_generic_class_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"static void xl6_type_copy_XL_Class_Mapper__int" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type copy helper.
    exit /b 1
)
findstr /C:"static const xrt_type_desc* xl6_type_desc_XL_Class_Mapper__int" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class type desc helper.
    exit /b 1
)
findstr /C:"static xvalue xl6_box_XL_Class_Mapper__int" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic class box helper.
    exit /b 1
)
findstr /C:"xl6_function_call_var((*((XL_Class_Mapper__int*)xl6_class_data(copied" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic member function call lowering.
    exit /b 1
)
findstr /C:"xl6_function_call_var((*((XL_Class_Mapper__int*)xl6_class_data(item" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic iteration member function call lowering.
    exit /b 1
)
findstr /C:"({ XL6_Function __xl6_member_fn_" build\test\cgen_generic_class_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_function_fields.c should not use GNU statement expressions for generic member function calls.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(*((xvalue*)xl6_xrt_typed_require_item" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed index retain.
    exit /b 1
)
findstr /C:"if (item != 0) { xvoAddRef(item); }" build\test\cgen_generic_class_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_function_fields.c missing generic typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_nested_function_fields.xl --emit build\test\cgen_generic_class_nested_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" build\test\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"xl_class_Mapper__destroy" build\test\cgen_generic_class_nested_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c should not call unresolved Mapper destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Mapper__int {" build\test\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested Mapper int instance.
    exit /b 1
)
findstr /C:"xvalue inner;" build\test\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing substituted nested class field.
    exit /b 1
)
findstr /C:"if (self->inner != 0) { xvoUnref(self->inner); self->inner = 0; }" build\test\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing generic nested destroy call.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(*((xvalue*)xl6_xrt_typed_require_item" build\test\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed index retain.
    exit /b 1
)
findstr /C:"if (item != 0) { xvoAddRef(item); }" build\test\cgen_generic_class_nested_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_nested_function_fields.c missing nested typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_nested_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_nested_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_deep_function_fields.xl --emit build\test\cgen_generic_class_deep_function_fields.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_Mapper__T" build\test\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Mapper generic type.
    exit /b 1
)
findstr /C:"XL_Class_Middle__T" build\test\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not emit unresolved Middle generic type.
    exit /b 1
)
findstr /C:"xl_class_Middle__destroy" build\test\cgen_generic_class_deep_function_fields.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c should not call unresolved Middle destroy.
    exit /b 1
)
findstr /C:"typedef struct XL_Class_Middle__int {" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing nested Middle int instance.
    exit /b 1
)
findstr /C:"xvalue mapper;" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Mapper field.
    exit /b 1
)
findstr /C:"xvalue middle;" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing substituted Middle field.
    exit /b 1
)
findstr /C:"if (self->middle != 0) { xvoUnref(self->middle); self->middle = 0; }" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Middle destroy call.
    exit /b 1
)
findstr /C:"if (self->mapper != 0) { xvoUnref(self->mapper); self->mapper = 0; }" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing generic Mapper destroy call.
    exit /b 1
)
findstr /C:"xl6_xrt_retain(*((xvalue*)xl6_xrt_typed_require_item" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed index retain.
    exit /b 1
)
findstr /C:"if (item != 0) { xvoAddRef(item); }" build\test\cgen_generic_class_deep_function_fields.c >nul
if errorlevel 1 (
    echo cgen_generic_class_deep_function_fields.c missing deep typed iteration retain.
    exit /b 1
)
"%XL6_TCC%" -Irelease\tcc\include_win %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_deep_function_fields.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_deep_function_fields.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_methods.xl --emit build\test\cgen_generic_class_methods.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__valueOf(XL_Class_Box__int* self);" build\test\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method prototype.
    exit /b 1
)
findstr /C:"= self->value;" build\test\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method body.
    exit /b 1
)
findstr /C:"value = xl_class_Box__int__valueOf(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_methods.c >nul
if errorlevel 1 (
    echo cgen_generic_class_methods.c missing generic class method call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_methods.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_methods.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_auto.xl --emit build\test\cgen_generic_class_auto.c
if errorlevel 1 exit /b 1
findstr /C:"fromField = (*((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic field auto inference.
    exit /b 1
)
findstr /C:"fromMethod = xl_class_Box__int__valueOf(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_auto.c >nul
if errorlevel 1 (
    echo cgen_generic_class_auto.c missing generic method return auto inference.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_auto.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_auto.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_overloads.xl --emit build\test\cgen_generic_class_overloads.c
if errorlevel 1 exit /b 1
findstr /C:"long long xl_class_Box__int__pick__int(XL_Class_Box__int* self, long long value);" build\test\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload prototype.
    exit /b 1
)
findstr /C:"long long xl_class_Box__int__pick__string(XL_Class_Box__int* self, const char* text);" build\test\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload prototype.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__int(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing int generic class overload call.
    exit /b 1
)
findstr /C:"expect_i64(xl_class_Box__int__pick__string(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_overloads.c >nul
if errorlevel 1 (
    echo cgen_generic_class_overloads.c missing string generic class overload call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_overloads.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_overloads.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_ctor.xl --emit build\test\cgen_generic_class_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"void xl_class_Box__int__create(XL_Class_Box__int* self, long long value);" build\test\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create prototype.
    exit /b 1
)
findstr /C:"self->value = value;" build\test\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create body.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&__xl6_box_ctor_stmt_" build\test\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class create call.
    exit /b 1
)
findstr /C:"value = xl_class_Box__int__valueOf(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_ctor.c missing generic class method call after create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_iter_protocol.xl --emit build\test\cgen_generic_class_iter_protocol.c
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_PairIter__int {" build\test\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic iterator class dependency.
    exit /b 1
)
findstr /C:"XL_Class_PairIter__T" build\test\cgen_generic_class_iter_protocol.c >nul
if not errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c leaked unresolved iterator generic type.
    exit /b 1
)
findstr /C:"xvalue xl_class_PairBox__int______iter____" build\test\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic __iter__ return type.
    exit /b 1
)
findstr /C:"xl_class_PairIter__int______next____((XL_Class_PairIter__int*)xl6_class_data(__xl6_class_iter_" build\test\cgen_generic_class_iter_protocol.c >nul
if errorlevel 1 (
    echo cgen_generic_class_iter_protocol.c missing generic __next__ call.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_iter_protocol.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_iter_protocol.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_iter_protocol.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_explicit_ctor.xl --emit build\test\cgen_generic_class_explicit_ctor.c
if errorlevel 1 exit /b 1
findstr /C:"xvalue intBox;" build\test\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class variable declaration.
    exit /b 1
)
findstr /C:"xl_class_Box__int__create(&__xl6_box_ctor_stmt_" build\test\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing explicit generic class create call.
    exit /b 1
)
findstr /C:"value = xl_class_Box__int__valueOf(((XL_Class_Box__int*)xl6_class_data(intBox" build\test\cgen_generic_class_explicit_ctor.c >nul
if errorlevel 1 (
    echo cgen_generic_class_explicit_ctor.c missing method call after explicit create.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_explicit_ctor.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_explicit_ctor.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_generic_class_type_values.xl --emit build\test\cgen_generic_class_type_values.c
if errorlevel 1 exit /b 1
findstr /C:"leftType = 0x" build\test\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing generic class type value.
    exit /b 1
)
findstr /C:"expect_i64((leftType == rightType), 1LL);" build\test\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing equal generic class type comparison.
    exit /b 1
)
findstr /C:"expect_i64((leftType == textType), 0LL);" build\test\cgen_generic_class_type_values.c >nul
if errorlevel 1 (
    echo cgen_generic_class_type_values.c missing distinct generic class type comparison.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_generic_class_type_values.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1
build\test\cgen_generic_class_type_values.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\cgen_basic.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_basic.xl -o build\test\xl6_build_basic.exe
if errorlevel 1 exit /b 1
build\test\xl6_build_basic.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_range.xl -o build\test\xl6_build_range.exe
if errorlevel 1 exit /b 1
build\test\xl6_build_range.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_main_entry.xl -o build\test\cgen_main_entry.exe
if errorlevel 1 exit /b 1
build\test\cgen_main_entry.exe alpha beta
if not errorlevel 3 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)
if errorlevel 4 (
    echo cgen_main_entry.exe should return argc 3.
    exit /b 1
)

release\xl.exe run test\cases\xl_run_args.xl -- alpha "two words" "quote\"value" beta ""
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\cgen_main_entry.xl --emit build\test\cgen_main_entry.c
if errorlevel 1 exit /b 1
findstr /C:"#define XL_RUNTIME_NEED_ARGV 1" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv runtime need macro.
    exit /b 1
)
findstr /C:"static xvalue xl6_argv_to_arg(int argc, char** argv)" release\include\xl_runtime.h >nul
if errorlevel 1 (
    echo xl_runtime.h missing argv-to-arg helper.
    exit /b 1
)
findstr /C:"XL_InitOptions __xl6_options = xlInitOptions(argc, argv);" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlInitOptions call.
    exit /b 1
)
findstr /C:"__xl6_arg = xl6_command_argv_to_arg(argc, argv);" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv-to-arg call.
    exit /b 1
)
findstr /C:"__xl6_code = xlang_main(__xl6_arg);" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlang main dispatch.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_main_entry.c' -Raw; $s=$t.IndexOf('int xlang_main(xvalue arg)'); $e=$t.IndexOf('int main(int argc, char** argv)', $s); if($s -lt 0 -or $e -lt 0){exit 1}; $body=$t.Substring($s, $e - $s); if($body -match 'xvoUnref\(arg\)'){exit 2}"
if errorlevel 1 (
    echo cgen_main_entry.c should borrow xlang_main arg.
    exit /b 1
)
findstr /C:"xvoUnref(__xl6_arg);" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing argv arg release.
    exit /b 1
)
findstr /C:"xlUnit();" build\test\cgen_main_entry.c >nul
if errorlevel 1 (
    echo cgen_main_entry.c missing xlUnit call.
    exit /b 1
)
release\xl.exe cgen test\cases\cgen_main_uncaught.xl --emit build\test\cgen_main_uncaught.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_error_print_uncaught();" build\test\cgen_main_uncaught.c >nul
if errorlevel 1 (
    echo cgen_main_uncaught.c missing uncaught main error printer.
    exit /b 1
)
release\xl.exe run test\cases\cgen_main_uncaught.xl >nul 2> build\test\cgen_main_uncaught.err.txt
if not errorlevel 1 (
    echo cgen_main_uncaught.xl should fail at runtime.
    exit /b 1
)
findstr /C:"uncaught Error: main boom" build\test\cgen_main_uncaught.err.txt >nul
if errorlevel 1 (
    echo cgen_main_uncaught.err.txt missing uncaught main exception diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_main_case_sensitive.xl --emit build\test\cgen_main_case_sensitive.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_main_case_sensitive.c' -Raw; if($t -match 'xlang_main'){exit 1}; if($t -match 'int main\('){exit 2}; if($t -notmatch 'long long xl_fn_Main\(xvalue arg\)'){exit 3}"
if errorlevel 1 (
    echo cgen_main_case_sensitive.c should treat Main as a normal function.
    exit /b 1
)
release\xl.exe check test\cases\cgen_main_case_sensitive_script.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\cgen_main_case_sensitive_script.xl --emit build\test\cgen_main_case_sensitive_script.c
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\cgen_main_case_sensitive_script.c' -Raw; if($t -notmatch 'int xlang_main\(xvalue arg\)'){exit 1}; if($t -notmatch 'int main\('){exit 2}; if($t -notmatch 'long long xl_fn_Main\(xvalue arg\)'){exit 3}; if($t -notmatch 'expect_i64\(1LL, 1LL\);'){exit 4}"
if errorlevel 1 (
    echo cgen_main_case_sensitive_script.c should treat Main as normal function and top-level code as script main.
    exit /b 1
)
release\xl.exe build test\cases\cgen_main_case_sensitive_script.xl test\cases\cgen_class_host.c -o build\test\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1
build\test\cgen_main_case_sensitive_script.exe
if errorlevel 1 exit /b 1

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c -o build\test\build_mixed_host.exe
if errorlevel 1 exit /b 1
build\test\build_mixed_host.exe
if errorlevel 1 exit /b 1

release\xl.exe help > build\test\xl_help.txt
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "if(-not (Select-String -Path 'build\test\xl_help.txt' -SimpleMatch 'xl build')){exit 1}"
if errorlevel 1 (
    echo xl_help.txt missing build usage.
    exit /b 1
)

if exist cgen_basic.exe del /f /q cgen_basic.exe

echo DEMO6 suite ok: OOP ABI export and interop baseline 5/8
exit /b 0
