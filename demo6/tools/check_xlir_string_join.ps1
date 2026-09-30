param(
    [Parameter(Mandatory=$true)][string]$PublicSourceDirectory,
    [string]$WorkDirectory = '',
    [string]$Root = ''
)
$ErrorActionPreference = 'Stop'
if (!$Root) { $Root = Split-Path -Parent $PSScriptRoot }
$Root = (Resolve-Path -LiteralPath $Root).Path
. (Join-Path $Root 'tools/extlib_smoke_process.ps1')
$compilerDir = (Resolve-Path -LiteralPath (Join-Path $Root $PublicSourceDirectory)).Path
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
    'xlir_string_chars_bad_result','xlir_string_chars_bad_parameter')
$objects = @($build.Sources | Where-Object {$_ -like 'src/runtime/*' -or $_ -eq 'tools/xrt_single_header_impl.c'} |
    ForEach-Object {Join-Path $compilerDir ([IO.Path]::GetFileNameWithoutExtension($_)+'.o')})
$paths = @($build.Inputs.Path) + @($driver,$gcc,$PSCommandPath,(Join-Path $compilerDir 'build.json')) + $objects
$paths += @($artifactPath,(Join-Path $Root $artifact.Mapping)) + @($artifact.Files.Source) + @($artifact.Files.Path)
$paths += @($source,$failure,$example,$cExample,'test/cases/xlir_string_bridge_minimal.xl',
    'test/cases/xlir_string_bridge_abi_host.c','tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1',
    'test/cases/stdlib_string.xl','test/cases/stdlib_string_support.c',
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
    $fallible = @('join','split','split_lines','repeat','replace','slice','bytes','chars')
    foreach ($name in @('byte_len','byte_at','starts_with','ends_with')+$fallible) {
        $targets = @($ir.callTargets | Where-Object cName -eq "xl6_xrt_string_$name")
        $effect = if ($name -in $fallible) {'may-raise'} else {'no-raise'}
        $physical = if ($name -in $fallible) {'xrt-thread-error'} else {'none'}
        if (!$targets.Count) { throw "Missing string contract: $name" }
        foreach ($target in $targets) {
            if ($target.runtimeBridgeName -ne "string-$($name.Replace('_','-'))" -or
                $target.errorAbiName -ne $effect -or $target.errorPhysicalAbiName -ne $physical -or
                $target.resultConversionAbiName -ne 'direct') { throw "Invalid string contract: $name" }
        }
    }
    $take = @($ir.callTargets | Where-Object cName -eq 'xl6_xrt_take_cstr')
    if ($take.Count -ne 1 -or $take[0].errorPhysicalAbiName -ne 'xrt-thread-error') { throw 'takeCstr lost its failure contract' }
    foreach ($negative in $negatives) {
        $null = Check $negative $driver @("test/cases/$negative.xl",'emit',(Join-Path $work $negative)) 'string runtime bridge has no exact parameter/result ABI'
    }
    $null = Check 'minimal' $driver @('test/cases/xlir_string_bridge_minimal.xl','emit',(Join-Path $work 'minimal'))
    $minimal = Get-Content (Join-Path $work 'minimal.c') -Raw
    if ($minimal -match 'xl6_xrt_string_(join|split|replace|slice|bytes|chars)\(' -or $minimal -match 'xlir_xvalue_to_string\(') { throw 'Unused string adapters were emitted' }

    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label',
        '-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-missing-field-initializers',
        "-I$compilerDir/include",'-Irelease/include','-Ilib/xrt','-Ilib/xruntime','-I.')
    $native = Join-Path $work 'native.exe'
    $null = Check 'native-build' $gcc ($flags+@('-include',"$module.c",'test/cases/xlir_string_bridge_abi_host.c')+
        $objects+@('-lWs2_32','-lIPHLPAPI','-lShell32','-o',$native))
    $nativeOutput = Check 'native' $native @()
    if ($nativeOutput -notmatch 'string join allocation failures ok' -or
        $nativeOutput -notmatch 'string join threads ok: 2000 rounds' -or
        $nativeOutput -notmatch 'string enumeration failures ok' -or
        $nativeOutput -notmatch 'string bridges ok') { throw 'Native oracle did not finish' }
    foreach ($entry in @(@('boundary',$source),@('allocation',$failure),
            @('regression','test/fixtures/extlib_boundary_pending/string_join_symbol.xl'))) {
        $null = Check "$($entry[0])-jit" $driver @($entry[1],'run',(Join-Path $work "$($entry[0])-jit"))
        $prefix = Join-Path $work "$($entry[0])-aot"
        $null = Check "$($entry[0])-build" $driver @($entry[1],'exe',$prefix)
        $null = Check "$($entry[0])-aot" "$prefix.exe" @()
    }
    $null = Check 'relocated-module' $driver @($source,'module',(Join-Path $work 'relocated-module'))
    $null = Check 'stdlib-jit' $driver @('test/cases/stdlib_string.xl','run',(Join-Path $work 'stdlib-jit'),'test/cases/stdlib_string_support.c')
    $stdlib = Join-Path $work 'stdlib-aot'
    $null = Check 'stdlib-build' $driver @('test/cases/stdlib_string.xl','exe',$stdlib,'test/cases/stdlib_string_support.c')
    $null = Check 'stdlib-aot' "$stdlib.exe" @()
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
        scope='Current-source compiler/public API; exact string bridge contracts, deterministic C/H/IR, negative ABIs, native OOM, language OOM JIT/AOT, module relocation and C/xlang examples. Not formal release, Linux, or whole ecosystem admission.';
        inputs=$inputs;changedInputs=$changed;checks=$checks.ToArray()}
    [IO.File]::WriteAllText((Join-Path $work 'results.json'),($record | ConvertTo-Json -Depth 8),$utf8)
}
if ($outcome -ne 'passed-scoped') { throw "String gate failed: $work/results.json" }
Write-Output "String join and allocation-error contracts: $($checks.Count) checks passed."
