param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/geohash/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('geohash_guard_' + [guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
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
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/geohash/include') +
        $moduleFlags + @('release/extlibs/geohash/geohash_native.c'))
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
    $null = Run 'owned-text cgen' $xl @('cgen',
        'release/extlibs/geohash/test/boundaries.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $adoptions = [regex]::Matches($generatedText,
        '\bxl6CStringTakeV1\s*\(').Count
    # IR may specialize calls with owned arguments; check the six public returns.
    if ($adoptions -lt 6 -or $generatedText.Contains('xl6_xrt_take_cstr')) {
        throw "geohash returns must adopt C owners without copying (got $adoptions adoptions)"
    }
    Write-Host '[geohash] owned-text codegen: PASS'
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/geohash/test/allocations.xl')
    $cOutput = RunC 'C example' 'release/extlibs/geohash/examples/geohash_c.c'
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/geohash/examples/geohash_c.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs from GCC example' }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/geohash.xl')
    $goldenPath = Join-Path $root 'release/extlibs/geohash/examples/geohash.expected.txt'
    $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
        throw "C/XLang geohash output differs from fixed golden output:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[geohash] C/XLang golden output: PASS'

    if (-not $SkipAot) {
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
    }
    if ($SkipAot) {
        Write-Host '[geohash] Windows C/XLang quick gate: PASS (AOT skipped)'
    } else {
        Write-Host '[geohash] Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('geohash_guard_')) {
        throw "geohash test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
