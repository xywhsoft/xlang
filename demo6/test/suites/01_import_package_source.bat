@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: import package source...

release\xl.exe run test\cases\import_source_use.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_source_math.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_source_use.xl -o build\test\import_source_use.exe
if errorlevel 1 exit /b 1
build\test\import_source_use.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\import_source_native_use.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_source_native.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_source_native_use.xl -o build\test\import_source_native_use.exe
if errorlevel 1 exit /b 1
build\test\import_source_native_use.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\companion_entry.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\companion_entry.xl -o build\test\companion_entry.exe
if errorlevel 1 exit /b 1
build\test\companion_entry.exe
if errorlevel 1 exit /b 1

release\xl.exe run test\fixtures\xlp_app\app.xlp
if errorlevel 1 exit /b 1
release\xl.exe build test\fixtures\xlp_app\app.xlp -o build\test\xlp_app.exe
if errorlevel 1 exit /b 1
build\test\xlp_app.exe
if errorlevel 1 exit /b 1
release\xl.exe build test\fixtures\xlp_lib\lib.xlp -o build\test\xlp_lib.dll
if errorlevel 1 exit /b 1

release\xl.exe check test\fixtures\xlp_bad_missing_entry.xlp >nul 2> build\test\xlp_bad_missing_entry.err.txt
if not errorlevel 1 (
    echo xlp_bad_missing_entry.xlp should fail.
    exit /b 1
)
findstr /C:"xlp requires entry" build\test\xlp_bad_missing_entry.err.txt >nul
if errorlevel 1 (
    echo xlp_bad_missing_entry.err.txt missing entry diagnostic.
    exit /b 1
)

release\xl.exe check test\fixtures\xlp_bad_object.xlp >nul 2> build\test\xlp_bad_object.err.txt
if not errorlevel 1 (
    echo xlp_bad_object.xlp should fail.
    exit /b 1
)
findstr /C:"xlp cSources only accepts C source files" build\test\xlp_bad_object.err.txt >nul
if errorlevel 1 (
    echo xlp_bad_object.err.txt missing cSources diagnostic.
    exit /b 1
)

release\xl.exe build test\fixtures\xlp_app\app.xlp -o build\test\xlp_app_conflict.dll --shared >nul 2> build\test\xlp_app_conflict.err.txt
if not errorlevel 1 (
    echo xlp_app.xlp --shared conflict should fail.
    exit /b 1
)
findstr /C:"xlp kind conflicts with -shared" build\test\xlp_app_conflict.err.txt >nul
if errorlevel 1 (
    echo xlp_app_conflict.err.txt missing kind conflict diagnostic.
    exit /b 1
)

release\xl.exe build test\cases\import_source_no_global_pollution.xl -o build\test\import_source_no_global_pollution.exe >nul 2> build\test\import_source_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_source_no_global_pollution.xl should fail.
    exit /b 1
)
findstr /C:"[checked]" build\test\import_source_no_global_pollution.err.txt >nul
if errorlevel 1 (
    echo import_source_no_global_pollution.err.txt missing unresolved symbol diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_package_use.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_builtin_features.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_dependency_version_mismatch.xl >nul 2> build\test\import_dependency_version_mismatch.err.txt
if not errorlevel 1 (
    echo import_dependency_version_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"package dependency version mismatch" build\test\import_dependency_version_mismatch.err.txt >nul
if errorlevel 1 (
    echo import_dependency_version_mismatch.err.txt missing version mismatch diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_lib_search.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1
call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1
"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\metadata_combo_link_host.c -o build\test\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs build\test\metadata_combo_link.a build\test\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\metadata_combo_loadlib_host.c -o build\test\metadata_combo_loadlib.dll
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_metadata_combo.xl -o build\test\import_metadata_combo.exe
if errorlevel 1 exit /b 1
build\test\import_metadata_combo.exe
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\import_deps_snapshot_host.c src\compile\xl6_imports.c src\package\xl6_package.c src\abi\xl6_abi_json.c src\abi\xl6_callable.c src\type\xl6_type_model.c src\common\xl6_console.c src\common\xl6_output.c src\parser\xl6_parser_api.c src\parser\xl6_parser.c src\lexer\xl6_lexer_api.c src\lexer\xl6_lexer_gen.c src\lexer\xl6_token.c src\ast\xl6_ast.c src\diag\xl6_diag.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\import_deps_snapshot_host.exe
if errorlevel 1 exit /b 1
build\test\import_deps_snapshot_host.exe test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
build\test\import_deps_snapshot_host.exe test\cases\import_builtin_features.xl 7
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_native_singleton_conflict.xl >nul 2> build\test\import_native_singleton_conflict.err.txt
if not errorlevel 1 (
    echo import_native_singleton_conflict.xl should fail.
    exit /b 1
)
findstr /C:"native singleton package conflict" build\test\import_native_singleton_conflict.err.txt >nul
if errorlevel 1 (
    echo import_native_singleton_conflict.err.txt missing native singleton diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_no_global_pollution.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_path_no_ext.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_relative_scope\root\source\import_relative_scope.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_package_use.xl --emit build\test\import_package_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_import_counter_add\" func __xl6_import_counter_counterAdd" build\test\import_package_use.xlir >nul
if errorlevel 1 (
    echo import_package_use.xlir missing package extern.
    exit /b 1
)
findstr /C:"call __xl6_import_counter_counterAdd" build\test\import_package_use.xlir >nul
if errorlevel 1 (
    echo import_package_use.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe run test\cases\import_package_use.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\import_dependency_private.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_dependency_versions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_versions.xl -o build\test\import_dependency_versions.exe
if errorlevel 1 exit /b 1
build\test\import_dependency_versions.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_dependency_no_pollution.xl >nul 2> build\test\import_dependency_no_pollution.err.txt
if not errorlevel 1 (
    echo import_dependency_no_pollution.xl should fail.
    exit /b 1
)
findstr /C:"private package dependency is not exported" build\test\import_dependency_no_pollution.err.txt >nul
if errorlevel 1 (
    echo import_dependency_no_pollution.err.txt missing private dependency diagnostic.
    exit /b 1
)

release\xl.exe run test\cases\import_package_type_class_use.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_package_type_union_use.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_package_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_package_var_use.xl --emit build\test\import_package_var_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_imported_counter\" var __xl6_import_settings_importedCounter : int" build\test\import_package_var_use.xlir >nul
if errorlevel 1 (
    echo import_package_var_use.xlir missing package extern variable.
    exit /b 1
)

echo DEMO6 suite ok: import package source
