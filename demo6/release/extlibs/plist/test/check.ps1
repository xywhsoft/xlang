$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/plist/test/test_plist.c',
        'release/extlibs/plist/test/test_native.c'
    )) {
        & $runner -Package plist -Source $source
        if ($LASTEXITCODE -ne 0) { throw "plist C test failed: $source" }
    }

    & $xl run release/extlibs/plist/test/boundaries.xl
    if ($LASTEXITCODE -ne 0) { throw 'plist XLang boundary test failed' }

    & $xl run release/extlibs/plist/test/allocations.xl
    if ($LASTEXITCODE -ne 0) { throw 'plist XLang allocation test failed' }

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $privateDiagnostic = ((& $xl check release/extlibs/plist/test/private_native_document_negative.xl 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $privateExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($privateExitCode -eq 0 -or -not $privateDiagnostic.Contains('no such exported type')) {
        throw "plist native document access check failed: $privateDiagnostic"
    }

    $xlangOutput = & $xl run release/examples/extlibs/plist.xl
    if ($LASTEXITCODE -ne 0) { throw 'plist XLang example failed' }
    $cOutput = & $xl run release/examples/extlibs/plist_c.c
    if ($LASTEXITCODE -ne 0) { throw 'plist C example failed' }
    if (($xlangOutput -join "`n") -cne ($cOutput -join "`n")) {
        throw 'plist C/XLang example output differs'
    }
    Write-Output 'plist Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
