param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
$rootPath = Split-Path -Parent $PSScriptRoot
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/stack-native-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior Stack native evidence' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source Stack compiler build failed' }
    }
    $public = Get-Content -Encoding UTF8 -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    if ($public.RuntimeOnly -or !$public.SourceXrt) { throw 'Full current-source compiler/runtime required' }
    & "$PSScriptRoot/stage_xlir_artifact_runtime_sources.ps1" -Directory $PublicDirectory *> "$outPath/stage.log"
    if (!$?) { throw 'Current-source native package staging failed' }
    $cases = @('stdlib_stack_native','stdlib_stack_scalar','xlir_stack_resource_abi','xlir_stack_class_default_abi','stdlib_stack_values','stdlib_stack_owned','stdlib_stack_owned_continuation','stdlib_stack_try_owned','stdlib_stack_try_continuation',
        'stdlib_stack_nullable_scalar','stdlib_stack_nullable_c_scalar','stdlib_stack_nullable_c_i8','stdlib_stack_nullable_continuation','stdlib_stack_nullable_i64','stdlib_stack_nullable_bool','stdlib_stack_nullable_u64','stdlib_stack_nullable_f32','stdlib_stack_nullable_f64','stdlib_stack_nullable_time','stdlib_stack_nullable_char','stdlib_sequence_nullable_identity')
    $projections = @('stdlib_nullable_projection_bool','stdlib_nullable_projection_i64','stdlib_nullable_projection_u64',
        'stdlib_nullable_projection_f32','stdlib_nullable_projection_f64','stdlib_nullable_projection_time','stdlib_nullable_projection_char','stdlib_nullable_projection_oom','stdlib_scalar_projection_char','stdlib_scalar_projection_text','stdlib_scalar_projection_oom')
    $extraCases = @('xlir_nullable_scalar_no_resource','stdlib_nullable_resource_transport','stdlib_nullable_queue_transport','stdlib_nullable_text_resource_transport','stdlib_nullable_text_batch_projection','xlir_nullable_string_equal') + $projections
    $negative = @('char_output_type','bytes_output_type','bytes_input_type','owned_argument','owned_depth','owned_result','try_argument','try_depth','try_result','try_nullable_result',
        'nullable_literal_range','nullable_variable_narrow','nullable_identity',
        'retired_pop_into','retired_peek_into','retired_peek_at_into')
    $gcc = (Get-Command gcc -CommandType Application | Select-Object -First 1).Source
    $python = (Get-Command python -CommandType Application | Select-Object -First 1).Source
    $files = @(($cases + $extraCases) | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @($negative | ForEach-Object { Get-Item "test/fixtures/stack_value_contract/$_.xl" }) +
        @(Get-Item -LiteralPath @($PSCommandPath,$gcc,$python,'release/xl.dll','test/cases/stdlib_stack_native_host.c',
            'test/support/stack_native.c','test/support/stack_take_found.h',
            'test/cases/stdlib_sequence_take_status_native_host.c','test/cases/stdlib_stack_retired_try_abi.c',
            'test/cases/xlir_module_product_oom_host.c',
            'test/support/module_product_memory.c','test/support/module_product_memory.h',
            'test/cases/xlir_stack_native_contract.c','test/cases/xlir_nullable_scalar_native.c','tools/check_xlir_artifact_runtime.ps1',
            'tools/check_stdlib_sequence_clear.py','test/support/sequence_clear_probe.c','test/support/array_value_native.c',
            'test/cases/stdlib_sequence_clear_native_host.c','test/cases/xlir_time_text_closure_host.c',
            'test/cases/stdlib_stack_clear_minimal.xl','test/cases/stdlib_stack_clear_error.xl',
            'test/cases/stdlib_stack_clear_error_continuation.xl','tools/check_stdlib_collection_admission.py',
            'test/cases/stdlib_collection_admission.xl','test/cases/stdlib_collection_nominal_admission.xl',
            'test/cases/stdlib_array_defaults.xl','test/cases/stdlib_array_defaults_continuation.xl',
            'test/cases/stdlib_array_layouts.xl','tools/check_stdlib_callable_scalars.py',
            'tools/check_xlir_function_value_calls.py','test/cases/xlir_function_value_calls.xl',
            'test/cases/xlir_function_value_calls_contract.c',
            'test/cases/stdlib_callable_scalars.xl','test/cases/stdlib_callable_scalars_unused.xl',
            'test/cases/stdlib_callable_scalars_contract.c','test/cases/stdlib_callable_scalars_host.c',
            'test/cases/stdlib_callable_integer_domains.xl','test/cases/stdlib_callable_integer_domains_host.c',
            'test/cases/stdlib_callable_native_scalars.xl','test/cases/stdlib_callable_native_scalars_host.c',
            'test/cases/xlir_callable_bridge_mutations.h'))
    $files += @(Get-Item -LiteralPath 'test/cases/stdlib_array_future_reference.xl','tools/check_stdlib_array_future.py','test/cases/stdlib_array_future_native_host.c',
        'test/support/array_future_native.c','test/cases/stdlib_sequence_retired_admission_abi.c')
    $files += @(Get-Item -LiteralPath 'tools/check_stdlib_array_input.py','test/cases/stdlib_array_input_host.c',
        'test/cases/stdlib_future_metadata_host.c','test/cases/xlir_future_metadata_contract.c',
        'test/cases/stdlib_future_aggregate_variadic_only.xl','test/cases/stdlib_future_aggregate_array_only.xl','test/cases/stdlib_future_metadata_only.xl',
        'test/cases/stdlib_text_join_only.xl','test/cases/stdlib_text_join.xl','test/cases/stdlib_regex_set_only.xl',
        'test/cases/stdlib_pattern_builder.xl','test/cases/stdlib_pattern_builder_lifetime.xl',
        'test/cases/xlir_task_aggregate_array_abi.xl','test/cases/xlir_task_when_any_array_abi.xl')
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale core input $($record.Path)" }
    }
    $rows = [Collections.Generic.List[object]]::new()
    function Run([string]$Name, [string]$File, [string[]]$Arguments, [bool]$Reject = $false,
        [int]$TimeoutMs = 45000) {
        $process = [Diagnostics.Process]::new()
        $process.StartInfo.FileName = $File
        $process.StartInfo.Arguments = Join-XlirWindowsCommandLine $Arguments
        $process.StartInfo.WorkingDirectory = $rootPath
        $process.StartInfo.UseShellExecute = $false
        $process.StartInfo.CreateNoWindow = $true
        $process.StartInfo.RedirectStandardOutput = $true
        $process.StartInfo.RedirectStandardError = $true
        $process.StartInfo.StandardOutputEncoding = [Text.Encoding]::UTF8
        $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
        $watch = [Diagnostics.Stopwatch]::StartNew()
        try {
            [void]$process.Start()
            $stdout = $process.StandardOutput.ReadToEndAsync(); $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit($TimeoutMs)) { Stop-XlirProcessTree $process; throw "Timeout $Name" }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log", $stdout.Result + $stderr.Result, [Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        $ok = if ($Reject) { $code -ne 0 } else { $code -eq 0 }
        $watch.Stop()
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Complete=$ok;
            Seconds=$watch.Elapsed.TotalSeconds;TimeoutMs=$TimeoutMs})
        if (!$ok) { Get-Content -Encoding UTF8 "$outPath/$Name.log" -Tail 25; throw "$Name failed: $code" }
    }
    & "$PSScriptRoot/check_xlir_artifact_runtime.ps1" -PublicDirectory $PublicDirectory -Directory "$outPath/artifact" -Cases ($cases + $extraCases) *> "$outPath/artifact.log"
    if (!$?) { throw 'Stack actual artifact acceptance failed' }
    $artifact = Get-Content -Encoding UTF8 "$outPath/artifact/results.json" -Raw | ConvertFrom-Json
    if (!$artifact.Complete -or !$artifact.InputsUnchanged) { throw 'Incomplete Stack artifact evidence' }
    foreach ($case in $cases) {
        $plan = $artifact.NativeLinkage | Where-Object Fixture -eq $case
        if ('src/runtime/xl6_stack.c' -notin $plan.Sources -or 'src/runtime/xl6_sequence.c' -notin $plan.Sources -or
            'src/runtime/xl6_array.c' -in $plan.Sources -or 'xrtTypedStackCreate' -notin $plan.Defined -or
            'xrtTypedStackCreate' -notin $plan.Required) { throw 'Missing actual native typed Stack closure' }
        if (@($plan.Sources | Where-Object { $_ -match 'xl6_buffer_|xl6_regex_|xl6_log_|xl6_process_' }).Count) { throw 'Unrelated resident component in Stack' }
        if (@($plan.Defined | Where-Object { $_ -match '^_?xl6Stack(PopOrPeek|PeekAt|TryTake)V1$' }).Count) { throw 'Retired native Stack entry remains in actual artifact' }
    }
    foreach ($case in ($cases + $extraCases)) {
        $plan = $artifact.NativeLinkage | Where-Object Fixture -eq $case
        $expected = if ($case -eq 'stdlib_stack_nullable_scalar') { @('bool','i64','u64','f32','f64','time','char') }
            elseif ($case -eq 'stdlib_stack_nullable_c_scalar') { @('bool','i64','u64','f32','f64','native_i8','native_u8','native_i16','native_u16','native_i32','native_u32') }
            elseif ($case -eq 'stdlib_stack_nullable_c_i8') { @('native_i8') }
            elseif ($case -eq 'stdlib_nullable_projection_oom') { @('bool','i64','u64','f32','f64','time','char') | ForEach-Object { $_; "${_}_value" } }
            elseif ($case -match '^stdlib_nullable_projection_(bool|i64|u64|f32|f64|time|char)$') { @($Matches[1],($Matches[1] + '_value')) }
            elseif ($case -eq 'stdlib_sequence_nullable_identity' -or $case -like 'stdlib_nullable_text_*' -or $case -eq 'xlir_nullable_string_equal') { @('string') }
            elseif ($case -eq 'stdlib_stack_nullable_continuation' -or $case -eq 'stdlib_nullable_resource_transport') { @('i64') }
            elseif ($case -match '^stdlib_stack_nullable_(bool|i64|u64|f32|f64|time|char)$') { @($Matches[1]) }
            else { @() }
        $selected = @($plan.Sources | Where-Object { $_ -match '^src/runtime/xl6_nullable_' } | ForEach-Object {
            $_ -replace '^src/runtime/xl6_nullable_|\.c$', '' })
        if ((($expected | Sort-Object) -join ',') -cne (($selected | Sort-Object) -join ',')) { throw "Wrong nullable native profile closure: $case" }
        if ($case -in @('stdlib_scalar_projection_char','stdlib_scalar_projection_oom') -and
            ('src/runtime/xl6_char_type.c' -notin $plan.Sources -or 'src/runtime/xl6_char_value.c' -notin $plan.Sources)) {
            throw 'Missing exact char descriptor/projection'
        }
        if ($case -eq 'stdlib_stack_values' -and
            ('src/runtime/xl6_char_type.c' -notin $plan.Sources -or 'src/runtime/xl6_char_value.c' -in $plan.Sources)) {
            throw 'Storage-only Stack pulled char projection'
        }
        if ($case -in @('stdlib_scalar_projection_text','stdlib_scalar_projection_oom') -and
            ('src/runtime/xl6_string_type.c' -notin $plan.Sources -or 'src/runtime/xl6_string_value.c' -notin $plan.Sources)) {
            throw 'Missing non-null string descriptor/projection'
        }
        if ($case -eq 'stdlib_stack_native' -and
            ('src/runtime/xl6_string_type.c' -notin $plan.Sources -or 'src/runtime/xl6_string_value.c' -in $plan.Sources)) {
            throw 'Storage-only Stack pulled string projection'
        }
    }
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Irelease','-Irelease/include',"-I$PublicDirectory/include")
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_stack_native_host.c','test/support/stack_native.c','src/runtime/xl6_sequence.c','src/runtime/xl6_stack.c','-o',"$outPath/native.exe"))
    Run 'native-vectors' "$outPath/native.exe" @()
    Run 'retired-try.compile' $gcc (@('-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease',
        '-c','test/cases/stdlib_stack_retired_try_abi.c','-o',"$outPath/retired-try.o"))
    Run 'retired-try.link' $gcc @("$outPath/retired-try.o",'release/xl.dll','-o',"$outPath/retired-try.exe") $true
    if ((Get-Content -Encoding UTF8 "$outPath/retired-try.link.log" -Raw) -notmatch 'undefined reference.*xl6StackTryTakeV1') {
        throw 'Retired ABI rejection was not the missing old symbol'
    }
    foreach ($level in @(0,2)) {
        Run "take-status.O$level.build" $gcc (@('-std=c11','-Wall','-Wextra','-Werror',"-O$level",
            '-DXL6_TEST_ERASED_STACK','-I.','-Irelease','-Irelease/include',
            'test/cases/stdlib_sequence_take_status_native_host.c','test/support/array_value_native.c',
            'src/runtime/xl6_sequence.c','src/runtime/xl6_array.c','src/runtime/xl6_stack.c',
            'src/runtime/xl6_ownership_plan.c','-o',"$outPath/take-status.O$level.exe"))
        Run "take-status.O$level.vectors" "$outPath/take-status.O$level.exe" @()
    }
    Run 'nullable-native-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_nullable_scalar_native.c') +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/nullable-native.exe"))
    Run 'nullable-native-vectors' "$outPath/nullable-native.exe" @()
    $sweeps = @()
    foreach ($operation in @('create','reserve','push','peek','clone','trim','owned-peek','try-peek')) {
        for ($point=0; $point -le 64; ++$point) {
            $name = "native.$operation.oom-$point"
            Run $name "$outPath/native.exe" @($operation,"$point")
            if ((Get-Content -Encoding UTF8 "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
        }
        if ($point -gt 64) { throw "Incomplete Stack native prefix: $operation" }
        $sweeps += [pscustomobject]@{Fixture=$operation;FirstNoHit=$point}
    }
    Run 'contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_stack_native_contract.c') +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/contract.exe"))
    foreach ($case in ($cases + @('xlir_nullable_string_equal','xlir_nullable_scalar_no_resource') + $projections)) {
        $prefix = "$outPath/$case"
        Run "$case.emit" $public.Driver @("test/cases/$case.xl",'emit',$prefix)
        # Large nullable fixtures deliberately reverify hundreds of malformed
        # whole-module contracts. Measured ~49 seconds at O0; keep the ordinary
        # operation bound, with a separate finite budget for this CPU test.
        Run "$case.contract" "$outPath/contract.exe" @("test/cases/$case.xl","$prefix.contract.c") -TimeoutMs 90000
        if ((Get-Content -Encoding UTF8 "$prefix.c" -Raw) -cne (Get-Content -Encoding UTF8 "$prefix.contract.c" -Raw)) { throw 'Noncanonical Stack C output' }
        if ((Get-Content -Encoding UTF8 "$prefix.c" -Raw) -match 'xlir_stack_(handle|payload|create|push|clone|raise|reserve|pop|peek|equals|trim|clear|size)') { throw 'Retired generated Stack implementation emitted' }
        if ((Get-Content -Encoding UTF8 "$prefix.c" -Raw) -match 'xl6Stack(PopOrPeek|PeekAt)V1') { throw 'Retired Stack native prototype or call emitted' }
        $code = Get-Content -Encoding UTF8 "$prefix.c" -Raw
        $retrievals = [regex]::Matches($code,'bool projection_ok_(\d+) = xl6Stack(?:TakeV1|TryTakeV2)\(')
        if ($retrievals.Count -ne [regex]::Matches($code,'xl6Stack(?:TakeV1|TryTakeV2)\(v').Count) {
            throw 'Stack retrieval discarded its independent operation status'
        }
        if ($code.Contains('xl6StackTryTakeV1')) { throw 'Retired presence-only private Stack ABI emitted' }
        foreach ($retrieval in $retrievals) {
            if ($code -notmatch ('if \(projection_ok_' + $retrieval.Groups[1].Value + '\)')) {
                throw 'Stack retrieval did not branch on its independent operation status'
            }
        }
    }
    foreach($case in $negative){
        $prefix="$outPath/negative.$case"
        Run "negative.$case" $public.Driver @("test/fixtures/stack_value_contract/$case.xl",'emit',$prefix) $true
        $diagnostic = switch ($case) {
            'char_output_type' { '\[sem\].*: target Stack<uint32>, value Stack<char>' }
            'bytes_output_type' { '\[sem\].*: target string, value bytes' }
            { $_ -like 'retired_*' } { '\[sem\].*unknown Stack method' }
            'owned_argument' { '\[sem\].*\[SC-ARG-COUNT\]' }
            'owned_depth' { '\[sem\].*Stack\.peekAt.*: target int, value string' }
            'owned_result' { '\[sem\].*: target string, value bytes' }
            'try_argument' { '\[sem\].*\[SC-ARG-COUNT\]' }
            'try_depth' { '\[sem\].*Stack\.peekAt.*: target int, value string' }
            'try_result' { '\[sem\].*: target nullable<bytes>, value nullable' }
            'try_nullable_result' { '\[sem\].*: target string, value nullable' }
            'nullable_literal_range' { '\[sem\].*Stack.push.*target nullable<int32>, value int' }
            'nullable_variable_narrow' { '\[sem\].*Stack.push.*target nullable<int32>, value int' }
            'nullable_identity' { '\[sem\].*Stack.push.*target nullable<char>' }
            default { '\[sem\].*Stack.*: target (char|bytes), value (uint32|var|string)' }
        }
        if((Get-Content -Encoding UTF8 "$prefix.log" -Raw) -notmatch $diagnostic -or
            (Test-Path "$prefix.c") -or (Test-Path "$prefix.h")){throw 'Wrong Stack type rejection or partial output'}
    }
    Run 'debug-host-build' $gcc ($strict + @('-c','test/support/module_product_memory.c','-o',"$outPath/debug-host.o"))
    foreach($case in @('stdlib_stack_native','stdlib_stack_values','stdlib_stack_owned','stdlib_stack_try_owned','stdlib_stack_nullable_scalar','stdlib_stack_nullable_c_scalar','stdlib_stack_nullable_c_i8','stdlib_sequence_nullable_identity','stdlib_nullable_text_resource_transport','xlir_nullable_string_equal','stdlib_nullable_projection_oom','stdlib_scalar_projection_oom')){
    $prefix = "$outPath/$case"
    if (!(Test-Path -LiteralPath "$prefix.c")) {
        Run "$case.language-oom-emit" $public.Driver @("test/cases/$case.xl",'emit',$prefix)
    }
    Run "$case.language-oom-build" $gcc ($strict + @('-DXL6_PRODUCT_USER_ENTRY','-include',"$prefix.c",
        'test/cases/xlir_module_product_oom_host.c',"$outPath/debug-host.o") +
        $objects + @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$prefix.host.exe"))
    Run "$case.language-oom-normal" "$prefix.host.exe" @()
    for ($point=0; $point -le 4096; ++$point) {
        $name = "$case.language.oom-$point"
        Run $name "$prefix.host.exe" @("$point")
        if ((Get-Content -Encoding UTF8 "$outPath/$name.log" -Raw) -match 'hit=0 balanced') { break }
    }
    if ($point -gt 4096) { throw 'Incomplete Stack language allocation prefix' }
    $sweeps += [pscustomobject]@{Fixture=$case;FirstNoHit=$point}
    }
    Run 'sequence-clear' $python @('tools/check_stdlib_sequence_clear.py','--out',"$outPath/sequence-clear",'--public-directory',$PublicDirectory) -TimeoutMs 900000
    $clear = Get-Content -Encoding UTF8 -LiteralPath "$outPath/sequence-clear/results.json" -Raw | ConvertFrom-Json
    if (!$clear.Complete -or !$clear.InputsUnchanged) { throw 'Incomplete explicit clear evidence' }
    foreach ($mode in @('base','nominal','defaults','layouts','numeric-enums')) {
        $arguments=@('tools/check_stdlib_collection_admission.py','--out',"$outPath/collection-$mode",'--public-directory',$PublicDirectory)
        if ($mode -ne 'base') { $arguments += "--$mode" }
        # Layouts includes the separately bounded scalar and expression gates.
        $collectionTimeout = if ($mode -eq 'layouts') { 2700000 } else { 600000 }
        Run "collection-$mode" $python $arguments -TimeoutMs $collectionTimeout
        $admission=Get-Content -Encoding UTF8 -LiteralPath "$outPath/collection-$mode/results.json" -Raw | ConvertFrom-Json
        if (!$admission.Complete -or !$admission.InputsUnchanged) { throw "Incomplete collection $mode evidence" }
    }
    Run 'array-input' $python @('tools/check_stdlib_array_input.py','--out',"$outPath/array-input",'--public-directory',$PublicDirectory) -TimeoutMs 900000
    $arrayInput=Get-Content -Encoding UTF8 -LiteralPath "$outPath/array-input/results.json" -Raw | ConvertFrom-Json
    if (!$arrayInput.Complete -or !$arrayInput.InputsUnchanged -or $arrayInput.Sweeps.Count -ne 26 -or $arrayInput.MetadataSweeps.Count -ne 14) { throw 'Incomplete Array/Future input evidence' }
    Run 'array-future' $python @('tools/check_stdlib_array_future.py','--out',"$outPath/array-future") -TimeoutMs 900000
    $arrayFuture=Get-Content -Encoding UTF8 -LiteralPath "$outPath/array-future/results.json" -Raw | ConvertFrom-Json
    if (!$arrayFuture.Complete -or !$arrayFuture.InputsUnchanged -or $arrayFuture.Sweeps.Count -ne 42) { throw 'Incomplete Future Array native evidence' }
    $unchanged = !@($inputs | Where-Object { (Get-FileHash -LiteralPath $_.Path).Hash -ne $_.Sha256 }).Count
    $report = [pscustomobject]@{Complete=$unchanged;InputsUnchanged=$unchanged;Checks=$rows.Count;Inputs=$inputs;
        PositiveFixtures=$cases;ExtraFixtures=$extraCases;NegativeFixtures=$negative;Sweeps=$sweeps;Results=$rows;
        Scope='Windows actual strict C/JIT/AOT/pruned Stack artifacts: owned/try, seven tagged scalar profiles and nullable string presence/equality/hash/match, exact carrier identity, padding-independent compare/hash, literal fit/widening, parameter isolation, continuation/finally lifetime and pre-output mutation refusal. Nullable text Queue/AVL transport preserves absence and exact lengths. Old Into methods, IR and native exports retired. Nullable values/raw scalar queue exclude descriptors; consumers select only used profiles. Native and calling-thread language OOM prefixes. Batch and other 53-module acceptance remain pending.'}
    [IO.File]::WriteAllText("$outPath/results.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    if (!$unchanged) { throw 'Stack inputs changed during acceptance' }
    Write-Host "Stack native Windows: $($rows.Count) checks"
} finally { Pop-Location }
