param(
    [string]$Root = '',
    [string]$NativeRoot = '',
    [string]$Tcc = '',
    [string]$OutputRoot = '',
    [ValidateRange(1, 8)][int]$MaxParallel = 3,
    [switch]$ListOnly
)

# Parameterized XLIR checks are scenario workers, not standalone commands.  This
# runner owns their shared source builds, exact driver inputs, dependency phases
# and immutable reports.  Adding a worker fails the reviewed inventory until its
# argument and resource contract is added here.
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
$toolsDirectory = Join-Path $Root 'tools'

$reviewedGuards = @(
    'check_xlir_aggregate_dependencies.ps1',
    'check_xlir_aggregate_strict_source.ps1',
    'check_xlir_artifact_runtime.ps1',
    'check_xlir_artifact_staging.ps1',
    'check_xlir_async_cancel_wait.ps1',
    'check_xlir_async_dependency_cycles.ps1',
    'check_xlir_async_dispatch.ps1',
    'check_xlir_async_module_lifetime.ps1',
    'check_xlir_async_module_tcc.ps1',
    'check_xlir_async_storage.ps1',
    'check_xlir_await_status.ps1',
    'check_xlir_callable_admission.ps1',
    'check_xlir_callable_contextual.ps1',
    'check_xlir_c_owned_boundary_fault.ps1',
    'check_xlir_c_owned_exports.ps1',
    'check_xlir_c_owned_globals_fault.ps1',
    'check_xlir_capture_ownership.ps1',
    'check_xlir_cell_storage.ps1',
    'check_xlir_class_callable_cycles.ps1',
    'check_xlir_class_copy_storage.ps1',
    'check_xlir_class_storage.ps1',
    'check_xlir_code_finalizer.ps1',
    'check_xlir_code_publication.ps1',
    'check_xlir_continuation_scopes.ps1',
    'check_xlir_default_admission.ps1',
    'check_xlir_default_backend_boundary.ps1',
    'check_xlir_executor_ownership.ps1',
    'check_xlir_foreign_prepare_result.ps1',
    'check_xlir_frame_ownership.ps1',
    'check_xlir_future_aggregate_code_owner.ps1',
    'check_xlir_future_aggregate_ownership.ps1',
    'check_xlir_future_callback_storage.ps1',
    'check_xlir_future_ownership.ps1',
    'check_xlir_future_retirement.ps1',
    'check_xlir_future_wait_oracle.ps1',
    'check_xlir_generator_ownership.ps1',
    'check_xlir_generator_public_source.ps1',
    'check_xlir_generator_storage.ps1',
    'check_xlir_http_async_source.ps1',
    'check_xlir_member_call_diagnostics.ps1',
    'check_xlir_mixed_dependencies.ps1',
    'check_xlir_module_binding_regression.ps1',
    'check_xlir_module_cycles.ps1',
    'check_xlir_module_executor.ps1',
    'check_xlir_module_ownership.ps1',
    'check_xlir_module_prepare.ps1',
    'check_xlir_module_retirement_v4.ps1',
    'check_xlir_native_adapters.ps1',
    'check_xlir_native_future_map.ps1',
    'check_xlir_native_linkage.ps1',
    'check_xlir_native_process.ps1',
    'check_xlir_nullable_getter.ps1',
    'check_xlir_safe_index.ps1',
    'check_xlir_future_cancel_core.ps1',
    'check_xlir_network_lifetime.ps1',
    'check_xlir_network_service_runtime.ps1',
    'check_xlir_object_escape.ps1',
    'check_xlir_one_shot_lifetime.ps1',
    'check_xlir_one_shot_tcc.ps1',
    'check_xlir_ownership_plans.ps1',
    'check_xlir_pending_module_preparation.ps1',
    'check_xlir_public_artifact_batch.ps1',
    'check_xlir_public_interop_batch.ps1',
    'check_xlir_public_runtime_batch.ps1',
    'check_xlir_public_source_batch.ps1',
    'check_xlir_resident_closure.ps1',
    'check_xlir_resident_ownership_scopes.ps1',
    'check_xlir_regex_values.ps1',
    'check_xlir_slot_transfers.ps1',
    'check_xlir_string_join.ps1',
    'check_xlir_task_pool_graph.ps1',
    'check_xlir_task_storage.ps1',
    'check_xlir_terminal_source_runtime.ps1',
    'check_xlir_xcall_expression.ps1'
)

