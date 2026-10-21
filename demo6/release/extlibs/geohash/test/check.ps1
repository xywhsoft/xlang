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
    $output = & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")" }
    Write-Host "[geohash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

function RunC([string]$name, [string]$source) {
    $output = & $runner -Package geohash -Source $source 2>&1
    if ($LASTEXITCODE -ne 0) { throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")" }
    Write-Host "[geohash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/geohash/include') +
        $moduleFlags + @('release/extlibs/geohash/geohash_native.c'))
    $null = RunC 'C boundaries' 'release/extlibs/geohash/test/test_geohash.c'
    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_geohash.xl')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/geohash/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/geohash/test/allocations.xl')
    $cOutput = RunC 'C example' 'release/extlibs/geohash/examples/geohash_c.c'
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/geohash/examples/geohash_c.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs from GCC example' }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/geohash.xl')
    if ($cOutput -cne $xlOutput) { throw "C/XLang geohash output differs:`nC:`n$cOutput`nXL:`n$xlOutput" }
    Write-Host '[geohash] C/XLang parity: PASS'

    if (-not $SkipAot) {
        $core = Join-Path $work 'core.exe'
        $null = Run 'core AOT build' $xl @('build',
            'test/cases/extlib_geohash.xl', '-o', $core)
        $null = Run 'core AOT run' $core @()

        $boundaries = Join-Path $work 'boundaries.exe'
        $null = Run 'boundaries AOT build' $xl @('build',
            'release/extlibs/geohash/test/boundaries.xl', '-o', $boundaries)
        $null = Run 'boundaries AOT run' $boundaries @()
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
