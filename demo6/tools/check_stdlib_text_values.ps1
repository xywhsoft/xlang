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
        'stdlib_text_queries','stdlib_text_binary','stdlib_text_glob','stdlib_text_distance')
    $negative = @('bad_xlir_text_input','bad_xlir_text_result','bad_xlir_text_effect','bad_xlir_text_no_raise',
        'bad_xlir_text_chars_type','bad_xlir_text_byte_type','bad_xlir_text_length_width')
    $defaultsNegative = @('bad_extern_default_type','bad_extern_default_order')
    $files = @($cases + $negative + $defaultsNegative | ForEach-Object { Get-Item "test/cases/$_.xl" }) +
        @(Get-Item test/cases/stdlib_text_values_host.c,test/cases/xlir_text_value_contract.c,
            test/cases/stdlib_text_queries_host.c,test/support/text_queries_native.c,
            test/support/text_selected_native.c,test/cases/stdlib_string_support.c,
            tools/stage_xlir_artifact_runtime_sources.ps1,tools/xlir_native_process.ps1,$PSCommandPath,$gcc)
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
    Run 'native-build' $gcc ($strict + @('test/cases/stdlib_text_values_host.c','test/support/text_selected_native.c',
        'src/runtime/xl6_text_value.c','-o',"$outPath/native.exe"))
    Run 'native-run' "$outPath/native.exe" @()
    Run 'query-native-build' $gcc ($strict + @('test/cases/stdlib_text_queries_host.c','test/support/text_queries_native.c',
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
    $runtime = @($public.Objects | Where-Object { [IO.Path]::GetFileName($_.Path) -in @('xrt_single_header_impl.o','xl6_text_value.o',
        'xl6_text_query.o','xl6_text_binary.o','xl6_text_chars.o','xl6_text_glob.o','xl6_text_distance.o','xl6_slot.o','xl6_value_iterator.o') } | ForEach-Object Path)
    foreach ($name in $cases) {
        $source = "test/cases/$name.xl"
        Run "$name.emit" $driver @($source,'cgen-default',"$outPath/$name.c")
        Run "$name.repeat" $driver @($source,'cgen-default',"$outPath/$name.repeat.c")
        if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.repeat.c").Hash) { throw "Nondeterministic text output $name" }
        if ($name -ne 'cgen_extern_defaults') {
            Run "$name.contract" "$outPath/contract.exe" @($source,"$outPath/$name.contract.c")
            if ((Get-FileHash "$outPath/$name.c").Hash -ne (Get-FileHash "$outPath/$name.contract.c").Hash) { throw "ABI/Cgen mismatch $name" }
            $c = Get-Content -LiteralPath "$outPath/$name.c" -Raw
            if ($c -notmatch '#include <__xl_runtime/xl6_text_value.h>' -or
                $c -match 'xl6_xrt_(?:take_cstr|string_(?:lower|upper|trim|filter|replace|slice|repeat|reverse|compare|char_at|byte_at|chars|bytes|pad_\w+|like|approx|sim))\(' -or
                $c -match '(?m)^\s*(?:static\s+)?(?:xstrview|xlir_string)\s+xl6Text\w+\([^;]*\)\s*\{') { throw "Legacy/generated text implementation $name" }
            if ($name -eq 'stdlib_text_lower_only' -and $c -match 'xl6Text(?:Upper|Trim|Replace|Slice|Repeat|Reverse)V1\(') { throw 'Unused text calls retained' }
        }
        Run "$name.strict-build" $gcc ($strict + @('-include',"$outPath/$name.c",'-DXLIR_GENERATED_MODULE_PREINCLUDED',
            'test/cases/xlir_generated_module_host.c') + $runtime + @('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',"$outPath/$name.strict.exe"))
        Run "$name.strict-run" "$outPath/$name.strict.exe" @()
        Run "$name.jit" $driver @($source,'run',"$outPath/$name.jit")
        Run "$name.aot-build" $driver @($source,'exe',"$outPath/$name.aot")
        Run "$name.aot-run" "$outPath/$name.aot.exe" @()
    }
    foreach ($name in $negative) {
        $path = "$outPath/$name.c"
        Run $name $driver @("test/cases/$name.xl",'cgen-default',$path) 1
        if ((Get-Content "$outPath/$name.log" -Raw) -notmatch 'XLIR direct runtime bridge has no exact declared ABI' -or (Test-Path -LiteralPath $path)) { throw "Unsafe text ABI emitted $name" }
    }
    foreach ($name in $defaultsNegative) {
        $path = "$outPath/$name.c"
        Run $name $driver @("test/cases/$name.xl",'cgen-default',$path) 1
        $marker = if ($name -eq 'bad_extern_default_type') { 'extern default expression does not match parameter type' }
            else { 'required extern parameter follows an optional parameter' }
        if (!(Get-Content "$outPath/$name.log" -Raw).Contains($marker) -or (Test-Path -LiteralPath $path)) { throw "Invalid extern default emitted $name" }
    }
    foreach ($name in @('stdlib_string','stdlib_crt_env','cgen_container_ops','stdlib_log_core')) {
        $arguments = @("test/cases/$name.xl",'run',"$outPath/$name.regression")
        if ($name -eq 'stdlib_string') { $arguments += 'test/cases/stdlib_string_support.c' }
        Run "$name.regression" $driver $arguments
    }
    foreach ($record in $inputs) {
        if ((Get-FileHash -LiteralPath $record.Path).Hash -ne $record.Sha256) { throw "Text input changed $($record.Path)" }
    }
    $report = [pscustomobject]@{Inputs=$inputs;Rows=$rows;PublicBuild=$public;Scope='Text owned/query/char/bytes/glob/distance batches only; not 53-module completion or full RC0'}
    [IO.File]::WriteAllText("$outPath/summary.json", ($report | ConvertTo-Json -Depth 8), [Text.UTF8Encoding]::new($false))
    Write-Output "Text values: $($rows.Count) checks passed. Evidence: $outPath"
} finally { Pop-Location }
