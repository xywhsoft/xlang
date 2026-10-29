param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/cString_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $ErrorActionPreference = 'Continue'
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[cString] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $test = Join-Path $work 'cString_test.exe'
    $null = Run 'C test compile' $gcc @('-std=c11', '-O2', '-Wall',
        '-Wextra', '-Werror', 'release/extlibs/cString/test/test_cstring.c',
        '-o', $test)
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'cString_example.exe'
    $null = Run 'C example compile' $gcc @('-std=c11', '-O2', '-Wall',
        '-Wextra', '-Werror', 'release/extlibs/cString/examples/cString_c.c',
        '-o', $example)
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/cString/examples/cString_c.c')

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/cString/test/boundaries.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/cString/examples/cString.xl')
        if ($cOutput -cne $directOutput -or $cOutput -cne $xlOutput) {
            throw "C/XLang cString example outputs differ:`nC:`n$cOutput`nTCC:`n$directOutput`nXL:`n$xlOutput"
        }
        Write-Host '[cString] C/XLang parity: PASS'

        if (-not $SkipAot) {
            $target = Join-Path $work 'cString_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/cString/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'cString_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/cString/examples/cString.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT cString example differs from C'
            }
            $cExampleAot = Join-Path $work 'cString_c_example_aot.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/cString/examples/cString_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT cString example differs from GCC C'
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "cString temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
