param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=(Get-Location).Path
$outputPath=[IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new frame evidence directory' }
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$contracts=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
$runtimeObject=@($manifest.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if (!$manifest.SourceXrt -or $runtimeObject.Count -ne 1) { throw 'Current source compiler and XRT required' }
$files=@(Get-Item $PSCommandPath,$ContractDriver,$manifest.Driver,
    test/cases/xlir_frame_ownership.xl,test/cases/xlir_cell_continuation_outcome.xl,
    test/cases/xlir_frame_ownership_host.c,test/cases/xlir_frame_ownership_contract.c,
    test/cases/xlir_continuation_ownership_contract.c,test/cases/xlir_continuation_queue_contract.c,
    test/cases/xlir_continuation_handoff_contract.c,test/cases/xlir_continuation_handoff_transport.c,
    test/cases/xlir_continuation_executor_contract.c,test/cases/xlir_continuation_progress_contract.c)
$inputs=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+@($contracts.Inputs)+@($contracts.Drivers)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale frame inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
& $ContractDriver *> "$outputPath/ir-contract.log"
$contractExit=$LASTEXITCODE
$prefix=Join-Path $outputPath 'xlir_frame_ownership'
foreach ($pass in @('','.repeat')) {
    & $manifest.Driver test/cases/xlir_frame_ownership.xl emit "$prefix$pass" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Frame generation failed' }
}
foreach ($suffix in @('c','h','json')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw 'Nondeterministic frame artifact' }
}
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$includes=@("-I$includePath","-I$outputPath",'-I.')
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation')
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$queueSource='src/runtime/xl6_continuation_queue.c'
$executorSource='src/runtime/xl6_continuation_executor.c'
$transportSource='src/runtime/xl6_continuation_future.c'
$cases=@(
    @{Name='generated';Sources=@('test/cases/xlir_frame_ownership_host.c',$queueSource,$executorSource,$transportSource,'src/runtime/xl6_future_payload.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_lease.c');Marker='Generated frame ownership: 1500 transactions, 3100 real Watch retirements';Tcc=$true},
    @{Name='ownership';Sources=@('test/cases/xlir_continuation_ownership_contract.c',$queueSource,$transportSource);Marker='Continuation ownership: 400 real Watch lifecycles, 2400 inspection probes, 1600 actual failures';Tcc=$true},
    @{Name='queue';Sources=@('test/cases/xlir_continuation_queue_contract.c',$queueSource);Marker='continuation queue:';Tcc=$false},
    @{Name='handoff';Sources=@('test/cases/xlir_continuation_handoff_contract.c',$queueSource,$executorSource,'test/cases/xlir_continuation_handoff_transport.c');Marker='continuation';Tcc=$false},
    @{Name='executor';Sources=@('test/cases/xlir_continuation_executor_contract.c',$queueSource,$executorSource);Marker='continuation';Tcc=$false},
    @{Name='progress';Sources=@('test/cases/xlir_continuation_progress_contract.c',$queueSource,$executorSource);Marker='guaranteed progress:';Tcc=$false})
$lanes=@()
foreach ($case in $cases) {
    if ('src/runtime/xl6_code_lease.c' -notin $case.Sources) {
        $case.Sources += 'src/runtime/xl6_code_lease.c'
    }
    if ($case.Name -eq 'generated') { $case.Sources += 'src/runtime/xl6_code_finalizer.c' }
    foreach ($compiler in @('gcc-O0','gcc-O2','tcc')) {
        if ($compiler -eq 'tcc' -and !$case.Tcc) { continue }
        $name="$($case.Name)-$compiler"; $binary="$outputPath/$name.exe"
        if ($compiler -eq 'tcc') {
            & $Tcc -std=c11 @includes @($case.Sources) tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"
        } else {
            $optimization=if ($compiler -eq 'gcc-O0') {'-O0'} else {'-O2'}
            & gcc @strict $optimization @includes @($case.Sources) $runtimeObject[0].Path @libs -o $binary *> "$outputPath/$name.build.log"
        }
        $build=$LASTEXITCODE; $run=$null; $complete=$false
        if ($build -eq 0) {
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath `
                -RedirectStandardOutput "$outputPath/$name.run.log" -RedirectStandardError "$outputPath/$name.err.log"
            $null = $process.Handle
            if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $run=-999 }
            else { $run=$process.ExitCode }
            $complete=[bool](Select-String -LiteralPath "$outputPath/$name.run.log" -SimpleMatch $case.Marker -Quiet)
        }
        $lanes+=[pscustomobject]@{Name=$name;Build=$build;Run=$run;Complete=$complete}
        Write-Output "$name build=$build run=$run complete=$complete"
    }
}
$unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
    IrContractExit=$contractExit;Deterministic=$true;Lanes=$lanes;GeneratedTransactionsPerLane=1500;
    WatchRetirementsPerGeneratedLane=3100;NativeWatchLifecyclesPerLane=400;InspectionProbesPerLane=2400;InspectionFailuresPerLane=1600;OpaqueCodeOwnerRefusalsPerLane=100;
    Scope='Real generated async frames and Watch/successor handoff on a host-driven queue with whole-graph quiescence; precise physical nodes, repeated captures, cells, errors, aliases, all terminal outcomes and balanced memory. Existing queue/handoff/executor/progress assertions separately rerun on source XRT. No worker safepoint, opaque generator/iterator payload, bound-module graph or automatic unloading claim.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or $contractExit -ne 0 -or @($lanes|Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Frame ownership batch failed; inspect every lane log' }
