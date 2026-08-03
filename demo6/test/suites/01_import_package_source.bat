@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: import package source...

release\xl.exe check test\cases\import_missing_target.xl >nul 2> build\test\import_missing_target.err.txt
if not errorlevel 1 (
    echo import_missing_target.xl should fail when the import cannot be resolved.
    exit /b 1
)
findstr /C:"cannot resolve import" build\test\import_missing_target.err.txt >nul
if errorlevel 1 (
    echo import_missing_target.err.txt missing unresolved import diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_source_self_cycle.xl >nul 2> build\test\import_source_self_cycle.err.txt
if not errorlevel 1 (
    echo import_source_self_cycle.xl should reject a source import cycle.
    exit /b 1
)
findstr /C:"source import cycle detected" build\test\import_source_self_cycle.err.txt >nul
if errorlevel 1 (
    echo import_source_self_cycle.err.txt missing source cycle diagnostic.
    exit /b 1
)

release\xl.exe check test\fixtures\import_source_cycle\a.xl >nul 2> build\test\import_source_mutual_cycle.err.txt
if not errorlevel 1 (
    echo mutual source import cycle should fail.
    exit /b 1
)
findstr /C:"source import cycle detected" build\test\import_source_mutual_cycle.err.txt >nul
if errorlevel 1 (
    echo import_source_mutual_cycle.err.txt missing source cycle diagnostic.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=Join-Path (Resolve-Path 'build\test') (([char]0x4E2D)+([char]0x6587)+'_install_package'); New-Item -ItemType Directory -Force -Path $root | Out-Null; Copy-Item -LiteralPath 'release\xl.exe','release\xlc.dll','release\xl.dll' -Destination $root -Force; Copy-Item -LiteralPath 'test\cases\import_package_use.xl' -Destination $root -Force; $fixtures=Join-Path $root 'fixtures'; if(-not (Test-Path -LiteralPath $fixtures)){Copy-Item -LiteralPath 'test\fixtures' -Destination $fixtures -Recurse -Force}; $old=$env:XLANG_PACKAGE_PATH; try{$env:XLANG_PACKAGE_PATH=$fixtures; & (Join-Path $root 'xl.exe') check (Join-Path $root 'import_package_use.xl'); if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}; & (Join-Path $root 'xl.exe') run (Join-Path $root 'import_package_use.xl'); exit $LASTEXITCODE} finally{$env:XLANG_PACKAGE_PATH=$old}"
if errorlevel 1 (
    echo package resolution from a UTF-8 installation path failed.
    exit /b 1
)

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
findstr /C:"undeclared identifier 'add'" build\test\import_source_no_global_pollution.err.txt >nul
if errorlevel 1 (
    echo import_source_no_global_pollution.err.txt missing undeclared import symbol diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_package_use.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_rapidocr_main_entry.xl
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
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\import_deps_snapshot_host.c src\compile\xl6_imports.c src\package\xl6_package.c src\abi\xl6_abi_json.c src\abi\xl6_callable.c src\type\xl6_type_model.c src\sem\xl6_type_registry.c src\common\xl6_console.c src\common\xl6_output.c src\parser\xl6_parser_api.c src\parser\xl6_parser.c src\lexer\xl6_lexer_api.c src\lexer\xl6_lexer_gen.c src\lexer\xl6_token.c src\ast\xl6_ast.c src\diag\xl6_diag.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\import_deps_snapshot_host.exe
if errorlevel 1 exit /b 1
build\test\import_deps_snapshot_host.exe test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
build\test\import_deps_snapshot_host.exe test\cases\import_builtin_features.xl 7
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\package_manifest_strict_host.c src\package\xl6_package.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\package_manifest_strict_host.exe
if errorlevel 1 exit /b 1
build\test\package_manifest_strict_host.exe
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
release\xl.exe check test\cases\import_no_global_pollution.xl >nul 2> build\test\import_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_no_global_pollution.xl should fail.
    exit /b 1
)
findstr /C:"undeclared identifier 'counterAdd'" build\test\import_no_global_pollution.err.txt >nul
if errorlevel 1 (
    echo import_no_global_pollution.err.txt missing undeclared package symbol diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_path_no_ext.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_relative_scope\root\source\import_relative_scope.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_package_use.xl --emit build\test\import_package_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_import_counter_add\" func __xl6_import_counter___xl6_native_counter_counterAdd" build\test\import_package_use.xlir >nul
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
release\xl.exe run test\cases\import_namespace_local_shadow.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_package_multi_source_scope.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_bugfix_pkg.xl
if errorlevel 1 exit /b 1

release\xl.exe run test\cases\import_dependency_private.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_dependency_versions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_versions.xl -o build\test\import_dependency_versions.exe
if errorlevel 1 exit /b 1
build\test\import_dependency_versions.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_dependency_string_shorthand.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_shared_transitive_dependency.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_export_overload_pkg.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\import_export_overload_pkg.xl --emit build\test\import_export_overload_pkg.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_export_____xl6__import__export__overload__pkg__value__int" build\test\import_export_overload_pkg.c >nul
if errorlevel 1 (
    echo import_export_overload_pkg.c missing int export overload symbol.
    exit /b 1
)
findstr /C:"xl6_export_____xl6__import__export__overload__pkg__value__string" build\test\import_export_overload_pkg.c >nul
if errorlevel 1 (
    echo import_export_overload_pkg.c missing string export overload symbol.
    exit /b 1
)
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
findstr /C:"export extern \"xl6_imported_counter\" int __xl6_import_settings_importedCounter" build\test\import_package_var_use.xlir >nul
if errorlevel 1 (
    echo import_package_var_use.xlir missing package extern variable.
    exit /b 1
)

rem Public module types, namespace aliases and selective imports.
release\xl.exe run test\cases\import_public_types_namespace.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_public_types_alias.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_public_types_selective.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_public_types_identity.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_public_types_namespace_upgrade.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_public_types_qualified_surfaces.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_builtin_alias.xl
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_source_public_types.xl
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_source_public_types_module.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\import_public_types_qualified_surfaces.xl -o build\test\import_public_types_qualified_surfaces.exe
if errorlevel 1 exit /b 1
build\test\import_public_types_qualified_surfaces.exe
if errorlevel 1 exit /b 1

release\xl.exe parse test\cases\import_public_types_selective.xl --format tree --emit build\test\import_public_types_selective.ast
if errorlevel 1 exit /b 1
findstr /C:"import-selective" build\test\import_public_types_selective.ast >nul
if errorlevel 1 (
    echo import_public_types_selective.ast missing selective import flag.
    exit /b 1
)

"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\import_expand_host.c src\compile\xl6_imports.c src\package\xl6_package.c src\abi\xl6_abi_json.c src\abi\xl6_callable.c src\type\xl6_type_model.c src\sem\xl6_type_registry.c src\common\xl6_console.c src\common\xl6_output.c src\parser\xl6_parser_api.c src\parser\xl6_parser.c src\lexer\xl6_lexer_api.c src\lexer\xl6_lexer_gen.c src\lexer\xl6_token.c src\ast\xl6_ast.c src\diag\xl6_diag.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o build\test\import_expand_host.exe
if errorlevel 1 exit /b 1
build\test\import_expand_host.exe test\cases\import_public_types_namespace.xl test\cases\import_public_types_alias.xl test\cases\import_public_types_selective.xl test\cases\import_public_types_identity.xl test\cases\import_public_types_namespace_upgrade.xl test\cases\import_public_types_qualified_surfaces.xl test\cases\import_source_public_types.xl
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_private_fail.xl >nul 2> build\test\import_public_types_private_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"cannot import private or missing symbol" build\test\import_public_types_private_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_missing_fail.xl >nul 2> build\test\import_public_types_missing_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"cannot import private or missing symbol" build\test\import_public_types_missing_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_unqualified_fail.xl >nul 2> build\test\import_public_types_unqualified_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"undeclared identifier 'Point'" build\test\import_public_types_unqualified_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_alias_conflict_fail.xl >nul 2> build\test\import_public_types_alias_conflict_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import alias conflicts with an existing declaration" build\test\import_public_types_alias_conflict_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_non_type_fail.xl >nul 2> build\test\import_public_types_non_type_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import symbol is not a type" build\test\import_public_types_non_type_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_hidden_qualified_fail.xl >nul 2> build\test\import_public_types_hidden_qualified_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import package has no such exported type" build\test\import_public_types_hidden_qualified_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_alias_hides_canonical_fail.xl >nul 2> build\test\import_public_types_alias_hides_canonical_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"private package dependency is not exported" build\test\import_public_types_alias_hides_canonical_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_builtin_alias_fail.xl >nul 2> build\test\import_public_types_builtin_alias_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"selective import name conflicts with an existing declaration" build\test\import_public_types_builtin_alias_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_namespace_builtin_alias_fail.xl >nul 2> build\test\import_public_types_namespace_builtin_alias_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import alias conflicts with an existing declaration" build\test\import_public_types_namespace_builtin_alias_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_namespace_decl_conflict_fail.xl >nul 2> build\test\import_public_types_namespace_decl_conflict_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import namespace conflicts with an existing declaration" build\test\import_public_types_namespace_decl_conflict_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_direct_then_namespace_conflict_fail.xl >nul 2> build\test\import_public_types_direct_then_namespace_conflict_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"import namespace conflicts with an existing declaration" build\test\import_public_types_direct_then_namespace_conflict_fail.err.txt >nul
if errorlevel 1 exit /b 1

release\xl.exe check test\cases\import_public_types_anonymous_union_export_fail.xl >nul 2> build\test\import_public_types_anonymous_union_export_fail.err.txt
if not errorlevel 1 exit /b 1
findstr /C:"export union requires a name" build\test\import_public_types_anonymous_union_export_fail.err.txt >nul
if errorlevel 1 exit /b 1

echo DEMO6 suite ok: import package source
