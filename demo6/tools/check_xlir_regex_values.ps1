param([Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [switch]$PatternOnly,
    [string]$FormalDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$formal=if($FormalDirectory){(Resolve-Path -LiteralPath $FormalDirectory).Path}else{Join-Path $root 'release'}
$formalCli=Join-Path $formal 'xl.exe'
. "$root/tools/extlib_smoke_process.ps1"
. "$root/tools/xlir_native_process.ps1"
. "$root/tools/xlir_resident_closure.ps1"
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
        'stdlib_regex_split_only','stdlib_regex_set_only','stdlib_regex_extended_errors','stdlib_regex_byte_boundaries',
        'stdlib_regex_replace_func','stdlib_regex_replace_func_buffer','stdlib_regex_replace_func_errors','stdlib_regex_error_metadata',
        'stdlib_pattern_core_only','stdlib_pattern_options','stdlib_pattern_options_only','stdlib_pattern_match','stdlib_pattern_errors','stdlib_pattern_tags','stdlib_pattern_error_modes',
        'stdlib_pattern_builder','stdlib_pattern_builder_lifetime','stdlib_pattern_builder_cleanup')
    $negatives=@('xlir_regex_bad_result','xlir_regex_bad_owner','xlir_regex_bad_constructor','xlir_regex_bad_nullable',
        'xlir_regex_bad_set_items','xlir_regex_bad_set_hits','xlir_regex_bad_iterator_owner',
        'xlir_regex_bad_callback_arity','xlir_regex_bad_callback_result','xlir_regex_bad_callback_raw','xlir_regex_bad_error_input',
        'xlir_pattern_bad_result','xlir_pattern_bad_tag','xlir_pattern_bad_options','xlir_pattern_bad_builder_tags',
        'xlir_pattern_bad_edit_items','xlir_pattern_bad_edit_payload','xlir_pattern_bad_key_owner')
    $privacy=@('xlir_regex_private_consume','xlir_regex_private_split_consume','xlir_pattern_private_code','xlir_pattern_private_builder')
    $undeclared=@('sem_native_class_extern_bad_type','sem_native_class_extern_return_bad')
    if($PatternOnly){
        $cases=@($cases|Where-Object {$_-like'stdlib_pattern_*'})
        $negatives=@($negatives|Where-Object {$_-like'xlir_pattern_*'})
        $privacy=@($privacy|Where-Object {$_-like'xlir_pattern_*'})
        $undeclared=@()
    }
    $nativeGroups=[ordered]@{
        native=@('src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_match.c')
        extended=@('src/runtime/xl6_regex_core.c','src/runtime/xl6_regex_match.c','src/runtime/xl6_regex_iterator.c',
            'src/runtime/xl6_regex_replace.c','src/runtime/xl6_regex_replace_buffer.c','src/runtime/xl6_regex_split.c',
            'src/runtime/xl6_regex_set.c','src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c')
        pattern=@('src/runtime/xl6_pattern_core.c','src/runtime/xl6_pattern_options.c','src/runtime/xl6_pattern_match.c')
        builder=@('src/runtime/xl6_pattern_builder.c','src/runtime/xl6_pattern_core.c','src/runtime/xl6_pattern_options.c',
            'src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c')
    }
    $nativeClosures=@{}
    foreach($entry in $nativeGroups.GetEnumerator()){
        if($PatternOnly-and$entry.Key-in@('native','extended')){continue}
        $nativeClosures[$entry.Key]=Get-XlirResidentClosure -Root $root -Directory "$work/closure-$($entry.Key)" -Sources $entry.Value
    }
    $extra=@($PSCommandPath,'tools/check_stdlib_regex.sh','test/cases/xlir_regex_host.c','test/support/regex_native.c',
        'test/cases/xlir_pattern_host.c','test/support/pattern_native.c','test/cases/xlir_pattern_builder_host.c','test/support/pattern_builder_native.c','test/cases/xlir_error_projection_contract.c','test/cases/xlir_continuation_owner_test.h','test/cases/xlir_synthetic_owner_edit.h',
        'test/cases/xlir_text_value_contract.c','test/cases/xlir_string_interpolation_host.c','test/cases/xlir_regex_extended_host.c','test/cases/xlir_regex_callback_host.c',
        'test/cases/xlir_constructor_flow_solver.c','test/cases/xlir_constructor_definite_assignment_contract.c',
        'test/cases/xlir_constructor_definite_assignment.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','tools/xlir_resident_closure.ps1',
        'tools/stage_xlir_artifact_runtime_sources.ps1',
        $formalCli,(Join-Path $formal 'xlc.dll'),(Join-Path $formal 'xl.dll'),'release/xl.dll',
        "$public/build.json","$public/artifact_sources.json",$build.Driver,(Get-Command gcc).Source)
    $paths=@($build.Inputs.Path)+@($build.Headers.Path)+@($build.Objects.Path)+@($stage.Files.Path)+@($stage.Files.Source)+$extra
    $paths+=@($nativeClosures.Values|ForEach-Object {$_.Inputs.Path})
    $paths+=@($cases+$negatives+$privacy+$undeclared|ForEach-Object {"test/cases/$_.xl"})
    foreach($path in @($paths|ForEach-Object {(Resolve-Path -LiteralPath $_).Path}|Sort-Object -Unique)){$inputs[$path]=(Get-FileHash $path).Hash}
    function Check($name,$program,[string[]]$arguments,$error='') {
        $result=if($program-eq'gcc'){
            Invoke-XlirGccToLog $arguments "$work/$name.log" -PassThru -TimeoutSeconds 120
        }else{Invoke-ExtlibSmokeProcess $program $arguments $root 120 @{}}
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
            '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',"-I$public/include",'-I.','-Irelease','-Irelease/include')
        $libs=@('-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
        if(!$PatternOnly){
        $null=Check 'native-build' gcc ($flags+@('test/cases/xlir_regex_host.c','test/support/regex_native.c')+
            $nativeClosures.native.Sources+$libs+@('-o',"$work/native.exe"))
        if((Check 'native' "$work/native.exe" @())-notmatch '68 actual failures, 5 first no-hit completions'){throw 'Native OOM oracle incomplete'}
        $null=Check 'extended-native-build' gcc ($flags+@('test/cases/xlir_regex_extended_host.c','test/support/regex_native.c')+
            $nativeClosures.extended.Sources+$libs+@('-o',"$work/extended-native.exe"))
        if((Check 'extended-native' "$work/extended-native.exe" @())-notmatch '[1-9][0-9]* actual failures, 9 first no-hit completions'){throw 'Extended native OOM oracle incomplete'}
        }
        $null=Check 'pattern-native-build' gcc ($flags+@('test/cases/xlir_pattern_host.c','test/support/pattern_native.c')+
            $nativeClosures.pattern.Sources+$libs+@('-o',"$work/pattern-native.exe"))
        if((Check 'pattern-native' "$work/pattern-native.exe" @())-notmatch '60 actual failures, 5 first no-hit completions'){throw 'Pattern native OOM oracle incomplete'}
        $null=Check 'pattern-builder-native-build' gcc ($flags+@('test/cases/xlir_pattern_builder_host.c','test/support/pattern_builder_native.c')+
            $nativeClosures.builder.Sources+$libs+@('-o',"$work/pattern-builder-native.exe"))
        if((Check 'pattern-builder-native' "$work/pattern-builder-native.exe" @())-notmatch '[1-9][0-9]* actual failures, 10 first no-hit completions'){throw 'Builder adapter OOM oracle incomplete'}
        $compilerObjects=@($build.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
        $nativeObjects=@($build.Objects|Where-Object {
            $source=$_.Source.Replace('\','/')
            $source-match'(?:^|/)src/runtime/'-or$source-match'(?:^|/)(?:tools/xrt_single_header_impl|src/api/xl_task_group_bridge)\.c$'
        }|ForEach-Object Path)
        Copy-Item release/xl.dll "$work/xl.dll"
        if(!$PatternOnly){
        $null=Check 'callback-native-build' gcc ($flags+@('test/cases/xlir_regex_callback_host.c')+$compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/callback-native.exe"))
        $callbackOutput=Check 'callback-native' "$work/callback-native.exe" @()
        if ($callbackOutput-notmatch 'regex callback native OOM: 133 actual failures, 3 first no-hit completions, balanced') {throw 'Callback native OOM oracle incomplete'}
        }
        $null=Check 'contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+
            $compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/contract.exe"))
        $null=Check 'constructor-flow-solver-build' gcc ($flags+@('test/cases/xlir_constructor_flow_solver.c','-o',"$work/constructor-flow-solver.exe"))
        $null=Check 'constructor-flow-solver' "$work/constructor-flow-solver.exe" @()
        $null=Check 'constructor-assignment-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_constructor_definite_assignment_contract.c')+
            $compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/constructor-assignment.exe"))
        $null=Check 'constructor-assignment' "$work/constructor-assignment.exe" @('test/cases/xlir_constructor_definite_assignment.xl')
        $null=Check 'error-projection-contract-build' gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_error_projection_contract.c')+
            $compilerObjects+@('release/xl.dll')+$libs+@('-o',"$work/error-projection-contract.exe"))
        $null=Check 'error-projection-contract' "$work/error-projection-contract.exe" @('test/cases/stdlib_pattern_tags.xl',"$work/error-projection.closed.c")
        $null=Check 'error-projection-modes-contract' "$work/error-projection-contract.exe" @('test/cases/stdlib_pattern_error_modes.xl',"$work/error-projection-modes.closed.c")
        Copy-Item release/xl.dll "$work/xl.dll"
        foreach($case in $cases) {
            $prefix="$work/$case"; $source="test/cases/$case.xl"
            $null=Check "$case-emit" $build.Driver @($source,'emit',$prefix)
            if($case-in@('stdlib_pattern_tags','stdlib_pattern_error_modes')){
                $closed=if($case-eq'stdlib_pattern_tags'){"$work/error-projection.closed.c"}else{"$work/error-projection-modes.closed.c"}
                if((Get-FileHash "$prefix.c").Hash-ne(Get-FileHash $closed).Hash){throw 'Error projection contract changed generated C'}
            }
            $null=Check "$case-repeat" $build.Driver @($source,'emit',"$prefix.repeat")
            foreach($ext in @('c','h','json')){if((Get-FileHash "$prefix.$ext").Hash-ne(Get-FileHash "$prefix.repeat.$ext").Hash){throw 'Nondeterministic regex output'}}
            $null=Check "$case-contract" "$work/contract.exe" @($source,"$prefix.closed.c")
            $null=Check "$case-default" $build.Driver @($source,'cgen-default',"$prefix.default.c")
            if((Get-FileHash "$prefix.closed.c").Hash-ne(Get-FileHash "$prefix.default.c").Hash){throw 'Closed regex emission differs'}
            $generated=Get-Content "$prefix.c" -Raw -Encoding UTF8
            if($generated-match '(?m)^\s*(?:static\s+)?(?:void\s*\*|xstrview|int64_t|bool)\s*xl6(?:Regex|Pattern)\w+\([^;]*\)\s*\{'-or
                $generated-match 'xl6_xrt_take_cstr|xrtRegexMatcher(?:Find|Capture)\(|xrtPattern(?:CompileManyConfig|Match|Lookup)\('){throw 'Cgen contains matcher implementation'}
            $null=Check "$case-strict-build" gcc ($flags+@('-include',"$prefix.c",'test/cases/xlir_string_interpolation_host.c')+
                $nativeObjects+$libs+@('-o',"$prefix.strict.exe"))
            $null=Check "$case-strict-run" "$prefix.strict.exe" @()
            $null=Check "$case-jit" $build.Driver @($source,'run',"$prefix.jit")
            $null=Check "$case-aot-build" $build.Driver @($source,'exe',"$prefix.aot")
            $null=Check "$case-aot-run" "$prefix.aot.exe" @()
            $null=Check "$case-formal-run" $formalCli @('run',$source)
            $null=Check "$case-formal-build" $formalCli @('build',$source,'-o',"$prefix.formal.exe")
            $null=Check "$case-formal-execute" "$prefix.formal.exe" @()
            if($case-in@('stdlib_regex_match','stdlib_regex_iterators','stdlib_regex_replace_only',
                'stdlib_regex_replace_buffer','stdlib_regex_split_only','stdlib_regex_set_only','stdlib_regex_replace_func','stdlib_regex_replace_func_buffer',
                'stdlib_pattern_core_only','stdlib_pattern_options','stdlib_pattern_options_only','stdlib_pattern_match','stdlib_pattern_tags','stdlib_pattern_builder_lifetime')) {
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
        $report=[ordered]@{Complete=($complete-and!$changed.Count);Checks=$checks.Count;PatternOnly=[bool]$PatternOnly;Cases=$cases;Negatives=$negatives;Privacy=$privacy;UndeclaredArrays=$undeclared;
            ChangedInputs=$changed;Inputs=$inputs;Results=$checks;
            Scope='B13-B17b Regex and Pattern resources, independent captures, managed tags, options, Builder/key/edit transactions and cleanup. Source strict C/JIT/AOT and rebuilt Windows CLI run/build/execute. Native and language OOM use logical allocation injection, not ASan. Excludes isolated deployment and whole-task release acceptance'}
        [IO.File]::WriteAllText("$work/results.json",($report|ConvertTo-Json -Depth 8),$utf8)
        if($changed.Count){throw 'Regex gate inputs changed'}
    }
    Write-Output "Regex/Pattern B13-B17b: $($checks.Count) checks passed"
}finally{Pop-Location}
