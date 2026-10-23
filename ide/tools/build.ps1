[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [switch]$Tests, [string]$FrontendRoot='')
$ErrorActionPreference='Stop'
function Get-XideSha256([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    $hasher=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-','').ToLowerInvariant() }
    finally { $hasher.Dispose(); $stream.Dispose() }
}
$ideRoot = Split-Path -Parent $PSScriptRoot
$sdkRoot = Join-Path $ideRoot 'third_party\xge_sdk'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify_xge_sdk.ps1') -SdkRoot $sdkRoot
if ($LASTEXITCODE -ne 0) { throw 'SDK verification failed.' }
$compiler = (Get-Command gcc -ErrorAction Stop).Source
$objectRoot = Join-Path $ideRoot ('build\' + $Configuration)
$releaseRoot = Join-Path $ideRoot 'release'
[void](New-Item -ItemType Directory -Path $objectRoot -Force)
[void](New-Item -ItemType Directory -Path $releaseRoot -Force)
$flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-DXGE_DLL','-DXUI_DLL','-DXGE_XRT_PROFILE_IDE=','-DXGE_DEBUGMODE=0','-finput-charset=UTF-8','-fexec-charset=UTF-8','-I'+(Join-Path $ideRoot 'src'),'-I'+$sdkRoot,'-I'+(Join-Path $sdkRoot 'lib\xrt'))
$sdkManifest = Get-Content -LiteralPath (Join-Path $sdkRoot 'sdk_manifest.json') -Raw | ConvertFrom-Json
if ($sdkManifest.xrtDirectory) { $flags += '-I' + (Join-Path $sdkRoot $sdkManifest.xrtDirectory) }
if ($sdkManifest.configurationHeader) { $flags += @('-include', (Join-Path $sdkRoot $sdkManifest.configurationHeader)) }
if ($Configuration -eq 'Release') { $flags += '-O2' } else { $flags += @('-O0','-g') }
$sources = @('src\app\xide_app.c','src\app\actions.c','src\ui\workbench.c','src\platform\xide_input.c','src\lang\languages.c','src\theme\theme.c','src\platform\path.c','src\model\workspace.c','src\model\resources.c','src\model\resource_text.c','src\services\resource_creation.c','src\services\resource_inputs.c','src\services\resource_rebind.c','src\ui\resource_view.c','src\ui\workspace_view.c','src\platform\text_file.c','src\platform\file_rename.c','src\editor\documents.c','src\ui\editor_view.c','src\ui\goto_view.c','src\platform\process.c','src\services\tasks.c','src\services\toolchain.c','src\protocol\lsp_wire.c','src\services\language_check.c','src\services\language_snapshot.c','src\services\semantic_definition.c','src\services\semantic_completion.c','src\services\language_client.c','src\services\c_commands.c','src\services\c_language.c','src\services\c_completion_job.c','src\services\c_input_trace.c','tools\clangd_input_trace\NativeInput.c','src\services\c_bridge.c','src\services\c_bridge_map.c','src\ui\language_view.c','src\ui\completion_view.c','src\ui\c_completion_view.c','src\ui\c_analysis_inputs.c','src\ui\c_language_view.c','src\ui\toolchain_view.c','src\services\project_templates.c','src\services\file_rebind.c','src\services\rename_journal.c','src\services\project_migration.c','src\services\legacy_import.c','src\ui\task_view.c','src\ui\configuration_view.c','src\ui\references_view.c','src\ui\template_view.c','src\ui\import_view.c','src\ui\rename_view.c','src\platform\settings.c','src\services\command_catalog.c','src\services\library_info.c','src\ui\catalog_view.c','src\ui\help_document.c','src\platform\state_file.c','src\platform\state_repair.c','src\editor\coordinates.c','src\ui\layout_state.c','src\services\session.c','src\services\editor_font.c','src\ui\font_view.c','src\platform\window_state.c','src\platform\dpi_proxy.c','src\services\ui_font.c','src\ui\reading_state.c','src\platform\recovery_store.c','src\platform\recovery_backup.c','src\services\recovery.c','src\ui\recovery_view.c','src\platform\file_watch.c','src\services\file_sync.c','src\ui\file_changes_view.c')
$sources += 'src\editor\lsp_coordinates.c'
$sources += 'src\platform\text_utf.c'
$sources += 'src\services\highlight_result.c'
$sources += 'src\services\completion_result.c'
$sources += 'src\services\sync_plan.c'
$sources += 'src\services\diagnostic_dependencies.c'
$sources += 'src\services\diagnostic_notice.c'
$sources += 'src\services\diagnostic_stream.c'
$sources += 'src\platform\startup_alias.c'
$sources += 'src\platform\startup_stage.c'
$sources += 'src\platform\startup_cache.c'
$objects = @()
$sources += 'src\services\build_inputs.c'
$sources += 'src\platform\file_io.c'
$sources += 'src\platform\task_stage.c'
$sources += 'src\platform\runtime_failure.c'
$sources += 'src\services\catalog_sources.c'
$sources += 'src\ui\dialog.c'
$sources += 'src\ui\message_box.c'
$sources += 'src\ui\file_dialog.c'
$sources += 'src\platform\failure_window.c'
$sources += 'src\analysis\text.c'
$sources += 'src\analysis\context.c'
$sources += 'src\services\analysis_context.c'
$sources += 'src\protocol\document_sync.c'
$sources += 'src\protocol\input_state.c'
$sources += 'src\protocol\diagnostic_paths.c'
foreach ($source in $sources) {
    $object = Join-Path $objectRoot (($source.Replace('\','_')) + '.o')
    & $compiler @flags -c (Join-Path $ideRoot $source) -o $object
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $object
}
$libraries = @((Join-Path $sdkRoot 'xge.lib'),'-lm','-lws2_32','-lwinhttp','-liphlpapi','-lgdi32','-luser32','-lshell32','-lopengl32','-lole32','-lwinmm','-lavrt','-lversion','-lbcrypt','-ladvapi32')
if (-not $FrontendRoot) {
    $portableInputs=Join-Path $ideRoot 'data\portable-build-inputs.json'
    if (Test-Path -LiteralPath $portableInputs) {
        $portableConfig=Get-Content -LiteralPath $portableInputs -Raw | ConvertFrom-Json
        if ($portableConfig.compiler_root) { $candidateFrontend=Split-Path -Parent (Join-Path $ideRoot $portableConfig.compiler_root); if (Test-Path -LiteralPath (Join-Path $candidateFrontend 'src\parser\xl6_parser.c')) { $FrontendRoot=$candidateFrontend } }
    }
    if (-not $FrontendRoot) { $FrontendRoot=Join-Path (Split-Path -Parent $ideRoot) 'demo6' }
}
$nativeRoot=Join-Path $objectRoot ('lsp-'+[Guid]::NewGuid().ToString('N'))
$nativeArguments=@('-X','utf8','-B',(Join-Path $PSScriptRoot 'build_lsp_core.py'),'--server','--frontend-root',$FrontendRoot,'--output',$nativeRoot,'--cc',$compiler,'--configuration',$Configuration)
if (-not $Tests) { $nativeArguments+='--skip-tests' }
& python @nativeArguments
if ($LASTEXITCODE -ne 0) { throw 'Standalone language server/core build failed.' }
Copy-Item -LiteralPath (Join-Path $nativeRoot 'xlang-ls.exe') -Destination (Join-Path $releaseRoot 'xlang-ls.exe') -Force
$nativeReport=Join-Path $nativeRoot 'build-result.json'
[pscustomobject]@{report=$nativeReport;reportSha256=(Get-XideSha256 $nativeReport);frontendRoot=[IO.Path]::GetFullPath($FrontendRoot)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $objectRoot 'language-server-build.json') -Encoding utf8
$consoleExe = Join-Path $releaseRoot 'xlang_ide_console.exe'
& $compiler @flags (Join-Path $ideRoot 'src\main.c') @objects @libraries -o $consoleExe
if ($LASTEXITCODE -ne 0) { throw 'Console link failed.' }
$guiStaging=Join-Path $objectRoot 'xlang_ide.link.exe'
& $compiler @flags '-DXIDE_GUI=1' -mwindows (Join-Path $ideRoot 'src\main.c') @objects @libraries -o $guiStaging
if ($LASTEXITCODE -ne 0) { throw 'GUI link failed.' }
$guiExe=Join-Path $releaseRoot 'xlang_ide.exe'
if (Test-Path -LiteralPath $guiExe) { [IO.File]::Replace($guiStaging,$guiExe,(Join-Path $objectRoot 'xlang_ide.previous.exe')) } else { [IO.File]::Move($guiStaging,$guiExe) }
$sdkDll=Join-Path $sdkRoot 'xge.dll'
$releaseDll=Join-Path $releaseRoot 'xge.dll'
if (-not (Test-Path -LiteralPath $releaseDll) -or (Get-XideSha256 $sdkDll) -ne (Get-XideSha256 $releaseDll)) {
    Copy-Item -LiteralPath $sdkDll -Destination $releaseDll -Force
}
[void](New-Item -ItemType Directory -Path (Join-Path $releaseRoot 'data') -Force)
Copy-Item -LiteralPath (Join-Path $ideRoot 'data\commands.json') -Destination (Join-Path $releaseRoot 'data\commands.json') -Force
& python -X utf8 (Join-Path $PSScriptRoot 'catalog_sources.py') --source (Join-Path $ideRoot 'data\catalog-sources') --catalog (Join-Path $ideRoot 'data\commands.json') --destination (Join-Path $releaseRoot 'data\catalog-sources')
if ($LASTEXITCODE -ne 0) { throw 'Catalog source verification failed.' }
& python -X utf8 (Join-Path $PSScriptRoot 'font_resources.py') --source (Join-Path $ideRoot 'data\fonts') --destination (Join-Path $releaseRoot 'data\fonts')
if ($LASTEXITCODE -ne 0) { throw 'Pinned offline font resource verification failed.' }
if ($Tests) {
    foreach ($fixture in @('diagnostic_dependencies_test','diagnostic_notice_test')) {
        & $compiler @flags (Join-Path $ideRoot ('tests\'+$fixture+'.c')) @objects @libraries -o (Join-Path $releaseRoot ($fixture+'.exe'))
        if ($LASTEXITCODE -ne 0) { throw "$fixture link failed." }
    }
    # The stream fixture owns the implementation for deterministic worker gates.
    $diagnosticStreamObjects = $objects | Where-Object { $_ -notmatch 'src_services_diagnostic_stream\.c\.o$' }
    & $compiler @flags (Join-Path $ideRoot 'tests\diagnostic_stream_test.c') @diagnosticStreamObjects @libraries -o (Join-Path $releaseRoot 'diagnostic_stream_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic background stream fixture link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\diagnostic_paths_test.c') @objects @libraries -o (Join-Path $releaseRoot 'diagnostic_paths_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic dependency digest fixture link failed.' }
    # This boundary fixture owns the real snapshot implementation.
    $reportObjects = $objects | Where-Object { $_ -notmatch 'src_services_language_snapshot\.c\.o$' }
    & $compiler @flags (Join-Path $ideRoot 'tests\snapshot_report_test.c') @reportObjects @libraries -o (Join-Path $releaseRoot 'snapshot_report_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Snapshot report boundary fixture link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\build_inputs_test.c') @objects @libraries -o (Join-Path $releaseRoot 'build_inputs_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Build input receipt test link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\task_path_test.c') @objects @libraries -o (Join-Path $releaseRoot 'task_path_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Task long path I/O test link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\task_history_test.c') @objects @libraries '-lpsapi' -o (Join-Path $releaseRoot 'task_history_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Task history retention test link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\task_stage_test.c') @objects @libraries -o (Join-Path $releaseRoot 'task_stage_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Task stage ownership test link failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\package_stage_test.c') @objects @libraries -o (Join-Path $releaseRoot 'package_stage_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Package path stage protection test link failed.' }
    & $compiler '-std=c11' '-Wall' '-Wextra' '-Werror' '-O2' (Join-Path $ideRoot 'tests\task_program.c') '-lshell32' -o (Join-Path $releaseRoot 'xide_task_program.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Task program fixture compile failed.' }
    $resourceCompiler = (Get-Command windres -ErrorAction Stop).Source
    $toolchainResource = Join-Path $objectRoot 'toolchain_program.res.o'
    & $resourceCompiler (Join-Path $ideRoot 'tests\toolchain_program.rc') '-O' 'coff' '-o' $toolchainResource
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain version resource fixture failed.' }
    & $compiler '-std=c11' '-Wall' '-Wextra' '-Werror' '-O2' (Join-Path $ideRoot 'tests\toolchain_program.c') $toolchainResource '-lshell32' '-o' (Join-Path $releaseRoot 'xide_toolchain_program.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Toolchain fixture compile failed.' }
    & $compiler '-std=c11' '-Wall' '-Wextra' '-Werror' '-O2' (Join-Path $ideRoot 'tests\language_program.c') '-lshell32' -o (Join-Path $releaseRoot 'xide_language_program.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Language protocol fixture compile failed.' }
    & $compiler @flags (Join-Path $ideRoot 'tests\c_language_program.c') @libraries -o (Join-Path $releaseRoot 'c_language_program.exe')
    if ($LASTEXITCODE -ne 0) { throw 'C transport fixture compile failed.' }
    foreach ($test in @('analysis_context_test','analysis_project_context_test','semantic_lookup_test','language_refresh_test','automatic_diagnostics_view_test','completion_frame_test','lsp_receive_test','completion_result_test','sync_view_test','sync_plan_test','highlight_view_test','sdk_background_index_test','sdk_background_layout_test','sdk_background_view_test','highlight_result_test','editor_lsp_coordinates_test','document_sync_test','analysis_text_test','standalone_task_test','file_dialog_test','dialog_message_test','catalog_source_view_test','path_context_test','state_repair_test','global_recovery_view_test','c_input_trace_test','c_record_layout_test','c_extension_test','c_input_directory_test','c_input_trace_view_test','c_header_context_view_test','find_view_test','find_escape_test','find_options_view_test','find_boundaries_test','find_boundary_view_test','goto_view_test','c_draft_test','portable_view_test','c_bridge_map_test','c_bridge_definition_view_test','c_bridge_test','c_bridge_view_test','c_environment_test','c_language_test','c_language_view_test','c_completion_view_test','completion_view_test','symbol_view_test','definition_view_test','snapshot_view_test','snapshot_check_test','lsp_test','language_view_test','toolchain_test','language_test','workbench_test','model_test','workspace_view_test','document_test','editor_view_test','process_test','process_workdir_test','task_test','project_test_test','project_test_view_test','task_view_test','task_pending_input_view_test','configuration_test','settings_test','catalog_test','catalog_view_test','help_document_test','state_test','session_test','input_test','font_test','window_test','dpi_test','ui_font_test','reading_test','recovery_test','recovery_backup_test','recovery_view_test','file_watch_test','file_sync_test','file_changes_view_test','actions_test','references_test','template_test','template_view_test','file_rebind_test','rename_test','tree_rename_test','project_migration_test','project_migration_view_test','legacy_import_test','import_view_test','resource_view_test','resource_creation_test','resource_inputs_test','resource_text_test','resource_rebind_test','rename_checkpoint_test','migration_checkpoint_test','migration_store_test','help_render_test','help_viewport_test','frame_failure_test','runtime_failure_test','font_fallback_test','startup_alias_test','startup_stage_test','startup_cache_test')) {
        $testObjects = $objects
        if ($test -eq 'analysis_context_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_analysis_context\.c\.o$' } }
        # The annotation oracle owns a local module and must never pass its handles to GUI controls.
        if ($test -eq 'lsp_receive_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_language_client\.c\.o$' } }
        if ($test -eq 'semantic_lookup_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_semantic_definition\.c\.o$' } }
        if ($test -eq 'completion_result_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_completion_result\.c\.o$' } }
        if ($test -eq 'sync_plan_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_(language_client|sync_plan)\.c\.o$' } }
        if ($test -eq 'sdk_background_index_test') { $testObjects = @() }
        if ($test -eq 'font_fallback_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_editor_font\.c\.o$' } }
        if ($test -in @('actions_test','help_viewport_test','frame_failure_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_app_xide_app\.c\.o$' } }
        if ($test -in @('rename_checkpoint_test','migration_checkpoint_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_rename_view\.c\.o$' } }
        if ($test -eq 'resource_rebind_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_resource_rebind\.c\.o$' } }
        if ($test -eq 'state_repair_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_platform_state_repair\.c\.o$' } }
        # The native snapshot test compiles the real view to replay malformed responses.
        if ($test -in @('automatic_diagnostics_view_test','snapshot_view_test','definition_view_test','symbol_view_test','highlight_view_test','sync_view_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_language_view\.c\.o$' } }
        if ($test -eq 'c_record_layout_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_c_bridge\.c\.o$' } }
        if ($test -in @('c_bridge_test','c_extension_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_c_completion_job\.c\.o$' } }
        if ($test -eq 'c_bridge_view_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_c_(language|completion)_view\.c\.o$' } }
        if ($test -in @('c_language_view_test','c_input_trace_view_test','c_header_context_view_test','c_bridge_definition_view_test','portable_view_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_c_language_view\.c\.o$' } }
        if ($test -eq 'c_completion_view_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_c_completion_view\.c\.o$' } }
        if ($test -in @('c_language_test','c_draft_test')) { $testObjects = $objects | Where-Object { $_ -notmatch 'src_services_c_language\.c\.o$' } }
        if ($test -eq 'language_refresh_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_lang_languages\.c\.o$' } }
        if ($test -eq 'completion_frame_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_completion_view\.c\.o$' } }
        if ($test -eq 'completion_view_test') { $testObjects = $objects | Where-Object { $_ -notmatch 'src_ui_completion_view\.c\.o$' } }
        if ($test -eq 'model_test') { $testObjects = $objects | Where-Object { $_ -match 'src_(platform_path|platform_file_io|model_workspace)\.c\.o$' } }
        & $compiler @flags (Join-Path $ideRoot ('tests\'+$test+'.c')) @testObjects @libraries -o (Join-Path $releaseRoot ($test+'.exe'))
        if ($LASTEXITCODE -ne 0) { throw "Test link failed: $test" }
    }
}
Write-Output "Built: $(Join-Path $releaseRoot 'xlang_ide.exe') ($Configuration)"
