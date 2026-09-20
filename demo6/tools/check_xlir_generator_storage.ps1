param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [switch]$ResidentOnly,[string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior generator storage evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$public.SourceXrt -or $runtime.Count -ne 1){throw 'Current source XRT required'}
    $residentSources=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_generator.c','src/runtime/xl6_ownership_plan.c','src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c',
        'src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c',
        'src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,$public.Driver)+
        @(Get-ChildItem test/cases -File | Where-Object {$_.Name -match '^xlir_(generator_|async_.*launch_allocation|owner_fault_runtime|launch_test)'})
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($inputRecord in $inputs){if((Get-FileHash -LiteralPath $inputRecord.Path).Hash -ne $inputRecord.Sha256){throw "Stale generator input: $($inputRecord.Path)"}}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $cases=@(@{Name='xlir_generator_storage';Kind='resident';Contract='xlir_generator_storage_contract';
        Pattern='Resident generators: 200 generated freezes, 200 active control/frame refusals, 200 real OOM rollbacks, 100 armed-OOM transfers, 100 actual DLL lifetimes with after-Close child leases; exact graphs and allocator balance, no native root registration'})
    if(!$ResidentOnly){
        $cases+=@(
            @{Name='xlir_generator_close_routes';Kind='close';Contract='xlir_generator_close_routes_contract';Pattern='generator close routes: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_close_extended';Kind='close';Contract='xlir_generator_close_routes_contract';SourceArgument=$true;Pattern='extended close: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_close_abrupt_finally';Kind='close';Contract='xlir_generator_close_abrupt_finally_contract';Pattern='abrupt finally close: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_close_catch_dispatch';Kind='close';Contract='xlir_generator_close_catch_dispatch_contract';Pattern='catch dispatch close: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_close_first_outcome';Kind='close';Contract='xlir_generator_close_first_outcome_contract';Binding=$true;Pattern='generator close first outcome: 900 source executions, 2700 repeated-close sequences, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_close_item_outcome';Kind='custom';Contract='xlir_generator_payload_contract';Pattern='generator owned item close: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_frame_loan';Kind='loan';Contract='xlir_generator_frame_loan_contract';Pattern='generator frame loan: [1-9][0-9]* executions, [1-9][0-9]* balanced allocations; private string/object observations, shared updates, natural completion/early close, destructor errors and finally'},
            @{Name='xlir_generator_frame_loan.baseline';Fixture='xlir_generator_frame_loan';Kind='loan';Baseline=$true;Contract='xlir_generator_frame_loan_contract';Pattern='generator frame loan: [1-9][0-9]* executions, [1-9][0-9]* balanced allocations; private string/object observations, shared updates, natural completion/early close, destructor errors and finally'},
            @{Name='xlir_generator_item_object';Kind='basic';Entry='generator_item_object';Expected=16},
            @{Name='xlir_generator_item_array';Kind='basic';Entry='generator_item_array';Expected=18},
            @{Name='xlir_generator_item_callable';Kind='basic';Entry='generator_item_callable';Expected=7},
            @{Name='xlir_generator_item_dynamic';Kind='basic';Entry='generator_item_dynamic';Expected=16},
            @{Name='xlir_generator_item_future';Kind='basic';Entry='generator_item_future';Expected=1},
            @{Name='xlir_generator_item_dynamic_capture';Kind='basic';Entry='generator_item_dynamic_capture';Expected=7},
            @{Name='xlir_generator_payload_lifetime';Kind='custom';Pattern='generator payload lifetime: [1-9][0-9]* executions, 0 semantic failures; memory checks balanced'},
            @{Name='xlir_generator_item_error';Kind='basic';Entry='payload_probe';Expected=2},
            @{Name='xlir_generator_item_nested';Kind='basic';Entry='payload_probe';Expected=7},
            @{Name='xlir_generator_item_nullable_int';Kind='basic';Entry='payload_probe';Expected=7},
            @{Name='xlir_generator_item_nullable_object';Kind='basic';Entry='payload_probe';Expected=7},
            @{Name='xlir_generator_launch_allocation';Kind='allocation'},
            @{Name='xlir_generator_yield_allocation';Kind='allocation'},
            @{Name='xlir_async_launch_allocation';Kind='allocation';Ready=$true},
            @{Name='xlir_async_pool_launch_allocation';Kind='custom';Pattern='async pool launch: 30 module lifetimes, 20 real pre-submission allocation failures, caught/propagated outcomes and balanced memory'})
    }
    $cases+=@(@{Name='xlir_generator_managed';Kind='managed';Contract='xlir_generator_managed_contract';
        Pattern='Generated generator cycles: 140 complete six-node/seven-edge transactions, 280 finally steps, 140 finalizers, 140 context drops, 560 cross-thread freezes, 180 real-root deferrals, 140 missing-frame refusals, 3360 OOM budgets and [1-9][0-9]* actual refusals; 40 preparation failures, 20 finalizer failures, native resurrection and independently pinned frames; exact memory balance'})
    $emission=@();$deterministic=$true
    foreach($case in $cases){
        if(!$case.Fixture){$case.Fixture=$case.Name}
        if(!$case.Contract){$case.Contract='xlir_generator_payload_contract'}
        $driver=[IO.Path]::GetFullPath((Join-Path $ContractDirectory "$($case.Contract).exe"))
        if(@($contracts.Drivers | Where-Object Path -eq $driver).Count -ne 1){throw "Unrecorded generator contract $driver"}
        foreach($repeat in @('','.repeat')){
            $prefix="$outputPath/$($case.Name)$repeat"
            $arguments=@("$prefix.c","$prefix.h","$prefix.json")
            if($case.Kind -in @('resident','managed')){$arguments=@("$prefix.c","$prefix.bind.h","$prefix.json")}
            elseif($case.Contract -eq 'xlir_generator_payload_contract'){$arguments=@("test/cases/$($case.Fixture).xl")+$arguments}
            elseif($case.SourceArgument){$arguments+=@("test/cases/$($case.Fixture).xl")}
            elseif($case.Baseline){$arguments+=@('--baseline')}
            $code=Invoke-XlirNativeToLog $driver $arguments "$prefix.emit.log"
            $emission += [pscustomobject]@{Name=$case.Name;Pass=$repeat;Exit=$code;Complete=($code -eq 0);Log="$prefix.emit.log"}
        }
        $suffixes=if($case.Kind -in @('resident','managed')){@('c','bind.h','json')}else{@('c','h','json')}
        if($case.Binding){$suffixes+=@('h.bind.h')}
        if($case.Contract -eq 'xlir_generator_payload_contract'){$suffixes+=@('h.launch.h')}
        foreach($suffix in $suffixes){
            $a="$outputPath/$($case.Name).$suffix";$b="$outputPath/$($case.Name).repeat.$suffix"
            if(!(Test-Path $a) -or !(Test-Path $b) -or (Get-FileHash $a).Hash -ne (Get-FileHash $b).Hash){$deterministic=$false}
        }
        $prefix=("$outputPath/$($case.Name)").Replace('\','/')
        $body=@();$case.Arguments=@()
        switch($case.Kind){
            'resident' {$case.Sources=@('test/cases/xlir_generator_storage_runtime.c','test/cases/xlir_generator_storage_host.c')}
            'managed' {$case.Sources=@('test/cases/xlir_generator_managed_runtime.c','test/cases/xlir_generator_managed_host.c')}
            'loan' {
                $body=@(('#define XLIR_GENERATOR_LOAN_SOURCE "'+$prefix+'.c"'),'#include "test/cases/xlir_generator_frame_loan_host.c"')
                $case.Sources=@('test/cases/xlir_generator_frame_loan_runtime.c')
            }
            'basic' {
                $body=@(('#define XLIR_GENERATOR_ITEM_SOURCE "'+$prefix+'.c"'),"#define XLIR_GENERATOR_ITEM_ENTRY $($case.Entry)",
                    "#define XLIR_GENERATOR_ITEM_EXPECTED $($case.Expected)",'#include "test/cases/xlir_generator_item_basic_host.c"')
                $case.Sources=@('test/cases/xlir_generator_item_basic_runtime.c');$case.Pattern='generator item basic: [1-9][0-9]* executions passed; memory checks balanced'
            }
            'allocation' {
                $body=@(('#define XLIR_LAUNCH_ALLOCATION_SOURCE "'+$prefix+'.c"'),"#define XLIR_LAUNCH_EXPECT_BODY $(if($case.Ready){0}else{1})",
                    ('#include "'+$prefix+'.h.launch.h"'),'#include "test/cases/xlir_generator_launch_allocation_host.c"')
                $case.Sources=@('test/cases/xlir_generator_launch_allocation_runtime.c');$case.Pattern='activation allocation: [1-9][0-9]* runs, [1-9][0-9]* real allocation failures, caught/propagated MemoryError and balanced memory'
            }
            default {
                $body=@('#define XLIR_GENERATOR_STORAGE_SOURCE "'+$prefix+'.c"')
                if($case.Binding){$body+=@('#define XLIR_GENERATOR_STORAGE_BINDING "'+$prefix+'.h.bind.h"')}
                $body+=@('#include "test/cases/'+$case.Fixture+'_host.c"');$case.Sources=@("test/cases/$($case.Fixture)_runtime.c")
            }
        }
        if($body.Count){
            $hostSource="$prefix.host.c"
            [IO.File]::WriteAllText($hostSource,($body -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
            $case.Sources+=@($hostSource)
        }
    }
    $cases+=@(@{Name='xlir_generator_managed_lifetime';Fixture='xlir_generator_managed_lifetime';Kind='managed-lifetime';
        Sources=@('test/cases/xlir_generator_managed_lifetime.c');
        Pattern='Managed generator lifetimes: 150 last-owner reentrant closes, 150 sparse payload drops, 100 actual DLL unloads with separately held control/frame descriptors, 400 cross-thread freezes, 100 real OOM failures, 250 ABI refusals; exact memory balance'})
    $includes=@('-I.',('-I'+[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))),"-I$outputPath")
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@();$runtimeBuilds=@()
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $objects=@();$level=if($compiler -eq 'gcc-O0'){'-O0'}else{'-O2'}
        if($compiler -ne 'tcc'){
            foreach($source in $residentSources){
                $object="$outputPath/$compiler-$([IO.Path]::GetFileNameWithoutExtension($source)).o"
                $objectExit=Invoke-XlirNativeToLog gcc ($strict+@($level)+$includes+@('-c',$source,'-o',$object)) "$object.log"
                $runtimeBuilds+=[pscustomobject]@{Compiler=$compiler;Source=$source;Exit=$objectExit;Object=$object}
                $objects+=@($object)
            }
        }
        $dll="$outputPath/$compiler-callback.dll"
        $dllBuild=Invoke-XlirNativeToLog gcc ($strict+@($level)+$includes+@('-shared','test/cases/xlir_generator_storage_drop.c','-o',$dll)) "$dll.log"
        foreach($case in $cases){
            $name="$($case.Name)-$compiler";$binary="$outputPath/$name.exe";$timer=[Diagnostics.Stopwatch]::StartNew()
            if($compiler -eq 'tcc'){
                $buildExit=Invoke-XlirNativeToLog $Tcc (@('-std=c11')+$includes+@($case.Sources)+$residentSources+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
            }else{
                $buildExit=Invoke-XlirNativeToLog gcc ($strict+@($level)+$includes+@($case.Sources)+$objects+@($runtime[0].Path)+$libs+@('-o',$binary)) "$outputPath/$name.build.log"
            }
            $runExit=$null;$complete=$false;$marker=''
            if($buildExit -eq 0 -and ($case.Kind -notin @('resident','managed-lifetime') -or $dllBuild -eq 0)){
                $options=@{FilePath=$binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;
                    RedirectStandardOutput="$outputPath/$name.out.log";RedirectStandardError="$outputPath/$name.err.log"}
                if($case.Kind -in @('resident','managed-lifetime')){$options.ArgumentList=@('"'+$dll+'"')}
$process=Start-Process @options
                $null = $process.Handle
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$runExit=-999}
                else{$process.Refresh();$runExit=$process.ExitCode}
                $process.Dispose();$output=[IO.File]::ReadAllText("$outputPath/$name.out.log")
                $matches=[regex]::Matches($output,$case.Pattern);$complete=$runExit -eq 0 -and $matches.Count -eq 1
                if($case.Kind -eq 'resident'){
                    $complete=$complete -and [regex]::Matches($output,'Generator storage allocator: [1-9][0-9]* real allocations, zero live delta').Count -eq 1
                }
                if($matches.Count -eq 1){$marker=$matches[0].Value}
            }
            $timer.Stop();$lanes+=[pscustomobject]@{Name=$name;Fixture=$case.Fixture;Compiler=$compiler;Build=$buildExit;Run=$runExit;
                Complete=$complete;Marker=$marker;Seconds=[Math]::Round($timer.Elapsed.TotalSeconds,3)}
            Write-Output "$name build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;
        ResidentOnly=[bool]$ResidentOnly;Cases=$cases.Count;Emission=$emission;Deterministic=$deterministic;RuntimeBuilds=$runtimeBuilds;Lanes=$lanes;
        PerResidentProgram=[pscustomobject]@{GeneratedFreezes=200;ActiveControlAndFrameRefusals=200;RealOomRollbacks=200;ArmedOomTransfers=100;DllLifetimes=100};
        PerManagedProgram=[pscustomobject]@{GeneratedCycles=140;FinallySteps=280;Finalizers=140;RealRootDeferrals=180;MissingFrameRefusals=140;OomBudgets=3360;PrepareFailures=40;FinalizerFailures=20};
        PerManagedLifetimeProgram=[pscustomobject]@{LastOwnerReentrantCloses=150;PayloadDrops=150;DllLifetimes=100;CrossThreadFreezes=400;RealOomFailures=100;AbiRefusals=250};
        Scope='Original resident generator control/frame/item storage, generated freeze probes and actual DLL after-Close callback tails retained. Added generated V2 control/frame/object cycles through the production shared Plan, source finally before object finalization, explicit failure and native resurrection, independent real control/frame pins and actual DLL descriptor lifetime. Unless ResidentOnly is true, all original payload, close, allocation and both loan populations run unchanged on GCC O0/O2 and source TCC/UCRT. This does not prove arbitrary-child admission, combined generated-module unloading, release/JIT/AOT/Linux or complete language acceptance.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($runtimeBuilds | Where-Object Exit -ne 0).Count -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count){throw 'Generator storage batch collected failures; retain every original case'}
}finally{Pop-Location}
