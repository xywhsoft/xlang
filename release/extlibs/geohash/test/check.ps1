param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/geohash/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('geohash_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/geohash') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_geohash.xl', 'release/examples/extlibs/geohash.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[geohash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function RunC([string]$name, [string]$source) {
    Run $name 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', $runner, '-Package', 'geohash', '-Source', $source)
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/geohash/include') +
        $moduleFlags + @('release/extlibs/geohash/geohash_native.c'))
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/geohash/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_geohash.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/geohash/geohash_native.c', 'release/extlibs/geohash/test/test_geohash.c',
        $runtime, '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    $null = RunC 'C boundaries' 'release/extlibs/geohash/test/test_geohash.c'
    $threadsOutput = RunC 'C threads' 'release/extlibs/geohash/test/test_threads.c'
    if ($threadsOutput -cne 'geohash threads: PASS') {
        throw "unexpected C threads output: $threadsOutput"
    }
    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_geohash.xl')
    $boundariesOutput = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/geohash/test/boundaries.xl')
    if ($boundariesOutput -cne 'geohash boundaries: PASS') {
        throw "unexpected XLang boundaries output: $boundariesOutput"
    }
    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/geohash/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($entry in @{encode=1; bbox=1; neighbor=4}.GetEnumerator()) {
        $pattern = '\b\w+\.Data\s*=\s*xgeohash_' + $entry.Key + '\s*\('
        if ([regex]::Matches($generatedText, $pattern).Count -ne $entry.Value) {
            throw "geohash probe must cover $($entry.Key) $($entry.Value) times"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'geohash must not copy text after the native result is adopted'
    }
    Write-Host '[geohash] direct string adoption: PASS'
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/geohash/test/allocations.xl')
    $null = Run 'XLang owned result budgets' $xl @('run',
        'release/extlibs/geohash/test/owned_results.xl')
    $cOutput = RunC 'C example' 'release/extlibs/geohash/examples/geohash_c.c'
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/geohash/examples/geohash_c.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs from C runner example' }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/geohash.xl')
    $goldenPath = Join-Path $root 'release/extlibs/geohash/examples/geohash.expected.txt'
    $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
        throw "C/XLang geohash output differs from fixed golden output:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[geohash] C/XLang golden output: PASS'

    if (-not $SkipAot) {
        $owned = Join-Path $work 'owned_results.exe'
        $null = Run 'owned result AOT build' $xl @('build',
            'release/extlibs/geohash/test/owned_results.xl', '-o', $owned)
        $null = Run 'owned result AOT run' $owned @()
        $core = Join-Path $work 'core.exe'
        $null = Run 'core AOT build' $xl @('build',
            'test/cases/extlib_geohash.xl', '-o', $core)
        if ((Run 'core AOT run' $core @()) -cne '') { throw 'core AOT produced output' }

        $example = Join-Path $work 'example.exe'
        $null = Run 'example AOT build' $xl @('build',
            'release/examples/extlibs/geohash.xl', '-o', $example)
        if ((Run 'example AOT run' $example @()) -cne $golden) {
            throw 'example AOT output differs from fixed golden output'
        }

        $boundaries = Join-Path $work 'boundaries.exe'
        $null = Run 'boundaries AOT build' $xl @('build',
            'release/extlibs/geohash/test/boundaries.xl', '-o', $boundaries)
        if ((Run 'boundaries AOT run' $boundaries @()) -cne $boundariesOutput) {
            throw 'boundaries AOT output differs from JIT'
        }

        $allocations = Join-Path $work 'allocations.exe'
        $null = Run 'allocations AOT build' $xl @('build',
            'release/extlibs/geohash/test/allocations.xl', '-o', $allocations)
        $null = Run 'allocations AOT run' $allocations @()

        $cExample = Join-Path $work 'c_example.exe'
        $null = Run 'C example AOT build' $xl @('build',
            'release/extlibs/geohash/examples/geohash_c.c', '-o', $cExample)
        if ((Run 'C example AOT run' $cExample @()) -cne $golden) {
            throw 'C example AOT output differs from fixed golden output'
        }
    }
    if ($SkipAot) {
        Write-Host '[geohash] Windows C/XLang quick gate: PASS (AOT skipped)'
    } else {
        Write-Host '[geohash] Windows C/XLang/AOT gate: PASS'
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "geohash validation input changed: $path"
        }
    }
    $passed = $true
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('geohash_guard_')) {
        throw "geohash test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        if ($passed) {
            Get-ChildItem -LiteralPath $resolvedWork -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolvedWork -Force
        } else {
            Write-Warning "geohash failure artifacts retained: $resolvedWork"
        }
    }
}
