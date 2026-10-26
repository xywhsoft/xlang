param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$outputPath=if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new aggregate evidence directory' }
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
if (!$manifest.SourceXrt) { throw 'Aggregate graphs require current source XRT' }
$runtimeObject=@($manifest.Objects|Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if ($runtimeObject.Count -ne 1) { throw 'Ambiguous source runtime object' }
$caseNames=@('xlir_task_when_any_literal_abi','xlir_task_when_any_array_abi',
    'xlir_task_aggregate_array_abi','cgen_task_aggregate_type_closure','xlir_task_all_void_outcome')
$files=@(Get-Item $PSCommandPath,$ContractDriver,$manifest.Driver,
    test/cases/xlir_future_aggregate_ownership.xl,test/cases/xlir_future_aggregate_ownership_host.c,
    test/cases/xlir_future_aggregate_ownership_contract.c,test/cases/xlir_task_all_error_abi.xl)
$files+=@($caseNames|ForEach-Object {Get-Item -LiteralPath "test/cases/$_.xl"})
$records=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if ((Get-FileHash -LiteralPath $manifest.Driver).Hash -ne $manifest.DriverSha256 -or
    @($records|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale aggregate inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$prefix=Join-Path $outputPath 'xlir_future_aggregate_ownership'
foreach ($pass in @('','.repeat')) {
    & $manifest.Driver test/cases/xlir_future_aggregate_ownership.xl emit "$prefix$pass" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Aggregate graph emission failed' }
}
foreach ($suffix in @('c','h','json')) {
    if ((Get-FileHash -LiteralPath "$prefix.$suffix").Hash -ne (Get-FileHash -LiteralPath "$prefix.repeat.$suffix").Hash) { throw 'Nondeterministic aggregate emission' }
}
& $ContractDriver *> "$outputPath/contract.log"; $contractExit=$LASTEXITCODE
$contractComplete=[bool](Select-String -LiteralPath "$outputPath/contract.log" -SimpleMatch '10 operations, 43 C/header-gated mutations' -Quiet)
$contractComplete=$contractComplete -and [bool](Select-String -LiteralPath "$outputPath/contract.log" -SimpleMatch 'Aggregate module ABI: 13 additional C/header-gated mutations, all 6 operations independently derived' -Quiet)
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
$includes=@("-I$includePath","-I$outputPath",'-I.','-Irelease','-DXL_STATIC_EMBED')
$sources=@('test/cases/xlir_future_aggregate_ownership_host.c','src/api/xl_future_bridge.c','src/runtime/xl6_future_aggregate.c','src/runtime/xl6_future_aggregate_array.c','src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c','src/runtime/xl6_future_payload.c',
    'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_slot.c',
    'src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c',
    'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c',
    'src/runtime/xl6_value_admission.c','src/runtime/xl6_future_metadata.c')
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'); $lanes=@()
foreach ($lane in @('gcc-O0','gcc-O2','tcc')) {
    $binary="$outputPath/$lane.exe"
    if ($lane -eq 'tcc') {
        & $Tcc -std=c11 @includes @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$lane.build.log"
    } else {
        $optimization=if ($lane -eq 'gcc-O0') {'-O0'} else {'-O2'}
        & gcc @strict $optimization @includes @sources $runtimeObject[0].Path @libs -o $binary *> "$outputPath/$lane.build.log"
    }
    $build=$LASTEXITCODE; $run=$null; $complete=$false
    if ($build -eq 0) {
        & $binary *> "$outputPath/$lane.run.log"; $run=$LASTEXITCODE
        $complete=[bool](Select-String -LiteralPath "$outputPath/$lane.run.log" -SimpleMatch 'Future aggregate ownership: 3500 generated transactions' -Quiet) -and
            [bool](Select-String -LiteralPath "$outputPath/$lane.run.log" -SimpleMatch 'Aggregate bridge launch: 192 probes, 40 preparation failures, 8 accepted mapping failures' -Quiet)
    }
    $lanes+=[pscustomobject]@{Name=$lane;Build=$build;Run=$run;Complete=$complete}
    Write-Output "$lane aggregate graphs build=$build run=$run complete=$complete"
}
function Invoke-Public([string]$source,[string]$mode,[string]$prefix,[string]$log) {
$process=Start-Process -FilePath $manifest.Driver -ArgumentList @($source,$mode,$prefix) -WindowStyle Hidden -PassThru -RedirectStandardOutput "$log.log" -RedirectStandardError "$log.err.log"
    $null = $process.Handle
    if ($process.WaitForExit(45000)) { $code=$process.ExitCode }
    else { $process.Kill(); $process.WaitForExit(); $code='timeout-45s' }
    $process.Dispose()
    $complete=[bool](Select-String -LiteralPath "$log.log" -SimpleMatch "public source $mode complete" -Quiet)
    return [pscustomobject]@{Exit=$code;Complete=$complete}
}
$public=@()
foreach ($name in $caseNames) {
    $prefix="$outputPath/$name"; $source="test/cases/$name.xl"
    $entry=[ordered]@{Name=$name;Emit=$null;Repeat=$null;Deterministic=$false;Run=$null;Module=$null}
    $entry.Emit=Invoke-Public $source 'emit' $prefix "$prefix.emit"
    if ($entry.Emit.Exit -eq 0 -and $entry.Emit.Complete) {
        $entry.Repeat=Invoke-Public $source 'emit' "$prefix.repeat" "$prefix.repeat"
        $entry.Deterministic=$entry.Repeat.Exit -eq 0 -and $entry.Repeat.Complete
        if ($entry.Deterministic) {
            foreach ($suffix in @('c','h','json')) { if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) {$entry.Deterministic=$false} }
        }
        $entry.Run=Invoke-Public $source 'run' $prefix "$prefix.run"
        $entry.Module=Invoke-Public $source 'module' $prefix "$prefix.module"
    }
    $public+=[pscustomobject]$entry
    Write-Output "$name emit=$($entry.Emit.Exit) run=$($entry.Run.Exit) module=$($entry.Module.Exit) deterministic=$($entry.Deterministic)"
}
$unchanged=@($records|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$records;InputsUnchanged=$unchanged;
    ContractExit=$contractExit;ContractComplete=$contractComplete;IrMutations=43;AdditionalModuleAbiMutations=13;Deterministic=$true;
    Lanes=$lanes;TransactionsPerLane=3500;BridgeLaunchProbesPerLane=192;Public=$public;
    Scope='Generated literal/array All/Race/WhenAny and callable payload graphs, exact pending/terminal ownership, original error identity, producer/output cancellation, closed sources, OOM before and after acceptance, and actual void-All error propagation. Code stays resident; generic suspended-frame/module lifetime remains open.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or $contractExit -ne 0 -or !$contractComplete -or @($lanes|Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count -or
    @($public|Where-Object {!$_.Deterministic -or $_.Run.Exit -ne 0 -or !$_.Run.Complete -or $_.Module.Exit -ne 0 -or !$_.Module.Complete}).Count) { throw 'Aggregate batch failed; inspect all fixed-denominator results' }
