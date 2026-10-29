param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
. (Join-Path $PSScriptRoot 'xlir_resident_closure.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try{
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve prior executor evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw|ConvertFrom-Json
    $runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Current source runtime required'}
    $native=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_ownership_plan.c')
    $generated=@('test/cases/xlir_async_executor_ownership_runtime.c','test/cases/xlir_async_executor_ownership_host.c')+$native+@(
        'src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c',
        'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c',
        'src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    $programs=@(
        [pscustomobject]@{Name='queue';Sources=@('test/cases/xlir_continuation_queue_contract.c')+$native;Marker='continuation queue:'},
        [pscustomobject]@{Name='executor';Sources=@('test/cases/xlir_continuation_executor_contract.c')+$native;Marker='continuation executor: 100 isolated deliveries, 300 real Future Watch handoffs, 150 blocking/limit/fault cases, 50 multiworker saturation cases, 50 compensation faults,'},
        [pscustomobject]@{Name='ownership';Sources=@('test/cases/xlir_executor_ownership_contract.c')+$native;Marker='Executor ownership: 120 real parked graphs, 60 external queue roots, 120 policy refusals, 20 retained joined shells, 40 run/release refusals, 40 host acquire/drop shutdown refusals,'},
        [pscustomobject]@{Name='generated';Sources=$generated;Marker='Generated executor ownership: 60 real worker-driven rings, 20 feeders, 320 executor/queue/frame/Watch refusals, 40 cleanup-await resurrections; native roots respected, normal catch/finally, joined workers and exact memory balance'}
    )
    $files=@(Get-Item -LiteralPath $PSCommandPath,$ContractDriver,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_async_dependency_cycle_contract.c','test/cases/xlir_async_dependency_cycle.xl')+@(
        $programs|ForEach-Object {$_.Sources}|Sort-Object -Unique|ForEach-Object {Get-Item -LiteralPath $_})
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count){throw 'Stale executor inputs'}
    New-Item -ItemType Directory -Path $out|Out-Null
    $closure=Get-XlirResidentClosure -Root $rootPath -Directory "$out/resident-closure" -Sources @($generated|Where-Object {$_-like'src/*'}) -NativeProvider 'tools/xrt_single_header_impl.c' -PublicDirectory $PublicDirectory -Fixtures @('test/cases/xlir_async_dependency_cycle.xl')
    $programs[3].Sources=@($generated|Where-Object {$_-notlike'src/*'})+@($closure.Sources)
    $inputs+=@($closure.Inputs)
    $marker='Async dependency IR: verified producer and resident cancellation, 2 C/header-gated mutations, original catch/finally resume CFG'
    $emission=@();$deterministic=$true
    foreach($repeat in @('','.repeat')){
        $prefix="$out/xlir_async_dependency_cycle$repeat"
        & $ContractDriver "$prefix.c" "$prefix.bind.h" "$prefix.json" *> "$prefix.emit.log"
        $code=$LASTEXITCODE;$log=[IO.File]::ReadAllText("$prefix.emit.log")
        $emission+=[pscustomobject]@{Pass=$repeat;Exit=$code;Complete=[regex]::Matches($log,[regex]::Escape($marker)).Count-eq1}
    }
    foreach($suffix in @('c','bind.h','json')){
        if(!(Test-Path "$out/xlir_async_dependency_cycle.$suffix")-or!(Test-Path "$out/xlir_async_dependency_cycle.repeat.$suffix")-or
            (Get-FileHash "$out/xlir_async_dependency_cycle.$suffix").Hash-ne(Get-FileHash "$out/xlir_async_dependency_cycle.repeat.$suffix").Hash){$deterministic=$false}
    }
    $includes=@("-I$([IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include')))","-I$out",'-I.','-Irelease','-Irelease/include')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@()
    foreach($program in $programs){foreach($configuration in @('gcc-O0','gcc-O2','tcc')){
        $name="$($program.Name)-$configuration";$binary="$out/$name.exe";$sources=$program.Sources
        if($configuration-eq'tcc'){
            $build=Invoke-XlirTccToLog $Tcc (@('-std=c11')+$includes+@($sources)+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$out/$name.build.log"
        }else{
            $build=Invoke-XlirGccToLog ($strict+@("-$($configuration.Substring(4))")+$includes+@($sources)+@($runtime[0].Path)+$libs+@('-o',$binary)) "$out/$name.build.log"
        }
        $run=$null;$complete=$false;$oom=$null
        if($build-eq0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath `
                -RedirectStandardOutput "$out/$name.out.log" -RedirectStandardError "$out/$name.err.log"
            $null = $process.Handle
            if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
            $process.Dispose();$log=[IO.File]::ReadAllText("$out/$name.out.log")
            $complete=$run-eq0-and[regex]::Matches($log,[regex]::Escape($program.Marker)).Count-eq1
            if($program.Name-eq'ownership'){
                $match=[regex]::Matches($log,'([1-9][0-9]*) real snapshot OOM failures; exact wait-slot references, cooperative close/join, zero detached workers and balanced memory')
                if($match.Count-eq1){$oom=[int]$match[0].Groups[1].Value}
                $complete=$complete-and$oom-gt20
            }
        }
        $lanes+=[pscustomobject]@{Name=$name;Build=$build;Run=$run;Complete=$complete;ActualOom=$oom}
        Write-Output "$name build=$build run=$run complete=$complete oom=$oom"
    }}
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;
        PerProgram=[pscustomobject]@{ParkedGraphs=120;ExternalQueueRoots=60;PolicyRefusals=120;RetainedShells=20;RunReleaseRefusals=40;NativeEntryRefusals=40;AllocationBudgets=4800;
            GeneratedRings=60;GeneratedFeeders=20;GeneratedRefusals=320;CleanupResurrections=40};
        Scope='Actual executor/queue wait-slot ownership, native and real worker-driven generated activation lifecycles. All original native queue/executor programs run unchanged under GCC O0/O2/TCC. Whole-graph inspection refuses running/entry/retirement phases; independently rooted queues prevent shutdown; join precedes resource retirement, retained shells remain real allocations. Host bindings select a real resident executor, not a manually driven queue. Generated module service ABI/trace integration and combined module unload, mixed producers, necessary native/network and full platform/release acceptance remain open.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or!$deterministic-or@($emission|Where-Object {$_.Exit-ne0-or!$_.Complete}).Count-or
        @($lanes|Where-Object {$_.Build-ne0-or$_.Run-ne0-or!$_.Complete}).Count){throw 'Executor batch collected failures'}
}finally{Pop-Location}
