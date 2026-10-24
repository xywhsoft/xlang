param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('jwt_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[jwt] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict native ABI' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/jwt/jwt_native.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT runtime compile' $gcc ($flags + @('-c',
        'release/extlibs/netrc/test/xrt_runtime.c', '-o', $runtime))
    $cTest = Join-Path $work 'jwt_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @(
        'release/extlibs/jwt/test/test_jwt.c', $runtime) + $links + @('-o', $cTest))
    $null = Run 'C test' $cTest @()
    $cExample = Join-Path $work 'jwt_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/jwt/examples/basic.c',
        'release/extlibs/jwt/jwt_native.c', $runtime) + $links + @('-o', $cExample))
    $cOutput = Run 'C example' $cExample @()
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/jwt.xl')
    if ($cOutput -cne $xlOutput) { throw 'C/XLang example output differs' }
    $null = Run 'XLang core' $xl @('run', 'test/cases/extlib_jwt.xl')
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/jwt/test/boundaries.xl')
    if (-not $SkipAot) {
        $aot = Join-Path $work 'jwt_example_aot.exe'
        $null = Run 'XLang AOT build' $xl @('build',
            'release/examples/extlibs/jwt.xl', '-o', $aot)
        $aotOutput = Run 'XLang AOT example' $aot @()
        if ($aotOutput -cne $cOutput) { throw 'AOT example output differs from C' }
        $aotBoundaries = Join-Path $work 'jwt_boundaries_aot.exe'
        $null = Run 'XLang AOT boundaries build' $xl @('build',
            'release/extlibs/jwt/test/boundaries.xl', '-o', $aotBoundaries)
        $boundaryOutput = Run 'XLang AOT boundaries' $aotBoundaries @()
        if ($boundaryOutput -cne 'jwt boundaries: PASS') {
            throw 'unexpected XLang AOT boundaries output'
        }
    }
} finally {
    Pop-Location
    $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/') + '\'
    $resolved = [IO.Path]::GetFullPath($work)
    if (-not $resolved.StartsWith($base, [StringComparison]::OrdinalIgnoreCase)) {
        throw "jwt work path escaped build/test: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
