$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$aotDirectory = Join-Path $root 'build/test'
$aot = Join-Path $aotDirectory ("cuckoo_aot_core_$([guid]::NewGuid().ToString('N')).exe")

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/cuckoo/cuckoo_native.c',
        'release/extlibs/cuckoo/test/test_cuckoo.c',
        'release/extlibs/cuckoo/examples/cuckoo.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/cuckoo/include $source
        if ($LASTEXITCODE -ne 0) { throw "cuckoo strict GCC check failed: $source" }
    }

    & $runner -Package cuckoo -Source release/extlibs/cuckoo/test/test_cuckoo.c
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_cuckoo.xl',
        'test/cases/extlib_cuckoo_boundaries.xl',
        'release/extlibs/cuckoo/test/high_load.xl',
        'release/extlibs/cuckoo/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "cuckoo XLang test failed: $source" }
    }

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $diagnostic = ((& $xl check release/extlibs/cuckoo/test/private_native_negative.xl 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $negativeExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($negativeExitCode -eq 0 -or -not $diagnostic.Contains('addNative') -or
        -not $diagnostic.Contains('access')) {
        throw "cuckoo native access check failed: $diagnostic"
    }

    $cOutput = & $runner -Package cuckoo -Source release/extlibs/cuckoo/examples/cuckoo.c
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo C example failed' }
    $directOutput = & $xl run release/extlibs/cuckoo/examples/cuckoo.c
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo direct C example failed' }
    $xlOutput = & $xl run release/examples/extlibs/cuckoo.xl
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo XLang example failed' }
    if (($cOutput -join "`n") -cne ($xlOutput -join "`n")) {
        throw 'cuckoo C/XLang example output differs'
    }
    if (($directOutput -join "`n") -cne ($xlOutput -join "`n")) {
        throw 'cuckoo direct C/XLang example output differs'
    }

    New-Item -ItemType Directory -Force -Path $aotDirectory | Out-Null
    & $xl build release/extlibs/cuckoo/test/aot_core.xl -o $aot
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo core AOT build failed' }
    & $aot
    if ($LASTEXITCODE -ne 0) { throw 'cuckoo core AOT run failed' }

    Write-Output 'cuckoo Windows C/XLang gate: PASS'
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $aot -PathType Leaf) {
        Remove-Item -LiteralPath $aot -Force
    }
}