function Get-RequiredParameters([string]$Path) {
    $tokens = $null
    $parseErrors = $null
    $ast = [Management.Automation.Language.Parser]::ParseFile(
        $Path, [ref]$tokens, [ref]$parseErrors)
    if ($parseErrors.Count) { throw "Cannot parse XLIR worker: $Path ($($parseErrors[0].Message))" }
    $required = @()
    foreach ($parameter in $ast.ParamBlock.Parameters) {
        foreach ($attribute in $parameter.Attributes) {
            foreach ($argument in $attribute.NamedArguments) {
                if ($argument.ArgumentName -eq 'Mandatory' -and
                    ($null -eq $argument.Argument -or $argument.Argument.Extent.Text -match '^\$true$')) {
                    $required += $parameter.Name.VariablePath.UserPath
                }
            }
        }
    }
    return @($required | Sort-Object -Unique)
}

$actualGuards = @(Get-ChildItem -LiteralPath $toolsDirectory -Filter 'check_xlir_*.ps1' -File |
    Where-Object { @(Get-RequiredParameters $_.FullName).Count -ne 0 } |
    ForEach-Object Name | Sort-Object)
$reviewedGuards = @($reviewedGuards | Sort-Object)
$missing = @($actualGuards | Where-Object { $_ -notin $reviewedGuards })
$stale = @($reviewedGuards | Where-Object { $_ -notin $actualGuards })
if ($missing.Count -or $stale.Count) {
    throw "Parameterized XLIR inventory changed; missing review: $($missing -join ', '); stale entries: $($stale -join ', ')"
}
if ($ListOnly) { $reviewedGuards; exit 0 }

if (!$NativeRoot) {
    $candidate = Join-Path (Split-Path -Parent $Root) 'xrt'
    if (Test-Path -LiteralPath $candidate -PathType Container) { $NativeRoot = $candidate }
}
if (!$NativeRoot) { throw 'Native XRT source root is required; pass -NativeRoot or place xrt beside x-lang.' }
$NativeRoot = (Resolve-Path -LiteralPath $NativeRoot).Path
foreach ($relative in @('config/modules.json', 'extlibs/xruntime/config/modules.json')) {
    if (!(Test-Path -LiteralPath (Join-Path $NativeRoot $relative) -PathType Leaf)) {
        throw "Native XRT source root is incomplete: $relative"
    }
}
if (!$Tcc) {
    if ($env:XL6_TCC -and (Test-Path -LiteralPath $env:XL6_TCC -PathType Leaf)) { $Tcc = $env:XL6_TCC }
    elseif (Test-Path -LiteralPath 'E:/software/tcc/tcc.exe' -PathType Leaf) { $Tcc = 'E:/software/tcc/tcc.exe' }
    elseif (Test-Path -LiteralPath (Join-Path $Root 'build/tmp/tcc_bootstrap.exe') -PathType Leaf) {
        $Tcc = Join-Path $Root 'build/tmp/tcc_bootstrap.exe'
    }
    else { throw 'A standalone TinyCC executable is required; pass -Tcc or set XL6_TCC.' }
}
$Tcc = (Resolve-Path -LiteralPath $Tcc).Path

