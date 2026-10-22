param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=(Get-Location).Path
$outputPath=[IO.Path]::GetFullPath((Join-Path $rootPath $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Keep prior generator evidence; choose a fresh directory' }
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$contracts=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
$runtimeObject=@($manifest.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if (!$manifest.SourceXrt -or $runtimeObject.Count -ne 1) { throw 'Current source compiler/XRT required' }
$files=@(Get-Item $PSCommandPath,$ContractDriver,$manifest.Driver,test/cases/xlir_generator_ownership.xl,
    test/cases/xlir_generator_ownership_host.c,test/cases/xlir_generator_ownership_contract.c,tools/xrt_single_header_impl.c)
$inputs=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+@($contracts.Inputs)+@($contracts.Drivers)+
    @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale generator inputs' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$prefix=Join-Path $outputPath 'xlir_generator_ownership'
foreach ($pass in @('','.repeat')) {
    & $ContractDriver "$prefix$pass.bind.h" *> "$outputPath/ir-contract$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Generator item/frame/consumer IR contract failed' }
    & $manifest.Driver test/cases/xlir_generator_ownership.xl emit "$prefix$pass" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Public generator generation failed' }
}
foreach ($suffix in @('c','h','json','bind.h')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw "Nondeterministic generator $suffix" }
}
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$includes=@("-I$includePath","-I$outputPath",'-I.')
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
    '-Wno-pointer-sign','-Wno-format-truncation')
$sources=@('test/cases/xlir_generator_ownership_host.c','src/runtime/xl6_continuation_queue.c',
    'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c',
    'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c')
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
$lanes=@()
foreach ($compiler in @('gcc-O0','gcc-O2','tcc')) {
    $binary="$outputPath/$compiler.exe"
    if ($compiler -eq 'tcc') {
        & $Tcc -std=c11 @includes @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$compiler.build.log"
    } else {
        $optimization=if ($compiler -eq 'gcc-O0') {'-O0'} else {'-O2'}
        & gcc @strict $optimization @includes @sources $runtimeObject[0].Path @libs -o $binary *> "$outputPath/$compiler.build.log"
    }
    $build=$LASTEXITCODE; $run=$null; $complete=$false
    if ($build -eq 0) {
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath `
            -RedirectStandardOutput "$outputPath/$compiler.run.log" -RedirectStandardError "$outputPath/$compiler.err.log"
        $null = $process.Handle
        if (!$process.WaitForExit(60000)) { $process.Kill(); $process.WaitForExit(); $run=-999 }
        else { $run=$process.ExitCode }
        $complete=[bool](Select-String -LiteralPath "$outputPath/$compiler.run.log" -SimpleMatch 'Generated generator ownership: 2400 lifecycles, 1000 async transactions, 2000 real suspensions, 1800 running inspection refusals, 16000 inspection probes, 6600 actual failures,' -Quiet)
    }
    $lanes+=[pscustomobject]@{Name=$compiler;Build=$build;Run=$run;Complete=$complete}
    Write-Output "$compiler build=$build run=$run complete=$complete"
}
$unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
    IrContractExit=0;Deterministic=$true;Lanes=$lanes;GeneratorLifecyclesPerLane=2400;AsyncTransactionsPerLane=1000;
    RealSuspensionsPerLane=2000;RunningRefusalsPerLane=1800;InspectionProbesPerLane=16000;ActualInspectionFailuresPerLane=6600;
    Scope='Current-source generated generator controls/frames/current items, native Value snapshot iterators, nested consumers in sync/generator/async and real Watch handoffs. Exact owning edges, native aliases, error identity, failure-atomic inspection, all async terminal outcomes, early close and per-transaction allocation balance. Host supplies quiescence and keeps code resident; no module auto-pin/global-cycle/safepoint/unload claim.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Generator ownership batch failed; inspect all compiler lanes' }
