param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$aot = Join-Path $root ("build/test/rss_core_$([guid]::NewGuid().ToString('N')).exe")
$aotExample = Join-Path $root ("build/test/rss_example_$([guid]::NewGuid().ToString('N')).exe")

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) {
        throw "${label}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[rss] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict C syntax' $gcc @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
        '-Ilib/xrt', '-Irelease/extlibs/rss/include',
        '-Irelease/extlibs/xml.dom', '-Irelease/extlibs/xml.sax',
        '-Irelease/extlibs/_expat/vendor/expat/lib',
        '-Irelease/extlibs/_expat/vendor/expat',
        'release/extlibs/rss/rss_parse.c', 'release/extlibs/rss/rss_build.c',
        'release/extlibs/rss/test/test_rss.c',
        'release/examples/extlibs/rss_c.c',
        'release/extlibs/rss/examples/atom_author_c.c')
    $null = Run 'C boundaries' $runner @('rss',
        'release/extlibs/rss/test/test_rss.c')
    $rssC = Run 'RSS C example' $runner @('rss',
        'release/examples/extlibs/rss_c.c')
    $atomC = Run 'Atom C example' $runner @('rss',
        'release/extlibs/rss/examples/atom_author_c.c')

    Push-Location $release
    try {
        $null = Run 'XLang boundaries' $xl @('run', '../test/cases/extlib_rss.xl')
        $null = Run 'XLang author inheritance' $xl @('run',
            'extlibs/rss/test/author_inheritance.xl')
        $rssXl = Run 'RSS XLang example' $xl @('run', 'examples/extlibs/rss_pair.xl')
        $atomXl = Run 'Atom XLang example' $xl @('run',
            'extlibs/rss/examples/atom_author.xl')
        $null = Run 'extended XLang example' $xl @('run', 'examples/extlibs/rss.xl')
        if (-not $SkipAot) {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $aot)) | Out-Null
            $null = Run 'core AOT build' $xl @('build',
                'extlibs/rss/test/aot_core.xl', '-o', $aot)
            $null = Run 'core AOT' $aot @()
            $null = Run 'Atom example AOT build' $xl @('build',
                'extlibs/rss/examples/atom_author.xl', '-o', $aotExample)
            $atomAot = Run 'Atom example AOT' $aotExample @()
            if ($atomAot -cne $atomC) {
                throw '[rss] Atom AOT/C example output differs'
            }
        }
    } finally {
        Pop-Location
    }
    if ($rssC -cne $rssXl -or $atomC -cne $atomXl) {
        throw '[rss] C/XLang example output differs'
    }
    if ($rssC -cne "type: rss`nitems: 1") {
        throw '[rss] RSS example differs from expected feed facts'
    }
    if ($atomC -cne "Editor`nSource") {
        throw '[rss] Atom author example differs from expected inheritance'
    }
    Write-Host '[rss] C/XLang example parity: PASS'
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $aot -PathType Leaf) {
        Remove-Item -LiteralPath $aot -Force
    }
    if (Test-Path -LiteralPath $aotExample -PathType Leaf) {
        Remove-Item -LiteralPath $aotExample -Force
    }
}
