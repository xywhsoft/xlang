@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: OOP ABI export and interop baseline 6/8...

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

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c -o build\test\build_mixed_two_c.exe
if errorlevel 1 exit /b 1
build\test\build_mixed_two_c.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$argsList=@('build','release\examples\feature_suite\09_c_export_abi.xl'); for($i=0;$i -lt 257;$i++){ $argsList += 'test\cases\build_mixed_extra.c' }; $argsList += @('-o','build\test\too_many_c.exe'); $p=Start-Process -FilePath 'release\xl.exe' -ArgumentList $argsList -NoNewWindow -PassThru -Wait -RedirectStandardOutput 'build\test\build_too_many_c.out.txt' -RedirectStandardError 'build\test\build_too_many_c.err.txt'; if($p.ExitCode -eq 0){exit 1}; if(-not (Select-String -Path 'build\test\build_too_many_c.err.txt' -SimpleMatch 'too many C source files')){exit 2}"
if errorlevel 1 (
    echo build too many C source diagnostic missing.
    exit /b 1
)

release\xl.exe build release\examples\feature_suite\09_c_export_abi.xl test\cases\build_mixed_host.c test\cases\build_mixed_extra.c --save-temps -o build\test\build_mixed_save_temps.exe > build\test\build_mixed_save_temps.log 2>&1
if errorlevel 1 exit /b 1
build\test\build_mixed_save_temps.exe
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $log=Get-Content 'build\test\build_mixed_save_temps.log' -Raw; $m=[regex]::Match($log,'temporary files kept:\s*(.+)'); if(-not $m.Success){exit 1}; $dir=$m.Groups[1].Value.Trim(); if(-not (Test-Path -LiteralPath $dir -PathType Container)){exit 2}; foreach($f in @('csrc_0.c','csrc_1.c')){$p=Join-Path $dir $f; if(-not (Test-Path -LiteralPath $p)){exit 3}; $t=Get-Content -LiteralPath $p -Raw; if($t -notmatch 'xrt\.h'){exit 4}; if($t -notmatch 'module\.h'){exit 5}}; $t0=Get-Content -LiteralPath (Join-Path $dir 'csrc_0.c') -Raw; $t1=Get-Content -LiteralPath (Join-Path $dir 'csrc_1.c') -Raw; if($t0 -notmatch 'build_mixed_host\.c'){exit 6}; if($t1 -notmatch 'build_mixed_extra\.c'){exit 7}; Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction Stop"
if errorlevel 1 (
    echo build save-temps wrapper check failed.
    exit /b 1
)

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\cgen_main_entry.xl -o build\test\explicit_xrt_link.exe --link %XL6_XRT_OBJ%
if errorlevel 1 exit /b 1
build\test\explicit_xrt_link.exe one
if not errorlevel 2 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)
if errorlevel 3 (
    echo explicit_xrt_link.exe should return argc 2.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'build\test' -Directory -Filter 'xl6_build_*').Count; Set-Content 'build\test\build_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cgen_main_entry.xl -shared -o build\test\bad_main_shared.dll >nul 2> build\test\bad_main_shared.err.txt
