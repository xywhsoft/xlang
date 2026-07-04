@echo off
setlocal
call "%~dp0common.bat"
if errorlevel 1 exit /b 1
echo Running DEMO6 suite: import package XAPI...

release\xl.exe check test\cases\import_metadata.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_metadata.xl --emit release\tmp\import_metadata.xlir
if errorlevel 1 exit /b 1
findstr /C:"import counter" release\tmp\import_metadata.xlir >nul
if errorlevel 1 (
    echo import_metadata.xlir missing package import.
    exit /b 1
)
findstr /C:"import \"local/math\"" release\tmp\import_metadata.xlir >nul
if errorlevel 1 (
    echo import_metadata.xlir missing string import.
    exit /b 1
)
release\xl.exe xapi test\cases\import_metadata.xl --emit release\tmp\import_metadata.xapi.json
if errorlevel 1 exit /b 1
findstr /C:"\"imports\"" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing imports array.
    exit /b 1
)
findstr /C:"counter" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing counter import.
    exit /b 1
)
findstr /C:"local/math" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing local path import.
    exit /b 1
)
findstr /C:"\"resolved\":true" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing resolved package import.
    exit /b 1
)
findstr /C:"xlang.package.json" release\tmp\import_metadata.xapi.json >nul
if errorlevel 1 (
    echo import_metadata.xapi.json missing resolved package manifest.
    exit /b 1
)
release\xl.exe cgen test\cases\import_metadata.xl --emit release\tmp\import_metadata.c
if errorlevel 1 exit /b 1
findstr /C:"int main(" release\tmp\import_metadata.c >nul
if not errorlevel 1 (
    echo import_metadata.c should not emit main.
    exit /b 1
)

