param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-vcard-' + [guid]::NewGuid().ToString('N'))
$cFile = Join-Path $work 'c.vcf'
$xlangFile = Join-Path $work 'xlang.vcf'
$aotFile = Join-Path $work 'aot.vcf'
$coreExe = Join-Path $work 'vcard_core.exe'
$aotExe = Join-Path $work 'vcard_example.exe'
New-Item -ItemType Directory -Path $work | Out-Null

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/vcard/vcard_native.c',
        'release/extlibs/vcard/vcard_parse.c',
        'release/extlibs/vcard/vcard_build.c',
        'release/extlibs/vcard/test/test_vcard.c',
        'release/extlibs/vcard/examples/vcard.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/vcard/include $source
        if ($LASTEXITCODE -ne 0) { throw "vcard GCC check failed: $source" }
    }

    & $runner -Package vcard -Source release/extlibs/vcard/test/test_vcard.c
    if ($LASTEXITCODE -ne 0) { throw 'vcard C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_vcard.xl',
        'test/cases/extlib_vcard_boundaries.xl',
        'release/extlibs/vcard/test/control_characters.xl',
        'release/extlibs/vcard/test/allocations.xl',
        'release/extlibs/vcard/test/uri_values.xl',
        'release/extlibs/vcard/test/structure.xl'
        'release/extlibs/vcard/test/limits.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "vcard XLang test failed: $source" }
    }

    $runnerOutput = & $runner -Package vcard -Source release/extlibs/vcard/examples/vcard.c
    if ($LASTEXITCODE -ne 0) { throw 'vcard C package example failed' }
    $direct = & $xl run release/extlibs/vcard/examples/vcard.c -- $cFile
    if ($LASTEXITCODE -ne 0) { throw 'vcard direct C example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/vcard.xl -- $xlangFile
    if ($LASTEXITCODE -ne 0) { throw 'vcard XLang example failed' }
    if (($runnerOutput -join "`n") -cne ($direct -join "`n") -or
        ($direct -join "`n") -cne ($xlangOutput -join "`n") -or
        ($direct -join "`n") -notmatch 'vcard example: PASS') {
        throw 'vcard C/XLang example output differs'
    }
    if (-not (Test-Path -LiteralPath $cFile -PathType Leaf) -or
        -not (Test-Path -LiteralPath $xlangFile -PathType Leaf)) {
        throw 'vcard example output file missing'
    }
    $cBytes = [IO.File]::ReadAllBytes($cFile)
    $xlangBytes = [IO.File]::ReadAllBytes($xlangFile)
    if ($cBytes.Length -eq 0 -or $cBytes.Length -ne $xlangBytes.Length -or
        -not [Linq.Enumerable]::SequenceEqual([byte[]]$cBytes, [byte[]]$xlangBytes)) {
        throw 'vcard C/XLang file bytes differ'
    }

    $oldErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $failureOutput = ((& $xl run release/extlibs/vcard/examples/vcard.c -- $work 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n")
        $failureCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    if ($failureCode -ne 1 -or $failureOutput.Length -ne 0) {
        throw "vcard failed-write example boundary failed: $failureOutput"
    }

    if (-not $SkipAot) {
        & $xl build release/extlibs/vcard/test/aot_core.xl -o $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'vcard core AOT build failed' }
        & $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'vcard core AOT failed' }

        & $xl build release/examples/extlibs/vcard.xl -o $aotExe
        if ($LASTEXITCODE -ne 0) { throw 'vcard example AOT build failed' }
        $aotOutput = & $aotExe $aotFile
        if ($LASTEXITCODE -ne 0 -or
            ($aotOutput -join "`n") -cne ($runnerOutput -join "`n")) {
            throw 'vcard example AOT output differs'
        }
        if (-not (Test-Path -LiteralPath $aotFile -PathType Leaf) -or
            -not [Linq.Enumerable]::SequenceEqual([byte[]]$cBytes,
                [byte[]][IO.File]::ReadAllBytes($aotFile))) {
            throw 'vcard example AOT file bytes differ'
        }
    }

    if ($SkipAot) {
        Write-Output 'vcard Windows C/XLang focused gate: PASS'
    } else {
        Write-Output 'vcard Windows C/XLang and AOT release gate: PASS'
    }
} finally {
    Pop-Location
    foreach ($file in @($cFile, $xlangFile, $aotFile, $coreExe, $aotExe)) {
        if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
    }
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Force }
}