if not errorlevel 1 (
    echo shared build with xlang main should fail.
    exit /b 1
)
findstr /C:"shared build cannot contain xlang main" build\test\bad_main_shared.err.txt >nul
if errorlevel 1 (
    echo shared build error message missing.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'build\test\build_fail_dirs.before'); $after=@(Get-ChildItem 'build\test' -Directory -Filter 'xl6_build_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed build should clean temporary work directory.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=@(Get-ChildItem 'build\test' -Directory -Filter 'xl6_run_*').Count; Set-Content 'build\test\run_fail_dirs.before' $before"
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\sem_time_literal_bad.xl >nul 2> build\test\run_bad_script.err.txt
if not errorlevel 1 (
    echo bad script run should fail.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$before=[int](Get-Content 'build\test\run_fail_dirs.before'); $after=@(Get-ChildItem 'build\test' -Directory -Filter 'xl6_run_*').Count; if($after -ne $before){exit 1}"
if errorlevel 1 (
    echo failed run should clean temporary work directory.
    exit /b 1
)

"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\cli_link_host.c -o build\test\cli_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs build\test\libxl6clitest.a build\test\cli_link_host.o
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\cli_link.xl --lib-path build\test --link xl6clitest
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\cli_link.xl -o build\test\cli_link_build.exe --lib-path build\test --link xl6clitest
if errorlevel 1 exit /b 1
build\test\cli_link_build.exe
if errorlevel 1 exit /b 1

"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\cli_loadlib_host.c -o build\test\cli_loadlib_host.dll
if errorlevel 1 exit /b 1
cmd /c "set XL6_TCC=__missing_tcc__.exe&& release\xl.exe run test\cases\cli_loadlib.xl --loadlib build\test\cli_loadlib_host.dll"
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_puts.xl --emit build\test\extern_puts.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"puts\" func puts(text: string) as int32" build\test\extern_puts.xlir >nul
if errorlevel 1 (
    echo extern_puts.xlir missing extern declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_puts.xl --emit build\test\extern_puts.c
if errorlevel 1 exit /b 1
findstr /C:"int32_t puts(const char* text);" build\test\extern_puts.c >nul
if errorlevel 1 (
    echo extern_puts.c missing C prototype.
    exit /b 1
)

release\xl.exe run test\cases\extern_puts.xl > build\test\extern_puts.out.txt
if errorlevel 1 exit /b 1
findstr /C:"xlang calls c" build\test\extern_puts.out.txt >nul
if errorlevel 1 (
    echo extern_puts.xl did not call C puts.
    exit /b 1
)

release\xl.exe run release\examples\c_hello.xl > build\test\example_c_hello.out.txt
if errorlevel 1 exit /b 1
findstr /C:"hello from c" build\test\example_c_hello.out.txt >nul
if errorlevel 1 (
    echo release\examples\c_hello.xl did not run.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_bool.xl --emit build\test\extern_bool.c
if errorlevel 1 exit /b 1
findstr /C:"bool native_flip_bool(bool value);" build\test\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool extern prototype.
    exit /b 1
)
findstr /C:"void expect_bool(bool actual, bool expected);" build\test\extern_bool.c >nul
if errorlevel 1 (
    echo extern_bool.c missing C bool expect prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\extern_bool.c test\cases\extern_bool_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\extern_bool.exe
if errorlevel 1 exit /b 1
build\test\extern_bool.exe
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\c_abi_types.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\c_abi_types.xl --emit build\test\c_abi_types.c
if errorlevel 1 exit /b 1
findstr /C:"#include <stddef.h>" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing stddef include.
    exit /b 1
)
findstr /C:"size_t xl6_c_echo_size(size_t value);" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_size prototype.
    exit /b 1
)
findstr /C:"typedef intptr_t xl6_ssize_t;" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize typedef.
    exit /b 1
)
findstr /C:"xl6_ssize_t xl6_c_echo_ssize(xl6_ssize_t value);" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ssize prototype.
    exit /b 1
)
findstr /C:"ptrdiff_t xl6_c_echo_ptrdiff(ptrdiff_t value);" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_ptrdiff prototype.
    exit /b 1
)
findstr /C:"wchar_t xl6_c_echo_wchar(wchar_t value);" build\test\c_abi_types.c >nul
if errorlevel 1 (
    echo c_abi_types.c missing c_wchar prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\c_abi_types.c test\cases\c_abi_types_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\c_abi_types.exe
if errorlevel 1 exit /b 1
build\test\c_abi_types.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\c_abi_mismatch_extern_header.xl --emit build\test\c_abi_mismatch_extern_header.c --emit-header build\test\c_abi_mismatch_extern_header.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_bad_native_add(int64_t a, int64_t b);" build\test\c_abi_mismatch_extern_header.h >nul
if errorlevel 1 (
    echo c_abi_mismatch_extern_header.h missing extern prototype.
    exit /b 1
)
release\xl.exe build test\cases\c_abi_mismatch_extern_header.xl test\cases\c_abi_mismatch_extern_header_host.c -o build\test\c_abi_mismatch_extern_header.exe > build\test\c_abi_mismatch_extern_header.out.txt 2> build\test\c_abi_mismatch_extern_header.err.txt
if not errorlevel 1 (
    echo c_abi_mismatch_extern_header should fail.
    exit /b 1
)
findstr /C:"incompatible types for redefinition" build\test\c_abi_mismatch_extern_header.err.txt >nul
if errorlevel 1 (
    echo c_abi_mismatch_extern_header missing C ABI mismatch diagnostic.
    exit /b 1
)

release\xl.exe cgen test\cases\cgen_extern_string_return.xl --emit build\test\cgen_extern_string_return.c
if errorlevel 1 exit /b 1
findstr /C:"const char* native_text(void);" build\test\cgen_extern_string_return.c >nul
if errorlevel 1 (
    echo cgen_extern_string_return.c missing string return extern prototype.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\cgen_extern_string_return.c test\cases\cgen_extern_string_return_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1
build\test\cgen_extern_string_return.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\extern_var.xl --emit build\test\extern_var.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"host_counter\" int counter" build\test\extern_var.xlir >nul
if errorlevel 1 (
    echo extern_var.xlir missing extern var declaration.
    exit /b 1
)

release\xl.exe cgen test\cases\extern_var.xl --emit build\test\extern_var.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t host_counter;" build\test\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable declaration.
    exit /b 1
)
findstr /C:"host_counter = (host_counter + 2LL);" build\test\extern_var.c >nul
if errorlevel 1 (
    echo extern_var.c missing extern C variable assignment.
    exit /b 1
)
"%XL6_TCC%" %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\extern_var.c test\cases\extern_var_host.c test\cases\cgen_class_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\extern_var.exe
if errorlevel 1 exit /b 1
build\test\extern_var.exe
if errorlevel 1 exit /b 1

release\xl.exe ir test\cases\export_add.xl --emit build\test\export_add.xlir
if errorlevel 1 exit /b 1
findstr /C:"export \"xl6_add_i64\" func add_i64(a: int, b: int) as int" build\test\export_add.xlir >nul
if errorlevel 1 (
    echo export_add.xlir missing export declaration.
    exit /b 1
)

release\xl.exe abi test\cases\export_add.xl --emit build\test\export_add.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"export xl6_add_i64" build\test\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing C export symbol.
    exit /b 1
)
findstr /C:"symbol: xl6_export_add__i64" build\test\export_add.abi.txt >nul
if errorlevel 1 (
    echo export_add.abi.txt missing internal ABI symbol.
    exit /b 1
)