$runId = (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
if (!$OutputRoot) { $OutputRoot = "build/tmp/xlir_parameterized_$runId" }
if ([IO.Path]::IsPathRooted($OutputRoot)) { $outputPath = [IO.Path]::GetFullPath($OutputRoot) }
else { $outputPath = [IO.Path]::GetFullPath((Join-Path $Root $OutputRoot)) }
if (Test-Path -LiteralPath $outputPath) { throw "Refusing to reuse XLIR scenario evidence: $outputPath" }
[IO.Directory]::CreateDirectory($outputPath) | Out-Null

$publicDirectory = [IO.Path]::GetFullPath((Join-Path $Root "build/test/xlir_parameterized_${runId}_public"))
$runtimeDirectory = [IO.Path]::GetFullPath((Join-Path $Root "build/test/xlir_parameterized_${runId}_runtime"))
$contractDirectory = [IO.Path]::GetFullPath((Join-Path $Root "build/test/xlir_parameterized_${runId}_contracts"))
$phaseOneDirectory = Join-Path $OutputRoot 'phase1'
$phaseTwoDirectory = Join-Path $OutputRoot 'phase2'
$contractSources = @(
    'test/cases/xlir_c_owned_contract.c',
    'test/cases/xlir_async_cancel_watch_contract.c',
    'test/cases/xlir_async_dependency_cycle_contract.c',
    'test/cases/xlir_async_storage_contract.c',
    'test/cases/xlir_async_terminal_outcome_contract.c',
    'test/cases/xlir_callable_contextual_contract.c',
    'test/cases/xlir_capture_ownership_contract.c',
    'test/cases/xlir_cell_cancel_race_contract.c',
    'test/cases/xlir_cell_storage_contract.c',
    'test/cases/xlir_cell_allocation_outcome_contract.c',
    'test/cases/xlir_cell_thread_lifetime_contract.c',
    'test/cases/xlir_class_publication_outcome_contract.c',
    'test/cases/xlir_class_storage_ir_contract.c',
    'test/cases/xlir_closure_storage_contract.c',
    'test/cases/xlir_code_owner_binding_contract.c',
    'test/cases/xlir_frame_ownership_contract.c',
    'test/cases/xlir_future_aggregate_ownership_contract.c',
    'test/cases/xlir_future_callback_storage_contract.c',
    'test/cases/xlir_callback_boundary_contract.c',
    'test/cases/xlir_generator_ownership_contract.c',
    'test/cases/xlir_generator_storage_contract.c',
    'test/cases/xlir_generator_close_routes_contract.c',
    'test/cases/xlir_generator_close_abrupt_finally_contract.c',
    'test/cases/xlir_generator_close_catch_dispatch_contract.c',
    'test/cases/xlir_generator_close_first_outcome_contract.c',
    'test/cases/xlir_generator_payload_contract.c',
    'test/cases/xlir_generator_frame_loan_contract.c',
    'test/cases/xlir_generator_managed_contract.c',
    'test/cases/xlir_module_executor_contract.c',
    'test/cases/xlir_module_ownership_contract.c',
    'test/cases/xlir_slot_transfer_contract.c',
    'test/cases/xlir_task_pool_graph_contract.c',
    'test/cases/xlir_task_storage_contract.c',
    'test/cases/xlir_task_closed_capture_contract.c',
    'test/cases/xlir_dynamic_iteration_allocation_contract.c',
    'test/cases/xlir_module_task_pool_contract.c',
    'test/cases/xlir_async_pool_outcome_contract.c'
)

$fixtureInputs = @(
    'test/cases/xlir_native_adapter_cases.json'
)
foreach ($fixture in $fixtureInputs) {
    $fixturePath = Join-Path $Root $fixture
    if (!(Test-Path -LiteralPath $fixturePath -PathType Leaf)) { throw "XLIR scenario fixture missing: $fixture" }
    try { $null = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json }
    catch { throw "XLIR scenario fixture is invalid JSON: $fixture" }
}

$contractDrivers = @{
    'check_xlir_c_owned_boundary_fault.ps1' = 'xlir_c_owned_contract.exe'
    'check_xlir_c_owned_globals_fault.ps1' = 'xlir_c_owned_contract.exe'
    'check_xlir_async_cancel_wait.ps1' = 'xlir_async_cancel_watch_contract.exe'
    'check_xlir_async_dependency_cycles.ps1' = 'xlir_async_dependency_cycle_contract.exe'
    'check_xlir_async_storage.ps1' = 'xlir_async_storage_contract.exe'
    'check_xlir_callable_contextual.ps1' = 'xlir_callable_contextual_contract.exe'
    'check_xlir_capture_ownership.ps1' = 'xlir_capture_ownership_contract.exe'
    'check_xlir_class_storage.ps1' = 'xlir_class_storage_ir_contract.exe'
    'check_xlir_code_publication.ps1' = 'xlir_class_publication_outcome_contract.exe'
    'check_xlir_executor_ownership.ps1' = 'xlir_async_dependency_cycle_contract.exe'
    'check_xlir_frame_ownership.ps1' = 'xlir_frame_ownership_contract.exe'
    'check_xlir_future_aggregate_ownership.ps1' = 'xlir_future_aggregate_ownership_contract.exe'
    'check_xlir_generator_ownership.ps1' = 'xlir_generator_ownership_contract.exe'
    'check_xlir_module_cycles.ps1' = 'xlir_code_owner_binding_contract.exe'
    'check_xlir_module_executor.ps1' = 'xlir_module_executor_contract.exe'
    'check_xlir_module_ownership.ps1' = 'xlir_code_owner_binding_contract.exe'
    'check_xlir_resident_closure.ps1' = 'xlir_closure_storage_contract.exe'
    'check_xlir_slot_transfers.ps1' = 'xlir_slot_transfer_contract.exe'
    'check_xlir_terminal_source_runtime.ps1' = 'xlir_async_terminal_outcome_contract.exe'
}
$sourceDriver = [IO.Path]::GetFullPath((Join-Path $contractDirectory 'xlir_code_owner_binding_contract.exe'))
$aggregateDirectory = Join-Path $phaseOneDirectory 'aggregate-dependencies'
$dependentGuards = @(
    'check_xlir_aggregate_strict_source.ps1'
)

function New-ScenarioGate([string]$Name, [string]$PhaseDirectory) {
    $path = Join-Path $toolsDirectory $Name
    $parameters = (Get-Command -Name $path -ErrorAction Stop).Parameters
    $arguments = [ordered]@{}
    $slug = $Name.Substring('check_xlir_'.Length)
    $slug = $slug.Substring(0, $slug.Length - '.ps1'.Length).Replace('_', '-')
    foreach ($required in @(Get-RequiredParameters $path)) {
        switch ($required) {
            'PublicDirectory' { $arguments[$required] = $publicDirectory }
            'PublicSourceDirectory' { $arguments[$required] = $publicDirectory }
            'ContractDirectory' { $arguments[$required] = $contractDirectory }
            'Directory' { $arguments[$required] = Join-Path $PhaseDirectory $slug }
            'SourceDriver' { $arguments[$required] = $sourceDriver }
            'PublicDriver' { $arguments[$required] = Join-Path $publicDirectory 'public_source.exe' }
            'Compiler' {
                if (!$contractDrivers.ContainsKey($Name)) { throw "No reviewed compiler driver for $Name" }
                $arguments[$required] = Join-Path $contractDirectory $contractDrivers[$Name]
            }
            'ContractDriver' {
                if (!$contractDrivers.ContainsKey($Name)) { throw "No reviewed contract driver for $Name" }
                $arguments[$required] = Join-Path $contractDirectory $contractDrivers[$Name]
            }
            'CellContractDriver' { $arguments[$required] = Join-Path $contractDirectory 'xlir_cell_cancel_race_contract.exe' }
            'IrContractDriver' { $arguments[$required] = Join-Path $contractDirectory 'xlir_module_ownership_contract.exe' }
            'NativeRoot' { $arguments[$required] = $NativeRoot }
            'AggregateDirectory' { $arguments[$required] = $aggregateDirectory }
            'OutputDirectory' { $arguments[$required] = Join-Path $PhaseDirectory $slug }
            default { throw "No reviewed argument source for $Name -$required" }
        }
    }
    if ($parameters.ContainsKey('Directory') -and !$arguments.Contains('Directory')) {
        $arguments.Directory = Join-Path $PhaseDirectory $slug
    }
    if ($parameters.ContainsKey('WorkDirectory')) {
        $arguments.WorkDirectory = Join-Path $PhaseDirectory $slug
    }
    switch ($Name) {
        'check_xlir_object_escape.ps1' {
            $arguments.Directory = Join-Path $PhaseDirectory $slug
            $arguments.SourceRuntimeBuildManifest = Join-Path $publicDirectory 'build.json'
        }
        'check_xlir_module_ownership.ps1' {
            $arguments.IrContractDriver = Join-Path $contractDirectory 'xlir_module_ownership_contract.exe'
        }
        'check_xlir_public_artifact_batch.ps1' { $arguments.Directory = $publicDirectory }
        'check_xlir_public_interop_batch.ps1' { $arguments.Directory = $publicDirectory }
        'check_xlir_public_runtime_batch.ps1' { $arguments.Directory = $runtimeDirectory }
        'check_xlir_public_source_batch.ps1' {
            $arguments.Directory = $publicDirectory
            $arguments.OutputDirectory = Join-Path $PhaseDirectory $slug
        }
        'check_xlir_http_async_source.ps1' {
            # This worker consumes the staged public source/artifact manifest,
            # not a newly-created empty scenario directory.
            $arguments.Directory = $publicDirectory
            $arguments.OutputDirectory = Join-Path $PhaseDirectory $slug
        }
    }
    if ($parameters.ContainsKey('Tcc')) { $arguments.Tcc = $Tcc }
    $resources = @("scenario-$slug")
    # Focused probes without a private evidence directory still emit
    # fixed build/test paths. Serialize those probes until each owns an
    # explicit output root; other comprehensive scenarios remain parallel.
    if (!$arguments.Contains('Directory') -and !$arguments.Contains('WorkDirectory')) {
        $resources += 'shared-build-test-output'
    }
    return [ordered]@{
        Script = $Name
        Resources = $resources
        Arguments = $arguments
    }
}

function Invoke-CheckedScript([string]$Path, [hashtable]$Arguments) {
    & $Path @Arguments
    if (!$? -or $LASTEXITCODE -ne 0) { throw "XLIR scenario command failed: $Path" }
}

$report = [ordered]@{
    SchemaVersion = 1
    RunId = $runId
    StartedUtc = [DateTime]::UtcNow.ToString('o')
    CompletedUtc = $null
    Status = 'running'
    NativeRoot = $NativeRoot
    Tcc = $Tcc
    FixtureInputs = @($fixtureInputs)
    PublicDirectory = $publicDirectory
    RuntimeDirectory = $runtimeDirectory
    ContractDirectory = $contractDirectory
    ArgumentsReport = Join-Path $outputPath 'arguments.json'
    PhaseReports = @()
    Error = $null
}
$reportPath = Join-Path $outputPath 'results.json'
function Save-Report {
    [IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
}
Save-Report

$previousLocation = Get-Location
try {
    Set-Location $Root
    & (Join-Path $toolsDirectory 'check_xlir_parameterized_arguments.ps1') -Root $Root -Report $report.ArgumentsReport
    if (!$?) { throw 'Parameterized argument roles failed before source builds' }
    Invoke-CheckedScript (Join-Path $toolsDirectory 'build_xlir_public_source_driver.ps1') @{
        Directory = $publicDirectory
    }
    Invoke-CheckedScript (Join-Path $toolsDirectory 'build_xlir_public_source_driver.ps1') @{
        Directory = $runtimeDirectory
        RuntimeOnly = $true
    }
    Invoke-CheckedScript (Join-Path $toolsDirectory 'stage_xlir_artifact_runtime_sources.ps1') @{
        Directory = $publicDirectory
    }
    Invoke-CheckedScript (Join-Path $toolsDirectory 'build_xlir_validation_driver.ps1') @{
        BatchDrivers = $contractSources
        BatchDirectory = $contractDirectory
        PublicDirectory = $publicDirectory
    }
    $sharedRuntimeDirectory = Join-Path $Root 'build/test/__xl_runtime'
    [IO.Directory]::CreateDirectory($sharedRuntimeDirectory) | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $publicDirectory 'include/__xl_runtime') -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $sharedRuntimeDirectory $_.Name) -Force
    }
    Copy-Item -LiteralPath (Join-Path $Root 'release/xl.dll') -Destination (Join-Path $Root 'build/test/xl.dll') -Force

    $phaseOneGates = @($reviewedGuards | Where-Object { $_ -notin $dependentGuards } |
        ForEach-Object { New-ScenarioGate $_ $phaseOneDirectory })
    $phaseOneManifest = Join-Path $outputPath 'phase1.manifest.json'
    [IO.File]::WriteAllText($phaseOneManifest,
        ([ordered]@{SchemaVersion=2;Name='xlir-parameterized-phase1';Gates=$phaseOneGates} | ConvertTo-Json -Depth 8),
        [Text.UTF8Encoding]::new($false))
    $phaseOneReport = Join-Path $outputPath 'phase1.results.json'
    $phaseOneArguments = @{
        Root = $Root; ManifestPath = $phaseOneManifest; ReportPath = $phaseOneReport; MaxParallel = $MaxParallel
    }
    & (Join-Path $toolsDirectory 'run_xlir_batch.ps1') @phaseOneArguments
    $phaseOneExit = $LASTEXITCODE
    if (!(Test-Path -LiteralPath $phaseOneReport -PathType Leaf)) {
        throw 'XLIR phase-one batch produced no report'
    }
    $phaseOneResult = Get-Content -LiteralPath $phaseOneReport -Raw | ConvertFrom-Json
    if ($phaseOneResult.Status -in @('infrastructure_failed','invalidated')) {
        throw "XLIR phase-one evidence is unusable: $($phaseOneResult.Status) / $($phaseOneResult.Error)"
    }
    $report.PhaseReports += $phaseOneReport
    Save-Report

    $blockedDependencies = @($phaseOneResult.Results | Where-Object {
        $_.Gate -eq 'check_xlir_aggregate_dependencies.ps1' -and
        $_.Status -ne 'passed'
    })
    if ($blockedDependencies.Count) {
        throw "XLIR phase two blocked by failed producers: $($blockedDependencies.Gate -join ', ')"
    }
    foreach ($dependency in @((Join-Path $aggregateDirectory 'results.json'))) {
        if (!(Test-Path -LiteralPath $dependency -PathType Leaf)) { throw "XLIR scenario dependency missing: $dependency" }
    }
    $phaseTwoGates = @($dependentGuards | ForEach-Object { New-ScenarioGate $_ $phaseTwoDirectory })
    # The package branch is a distinct invocation of the public interop worker.
    $phaseTwoGates += [ordered]@{
        Script = 'check_xlir_public_interop_batch.ps1'
        Resources = @('scenario-public-package')
        Arguments = [ordered]@{ Directory = $publicDirectory; PackageBatch = $true }
    }
    $phaseTwoManifest = Join-Path $outputPath 'phase2.manifest.json'
    [IO.File]::WriteAllText($phaseTwoManifest,
        ([ordered]@{SchemaVersion=2;Name='xlir-parameterized-phase2';Gates=$phaseTwoGates} | ConvertTo-Json -Depth 8),
        [Text.UTF8Encoding]::new($false))
    $phaseTwoReport = Join-Path $outputPath 'phase2.results.json'
    $phaseTwoArguments = @{
        Root = $Root; ManifestPath = $phaseTwoManifest; ReportPath = $phaseTwoReport; MaxParallel = $MaxParallel
    }
    & (Join-Path $toolsDirectory 'run_xlir_batch.ps1') @phaseTwoArguments
    $phaseTwoExit = $LASTEXITCODE
    if (!(Test-Path -LiteralPath $phaseTwoReport -PathType Leaf)) {
        throw 'XLIR phase-two batch produced no report'
    }
    $phaseTwoResult = Get-Content -LiteralPath $phaseTwoReport -Raw | ConvertFrom-Json
    if ($phaseTwoResult.Status -in @('infrastructure_failed','invalidated')) {
        throw "XLIR phase-two evidence is unusable: $($phaseTwoResult.Status) / $($phaseTwoResult.Error)"
    }
    $report.PhaseReports += $phaseTwoReport
    if ($phaseOneExit -ne 0 -or $phaseTwoExit -ne 0) {
        throw "XLIR scenario failures collected: phase1=$phaseOneExit phase2=$phaseTwoExit"
    }
    $report.Status = 'passed'
} catch {
    $report.Status = 'failed'
    $report.Error = $_.Exception.Message
    throw
} finally {
    Set-Location $previousLocation
    $report.CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Save-Report
}

Write-Host "XLIR parameterized scenarios passed: $($reviewedGuards.Count) workers plus the package interop branch."
Write-Host "Report: $reportPath"
exit 0
