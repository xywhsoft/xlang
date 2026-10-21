$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/roaring/roaring_native.c',
        'release/extlibs/roaring/test/test_roaring.c',
        'release/extlibs/roaring/examples/roaring.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/roaring/include $source
        if ($LASTEXITCODE -ne 0) { throw "roaring GCC check failed: $source" }
    }

    & $runner -Package roaring -Source release/extlibs/roaring/test/test_roaring.c
    if ($LASTEXITCODE -ne 0) { throw 'roaring C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_roaring.xl',
        'test/cases/extlib_roaring_boundaries.xl',
        'release/extlibs/roaring/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "roaring XLang test failed: $source" }
    }

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $diagnostic = ((& $xl check release/extlibs/roaring/test/private_native_negative.xl 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $negativeExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($negativeExitCode -eq 0 -or -not $diagnostic.Contains('addNative') -or
        -not $diagnostic.Contains('access')) {
        throw "roaring native access check failed: $diagnostic"
    }

    $cOutput = & $runner -Package roaring -Source release/extlibs/roaring/examples/roaring.c
    if ($LASTEXITCODE -ne 0) { throw 'roaring C package example failed' }
    $direct = & $xl run release/extlibs/roaring/examples/roaring.c
    if ($LASTEXITCODE -ne 0) { throw 'roaring direct C example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/roaring.xl
    if ($LASTEXITCODE -ne 0) { throw 'roaring XLang example failed' }
    $expected = @(
        'count: 4',
        'contains 20: true',
        'contains 25: false',
        'min: 10',
        'max: 1000000',
        'after remove 20, count: 3',
        'roaring example: PASS'
    ) -join "`n"
    if (($cOutput -join "`n") -cne $expected -or
        ($direct -join "`n") -cne $expected -or
        ($xlangOutput -join "`n") -cne $expected) {
        throw 'roaring C/XLang example output differs'
    }

    Write-Output 'roaring Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