release\xl.exe cgen test\cases\export_add.xl --emit build\test\export_add.c --emit-header build\test\export_add.h
if errorlevel 1 exit /b 1
findstr /C:"long long xl6_export_add__i64(long long a, long long b);" build\test\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing internal export prototype.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" build\test\export_add.c >nul
if errorlevel 1 (
    echo export_add.c missing C export wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_add.c test\cases\export_add_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_add_host.exe
if errorlevel 1 exit /b 1
build\test\export_add_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_add.xl --emit build\test\export_add_header.c --emit-header build\test\export_add.h
if errorlevel 1 exit /b 1
findstr /C:"#ifndef XL6_EXPORT_ADD_H" build\test\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing header guard.
    exit /b 1
)
findstr /C:"int64_t xl6_add_i64(int64_t a, int64_t b);" build\test\export_add.h >nul
if errorlevel 1 (
    echo export_add.h missing C export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_add_header.c test\cases\export_add_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_add_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_add_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class.xl --emit build\test\export_class.c --emit-header build\test\export_class.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Point" build\test\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class typedef.
    exit /b 1
)
findstr /C:"xvalue xl6_point_make(int64_t x, int64_t y);" build\test\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class return export declaration.
    exit /b 1
)
findstr /C:"int64_t xl6_point_sum(xvalue point);" build\test\export_class.h >nul
if errorlevel 1 (
    echo export_class.h missing class parameter export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_class.c test\cases\export_class_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_class_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_method.xl --emit build\test\export_class_method.c --emit-header build\test\export_class_method.h
if errorlevel 1 exit /b 1
findstr /C:"int64_t xl6_point_move(XL_Class_Point* self, int64_t dx, int64_t dy);" build\test\export_class_method.h >nul
if errorlevel 1 (
    echo export_class_method.h missing class method C export declaration.
    exit /b 1
)
findstr /C:"void xl6_point_pair(XL_Class_Point* self, int64_t* out0, int64_t* out1);" build\test\export_class_method.h >nul
if errorlevel 1 (
    echo export_class_method.h missing class method multi-return C declaration.
    exit /b 1
)
findstr /C:"return xl_class_Point__move(self, dx, dy);" build\test\export_class_method.c >nul
if errorlevel 1 (
    echo export_class_method.c missing class export wrapper call.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_class_method.c test\cases\export_class_method_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_class_method_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_method_header_host.exe
if errorlevel 1 exit /b 1
release\xl.exe abi test\cases\export_class_method.xl --emit build\test\export_class_method.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.Point.move(int,int) as int" build\test\export_class_method.abi.txt >nul
if errorlevel 1 (
    echo export_class_method.abi.txt missing class method ABI signature.
    exit /b 1
)
release\xl.exe abi test\cases\export_class_method.xl --json --emit build\test\export_class_method.abi.json
if errorlevel 1 exit /b 1
findstr /C:"\"kind\":\"export_class_method\"" build\test\export_class_method.abi.json >nul
if errorlevel 1 (
    echo export_class_method.abi.json missing class method export kind.
    exit /b 1
)

release\xl.exe cgen test\cases\export_class_inherit.xl --emit build\test\export_class_inherit.c --emit-header build\test\export_class_inherit.h
if errorlevel 1 exit /b 1
findstr /C:"typedef struct XL_Class_Entity" build\test\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing base class typedef.
    exit /b 1
)
findstr /C:"XL_Class_Entity base;" build\test\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited base field.
    exit /b 1
)
findstr /C:"xvalue xl6_named_point_make(int64_t id, int64_t x, int64_t y);" build\test\export_class_inherit.h >nul
if errorlevel 1 (
    echo export_class_inherit.h missing inherited class return export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_class_inherit.c test\cases\export_class_inherit_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_inherit_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_class_union_field.xl --emit build\test\export_class_union_field.c --emit-header build\test\export_class_union_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" build\test\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing dependent union typedef.
    exit /b 1
)
findstr /C:"XL_Union_Payload payload;" build\test\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class union field.
    exit /b 1
)
findstr /C:"xvalue xl6_box_make(int64_t value);" build\test\export_class_union_field.h >nul
if errorlevel 1 (
    echo export_class_union_field.h missing class-with-union export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_class_union_field.c test\cases\export_class_union_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_class_union_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_union_class_field.xl --emit build\test\export_union_class_field.c --emit-header build\test\export_union_class_field.h
if errorlevel 1 exit /b 1
findstr /C:"typedef union XL_Union_Payload" build\test\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union typedef.
    exit /b 1
)
findstr /C:"int64_t integer;" build\test\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union scalar field.
    exit /b 1
)
findstr /C:"XL_Union_Payload xl6_payload_make(int64_t value);" build\test\export_union_class_field.h >nul
if errorlevel 1 (
    echo export_union_class_field.h missing union export declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_union_class_field.c test\cases\export_union_class_field_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_union_class_field_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\export_add.xl -shared -o build\test\xl6_export_add.dll --emit-header build\test\xl6_export_add_shared.h
if errorlevel 1 exit /b 1
if not exist build\test\xl6_export_add.dll (
    echo xl6_export_add.dll missing after build -shared.
    exit /b 1
)
if not exist build\test\xl6_export_add_shared.h (
    echo xl6_export_add_shared.h missing after build -shared.
    exit /b 1
)

release\xl.exe abi test\cases\export_multi_return.xl --emit build\test\export_multi_return.abi.txt
if errorlevel 1 exit /b 1
findstr /C:"signature: fn:app.pair(int) as (int,int)" build\test\export_multi_return.abi.txt >nul
if errorlevel 1 (
    echo export_multi_return.abi.txt missing multi-return signature.
    exit /b 1
)

release\xl.exe cgen test\cases\export_multi_return.xl --emit build\test\export_multi_return.c --emit-header build\test\export_multi_return.h
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\export_multi_return.c' -Raw; if($t -notmatch 'typedef struct XL6_Function_Return_[0-9A-F]+'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing internal return struct.
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=Get-Content 'build\test\export_multi_return.c' -Raw; if($t -notmatch 'XL6_Function_Return_[0-9A-F]+ xl6_export_pair\(long long seed\);'){exit 1}"
if errorlevel 1 (
    echo export_multi_return.c missing multi-return internal prototype.
    exit /b 1
)
findstr /C:"void xl6_pair_i64(int64_t seed, int64_t* out0, int64_t* out1);" build\test\export_multi_return.h >nul
if errorlevel 1 (
    echo export_multi_return.h missing out-param wrapper declaration.
    exit /b 1
)
findstr /C:"*out1 = (int64_t)__xl6_return.value1;" build\test\export_multi_return.c >nul
if errorlevel 1 (
    echo export_multi_return.c missing out-param assignment.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_multi_return.c test\cases\export_multi_return_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_multi_return_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_bool.xl --emit build\test\export_bool.c --emit-header build\test\export_bool.h
if errorlevel 1 exit /b 1
findstr /C:"int xl6_export_isEven(long long value);" build\test\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing internal bool export prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" build\test\export_bool.c >nul
if errorlevel 1 (
    echo export_bool.c missing C bool wrapper prototype.
    exit /b 1
)
findstr /C:"bool xl6_is_even(int64_t value);" build\test\export_bool.h >nul
if errorlevel 1 (
    echo export_bool.h missing C bool wrapper declaration.
    exit /b 1
)
"%XL6_TCC%" -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_bool.c test\cases\export_bool_header_host.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\export_bool_header_host.exe
if errorlevel 1 exit /b 1
build\test\export_bool_header_host.exe
if errorlevel 1 exit /b 1

release\xl.exe cgen test\cases\export_primitive_widths.xl --emit build\test\export_primitive_widths.c --emit-header build\test\export_primitive_widths.h
if errorlevel 1 exit /b 1
findstr /C:"uint64_t xl6_pick_u64(" build\test\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing uint64 return wrapper.
    exit /b 1
)
findstr /C:"int8_t i8, uint8_t u8, int16_t i16, uint16_t u16, int32_t i32, uint32_t u32, int64_t i64, uint64_t u64, float f32, double f64, bool ok, void* handle" build\test\export_primitive_widths.h >nul
if errorlevel 1 (
    echo export_primitive_widths.h missing primitive width parameter mapping.
    exit /b 1
)
findstr /C:"uint64_t xl6_pick_u64(" build\test\export_primitive_widths.c >nul
if errorlevel 1 (
    echo export_primitive_widths.c missing C wrapper prototype.
    exit /b 1
)
"%XL6_TCC%" -c -Ibuild\test %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% build\test\export_primitive_widths.c -o build\test\export_primitive_widths.o
if errorlevel 1 exit /b 1


echo DEMO6 suite ok: OOP ABI export and interop baseline 6/8
exit /b 0
