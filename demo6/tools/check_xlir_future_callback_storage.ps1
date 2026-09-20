param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$ContractDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$rootPath=Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior Future callback evidence'}
    $public=Get-Content (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    $contracts=Get-Content (Join-Path $ContractDirectory 'build.json') -Raw | ConvertFrom-Json
    $runtime=@($public.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -eq 'xrt_single_header_impl.o'})
    if(!$public.SourceXrt -or $runtime.Count -ne 1){throw 'Current source XRT required'}
    $residentSources=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_ownership_plan.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c','src/runtime/xl6_future_callback.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c',
        'src/runtime/xl6_slot.c','src/runtime/xl6_frame_storage.c','src/runtime/xl6_continuation_frame.c','src/runtime/xl6_future_wait.c','src/runtime/xl6_task.c','src/runtime/xl6_generator.c','src/runtime/xl6_code_finalizer.c',
        'src/runtime/xl6_continuation_queue.c','src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_future_payload.c')
    $files=@(Get-Item -LiteralPath $PSCommandPath,$Tcc,'C:/Windows/System32/ucrtbase.dll',(Get-Command gcc).Source,
        'test/cases/xlir_class_release_isolation.c','test/cases/xlir_terminal_release_transitive.c','test/cases/xlir_future_producer_roots.c')+
        @(Get-ChildItem test/cases -File | Where-Object {$_.Name -match '^xlir_(future_callback_storage|future_continuation_abi_host|callback_boundary|pending_callback_preparation)' -or $_.Name -eq 'stdlib_future_continuation.xl'})
    $inputs=@($public.Inputs)+@($public.Headers)+@($runtime)+@($contracts.Inputs)+@($contracts.Drivers)+
        @($files | ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash -ne $row.Sha256){throw "Stale Future callback input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $outputPath | Out-Null
    $irMarker='Future callback storage IR: 3 fixtures, 15 Future continuations, 10 tasks, 125 rejected C/header-gated storage mutations; exact text/JSON ABI'
    $cases=@(
        @{Name='xlir_future_callback_storage';Driver='xlir_future_callback_storage_contract';Pattern=[regex]::Escape($irMarker)},
        @{Name='xlir_callback_boundary.future_only';Driver='xlir_callback_boundary_contract';Extra=@('--future-only');Pattern='callback boundary contract: 1 activations, 2 captures, 36 rejected mutations'})
    $emission=@();$deterministic=$true
    foreach($case in $cases){
        $driver=Join-Path $ContractDirectory ($case.Driver+'.exe')
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
    $pendingDriver=Join-Path $ContractDirectory 'xlir_code_owner_binding_contract.exe'
    foreach($pass in @('','-repeat')){
        $prefix="$outputPath/xlir_pending_callback_preparation$pass"
        & $pendingDriver test/cases/xlir_pending_callback_preparation.xl "$prefix.c" "$prefix.h" "$prefix.json" --script *> "$prefix.emit.log"
        $emission += [pscustomobject]@{Name='pending-callback-preparation';Pass=$pass;Exit=$LASTEXITCODE;Complete=($LASTEXITCODE -eq 0);Log="$prefix.emit.log"}
    }
    foreach($suffix in @('c','h','json')){
        $first="$outputPath/xlir_pending_callback_preparation.$suffix";$second="$outputPath/xlir_pending_callback_preparation-repeat.$suffix"
        if(!(Test-Path $first) -or !(Test-Path $second) -or (Get-FileHash $first).Hash -ne (Get-FileHash $second).Hash){$deterministic=$false}
    }
    $includes=@('-I.',"-I$([IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include')))","-I$outputPath")
    $strict=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation')
    $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32');$lanes=@();$dlls=@()
    $runtimeMarker='Resident Future callbacks: 100 pending graphs, 100 ready outcomes, 200 active/drop inspection refusals, 100 reverse cancellations, 100 allocation rollbacks, 100 armed-OOM discards, 1000 registration races, 100 real DLL parent/child continuations and closures after Close; no native root registration'
    $programs=@(
        @{Name='pending-preparation';Sources=@('test/cases/xlir_pending_callback_preparation_runtime.c','test/cases/xlir_pending_callback_preparation_artifact.c');Pattern='Pending callback preparation: 100 generated two-finally graphs, 50 external-result deferrals, 200 ordered callbacks before 100 class finalizers, complete trace/prepare/clear/finish and zero live delta; no fake roots or guard leases'},
        @{Name='producer-roots';Sources=@('test/cases/xlir_future_producer_roots.c');Pattern='Future producer roots: 200 output-only root graphs, 100 unobserved rootless cycles, 200 accepted callbacks and cleanups across four outcomes, exact physical edges and zero live delta; no guard leases or native roots, not pending graph collection'},
        @{Name='resident';Sources=@('test/cases/xlir_future_callback_storage_runtime.c');Pattern=[regex]::Escape($runtimeMarker)},
        @{Name='graph';Sources=@('test/cases/xlir_future_callback_storage_graph.c','test/cases/xlir_callback_boundary_future_only_host.c');Pattern='Generated Future callbacks: 100 pending complete graphs and four terminal outcomes, exactly 100 capture drops, zero live delta; no native roots'},
        @{Name='language';Sources=@('test/cases/xlir_future_callback_storage_language.c');Pattern='Original Future language fixture: all 10 continuation sites, complete cleanup, zero live delta'},
        @{Name='layout';Sources=@('test/cases/xlir_future_callback_storage_layout.c');Pattern='Closed capture layouts: 325 malformed layouts refused by both allocators, 100 pending and 50 ready Future callbacks, 50 accepted tasks, 100 allocation failures and 100 allocation-free rollbacks, 600 ordered finalizers, 400 cross-thread unfrozen active refusals; all six kinds and zero live delta'},
        @{Name='ordinary-value';Sources=@('test/cases/xlir_class_release_isolation.c');Arguments=@('0');Pattern='Ordinary release mode 0: finalizers=1 isolated=1, all threads joined and zero live delta'},
        @{Name='ordinary-closure';Sources=@('test/cases/xlir_class_release_isolation.c');Arguments=@('1');Pattern='Ordinary release mode 1: finalizers=1 isolated=1, all threads joined and zero live delta'},
        @{Name='transitive';Sources=@('test/cases/xlir_terminal_release_transitive.c');Pattern='Terminal release isolation: 21 routes x25, 526 actual finalizers, 528 cross-thread probes; containers/COW/cursors/cells/closure scopes/Future/callable bridges/deep clones, receiver/weak/error contracts, explicit native phases and caller-scope preservation; zero live delta'})
    foreach($compiler in @('gcc-O0','gcc-O2','tcc')){
        $dll="$outputPath/future-$compiler.dll"
        if($compiler -eq 'tcc'){& $Tcc -std=c11 -shared @includes test/cases/xlir_future_callback_storage_drop.c -o $dll *> "$dll.build.log"}
        else{& gcc @strict "-$($compiler.Substring(4))" -shared @includes test/cases/xlir_future_callback_storage_drop.c -o $dll *> "$dll.build.log"}
        $dlls += [pscustomobject]@{Name=$compiler;Build=$LASTEXITCODE;Path=$dll}
        foreach($program in $programs){
            $name="$($program.Name)-$compiler";$binary="$outputPath/$name.exe";$sources=@($program.Sources)+$residentSources
            if($compiler -eq 'tcc'){& $Tcc -std=c11 @includes @sources tools/xrt_single_header_impl.c C:/Windows/System32/ucrtbase.dll @libs -o $binary *> "$outputPath/$name.build.log"}
            else{& gcc @strict "-$($compiler.Substring(4))" @includes @sources $runtime[0].Path @libs -o $binary *> "$outputPath/$name.build.log"}
            $buildExit=$LASTEXITCODE;$runExit=$null;$complete=$false
            if($buildExit -eq 0){
                $options=@{FilePath=$binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;RedirectStandardOutput="$outputPath/$name.out.log";RedirectStandardError="$outputPath/$name.err.log"}
                if($program.Name -eq 'resident'){$options.ArgumentList=@('"'+$dll+'"')}
                elseif($program.Arguments){$options.ArgumentList=$program.Arguments}
                $process=Start-Process @options
                if(!$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();$runExit=-999}
                else{$process.Refresh();$runExit=$process.ExitCode}
                $process.Dispose();$complete=$runExit -eq 0 -and [regex]::Matches([IO.File]::ReadAllText("$outputPath/$name.out.log"),$program.Pattern).Count -eq 1
            }
            $lanes += [pscustomobject]@{Name=$name;Build=$buildExit;Run=$runExit;Complete=$complete;Output="$outputPath/$name.out.log";Error="$outputPath/$name.err.log"}
            Write-Output "$name build=$buildExit run=$runExit complete=$complete"
        }
    }
    $unchanged=@($inputs | Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256}).Count -eq 0
    $report=[pscustomobject]@{RecordedAt=(Get-Date).ToString('o');Inputs=$inputs;InputsUnchanged=$unchanged;Emission=$emission;Deterministic=$deterministic;Dlls=$dlls;Lanes=$lanes;
        IrMarker=$irMarker;RuntimeMarker=$runtimeMarker;Scope='One resident Future callback storage/registration/retirement implementation selected by verified callback storage ABI. Full then/catch/finally source fixture, generated pending graph, resident four outcomes/active/drop/cancel/OOM/races and real DLL child continuations/closures after Close. New generated two-finally owner graphs require normal notification before class finalization and external-result deferral. That additive graph host does not prove actual PE/TCC unmapping. No native roots or full native participation, Linux/release/default-backend claim.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report | ConvertTo-Json -Depth 7),[Text.UTF8Encoding]::new($false))
    if(!$unchanged -or !$deterministic -or @($emission | Where-Object {$_.Exit -ne 0 -or !$_.Complete}).Count -or
        @($dlls | Where-Object Build -ne 0).Count -or @($lanes | Where-Object {$_.Build -ne 0 -or $_.Run -ne 0 -or !$_.Complete}).Count){throw 'Future callback storage batch collected failures'}
}finally{Pop-Location}
