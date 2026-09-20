param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior task-storage evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$public.SourceXrt -or $runtime.Count -ne 1){throw 'Current source XRT required'}
    $residentSources=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c',
        'src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source)+
        @(Get-ChildItem test/cases -File | Where-Object {$_.Name -match '^xlir_(task_storage|task_capture_layout|activation_capture_layout|task_closed_capture|value_iterator_cycle|dynamic_iteration_allocation|owner_fault|task_outcome_whitebox|module_task_pool|async_pool_outcome|callback_boundary|launch_test)'})
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash -ne $row.Sha256){throw "Stale task-storage input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $cases=@(
        @{Name='xlir_task_storage';Pattern='Task storage IR: 8 fixtures, [1-9][0-9]* rejected C/header-gated mutations, [1-9][0-9]* async parameter traces, [1-9][0-9]* callback capture traces, [1-9][0-9]* task pool modules; exact storage/text/JSON ABI'},
        @{Name='xlir_task_closed_capture';Pattern='Closed async capture IR: 6 exact physical captures, no state-machine fallback, 6 C/header-gated destroy mutations'},
        @{Name='xlir_dynamic_iteration_allocation';Pattern='dynamic iteration: 4 key \+ 1 logical checked projections, 2 cells, 6 construction effects, [1-9][0-9]* exit prefixes, two resident cursors, [1-9][0-9]* C/header-gated mutations'},
        @{Name='xlir_module_task_pool';Pattern='module pool: 9 ABI partitions, [1-9][0-9]* rejected IR mutations, canonical resource dump'},
        @{Name='xlir_async_pool_outcome';Pattern='pool outcome contract:'},
        @{Name='xlir_callback_boundary';Pattern='callback boundary contract: 5 activations, 12 captures, 188 rejected mutations'},
        @{Name='xlir_callback_boundary.future_only';Driver='xlir_callback_boundary_contract';Extra=@('--future-only');Pattern='callback boundary contract: 1 activations, 2 captures, 36 rejected mutations'})
    $emission=@();$deterministic=$true
    foreach($case in $cases){
        $driver=Join-Path $ContractDirectory ($(if($case.Driver){$case.Driver}else{$case.Name+'_contract'})+'.exe')
        if(!(Test-Path -LiteralPath $driver)){throw "Missing task contract: $driver"}
        foreach($repeat in @('','.repeat')){
            $prefix="$outputPath/$($case.Name)$repeat"
            & $driver "$prefix.c" "$prefix.h" "$prefix.json" @($case.Extra) *> "$prefix.emit.log"
            $exitCode=$LASTEXITCODE
            $complete=[regex]::Matches([IO.File]::ReadAllText("$prefix.emit.log"),$case.Pattern).Count -eq 1
            $emission += [pscustomobject]@{Name=$case.Name;Pass=$repeat;Exit=$exitCode;Complete=$complete;Log="$prefix.emit.log"}
        }
        foreach($suffix in @('c','h','json','h.bind.h','json.txt')){
            $first="$outputPath/$($case.Name).$suffix";$second="$outputPath/$($case.Name).repeat.$suffix"
            if($suffix -in @('h.bind.h','json.txt') -and !(Test-Path -LiteralPath $first) -and !(Test-Path -LiteralPath $second)){continue}
            if(!(Test-Path -LiteralPath $first) -or !(Test-Path -LiteralPath $second) -or
                (Get-FileHash -LiteralPath $first).Hash -ne (Get-FileHash -LiteralPath $second).Hash){$deterministic=$false}
        }
    }
    $includes=@('-I.',"-I$([IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include')))","-I$outputPath")
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@();$dlls=@()
    $runtimeMarker='Resident tasks: 200 queued physical graphs, 200 executing refusals, 100 blocked-drop refusals, 100 frozen notification graphs and callback-tail joins across four outcomes, 200 synchronous cancellation self-join refusals, 100 allocation rollbacks, 100 armed-OOM discards, 100 actual DLL lifetimes with after-Close child closures; no native root registration'
    $programs=@(
        @{Name='activation-layout';Sources=@('test/cases/xlir_activation_capture_layout_runtime.c');
            Pattern='Activation capture layouts: 256 sparse owning masks, 300 exact-order first-failure transactions, 1028 detached callback freezes; poisoned loans untouched, repeated cleanup and armed OOM allocation-free, exact memory balance'},
        @{Name='resident';Sources=@('test/cases/xlir_task_storage_runtime.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');Pattern=[regex]::Escape($runtimeMarker)},
        @{Name='closed-layout';Sources=@('test/cases/xlir_task_capture_layout_runtime.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');
            Pattern='Closed task captures: 9 physical kinds, 100 reverse cleanups and cross-thread freezes, 400 resident iterator snapshots, 12 OOM budgets; exact borrowed views, real pins and balanced allocations'},
        @{Name='generated-closed';Sources=@('test/cases/xlir_task_closed_capture_host.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');
            Pattern='Generated closed async captures: 100 success/failure transactions and 24 OOM budgets; actual six-slot V2 layout, pre-publication cleanup and balanced allocations'},
        @{Name='dynamic-iteration';Sources=@('test/cases/xlir_dynamic_iteration_allocation_runtime.c','test/cases/xlir_dynamic_iteration_allocation_host.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');
            Pattern='dynamic iteration: [1-9][0-9]* executions, [1-9][0-9]* real projection/cell allocation faults \([1-9][0-9]* after pending resume\), [1-9][0-9]* generator early/exhausted closes, [1-9][0-9]* balanced allocations'},
        @{Name='iterator-cycle';Sources=@('test/cases/xlir_value_iterator_cycle.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_ownership_plan.c');
            Pattern='Managed iterator cycles: 75 complete six-node/seven-edge graphs, 100 real-root deferrals, 75 missing-child refusals, 1800 OOM budgets and [1-9][0-9]* actual refusals, 75 finalizers, 75 context drops and 150 cross-thread freezes; failure retry and native resurrection without duplicate finalization, exact memory balance'},
        @{Name='ownership';Sources=@('test/cases/xlir_task_storage_ownership.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_ownership_plan.c');
            Pattern='Task ownership lifecycle: 200 complete retired raw/language Job graphs, 200 active-pool refusals, 400 real-root deferrals, 400 queued BUSY preparations, 200 normal outcomes, 400 class finalizers, 150 active refusals and 400 finalizer freeze probes; prepare/finalize/clear/finish with exact allocator balance'},
        @{Name='module-pool';Sources=@('test/cases/xlir_module_task_pool_runtime.c','test/cases/xlir_module_task_pool_host.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');
            Pattern='module pool: generated quiescence precedes global cleanup; raising cleanup, retry, reactivation and callback join passed'},
        @{Name='pool-outcome';Sources=@('test/cases/xlir_async_pool_outcome_runtime.c','test/cases/xlir_async_pool_outcome_host.c','test/cases/xlir_task_outcome_whitebox.c');
            Pattern='pool outcome runtime: [1-9][0-9]* executions, [1-9][0-9]* reverse cancellations, [1-9][0-9]* pre-submit faults, [1-9][0-9]* balanced allocations'},
        @{Name='callback';Sources=@('test/cases/xlir_callback_boundary_runtime.c','test/cases/xlir_callback_boundary_host.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');
            Pattern='callback boundary runtime: [1-9][0-9]* executions, [1-9][0-9]* pre-callback cancellations, [1-9][0-9]* allocation faults, [1-9][0-9]* balanced allocations'},
        @{Name='future-only';Sources=@('test/cases/xlir_callback_boundary_future_only_runtime.c','test/cases/xlir_callback_boundary_future_only_host.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c');Pattern='callback cleanup raising=1 state=[0-9]+ code=77 drops=1 thread_error=0'})
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $dll="$outputPath/task-$compiler.dll"
        if($compiler -eq 'tcc'){$dllBuild=Invoke-XlirNativeToLog $Tcc (@('-std=c11','-shared')+$includes+@('test/cases/xlir_task_storage_drop.c','-o',$dll)) "$dll.build.log"}
        else{$dllBuild=Invoke-XlirNativeToLog gcc ($strict+@("-$($compiler.Substring(4))",'-shared')+$includes+@('test/cases/xlir_task_storage_drop.c','-o',$dll)) "$dll.build.log"}
        $dlls += [pscustomobject]@{Name=$compiler;Build=$dllBuild;Path=$dll}
        foreach($program in $programs){
            $name="$($program.Name)-$compiler";$binary="$outputPath/$name.exe";$sources=@($program.Sources)+$residentSources
            if($compiler -eq 'tcc'){$buildExit=Invoke-XlirNativeToLog $Tcc (@('-std=c11')+$includes+$sources+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"}
            else{$buildExit=Invoke-XlirNativeToLog gcc ($strict+@("-$($compiler.Substring(4))")+$includes+$sources+@($runtime[0].Path)+$libs+@('-o',$binary)) "$outputPath/$name.build.log"}
            $runExit=$null;$complete=$false
            if($buildExit -eq 0){
                $options=@{FilePath=$binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;RedirectStandardOutput="$outputPath/$name.out.log";RedirectStandardError="$outputPath/$name.err.log"}
                if($program.Name -eq 'resident'){$options.ArgumentList=@('"'+$dll+'"')}
$process=Start-Process @options
                $null = $process.Handle
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$runExit=-999}
                else{$process.Refresh();$runExit=$process.ExitCode}
                $process.Dispose();$complete=$runExit -eq 0 -and [regex]::Matches([IO.File]::ReadAllText("$outputPath/$name.out.log"),$program.Pattern).Count -eq 1
                if($program.Name-eq'resident'){$complete=$complete-and[bool](Select-String -LiteralPath "$outputPath/$name.out.log" -SimpleMatch 'Task pool inspection: 200 complete parked-worker graphs plus 200 independently retained active-pool refusals' -Quiet)}
            }
            $lanes += [pscustomobject]@{Name=$name;Build=$buildExit;Run=$runExit;Complete=$complete;Output="$outputPath/$name.out.log";Error="$outputPath/$name.err.log"}
            Write-Output "$name build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Emission=$emission;Deterministic=$deterministic;Dlls=$dlls;Lanes=$lanes;
        RuntimeMarker=$runtimeMarker;TaskOwnership=[pscustomobject]@{RetiredGraphsPerProgram=200;ActivePoolRefusalsPerProgram=200;ParkedGraphsPerResidentProgram=200;ParkedRawNodes=7;ParkedRawEdges=9;ParkedLanguageNodes=9;ParkedLanguageEdges=15;RootDeferralsPerProgram=400;QueuedBusyPerProgram=400;OutcomesPerProgram=200;ClassFinalizersPerProgram=400;ActiveRefusalsPerProgram=150;FinalizerFreezeProbesPerProgram=400};
        Scope='Resident physical task frames and actual owning pool/native edges; exact IR capture trace/destroy vectors and V2 closed task storage. All original module pool/outcome/callback/future-only lifecycle and failure populations retained in GCC O0/O2 and source TCC/UCRT. An active same-pool blocker now correctly refuses whole-graph inspection: retain those 200 active transactions and add 200 complete physically parked queued graphs. The original 200 raw/language Job, outcome and class-finalizer transactions keep real explicit held-reference bundles through quiescence, then verify complete retired graphs, 400 root deferrals, preparation/finalization and shared Plan commit/Finish. No borrowed pool pointer is omitted or fabricated as a leaf. All nine capture kinds, six generated async captures, dynamic iteration allocation and managed iterator cycle populations remain. The 25 direct outcome bridge probes now own real idle pool shells through frame/activity tails. No source-vs-release substitution, native graph root registration, release rebuild, Linux or full CLI/JIT/AOT acceptance claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($dlls | Where-Object Build -ne 0).Count -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count){throw 'Task storage batch collected failures'}
}finally{Pop-Location}
