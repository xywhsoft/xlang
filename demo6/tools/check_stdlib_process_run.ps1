param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [switch]$FutureOnly)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior process_run evidence'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Full current-source compiler required'}
    if((Get-FileHash release/xl.dll).Hash-ne$build.RuntimeSha256){throw 'Source build runtime changed'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Source build changed'}}
    & ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $public
    if(!$?){throw 'Stage failed'}
    $stage=Get-Content "$public/artifact_sources.json" -Raw|ConvertFrom-Json
    $cases=@('stdlib_process_command_only','stdlib_process_command_config','stdlib_process_options_only','stdlib_process_run_bytes','stdlib_process_run_limits','stdlib_process_run_allocations','stdlib_process_shell_exact','xlir_process_exec_bridge','xlir_process_success_bridge','xlir_process_exit_code_bridge')
    $cases+=@('stdlib_process_support','stdlib_process_terminal_only','stdlib_process_open_only')
    $cases+=@('stdlib_process_live_wait','stdlib_process_live_streams','stdlib_process_live_drop','stdlib_process_live_allocations')
    $liveOomCases=@('stdlib_process_live_wait','stdlib_process_live_drop','stdlib_process_live_allocations')
    $cases+=@('stdlib_process_future','stdlib_process_future_drop','stdlib_process_future_cancel')
    $cases+=@('stdlib_process_future_allocations','stdlib_process_future_drop_allocations','stdlib_process_future_cancel_allocations')
    $futureOomCases=@('stdlib_process_future','stdlib_process_future_allocations','stdlib_process_future_drop_allocations','stdlib_process_future_cancel_allocations')
    $cases+=@('stdlib_process_pipeline_config','stdlib_process_pipeline_bytes','stdlib_process_pipeline_limits','stdlib_process_pipeline_allocations')
    $cases+=@('stdlib_process_file_config','stdlib_process_file_bytes','stdlib_process_file_allocations')
    $cases+=@('stdlib_process_terminal_config','stdlib_process_terminal_session','stdlib_process_terminal_allocations')
    $liveOomCases+=@('stdlib_process_terminal_allocations')
    $negatives=@('xlir_process_bad_array','xlir_process_bad_effect','xlir_process_bad_width','xlir_process_bad_owner')
    $negatives+=@('xlir_process_open_bad_width','xlir_process_open_bad_effect','xlir_process_terminal_bad_pure')
    $negatives+=@('xlir_process_live_bad_owner','xlir_process_live_bad_width','xlir_process_live_bad_effect','xlir_process_live_bad_stream')
    $negatives+=@('xlir_process_future_bad_element','xlir_process_future_bad_nullable','xlir_process_future_bad_effect','xlir_process_future_bad_owner')
    $negatives+=@('xlir_process_pipeline_bad_owner','xlir_process_pipeline_bad_width','xlir_process_pipeline_bad_effect','xlir_process_pipeline_bad_input')
    $negatives+=@('xlir_process_file_bad_owner','xlir_process_file_bad_width','xlir_process_file_bad_effect','xlir_process_file_bad_input')
    $negatives+=@('xlir_process_resize_bad_owner','xlir_process_resize_bad_width','xlir_process_resize_bad_effect')
    if($FutureOnly){
        $cases=@('stdlib_process_future','stdlib_process_future_drop','stdlib_process_future_cancel',
            'stdlib_process_future_allocations','stdlib_process_future_drop_allocations','stdlib_process_future_cancel_allocations')
        $negatives=@('xlir_process_future_bad_element','xlir_process_future_bad_nullable','xlir_process_future_bad_effect','xlir_process_future_bad_owner')
        $liveOomCases=@()
    }
    $extra=@($PSCommandPath,'tools/check_stdlib_process_run.sh','test/cases/xlir_process_run_host.c','test/cases/xlir_process_child.c',
        'test/support/process_native.c','test/cases/xlir_text_value_contract.c','test/cases/xlir_process_oom_host.c','test/cases/xlir_process_support_host.c',
        'test/cases/xlir_time_text_closure_host.c','test/cases/xlir_process_live_host.c','test/cases/xlir_process_io_loan.c','test/support/process_io_loan_probe.h',
        'test/cases/xlir_process_future_host.c','test/cases/xlir_process_future_oom_host.c','test/cases/xlir_process_live_oom_host.c','test/support/process_oom_observer.c',
        'test/cases/xlir_process_pipeline_prefix.c',
        'test/cases/xlir_process_pipeline_host.c',
        'test/cases/xlir_process_file_host.c',
        'test/cases/xlir_process_terminal_host.c',
        'tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',
        'release/xl.exe','release/xlc.dll','release/xl.dll','release/examples/stdlib_process_run.xl','release/examples/stdlib_process_support.xl','release/examples/stdlib_process_pipeline.xl',"$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($stage.Files.Source)+@($stage.Files.Path)+$extra
    $paths+=@($cases+$negatives|ForEach-Object {"test/cases/$_.xl"})
    $inputs=[ordered]@{}
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new(); $utf8=[Text.UTF8Encoding]::new($false)
    $child="$work/child.exe"
    function Check($name,$program,[string[]]$arguments,$error='') {
        $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{XL6_PROCESS_CHILD=$child}
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
        $null=Check 'child-build' gcc @('-std=c11','-Wall','-Wextra','-Werror','-municode','test/cases/xlir_process_child.c','-o',$child)
        $inputs[$child]=(Get-FileHash $child).Hash
        if(!$FutureOnly){
        $null=Check 'terminal-native-build' gcc ($flags+@('test/cases/xlir_process_terminal_host.c')+$libs+@('-o',"$work/terminal-native.exe"))
        $terminalNative=Check 'terminal-native' "$work/terminal-native.exe" @($child)
        if($terminalNative-notmatch 'terminal: real tty/merged stderr, child-observed resize, independent stream leases; 13 guards, heap/owned handles balanced; raw Win32 platform retention=[0-9]+, no extra handles'){throw 'Real terminal IO/resize/lifetime witness missing'}
        $null=Check 'file-native-build' gcc ($flags+@('test/cases/xlir_process_file_host.c')+$libs+@('-o',"$work/file-native.exe"))
        $fileNative=Check 'file-native' "$work/file-native.exe" @($child)
        if($fileNative-notmatch 'process file: independent non-inheritable handles, replacement/rollback, original close, binary child, 5 guards; OS error prefix [0-9]+ failures \+ 1 first no-hit, heap/handles balanced'){throw 'Process File ownership/error ledger witness missing'}
        $null=Check 'pipeline-prefix-build' gcc ($flags+@('-DXL6_PIPELINE_PREFIX_FROM_XLANG=1','test/cases/xlir_process_pipeline_prefix.c')+$libs+@('-o',"$work/pipeline-prefix.exe"))
        $pipelinePrefix=Check 'pipeline-prefix' "$work/pipeline-prefix.exe" @()
        if($pipelinePrefix-notmatch 'pipeline native prefixes: [0-9]+/[0-9]+ failures \+ 2 first no-hit, unrelated stdin intact, exact binary per-stage stderr, private waiters retired, balanced'){throw 'Pipeline physical-owner/setup prefix witness missing'}
        $null=Check 'pipeline-prefix-full-build' gcc ($flags+@('-DXRT_MODULE_ALL=1','-DXL6_PIPELINE_PREFIX_FROM_XLANG=1','test/cases/xlir_process_pipeline_prefix.c')+$libs+@('-o',"$work/pipeline-prefix-full.exe"))
        $pipelineFull=Check 'pipeline-prefix-full' "$work/pipeline-prefix-full.exe" @()
        if($pipelineFull-notmatch 'pipeline native prefixes: [0-9]+/[0-9]+ failures \+ 2 first no-hit, unrelated stdin intact, exact binary per-stage stderr, private waiters retired, balanced'){throw 'Full native Pipeline/Process Future retirement witness missing'}
        $null=Check 'live-native-build' gcc ($flags+@('test/cases/xlir_process_live_host.c')+$libs+@('-o',"$work/live-native.exe"))
        $live=Check 'live-native' "$work/live-native.exe" @($child)
        if($live-notmatch 'exact binary streams survive parent drop; unique leases, timeout/range/state, independent status; reader/writer OOM 3/3 failures \+ 2 first no-hit, balanced'){throw 'Live process ownership witness missing'}
        $null=Check 'io-loan-build' gcc ($flags+@('test/cases/xlir_process_io_loan.c')+$libs+@('-o',"$work/io-loan.exe"))
        $loan=Check 'io-loan' "$work/io-loan.exe" @()
        if($loan-notmatch 'process IO loan: 3 deterministic close races, 8 failure guards, short/EOF/zero/peer guards, balanced; safe=1'){throw 'Process IO retirement witness missing'}
        $arrayInputSources=@('src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c')
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_process_run_host.c','test/support/process_native.c','src/runtime/xl6_process_command.c','src/runtime/xl6_process_terminal_config.c','src/runtime/xl6_process_options.c','src/runtime/xl6_process_run.c','src/runtime/xl6_process_status.c')+$arrayInputSources+$libs+@('-o',"$work/native.exe"))
        $native=Check 'native' "$work/native.exe" @($child)
        foreach($witness in @('3 first no-hit completions, balanced','13 owner guards','projection OOM retry','concurrent cancellation and absolute deadline','native run complete prefix: [0-9]+ failures and 1 first no-hit, balanced')){if($native-notmatch$witness){throw 'Process native witness missing'}}
        $null=Check 'pipeline-resident-build' gcc ($flags+@('-DXRT_MODULE_PROCESS_PIPELINE=1','test/cases/xlir_process_pipeline_host.c','test/support/process_native.c','src/runtime/xl6_process_command.c','src/runtime/xl6_process_terminal_config.c','src/runtime/xl6_process_options.c','src/runtime/xl6_process_status.c','src/runtime/xl6_process_pipeline_config.c','src/runtime/xl6_process_pipeline.c')+$arrayInputSources+$libs+@('-o',"$work/pipeline-resident.exe"))
        $pipelineResident=Check 'pipeline-resident' "$work/pipeline-resident.exe" @($child)
        if($pipelineResident-notmatch 'pipeline resident prefixes: [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ failures \+ 5 first no-hit; snapshots, 5 owner guards, real collectors balanced' -or $pipelineResident-notmatch 'pipeline projections: [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ failures \+ 6 first no-hit; independent binary/status, cancellation/deadline, balanced'){throw 'Pipeline resident prefix witness missing'}
        $null=Check 'support-native-build' gcc ($flags+@('test/cases/xlir_process_support_host.c')+$libs+@('-o',"$work/support-native.exe"))
        $support=Check 'support-native' "$work/support-native.exe" @()
        if($support-notmatch '7 rejected inputs, exact prefix/clamp, native error transport; [0-9]+ allocation failures and 1 first no-hit, balanced; real terminal probe='){throw 'Process support native witness missing'}
        if($support-notmatch 'process open error prefix: 3 failures and 3 first no-hit, causes preserved, balanced'){throw 'Real process open error-build prefix missing'}
        }
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'tools/xrt_single_header_impl.c'-or$_-eq'src/api/xl_task_group_bridge.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
        $null=Check 'live-oom-observer-build' gcc ($flags+@('-include','tools/xrt_single_header_impl.c','-c','test/support/process_oom_observer.c','-o',"$work/live-oom-observer.o"))
        $retirementObjects=@($nativeObjects|Where-Object {[IO.Path]::GetFileName($_)-ne'xrt_single_header_impl.o'})+@("$work/live-oom-observer.o")
        $futureResident=@('xl6_code_lease','xl6_code_finalizer','xl6_continuation_queue','xl6_continuation_executor','xl6_continuation_future',
            'xl6_continuation_frame','xl6_future_wait','xl6_closure','xl6_cell','xl6_slot','xl6_frame_storage','xl6_task','xl6_capture',
            'xl6_value_iterator','xl6_future_callback','xl6_generator','xl6_future_payload')|ForEach-Object {Join-Path $public ($_.ToString()+'.o')}
        $null=Check 'future-native-build' gcc ($flags+@('test/cases/xlir_process_future_host.c')+$futureResident+$libs+@('-o',"$work/future-native.exe"))
        $future=Check 'future-native' "$work/future-native.exe" @($child)
        if($future-notmatch 'mapper/pending/terminal OOM 1/[0-9]+/[0-9]+ failures \+ 3 first no-hit, waiter retired, balanced'){throw 'Process Future native/OOM witness missing'}
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+$compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $source="test/cases/$case.xl";$prefix="$work/$case"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.repeat.c").Hash){throw 'Process run C nondeterministic'}
            $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.closed.c").Hash){throw 'Process run contract C mismatch'}
            if((Get-Content "$prefix.c" -Raw)-match 'xl6_xrt_process_|\bxrtProcess[A-Za-z]+\(|xl6_process_run_text'){throw 'Process run algorithm leaked into Cgen'}
            $strictHost=if($case-like'stdlib_process_live_*'-or$case-like'stdlib_process_future*'-or$case-like'stdlib_process_file*'){'test/cases/xlir_time_text_closure_host.c'}else{'test/cases/xlir_process_oom_host.c'}
            if($case-eq'stdlib_process_file_allocations'){$strictHost='test/cases/xlir_process_oom_host.c'}
            $strictObjects=$nativeObjects; $wrap=@()
            if($case-in$liveOomCases){$strictHost='test/cases/xlir_process_live_oom_host.c';$strictObjects=$retirementObjects;$wrap=@('-Wl,--wrap=xrtProcessSpawn')}
            if($case-in$futureOomCases){$strictHost='test/cases/xlir_process_future_oom_host.c';$strictObjects=$retirementObjects;$wrap=@('-Wl,--wrap=xrtProcessSpawn','-Wl,--wrap=xrtErrorBuild')}
            $null=Check "$case-strict-build" gcc ($flags+@('-include',"$prefix.c",$strictHost)+$strictObjects+$wrap+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            $null=Check "$case-formal-run" 'release/xl.exe' @('run',$source)
            $null=Check "$case-formal-build" 'release/xl.exe' @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
            if($case-in@('stdlib_process_run_allocations','stdlib_process_pipeline_allocations','stdlib_process_file_allocations')){
                $terminal=$false
                for($point=0;$point-lt512;++$point){
                    if((Check "$case-oom-$point" "$prefix.strict.exe" @("$point"))-match'hit=0 balanced'){$terminal=$true;break}
                }
                if(!$terminal){throw 'Process collector language allocation sweep incomplete'}
            }
            if($case-in$liveOomCases){
                $terminal=$false; $children=if($case-eq'stdlib_process_live_wait'){2}else{1}
                for($point=0;$point-lt512;++$point){
                    $witness=Check "$case-live-oom-$point" "$prefix.strict.exe" @("$point")
                    if($witness-match "hit=0 children=$children waiter retired balanced"){$terminal=$true;break}
                }
                if(!$terminal){throw 'Generated live Process current-thread allocation prefix incomplete'}
            }
            if($case-in$futureOomCases){
                $terminal=$false; $children=if($case-eq'stdlib_process_future'){2}else{1}
                for($point=0;$point-lt512;++$point){
                    $witness=Check "$case-future-oom-$point" "$prefix.strict.exe" @("$point")
                    if($witness-match'outcome=cancelled-diagnostic'-and$case-ne'stdlib_process_future_cancel_allocations'){throw 'Unexpected cancelled diagnostic fallback'}
                    if($witness-match "hit=0 children=$children waiter/executor retired balanced; outcome=success cancelled-diagnostic-failures=0"){$terminal=$true;break}
                }
                if(!$terminal){throw 'Generated Process Future current-thread allocation prefix incomplete'}
            }
        }
        foreach($case in $negatives){
            $prefix="$work/$case"
            $rejection=if($case-eq'xlir_process_file_bad_input'){'XLIR VFS resource parameter has no exact native field borrow'}else{'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'}
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) $rejection
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Unsafe process_run ABI produced output'}}
        }
        if(!$FutureOnly){
        $null=Check 'example-run' 'release/xl.exe' @('run','release/examples/stdlib_process_run.xl')
        $null=Check 'example-build' 'release/xl.exe' @('build','release/examples/stdlib_process_run.xl','-o',"$work/example.exe")
        $null=Check 'example-execute' "$work/example.exe" @()
        $null=Check 'support-example-run' 'release/xl.exe' @('run','release/examples/stdlib_process_support.xl')
        $null=Check 'support-example-build' 'release/xl.exe' @('build','release/examples/stdlib_process_support.xl','-o',"$work/support-example.exe")
        $null=Check 'support-example-execute' "$work/support-example.exe" @()
        $null=Check 'pipeline-example-run' 'release/xl.exe' @('run','release/examples/stdlib_process_pipeline.xl')
        $null=Check 'pipeline-example-build' 'release/xl.exe' @('build','release/examples/stdlib_process_pipeline.xl','-o',"$work/pipeline-example.exe")
        $null=Check 'pipeline-example-execute' "$work/pipeline-example.exe" @()
        }
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;Cases=$cases;Negatives=$negatives;FutureOnly=[bool]$FutureOnly;
            ChangedInputs=$changed;Inputs=$inputs;Results=$rows;Scope='Default checks Process configuration/run/live/Future/pipeline/File/terminal; FutureOnly checks six Process Future fixtures and four Future ABI negatives only, plus native mapper/pending/terminal factory prefixes. Both lanes check strict C, exact ABI/owner/effect, deterministic source/formal JIT/AOT and four generated Future current-thread logical allocation prefixes with real Spawn, private waiter/executor retirement, native factories and managed boxing. Future failure propagation and exact cancelled diagnostic OOM fallback checked. No other-thread allocation injection. Default terminal checks report raw Win32 platform retention and owned/no-extra handle closure, not global Windows zero-handle growth. Generated Process Future provider unload and 53-module total acceptance remain pending.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen process_run inputs changed'}
    }
    Write-Output "Process run Windows checks: $($rows.Count)"
} finally {Pop-Location}
