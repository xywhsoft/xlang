param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
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
    $negatives=@('xlir_process_bad_array','xlir_process_bad_effect','xlir_process_bad_width','xlir_process_bad_owner')
    $negatives+=@('xlir_process_open_bad_width','xlir_process_open_bad_effect','xlir_process_terminal_bad_pure')
    $extra=@($PSCommandPath,'tools/check_stdlib_process_run.sh','test/cases/xlir_process_run_host.c','test/cases/xlir_process_child.c',
        'test/support/process_native.c','test/cases/xlir_text_value_contract.c','test/cases/xlir_process_oom_host.c','test/cases/xlir_process_support_host.c',
        'test/cases/xlir_time_text_closure_host.c','tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',
        'release/xl.exe','release/xlc.dll','release/xl.dll','release/examples/stdlib_process_run.xl','release/examples/stdlib_process_support.xl',"$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
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
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_process_run_host.c','test/support/process_native.c','src/runtime/xl6_process_command.c','src/runtime/xl6_process_options.c','src/runtime/xl6_process_run.c','src/runtime/xl6_process_status.c')+$libs+@('-o',"$work/native.exe"))
        $native=Check 'native' "$work/native.exe" @($child)
        foreach($witness in @('3 first no-hit completions, balanced','13 owner guards','projection OOM retry','concurrent cancellation and absolute deadline','native run complete prefix: [0-9]+ failures and 1 first no-hit, balanced')){if($native-notmatch$witness){throw 'Process native witness missing'}}
        $null=Check 'support-native-build' gcc ($flags+@('test/cases/xlir_process_support_host.c')+$libs+@('-o',"$work/support-native.exe"))
        $support=Check 'support-native' "$work/support-native.exe" @()
        if($support-notmatch '7 rejected inputs, exact prefix/clamp, native error transport; [0-9]+ allocation failures and 1 first no-hit, balanced; real terminal probe='){throw 'Process support native witness missing'}
        if($support-notmatch 'process open error prefix: 3 failures and 3 first no-hit, causes preserved, balanced'){throw 'Real process open error-build prefix missing'}
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'tools/xrt_single_header_impl.c'-or$_-eq'src/api/xl_task_group_bridge.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
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
            $null=Check "$case-strict-build" gcc ($flags+@('-include',"$prefix.c",'test/cases/xlir_process_oom_host.c')+$nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            $null=Check "$case-formal-run" 'release/xl.exe' @('run',$source)
            $null=Check "$case-formal-build" 'release/xl.exe' @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
            if($case-eq'stdlib_process_run_allocations'){
                $terminal=$false
                for($point=0;$point-lt512;++$point){
                    if((Check "$case-oom-$point" "$prefix.strict.exe" @("$point"))-match'hit=0 balanced'){$terminal=$true;break}
                }
                if(!$terminal){throw 'Process run language allocation sweep incomplete'}
            }
        }
        foreach($case in $negatives){
            $prefix="$work/$case"
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Unsafe process_run ABI produced output'}}
        }
        $null=Check 'example-run' 'release/xl.exe' @('run','release/examples/stdlib_process_run.xl')
        $null=Check 'example-build' 'release/xl.exe' @('build','release/examples/stdlib_process_run.xl','-o',"$work/example.exe")
        $null=Check 'example-execute' "$work/example.exe" @()
        $null=Check 'support-example-run' 'release/xl.exe' @('run','release/examples/stdlib_process_support.xl')
        $null=Check 'support-example-build' 'release/xl.exe' @('build','release/examples/stdlib_process_support.xl','-o',"$work/support-example.exe")
        $null=Check 'support-example-execute' "$work/support-example.exe" @()
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;Cases=$cases;Negatives=$negatives;
            ChangedInputs=$changed;Inputs=$inputs;Results=$rows;Scope='Process command/configuration, exact UTF-8 OS text, owned args/environment, run options/cancellation/deadline, native concurrent drains, binary result and independent status. Association adapter uses a controlled launcher (no GUI/OS launch acceptance claim); terminal capability probe uses real XRT. Strict C, closed ABI mutations, source/formal JIT/AOT and logical OOM. Live Process, pipeline, Future, redirection and PTY sessions remain pending; not the 53-module acceptance.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen process_run inputs changed'}
    }
    Write-Output "Process run Windows checks: $($rows.Count)"
} finally {Pop-Location}
