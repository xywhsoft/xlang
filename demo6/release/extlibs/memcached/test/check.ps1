param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test/memcached_guard'
$work = Join-Path $workRoot ([guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($lines -join "`n")" }
    Write-Host "[memcached] ${label}: PASS"
    return (($lines -join "`n").Replace("`r", '').TrimEnd("`n"))
}

Push-Location $root
try {
    $expected = [IO.File]::ReadAllText(
        (Join-Path $root 'release/extlibs/memcached/examples/basic.expected.txt')).
        Replace("`r", '').TrimEnd("`n")
    $null = Run 'C protocol' 'powershell.exe' @('-NoProfile', '-ExecutionPolicy',
        'Bypass', '-File', $runner, '-Package', 'memcached',
        '-Source', 'release/extlibs/memcached/test/test_memcached.c')
    $cOutput = Run 'C example' 'powershell.exe' @('-NoProfile', '-ExecutionPolicy',
        'Bypass', '-File', $runner, '-Package', 'memcached',
        '-Source', 'release/extlibs/memcached/examples/basic_c.c')
    $null = Run 'XLang protocol' $xl @('run', 'release/extlibs/memcached/test/boundaries.xl')
    $null = Run 'XLang loopback' $xl @('run', 'release/extlibs/memcached/test/loopback.xl')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/extlibs/memcached/examples/basic.xl')
    if ($cOutput -cne $expected -or $xlOutput -cne $expected) {
        throw 'C/XLang example output differs from the fixed result'
    }
    if (-not $SkipAot) {
        foreach ($item in @(
            @('protocol', 'release/extlibs/memcached/test/boundaries.xl'),
            @('loopback', 'release/extlibs/memcached/test/loopback.xl'),
            @('example', 'release/extlibs/memcached/examples/basic.xl')
        )) {
            $target = Join-Path $work ($item[0] + '.exe')
            $null = Run "$($item[0]) AOT build" $xl @('build', $item[1], '-o', $target)
            $output = Run "$($item[0]) AOT" $target @()
            if ($item[0] -eq 'example' -and $output -cne $expected) {
                throw 'XLang AOT example differs from the fixed result'
            }
        }
        $cTarget = Join-Path $work 'c_example.exe'
        $null = Run 'C AOT build' $xl @('build',
            'release/extlibs/memcached/examples/basic_c.c',
            'release/extlibs/memcached/memcached_request.c',
            'release/extlibs/memcached/memcached_response.c',
            '--inc-path', 'lib/xrt', '-o', $cTarget)
        if ((Run 'C AOT example' $cTarget @()) -cne $expected) {
            throw 'C AOT example differs from the fixed result'
        }
    }
    Write-Host '[memcached] Windows gate: PASS'
} finally {
    Pop-Location
    $parent = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "unsafe memcached test work path: $target"
    }
    Remove-Item -LiteralPath $target -Recurse -Force
}
