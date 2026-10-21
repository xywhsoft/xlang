[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Debug', [switch]$SkipBuild, [switch]$SkipRender, [string]$CompilerPath, [string]$CompletionCompilerPath, [string]$CServerPath, [string]$CInputTraceServerPath, [string]$BridgeCompilerPath, [string]$MappingCompilerPath, [string]$PortablePackagePath, [string]$BuildInputCompilerPath, [string]$TestRoot, [string]$TypeFactsCompilerPath, [string]$PublicTypeCompilerPath, [string]$ClassScalarCompilerPath, [string]$NativeScalarCompilerPath, [switch]$NativeDebugTrace, [string]$CServerMaterialsManifest, [string]$NativeAggregateCompilerPath, [string]$NativeExtensionCompilerPath)
$ErrorActionPreference='Stop'
$ideRoot=Split-Path -Parent $PSScriptRoot
$releaseRoot=Join-Path $ideRoot 'release'
$testRoot=if ($TestRoot) { [IO.Path]::GetFullPath($TestRoot) } else { Join-Path $ideRoot 'build\tests' }
[void](New-Item -ItemType Directory -Path $testRoot -Force)
$projectTestRoot = Join-Path $testRoot ('project-tests-'+[Guid]::NewGuid().ToString('N'))
if (-not $SkipBuild) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Tests
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}
& python -X utf8 -B (Join-Path $PSScriptRoot 'check_lsp_folding.py') --server (Join-Path $releaseRoot 'xlang-ls.exe') --output (Join-Path $testRoot ('resident-folding-'+[Guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE -ne 0) { throw 'Resident native folding, cache and cancellation gate failed.' }
& python -X utf8 -B (Join-Path $PSScriptRoot 'check_lsp_native_completion.py') --server (Join-Path $releaseRoot 'xlang-ls.exe') --output (Join-Path $testRoot ('native-completion-'+[Guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE -ne 0) { throw 'Native semantic completion, source guards and cancellation gate failed.' }
& python -X utf8 -B (Join-Path $PSScriptRoot 'check_lsp_native_analysis.py') --server (Join-Path $releaseRoot 'xlang-ls.exe') --output (Join-Path $testRoot ('native-analysis-'+[Guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE -ne 0) { throw 'Native semantic/IR diagnostics, guarded index and navigation gate failed.' }
if ($NativeExtensionCompilerPath) {
    if (-not $CServerPath) { throw 'Native extension checks require CServerPath.' }
    & python -X utf8 (Join-Path $PSScriptRoot 'check_c_extensions.py') --compiler $NativeExtensionCompilerPath --server $CServerPath --fixture (Join-Path $releaseRoot 'c_extension_test.exe') --output (Join-Path $testRoot 'c-extensions')
    if ($LASTEXITCODE -ne 0) { throw 'Actual extension imports, stdio execution and IDE C context checks failed.' }
}
if ($NativeAggregateCompilerPath) {
    if (-not $CServerPath -or -not $CompilerPath) { throw 'Native aggregate checks require CServerPath and the legacy CompilerPath.' }
    & python -X utf8 (Join-Path $PSScriptRoot 'check_c_native_aggregates.py') --compiler $NativeAggregateCompilerPath --server $CServerPath --fixture (Join-Path $releaseRoot 'c_bridge_test.exe') --legacy $CompilerPath --output (Join-Path $testRoot 'c-native-aggregates')
    if ($LASTEXITCODE -ne 0) { throw 'Actual TCC aggregate layout and IDE C bridge comparison failed.' }
}
if ($NativeScalarCompilerPath) {
    if (-not $CServerPath -or -not $CompilerPath) { throw 'Native C scalar checks require CServerPath and the legacy CompilerPath.' }
    & python (Join-Path $PSScriptRoot 'check_c_native_scalars.py') --compiler $NativeScalarCompilerPath --server $CServerPath --fixture (Join-Path $releaseRoot 'c_bridge_test.exe') --legacy $CompilerPath --output (Join-Path $testRoot 'c-native-scalars')
    if ($LASTEXITCODE -ne 0) { throw 'Actual TCC scalar target and IDE C bridge comparison failed.' }
}
if ($CServerMaterialsManifest) {
    & python -X utf8 (Join-Path $PSScriptRoot 'check_c_server_materials.py') --manifest $CServerMaterialsManifest --output (Join-Path $testRoot 'c-server-materials')
    if ($LASTEXITCODE -ne 0) { throw 'Selected C server material integrity gate failed.' }
}
if ($CInputTraceServerPath) {
    & (Join-Path $releaseRoot 'c_input_trace_test.exe') (Join-Path $testRoot 'c-input-trace') $CInputTraceServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real compiler-observed C input trace gate failed.' }
    & (Join-Path $releaseRoot 'c_input_trace_view_test.exe') (Join-Path $testRoot 'c-input-trace-ui') $CInputTraceServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native compiler-observed C input trace gate failed.' }
    & (Join-Path $releaseRoot 'c_header_context_view_test.exe') (Join-Path $testRoot 'c-header-context-ui') $CInputTraceServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native C navigation project context gate failed.' }
} else { Write-Output 'Real C input trace gates omitted: CInputTraceServerPath was not supplied.' }
if ($ClassScalarCompilerPath) {
    & python (Join-Path $PSScriptRoot 'check_class_scalar_abi.py') --compiler $ClassScalarCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Actual class C scalar defaults, projection and mixed execution gate failed.' }
}
if ($NativeDebugTrace) {
    & python (Join-Path $PSScriptRoot 'check_native_test_debug.py') --output (Join-Path $testRoot ('native-debug-contracts-'+[Guid]::NewGuid().ToString('N')))
    if ($LASTEXITCODE -ne 0) { throw 'Native debugger observation contracts failed.' }
}
if ($TypeFactsCompilerPath) {
    & python (Join-Path $PSScriptRoot 'check_type_facts.py') --compiler $TypeFactsCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real compiler type declaration/reference fact gate failed.' }
    & python (Join-Path $PSScriptRoot 'check_type_navigation.py') --compiler $TypeFactsCompilerPath --type-facts
    if ($LASTEXITCODE -ne 0) { throw 'Extended compiler type navigation compatibility gate failed.' }
    & (Join-Path $releaseRoot 'definition_view_test.exe') (Join-Path $testRoot 'type-facts-ui') $TypeFactsCompilerPath 'type-facts'
    if ($LASTEXITCODE -ne 0) { throw 'Native protocol/base/generic type navigation gate failed.' }
}
if ($PublicTypeCompilerPath) {
    if (-not $CServerPath) { throw 'Public C type navigation gates require CServerPath.' }
    & python (Join-Path $PSScriptRoot 'check_public_type_map.py') --compiler $PublicTypeCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Actual public C type emission and mixed execution gate failed.' }
    & (Join-Path $releaseRoot 'c_bridge_map_test.exe') (Join-Path $testRoot 'public-type-map') $PublicTypeCompilerPath 'public-types'
    if ($LASTEXITCODE -ne 0) { throw 'Public C type source identity validation gate failed.' }
    & (Join-Path $releaseRoot 'c_bridge_definition_view_test.exe') (Join-Path $testRoot 'public-type-definition-ui') $CServerPath $PublicTypeCompilerPath 'public-types'
    if ($LASTEXITCODE -ne 0) { throw 'Native C typedef/tag to xlang nominal type navigation gate failed.' }
} else { Write-Output 'Public C type navigation gates omitted: PublicTypeCompilerPath was not supplied.' }
if ($BridgeCompilerPath) {
    if (-not $CServerPath -or -not $CompilerPath) { throw 'Automatic bridge gates require CServerPath and a legacy CompilerPath.' }
    & (Join-Path $releaseRoot 'c_bridge_test.exe') (Join-Path $testRoot 'c-bridge') $CServerPath $BridgeCompilerPath $CompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real automatic C/xlang bridge backend gate failed.' }
    & (Join-Path $releaseRoot 'c_bridge_view_test.exe') (Join-Path $testRoot 'c-bridge-ui') $CServerPath $BridgeCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native automatic C/xlang bridge gate failed.' }
} else { Write-Output 'Automatic C/xlang bridge gate omitted: BridgeCompilerPath was not supplied.' }
if ($MappingCompilerPath) {
    if (-not $CServerPath) { throw 'Cross-language definition gates require CServerPath.' }
    & (Join-Path $releaseRoot 'c_bridge_map_test.exe') (Join-Path $testRoot 'c-bridge-map') $MappingCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real C declaration source mapping gate failed.' }
    & (Join-Path $releaseRoot 'c_bridge_definition_view_test.exe') (Join-Path $testRoot 'c-bridge-definition-ui') $CServerPath $MappingCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native C to xlang definition navigation gate failed.' }
} else { Write-Output 'C to xlang definition gates omitted: MappingCompilerPath was not supplied.' }
$cEnvironmentArguments=@((Join-Path $testRoot 'c-environment'))
if ($CServerPath) { $cEnvironmentArguments += $CServerPath; if ($CompilerPath) { $cEnvironmentArguments += $CompilerPath } }
& (Join-Path $releaseRoot 'c_environment_test.exe') @cEnvironmentArguments
if ($LASTEXITCODE -ne 0) { throw 'C analysis configuration and forced input gate failed.' }
$cArguments=@((Join-Path $testRoot 'c-language'))
if ($CServerPath) { $cArguments += $CServerPath }
& (Join-Path $releaseRoot 'c_language_test.exe') @cArguments
if ($LASTEXITCODE -ne 0) { throw 'C language session gate failed.' }
if ($CServerPath) {
    & (Join-Path $releaseRoot 'c_draft_test.exe') (Join-Path $testRoot 'c-drafts') $CServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real C immutable draft startup gate failed.' }
    & (Join-Path $releaseRoot 'c_language_view_test.exe') (Join-Path $testRoot 'c-semantic-ui') $CServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native C semantics gate failed.' }
    & (Join-Path $releaseRoot 'c_completion_view_test.exe') (Join-Path $testRoot 'c-completion-ui') $CServerPath
    if ($LASTEXITCODE -ne 0) { throw 'Native C completion gate failed.' }
}
foreach ($test in @('language_test','resource_text_test','workbench_test','catalog_view_test')) {
    & (Join-Path $releaseRoot ($test+'.exe'))
    if ($LASTEXITCODE -ne 0) { throw "$test failed." }
}
& python -X utf8 (Join-Path $PSScriptRoot 'check_startup_cache.py') --fixture (Join-Path $releaseRoot 'startup_cache_test.exe') --output (Join-Path $testRoot 'startup-cache')
if ($LASTEXITCODE -ne 0) { throw 'Startup cache ownership and actual interrupted-parent lease gate failed.' }
foreach ($fixture in @(@('document_sync_test','document-sync'),@('analysis_text_test','analysis-text'),@('file_dialog_test','file-dialogs'),@('dialog_message_test','dialog-message-ui'),@('catalog_source_view_test','catalog-source-ui'),@('startup_stage_test','startup-stage'),@('state_repair_test','state-repairs'),@('global_recovery_view_test','global-recovery-ui'),@('c_input_trace_test','c-input-trace'),@('c_record_layout_test','c-record-layout'),@('c_input_directory_test','c-input-directories'),@('model_test','models'),@('workspace_view_test','workspace-ui'),@('document_test','documents'),@('editor_view_test','editor-ui'),@('process_test','processes'),@('task_view_test','task-ui'),@('task_pending_input_view_test','task-pending-input-ui'),@('standalone_task_test','standalone-tasks'),@('project_test_test','project-tests'),@('project_test_view_test','project-test-ui'),@('configuration_test','configurations'),@('settings_test','settings'),@('catalog_test','catalog'),@('help_document_test','help'),@('help_render_test','help-render'),@('help_viewport_test','help-viewport'),@('state_test','state'),@('session_test','sessions'),@('input_test','input'),@('font_test','fonts'),@('window_test','windows'),@('dpi_test','dpi'),@('ui_font_test','ui-fonts'),@('reading_test','reading'),@('recovery_test','recovery'),@('migration_store_test','migration-store'),@('recovery_backup_test','recovery-backups'),@('recovery_view_test','recovery-ui'),@('file_watch_test','file-watch'),@('file_sync_test','file-sync'),@('file_changes_view_test','file-changes-ui'),@('actions_test','actions'),@('find_view_test','find-ui'),@('find_escape_test','find-escapes'),@('find_options_view_test','find-options-ui'),@('find_boundaries_test','find-boundaries'),@('find_boundary_view_test','find-boundaries-ui'),@('goto_view_test','goto-ui'),@('references_test','references'),@('template_test','templates'),@('template_view_test','template-ui'),@('file_rebind_test','file-rebind'),@('rename_test','rename'),@('tree_rename_test','tree-rename'),@('resource_rebind_test','resource-rename'),@('project_migration_test','project-migration'),@('project_migration_view_test','project-migration-ui'),@('legacy_import_test','legacy-import'),@('import_view_test','legacy-import-ui'),@('resource_view_test','resources-ui'),@('resource_creation_test','resource-create-ui'),@('toolchain_test','toolchains-ui'),@('symbol_view_test','language-symbol-ui'),@('definition_view_test','language-definition-ui'),@('snapshot_view_test','language-snapshot-ui'),@('snapshot_check_test','language-snapshots'),@('lsp_test','language-protocol'),@('language_view_test','language-ui'))) {
    $fixtureArguments=@((Join-Path $testRoot $fixture[1]))
    if ($fixture[0] -in @('task_pending_input_view_test','standalone_task_test')) {
        if (-not $BuildInputCompilerPath) { Write-Output 'Actual pending build input UI gate omitted: BuildInputCompilerPath was not supplied.'; continue }
        $fixtureArguments += $BuildInputCompilerPath
    }
    if ($fixture[0] -eq 'project_test_test') { $fixtureArguments=@((Join-Path $projectTestRoot 'core')) }
    if ($fixture[0] -eq 'project_test_view_test') { $fixtureArguments=@((Join-Path $projectTestRoot 'native')) }
    if ($fixture[0] -in @('snapshot_check_test','snapshot_view_test','definition_view_test','symbol_view_test') -and -not $CompilerPath) { continue }
    if ($fixture[0] -in @('symbol_view_test','definition_view_test','snapshot_view_test','snapshot_check_test','references_test','template_test','file_rebind_test','rename_test','tree_rename_test','project_migration_test','project_migration_view_test','legacy_import_test','import_view_test','resource_view_test','resource_creation_test','project_test_test','toolchain_test','lsp_test','language_view_test') -and $CompilerPath) { $fixtureArguments += $CompilerPath }
    if ($fixture[0] -eq 'resource_rebind_test' -and $CompletionCompilerPath) { $fixtureArguments += $CompletionCompilerPath }
    if ($fixture[0] -in @('catalog_test','help_document_test')) { $fixtureArguments += (Join-Path $releaseRoot 'data\commands.json') }
    if ($NativeDebugTrace -and $fixture[0] -eq 'tree_rename_test') {
        $debugRoot=Join-Path $testRoot ('tree-native-debug-'+[Guid]::NewGuid().ToString('N'))
        $previousTrace=$env:XIDE_TEST_TRACE
        try {
            $env:XIDE_TEST_TRACE='1'
            & python (Join-Path $PSScriptRoot 'native_test_debug.py') --output $debugRoot -- (Join-Path $releaseRoot ($fixture[0]+'.exe')) @fixtureArguments
            $debugExit=$LASTEXITCODE
        } finally { $env:XIDE_TEST_TRACE=$previousTrace }
        if ($debugExit -ne 0) { throw "$($fixture[0]) debug observation failed; native exit and events: $debugRoot\result.json" }
        $nativeExit=$debugExit
    } else {
        & (Join-Path $releaseRoot ($fixture[0]+'.exe')) @fixtureArguments
        $nativeExit=$LASTEXITCODE
    }
    if ($nativeExit -ne 0) { throw "$($fixture[0]) failed with native exit code $nativeExit." }
}
$workdirArguments=@('--fixture',(Join-Path $releaseRoot 'process_workdir_test.exe'),'--output',(Join-Path $testRoot 'process-workdir'))
if ($BuildInputCompilerPath) { $workdirArguments += @('--ide',(Join-Path $releaseRoot 'xlang_ide_console.exe'),'--compiler',$BuildInputCompilerPath) }
& python -X utf8 (Join-Path $PSScriptRoot 'check_process_workdir.py') @workdirArguments
if ($LASTEXITCODE -ne 0) { throw 'Native existing-short-path working directory and genuine no-alias failure gate failed.' }
& python -X utf8 (Join-Path $PSScriptRoot 'check_font_fallback.py') --fixture (Join-Path $releaseRoot 'font_fallback_test.exe') --output (Join-Path $testRoot 'font-fallback')
if ($LASTEXITCODE -ne 0) { throw 'Native offline Chinese font and portable resource gate failed.' }
& python -X utf8 (Join-Path $PSScriptRoot 'check_startup_alias.py') --fixture (Join-Path $releaseRoot 'startup_alias_test.exe') --output (Join-Path $testRoot 'startup-alias')
if ($LASTEXITCODE -ne 0) { throw 'Native automatic long-directory startup alias gate failed.' }
$noAliasArguments=@('--fixture',(Join-Path $releaseRoot 'startup_alias_test.exe'),'--output',(Join-Path $testRoot 'startup-noalias'))
if ($BuildInputCompilerPath) { $noAliasArguments += @('--compiler',$BuildInputCompilerPath) }
& python -X utf8 (Join-Path $PSScriptRoot 'check_startup_noalias.py') @noAliasArguments
if ($LASTEXITCODE -ne 0) { throw 'Native no-usable-short-alias startup gate failed.' }
& python (Join-Path $PSScriptRoot 'check_runtime_failure.py') --fixture (Join-Path $releaseRoot 'runtime_failure_test.exe') --gui (Join-Path $releaseRoot 'xlang_ide.exe') --console (Join-Path $releaseRoot 'xlang_ide_console.exe') --output (Join-Path $testRoot 'runtime-failure')
if ($LASTEXITCODE -ne 0) { throw 'Native GUI error visibility and bounded diagnostics gate failed.' }
& python (Join-Path $PSScriptRoot 'check_frame_failure_recovery.py') --fixture (Join-Path $releaseRoot 'frame_failure_test.exe') --output (Join-Path $testRoot 'frame-failure')
if ($LASTEXITCODE -ne 0) { throw 'Native frame failure checkpoint and separate-process recovery gate failed.' }
& python (Join-Path $PSScriptRoot 'check_rename_checkpoints.py') --fixture (Join-Path $releaseRoot 'rename_checkpoint_test.exe') --output (Join-Path $testRoot 'rename-checkpoints')
if ($LASTEXITCODE -ne 0) { throw 'Native rename checkpoint interruption/recovery gate failed.' }
& python (Join-Path $PSScriptRoot 'check_rename_journal.py') --fixture (Join-Path $releaseRoot 'rename_checkpoint_test.exe') --output (Join-Path $testRoot 'rename-journal')
if ($LASTEXITCODE -ne 0) { throw 'Native filename transaction recovery gate failed.' }
& python (Join-Path $PSScriptRoot 'check_migration_journal.py') --fixture (Join-Path $releaseRoot 'migration_checkpoint_test.exe') --output (Join-Path $testRoot 'migration-journal')
if ($LASTEXITCODE -ne 0) { throw 'Native project migration transaction recovery gate failed.' }
& python (Join-Path $PSScriptRoot 'check_migration_store.py') --fixture (Join-Path $releaseRoot 'migration_checkpoint_test.exe') --output (Join-Path $testRoot 'migration-storage')
if ($LASTEXITCODE -ne 0) { throw 'Native retired migration body maintenance gate failed.' }
& (Join-Path $releaseRoot 'session_test.exe') (Join-Path $testRoot 'deep-session') '--deep'
if ($LASTEXITCODE -ne 0) { throw "Deep Unicode checkpoint recovery failed with native exit code $LASTEXITCODE." }
if ($CompilerPath) {
    & python (Join-Path $PSScriptRoot 'check_language_snapshots.py') --compiler $CompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Multi-document language snapshot protocol gate failed.' }
    & python (Join-Path $PSScriptRoot 'check_language_definitions.py') --compiler $CompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Checked-compilation definition protocol gate failed.' }
    & python (Join-Path $PSScriptRoot 'check_type_navigation.py') --compiler $CompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Checked-compilation type navigation gate failed.' }
    & python (Join-Path $PSScriptRoot 'check_language_symbols.py') --compiler $CompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Checked-compilation outline/hover protocol gate failed.' }
}
if ($CompletionCompilerPath) {
    $legacyCompletionCompiler=if ($CompilerPath) { $CompilerPath } else { Join-Path $releaseRoot 'xide_toolchain_program.exe' }
    & (Join-Path $releaseRoot 'completion_view_test.exe') (Join-Path $testRoot 'language-completion-ui') $CompletionCompilerPath $legacyCompletionCompiler
    if ($LASTEXITCODE -ne 0) { throw 'Native semantic completion gate failed.' }
    & python (Join-Path $PSScriptRoot 'check_language_completions.py') --compiler $CompletionCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Guarded cursor completion protocol gate failed.' }
    & python -X utf8 -B (Join-Path $PSScriptRoot 'check_lsp_incremental.py') --server (Join-Path $releaseRoot 'xlang-ls.exe') --compiler $CompletionCompilerPath --output (Join-Path $testRoot ('lsp-incremental-'+[Guid]::NewGuid().ToString('N')))
    if ($LASTEXITCODE -ne 0) { throw 'Atomic incremental document protocol gate failed.' }
}
$taskArguments=@((Join-Path $testRoot 'tasks'))
if ($CompilerPath) { $taskArguments += $CompilerPath }
& (Join-Path $releaseRoot 'task_test.exe') @taskArguments
if ($LASTEXITCODE -ne 0) { throw 'task_test failed.' }
& (Join-Path $releaseRoot 'task_history_test.exe') (Join-Path $testRoot 'task-history')
if ($LASTEXITCODE -ne 0) { throw 'Native task history retention gate failed.' }
& (Join-Path $releaseRoot 'task_stage_test.exe') (Join-Path $testRoot 'task-stage')
if ($LASTEXITCODE -ne 0) { throw 'Native task stage ownership gate failed.' }
& (Join-Path $releaseRoot 'package_stage_test.exe') (Join-Path $testRoot 'package-stage')
if ($LASTEXITCODE -ne 0) { throw 'Native package path stage protection gate failed.' }
& (Join-Path $releaseRoot 'package_stage_test.exe') (Join-Path $testRoot 'package-drive-stage') '--drive-relative'
if ($LASTEXITCODE -ne 0) { throw 'Native drive-relative package cache gate failed.' }
& python (Join-Path $PSScriptRoot 'check_windows_paths.py') --fixture (Join-Path $releaseRoot 'path_context_test.exe') --root (Join-Path $testRoot 'windows-path-context')
if ($LASTEXITCODE -ne 0) { throw 'Native Windows path context gate failed.' }
& (Join-Path $releaseRoot 'task_path_test.exe') (Join-Path $testRoot 'task-paths')
if ($LASTEXITCODE -ne 0) { throw 'Native task long path I/O gate failed.' }
$resourceInputArguments=@((Join-Path $testRoot ('resource-inputs-'+[Guid]::NewGuid().ToString('N'))))
if ($CompilerPath) { $resourceInputArguments += $CompilerPath }
& (Join-Path $releaseRoot 'resource_inputs_test.exe') @resourceInputArguments
if ($LASTEXITCODE -ne 0) { throw 'Resource asset input graph gate failed.' }
if ($BuildInputCompilerPath) {
    if (-not (Test-Path -LiteralPath $BuildInputCompilerPath -PathType Leaf)) { throw 'BuildInputCompilerPath must name a real compiler executable.' }
    & (Join-Path $releaseRoot 'build_inputs_test.exe') (Join-Path $testRoot 'build-input-receipts') $BuildInputCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real compiler build input receipt task gate failed.' }
    & python -X utf8 (Join-Path $PSScriptRoot 'check_build_targets.py') --ide (Join-Path $releaseRoot 'xlang_ide_console.exe') --compiler $BuildInputCompilerPath --output (Join-Path $testRoot ('build-targets-'+[Guid]::NewGuid().ToString('N')))
    if ($LASTEXITCODE -ne 0) { throw 'Standalone file/project/workspace main-entry gate failed.' }
    & (Join-Path $releaseRoot 'task_path_test.exe') (Join-Path $testRoot 'task-paths-receipts') $BuildInputCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Real C/xlang task long path I/O and receipt gate failed.' }
    & (Join-Path $releaseRoot 'task_test.exe') (Join-Path $testRoot 'tasks-input-receipts') $BuildInputCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Build receipt compiler task compatibility gate failed.' }
    & (Join-Path $releaseRoot 'resource_inputs_test.exe') (Join-Path $testRoot 'resources-input-receipts') $BuildInputCompilerPath
    if ($LASTEXITCODE -ne 0) { throw 'Build receipt compiler resource compatibility gate failed.' }
} else { Write-Output 'Actual build input receipt gates omitted: BuildInputCompilerPath was not supplied.' }
function Test-NativeRender([string[]]$Arguments,[string]$ImagePath,[switch]$ExpectTaskFailure) {
    $started=[DateTime]::UtcNow
    $renderOutput=& (Join-Path $releaseRoot 'xlang_ide_console.exe') --hidden --frames 3 @Arguments --screenshot $ImagePath 2>&1
    $renderExit=$LASTEXITCODE
    $renderOutput | Write-Output
    if ($ExpectTaskFailure) {
        if ($renderExit -ne 1 -or ($renderOutput | Out-String) -notmatch 'state=4 built=0 ran=0') { throw "Expected a failed build without a run: $ImagePath" }
    } elseif ($renderExit -ne 0) { throw "Render failed: $ImagePath" }
    $taskIndex=[Array]::IndexOf($Arguments,'--task')
    if ($taskIndex -ge 0 -and $taskIndex+1 -lt $Arguments.Length -and $Arguments[$taskIndex+1] -eq 'test' -and ($renderOutput | Out-String) -notmatch 'state=3 built=0 ran=0 code=0 tested=1 kind=2') { throw "Configured native test did not execute successfully: $ImagePath" }
    $image=Get-Item -LiteralPath $ImagePath
    if ($image.Length -lt 1000 -or $image.LastWriteTimeUtc -lt $started) { throw "Missing or stale render: $ImagePath" }
}
if (-not $SkipRender) {
    $renderRoot=Join-Path $testRoot 'render'
    [void](New-Item -ItemType Directory -Path $renderRoot -Force)
    foreach ($theme in @('classic-light','graphite-dark','warm-white','ocean')) {
        foreach ($language in @('xlang','c')) {
            Test-NativeRender @('--theme',$theme,'--demo',$language) (Join-Path $renderRoot ($theme+'-'+$language+'.png'))
        }
    }
    foreach($dpi in @(@('1.25','c'),@('2','xlang'))) {
        Test-NativeRender @('--dpi-scale',$dpi[0],'--size','1600','1000','--demo',$dpi[1]) (Join-Path $renderRoot ('dpi-'+$dpi[0]+'-'+$dpi[1]+'.png'))
    }
    $fontSettings=Join-Path $testRoot 'fonts\中文 字体\settings.json'
    foreach ($theme in @('classic-light','graphite-dark')) {
        $fontArguments=@('--theme',$theme,'--settings',$fontSettings,'--font-settings')
        if($theme -eq 'graphite-dark') { $fontArguments += @('--size','900','600') }
        Test-NativeRender $fontArguments (Join-Path $renderRoot ('font-settings-'+$theme+'.png'))
    }
    $uiFontSettings=Join-Path $testRoot 'ui-fonts\中文 界面字体\settings.json'
    foreach($theme in @('classic-light','graphite-dark')) {
        Test-NativeRender @('--theme',$theme,'--size','900','600','--settings',$uiFontSettings,'--ui-font-settings') (Join-Path $renderRoot ('ui-font-'+$theme+'.png'))
    }
    foreach($theme in @('classic-light','graphite-dark')) {
        $recoverySession=Join-Path $testRoot ('manager-session-'+[guid]::NewGuid().ToString('N'))
        Test-NativeRender @('--theme',$theme,'--size','900','600','--session-root',$recoverySession,'--recovery-manager') (Join-Path $renderRoot ('recovery-manager-'+$theme+'.png'))
    }
    $workspaceFixture=Join-Path $testRoot 'workspace-ui\中文 UI 工作空间'
    foreach ($theme in @('classic-light','graphite-dark')) {
        foreach ($source in @('main.xl','helper.c')) {
            Test-NativeRender @('--theme',$theme,'--workspace',(Join-Path $workspaceFixture 'scene.xlw'),'--select-project','beta','--open-file',(Join-Path $workspaceFixture $source)) (Join-Path $renderRoot ('workspace-'+$theme+'-'+$source+'.png'))
        }
    }
    $catalog=Get-Content -LiteralPath (Join-Path $releaseRoot 'data\commands.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $split=($catalog.entries | Where-Object name -eq 'string.split' | Select-Object -First 1).id
    foreach ($theme in @('classic-light','graphite-dark')) {
        Test-NativeRender @('--theme',$theme,'--command-search','string split','--command',$split) (Join-Path $renderRoot ('catalog-'+$theme+'.png'))
    }
    Test-NativeRender @('--theme','classic-light','--command','language:func','--help-chapter','3') (Join-Path $renderRoot 'help-long-classic-light.png')
    Test-NativeRender @('--theme','graphite-dark','--size','900','600','--command','language:func','--help-chapter','3') (Join-Path $renderRoot 'help-long-narrow.png')
    Test-NativeRender @('--catalog',(Join-Path $testRoot 'help\中文 帮助恢复\commands.json')) (Join-Path $renderRoot 'catalog-recovery.png')
    $nativeSession=Join-Path $testRoot ('native-session-'+[guid]::NewGuid().ToString('N'))
    Test-NativeRender @('--session-root',$nativeSession,'--workspace',(Join-Path $workspaceFixture 'scene.xlw'),'--select-project','beta','--open-file',(Join-Path $workspaceFixture 'helper.c'),'--theme','graphite-dark','--command','language:func','--help-chapter','3') (Join-Path $renderRoot 'session-first.png')
    $sessionFile=Get-ChildItem -LiteralPath (Join-Path $nativeSession 'workspaces') -Filter 'session.json' -Recurse | Select-Object -First 1
    if (-not $sessionFile) { throw 'Native session was not saved.' }
    $before=Get-Content -LiteralPath $sessionFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    Test-NativeRender @('--session-root',$nativeSession,'--size','900','600') (Join-Path $renderRoot 'session-restored.png')
    $after=Get-Content -LiteralPath $sessionFile.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $before.reading.help.position.block -or $after.reading.help.position.block.id -ne $before.reading.help.position.block.id -or $after.reading.help.position.chapter.title -ne $before.reading.help.position.chapter.title) { throw 'Native process did not restore the long-help block/chapter across width changes.' }
    if ($after.theme -ne 'graphite-dark' -or $after.activeProject -ne 'beta' -or $after.selectedCommand -ne 'language:func' -or $after.openDocuments.Count -ne 1 -or $after.openDocuments[0].language -ne 'c' -or $after.openDocuments[0].id -ne $before.openDocuments[0].id) { throw 'Native process did not restore the saved C editor/workspace/theme/help.' }
    $configurationFixture=Join-Path $testRoot 'configurations\中文 配置'
    foreach ($theme in @('classic-light','graphite-dark')) {
        Test-NativeRender @('--theme',$theme,'--workspace',(Join-Path $configurationFixture 'sample.xlw'),'--select-project','beta','--edit-configurations') (Join-Path $renderRoot ('configuration-'+$theme+'.png'))
    }
    if ($CompilerPath) {
        foreach ($theme in @('classic-light','graphite-dark')) {
            foreach ($task in @('build','run')) {
                $selection=if ($task -eq 'build') { 'alpha' } else { 'beta' }
                Test-NativeRender @('--theme',$theme,'--workspace',(Join-Path $workspaceFixture 'scene.xlw'),'--select-project',$selection,'--open-file',(Join-Path $workspaceFixture 'main.xl'),'--compiler',$CompilerPath,'--task',$task,'--wait-task') (Join-Path $renderRoot ('task-'+$theme+'-'+$task+'.png'))
            }
        }
        $failedProject=Join-Path $testRoot 'tasks\中文 构建运行\项目.xlp'
        Test-NativeRender @('--theme','classic-light','--project',$failedProject,'--compiler',$CompilerPath,'--task','run','--wait-task') (Join-Path $renderRoot 'task-failed-build.png') -ExpectTaskFailure
        Write-Output 'Native task smoke passed: 4 successful renders and a failed build that did not run the previous artifact.'
        $explicitTestProject=Join-Path $projectTestRoot 'core\迁移后 项目测试\real.xlp'
        foreach ($theme in @('classic-light','graphite-dark','warm-white','ocean')) {
            Test-NativeRender @('--theme',$theme,'--project',$explicitTestProject,'--task','test','--wait-task') (Join-Path $renderRoot ('task-test-'+$theme+'.png'))
        }
        Write-Output 'Native explicit test smoke passed: 4 real C/xlang configured program renders, tested=1 built=0 ran=0.'
    }
    Write-Output "Native render smoke passed: 29 baseline screenshots in $renderRoot"
}
if ($CompilerPath) {
    if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) { throw 'CompilerPath must name a real compiler executable.' }
    foreach ($project in @(@('models\中文 空格工程\app.xlp','model-project'),@('workspace-ui\中文 UI 工作空间\alpha.xlp','mixed-project'))) {
        $artifact=Join-Path $testRoot ($project[1]+'.exe')
        & $CompilerPath build (Join-Path $testRoot $project[0]) -o $artifact
        if ($LASTEXITCODE -ne 0) { throw "Compiler rejected generated manifest: $($project[0])" }
        & $artifact
        if ($LASTEXITCODE -ne 0) { throw "Generated program failed: $artifact" }
    }
    Write-Output 'Compiler integration passed: IDE metadata ignored correctly, real xlang/C mixed project built and ran with Chinese paths.'
}
# Change only an isolated SDK fixture: never patch the pinned production SDK.
$fixtureRoot=Join-Path $testRoot 'sdk-mismatch'
[void](New-Item -ItemType Directory -Path $fixtureRoot -Force)
Copy-Item -Path (Join-Path $ideRoot 'third_party\xge_sdk\*') -Destination $fixtureRoot -Recurse -Force
[IO.File]::AppendAllText((Join-Path $fixtureRoot 'xui.h'),"`n/* simulated mismatched header */`n")
$ErrorActionPreference='Continue'
$negativeOutput=& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify_xge_sdk.ps1') -SdkRoot $fixtureRoot 2>&1 | Out-String
$negativeExit=$LASTEXITCODE
$ErrorActionPreference='Stop'
if ($negativeExit -eq 0 -or $negativeOutput -notmatch 'hash mismatch') { throw 'SDK mismatch was not rejected as expected.' }
Write-Output 'SDK integrity negative test passed: mixed header rejected.'
if ($PortablePackagePath) {
    & python (Join-Path $PSScriptRoot 'check_portable.py') '--package' $PortablePackagePath '--fixture' (Join-Path $releaseRoot 'portable_view_test.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Standalone portable candidate and native relocation gate failed.' }
} else { Write-Output 'Standalone portable gate omitted: PortablePackagePath was not supplied.' }
exit 0
