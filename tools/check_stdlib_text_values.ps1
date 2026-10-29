param([string]$PublicDirectory = '', [string]$Directory = '')
$ErrorActionPreference = 'Stop'
$rootPath = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'xlir_native_process.ps1')
Push-Location $rootPath
try {
    if (!$Directory) { $Directory = 'build/test/text-values-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') }
    $outPath = [IO.Path]::GetFullPath($Directory)
    if (Test-Path -LiteralPath $outPath) { throw 'Preserve prior text evidence; choose a fresh directory' }
    New-Item -ItemType Directory -Path $outPath | Out-Null
    if (!$PublicDirectory) {
        $PublicDirectory = "$outPath/source"
        & "$PSScriptRoot/build_xlir_public_source_driver.ps1" -Directory $PublicDirectory
        if (!$?) { throw 'Current-source text compiler build failed' }
    }
    $public = Get-Content -LiteralPath (Join-Path $PublicDirectory 'build.json') -Raw | ConvertFrom-Json
    & "$PSScriptRoot/stage_xlir_artifact_runtime_sources.ps1" -Directory $PublicDirectory *> "$outPath/stage.log"
    if (!$?) { throw 'Text standalone source package failed to stage' }
    $driver = $public.Driver
    $gcc = (Get-Command gcc -CommandType Application).Source
    $rows = [Collections.Generic.List[object]]::new()
    $cases = @('stdlib_text_values','stdlib_text_sizes','stdlib_text_lower_only','cgen_extern_defaults',
        'stdlib_text_queries','stdlib_text_binary','stdlib_text_glob','stdlib_text_distance','stdlib_text_split','stdlib_text_join',
        'stdlib_text_split_only','stdlib_text_join_only','stdlib_text_iterator','stdlib_text_iterator_only',
        'stdlib_number_format','stdlib_number_format_only','stdlib_number_values','stdlib_number_integer_only',
        'stdlib_number_float_only','stdlib_crt_parse','cgen_overload_default_rank',
        'stdlib_unicode','stdlib_unicode_only','stdlib_unicode_validator','stdlib_html','stdlib_html_only',
        'stdlib_encoding','stdlib_encoding_only','stdlib_encoding_meta_only','stdlib_encoding_transcode_only','stdlib_unicode_repair',
        'stdlib_encoding_detect','stdlib_encoding_detect_only','cgen_native_product_i64x3','cgen_extern_native_product_i64x3','cgen_signed_integer_constants')
    $negative = @('bad_xlir_text_input','bad_xlir_text_result','bad_xlir_text_effect','bad_xlir_text_no_raise',
        'bad_xlir_text_chars_type','bad_xlir_text_byte_type','bad_xlir_text_length_width','bad_xlir_text_split_type','bad_xlir_text_join_type',
        'bad_xlir_text_iterator_result','bad_xlir_text_iterator_owner',
        'bad_xlir_text_iterator_handle_read','bad_xlir_text_iterator_handle_write',
        'bad_xlir_number_format_result','bad_xlir_number_format_width','bad_xlir_number_format_unsigned','bad_xlir_number_buffer_format_owner',
        'bad_xlir_number_parse_width','bad_xlir_number_parse_result','bad_xlir_number_parse_no_raise',
        'bad_xlir_number_string_unsigned','bad_xlir_number_buffer_string_owner',
        'bad_xlir_unicode_bytes','bad_xlir_unicode_char','bad_xlir_unicode_width',
        'bad_xlir_unicode_validator_owner','bad_xlir_unicode_validator_handle_read','bad_xlir_html_result',
        'bad_xlir_encoding_input','bad_xlir_encoding_result','bad_xlir_encoding_width',
        'bad_xlir_encoding_policy','bad_xlir_encoding_no_raise','bad_xlir_encoding_effect',
        'bad_xlir_encoding_detect_shape','bad_xlir_encoding_detect_width','bad_xlir_encoding_detect_unsigned','bad_xlir_encoding_detect_owner')
    $defaultsNegative = @('bad_extern_default_type','bad_extern_default_order',
        'bad_native_extern_default_type','bad_native_extern_default_order')
    $overloadRegressions = @('cgen_overload_costs','cgen_overloads','cgen_overload_varargs',
        'cgen_class_overloads','cgen_generic_class_overloads','cgen_generic_explicit_overload',
        'cgen_generic_overload_priority','cgen_generic_overload_var_priority','cgen_protocol_overload_index')
    $integerNegative = @('sem_int64_positive_overflow','sem_int_out_of_range_literal','sem_auto_int_overflow','sem_var_int_overflow')
    $integerRegressions = @('cgen_numeric_literals','operators_int_literal_forms','type_int_narrowing_wrap',
        'xlir_integer_conversion_source_abi','xlir_checked_integer_arithmetic_abi',
        'xlir_checked_integer_compound_error_abi','xlir_checked_integer_continuation_abi')
    $files = @($cases + $negative + $defaultsNegative + $overloadRegressions + $integerNegative + $integerRegressions | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @(Get-Item test/cases/stdlib_text_values_host.c,test/cases/xlir_text_value_contract.c,test/cases/cgen_extern_native_product_i64x3_provider.c,
            test/cases/xlir_signed_integer_constants_contract.c,test/cases/xlir_component_set.c,
            test/cases/stdlib_text_queries_host.c,test/support/text_queries_native.c,
            test/support/array_producer_test.h,
            test/cases/stdlib_text_split_host.c,test/support/text_split_native.c,
            test/cases/stdlib_text_iterator_host.c,test/support/text_iterator_native.c,
            test/cases/stdlib_text_iterator_generated_host.c,
            test/cases/stdlib_number_format_host.c,test/support/number_format_native.c,
            test/cases/stdlib_number_values_host.c,test/support/number_values_native.c,
            test/cases/stdlib_unicode_host.c,test/support/unicode_native.c,
            test/cases/stdlib_encoding_host.c,test/support/encoding_native.c,
            test/cases/stdlib_encoding_detect_host.c,test/support/encoding_detect_native.c,
            test/support/text_selected_native.c,test/cases/stdlib_string_support.c,
            tools/stage_xlir_artifact_runtime_sources.ps1,tools/xlir_native_process.ps1,release/xl.exe,$PSCommandPath,$gcc)
    $inputs = @($public.Inputs) + @($public.Headers) + @($public.Objects) +
        @($files | ForEach-Object { [pscustomobject]@{Path=$_.FullName;Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Stale text input $($record.Path)" }
    }
    function Run([string]$Name, [string]$File, [string[]]$Arguments, [int]$Expected = 0) {
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
        try {
            [void]$process.Start()
            $stdout = $process.StandardOutput.ReadToEndAsync()
            $stderr = $process.StandardError.ReadToEndAsync()
            if (!$process.WaitForExit(45000)) {
                Stop-XlirProcessTree $process
                throw "Timeout: $Name"
            }
            $code = $process.ExitCode
            [IO.File]::WriteAllText("$outPath/$Name.log", $stdout.Result + $stderr.Result, [Text.UTF8Encoding]::new($false))
        } finally { $process.Dispose() }
        if ($code -ne $Expected) { Get-Content "$outPath/$Name.log" -Tail 35; throw "$Name failed: $code" }
        $rows.Add([pscustomobject]@{Name=$Name;Exit=$code;Expected=$Expected})
    }
    $strict = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-pointer-sign','-Wno-format-truncation',
        '-I.','-Irelease','-Irelease/include',"-I$PublicDirectory/include")
    Copy-Item -LiteralPath release/xl.dll -Destination "$outPath/xl.dll"
    $objects = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o' } | ForEach-Object Path)
    Run 'contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c') + $objects +
        @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/contract.exe"))
    Run 'signed-contract-build' $gcc ($strict + @('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_signed_integer_constants_contract.c') + $objects +
        @('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/signed-contract.exe"))
    Run 'component-set-build' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror','-I.','test/cases/xlir_component_set.c','-o',"$outPath/component-set.exe")
    Run 'component-set-run' "$outPath/component-set.exe" @()
    Run 'component-set-tcc' ([IO.Path]::GetFullPath('release/xl.exe')) @('run','test/cases/xlir_component_set.c','--inc-path',$rootPath)
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_text_values_host.c','test/support/text_selected_native.c',
        'src/runtime/xl6_text_value.c','-o',"$outPath/native.exe"))
    Run 'native-run' "$outPath/native.exe" @()
    Run 'query-native-build' $gcc ($strict + @('test/cases/stdlib_text_queries_host.c','test/support/text_queries_native.c',
        'src/runtime/xl6_array_producer.c','src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_sequence.c',
        'src/runtime/xl6_array_erased.c','src/runtime/xl6_array_reference_value.c','src/runtime/xl6_value_admission.c',
        'src/runtime/xl6_string_type.c','src/runtime/xl6_string_value.c','src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c',
        'src/api/xl_foreign_error.c',
        'src/runtime/xl6_text_query.c','src/runtime/xl6_text_binary.c','src/runtime/xl6_text_chars.c',
        'src/runtime/xl6_text_glob.c','src/runtime/xl6_text_distance.c','-o',"$outPath/query-native.exe"))
    Run 'query-native-run' "$outPath/query-native.exe" @()
    foreach ($kind in @('bytes','chars','distance')) {
        for ($point = 1; $point -le 256; ++$point) {
            $name = "query-native.$kind.oom-$point"
            Run $name "$outPath/query-native.exe" @($kind,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 256) {throw "Incomplete native allocation sweep: $kind"}
    }
    Run 'split-native-build' $gcc ($strict + @('test/cases/stdlib_text_split_host.c','test/support/text_split_native.c',
        'src/runtime/xl6_array_producer.c','src/runtime/xl6_array_primitive_admission.c','src/runtime/xl6_array_erased.c',
        'src/runtime/xl6_array_reference_value.c','src/runtime/xl6_value_admission.c','src/runtime/xl6_string_type.c',
        'src/runtime/xl6_string_value.c','src/runtime/xl6_char_type.c','src/runtime/xl6_char_value.c','src/api/xl_foreign_error.c',
        'src/runtime/xl6_text_split.c','src/runtime/xl6_text_join.c','src/runtime/xl6_array_input.c','src/runtime/xl6_sequence.c','-o',"$outPath/split-native.exe"))
    Run 'split-native-run' "$outPath/split-native.exe" @()
    foreach ($kind in @('split','lines','fields','join','split-large','lines-large','fields-large','join-large')) {
        for ($point = 1; $point -le 512; ++$point) {
            $name = "split-native.$kind.oom-$point"
            Run $name "$outPath/split-native.exe" @($kind,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 512) {throw "Incomplete split allocation sweep: $kind"}
    }
    Run 'iterator-native-build' $gcc ($strict + @('test/cases/stdlib_text_iterator_host.c','test/support/text_iterator_native.c',
        'src/runtime/xl6_text_iterator.c','-o',"$outPath/iterator-native.exe"))
    Run 'iterator-native-run' "$outPath/iterator-native.exe" @()
    Run 'iterator-native-reentry' "$outPath/iterator-native.exe" @('reentry')
    foreach ($kind in @('0','1','2')) {
        for ($point = 1; $point -le 32; ++$point) {
            $name = "iterator-native.$kind.oom-$point"
            Run $name "$outPath/iterator-native.exe" @($kind,"$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 32) {throw "Incomplete iterator allocation sweep: $kind"}
    }
    Run 'number-native-build' $gcc ($strict + @('test/cases/stdlib_number_format_host.c','test/support/number_format_native.c',
        'src/runtime/xl6_number_format.c','src/runtime/xl6_number_format_buffer.c','-o',"$outPath/number-native.exe"))
    Run 'number-native-run' "$outPath/number-native.exe" @()
    foreach ($mode in 0..5) {
        for ($point = 1; $point -le 32; ++$point) {
            $name = "number-native.$mode.oom-$point"
            Run $name "$outPath/number-native.exe" @("$mode","$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 32) {throw "Incomplete number allocation sweep: $mode"}
    }
    Run 'number-value-native-build' $gcc ($strict + @('test/cases/stdlib_number_values_host.c','test/support/number_values_native.c',
        'src/runtime/xl6_number_integer.c','src/runtime/xl6_number_float.c',
        'src/runtime/xl6_number_integer_buffer.c','src/runtime/xl6_number_float_buffer.c','-o',"$outPath/number-value-native.exe"))
    Run 'number-value-native-run' "$outPath/number-value-native.exe" @()
    foreach ($mode in 0..8) {
        for ($point = 1; $point -le 32; ++$point) {
            $name = "number-value-native.$mode.oom-$point"
            Run $name "$outPath/number-value-native.exe" @("$mode","$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 32) {throw "Incomplete number value allocation sweep: $mode"}
    }
    Run 'unicode-native-build' $gcc ($strict + @('test/cases/stdlib_unicode_host.c','test/support/unicode_native.c',
        'src/runtime/xl6_unicode.c','src/runtime/xl6_unicode_validator.c','src/runtime/xl6_html.c',
        '-o',"$outPath/unicode-native.exe"))
    Run 'unicode-native-run' "$outPath/unicode-native.exe" @()
    foreach ($mode in 0..4) {
        for ($point = 1; $point -le 32; ++$point) {
            $name = "unicode-native.$mode.oom-$point"
            Run $name "$outPath/unicode-native.exe" @("$mode","$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 32) {throw "Incomplete unicode allocation sweep: $mode"}
    }
    Run 'encoding-native-build' $gcc ($strict + @('test/cases/stdlib_encoding_host.c','test/support/encoding_native.c',
        'src/runtime/xl6_encoding.c','src/runtime/xl6_encoding_meta.c','-o',"$outPath/encoding-native.exe"))
    Run 'encoding-native-run' "$outPath/encoding-native.exe" @()
    foreach ($mode in 0..5) {
        for ($point = 1; $point -le 32; ++$point) {
            $name = "encoding-native.$mode.oom-$point"
            Run $name "$outPath/encoding-native.exe" @("$mode","$point")
            if ((Get-Content "$outPath/$name.log" -Raw) -match 'hit=0 balanced') {break}
        }
        if ($point -gt 32) {throw "Incomplete encoding allocation sweep: $mode"}
    }
    Run 'encoding-detect-native-build' $gcc ($strict + @('test/cases/stdlib_encoding_detect_host.c','test/support/encoding_detect_native.c',
        'src/runtime/xl6_encoding_detect.c','-o',"$outPath/encoding-detect-native.exe"))
    Run 'encoding-detect-native-run' "$outPath/encoding-detect-native.exe" @()
    # This is a source-linked regression lane, NOT a pruned artifact witness.
    # Use the frozen source providers instead of a hand-maintained object subset
    # that silently omits Buffer/Array dependencies as facade fixtures evolve.
    $runtime = $objects + @('release/xl.dll')
    foreach ($name in $cases) {
        $source = "test/cases/$name.xl"
        Run "$name.emit" $driver @($source,'cgen-default',"$outPath/$name.c")
        Run "$name.repeat" $driver @($source,'cgen-default',"$outPath/$name.repeat.c")
        if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.repeat.c").Hash) { throw "Nondeterministic text output $name" }
        if ($name -in @('cgen_native_product_i64x3','cgen_extern_native_product_i64x3')) {
            Run "$name.contract" "$outPath/contract.exe" @($source,"$outPath/$name.contract.c")
            if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.contract.c").Hash) { throw "Generic product ABI/Cgen mismatch $name" }
        }
        if ($name -eq 'cgen_signed_integer_constants') {
            Run "$name.contract" "$outPath/signed-contract.exe" @($source,"$outPath/$name.contract.c")
            if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.contract.c").Hash) { throw 'Signed constant ABI/Cgen mismatch' }
        }
        if ($name -notin @('cgen_extern_defaults','cgen_overload_default_rank','cgen_native_product_i64x3','cgen_extern_native_product_i64x3','cgen_signed_integer_constants')) {
            Run "$name.contract" "$outPath/contract.exe" @($source,"$outPath/$name.contract.c")
            if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.contract.c").Hash) { throw "ABI/Cgen mismatch $name" }
            $c = Get-Content -LiteralPath "$outPath/$name.c" -Raw
            if ($c -notmatch '#include <__xl_runtime/xl6_text_value.h>' -or
                $c -match 'xl6_xrt_(?:take_cstr|string_(?:lower|upper|trim|filter|replace|slice|repeat|reverse|compare|char_at|byte_at|chars|bytes|pad_\w+|like|approx|sim|split|split_lines|join))\(' -or
                $c -match '(?m)^\s*(?:static\s+)?(?:xstrview|xlir_string)\s+xl6Text\w+\([^;]*\)\s*\{') { throw "Legacy/generated text implementation $name" }
            if ($name -match '^stdlib_number_|^stdlib_crt_parse' -and ($c -match 'xl6_xrt_string_(?:int|uint|num)_format\(|xl6_crt_parse_|xlir_crt_parse_finish' -or
                $c -match '(?m)^\s*(?:static\s+)?(?:xstrview|xlir_string)\s+xl6Number\w+\([^;]*\)\s*\{')) {throw 'Legacy/generated numeric implementation'}
            if ($name -eq 'stdlib_text_lower_only' -and $c -match 'xl6Text(?:Upper|Trim|Replace|Slice|Repeat|Reverse)V1\(') { throw 'Unused text calls retained' }
            if ($name -match '^stdlib_encoding|^stdlib_unicode_repair' -and ($c -match 'xl6_xrt_encoding_|xl6_xrt_take_cstr' -or
                $c -match '(?m)^\s*(?:static\s+)?(?:xvalue\*|xstrview|xlir_string)\s+xl6Encoding\w+\([^;]*\)\s*\{')) {throw 'Legacy/generated encoding implementation'}
            if ($name -eq 'stdlib_text_join_only' -and $c -match 'xl6Text(?:Split|Lines|Fields)V2\(') {throw 'Join-only retained split calls'}
            if ($name -eq 'stdlib_text_split_only' -and $c -match 'xl6TextJoinV1\(') {throw 'Split-only retained join calls'}
        }
        $hostSource = if ($name -match '^stdlib_text_iterator|^stdlib_number_|^stdlib_crt_parse|^stdlib_unicode|^stdlib_html|^stdlib_encoding') {'test/cases/stdlib_text_iterator_generated_host.c'} else {'test/cases/xlir_generated_module_host.c'}
        [string[]]$sidecar = if ($name -eq 'cgen_extern_native_product_i64x3') { @('test/cases/cgen_extern_native_product_i64x3_provider.c') } else { @() }
        $sidecarObjects = @()
        if ($sidecar.Count) {
            Run "$name.provider-build" $gcc ($strict + @('-c',$sidecar[0],'-o',"$outPath/$name.provider.o"))
            $sidecarObjects = @("$outPath/$name.provider.o")
        }
        Run "$name.strict-build" $gcc ($strict + @('-include',"$outPath/$name.c",'-DXLIR_GENERATED_MODULE_PREINCLUDED',
            $hostSource) + $sidecarObjects + $runtime + @('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/$name.strict.exe"))
        Run "$name.strict-run" "$outPath/$name.strict.exe" @()
        if ($name -match '^stdlib_text_iterator|^stdlib_number_|^stdlib_crt_parse|^stdlib_unicode|^stdlib_html|^stdlib_encoding') {
            for ($point=1; $point -le 256; ++$point) {
                $failure = "$name.managed-oom-$point"
                Run $failure "$outPath/$name.strict.exe" @("$point")
                if ((Get-Content "$outPath/$failure.log" -Raw) -match 'hit=0 balanced') {break}
            }
            if ($point -gt 256) {throw 'Incomplete managed text/number allocation sweep'}
        }
        Run "$name.jit" $driver (@($source,'run',"$outPath/$name.jit") + $sidecar)
        Run "$name.aot-build" $driver (@($source,'exe',"$outPath/$name.aot") + $sidecar)
        Run "$name.aot-run" "$outPath/$name.aot.exe" @()
    }
    foreach ($name in $negative) {
        $path = "$outPath/$name.c"
        Run $name $driver @("test/cases/$name.xl",'cgen-default',$path) 1
        $marker = if ($name -match '(?:iterator|validator)_handle_') {'private'} else {'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI|C ABI extern product requires a closed primitive product profile'}
        if ((Get-Content "$outPath/$name.log" -Raw) -notmatch $marker -or (Test-Path -LiteralPath $path)) { throw "Unsafe text ABI emitted $name" }
    }
    foreach ($name in $defaultsNegative) {
        $path = "$outPath/$name.c"
        Run $name $driver @("test/cases/$name.xl",'cgen-default',$path) 1
        $marker = switch ($name) {
            'bad_extern_default_type' {'extern default expression does not match parameter type'}
            'bad_extern_default_order' {'required extern parameter follows an optional parameter'}
            # ASCII source stays portable to Windows PowerShell 5.1 script decoding.
            'bad_native_extern_default_type' {-join ([char[]]@(40664,35748,20540,34920,36798,24335,31867,22411,21644,21442,25968,31867,22411,19981,21305,37197))}
            'bad_native_extern_default_order' {-join ([char[]]@(24517,36873,21442,25968,19981,33021,20986,29616,22312,21487,36873,21442,25968,20043,21518))}
        }
        if (!(Get-Content "$outPath/$name.log" -Raw -Encoding UTF8).Contains($marker) -or (Test-Path -LiteralPath $path)) { throw "Invalid extern default emitted $name" }
    }
    foreach ($name in @('stdlib_string','runtime_uint64_format','stdlib_crt_parse','stdlib_crt_env','cgen_container_ops','stdlib_log_core')) {
        $arguments = @("test/cases/$name.xl",'run',"$outPath/$name.regression")
        if ($name -eq 'stdlib_string') { $arguments += 'test/cases/stdlib_string_support.c' }
        Run "$name.regression" $driver $arguments
    }
    foreach ($name in $overloadRegressions) {
        Run "$name.regression" $driver @("test/cases/$name.xl",'run',"$outPath/$name.regression",'test/cases/stdlib_string_support.c')
    }
    foreach ($name in $integerNegative) {
        $path = "$outPath/$name.c"
        Run "$name.literal-negative" $driver @("test/cases/$name.xl",'cgen-default',$path) 1
        $marker = -join ([char[]]@(25972,25968,23383,38754,37327,36229,20986))
        if (!(Get-Content "$outPath/$name.literal-negative.log" -Raw -Encoding UTF8).Contains($marker) -or
            (Test-Path -LiteralPath $path)) {throw "Invalid integer literal emitted $name"}
    }
    foreach ($name in $integerRegressions) {
        Run "$name.regression" $driver @("test/cases/$name.xl",'run',"$outPath/$name.regression",'test/cases/stdlib_string_support.c')
    }
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Text input changed $($record.Path)" }
    }
    $report = [pscustomobject]@{Inputs=$inputs;Rows=$rows;PublicBuild=$public;Scope='B1-B10 text/numeric/Unicode-validator/HTML/charset/guess/native-product/signed-constant/component-set batches and default overload rank only; not 53-module completion or full RC0'}
    [IO.File]::WriteAllText("$outPath/summary.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    Write-Output "Text values: $($rows.Count) checks passed. Evidence: $outPath"
} finally { Pop-Location }
