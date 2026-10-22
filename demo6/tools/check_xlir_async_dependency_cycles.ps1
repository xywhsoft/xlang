param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDriver,
    [Parameter(Mandatory=$true)][string]$Directory,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try{
    $out=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $out){throw 'Preserve prior dependency evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $contracts=Get-Content (Join-Path (Split-Path -Parent $ContractDriver) 'build.json') -Raw|ConvertFrom-Json
    $runtime=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-eq'xrt_single_header_impl.o'})
    if(!$public.SourceXrt-or$runtime.Count-ne1){throw 'Current source runtime required'}
    $sources=@('test/cases/xlir_async_dependency_cycle_runtime.c','test/cases/xlir_async_dependency_cycle_host.c',
        'src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c',
        'src/runtime/xl6_ownership_plan.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c',
        'src/runtime/xl6_frame_storage.c','src/runtime/xl6_task.c','src/runtime/xl6_task_frame.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$ContractDriver,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_async_dependency_cycle_contract.c','test/cases/xlir_async_dependency_cycle.xl')+@(Get-Item -LiteralPath $sources)
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    if(@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count){throw 'Stale dependency inputs'}
    New-Item -ItemType Directory -Path $out|Out-Null
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
    $includes=@("-I$([IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include')))","-I$out",'-I.')
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@()
    foreach($configuration in @('gcc-O0','gcc-O2','tcc')){
        $binary="$out/$configuration.exe"
        if($configuration-eq'tcc'){
            & $Tcc -std=c11 @includes @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$out/$configuration.build.log"
        }else{
            & gcc @strict "-$($configuration.Substring(4))" @includes @sources $runtime[0].Path @libs -o $binary *> "$out/$configuration.build.log"
        }
        $build=$LASTEXITCODE;$run=$null;$complete=$false;$oom=0
        if($build-eq0){
$process=Start-Process -FilePath $binary -WindowStyle Hidden -PassThru -WorkingDirectory $rootPath `
                -RedirectStandardOutput "$out/$configuration.out.log" -RedirectStandardError "$out/$configuration.err.log"
            $null = $process.Handle
            if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$run=-999}else{$process.Refresh();$run=$process.ExitCode}
            $process.Dispose();$log=[IO.File]::ReadAllText("$out/$configuration.out.log")
            $match=[regex]::Matches($log,'Async dependency cycles: 120 generated one/two/three-producer rings, 40 runnable acyclic chains, 40 cycle feeders, 800 exact queue/storage/frame/Watch refusals, 160 authorized cyclic interruptions, ([1-9][0-9]*) real snapshot OOM failures; external roots preserved, ready work and feeders not cancelled, normal catch/finally and cleanup-await resurrection, exact queue/frame/owner retirement and memory balance')
            if($match.Count-eq1){$oom=[int]$match[0].Groups[1].Value}
            $complete=$run-eq0-and$match.Count-eq1-and$oom-gt20
        }
        $lanes+=[pscustomobject]@{Name=$configuration;Build=$build;Run=$run;Complete=$complete;ActualOom=$oom}
        Write-Output "$configuration build=$build run=$run complete=$complete oom=$oom"
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        Emission=$emission;Deterministic=$deterministic;Lanes=$lanes;
        PerProgram=[pscustomobject]@{GeneratedCycles=120;RunnableAcyclicChains=40;CycleFeeders=40;ExactRefusals=800;AuthorizedShutdowns=160;AllocationBudgets=2400};
        Scope='Actual generated one/two/three-producer await rings, runnable acyclic chains and cycle feeders on an owning host-driven queue. Real queue owning slots plus frozen wait-for edges and exact output identity are required: ownership unreachability alone does not authorize cancellation. One cycle member is interrupted; ready work and noncyclic feeders complete through normal outcomes. Original external observers/host references, optional cleanup-await resurrection, per-node admission and exact physical retirement are preserved. Executor/worker graph integration, mixed producer dependencies, combined generated module unloading, Linux and full release acceptance remain open.'}
    [IO.File]::WriteAllText("$out/results.json",($report|ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged-or!$deterministic-or@($emission|Where-Object {$_.Exit-ne0-or!$_.Complete}).Count-or
        @($lanes|Where-Object {$_.Build-ne0-or$_.Run-ne0-or!$_.Complete}).Count){throw 'Dependency batch collected failures'}
}finally{Pop-Location}
