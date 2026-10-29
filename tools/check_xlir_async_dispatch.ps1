param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver, [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_async_dispatch'
    if($Directory){
        $Directory=[IO.Path]::GetFullPath($Directory)
        if(Test-Path -LiteralPath $Directory){throw 'Preserve prior dispatch evidence'}
        New-Item -ItemType Directory -Path $Directory|Out-Null
        $prefix=Join-Path $Directory 'xlir_async_dispatch'
    }
    & $SourceDriver test/cases/xlir_async_dispatch.xl "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch source generation failed' }
    & $SourceDriver test/cases/xlir_async_dispatch.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Dispatch repeat generation failed' }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) {
            throw "Dispatch nondeterministic $extension"
        }
    }
    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
    $awaits = @($ir.instructions | Where-Object kindName -eq 'await')
    if ($awaits.Count -ne 3 -or @($awaits | Where-Object awaitOutcomeAbi -ne 1).Count) { throw 'Dispatch await status carrier missing' }
    if ($ir.moduleContinuationAbi -ne 2 -or $ir.asyncMachines.Count -ne 1 -or
        $ir.asyncMachines[0].executionAbi -ne 3 -or $ir.asyncMachines[0].suspensionCount -ne 2) { throw 'Ready-entry fixed-worker dispatch IR contract missing' }
    $binding = "#define XLIR_DISPATCH_CHAIN xlir_async_start_$($ir.asyncMachines[0].targetSymbol)`n"
    [IO.File]::WriteAllText([IO.Path]::GetFullPath("$prefix.binding.h"), $binding, [Text.UTF8Encoding]::new($false))
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation','-I.','-Ibuild/test','-Irelease','-Irelease/include')
    $contractBuild=Get-Content -LiteralPath (Join-Path (Split-Path -Parent $SourceDriver) 'build.json') -Raw|ConvertFrom-Json
    if(!$contractBuild.Complete-or!$contractBuild.PublicDirectory){throw 'Dispatch requires the verified full source cohort'}
    $public=$contractBuild.PublicDirectory
    $build=Get-Content -LiteralPath (Join-Path $public 'build.json') -Raw|ConvertFrom-Json
    $native=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$build.Complete-or!$build.SourceXrt-or$native.Count-ne1){throw 'Dispatch source XRT missing'}
    $closure=Get-XlirResidentClosure -Root $Root -Directory "$prefix.resident-closure" -Sources @('src/runtime/xl6_continuation_future.c') -NativeProvider 'tools/xrt_single_header_impl.c' -PublicDirectory $public -Fixtures @('test/cases/xlir_async_dispatch.xl')
    $resident=@($closure.Sources|Where-Object {$_-ne'src/runtime/xl6_continuation_future.c'})
    $inputs=@($closure.Inputs)+@($native)+@($contractBuild.Inputs)+@($contractBuild.Drivers)
    $files=@($PSCommandPath,'test/cases/xlir_async_dispatch_host.c','test/cases/xlir_async_dispatch_runtime.c')
    $inputs+=@($files|ForEach-Object {[pscustomobject]@{Path=[IO.Path]::GetFullPath($_);Sha256=(Get-FileHash -LiteralPath $_).Hash}})
    $flags+=@("-I$public/include")
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    $code=Invoke-XlirGccToLog ($flags+@('-DxrtTakeError=dispatch_take_error','-DXLIR_CONTINUATION_CAPACITY=2','-include',"$prefix.c",'-include',"$prefix.binding.h",'-c','test/cases/xlir_async_dispatch_host.c','-o',"$prefix.host.o")) "$prefix.host.build.log"
    if ($code -ne 0) { throw 'Generated dispatch strict C compile failed' }
    $code=Invoke-XlirGccToLog ($flags+@('-DxrtFutureWatchAdd=dispatch_watch_add','-c','src/runtime/xl6_continuation_future.c','-o',"$prefix.transport.o")) "$prefix.transport.build.log"
    if ($code -ne 0) { throw 'Dispatch Watch boundary build failed' }
    $code=Invoke-XlirGccToLog ($flags+@("$prefix.host.o","$prefix.transport.o",'test/cases/xlir_async_dispatch_runtime.c')+$resident+@($native[0].Path)+$libs+@('-o',"$prefix.exe")) "$prefix.link.log"
    if ($code -ne 0) { throw 'Dispatch source runtime link failed' }
    $failed = @()
    $lanes=@()
    foreach ($mode in 0..6) {
        $log="$prefix.capacity2.mode$mode.log"
        $code=Invoke-XlirNativeToLog "$prefix.exe" @("$mode") $log
        $complete=[bool](Select-String -LiteralPath $log -SimpleMatch "generated dispatch: mode=$mode cases=25 balanced memory" -Quiet)
        $lanes+=[pscustomobject]@{Capacity=2;Mode=$mode;Exit=$code;Complete=$complete;Log=[IO.Path]::GetFullPath($log);Sha256=(Get-FileHash -LiteralPath $log).Hash}
        if ($code -ne 0-or!$complete) { $failed += "capacity=2 mode=$mode" }
    }
    $code=Invoke-XlirGccToLog ($flags+@('-DxrtTakeError=dispatch_take_error','-DXLIR_CONTINUATION_CAPACITY=1','-include',"$prefix.c",'-include',"$prefix.binding.h",'-c','test/cases/xlir_async_dispatch_host.c','-o',"$prefix.single.o")) "$prefix.single.build.log"
    if ($code -ne 0) { throw 'Single-credit dispatch strict C compile failed' }
    $code=Invoke-XlirGccToLog ($flags+@("$prefix.single.o","$prefix.transport.o",'test/cases/xlir_async_dispatch_runtime.c')+$resident+@($native[0].Path)+$libs+@('-o',"$prefix.single.exe")) "$prefix.single.link.log"
    if ($code -ne 0) { throw 'Single-credit dispatch link failed' }
    foreach ($mode in @(0,1,2,3,4)) {
        $log="$prefix.capacity1.mode$mode.log"
        $code=Invoke-XlirNativeToLog "$prefix.single.exe" @("$mode") $log
        $complete=[bool](Select-String -LiteralPath $log -SimpleMatch "generated dispatch: mode=$mode cases=25 balanced memory" -Quiet)
        $lanes+=[pscustomobject]@{Capacity=1;Mode=$mode;Exit=$code;Complete=$complete;Log=[IO.Path]::GetFullPath($log);Sha256=(Get-FileHash -LiteralPath $log).Hash}
        if ($code -ne 0-or!$complete) { $failed += "capacity=1 mode=$mode" }
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{Complete=(!$failed.Count-and$unchanged);InputsUnchanged=$unchanged;Inputs=$inputs;Lanes=$lanes;CasesPerLane=25;ExecutionAbi=3;Deterministic=$true;Scope='Real ready entry, fixed worker and source Watch transport; all original 12 dispatch modes and 300 transactions retained. No shipping acceptance.'}
    [IO.File]::WriteAllText("$prefix.results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if($Directory){Copy-Item -LiteralPath "$prefix.results.json" -Destination (Join-Path $Directory 'results.json')}
    if(!$unchanged){throw 'Dispatch inputs changed'}
    if ($failed.Count) { throw "All dispatch lanes collected; failures: $($failed -join ', ')" }
} finally { Pop-Location }
Write-Output 'Generated async dispatcher passed; source release integration and escaped-code coverage remain separate admission work.'