release\xl.exe check test\cases\import_path_no_ext.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_path_no_ext.xl --emit release\tmp\import_path_no_ext.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_path_no_ext_value\" func __xl6_import_probe_value" release\tmp\import_path_no_ext.xlir >nul
if errorlevel 1 (
    echo import_path_no_ext.xlir missing generated extern.
    exit /b 1
)
findstr /C:"call __xl6_import_probe_value" release\tmp\import_path_no_ext.xlir >nul
if errorlevel 1 (
    echo import_path_no_ext.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_path_no_ext.xl --emit release\tmp\import_path_no_ext.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_path_no_ext_value" release\tmp\import_path_no_ext.c >nul
if errorlevel 1 (
    echo import_path_no_ext.c missing imported C symbol.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command "$root=(Resolve-Path '.').Path; $work=Join-Path $root 'test\cases\import_relative_scope\root'; $case='test\cases\import_relative_scope\root\source\import_relative_scope.xl'; $src=Join-Path $root $case; $correct=Join-Path $root 'test\cases\import_relative_scope\root\source\pkg.xapi.json'; $wrong=Join-Path $root 'test\cases\import_relative_scope\root\pkg.xapi.json'; $out=Join-Path $root 'release\tmp\import_relative_scope.xlir'; foreach($file in @($src,$correct,$wrong)){ if(!(Test-Path -LiteralPath $file)){ exit 10 } }; Push-Location $work; & (Join-Path $root 'release\xl.exe') ir 'source\import_relative_scope.xl' --emit $out; $code=$LASTEXITCODE; Pop-Location; if($code -ne 0){ exit $code }; $text=[System.IO.File]::ReadAllText($out,[System.Text.Encoding]::UTF8); if($text -notmatch 'xl6_import_relative_local_value'){ exit 11 }; if($text -notmatch '__xl6_import_pkg_value'){ exit 12 }; if($text -match 'xl6_import_relative_wrong_value'){ exit 13 }"
if errorlevel 1 (
    echo import_relative_scope resolver check failed.
    exit /b 1
)

release\xl.exe check test\cases\import_lib_search.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_lib_search.xl --emit release\tmp\import_lib_search.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_libprobe_value\" func __xl6_import_libprobe_value" release\tmp\import_lib_search.xlir >nul
if errorlevel 1 (
    echo import_lib_search.xlir missing generated extern.
    exit /b 1
)
findstr /C:"call __xl6_import_libprobe_value" release\tmp\import_lib_search.xlir >nul
if errorlevel 1 (
    echo import_lib_search.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_lib_search.xl --emit release\tmp\import_lib_search.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_libprobe_value" release\tmp\import_lib_search.c >nul
if errorlevel 1 (
    echo import_lib_search.c missing imported C symbol.
    exit /b 1
)

release\xl.exe check test\cases\import_xapi_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_use.xl --emit release\tmp\import_xapi_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_import_counter_add\" func __xl6_import_counter_counterAdd" release\tmp\import_xapi_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_use.xlir missing injected extern func.
    exit /b 1
)
findstr /C:"call __xl6_import_counter_counterAdd" release\tmp\import_xapi_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_use.xlir missing namespace rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_use.xl --emit release\tmp\import_xapi_use.c
if errorlevel 1 exit /b 1
findstr /C:"xl6_import_counter_add" release\tmp\import_xapi_use.c >nul
if errorlevel 1 (
    echo import_xapi_use.c missing imported C symbol.
    exit /b 1
)
release\xl.exe run test\cases\import_xapi_use.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_xapi_use.xl -o release\tmp\import_xapi_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_union_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_union_use.xl --emit release\tmp\import_xapi_type_union_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_union_api_make\" func __xl6_import_union_api_makePayload" release\tmp\import_xapi_type_union_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.xlir missing imported union factory.
    exit /b 1
)
findstr /C:"__xl6_import_union_api_Payload" release\tmp\import_xapi_type_union_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.xlir missing internal imported union type.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_union_use.xl --emit release\tmp\import_xapi_type_union_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Union_____xl6__import__union__api__Payload" release\tmp\import_xapi_type_union_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_use.c missing generated internal union C type.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_union_use.xl -o release\tmp\import_xapi_type_union_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_union_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_union_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\import_xapi_type_union_var_use.xl --emit release\tmp\import_xapi_type_union_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern XL_Union_____xl6__import__union__api__Payload xl6_union_api_default;" release\tmp\import_xapi_type_union_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_var_use.c missing imported union extern variable.
    exit /b 1
)
findstr /C:"xl6_union_api_default.i" release\tmp\import_xapi_type_union_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_union_var_use.c missing imported union variable field access.
    exit /b 1
)
findstr /C:"__xl6_import_union_api_defaultPayload" release\tmp\import_xapi_type_union_var_use.c >nul
if not errorlevel 1 (
    echo import_xapi_type_union_var_use.c leaked internal union variable name.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_union_var_use.xl -o release\tmp\import_xapi_type_union_var_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_union_var_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\import_xapi_type_class_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_class_use.xl --emit release\tmp\import_xapi_type_class_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_class_api_make\" func __xl6_import_class_api_makePoint" release\tmp\import_xapi_type_class_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.xlir missing imported class factory.
    exit /b 1
)
findstr /C:"__xl6_import_class_api_Point" release\tmp\import_xapi_type_class_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.xlir missing internal imported class type.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_class_use.xl --emit release\tmp\import_xapi_type_class_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_____xl6__import__class__api__Point" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing generated internal class C type.
    exit /b 1
)
findstr /C:"long long x;" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing imported class field x.
    exit /b 1
)
findstr /C:"long long y;" release\tmp\import_xapi_type_class_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_use.c missing imported class field y.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_use.xl -o release\tmp\import_xapi_type_class_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe cgen test\cases\import_xapi_type_class_var_use.xl --emit release\tmp\import_xapi_type_class_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern XL_Class_____xl6__import__class__api__Point xl6_class_api_origin;" release\tmp\import_xapi_type_class_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_var_use.c missing imported class extern variable.
    exit /b 1
)
findstr /C:"xl6_class_api_origin.x" release\tmp\import_xapi_type_class_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_var_use.c missing imported class variable field access.
    exit /b 1
)
findstr /C:"__xl6_import_class_api_origin" release\tmp\import_xapi_type_class_var_use.c >nul
if not errorlevel 1 (
    echo import_xapi_type_class_var_use.c leaked internal class variable name.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_var_use.xl -o release\tmp\import_xapi_type_class_var_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_var_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_class_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_class_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe check test\cases\import_xapi_type_class_inherit_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_type_class_inherit_use.xl --emit release\tmp\import_xapi_type_class_inherit_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"class __xl6_import_class_inherit_api_NamedPoint extends __xl6_import_class_inherit_api_Entity" release\tmp\import_xapi_type_class_inherit_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.xlir missing rewritten class base.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_type_class_inherit_use.xl --emit release\tmp\import_xapi_type_class_inherit_use.c
if errorlevel 1 exit /b 1
findstr /C:"XL_Class_____xl6__import__class__inherit__api__Entity base;" release\tmp\import_xapi_type_class_inherit_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.c missing generated class base layout.
    exit /b 1
)
findstr /C:"point.base.id" release\tmp\import_xapi_type_class_inherit_use.c >nul
if errorlevel 1 (
    echo import_xapi_type_class_inherit_use.c missing inherited field access.
    exit /b 1
)
release\xl.exe build test\cases\import_xapi_type_class_inherit_use.xl -o release\tmp\import_xapi_type_class_inherit_use.exe
if errorlevel 1 exit /b 1
release\tmp\import_xapi_type_class_inherit_use.exe
if errorlevel 1 exit /b 1
release\xl.exe check test\cases\import_xapi_type_class_inherit_no_global_pollution.xl >nul 2> release\tmp\import_xapi_type_class_inherit_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_xapi_type_class_inherit_no_global_pollution.xl should fail.
    exit /b 1
)
release\xl.exe build test\cases\import_no_global_pollution.xl -o release\tmp\import_no_global_pollution.exe >nul 2> release\tmp\import_no_global_pollution.err.txt
if not errorlevel 1 (
    echo import_no_global_pollution.xl should fail.
    exit /b 1
)

