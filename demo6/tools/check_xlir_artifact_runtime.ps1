param([Parameter(Mandatory=$true)][string]$PublicDirectory,
    [Parameter(Mandatory=$true)][string]$Directory,
    [string[]]$Cases = @(),
    [string]$ProcessChild = '',
    [string]$Root = '')
$ErrorActionPreference='Stop'
$rootPath=if($Root){[IO.Path]::GetFullPath($Root)}else{Split-Path -Parent $PSScriptRoot}
Push-Location $rootPath
$priorProcessChild=$env:XL6_PROCESS_CHILD
$processChildConfigured=$false
$priorTccEnvironment=@{}
foreach($variable in @('XL6_TCC_DLL','XL6_TCC_ROOT','XL6_TCC_LIB')){
    $priorTccEnvironment[$variable]=[Environment]::GetEnvironmentVariable($variable,'Process')
}
try {
    $outputPath=[IO.Path]::GetFullPath($Directory)
    if(Test-Path -LiteralPath $outputPath){throw 'Preserve prior artifact-runtime evidence'}
    $public=Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw|ConvertFrom-Json
    $cases=if ($Cases.Count) { $Cases } else { @('cgen_basic','cgen_optional_nullable_function_default','cgen_try_finally_pending_async',
        'xlir_task_aggregate_array_abi','xlir_task_when_any_array_abi','xlir_artifact_linkage_literal',
        'cgen_closure_ownership','cgen_generator','cgen_generator_two_targets','cgen_channel_string',
        'xlir_cell_loop_binding','xlir_nullable_cell_capture','cgen_function_value_error_propagation','xlir_generator_string_machine',
        'xlir_artifact_native_linkage','xlir_string_bridge_abi','xlir_artifact_native_cstring',
        'stdlib_future_aggregate_variadic_only','stdlib_future_aggregate_array_only','stdlib_future_metadata_only') }
    $fixtures=@($cases|ForEach-Object {"test/cases/$_.xl"})
    $processInputs=@()
    if('stdlib_process_extended'-in$cases-or'stdlib_process_run_bytes'-in$cases-or'stdlib_process_run_allocations'-in$cases-or'stdlib_process_file_bytes'-in$cases-or'stdlib_process_terminal_session'-in$cases-or'stdlib_process_terminal_allocations'-in$cases-or @($cases|Where-Object {$_-like'stdlib_process_live_*'-or$_-like'stdlib_process_future*'}).Count){
        if(!$ProcessChild){throw 'Process fixtures require an explicit private ProcessChild'}
        $ProcessChild=(Resolve-Path -LiteralPath $ProcessChild).Path
        $processInputs=@($ProcessChild)
        $env:XL6_PROCESS_CHILD=$ProcessChild
        $processChildConfigured=$true
    }
    $files=Get-Item -LiteralPath (@($PSCommandPath,'src/driver/xl6_xlc_shell.c','test/cases/xlir_artifact_runtime_contract.c',
        'release/xl.dll',
        'tools/stage_xlir_artifact_runtime_sources.ps1','src/compile/xl6_artifact_runtime_symbols.inc',
        'src/runtime/xl6_log_symbols.inc','src/runtime/xl6_text_symbols.inc',(Get-Command gcc).Source,(Get-Command nm).Source)+$fixtures+$processInputs)
    $inputs=@($public.Inputs)+@($public.Headers)+@($public.Objects)+@($files|ForEach-Object {[pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})+
        @([pscustomobject]@{Path=$PSCommandPath;Sha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash})
    foreach($row in $inputs){if((Get-FileHash -LiteralPath $row.Path).Hash-ne$row.Sha256){throw "Stale artifact input: $($row.Path)"}}
    New-Item -ItemType Directory -Path $outputPath|Out-Null
    Copy-Item -LiteralPath release/xl.dll -Destination "$outputPath/xl.dll"
    $objects=@($public.Objects|Where-Object {[IO.Path]::GetFileName($_.Path)-ne'xlir_public_source_driver.o'}|ForEach-Object Path)
    $flags=@('-std=c11','-O0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-pointer-sign','-Wno-format-truncation',
        '-Wno-missing-field-initializers','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include','-Ilib/tcc')
    $libs=@('release/xl.dll','-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32')
    & gcc @flags src/driver/xl6_xlc_shell.c @objects @libs -o "$outputPath/xl.exe" *> "$outputPath/cli-build.log"
    $cliBuild=$LASTEXITCODE
    & gcc @flags test/cases/xlir_artifact_runtime_contract.c @objects @libs -o "$outputPath/contract.exe" *> "$outputPath/contract-build.log"
    $contractBuild=$LASTEXITCODE
    $rows=@();$catalog=@();$linkage=@();$nativeLinkage=@();$contractRun=$null;$catalogRun=$null
    if($contractBuild-eq0){
        & "$outputPath/contract.exe" @fixtures 1> "$outputPath/contract.log" 2> "$outputPath/contract.err.log"
        $contractRun=$LASTEXITCODE
        & "$outputPath/contract.exe" --catalog 1> "$outputPath/catalog.jsonl" 2> "$outputPath/catalog.err.log"
        $catalogRun=$LASTEXITCODE
        if($catalogRun-eq0){$catalog=@(Get-Content -LiteralPath "$outputPath/catalog.jsonl"|ForEach-Object {$_|ConvertFrom-Json})}
    }
    # Build each actual provider separately. Validate the declared resident
    # closure against unresolved symbols; no successful host link may hide a
    # missing private dependency in a standalone artifact.
    $symbolOwners=@{};$componentObjects=@()
    foreach($component in $catalog){
        $object="$outputPath/component-$($component.Id).o"
        & gcc @flags -c $component.Source -o $object *> "$outputPath/component-$($component.Id).log"
        $componentBuild=$LASTEXITCODE;$defined=@();$undefined=@();$nativeUndefined=@()
        if($componentBuild-eq0){
            $defined=@(& nm -g --defined-only $object|ForEach-Object {if($_-match '\b[TRD] (xl6\w+)$'){$Matches[1]}})
            $undefined=@(& nm -u $object|ForEach-Object {if($_-match '\bU (xl6\w+)$'){$Matches[1]}})
            $nativeUndefined=@(& nm -u $object|ForEach-Object {if($_-match '\bU (xrt\w+)$'){$Matches[1]}})
            foreach($symbol in $defined){if($symbolOwners.ContainsKey($symbol)){throw "Duplicate resident export $symbol"};$symbolOwners[$symbol]=$component.Id}
        }
        $componentObjects+=[pscustomobject]@{Component=$component;Build=$componentBuild;Path=$object;Defined=$defined;Undefined=$undefined;NativeUndefined=$nativeUndefined}
    }
    $declared=@{}
    $enum=[regex]::Match((Get-Content -LiteralPath src/compile/xl6_artifact_runtime.h -Raw),
        '(?s)typedef\s+enum\s+XL6_ArtifactRuntimeId\s*\{(?<body>.*?)\}\s*XL6_ArtifactRuntimeId\s*;')
    if(!$enum.Success){throw 'Missing explicit resident component identities'}
    # Enum identifiers may contain digits (e.g. BUFFER_BASE64); count the exact
    # C identities instead of silently omitting valid resident components.
    $idNames=[regex]::Matches($enum.Groups['body'].Value,'\bXL6_ARTIFACT_(?!RUNTIME_COUNT\b)([A-Z_][A-Z_0-9]*)\b')
    $nameIds=@{};$componentIndex=0
    foreach($idName in $idNames){
        $name=$idName.Groups[1].Value
        if($nameIds.ContainsKey($name)){throw "Duplicate resident component identity $name"}
        $nameIds[$name]=$componentIndex;$componentIndex++
    }
    $expectedComponents=$nameIds.Count
    if($expectedComponents-lt24-or!$nameIds.ContainsKey('NETWORK_SERVICE')){throw 'Resident component population regressed'}
    foreach($entry in [regex]::Matches((Get-Content -LiteralPath src/compile/xl6_artifact_runtime_symbols.inc -Raw),'XL6_ARTIFACT_SYMBOL\("(\w+)", (\w+)\)')){
        $declared[$entry.Groups[1].Value]=$nameIds[$entry.Groups[2].Value]
    }
    # This catalog is also consumed by the resident API registration and the
    # artifact planner. Expand the same explicit rows, not a second symbol list.
    foreach($entry in [regex]::Matches((Get-Content -LiteralPath src/runtime/xl6_log_symbols.inc -Raw),'XL6_LOG_EXPORT\((\w+), (\w+)\)')){
        $symbol=$entry.Groups[1].Value;$owner=$entry.Groups[2].Value
        if($declared.ContainsKey($symbol)-or!$nameIds.ContainsKey($owner)){throw "Invalid logging resident export $symbol"}
        $declared[$symbol]=$nameIds[$owner]
    }
    $registryErrors=@()
    foreach($entry in [regex]::Matches((Get-Content -LiteralPath src/runtime/xl6_text_symbols.inc -Raw),'XL6_TEXT_EXPORT\((\w+), (\w+)\)')){
        $symbol=$entry.Groups[1].Value;$owner=$entry.Groups[2].Value
        if($declared.ContainsKey($symbol)-or!$nameIds.ContainsKey($owner)){throw "Invalid text resident export $symbol"}
        $declared[$symbol]=$nameIds[$owner]
    }
    foreach($symbol in $symbolOwners.Keys){if(!$declared.ContainsKey($symbol)-or$declared[$symbol]-ne$symbolOwners[$symbol]){$registryErrors+="Unregistered or misowned actual export: $symbol"}}
    foreach($symbol in $declared.Keys){if(!$symbolOwners.ContainsKey($symbol)){$registryErrors+="Declared export missing: $symbol"}}
    foreach($componentObject in $componentObjects){
        $missing=@()
        foreach($symbol in $componentObject.Undefined){
            if(!$symbolOwners.ContainsKey($symbol)-or$symbolOwners[$symbol]-notin$componentObject.Component.Closure){$missing+=$symbol}
        }
        $linkage+=[pscustomobject]@{Id=$componentObject.Component.Id;Source=$componentObject.Component.Source;Build=$componentObject.Build;
            Missing=$missing;Complete=($componentObject.Build-eq0-and!$missing.Count)}
    }
    if($cliBuild-eq0-and$contractBuild-eq0){
        $identity=[pscustomobject]@{Driver="$outputPath/xl.exe";DriverSha256=(Get-FileHash -LiteralPath "$outputPath/xl.exe").Hash}
        [IO.File]::WriteAllText("$outputPath/build.json",($identity|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
        & "$rootPath/tools/stage_xlir_artifact_runtime_sources.ps1" -Directory $outputPath *> "$outputPath/stage.log"
        # PowerShell location and the process/.NET current directory can differ.
        # Resolve provider paths against this frozen test root explicitly.
        $env:XL6_TCC_DLL=Join-Path $rootPath 'release/xl.dll'
        $env:XL6_TCC_ROOT=Join-Path $rootPath 'res/tcc/win64/x86_64'
        $env:XL6_TCC_LIB=Join-Path $rootPath 'res/tcc/win64/x86_64/lib'
        $includes=[IO.Path]::GetFullPath((Join-Path $PublicDirectory 'include'))
        foreach($case in $cases){
            foreach($phase in @('cgen','run','build','execute')){
                $prefix="$outputPath/$case";$binary="$outputPath/xl.exe";$arguments=@($phase,"test/cases/$case.xl",'--inc-path',$includes)
                if($phase-eq'cgen'){$arguments+=@('--emit',"$prefix.c",'--emit-header',"$prefix.h")}
                if($phase-eq'build'){$arguments+=@('-o',"$prefix.exe",'--save-temps')}
                if($phase-eq'execute'){$binary="$prefix.exe";$arguments=@()}
                $code=$null
                if(Test-Path -LiteralPath $binary){
                    $start=@{FilePath=$binary;WindowStyle='Hidden';PassThru=$true;WorkingDirectory=$rootPath;
                        RedirectStandardOutput="$prefix.$phase.out.log";RedirectStandardError="$prefix.$phase.err.log"}
                    if($arguments.Count){$start.ArgumentList=$arguments}
$process=Start-Process @start
                    $null = $process.Handle
                    if($process.WaitForExit(45000)){$process.Refresh();$code=$process.ExitCode}else{$process.Kill();$process.WaitForExit();$code='timeout-45s'}
                    $process.Dispose()
                }
                $rows+=[pscustomobject]@{Fixture=$case;Phase=$phase;Exit=$code;Complete=$code-eq0;Executable=$binary;Arguments=$arguments}
                Write-Output "Artifact $case $phase exit=$code"
            }
            # Independent object-level oracle: generated code and every selected
            # resident provider must resolve against this plan's actual XRT TU.
            # Inspecting symbols is test evidence, never production selection.
            $prefix="$outputPath/$case"
            & "$outputPath/contract.exe" --emit-native "test/cases/$case.xl" "$prefix.native.c" 1> "$prefix.native-plan.log" 2> "$prefix.native-plan.err.log"
            $nativePlanExit=$LASTEXITCODE;$nativeBuild=$null;$generatedBuild=$null;$missing=@();$defined=@();$required=@();$selectedSources=@();$assemblyMatches=$false;$selectionMatches=$false
            if($nativePlanExit-eq0){
                $plan=Get-Content -LiteralPath "$prefix.native-plan.log"|Where-Object {$_-match '^\{"Fixture"'}|ConvertFrom-Json
                $selectedSources=@($plan.Sources)
                $selectionMatches=$true
                if($case-in@('stdlib_text_split_only','stdlib_text_chars_only','stdlib_regex_set_only')){
                    $selectionMatches=$selectionMatches-and
                        'src/runtime/xl6_array_producer.c'-in$selectedSources-and
                        'src/runtime/xl6_array_erased.c'-in$selectedSources-and
                        'src/runtime/xl6_array_primitive_admission.c'-in$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources
                    if($case-eq'stdlib_text_chars_only'){
                        $selectionMatches=$selectionMatches-and
                            'src/runtime/xl6_char_value.c'-in$selectedSources-and
                            'src/runtime/xl6_text_split.c'-notin$selectedSources
                    }else{
                        $selectionMatches=$selectionMatches-and
                            'src/runtime/xl6_char_value.c'-notin$selectedSources-and
                            'src/runtime/xl6_char_type.c'-notin$selectedSources
                    }
                    if($case-eq'stdlib_text_split_only'){
                        $selectionMatches=$selectionMatches-and
                            'src/runtime/xl6_string_value.c'-in$selectedSources-and
                            'src/runtime/xl6_text_chars.c'-notin$selectedSources
                    }
                }
                if($case-in@('cgen_dynamic_container_reference','cgen_dynamic_container_methods')){
                    $selectionMatches='src/runtime/xl6_dynamic_collection.c'-in$selectedSources-and
                        'src/runtime/xl6_value_contains.c'-in$selectedSources-and
                        'src/runtime/xl6_array_method_remove.c'-in$selectedSources
                }
                if($case-eq'cgen_dynamic_container_param'){
                    # This fixture only reads Array/Dict parameters; its name
                    # does not make dynamic mutation/membership providers used.
                    $selectionMatches='src/runtime/xl6_dynamic_collection.c'-notin$selectedSources-and
                        'src/runtime/xl6_value_contains.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_method_remove.c'-notin$selectedSources
                }
                if($case-eq'stdlib_array_entry_argv'){
                    $selectionMatches='src/runtime/xl6_array_entry.c'-in$selectedSources-and
                        'src/runtime/xl6_array_os_argv.c'-in$selectedSources-and
                        'XRT_MODULE_UNICODE'-in$plan.NativeModules-and
                        'src/runtime/xl6_dynamic_collection.c'-notin$selectedSources
                }
                if($case-eq'cgen_basic'){
                    $selectionMatches=$selectionMatches-and
                        'src/runtime/xl6_array_producer.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_entry.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_os_argv.c'-notin$selectedSources-and
                        'src/runtime/xl6_dynamic_collection.c'-notin$selectedSources-and
                        'src/runtime/xl6_value_contains.c'-notin$selectedSources
                }
                if($case-in@('stdlib_class_interpolation','stdlib_class_values_continuation',
                    'stdlib_class_text','stdlib_class_text_derive_only',
                    'xlir_string_conversion_abi','cgen_derive_string_hash_equals')){
                    $selectionMatches='src/runtime/xl6_class_text.c'-in$selectedSources-and
                        'src/runtime/xl6_value_text.c'-in$selectedSources-and
                        'src/runtime/xl6_error_value.c'-in$selectedSources-and
                        'XRT_MODULE_VALUE_CONTAINER'-in$plan.NativeModules-and
                        'XRT_MODULE_STRING'-in$plan.NativeModules
                    if($case-eq'stdlib_class_text_derive_only'){
                        # Script entry diagnostics already require XRT console,
                        # even for cgen_basic. Class formatting must not select
                        # a language print/console provider of its own.
                        $selectionMatches=$selectionMatches-and
                            'src/runtime/xl6_print_values.c'-notin$selectedSources-and
                            !@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_console_'}).Count
                    }
                }
                if($case-in@('basic','cgen_basic','stdlib_error_text','stdlib_class_value_fields')){
                    $selectionMatches=$selectionMatches-and'src/runtime/xl6_class_text.c'-notin$selectedSources
                }
                if($case-like'stdlib_value_iterator_checked*'){
                    $selectionMatches='src/runtime/xl6_value_iterator.c'-in$selectedSources-and
                        'src/runtime/xl6_array_value_iterator.c'-in$selectedSources-and
                        'src/runtime/xl6_array_iterator.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources
                }
                if($case-like'stdlib_array_value_boundaries*'){
                    $selectionMatches='src/runtime/xl6_array_string_read.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_string_type.c'-in$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources
                    $selectionMatches=$selectionMatches-and
                        (('src/runtime/xl6_array_erased.c'-in$selectedSources)-eq($case-ne'stdlib_array_value_boundaries_only'))
                }
                if($case-like'stdlib_array_scalar_read*'){
                    $selectionMatches='src/runtime/xl6_array_scalar_read.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources
                    if($case-eq'stdlib_array_scalar_read_only'){
                        $selectionMatches=$selectionMatches-and 'src/runtime/xl6_collection_count.c'-notin$selectedSources
                    }
                }
                if($case-like'stdlib_collection_count*'){
                    $selectionMatches='src/runtime/xl6_collection_count.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources
                }
                if($case-in@('stdlib_array_shuffle_default_only','stdlib_array_shuffle_explicit_only')){
                    $default=$case-eq'stdlib_array_shuffle_default_only'
                    $selectionMatches=(('src/runtime/xl6_array_rand_shuffle.c'-in$selectedSources)-eq$default)-and
                        (('src/runtime/xl6_array_rng_shuffle.c'-in$selectedSources)-eq!$default)-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_future_admission.c'-notin$selectedSources-and
                        ($default-or'XRT_MODULE_RANDOM_DEFAULT'-notin$plan.NativeModules)
                }
                if($case-eq'stdlib_stack_factory_only'){
                    $selectionMatches='src/runtime/xl6_array_primitive_admission.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_stack.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        'XRUNTIME_MODULE_TYPED_STACK'-notin$plan.NativeModules-and
                        @($selectedSources|Where-Object {$_-match 'xl6_array_\w+_admission\.c$'}).Count-eq1
                }
                if($case-eq'stdlib_stack_nested_factory_only'){
                    $selectionMatches='src/runtime/xl6_array_sequence_admission.c'-in$selectedSources-and
                        'src/runtime/xl6_array_primitive_admission.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_nullable_char.c'-in$selectedSources-and
                        'src/runtime/xl6_stack.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        'XRUNTIME_MODULE_TYPED_STACK'-notin$plan.NativeModules-and
                        @($selectedSources|Where-Object {$_-match 'xl6_array_\w+_admission\.c$'}).Count-eq2
                }
                if($case-in@('stdlib_future_aggregate_variadic_only','stdlib_future_aggregate_array_only','stdlib_future_metadata_only')){
                    $metadata=$case-eq'stdlib_future_metadata_only'
                    $arrayInput=$case-eq'stdlib_future_aggregate_array_only'
                    $selectionMatches=(('src/runtime/xl6_future_aggregate.c'-in$selectedSources)-eq!$metadata)-and
                        (('src/runtime/xl6_future_aggregate_array.c'-in$selectedSources)-eq$arrayInput)-and
                        (('src/runtime/xl6_array_input.c'-in$selectedSources)-eq$arrayInput)-and
                        (('src/runtime/xl6_sequence.c'-in$selectedSources)-eq($arrayInput-or('src/runtime/xl6_value_iterator.c'-in$selectedSources)))-and
                        (('src/runtime/xl6_future_metadata.c'-in$selectedSources)-eq$metadata)-and
                        'src/api/xl_future_bridge.c'-notin$selectedSources
                }
                if($case-eq'stdlib_serialization_visit_json_prune'){
                    $generatedText=Get-Content -LiteralPath "$prefix.c" -Raw
                    $selectionMatches='XRT_MODULE_JSON_READ'-in$plan.NativeModules-and
                        'XRT_MODULE_XLON_READ'-notin$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(?:JSONL|XLONL)|^XRT_MODULE_(?:JSON|XLON)_FILE$'}).Count-and
                        $generatedText.Contains('xl6_serialization_callback.h')-and
                        $generatedText.Contains('xl6_serialization_visit_finish(')-and
                        !$generatedText.Contains('xl6_xrt_xlon_visit')
                }
                if($case-in@('stdlib_persistence_pretty_json_prune','stdlib_persistence_nominal_xlon_prune')){
                    $generatedText=Get-Content -LiteralPath "$prefix.c" -Raw
                    $json=$case-eq'stdlib_persistence_pretty_json_prune'
                    $format=if($json){'JSON'}else{'XLON'}
                    $other=if($json){'XLON'}else{'JSON'}
                    $selectionMatches=('XRT_MODULE_'+$format+'_READ')-in$plan.NativeModules-and
                        ('XRT_MODULE_'+$format+'_WRITE')-in$plan.NativeModules-and
                        ('XRT_MODULE_'+$other+'_READ')-notin$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(?:JSONL|XLONL)|^XRT_MODULE_(?:JSON|XLON)_FILE$'}).Count-and
                        $generatedText.Contains('xl6PersistenceProjectV1(')-and
                        $generatedText.Contains($(if($json){'xrtJsonStringify('}else{'xl6XlonEncodeV1('}))-and
                        !$generatedText.Contains('xl6_xrt_xlon_stringify_options(')
                }
                if($case-in@('stdlib_persistence_restore_json_prune','stdlib_persistence_restore_xlon_prune','stdlib_persistence_restore_array_json_prune')){
                    $generatedText=Get-Content -LiteralPath "$prefix.c" -Raw
                    $format=if($case-ne'stdlib_persistence_restore_xlon_prune'){'JSON'}else{'XLON'}
                    $other=if($format-eq'JSON'){'XLON'}else{'JSON'}
                    $entry=if($format-eq'JSON'){'xlir_persist_json_parse_t'}else{'xlir_persist_xlon_parse_t'}
                    $otherEntry=if($format-eq'JSON'){'xlir_persist_xlon_parse_t'}else{'xlir_persist_json_parse_t'}
                    $selectionMatches=('XRT_MODULE_'+$format+'_READ')-in$plan.NativeModules-and
                        ('XRT_MODULE_'+$other+'_READ')-notin$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(?:JSONL|XLONL)|^XRT_MODULE_(?:JSON|XLON)_FILE$'}).Count-and
                        $generatedText.Contains('xl6_restore_class.h')-and
                        $generatedText.Contains('xl6PersistenceRestoreParsedTakeV1(')-and
                        !$generatedText.Contains('xl6SerializeRestoreReadFinish')-and
                        !$generatedText.Contains('xlir_persist_json_assign_')-and
                        !$generatedText.Contains('xlir_persist_xlon_assign_')-and
                        'src/runtime/xl6_restore_class_closed.c'-in$selectedSources-and
                        'src/runtime/xl6_list.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_future_admission.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_import_chain.c'-in$selectedSources-and
                        $generatedText.Contains($entry)-and!$generatedText.Contains($otherEntry)-and
                        !$generatedText.Contains('_read_t')-and!$generatedText.Contains('xl6PersistenceProjectV1(')
                    if($case-eq'stdlib_persistence_restore_array_json_prune'){
                        $selectionMatches=$selectionMatches-and
                            $generatedText.Contains('xl6SequencePrimitiveCreateV1(')-and
                            'src/runtime/xl6_array_import_chain.c'-in$selectedSources
                    }
                }
                if($case-in@('stdlib_persistence_configured_json_prune','stdlib_persistence_sink_json_prune','stdlib_persistence_file_json_prune','stdlib_persistence_writer_json_prune')){
                    $generatedText=Get-Content -LiteralPath "$prefix.c" -Raw
                    # Value formatting already depends on XLON_WRITE in the
                    # shared runtime conversion closure. That baseline is not
                    # a configured XLON encoder; only demand absent unrelated
                    # line/file modules and absent physical encoder calls.
                    $fileWriter=$case-eq'stdlib_persistence_file_json_prune'
                    $entry=if($fileWriter){'WriteFileV1'}elseif($case-eq'stdlib_persistence_sink_json_prune'){'WriteSinkV1'}elseif($case-eq'stdlib_persistence_writer_json_prune'){'WriterValueV1'}else{'EncodeV1'}
                    $selectionMatches=('XRT_MODULE_JSON_FILE'-in$plan.NativeModules)-eq$fileWriter-and
                        'XRT_MODULE_JSON_WRITE'-in$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(JSONL_|XLONL_|XLON_FILE)'}).Count-and
                        $generatedText.Contains('xl6Json'+$entry+'(')-and
                        $generatedText-notmatch 'xl6(?:Xlon|Jsonl|Xlonl)(?:Encode|WriteFile|WriteSink|WriterValue|WriterName|WriterIndex)V1\('-and
                        $generatedText-notmatch '_(?:stringify|write_file|write_to)_configured\('-and
                        $generatedText-notmatch 'xlir_persist_plan_4'
                    if($entry-eq'EncodeV1'){$selectionMatches=$selectionMatches-and'XRT_MODULE_BUFFER'-in$plan.NativeModules}
                }elseif($case-eq'stdlib_text_join_only'){
                    $selectionMatches='src/runtime/xl6_text_join.c'-in$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_text_(?!join\.)'}).Count-and
                        'XRT_MODULE_STRING_SPLIT'-notin$plan.NativeModules
                }elseif($case-eq'stdlib_text_split_only'){
                    $selectionMatches='src/runtime/xl6_text_split.c'-in$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_text_(?!split\.)'}).Count
                }elseif($case-eq'stdlib_number_format_only'){
                    $selectionMatches='src/runtime/xl6_number_format.c'-in$selectedSources-and
                        'src/runtime/xl6_number_format_buffer.c'-notin$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_text_'}).Count-and
                        'XRT_MODULE_STRING_SPLIT'-notin$plan.NativeModules
                }elseif($case-in@('stdlib_array_methods_core_only','stdlib_array_methods_insert_only','stdlib_array_methods_find_only')){
                    $leaf=@{stdlib_array_methods_core_only='methods';stdlib_array_methods_insert_only='method_insert';stdlib_array_methods_find_only='method_find'}[$case]
                    $wanted="src/runtime/xl6_array_$leaf.c"
                    $methods=@($selectedSources|Where-Object {$_-match 'xl6_array_(methods|method_insert|method_find)\.c$'})
                    $selectionMatches=$methods.Count-eq1-and$methods[0]-eq$wanted-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array_import.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        (('XRUNTIME_MODULE_TYPED_ARRAY_VALUE'-in$plan.NativeModules)-eq($leaf-ne'methods'))
                }elseif($case-eq'stdlib_array_resize_only'){
                    $selectionMatches='src/runtime/xl6_array_default.c'-in$selectedSources-and
                        'src/runtime/xl6_sequence.c'-in$selectedSources-and
                        'src/runtime/xl6_array.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_import.c'-notin$selectedSources-and
                        'src/runtime/xl6_array_input.c'-notin$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_array_(methods|method_insert|method_find|.*admission)\.c$'}).Count-and
                        'XRUNTIME_MODULE_TYPED_ARRAY_VALUE'-notin$plan.NativeModules-and
                        'XRT_MODULE_VALUE_COLLECTION'-in$plan.NativeModules
                }elseif($case-eq'stdlib_text_iterator_only'){
                    $selectionMatches='src/runtime/xl6_text_iterator.c'-in$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_text_(?!iterator\.)'}).Count
                }elseif($case-eq'stdlib_number_integer_only'){
                    $selectionMatches='src/runtime/xl6_number_integer.c'-in$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_(?!integer\.)|xl6_text_'}).Count-and
                        'XRT_MODULE_NUMBER_FORMAT'-notin$plan.NativeModules-and
                        'XRT_MODULE_BUFFER'-notin$plan.NativeModules
                }elseif($case-eq'stdlib_number_float_only'){
                    $selectionMatches='src/runtime/xl6_number_float.c'-in$selectedSources-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_(?!float\.)|xl6_text_'}).Count-and
                        'XRT_MODULE_NUMBER_FORMAT'-notin$plan.NativeModules-and
                        'XRT_MODULE_BUFFER'-notin$plan.NativeModules
                }elseif($case-in@('stdlib_unicode_only','stdlib_html_only','stdlib_unicode_validator')){
                    $component=@{stdlib_unicode_only='unicode';stdlib_html_only='html';stdlib_unicode_validator='unicode_validator'}[$case]
                    $unwanted=@('unicode','unicode_validator','html')|Where-Object {$_-ne$component}
                    $selectionMatches="src/runtime/xl6_$component.c"-in$selectedSources-and
                        !@($unwanted|Where-Object {"src/runtime/xl6_$_.c"-in$selectedSources}).Count-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_|xl6_text_'}).Count-and
                        'XRT_MODULE_CHARSET'-notin$plan.NativeModules-and
                        'XRT_MODULE_BUFFER'-notin$plan.NativeModules
                }elseif($case-in@('stdlib_encoding_only','stdlib_encoding_meta_only','stdlib_encoding_transcode_only','stdlib_unicode_repair','stdlib_encoding_detect_only')){
                    $component=if($case-eq'stdlib_encoding_meta_only'){'encoding_meta'}elseif($case-eq'stdlib_encoding_detect_only'){'encoding_detect'}else{'encoding'}
                    $other=@('encoding','encoding_meta','encoding_detect')|Where-Object {$_-ne$component}
                    $selectionMatches="src/runtime/xl6_$component.c"-in$selectedSources-and
                        !@($other|Where-Object {"src/runtime/xl6_$_.c"-in$selectedSources}).Count-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_|xl6_text_|xl6_html\.|xl6_unicode\.'}).Count-and
                        (('XRT_MODULE_CHARSET'-in$plan.NativeModules)-eq($component-ne'encoding_detect'))-and
                        (('XRT_MODULE_CHARSET_DETECT'-in$plan.NativeModules)-eq($component-eq'encoding_detect'))-and
                        'XRT_MODULE_BUFFER'-notin$plan.NativeModules
                }elseif($case-in@('stdlib_pattern_core_only','stdlib_pattern_options','stdlib_pattern_options_only','stdlib_pattern_match','stdlib_pattern_errors','stdlib_pattern_tags','stdlib_pattern_error_modes',
                    'stdlib_pattern_builder','stdlib_pattern_builder_lifetime','stdlib_pattern_builder_cleanup')){
                    $expectedPattern=switch($case){
                        'stdlib_pattern_core_only' {@('core')}
                        'stdlib_pattern_options' {@('core','options')}
                        'stdlib_pattern_options_only' {@('options')}
                        'stdlib_pattern_match' {@('core','match')}
                        'stdlib_pattern_errors' {@('core','options','match')}
                        'stdlib_pattern_tags' {@('core','match')}
                        'stdlib_pattern_error_modes' {@('core')}
                        'stdlib_pattern_builder' {@('builder','core','match')}
                        'stdlib_pattern_builder_lifetime' {@('builder','core','options','match')}
                        'stdlib_pattern_builder_cleanup' {@('builder','core')}
                    }
                    $patternSources=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_pattern_'}|Sort-Object)
                    $expectedSources=@($expectedPattern|ForEach-Object {"src/runtime/xl6_pattern_$_.c"}|Sort-Object)
                    $selectionMatches=!(Compare-Object $patternSources $expectedSources)-and
                        'XRT_MODULE_PATTERN'-in$plan.NativeModules-and
                        'XRT_MODULE_REGEX_CORE'-notin$plan.NativeModules-and
                        'XRT_MODULE_BUFFER'-notin$plan.NativeModules-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_regex_|xl6_number_|xl6_text_|xl6_html\.|xl6_unicode\.|xl6_encoding'}).Count
                }elseif($case-in@('stdlib_process_command_only','stdlib_process_options_only','stdlib_process_open_only','stdlib_process_terminal_only')){
                    $leaf=$case.Replace('stdlib_process_','').Replace('_only','')
                    $expected="src/runtime/xl6_process_$leaf.c"
                    $selectedProcess=@($selectedSources|Where-Object {$_-match '/xl6_process_'})
                    $selectionMatches=$selectedProcess.Count-eq1-and$selectedProcess[0]-eq$expected
                }elseif($case-in@('stdlib_process_terminal_config','stdlib_process_terminal_session','stdlib_process_terminal_allocations')){
                    $parts=@('command','terminal_config')
                    if($case-ne'stdlib_process_terminal_config'){$parts+=@('live','status')}
                    if($case-eq'stdlib_process_terminal_session'){$parts+=@('stream','terminal_live')}
                    $actual=@($selectedSources|Where-Object {$_-match '/xl6_process_'}|Sort-Object)
                    $expected=@($parts|ForEach-Object {"src/runtime/xl6_process_$_.c"}|Sort-Object)
                    $selectionMatches=!(Compare-Object $actual $expected)-and
                        'XRT_MODULE_PROCESS_TERMINAL'-in$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-in@('XRT_MODULE_PROCESS_RUN','XRT_MODULE_PROCESS_PIPELINE','XRT_MODULE_PROCESS_FILE','XRT_MODULE_PROCESS_FUTURE','XRT_MODULE_PROCESS_OPEN')}).Count
                }elseif($case-eq'stdlib_signal_subscription_only'){
                    $baseline=Get-Content -LiteralPath "$outputPath/cgen_basic.native-plan.log" -TotalCount 1|ConvertFrom-Json
                    $expectedSources=@(@($baseline.Sources)+@('src/runtime/xl6_signal_core.c','src/runtime/xl6_signal_queue.c','src/runtime/xl6_code_finalizer.c')|Sort-Object -Unique)
                    $expectedModules=@(@($baseline.NativeModules)+@('XRT_MODULE_SIGNAL','XRT_MODULE_STRING','XRT_MODULE_TIME','XRT_MODULE_WAIT','XRT_MODULE_VALUE_CONTAINER')|Sort-Object -Unique)
                    $selectionMatches=!(Compare-Object @($selectedSources|Sort-Object -Unique) $expectedSources)-and
                        !(Compare-Object @($plan.NativeModules|Sort-Object -Unique) $expectedModules)
                }elseif($case-eq'stdlib_signal_core_only'){
                    $baseline=Get-Content -LiteralPath "$outputPath/cgen_basic.native-plan.log" -TotalCount 1|ConvertFrom-Json
                    $expectedSources=@(@($baseline.Sources)+@('src/runtime/xl6_signal_core.c')|Sort-Object -Unique)
                    $expectedModules=@(@($baseline.NativeModules)+@('XRT_MODULE_SIGNAL','XRT_MODULE_STRING')|Sort-Object -Unique)
                    $selectionMatches=!(Compare-Object @($selectedSources|Sort-Object -Unique) $expectedSources)-and
                        !(Compare-Object @($plan.NativeModules|Sort-Object -Unique) $expectedModules)
                }elseif($case-eq'stdlib_environment_only'){
                    $baseline=Get-Content -LiteralPath "$outputPath/cgen_basic.native-plan.log" -TotalCount 1|ConvertFrom-Json
                    $expectedSources=@(@($baseline.Sources)+@('src/runtime/xl6_environment.c')|Sort-Object -Unique)
                    $expectedModules=@(@($baseline.NativeModules)+@('XRT_MODULE_ENVIRONMENT','XRT_MODULE_UNICODE_TEXT','XRT_MODULE_STRING')|Sort-Object -Unique)
                    $selectionMatches=!(Compare-Object @($selectedSources|Sort-Object -Unique) $expectedSources)-and
                        !(Compare-Object @($plan.NativeModules|Sort-Object -Unique) $expectedModules)
                }elseif($case-in@('stdlib_time_text_only','stdlib_time_text','stdlib_time_protocol','stdlib_time_fields_text')){
                    $baseline=Get-Content -LiteralPath "$outputPath/cgen_basic.native-plan.log" -TotalCount 1|ConvertFrom-Json
                    $extraSources=if($case-eq'stdlib_time_fields_text'){@('src/runtime/xl6_code_finalizer.c','src/runtime/xl6_time_core.c',
                        'src/runtime/xl6_time_datetime.c','src/runtime/xl6_time_datetime_text.c')}
                        elseif($case-eq'stdlib_time_text_only'){@('src/runtime/xl6_time_text.c')}
                        else{@('src/runtime/xl6_time_core.c','src/runtime/xl6_time_text.c')}
                    if($case-eq'stdlib_time_text'){$extraSources+='src/runtime/xl6_text_value.c'}
                    $extraModules=@('XRT_MODULE_TIME_TEXT','XRT_MODULE_UNICODE_TEXT','XRT_MODULE_STRING')
                    if($case-ne'stdlib_time_text_only'){$extraModules+='XRT_MODULE_TIME'}
                    if($case-eq'stdlib_time_fields_text'){$extraModules+='XRT_MODULE_VALUE_CONTAINER'}
                    $expectedSources=@(@($baseline.Sources)+$extraSources|Sort-Object -Unique)
                    $expectedModules=@(@($baseline.NativeModules)+$extraModules|Sort-Object -Unique)
                    $selectionMatches=!(Compare-Object @($selectedSources|Sort-Object -Unique) $expectedSources)-and
                        !(Compare-Object @($plan.NativeModules|Sort-Object -Unique) $expectedModules)
                }elseif($case-in@('stdlib_time_core','stdlib_time_core_only')){
                    $selectionMatches='src/runtime/xl6_time_core.c'-in$selectedSources-and
                        'XRT_MODULE_TIME'-in$plan.NativeModules-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(TIME_LOCAL|PATTERN|REGEX_|PROCESS)'}).Count-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_pattern_|xl6_regex_|xl6_number_|xl6_text_|xl6_encoding|xl6_unicode'}).Count
                    $baselinePath="$outputPath/cgen_basic.native-plan.log"
                    if(Test-Path -LiteralPath $baselinePath){
                        $baseline=Get-Content -LiteralPath $baselinePath -TotalCount 1|ConvertFrom-Json
                        $expectedSources=@(@($baseline.Sources)+@('src/runtime/xl6_time_core.c')|Sort-Object -Unique)
                        $expectedModules=@(@($baseline.NativeModules)+@('XRT_MODULE_TIME','XRT_MODULE_STRING')|Sort-Object -Unique)
                        $selectionMatches=$selectionMatches-and !(Compare-Object @($selectedSources|Sort-Object -Unique) $expectedSources)-and
                            !(Compare-Object @($plan.NativeModules|Sort-Object -Unique) $expectedModules)
                    }
                }elseif($case-in@('stdlib_regex_core_only','stdlib_regex_match','stdlib_regex_errors')){
                    $selectionMatches='src/runtime/xl6_regex_core.c'-in$selectedSources-and
                        (('src/runtime/xl6_regex_match.c'-in$selectedSources)-eq($case-ne'stdlib_regex_core_only'))-and
                        'XRT_MODULE_REGEX_CORE'-in$plan.NativeModules-and
                        (('XRT_MODULE_REGEX_MATCH'-in$plan.NativeModules)-eq($case-ne'stdlib_regex_core_only'))-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_|xl6_text_|xl6_html\.|xl6_unicode\.|xl6_encoding'}).Count-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_(PATTERN|REGEX_(SET|REPLACE|SPLIT))$'}).Count
                }elseif($case-in@('stdlib_regex_iterators','stdlib_regex_replace_only','stdlib_regex_replace_buffer','stdlib_regex_replace_buffer_errors',
                    'stdlib_regex_split_only','stdlib_regex_set_only','stdlib_regex_extended_errors','stdlib_regex_byte_boundaries',
                    'stdlib_regex_replace_func','stdlib_regex_replace_func_buffer','stdlib_regex_replace_func_errors','stdlib_regex_error_metadata')){
                    $expectedRegex=switch($case){
                        'stdlib_regex_iterators' {@('core','match','iterator')}
                        'stdlib_regex_replace_only' {@('core','replace')}
                        'stdlib_regex_replace_buffer' {@('core','replace_buffer')}
                        'stdlib_regex_replace_buffer_errors' {@('core','replace_buffer')}
                        'stdlib_regex_split_only' {@('core','split')}
                        'stdlib_regex_set_only' {@('core','set')}
                        'stdlib_regex_extended_errors' {@('core','match','iterator','replace','split','set')}
                        'stdlib_regex_byte_boundaries' {@('core','match','replace','split')}
                        'stdlib_regex_replace_func' {@('core','match','replace_func')}
                        'stdlib_regex_replace_func_buffer' {@('core','match','replace_func','replace_func_buffer')}
                        'stdlib_regex_replace_func_errors' {@('core','match','replace_func','replace_func_buffer')}
                        'stdlib_regex_error_metadata' {@('core','set')}
                    }
                    $regexSources=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_regex_'}|Sort-Object)
                    $expectedSources=@($expectedRegex|ForEach-Object {"src/runtime/xl6_regex_$_.c"}|Sort-Object)
                    $selectionMatches=!(Compare-Object $regexSources $expectedSources)-and
                        (('XRT_MODULE_BUFFER'-in$plan.NativeModules)-eq($case-in@('stdlib_regex_replace_buffer','stdlib_regex_replace_buffer_errors',
                            'stdlib_regex_replace_func_buffer','stdlib_regex_replace_func_errors')))-and
                        'XRT_MODULE_PATTERN'-notin$plan.NativeModules-and
                        !@($selectedSources|Where-Object {$_-match 'xl6_number_|xl6_text_|xl6_html\.|xl6_unicode\.|xl6_encoding'}).Count
                }
                if($case-in@('stdlib_process_live_wait','stdlib_process_live_streams','stdlib_process_live_drop')){
                    $expectedProcess=@('command','live')
                    if($case-ne'stdlib_process_live_drop'){$expectedProcess+=@('status')}
                    if($case-ne'stdlib_process_live_wait'){$expectedProcess+=@('stream')}
                    $actualProcess=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_process_'}|Sort-Object)
                    $wantedProcess=@($expectedProcess|ForEach-Object {"src/runtime/xl6_process_$_.c"}|Sort-Object)
                    $selectionMatches=$selectionMatches-and!(Compare-Object $actualProcess $wantedProcess)-and
                        (('XRT_MODULE_IO'-in$plan.NativeModules)-eq($case-ne'stdlib_process_live_wait'))-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_PROCESS_(RUN|PIPELINE|FUTURE|FILE|TERMINAL|OPEN)$'}).Count
                }
                if($case-in@('stdlib_process_future','stdlib_process_future_drop','stdlib_process_future_cancel','stdlib_process_future_allocations','stdlib_process_future_drop_allocations','stdlib_process_future_cancel_allocations')){
                    $expectedProcess=@('command','live','status','future')
                    if($case-notin@('stdlib_process_future','stdlib_process_future_allocations')){$expectedProcess+=@('stream')}
                    $actualProcess=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_process_'}|Sort-Object)
                    $wantedProcess=@($expectedProcess|ForEach-Object {"src/runtime/xl6_process_$_.c"}|Sort-Object)
                    $selectionMatches=$selectionMatches-and!(Compare-Object $actualProcess $wantedProcess)-and
                        'XRT_MODULE_PROCESS_FUTURE'-in$plan.NativeModules-and
                        'src/runtime/xl6_future_payload.c'-in$selectedSources-and
                        (('XRT_MODULE_IO'-in$plan.NativeModules)-eq($case-notin@('stdlib_process_future','stdlib_process_future_allocations')))-and
                        !@($plan.NativeModules|Where-Object {$_-match '^XRT_MODULE_PROCESS_(RUN|PIPELINE|FILE|TERMINAL|OPEN)$'}).Count
                }
                $extractLeaves=@{
                    stdlib_array_extract_scalar_only='array_extract_scalar';
                    stdlib_array_extract_string_only='array_extract_string';
                    stdlib_array_extract_value_only='array_extract_value';
                    stdlib_array_extract_char_only='array_extract_char';
                    stdlib_array_extract_f32_only='array_extract_f32';
                    stdlib_array_extract_trivial_only='array_extract_trivial';
                    stdlib_array_extract_nullable_char_only='array_extract_trivial';
                    stdlib_array_extract_unreachable_c_scalar_only='array_extract_trivial';
                    stdlib_collection_extract_only='collection_extract_value'
                }
                if($extractLeaves.ContainsKey($case)) {
                    $wanted="src/runtime/xl6_$($extractLeaves[$case]).c"
                    $actual=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_(array_extract_|collection_extract_)'})
                    $selectionMatches=$selectionMatches-and$actual.Count-eq1-and$actual[0]-eq$wanted-and
                        !@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_array(?:\.c|_(?:import|input|methods|method_insert|method_find)\.c)$'}).Count
                    if($case-eq'stdlib_collection_extract_only') {
                        $selectionMatches=$selectionMatches-and'src/runtime/xl6_sequence.c'-notin$selectedSources-and
                            'XRUNTIME_MODULE_TYPED_ARRAY'-notin$plan.NativeModules-and
                            'XRUNTIME_MODULE_TYPED_ARRAY_VALUE'-notin$plan.NativeModules
                    } else {
                        # These executable fixtures populate their Array with a
                        # literal. Its consuming append genuinely selects the
                        # existing VALUE_V1 write provider and TypedArrayValue.
                        # That dependency is not an extraction projection: keep
                        # the one-leaf check above, and verify the constructor
                        # reference rather than forbidding a required module.
                        $generatedText=Get-Content -LiteralPath "$prefix.c" -Raw
                        $selectionMatches=$selectionMatches-and
                            $generatedText.Contains('xl6ArrayValueAppendTakeV1(')-and
                            'src/runtime/xl6_array_erased.c'-in$selectedSources-and
                            'XRUNTIME_MODULE_TYPED_ARRAY_VALUE'-in$plan.NativeModules
                        if($case-in@('stdlib_array_extract_trivial_only','stdlib_array_extract_nullable_char_only','stdlib_array_extract_unreachable_c_scalar_only')) {
                            $profile=if($case-eq'stdlib_array_extract_nullable_char_only'){'char'}else{'bool'}
                            $expected=@("src/runtime/xl6_nullable_$profile.c","src/runtime/xl6_nullable_${profile}_value.c")
                            $actual=@($selectedSources|Where-Object {$_-match 'src/runtime/xl6_nullable_.*\.c$'}|Sort-Object)
                            $selectionMatches=$selectionMatches-and!(Compare-Object $actual ($expected|Sort-Object))-and
                                (('src/runtime/xl6_char_type.c'-in$selectedSources)-eq($profile-eq'char'))-and
                                'src/runtime/xl6_char_value.c'-notin$selectedSources
                            if($case-eq'stdlib_array_extract_unreachable_c_scalar_only') {
                                $selectionMatches=$selectionMatches-and
                                    !@($selectedSources|Where-Object {$_-match 'native_scalar'}).Count
                            }
                        }
                    }
                }
                & gcc @flags "-I$includes" -Ilib/xruntime -c "$prefix.native.c" -o "$prefix.native.o" *> "$prefix.native-build.log"
                $nativeBuild=$LASTEXITCODE
                & gcc @flags "-I$includes" -c "$prefix.c" -o "$prefix.generated.o" *> "$prefix.generated-build.log"
                $generatedBuild=$LASTEXITCODE
                if($nativeBuild-eq0-and$generatedBuild-eq0){
                    $defined=@(& nm -g --defined-only "$prefix.native.o"|ForEach-Object {if($_-match '\b[TRD] (xrt\w+)$'){$Matches[1]}})
                    $required=@(& nm -u "$prefix.generated.o" "$prefix.native.o"|ForEach-Object {if($_-match '\bU (xrt\w+)$'){$Matches[1]}})+
                        @($componentObjects|Where-Object {$_.Component.Source-in$selectedSources}|ForEach-Object NativeUndefined)
                    $required=@($required|Sort-Object -Unique)
                    $missing=@($required|Where-Object {$_-notin$defined})
                    if($case-in@('stdlib_persistence_configured_json_prune','stdlib_persistence_sink_json_prune','stdlib_persistence_file_json_prune','stdlib_persistence_writer_json_prune','stdlib_serialization_visit_json_prune','stdlib_persistence_pretty_json_prune','stdlib_persistence_nominal_xlon_prune','stdlib_persistence_restore_json_prune','stdlib_persistence_restore_xlon_prune')){
                        $selectionMatches=$selectionMatches-and
                            !@(@($required)+@($defined)|Where-Object {$_-match '^xrt(?:Jsonl|Xlonl)'}).Count
                    }
                }
                $saved=@(Get-Content -LiteralPath "$prefix.build.err.log"|ForEach-Object {if($_-match '^temporary files kept: (.+)$'){$Matches[1]}})
                if($saved.Count-eq1){
                    $actual=Join-Path $saved[0] 'xl6_xrt_impl.c'
                    if(Test-Path -LiteralPath $actual){
                        Copy-Item -LiteralPath $actual -Destination "$prefix.actual-native.c"
                        $assemblyMatches=(Get-FileHash -LiteralPath $actual).Hash-eq(Get-FileHash -LiteralPath "$prefix.native.c").Hash
                    }
                }
            }
            $nativeLinkage+=[pscustomobject]@{Fixture=$case;Plan=$nativePlanExit;NativeBuild=$nativeBuild;GeneratedBuild=$generatedBuild;
                Sources=$selectedSources;Required=$required;Defined=$defined;Missing=$missing;ActualAssemblyMatchesPlan=$assemblyMatches;
                SelectionMatchesExpected=$selectionMatches;
                Complete=($nativePlanExit-eq0-and$nativeBuild-eq0-and$generatedBuild-eq0-and!$missing.Count-and$assemblyMatches-and$selectionMatches)}
            Write-Output "Native closure $case plan=$nativePlanExit build=$nativeBuild C=$generatedBuild missing=$($missing.Count) actual=$assemblyMatches selection=$selectionMatches"
        }
    }
    $unchanged=@($inputs|Where-Object {(Get-FileHash -LiteralPath $_.Path).Hash-ne$_.Sha256}).Count-eq0
    $complete=$unchanged-and$cliBuild-eq0-and$contractBuild-eq0-and$contractRun-eq0-and$catalogRun-eq0-and$catalog.Count-eq$expectedComponents-and
        $rows.Count-eq($cases.Count*4)-and!@($rows|Where-Object {!$_.Complete}).Count-and!$registryErrors.Count-and$declared.Count-eq$symbolOwners.Count-and
        $linkage.Count-eq$expectedComponents-and!@($linkage|Where-Object {!$_.Complete}).Count-and
        $nativeLinkage.Count-eq$cases.Count-and!@($nativeLinkage|Where-Object {!$_.Complete}).Count
    $report=[pscustomobject]@{Complete=$complete;Inputs=$inputs;InputsUnchanged=$unchanged;CliBuild=$cliBuild;ContractBuild=$contractBuild;
        ContractRun=$contractRun;CatalogRun=$catalogRun;ComponentCount=$expectedComponents;DependencyPairs=($expectedComponents*$expectedComponents);DeclaredExports=$declared.Count;ActualExports=$symbolOwners.Count;
        RegistryErrors=$registryErrors;Linkage=$linkage;NativeLinkage=$nativeLinkage;Results=$rows;Fixtures=$cases;
        Scope='Current-source standalone CLI with explicit private declaration/source staging and TCC provider. Covers exactly the listed fixtures, all resident export ownership and dependency pairs, native object closure and actual assembly/plan equality. Not shared release xlc.dll, isolated deployment or whole-platform acceptance.'}
    [IO.File]::WriteAllText("$outputPath/results.json",($report|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
    if(!$complete){throw 'Artifact runtime failures retained'}
} finally {
    if($processChildConfigured){$env:XL6_PROCESS_CHILD=$priorProcessChild}
    foreach($variable in $priorTccEnvironment.Keys){
        [Environment]::SetEnvironmentVariable($variable,$priorTccEnvironment[$variable],'Process')
    }
    Pop-Location
}
