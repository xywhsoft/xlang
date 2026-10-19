param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string]$Tcc='E:/software/tcc/tcc.exe')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior Future cancellation evidence'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly-or!$build.SourceXrt){throw 'Full current-source compiler/XRT required'}
    if((Get-FileHash release/xl.dll).Hash-ne$build.RuntimeSha256){throw 'Formal runtime changed'}
    foreach($row in @($build.Inputs)+@($build.Headers)+@($build.Objects)){
        if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw "Stale source build: $($row.Path)"}
    }
    $resident=@('src/runtime/xl6_code_lease.c','src/runtime/xl6_code_finalizer.c','src/runtime/xl6_continuation_queue.c',
        'src/runtime/xl6_continuation_executor.c','src/runtime/xl6_continuation_future.c','src/runtime/xl6_continuation_frame.c',
        'src/runtime/xl6_future_wait.c','src/runtime/xl6_closure.c','src/runtime/xl6_cell.c','src/runtime/xl6_slot.c',
        'src/runtime/xl6_frame_storage.c','src/runtime/xl6_task.c','src/runtime/xl6_task_pool.c','src/runtime/xl6_capture.c','src/runtime/xl6_value_iterator.c',
        'src/runtime/xl6_future_callback.c','src/runtime/xl6_generator.c','src/runtime/xl6_future_payload.c')
    $extra=@($PSCommandPath,'tools/check_xlir_future_cancel_core.sh','tools/extlib_smoke_process.ps1',
        'test/cases/xlir_future_producer_cancel.c','test/cases/xlir_continuation_publication_contract.c',
        'test/cases/xlir_async_cancel_watch_contract.c','test/cases/xlir_async_cancel_watch.xl',
        'test/cases/xlir_continuation_owner_test.h','test/cases/xlir_async_cancel_wait_runtime.c',
        'test/cases/xlir_async_cancel_wait_host.c','release/xl.dll',(Get-Command gcc).Source,$Tcc,
        "$public/build.json")+$resident
    $inputs=[ordered]@{}
    foreach($path in @(@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+$extra|Sort-Object -Unique)){
        $resolved=(Resolve-Path -LiteralPath $path).Path;$inputs[$resolved]=(Get-FileHash $resolved).Hash
    }
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new();$complete=$false;$utf8=[Text.UTF8Encoding]::new($false)
    function Check($name,$program,[string[]]$arguments,$witness=''){
        $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{}
        $rows.Add([ordered]@{Name=$name;Program=$program;Arguments=$arguments;Result=$result})
        [IO.File]::WriteAllText("$work/$name.log",$result.stdout+$result.stderr,$utf8)
        if($result.status-ne'pass'){throw "$name failed: $($result.stdout) $($result.stderr)"}
        if($witness-and$result.stdout-notmatch[regex]::Escape($witness)){throw "$name incomplete witness"}
    }
    try {
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
            '-Wno-pointer-sign','-Wno-format-truncation','-I.',"-I$public/include","-I$work")
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $objects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_async_cancel_watch_contract.c')+$objects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($repeat in @('','.repeat')){
            Check "emit$repeat" "$work/contract.exe" @("$work/xlir_async_cancel_watch$repeat.c","$work/xlir_async_cancel_watch$repeat.h","$work/xlir_async_cancel_watch$repeat.json") '15 C/header-gated mutations'
        }
        foreach($suffix in @('c','h','json','h.bind.h')){
            if((Get-FileHash "$work/xlir_async_cancel_watch.$suffix").Hash-ne(Get-FileHash "$work/xlir_async_cancel_watch.repeat.$suffix").Hash){throw 'Cancellation output nondeterministic'}
        }
        $xrt=Join-Path $public 'xrt_single_header_impl.o'
        foreach($optimization in @('O0','O2')){
            Check "producer-$optimization-build" gcc ($flags+@("-$optimization",'test/cases/xlir_future_producer_cancel.c')+$libs+@('-o',"$work/producer-$optimization.exe"))
            Check "producer-$optimization-run" "$work/producer-$optimization.exe" @() '300 terminal-publication races, balanced'
            Check "publication-$optimization-build" gcc ($flags+@("-$optimization",'test/cases/xlir_continuation_publication_contract.c','src/runtime/xl6_code_lease.c','src/runtime/xl6_continuation_queue.c',$xrt)+$libs+@('-o',"$work/publication-$optimization.exe"))
            Check "publication-$optimization-run" "$work/publication-$optimization.exe" @() '400 two-thread commit/source/close handshakes'
            Check "generated-$optimization-build" gcc ($flags+@("-$optimization",'test/cases/xlir_async_cancel_wait_runtime.c','test/cases/xlir_async_cancel_wait_host.c')+$resident+@($xrt)+$libs+@('-o',"$work/generated-$optimization.exe"))
            Check "generated-$optimization-run" "$work/generated-$optimization.exe" @() 'Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries'
            if((Get-Content "$work/generated-$optimization-run.log" -Raw)-notmatch'Async cancel wait: 2106 generated interruptions, 300 three-thread') {throw 'Legacy cancellation population missing'}
        }
        Check 'generated-tcc-build' $Tcc (@('-std=c11','-I.',"-I$public/include","-I$work",'test/cases/xlir_async_cancel_wait_runtime.c','test/cases/xlir_async_cancel_wait_host.c')+$resident+@('tools/xrt_single_header_impl.c','C:/Windows/System32/ucrtbase.dll')+$libs+@('-o',"$work/generated-tcc.exe"))
        Check 'generated-tcc-run' "$work/generated-tcc.exe" @() 'Explicit Future cancellation: 150 public requests, 50 active-predecessor/provisional-successor handshakes, 200 native reentries'
        if((Get-Content "$work/generated-tcc-run.log" -Raw)-notmatch'Async cancel wait: 2106 generated interruptions, 300 three-thread'){throw 'TCC legacy population missing'}
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;ChangedInputs=$changed;Inputs=$inputs;Results=$rows;
            Scope='Explicit cooperative Future producer cancellation and outside-lock continuation publication. Exact bind/refusal/OOM/ambient/physical producer policy compatibility; 300 producer-terminal races and 400 commit/source/close handshakes per GCC lane. Canonical IR negative/determinism, existing 2106 owning-host interrupts/300 source-cancel-executor races and new 150 public requests/50 provisional-successor cancels per GCC O0/O2/TCC lane. No inline script, source cancellation or finally skipping. Not Process typed Future, pruning, module-unload, Linux or 53-module total acceptance.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen cancellation inputs changed'}
    }
    Write-Output "Future cancellation Windows checks: $($rows.Count)"
} finally {Pop-Location}
