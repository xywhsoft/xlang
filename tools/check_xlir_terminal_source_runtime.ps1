param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$outputPath=if ([IO.Path]::IsPathRooted($Directory)) { [IO.Path]::GetFullPath($Directory) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Directory)) }
if (Test-Path -LiteralPath $outputPath) { throw 'Use a new terminal evidence directory' }
$manifest=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
$runtimeObject=@($manifest.Objects|Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
if (!$manifest.SourceXrt -or $runtimeObject.Count -ne 1) { throw 'Current source runtime required' }
$runtimeSources=@('test/cases/xlir_async_source_sync.c','test/cases/xlir_async_source_transport.c',
    'src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
$files=@(Get-Item $PSCommandPath,(Join-Path $PSScriptRoot 'xlir_native_process.ps1'),$ContractDriver,
    test/cases/xlir_async_terminal_outcome_contract.c,
    test/cases/xlir_async_terminal_outcome_runtime.c,test/cases/xlir_async_terminal_outcome_host.c,
    test/cases/xlir_async_source_sync.h,test/cases/xlir_launch_test.h)+@(Get-Item $runtimeSources)
$inputs=@($manifest.Inputs)+@($manifest.Headers)+@($runtimeObject)+
    @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
if (@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count) { throw 'Stale terminal sources' }
New-Item -ItemType Directory -Path $outputPath | Out-Null
$rootPath=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$closure=Get-XlirResidentClosure -Root $rootPath -Directory "$outputPath/resident-closure" `
    -Sources @($runtimeSources|Where-Object {$_-like'src/*'}) -NativeProvider 'tools/xrt_single_header_impl.c' `
    -Fixtures @('test/cases/xlir_async_terminal_outcome.xl') -PublicDirectory $PublicDirectory
# This test observer compiles the real provider with one native Watch hook.
# Keep its canonical dependency closure, but link the physical provider once.
$watchProvider='src/runtime/xl6_continuation_future.c'
if($watchProvider-notin$closure.Sources-or
    @(Select-String -LiteralPath 'test/cases/xlir_async_source_transport.c' -SimpleMatch `
        '#include "../../src/runtime/xl6_continuation_future.c"').Count-ne1){throw 'Watch observer no longer provides the declared resident source'}
$runtimeSources=@($runtimeSources|Where-Object {$_-notlike'src/*'})+@($closure.Sources|Where-Object {$_-ne$watchProvider})
$inputs+=@($closure.Inputs)
$prefix=(Join-Path $outputPath 'xlir_async_terminal_outcome').Replace('\','/')
foreach ($pass in @('','.repeat')) {
    & $ContractDriver "$prefix$pass.c" "$prefix$pass.h" "$prefix$pass.json" *> "$outputPath/emit$pass.log"
    if ($LASTEXITCODE -ne 0) { throw 'Terminal outcome contract failed' }
}
foreach ($suffix in @('c','h','json','h.bind.h')) {
    if ((Get-FileHash "$prefix.$suffix").Hash -ne (Get-FileHash "$prefix.repeat.$suffix").Hash) { throw 'Nondeterministic terminal artifact' }
    Copy-Item -LiteralPath "$prefix.$suffix" -Destination "$outputPath/module.$suffix"
}
$includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
$includes=@("-I$includePath",'-I.','-Itest/cases','-Ibuild/test','-Irelease','-Irelease/include','-DXL_STATIC_EMBED',
    (New-XlirCStringDefine -Name XLIR_TERMINAL_SOURCE -Value "$prefix.c" -RawArgument),
    (New-XlirCStringDefine -Name XLIR_TERMINAL_BINDING -Value "$prefix.h.bind.h" -RawArgument))
$strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
    '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
$sources=@('test/cases/xlir_async_terminal_outcome_runtime.c','test/cases/xlir_async_terminal_outcome_host.c')+$runtimeSources
$libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'); $lanes=@()
foreach ($lane in @('gcc-O0','gcc-O2','tcc')) {
    $binary="$outputPath/$lane.exe"
    if ($lane -eq 'tcc') {
        $arguments=@('-std=c11')+$includes+$sources+
            @('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)
        $build=Invoke-XlirTccToLog $Tcc $arguments "$outputPath/$lane.build.log"
    }
    else {
        $optimization=if ($lane -eq 'gcc-O0') {'-O0'} else {'-O2'}
        $build=Invoke-XlirGccToLog ($strict+@($optimization)+$includes+$sources+
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
