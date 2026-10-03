param(
    [Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [string]$WorkDirectory = '',
    [string]$Root = ''
)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $Root 'tools/extlib_smoke_process.ps1')
$compilerDir = (Resolve-Path -LiteralPath $(if ([IO.Path]::IsPathRooted($PublicSourceDirectory)) {
    $PublicSourceDirectory
} else {Join-Path $Root $PublicSourceDirectory})).Path
$build = Get-Content (Join-Path $compilerDir 'build.json') -Raw -Encoding utf8 | ConvertFrom-Json
$driver = $build.Driver
if ($build.RuntimeOnly -or (Get-FileHash $driver).Hash -ne $build.DriverSha256) { throw 'Invalid public source compiler' }
foreach ($input in $build.Inputs) {
    if ((Get-FileHash -LiteralPath $input.Path).Hash -ne $input.Sha256) { throw "Compiler source changed: $($input.Path)" }
}
# 场景调度器预先准备共享源码；工作项只读取并验证，避免并行覆盖。
$artifactPath = Join-Path $compilerDir 'artifact_sources.json'
if (!(Test-Path -LiteralPath $artifactPath)) {
    Push-Location $Root
    try { & ./tools/stage_xlir_artifact_runtime_sources.ps1 -Directory $compilerDir }
    finally { Pop-Location }
}
$artifact = Get-Content -LiteralPath $artifactPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($artifact.DriverSha256 -ne $build.DriverSha256 -or $artifact.Files.Count -lt 20 -or
    (Get-FileHash -LiteralPath (Join-Path $Root $artifact.Mapping)).Hash -ne $artifact.MappingSha256 -or
    (Get-FileHash -LiteralPath (Join-Path $Root 'tools/stage_xlir_artifact_runtime_sources.ps1')).Hash -ne $artifact.StageScriptSha256) {
    throw 'Staged runtime source package no longer matches its compiler inputs'
}
foreach ($file in $artifact.Files) {
    if ((Get-FileHash -LiteralPath $file.Source).Hash -ne $file.Sha256 -or
        (Get-FileHash -LiteralPath $file.Path).Hash -ne $file.Sha256) { throw "Staged source changed: $($file.Path)" }
}
if (!$WorkDirectory) { $WorkDirectory = 'build/test/string-join-' + (Get-Date -Format 'yyyyMMdd-HHmmss') }
$work = [IO.Path]::GetFullPath((Join-Path $Root $WorkDirectory))
if (Test-Path -LiteralPath $work) { throw 'Choose a fresh gate directory' }
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc.exe -ErrorAction Stop).Source
$checks = [Collections.Generic.List[object]]::new()
$inputs = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$source = 'test/cases/xlir_string_bridge_abi.xl'
$failure = 'test/cases/xlir_string_join_failure.xl'
$example = 'release/examples/stdlib_string_boundaries.xl'
$cExample = 'release/examples/stdlib_string_boundaries_c.c'
$negatives = @('xlir_string_bridge_bad_result','xlir_string_bridge_bad_parameter',
    'xlir_string_join_bad_result','xlir_string_join_bad_parameter',
    'xlir_string_bytes_bad_result','xlir_string_bytes_bad_parameter',
    'xlir_string_chars_bad_result','xlir_string_chars_bad_parameter',
    'xlir_string_repeat_bad_result','xlir_string_repeat_bad_parameter')