release\xl.exe run test\cases\import_dependency_private.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_private.xl -o release\tmp\import_dependency_private.exe
if errorlevel 1 exit /b 1
release\tmp\import_dependency_private.exe
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_dependency_versions.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_dependency_versions.xl -o release\tmp\import_dependency_versions.exe
if errorlevel 1 exit /b 1
release\tmp\import_dependency_versions.exe
if errorlevel 1 exit /b 1

powershell -NoProfile -ExecutionPolicy Bypass -Command "$j=Get-Content 'test\fixtures\metadata_combo\package.xapi.json' -Raw | ConvertFrom-Json; if($j.links[0] -ne '../../../release/tmp/metadata_combo_link.a'){exit 1}; if($j.loadlibs[0] -ne '../../../release/tmp/metadata_combo_loadlib.dll'){exit 2}; if($j.cSources[0] -ne 'metadata_combo_native.c'){exit 3}; if(-not $j.native.singleton){exit 4}; if($j.native.loadName -ne 'metadata_combo_runtime.dll'){exit 5}; if($j.native.version -ne '1.0.0'){exit 6}"
if errorlevel 1 (
    echo metadata_combo package.xapi.json missing combined dependency metadata.
    exit /b 1
)

call tools\bootstrap_tcc.bat
if errorlevel 1 exit /b 1

call tools\build_xrt_obj.bat
if errorlevel 1 exit /b 1

"%XL6_TCC%" %XL6_TCC_INCLUDES% -c test\cases\metadata_combo_link_host.c -o release\tmp\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -ar rcs release\tmp\metadata_combo_link.a release\tmp\metadata_combo_link_host.o
if errorlevel 1 exit /b 1
"%XL6_TCC%" -shared %XL6_TCC_INCLUDES% %XL6_TCC_LIB_FLAGS% test\cases\metadata_combo_loadlib_host.c -o release\tmp\metadata_combo_loadlib.dll
if errorlevel 1 exit /b 1
release\xl.exe run test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1
release\xl.exe build test\cases\import_metadata_combo.xl -o release\tmp\import_metadata_combo.exe
if errorlevel 1 exit /b 1
release\tmp\import_metadata_combo.exe
if errorlevel 1 exit /b 1
"%XL6_TCC%" -I. %XL6_TCC_INCLUDES_WINAPI% %XL6_TCC_LIB_FLAGS% test\cases\import_deps_snapshot_host.c src\compile\xl6_imports.c src\package\xl6_package.c src\abi\xl6_xapi_load.c src\abi\xl6_xapi_dump.c src\abi\xl6_callable.c src\type\xl6_type_model.c src\common\xl6_console.c src\common\xl6_output.c src\parser\xl6_parser_api.c src\parser\xl6_parser.c src\lexer\xl6_lexer_api.c src\lexer\xl6_lexer_gen.c src\lexer\xl6_token.c src\ast\xl6_ast.c src\diag\xl6_diag.c %XL6_XRT_OBJ% %XL6_TCC_LIBTCC1% -lWs2_32 -lIPHLPAPI -lShell32 -o release\tmp\import_deps_snapshot_host.exe
if errorlevel 1 exit /b 1
release\tmp\import_deps_snapshot_host.exe test\cases\import_metadata_combo.xl
if errorlevel 1 exit /b 1

