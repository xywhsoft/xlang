param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior time evidence'}
    $build=Get-Content "$public/build.json" -Raw|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Full current-source compiler required'}
    if((Get-FileHash release/xl.dll).Hash-ne$build.RuntimeSha256){throw 'Source build runtime changed'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Source build changed'}}
    & ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $public
    if(!$?){throw 'Stage failed'}
    $stage=Get-Content "$public/artifact_sources.json" -Raw|ConvertFrom-Json
    $cases=@('stdlib_time_text','stdlib_time_protocol','stdlib_time_fields_text','stdlib_time_text_only','stdlib_time_runtime','stdlib_time_core_only','stdlib_time_calendar_only','stdlib_time_datetime')
    $negatives=@('xlir_time_text_bad_result','xlir_time_text_bad_owner','xlir_time_text_bad_effect','xlir_time_text_bad_width')
    $extra=@($PSCommandPath,'tools/check_stdlib_time_text.sh','test/cases/xlir_time_core_host.c','test/support/time_native.c',
        'test/cases/xlir_text_value_contract.c','test/cases/xlir_string_interpolation_host.c',
        'test/cases/xlir_time_text_closure_host.c','release/examples/stdlib_time_core.xl','release/examples/stdlib_time_calendar.xl',
        'test/cases/xlir_time_text_host.c','test/support/time_text_native.c','release/examples/stdlib_time.xl',
        'release/examples/stdlib_time_c.c','test/support/time_expect.c','test/cases/stdlib_time.xl','test/cases/stdlib_time_extra.xl',
        'tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',
        'release/xl.exe','release/xlc.dll','release/xl.dll',"$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($stage.Files.Source)+@($stage.Files.Path)+$extra
    $paths+=@($cases+$negatives|ForEach-Object {"test/cases/$_.xl"})
    $inputs=[ordered]@{}
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    New-Item -ItemType Directory -Path $work|Out-Null
    $rows=[Collections.Generic.List[object]]::new()
    $utf8=[Text.UTF8Encoding]::new($false)
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
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_time_text_host.c','test/support/time_text_native.c','src/runtime/xl6_time_text.c','src/runtime/xl6_time_datetime_text.c','src/runtime/xl6_time_datetime.c')+$libs+@('-o',"$work/native.exe"))
        if((Check 'native' "$work/native.exe" @())-notmatch '11 actual allocation failures, 11 first no-hit completions, balanced'){throw 'Text native error allocation sweep incomplete'}
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'tools/xrt_single_header_impl.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+$compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $source="test/cases/$case.xl";$prefix="$work/$case"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.repeat.c").Hash){throw 'Time C nondeterministic'}
            $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.closed.c").Hash){throw 'Time contract C mismatch'}
            $entry=if($case-eq'stdlib_time_runtime'){@('-DXL6_TIME_USER_ENTRY')}else{@()}
            $hostSource=if($entry.Count){'test/cases/xlir_time_text_closure_host.c'}else{'test/cases/xlir_string_interpolation_host.c'}
            $null=Check "$case-strict-build" gcc ($flags+$entry+@('-include',"$prefix.c",$hostSource)+$nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            $null=Check "$case-formal-run" 'release/xl.exe' @('run',$source)
            $null=Check "$case-formal-build" 'release/xl.exe' @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
            if($case-in@('stdlib_time_text','stdlib_time_protocol','stdlib_time_fields_text','stdlib_time_datetime')){
                $terminal=$false
                for($point=0;$point-lt1024;++$point){
                    if((Check "$case-oom-$point" "$prefix.strict.exe" @("$point"))-match'hit=0 balanced'){$terminal=$true;break}
                }
                if(!$terminal){throw 'Time language allocation sweep incomplete'}
            }
        }
        foreach($case in $negatives){
            $prefix="$work/$case"
            $error=if($case-eq'xlir_time_datetime_private'){'private'}elseif($case-eq'xlir_time_datetime_readonly'){'read-only|readonly|read only|not writable|cannot assign'}else{'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI|C ABI extern product requires a closed primitive product profile'}
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) $error
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Unsafe time ABI produced output'}}
        }
        $null=Check 'example-run' 'release/xl.exe' @('run','release/examples/stdlib_time_core.xl')
        $null=Check 'example-build' 'release/xl.exe' @('build','release/examples/stdlib_time_core.xl','-o',"$work/example.exe")
        $null=Check 'example-execute' "$work/example.exe" @()
        $null=Check 'calendar-example-run' 'release/xl.exe' @('run','release/examples/stdlib_time_calendar.xl')
        $null=Check 'calendar-example-build' 'release/xl.exe' @('build','release/examples/stdlib_time_calendar.xl','-o',"$work/calendar-example.exe")
        $null=Check 'calendar-example-execute' "$work/calendar-example.exe" @()
        $exampleOutput=Check 'text-example-run' 'release/xl.exe' @('run','release/examples/stdlib_time.xl')
        $null=Check 'text-example-build' 'release/xl.exe' @('build','release/examples/stdlib_time.xl','-o',"$work/text-example.exe")
        $null=Check 'text-example-execute' "$work/text-example.exe" @()
        $null=Check 'text-companion-build' gcc ($flags+@('-Ilib/xrt','release/examples/stdlib_time_c.c')+$nativeObjects+$libs+@('-o',"$work/text-companion.exe"))
        $companionOutput=Check 'text-companion-run' "$work/text-companion.exe" @()
        if($exampleOutput.Replace("`r`n","`n")-ne$companionOutput.Replace("`r`n","`n")){throw 'Text C companion output differs'}
        $null=Check 'legacy-helper-build' gcc ($flags+@('-c','test/support/time_expect.c','-o',"$work/legacy-helper.o"))
        foreach($case in @('stdlib_time','stdlib_time_extra')){
            $source="test/cases/$case.xl"; $prefix="$work/$case"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash "$prefix.closed.c").Hash){throw 'Legacy time contract C mismatch'}
            $null=Check "$case-strict-build" gcc ($flags+@('-include',"$prefix.c",'test/cases/xlir_time_text_closure_host.c',"$work/legacy-helper.o")+$nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-formal-run" 'release/xl.exe' @('run',$source,'test/support/time_expect.c')
            $null=Check "$case-formal-build" 'release/xl.exe' @('build',$source,'test/support/time_expect.c','-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
        }
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        if($changed.Count){$complete=$false}
        [IO.File]::WriteAllText("$work/results.json",([ordered]@{Complete=$complete;Checks=$rows.Count;Cases=$cases;Negatives=$negatives;
            ChangedInputs=$changed;Inputs=$inputs;Results=$rows;Scope='Time text/protocol/field text subset with core/calendar regression. Windows strict C, source and formal JIT/AOT, closed ABI mutations, real native OOM and language logical allocation ledger. Windows local roundtrip only, no DST fixture or sanitizer. Signal/process/containers and cross-group 53-module acceptance remain outside this gate.'}|ConvertTo-Json -Depth 10),$utf8)
        if($changed.Count){throw 'Frozen time inputs changed'}
    }
    Write-Output "Time text Windows checks: $($rows.Count)"
} finally {Pop-Location}
