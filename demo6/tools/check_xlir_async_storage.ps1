param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior async-storage evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$public.SourceXrt -or $runtime.Count -ne 1){throw 'Current source runtime required'}
    $sources=@('test/cases/xlir_async_storage_runtime.c','test/cases/xlir_async_storage_host.c',
        'src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_slot.c','src/runtime/xl6_code_lease.c',
        'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c')
    $managedSources=@('test/cases/xlir_async_managed_runtime.c','test/cases/xlir_async_managed_host.c','src/runtime/xl6_ownership_plan.c')+
        @($sources | Where-Object {$_ -notmatch '^test/'})
    $producerSources=@('test/cases/xlir_async_producer_runtime.c','test/cases/xlir_async_managed_host.c','src/runtime/xl6_ownership_plan.c')+
        @($sources | Where-Object {$_ -notmatch '^test/'})
    $files=@(Get-Item -LiteralPath $PSCommandPath,$ContractDriver,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_async_storage_contract.c','test/cases/xlir_async_storage.xl','test/cases/xlir_frame_ownership.xl',
        'test/cases/xlir_cell_continuation_outcome.xl','test/cases/xlir_async_pool_launch_allocation.xl',
        'test/cases/xlir_async_launch_allocation.xl','test/cases/xlir_async_managed_dll.c','test/cases/xlir_async_managed_dll.h')+@(Get-Item -LiteralPath $sources)+@(Get-Item -LiteralPath $managedSources)+@(Get-Item -LiteralPath $producerSources)
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count){throw 'Stale async-storage inputs'}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $marker='Async storage IR: 5 fixtures, 30 rejected C/header-gated mutations, 4 queued frame fixtures, 3 resumable, 1 root-only, 1 frame-free ready future; explicit text/JSON ABI'
    $emission=@();$deterministic=$true
    foreach($repeat in @('','.repeat')){
        $prefix="$outputPath/xlir_async_storage$repeat"
        & $ContractDriver "$prefix.c" "$prefix.bind.h" "$prefix.json" *> "$prefix.emit.log"
        $exitCode=$LASTEXITCODE
        $complete=[regex]::Matches([IO.File]::ReadAllText("$prefix.emit.log"),[regex]::Escape($marker)).Count -eq 1
        $emission += [pscustomobject]@{Pass=$repeat;Exit=$exitCode;Complete=$complete}
    }
    foreach($suffix in @('c','bind.h','json')){
        if(!(Test-Path "$outputPath/xlir_async_storage.$suffix") -or !(Test-Path "$outputPath/xlir_async_storage.repeat.$suffix") -or
            (Get-FileHash "$outputPath/xlir_async_storage.$suffix").Hash -ne (Get-FileHash "$outputPath/xlir_async_storage.repeat.$suffix").Hash){$deterministic=$false}
    }
    $includePath=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
    $includes=@("-I$includePath","-I$outputPath",'-I.')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@()
    $runtimeMarker='Async physical storage: 200 frozen allocate/free transitions, 100 generated launches, 200 generated Watch retirements, 100 blocked-cleanup successor handoffs, 100 true OOM rollbacks, 100 armed-OOM frees; exact graph/count/credit/allocator balance, no native root registration'
    $managedMarker='Managed async: 240 lifecycle transactions, 80 real Watch outcomes, 50 generated successor chains, 580 cross-thread freezes, 820 ABI/admission refusals, 120 exact failure outcomes, 10 real OOM rollbacks, 20 actual DLL unloads; actual pins and plan retirement, exact memory balance'
    $producerMarker='Async producer graph: 100 generated pending/ready outcome graphs, 1040 exact policy/identity refusals, 30 root retries, 640 successor rollbacks and handoffs, 200 cross-thread freezes, 330 allocation budgets; stable producer, real Plan pins, constant live chain storage, exact memory balance'
    foreach($variant in @('original','managed','producer')){ foreach($configuration in @('gcc-O0','gcc-O2','tcc')){
        $name=if($variant -eq 'original'){$configuration}else{"$variant-$configuration"}
        $caseSources=if($variant -eq 'original'){$sources}elseif($variant -eq 'managed'){$managedSources}else{$producerSources}
        $binary="$outputPath/$name.exe"
        if($configuration -eq 'tcc'){
            $buildExit=Invoke-XlirNativeToLog $Tcc (@('-std=c11')+$includes+$caseSources+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
        }else{
            $buildExit=Invoke-XlirNativeToLog gcc ($strict+@("-$($configuration.Substring(4))")+$includes+$caseSources+@($runtime[0].Path)+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
        }
        $runExit=$null;$complete=$false;$allocations=0;$dllExit=$null;$trueOom=0
        if($variant -eq 'managed'){
            $library="$outputPath/$name.dll"
            if($configuration -eq 'tcc'){
                $dllExit=Invoke-XlirNativeToLog $Tcc @('-std=c11','-I.','-shared','test/cases/xlir_async_managed_dll.c','-o',$library) "$outputPath/$name.dll.build.log"
            }else{
                $dllExit=Invoke-XlirNativeToLog gcc ($strict+@("-$($configuration.Substring(4))",'-I.','-shared','test/cases/xlir_async_managed_dll.c','-o',$library)) "$outputPath/$name.dll.build.log"
            }
        }
        if($buildExit -eq 0){
            $launch=@{FilePath=$binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;RedirectStandardOutput="$outputPath/$name.out.log";RedirectStandardError="$outputPath/$name.err.log"}
            if($variant -eq 'managed'){$launch.ArgumentList=@($library)}
$process=Start-Process @launch
            $null = $process.Handle
            if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$runExit=-999}
            else{$process.Refresh();$runExit=$process.ExitCode}
            $process.Dispose()
            $output=[IO.File]::ReadAllText("$outputPath/$name.out.log")
            $allocator=[regex]::Matches($output,'Async storage allocator: ([1-9][0-9]*) real allocations, zero live delta')
            $complete=$runExit -eq 0 -and [regex]::Matches($output,[regex]::Escape($runtimeMarker)).Count -eq 1 -and $allocator.Count -eq 1
            if($variant -eq 'managed'){$complete=$dllExit -eq 0 -and $runExit -eq 0 -and [regex]::Matches($output,[regex]::Escape($managedMarker)).Count -eq 1}
            if($variant -eq 'producer'){
                $oom=[regex]::Matches($output,'Async producer OOM: ([1-9][0-9]*) actual allocation failures')
                if($oom.Count -eq 1){$trueOom=[int]$oom[0].Groups[1].Value}
                $complete=$runExit -eq 0 -and $trueOom -ge 20 -and [regex]::Matches($output,[regex]::Escape($producerMarker)).Count -eq 1
            }
            if($allocator.Count -eq 1){$allocations=[int]$allocator[0].Groups[1].Value}
        }
        $lanes += [pscustomobject]@{Name=$name;Build=$buildExit;DllBuild=$dllExit;Run=$runExit;Complete=$complete;RealAllocations=$allocations;ActualProducerOom=$trueOom}
        Write-Output "$name build=$buildExit run=$runExit complete=$complete allocations=$allocations"
    }}
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;IrMarker=$marker;RuntimeMarker=$runtimeMarker;ManagedMarker=$managedMarker;
        PerProgram=[pscustomobject]@{FrozenPhysicalTransitions=200;GeneratedLaunches=100;GeneratedWatchRetirements=200;
            BlockedCleanupSuccessorHandoffs=100;TrueOomRollbacks=100;ArmedOomFrees=100};
        ManagedPerProgram=[pscustomobject]@{Lifecycles=240;WatchOutcomes=80;GeneratedChains=50;CrossThreadFreezes=580;Refusals=820;FailureOutcomes=120;TrueOom=10;ActualDllUnloads=20};
        ProducerMarker=$producerMarker;
        ProducerPerProgram=[pscustomobject]@{GeneratedGraphs=100;Refusals=1040;RootRetries=30;SuccessorRollbacks=640;SuccessorHandoffs=640;CrossThreadFreezes=200;AllocationBudgets=330};
        Scope='Original physical-storage and managed-frame populations retained. Added stable produced-activation and exact Watch graph admission with real current-frame edges, Plan refresh across actual generated handoffs, root/successor retry and constant live chain storage under GCC O0/O2/TCC. Pending work remains externally rooted by its queue execution reference. Dependent SCC shutdown, combined module unloading, Linux and release acceptance remain open.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count){throw 'Async storage batch collected failures'}
}finally{Pop-Location}