$objects = @($build.Sources | Where-Object {$_ -like 'src/runtime/*' -or $_ -in @('tools/xrt_single_header_impl.c','src/api/xl_task_group_bridge.c')} |
    ForEach-Object {Join-Path $compilerDir ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
$paths = @($build.Inputs.Path) + @($build.Headers.Path) + @($build.Objects.Path) +
    @($driver,$gcc,$PSCommandPath,(Join-Path $compilerDir 'build.json')) + $objects
$paths += @($artifactPath,(Join-Path $Root $artifact.Mapping)) + @($artifact.Files.Source) + @($artifact.Files.Path)
$paths += @($source,$failure,$example,$cExample,'test/cases/xlir_string_bridge_minimal.xl',
    'test/cases/xlir_string_bridge_abi_host.c','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'test/cases/stdlib_string.xl','test/cases/stdlib_string_support.c',
    'test/cases/xlir_string_repeat_abi.xl','test/cases/xlir_string_repeat_host.c',
    'test/cases/stdlib_text_interpolation.xl','test/cases/xlir_string_interpolation_host.c',
    'test/cases/stdlib_text_interpolation_error.xl',
    'test/cases/stdlib_text_literals_nested.xl','test/cases/xlir_literal_span_contract.c',
    'test/fuzz/xl6_frontend_fuzz.c',
    'test/cases/cgen_fstring.xl',
    'test/cases/parse_fstring_unclosed_comment.xl','test/cases/parse_fstring_mismatched_group.xl',
    'test/cases/parse_fstring_unclosed_inner_quote.xl','test/cases/parse_fstring_bad_time.xl',
    'test/cases/parse_fstring_unclosed.xl','test/cases/parse_fstring_empty_expr.xl','test/cases/parse_fstring_bad_brace.xl',
    'test/cases/sem_fstring_nested_undeclared_identifier.xl','test/cases/sem_fstring_undeclared_identifier.xl',
    'test/cases/sem_fstring_multiline_undeclared_identifier.xl',
    'test/cases/xlir_text_value_contract.c','test/support/string_bridge_native.c',
    'tools/check_xlir_string_bridges.ps1','tools/check_xlir_string_repeat.ps1',
    'tools/stage_xlir_artifact_runtime_sources.ps1','test/fixtures/extlib_boundary_pending/string_join_symbol.xl') |
    ForEach-Object {Join-Path $Root $_}
$paths += $negatives | ForEach-Object {Join-Path $Root "test/cases/$_.xl"}
foreach ($path in @($paths | Sort-Object -Unique)) { $inputs[$path] = (Get-FileHash -LiteralPath $path).Hash }
function Check([string]$name,[string]$program,[string[]]$arguments,[string]$expectedError = '') {
    $r = Invoke-ExtlibSmokeProcess $program $arguments $Root 120 @{}
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$r})
    [IO.File]::WriteAllText((Join-Path $work "$name.log"),$r.stdout+$r.stderr,$utf8)
    if ($expectedError) {
        if ($r.status -ne 'fail' -or $r.exitCode -ne 1 -or $r.stderr -notmatch $expectedError) { throw "$name was not rejected correctly: $($r.stderr)" }
    } elseif ($r.status -ne 'pass') { throw "$name failed: $($r.stderr) $($r.stdout)" }
    Write-Host "[string] ${name}: checked"
    return $r.stdout.Replace("`r`n","`n").TrimEnd("`n")
}
$outcome = 'failed'
Push-Location $Root
try {
    $module = Join-Path $work 'module'
    $null = Check 'emit' $driver @($source,'emit',$module)
    $null = Check 'emit-repeat' $driver @($source,'emit',"$module.repeat")
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$module.$extension").Hash -ne (Get-FileHash "$module.repeat.$extension").Hash) { throw "Non-deterministic $extension emission" }
    }
    $ir = Get-Content "$module.json" -Raw -Encoding utf8 | ConvertFrom-Json
    $signatures = [ordered]@{Length=2;ByteLength=2;ByteAt=3;StartsWith=5;EndsWith=5;
        Join=4;Split=4;Lines=2;Repeat=3;Replace=6;Slice=4;Bytes=2;Chars=2}
    foreach ($name in $signatures.Keys) {
        $targets = @($ir.callTargets | Where-Object cName -eq "xl6Text${name}V1")
        if (!$targets.Count) { throw "Missing string contract: $name" }
        foreach ($target in $targets) {
            if ($target.runtimeBridgeName -ne 'text-value' -or $target.parameterCount -ne $signatures[$name] -or
                $target.parameterAbis[0] -ne 2 -or $target.errorAbiName -ne 'may-raise' -or
                $target.errorPhysicalAbiName -ne 'xrt-thread-error' -or
                $target.resultConversionAbiName -ne 'direct') { throw "Invalid string contract: $name" }
        }
    }
    foreach ($negative in $negatives) {
        $prefix = Join-Path $work $negative
        $null = Check $negative $driver @("test/cases/$negative.xl",'emit',$prefix) 'XLIR direct runtime bridge has no exact declared ABI|has no exact compiler-runtime bridge ABI'
        if ((Test-Path "$prefix.c") -or (Test-Path "$prefix.h")) {throw "Unsafe ABI output exists: $negative"}
    }
    $null = Check 'minimal' $driver @('test/cases/xlir_string_bridge_minimal.xl','emit',(Join-Path $work 'minimal'))
    $minimal = Get-Content (Join-Path $work 'minimal.c') -Raw
    if ($minimal -notmatch 'xl6TextByteLengthV1\(' -or
        $minimal -match 'xl6Text(?:Join|Split|Lines|Replace|Slice|Bytes|Chars)V1\(' -or
        $minimal -match 'xl6_xrt_(?:string_|take_cstr)|xlir_xvalue_to_string\(') { throw 'Unused string adapters were emitted' }
    $generated = Get-Content "$module.c" -Raw
    if ($generated -match '(?m)^\s*(?:static\s+)?(?:xstrview|xlir_string|xvalue\s*\*)\s+xl6Text\w+\([^;]*\)\s*\{' -or
        $generated -match 'xl6_xrt_(?:string_|take_cstr)|typedef struct xlir_string_list') {throw 'Cgen contains a text implementation'}

    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers','-Wno-format-truncation','-Wno-pointer-sign',
        "-I$compilerDir/include",'-Irelease/include','-Ilib/xrt','-Ilib/xruntime','-I.')
    $native = Join-Path $work 'native.exe'
    $null = Check 'native-build' $gcc ($flags+@('-include',"$module.c",'test/cases/xlir_string_bridge_abi_host.c')+
        $objects+@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$native))
    $nativeOutput = Check 'native' $native @()
    if ($nativeOutput -notmatch 'string join allocation failures ok' -or
        $nativeOutput -notmatch 'string join threads ok: 2000 rounds' -or
        $nativeOutput -notmatch 'string enumeration failures ok' -or
        $nativeOutput -notmatch 'string bridges ok') { throw 'Native oracle did not finish' }
    $repeat = Join-Path $work 'repeat'
    $null = Check 'repeat-emit' $driver @('test/cases/xlir_string_repeat_abi.xl','emit',$repeat)
    $null = Check 'repeat-emit-again' $driver @('test/cases/xlir_string_repeat_abi.xl','emit',"$repeat.again")
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$repeat.$extension").Hash -ne (Get-FileHash "$repeat.again.$extension").Hash) {throw "Nondeterministic repeat $extension"}
    }
    $repeatC = Get-Content "$repeat.c" -Raw
    if ($repeatC -notmatch 'xl6TextRepeatV1\(' -or $repeatC -match 'xl6Text(?:Replace|Split|Join)V1\(') {throw 'Repeat-only calls are not minimal'}
    $repeatExe = Join-Path $work 'repeat.exe'
    $null = Check 'repeat-strict-build' $gcc ($flags+@('-include',"$repeat.c",'test/cases/xlir_string_repeat_host.c')+
        $objects+@('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$repeatExe))
    if ((Check 'repeat-native' $repeatExe @()) -notmatch '600 cases, 300 real allocation failures, balanced memory') {throw 'Repeat native oracle incomplete'}
    $contract = Join-Path $work 'contract.exe'
    $compilerObjects = @($build.Objects | Where-Object {[IO.Path]::GetFileName($_.Path) -ne 'xlir_public_source_driver.o'} | ForEach-Object Path)
    $null = Check 'contract-build' $gcc ($flags+@('-DXL_STATIC_EMBED','-Ilib/tcc','test/cases/xlir_text_value_contract.c')+
        $compilerObjects+@('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$contract))
    Copy-Item -LiteralPath release/xl.dll -Destination (Join-Path $work 'xl.dll')
    $literal = Join-Path $work 'literal-span.exe'
    $null = Check 'literal-build' $gcc ($flags+@('test/cases/xlir_literal_span_contract.c')+
        $compilerObjects+@('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$literal))
    if ((Check 'literal-native' $literal @()) -notmatch 'literal-span contract: 101\d+ checks passed') {throw 'Literal oracle incomplete'}
    $fuzz = Join-Path $work 'frontend-fuzz.exe'
    $null = Check 'frontend-fuzz-build' $gcc ($flags+@('test/fuzz/xl6_frontend_fuzz.c')+
        $compilerObjects+@('release/xl.dll','-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$fuzz))
    if ((Check 'frontend-fuzz' $fuzz @('10000','1481397778')) -notmatch 'frontend fuzz ok: 10000 iterations') {throw 'Frontend fuzz incomplete'}
    foreach ($negative in @(
            @('parse_fstring_unclosed_comment','未闭合的块注释'),
            @('parse_fstring_mismatched_group','括号不匹配'),
            @('parse_fstring_unclosed_inner_quote','未闭合'),@('parse_fstring_bad_time','未闭合的时间字面量'),
            @('parse_fstring_unclosed','未闭合'),@('parse_fstring_empty_expr','不能为空'),
            @('parse_fstring_bad_brace','单独的'),
            @('sem_fstring_nested_undeclared_identifier',':1:16: \[sem\].*missingNested'),
            @('sem_fstring_undeclared_identifier',':2:28: \[sem\].*missingValue'),
            @('sem_fstring_multiline_undeclared_identifier',':3:8: \[sem\].*missingValue'))) {
        $prefix = Join-Path $work $negative[0]
        $null = Check $negative[0] $driver @("test/cases/$($negative[0]).xl",'emit',$prefix) $negative[1]
        foreach ($extension in @('c','h','json')) {
            if (Test-Path -LiteralPath "$prefix.$extension") {throw 'Malformed interpolation emitted an artifact'}
        }
    }
    foreach ($entry in @(@('boundary',$source),@('repeat','test/cases/xlir_string_repeat_abi.xl'),@('allocation',$failure),
            @('interpolation','test/cases/stdlib_text_interpolation.xl'),
            @('nested-literals','test/cases/stdlib_text_literals_nested.xl'),
            @('interpolation-error','test/cases/stdlib_text_interpolation_error.xl'),
            @('regression','test/fixtures/extlib_boundary_pending/string_join_symbol.xl'))) {
        $closed = Join-Path $work "$($entry[0])-contract.c"
        $null = Check "$($entry[0])-contract" $contract @($entry[1],$closed)
        $null = Check "$($entry[0])-default" $driver @($entry[1],'cgen-default',(Join-Path $work "$($entry[0])-default.c"))
        if ((Get-FileHash $closed).Hash -ne (Get-FileHash (Join-Path $work "$($entry[0])-default.c")).Hash) {throw 'Closed ABI emission differs from default compiler'}
        $null = Check "$($entry[0])-jit" $driver @($entry[1],'run',(Join-Path $work "$($entry[0])-jit"))
        $prefix = Join-Path $work "$($entry[0])-aot"
        $null = Check "$($entry[0])-build" $driver @($entry[1],'exe',$prefix)
        $null = Check "$($entry[0])-aot" "$prefix.exe" @()
    }
    $null = Check 'relocated-module' $driver @($source,'module',(Join-Path $work 'relocated-module'))
    $interpolation = Join-Path $work 'interpolation-native.exe'
    $null = Check 'interpolation-strict-build' $gcc ($flags+@('-include',(Join-Path $work 'interpolation-default.c'),
        'test/cases/xlir_string_interpolation_host.c')+$objects+
        @('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$interpolation))
    $null = Check 'interpolation-native' $interpolation @()
    for ($point=0; $point -le 256; ++$point) {
        $output = Check "interpolation-oom-$point" $interpolation @("$point")
        if ($output -match 'hit=0 balanced') {break}
    }
    if ($point -gt 256) {throw 'Incomplete interpolation allocation sweep'}
    $nested = Join-Path $work 'nested-literals-native.exe'
    $null = Check 'nested-literals-strict-build' $gcc ($flags+@('-include',(Join-Path $work 'nested-literals-default.c'),
        'test/cases/xlir_string_interpolation_host.c')+$objects+
        @('-lWs2_32','-lIPHLPAPI','-lShell32','-lOle32','-lAdvapi32','-o',$nested))
    $null = Check 'nested-literals-native' $nested @()
    for ($point=0; $point -le 512; ++$point) {
        $output = Check "nested-literals-oom-$point" $nested @("$point")
        if ($output -match 'hit=0 balanced') {break}
    }
    if ($point -gt 512) {throw 'Incomplete nested interpolation allocation sweep'}
    $null = Check 'stdlib-jit' $driver @('test/cases/stdlib_string.xl','run',(Join-Path $work 'stdlib-jit'),'test/cases/stdlib_string_support.c')
    $stdlib = Join-Path $work 'stdlib-aot'
    $null = Check 'stdlib-build' $driver @('test/cases/stdlib_string.xl','exe',$stdlib,'test/cases/stdlib_string_support.c')
    $null = Check 'stdlib-aot' "$stdlib.exe" @()
    $null = Check 'legacy-fstring-jit' $driver @('test/cases/cgen_fstring.xl','run',(Join-Path $work 'legacy-fstring-jit'),'test/cases/stdlib_string_support.c')
    $legacy = Join-Path $work 'legacy-fstring-aot'
    $null = Check 'legacy-fstring-build' $driver @('test/cases/cgen_fstring.xl','exe',$legacy,'test/cases/stdlib_string_support.c')
    $null = Check 'legacy-fstring-aot' "$legacy.exe" @()
    $xlOutput = Check 'example-xl' $driver @($example,'run',(Join-Path $work 'example-xl'))
    $xlOutput = ($xlOutput -split "`n" | Where-Object {$_ -cne 'public source run complete'}) -join "`n"
    $cExe = Join-Path $work 'example-c.exe'
    $null = Check 'example-c-build' $gcc ($flags+@($cExample,(Join-Path $compilerDir 'xrt_single_header_impl.o'),
        '-lWs2_32','-lIPHLPAPI','-lShell32','-o',$cExe))
    $cOutput = Check 'example-c' $cExe @()
    if ($xlOutput -cne $cOutput -or ($xlOutput -split "`n").Count -ne 21) { throw 'Bilingual examples differ' }
    $outcome = 'passed-scoped'
} finally {
    Pop-Location
    $changed = @($inputs.Keys | Where-Object {!(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -ne $inputs[$_]})
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $record = [ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;compiler=$driver;buildManifest=(Join-Path $compilerDir 'build.json');
        scope='Current-source compiler/public API; two-field strings, typed arrays/chars/bytes, 10 negative ABI declarations and 10 interpolation negatives, shared literal delimiter/position/bounded fuzz contract and 10000 actual frontend fuzz iterations, in-memory ABI/effect/ownership mutations, deterministic C/H/IR, native OOM and 4-thread 2000 joins, 600 repeat probes, nested interpolation OOM JIT/AOT, relocation and C/xlang examples. Host links resident object set; actual standalone pruning is a separate artifact gate. Not full RC0 or 53-module completion.';
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 8),$utf8)
}
if ($outcome -ne 'passed-scoped') { throw "String gate failed: $work/results.json" }
Write-Output "String join and allocation-error contracts: $($checks.Count) checks passed."
