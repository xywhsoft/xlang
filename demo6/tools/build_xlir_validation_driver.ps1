param([string]$Driver = 'test/cases/xlir_owner_consumption_contract.c', [string]$Output = 'build/test/xlir_validation_driver.exe',
    [string[]]$BatchDrivers = @(), [string]$BatchDirectory = '', [switch]$NoCache)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_c_object_cache.ps1')
New-Item -ItemType Directory -Force -Path build/test/__xl_runtime | Out-Null
foreach ($header in @('xl6_ownership_scope.h','xl6_ownership_plan.h','xl6_code_owner.h','xl6_code_lease.h','xl6_closure.h','xl6_cell.h','xl6_slot.h','xl6_frame_storage.h','xl6_task.h','xl6_capture.h','xl6_value_iterator.h','xl6_future_callback.h','xl6_future_aggregate.h','xl6_generator.h','xl6_continuation_queue.h','xl6_continuation_executor.h','xl6_continuation_future.h','xl6_future_payload.h','xl6_network_service.h')) {
    Copy-Item -LiteralPath "src/runtime/$header" -Destination "build/test/__xl_runtime/$header" -Force
}
$compilerSources = @('src/ast/xl6_ast.c','src/type/xl6_type_model.c',
    'src/ir/xl6_hir_type.c','src/ir/xl6_hir.c','src/ir/xl6_ir.c','src/ir/xl6_ir_lower.c',
    'src/cgen/xl6_ir_cgen.c','src/cgen/xl6_cgen_shared_runtime.c',
    'src/diag/xl6_diag.c','src/common/xl6_output.c','src/common/xl6_console.c',
    'src/lexer/xl6_token.c','src/lexer/xl6_lexer_api.c','src/lexer/xl6_lexer_gen.c',
    'src/parser/xl6_parser.c','src/parser/xl6_parser_api.c',
    'src/sem/xl6_sem.c','src/sem/xl6_protocol.c','src/sem/xl6_type_registry.c',
    'src/sem/xl6_sem_docs.c','src/sem/xl6_sem_field_attrs.c',
    'src/stdlib/xl6_stdlib.c','src/stdlib/xl6_stdlib_data.c')
$strict = @('-std=c11','-Wall','-Wextra','-Werror','-O0','-Wno-unused-function','-Wno-unused-label',
    '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable',
    '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign',
    '-Wno-format-truncation','-I.','-Ibuild/test')
if ($BatchDrivers.Count) {
    if (!$BatchDirectory) { throw 'BatchDirectory is required for a shared-source contract build' }
    $rootPath = (Get-Location).Path
    $batchPath = [IO.Path]::GetFullPath((Join-Path $rootPath $BatchDirectory))
    if (Test-Path -LiteralPath $batchPath) { throw 'Choose a fresh contract build directory; prior evidence must not be overwritten' }
    $buildTimer = [Diagnostics.Stopwatch]::StartNew()
    $buildContext = New-XlirCBuildContext -Root $rootPath -Directory $batchPath -NoCache:$NoCache
    $inputFiles = @(@(Get-ChildItem src,lib -Recurse -File) + @(Get-ChildItem test/cases -File -Filter '*.h') +
        @(Get-Item -LiteralPath $BatchDrivers) + @(Get-Item -LiteralPath $PSCommandPath,tools/xlir_c_object_cache.ps1,release/xl.dll) +
        @(Get-Item -LiteralPath $buildContext.Toolchain.Path)) | Sort-Object FullName -Unique
    $inputRecords = @($inputFiles | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    New-Item -ItemType Directory -Force -Path $batchPath | Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $batchPath 'xl.dll') -Force
    $objects = @($compilerSources | ForEach-Object { Join-Path $batchPath ([IO.Path]::GetFileNameWithoutExtension($_) + '.o') })
    if (@($objects | Select-Object -Unique).Count -ne $objects.Count) { throw 'Compiler object basename collision' }
    $objectRecords = @()
    for ($index = 0; $index -lt $compilerSources.Count; ++$index) {
        $record = Invoke-XlirCObject -Context $buildContext -Source (Join-Path $rootPath $compilerSources[$index]) -Object $objects[$index] -Flags $strict
        $objectRecords += $record
        Write-Output "[$($index + 1)/$($compilerSources.Count)] $(if ($record.CacheHit) {'reuse'} else {'compile'}) $($compilerSources[$index]) ($($record.TotalSeconds)s)"
    }
    $failed = @()
    foreach ($batchDriver in $BatchDrivers) {
        $target = Join-Path $batchPath ([IO.Path]::GetFileNameWithoutExtension($batchDriver) + '.exe')
        & $buildContext.Compiler @strict $batchDriver @objects release/xl.dll -o $target
        if ($LASTEXITCODE -ne 0) { $failed += $batchDriver }
    }
    if ($failed.Count) { throw "Contract driver links failed: $($failed -join ', ')" }
    if (@($inputRecords | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count) {
        throw 'Compiler inputs changed during shared build'
    }
    Assert-XlirCToolchainUnchanged $buildContext
    $driverRecords = @($BatchDrivers | ForEach-Object {
        $path = Join-Path $batchPath ([IO.Path]::GetFileNameWithoutExtension($_) + '.exe')
        [pscustomobject]@{Path=$path;Sha256=(Get-FileHash -LiteralPath $path).Hash}
    })
    $manifest = [pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputRecords;Drivers=$driverRecords;
        RuntimeSha256=(Get-FileHash release/xl.dll).Hash;CompilerObjects=$objects.Count;
        Objects=$objectRecords;ToolchainSha256=$buildContext.ToolchainSha256;
        CacheHits=@($objectRecords | Where-Object CacheHit).Count;CacheMisses=@($objectRecords | Where-Object {!$_.CacheHit}).Count;
        BuildSeconds=[Math]::Round($buildTimer.Elapsed.TotalSeconds,3);
        Scope='Shared source compilation; preprocessed content, flags and toolchain keyed object reuse; fresh links and before/after input validation.'}
    [IO.File]::WriteAllText((Join-Path $batchPath 'build.json'), ($manifest | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
    exit 0
}
& gcc @strict $Driver @compilerSources release/xl.dll -o $Output
exit $LASTEXITCODE
