param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('plist_guard_' + [guid]::NewGuid().ToString('N'))

Push-Location $root
try {
    [void][IO.Directory]::CreateDirectory($work)
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

    $cXml = Join-Path $work 'c.xml'
    $cBinary = Join-Path $work 'c.bplist'
    $xlXml = Join-Path $work 'xlang.xml'
    $xlBinary = Join-Path $work 'xlang.bplist'
    $xlangOutput = & $xl run release/examples/extlibs/plist.xl -- $xlXml $xlBinary
    if ($LASTEXITCODE -ne 0) { throw 'plist XLang example failed' }
    $cOutput = & $xl run release/examples/extlibs/plist_c.c -- $cXml $cBinary
    if ($LASTEXITCODE -ne 0) { throw 'plist C example failed' }
    if (($xlangOutput -join "`n") -cne ($cOutput -join "`n")) {
        throw 'plist C/XLang example output differs'
    }
    & python -I release/extlibs/plist/test/check_external.py $cXml $xlXml $cBinary $xlBinary
    if ($LASTEXITCODE -ne 0) { throw 'plist C/XLang plistlib interop failed' }
    if (-not $SkipAot) {
        $core = Join-Path $work 'plist_core.exe'
        & $xl build release/extlibs/plist/test/smoke.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'plist import AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'plist import AOT run failed' }

        $example = Join-Path $work 'plist_example.exe'
        $aotXml = Join-Path $work 'aot.xml'
        $aotBinary = Join-Path $work 'aot.bplist'
        & $xl build release/examples/extlibs/plist.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'plist example AOT build failed' }
        $aotOutput = & $example $aotXml $aotBinary
        if ($LASTEXITCODE -ne 0 -or ($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'plist example AOT output differs from C/XLang'
        }
        & python -I release/extlibs/plist/test/check_external.py $cXml $aotXml $cBinary $aotBinary
        if ($LASTEXITCODE -ne 0) { throw 'plist AOT plistlib interop failed' }
    }
    if ($SkipAot) {
        Write-Output 'plist Windows C/XLang (AOT skipped) gate: PASS'
    } else {
        Write-Output 'plist Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work) {
        $checked = [IO.Path]::GetFullPath($work)
        if (-not $checked.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) { throw 'unsafe plist test cleanup path' }
        Remove-Item -LiteralPath $checked -Recurse -Force
    }
}
