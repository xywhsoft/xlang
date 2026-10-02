param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
. "$root/tools/extlib_smoke_process.ps1"
Push-Location $root
try {
    $public=(Resolve-Path -LiteralPath $PublicSourceDirectory).Path
    $work=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $work){throw 'Preserve prior regex evidence'}
    $build=Get-Content "$public/build.json" -Raw -Encoding UTF8|ConvertFrom-Json
    if($build.RuntimeOnly){throw 'Full source compiler required'}
    foreach($row in $build.Inputs){if((Get-FileHash $row.Path).Hash-ne$row.Sha256){throw "Changed source: $($row.Path)"}}
    if(!(Test-Path "$public/artifact_sources.json")){& ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $public}
    $stage=Get-Content "$public/artifact_sources.json" -Raw -Encoding UTF8|ConvertFrom-Json
    foreach($row in $stage.Files){
        if((Get-FileHash $row.Source).Hash-ne$row.Sha256-or(Get-FileHash $row.Path).Hash-ne$row.Sha256){throw 'Stale staged source'}
    }
    New-Item -ItemType Directory -Path $work|Out-Null
    $utf8=[Text.UTF8Encoding]::new($false)
    $checks=[Collections.Generic.List[object]]::new()
    $inputs=[ordered]@{}
    $cases=@('stdlib_regex_core_only','stdlib_regex_match','stdlib_regex_errors','stdlib_regex_iterators',
        'stdlib_regex_replace_only','stdlib_regex_replace_buffer','stdlib_regex_replace_buffer_errors',
        'stdlib_regex_split_only','stdlib_regex_set_only','stdlib_regex_extended_errors','stdlib_regex_byte_boundaries')
    $negatives=@('xlir_regex_bad_result','xlir_regex_bad_owner','xlir_regex_bad_constructor','xlir_regex_bad_nullable',
        'xlir_regex_bad_set_items','xlir_regex_bad_set_hits','xlir_regex_bad_iterator_owner')
    $privacy=@('xlir_regex_private_consume','xlir_regex_private_split_consume')
    $undeclared=@('sem_native_class_extern_bad_type','sem_native_class_extern_return_bad')
    $extra=@($PSCommandPath,'tools/check_stdlib_regex.sh','test/cases/xlir_regex_host.c','test/support/regex_native.c',
        'test/cases/xlir_text_value_contract.c','test/cases/xlir_string_interpolation_host.c','test/cases/xlir_regex_extended_host.c',
        'tools/extlib_smoke_process.ps1','tools/stage_xlir_artifact_runtime_sources.ps1',
        "$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($stage.Files.Path)+@($stage.Files.Source)+$extra
    $paths+=@($cases+$negatives+$privacy+$undeclared|ForEach-Object {"test/cases/$_.xl"})
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    function Check($name,$program,[string[]]$arguments,$error='') {
        $result=Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{}
        $checks.Add([ordered]@{Name=$name;Program=$program;Arguments=$arguments;Result=$result})
        [IO.File]::WriteAllText("$work/$name.log",$result.stdout+$result.stderr,$utf8)
        if($error) {
            if($result.status-ne'fail'-or$result.exitCode-ne1-or$result.stderr-notmatch$error){throw "$name rejection mismatch: $($result.stderr)"}
        }elseif($result.status-ne'pass'){throw "$name failed: $($result.stderr) $($result.stdout)"}
        return $result.stdout
    }
    $complete=$false
    try {
        $flags=@('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
            '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
            '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',"-I$public/include",'-I.','-Irelease')
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_regex_host.c','test/support/regex_native.c',
            'src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_match.c')+$libs+@('-o',"$work/native.exe"))
        if((Check 'native' "$work/native.exe" @())-notmatch '68 actual failures, 5 first no-hit completions'){throw 'Native OOM oracle incomplete'}
        $null=Check 'extended-native-build' gcc ($flags+@('test/cases/xlir_regex_extended_host.c','test/support/regex_native.c',
            'src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_match.c','src/runtime/xl6_regex_iterator.c',
            'src/runtime/xl6_regex_replace.c','src/runtime/xl6_regex_replace_buffer.c','src/runtime/xl6_regex_split.c',
            'src/runtime/xl6_regex_set.c')+$libs+@('-o',"$work/extended-native.exe"))
        if((Check 'extended-native' "$work/extended-native.exe" @())-notmatch '222 actual failures, 9 first no-hit completions'){throw 'Extended native OOM oracle incomplete'}
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Sources|Where-Object {$_-like'src/runtime/*'-or$_-eq'tools/xrt_single_header_impl.c'}|
            ForEach-Object {Join-Path $public ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+
            $compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $prefix="$work/$case"; $source="test/cases/$case.xl"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            foreach($ext in @('c','h','json')){if((Get-FileHash "$prefix.$ext").Hash-ne(Get-FileHash "$prefix.repeat.$ext").Hash){throw 'Nondeterministic regex output'}}
            $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            $null=Check "$case-default" $build.Driver @($source,'cgen-default',"$prefix.default.c")
            if((Get-FileHash "$prefix.closed.c").Hash-ne(Get-FileHash "$prefix.default.c").Hash){throw 'Closed regex emission differs'}
            $generated=Get-Content "$prefix.c" -Raw -Encoding UTF8
            if($generated-match '(?m)^\s*(?:static\s+)?(?:void\s*\*|xstrview|int64_t|bool)\s*xl6Regex\w+\([^;]*\)\s*\{'-or
                $generated-match 'xl6_xrt_take_cstr|xrtRegexMatcher(?:Find|Capture)\('){throw 'Cgen contains regex implementation'}
            $null=Check "$case-strict-build" gcc ($flags+@('-include',"$prefix.c",'test/cases/xlir_string_interpolation_host.c')+
                $nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            if($case-in@('stdlib_regex_match','stdlib_regex_iterators','stdlib_regex_replace_only',
                'stdlib_regex_replace_buffer','stdlib_regex_split_only','stdlib_regex_set_only')) {
                $terminal=$false
                for($point=0;$point-lt2048;++$point) {
                    $output=Check "$case-oom-$point" "$prefix.strict.exe" @("$point")
                    if($output-match 'hit=0 balanced'){$terminal=$true;break}
                }
                if(!$terminal){throw 'Regex language OOM sweep did not reach first no-hit'}
            }
        }
        foreach($case in $negatives) {
            $prefix="$work/$case"
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Unsafe regex declaration emitted output'}}
        }
        foreach($case in $privacy) {
            $prefix="$work/$case"
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) 'unknown class/union field|private'
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Private native member emitted output'}}
        }
        foreach($case in $undeclared) {
            $prefix="$work/$case"
            $null=Check $case $build.Driver @("test/cases/$case.xl",'emit',$prefix) 'does not support typed container direct C ABI'
            foreach($ext in @('c','h','json')){if(Test-Path "$prefix.$ext"){throw 'Undeclared native container ABI emitted output'}}
        }
        $complete=$true
    } finally {
        $changed=@($inputs.Keys|Where-Object {(Get-FileHash $_).Hash-ne$inputs[$_]})
        $report=[ordered]@{Complete=($complete-and!$changed.Count);Checks=$checks.Count;Cases=$cases;Negatives=$negatives;Privacy=$privacy;UndeclaredArrays=$undeclared;
            ChangedInputs=$changed;Inputs=$inputs;Results=$checks;
            Scope='B13/B14 compile/match/all/template replace/transactional Buffer/lazy split/set; not function replacement, structured regex error facade, Pattern or whole-task release acceptance'}
        [IO.File]::WriteAllText("$work/results.json",($report|ConvertTo-Json -Depth 8),$utf8)
        if($changed.Count){throw 'Regex gate inputs changed'}
    }
    Write-Output "Regex B13/B14: $($checks.Count) checks passed"
}finally{Pop-Location}
