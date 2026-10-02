param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior signal_queue evidence'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Full current-source compiler required'}
    if((Get-FileHash release/xl.dll).Hash-ne$build.RuntimeSha256){throw 'Source build runtime changed'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Source build changed'}}
    & ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $public
    if(!$?){throw 'Stage failed'}
    $stage=Get-Content "$public/artifact_sources.json" -Raw|ConvertFrom-Json
    $cases=@('stdlib_signal_subscription_only','stdlib_signal_dispatch','stdlib_signal_dispatch_lifetime','stdlib_signal_dispatch_routes','stdlib_signal','sem_channel_class_element','stdlib_signal_core','stdlib_signal_core_only')
    $negatives=@('xlir_signal_queue_bad_count','xlir_signal_queue_bad_effect','xlir_signal_queue_bad_owner','xlir_signal_queue_bad_snapshot',
        'xlir_signal_bad_result','xlir_signal_bad_effect','xlir_signal_bad_width','xlir_signal_bad_name')
    $extra=@($PSCommandPath,'tools/check_stdlib_signal_queue.sh','test/cases/xlir_signal_queue_host.c',
        'test/support/signal_native.c','test/cases/xlir_text_value_contract.c','test/cases/xlir_signal_dispatch_host.c','test/cases/xlir_signal_closure_host.c',
        'release/examples/stdlib_signal.xl','tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',
        'release/xl.exe','release/xlc.dll','release/xl.dll',"$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($stage.Files.Source)+@($stage.Files.Path)+$extra
    $paths+=@($cases+$negatives+@('sem_channel_generic_class_element','sem_channel_class_graph_element')|ForEach-Object {"test/cases/$_.xl"})
    $inputs=[ordered]@{}
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new(); $utf8=[Text.UTF8Encoding]::new($false)
    function Check($name,$program,[string[]]$arguments,$error='') {
        $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{}
        $rows.Add([ordered]@{Name=$name;Program=$program;Arguments=$arguments;Result=$result})
        [IO.File]::WriteAllText("$work/$name.log",$result.stdout+$result.stderr,$utf8)
        if($error){if($result.status-ne'fail'-or$result.exitCode-ne1-or$result.stderr-notmatch$error){throw "$name rejection mismatch"}}
        elseif($result.status-ne'pass'){throw "$name failed: $($result.stderr) $($result.stdout)"}
        return $result.stdout
    }
    $complete=$false
    try {
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
            '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
            '-Wno-pointer-sign','-Wno-format-truncation',"-I$public/include",'-I.','-Irelease')
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_signal_queue_host.c','test/support/signal_native.c')+$libs+@('-o',"$work/native.exe"))
        $native=Check 'native' "$work/native.exe" @()
        foreach($witness in @('6 first no-hit completions, balanced','32 real once deliveries','10000 concurrent recorder events','inactive-once/in-flight-recorder race synchronized','close synchronously retires active recorder','spurious wakes do not extend','restore/ignore/shutdown end indefinite wait')){if($native-notmatch [regex]::Escape($witness)){throw 'Signal queue native sweep incomplete'}}
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'tools/xrt_single_header_impl.c'-or$_-eq'src/api/xl_task_group_bridge.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+$compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $source="test/cases/$case.xl";$prefix="$work/$case"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.repeat.c").Hash){throw 'Signal queue C nondeterministic'}
            if($case-ne'sem_channel_class_element'){
                $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
                if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.closed.c").Hash){throw 'Signal queue contract C mismatch'}
            }
            if((Get-Content "$prefix.c" -Raw)-match 'xl6_xrt_signal_|\bxrtSignal[A-Za-z]+\('){throw 'Signal queue algorithm leaked into Cgen'}
            $ledgerFlags=if($case-eq'sem_channel_class_element'){@('-DXL6_CHANNEL_LOGICAL_LEDGER')}else{@()}
            $null=Check "$case-strict-build" gcc ($flags+$ledgerFlags+@('-include',"$prefix.c",'test/cases/xlir_signal_dispatch_host.c')+$nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            $null=Check "$case-formal-run" 'release/xl.exe' @('run',$source)
            $null=Check "$case-formal-build" 'release/xl.exe' @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
            if($case-in@('stdlib_signal_dispatch','stdlib_signal','sem_channel_class_element','stdlib_signal_subscription_only')){
                $terminal=$false
                for($point=0;$point-lt512;++$point){
                    if((Check "$case-oom-$point" "$prefix.strict.exe" @("$point"))-match'hit=0 balanced'){$terminal=$true;break}
                }
                if(!$terminal){throw 'Signal queue language allocation sweep incomplete'}
            }
        }
        foreach($case in $negatives){
            $prefix="$work/$case"
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Unsafe signal_queue ABI produced output'}}
        }
        $null=Check 'example-run' 'release/xl.exe' @('run','release/examples/stdlib_signal.xl')
        $null=Check 'example-build' 'release/xl.exe' @('build','release/examples/stdlib_signal.xl','-o',"$work/example.exe")
        $null=Check 'example-execute' "$work/example.exe" @()
        $null=Check 'generic-class-reject' $build.Driver @('test/cases/sem_channel_generic_class_element.xl','emit',"$work/generic-class-reject") 'channel<T>'
        $null=Check 'class-graph-reject' $build.Driver @('test/cases/sem_channel_class_graph_element.xl','emit',"$work/class-graph-reject") 'channel<T>'
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;Cases=$cases;Negatives=$negatives;
            ChangedInputs=$changed;Inputs=$inputs;Results=$rows;Scope='Owner-thread explicit signal queue/pump: twenty-four closed managed bridges, allocation-free bounded recorder, coalescing/saturation, immutable prepare/snapshot/consume, constructor OOM, nine owner guards, once/close retirement and global disposition waits. Eight strict current-source/formal JIT/AOT cases cover typed/nullary on/once, closure self-close/reentrancy/errors, flat class channel backpressure and TaskGroup cancel entry. Three signal memory-kind logical ledgers plus one legacy channel rejection/ownership ledger (ChannelError may replace native OOM kind/cause). Generic and object-graph class messages rejected. No full TaskGroup/channel or 53-module completion claim.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen signal_queue inputs changed'}
    }
    Write-Output "Signal queue Windows checks: $($rows.Count)"
} finally {Pop-Location}
