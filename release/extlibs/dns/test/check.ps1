param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test/dns_guard'
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
    Write-Host "[dns] ${label}: PASS"
    return (($lines -join "`n").Replace("`r", '').TrimEnd("`n"))
}

Push-Location $root
try {
    $expected = [IO.File]::ReadAllText(
        (Join-Path $root 'release/extlibs/dns/examples/basic.expected.txt')).
        Replace("`r", '').TrimEnd("`n")
    $null = Run 'C boundaries' 'powershell.exe' @('-NoProfile',
        '-ExecutionPolicy', 'Bypass', '-File', $runner, '-Package', 'dns',
        '-Source', 'release/extlibs/dns/test/test_dns.c')
    $cOutput = Run 'C example' 'powershell.exe' @('-NoProfile',
        '-ExecutionPolicy', 'Bypass', '-File', $runner, '-Package', 'dns',
        '-Source', 'release/extlibs/dns/examples/basic.c')
    $null = Run 'XLang central suite' $xl @('run', 'test/cases/extlib_dns.xl')
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/dns/test/boundaries.xl')
    $null = Run 'owned text lifetime' $xl @('run', 'release/extlibs/dns/test/owned_text.xl')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/dns.xl')
    if ($cOutput -cne $expected -or $xlOutput -cne $expected) {
        throw 'C/XLang DNS example output differs from the fixed result'
    }

    $generated = Join-Path $work 'dns.c'
    $null = Run 'owned text cgen' $xl @('cgen',
        'release/extlibs/dns/test/owned_text.xl', '--emit', $generated)
    $cSource = [IO.File]::ReadAllText($generated)
    if ($cSource -cnotmatch 'extern char\* xdns_encode_name\(' -or
        $cSource -cnotmatch '\bv\d+\.Data\s*=\s*xdns_encode_name\s*\(' -or
        $cSource.Contains('xdns_encode_name_xl')) {
        throw 'DNS encoded text did not use the direct owned C return'
    }

    if (-not $SkipAot) {
        foreach ($item in @(
            @('boundaries', 'release/extlibs/dns/test/boundaries.xl'),
            @('owned_text', 'release/extlibs/dns/test/owned_text.xl'),
            @('example', 'release/examples/extlibs/dns.xl')
        )) {
            $target = Join-Path $work ($item[0] + '.exe')
            $null = Run "$($item[0]) AOT build" $xl @('build', $item[1], '-o', $target)
            $output = Run "$($item[0]) AOT" $target @()
            if ($item[0] -eq 'example' -and $output -cne $expected) {
                throw 'DNS XLang AOT example differs from the fixed result'
            }
        }
        $cTarget = Join-Path $work 'c_example.exe'
        $null = Run 'C AOT build' $xl @('build',
            'release/extlibs/dns/examples/basic.c',
            'release/extlibs/dns/dns_native.c', '-o', $cTarget)
        $cAotOutput = Run 'C AOT example' $cTarget @()
        if ($cAotOutput -cne $expected) {
            throw 'DNS C AOT example differs from the fixed result'
        }
    }
    Write-Host '[dns] Windows gate: PASS'
} finally {
    Pop-Location
    $parent = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/') + '\'
    $target = [IO.Path]::GetFullPath($work)
    if (-not $target.StartsWith($parent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "unsafe DNS test work path: $target"
    }
    Remove-Item -LiteralPath $target -Recurse -Force
}
