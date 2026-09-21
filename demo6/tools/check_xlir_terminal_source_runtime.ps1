param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$outputPath=[IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory))
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new terminal evidence directory' }
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$runtimeObject=@($manifest.Objects|Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if (!$manifest.SourceXrt -or $runtimeObject.Count -ne 1) { throw 'Current source runtime required' }
$runtimeSources=@('test/cases/xlir_async_source_sync.c','test/cases/xlir_async_source_transport.c',
    'src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
$files=@(Get-Item $PSCommandPath,(Join-Path $PSScriptRoot 'xlir_native_process.ps1'),$ContractDriver,
    test/cases/xlir_async_terminal_outcome_contract.c,
    test/cases/xlir_async_terminal_outcome_runtime.c,test/cases/xlir_async_terminal_outcome_host.c,
    test/cases/xlir_async_source_sync.h,test/cases/xlir_launch_test.h)+@(Get-Item $runtimeSources)
$inputs=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale terminal sources' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$prefix='build/test/xlir_async_terminal_outcome'
foreach ($pass in @('','.repeat')) {
    & $ContractDriver "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Terminal outcome contract failed' }
}
foreach ($suffix in @('c','h','json','h.bind.h')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw 'Nondeterministic terminal artifact' }
    Copy-Item -LiteralPath "$prefix.$suffix" -Destination "$outputPath/module.$suffix"
}
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$includes=@("-I$includePath",'-I.','-Ibuild/test','-Irelease','-DXL_STATIC_EMBED')
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
$sources=@('test/cases/xlir_async_terminal_outcome_runtime.c','test/cases/xlir_async_terminal_outcome_host.c')+$runtimeSources
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'); $lanes=@()
foreach ($lane in @('gcc-O0','gcc-O2','tcc')) {
    $binary="$outputPath/$lane.exe"
    if ($lane -eq 'tcc') {
        $arguments=@('-std=c11')+$includes+$sources+
            @('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)
        $build=Invoke-XlirNativeToLog $Tcc $arguments "$outputPath/$lane.build.log"
    }
    else {
        $optimization=if ($lane -eq 'gcc-O0') {'-O0'} else {'-O2'}
        $build=Invoke-XlirNativeToLog (Get-Command gcc).Source ($strict+@($optimization)+$includes+$sources+
            @($runtimeObject[0].Path)+$libs+@('-o',$binary)) "$outputPath/$lane.build.log"
    }
    $run=$null; $complete=$false
    if ($build -eq 0) { $run=Invoke-XlirNativeToLog $binary @() "$outputPath/$lane.run.log"
        $complete=[bool](Select-String -LiteralPath "$outputPath/$lane.run.log" -SimpleMatch 'async terminal outcomes:' -Quiet) }
    $lanes+=[pscustomobject]@{Name=$lane;Build=$build;Run=$run;Complete=$complete}
    Write-Output "$lane terminal outcomes build=$build run=$run complete=$complete"
}
$unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
$report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Deterministic=$true;Lanes=$lanes;
    Scope='Existing full terminal-outcome runtime assertions on current source XRT: catch/rethrow/finally, nested suspensions and returned Futures. Native GCC O0/O2 and standalone TinyCC, not the old release C-host JIT/AOT lanes.'}
[IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if (!$unchanged -or @($lanes|Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count) { throw 'Source terminal outcome batch failed' }