release\xl.exe build test\cases\import_dependency_no_pollution.xl -o release\tmp\import_dependency_no_pollution.exe >nul 2> release\tmp\import_dependency_no_pollution.err.txt
if not errorlevel 1 (
    echo import_dependency_no_pollution.xl should fail.
    exit /b 1
)
findstr /C:"private package dependency is not exported" release\tmp\import_dependency_no_pollution.err.txt >nul
if errorlevel 1 (
    echo import_dependency_no_pollution.err.txt missing private dependency diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_dependency_version_mismatch.xl >nul 2> release\tmp\import_dependency_version_mismatch.err.txt
if not errorlevel 1 (
    echo import_dependency_version_mismatch.xl should fail.
    exit /b 1
)
findstr /C:"package dependency version mismatch" release\tmp\import_dependency_version_mismatch.err.txt >nul
if errorlevel 1 (
    echo import_dependency_version_mismatch.err.txt missing version mismatch diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_xapi_var_use.xl
if errorlevel 1 exit /b 1
release\xl.exe ir test\cases\import_xapi_var_use.xl --emit release\tmp\import_xapi_var_use.xlir
if errorlevel 1 exit /b 1
findstr /C:"extern \"xl6_imported_counter\" var __xl6_import_settings_importedCounter : int" release\tmp\import_xapi_var_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_var_use.xlir missing injected extern variable.
    exit /b 1
)
findstr /C:"return __xl6_import_settings_importedCounter" release\tmp\import_xapi_var_use.xlir >nul
if errorlevel 1 (
    echo import_xapi_var_use.xlir missing namespace variable rewrite.
    exit /b 1
)
release\xl.exe cgen test\cases\import_xapi_var_use.xl --emit release\tmp\import_xapi_var_use.c
if errorlevel 1 exit /b 1
findstr /C:"extern int64_t xl6_imported_counter;" release\tmp\import_xapi_var_use.c >nul
if errorlevel 1 (
    echo import_xapi_var_use.c missing imported C variable.
    exit /b 1
)

release\xl.exe check test\cases\import_missing_xapi.xl >nul 2> release\tmp\import_missing_xapi.err.txt
if not errorlevel 1 (
    echo import_missing_xapi.xl should fail.
    exit /b 1
)
findstr /C:"package.xapi.json" release\tmp\import_missing_xapi.err.txt >nul
if errorlevel 1 (
    echo import_missing_xapi.err.txt missing interface file diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_broken_xapi.xl >nul 2> release\tmp\import_broken_xapi.err.txt
if not errorlevel 1 (
    echo import_broken_xapi.xl should fail.
    exit /b 1
)
findstr /C:"xlang-xapi-1" release\tmp\import_broken_xapi.err.txt >nul
if errorlevel 1 (
    echo import_broken_xapi.err.txt missing format diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_invalid_decl_xapi.xl >nul 2> release\tmp\import_invalid_decl_xapi.err.txt
if not errorlevel 1 (
    echo import_invalid_decl_xapi.xl should fail.
    exit /b 1
)
findstr /C:"XAPI import generated invalid extern declarations" release\tmp\import_invalid_decl_xapi.err.txt >nul
if errorlevel 1 (
    echo import_invalid_decl_xapi.err.txt missing generated declaration diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\import_unsupported_xapi_use.xl >nul 2> release\tmp\import_unsupported_xapi_use.err.txt
if not errorlevel 1 (
    echo import_unsupported_xapi_use.xl should fail.
    exit /b 1
)
findstr /C:"XAPI symbol cannot enter C ABI bridge yet: unsupported_xapi.unsupportedCall" release\tmp\import_unsupported_xapi_use.err.txt >nul
if errorlevel 1 (
    echo import_unsupported_xapi_use.err.txt missing unsupported import diagnostic.
    exit /b 1
)
release\xl.exe check test\cases\import_native_singleton_conflict.xl >nul 2> release\tmp\import_native_singleton_conflict.err.txt
if not errorlevel 1 (
    echo import_native_singleton_conflict.xl should fail.
    exit /b 1
)
findstr /C:"native singleton package conflict" release\tmp\import_native_singleton_conflict.err.txt >nul
if errorlevel 1 (
    echo import_native_singleton_conflict.err.txt missing singleton conflict diagnostic.
    exit /b 1
)

release\xl.exe check test\cases\sem_return_outside_func.xl --json > release\tmp\sem_return_outside_func.check.json
if not errorlevel 1 (
    echo sem_return_outside_func.xl should fail in JSON check.
    exit /b 1
)
findstr /C:"false" release\tmp\sem_return_outside_func.check.json >nul
if errorlevel 1 (
    echo sem_return_outside_func.check.json missing failure flag.
    exit /b 1
)
findstr /C:"message" release\tmp\sem_return_outside_func.check.json >nul
if errorlevel 1 (
    echo sem_return_outside_func.check.json missing diagnostic message.
    exit /b 1
)


echo DEMO6 suite ok: import package XAPI
exit /b 0

