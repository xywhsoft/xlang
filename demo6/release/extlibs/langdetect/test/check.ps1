param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('langdetect_guard_' + [guid]::NewGuid().ToString('N'))

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/langdetect/langdetect_native.c',
        'release/extlibs/langdetect/test/test_langdetect.c',
        'release/extlibs/langdetect/test/abi_contract.c',
        'release/extlibs/langdetect/examples/langdetect.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/langdetect/include $source
        if ($LASTEXITCODE -ne 0) { throw "langdetect GCC check failed: $source" }
    }

    & $runner -Package langdetect -Source release/extlibs/langdetect/test/test_langdetect.c
    if ($LASTEXITCODE -ne 0) { throw 'langdetect C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_langdetect.xl',
        'test/cases/extlib_langdetect_boundaries.xl',
        'release/extlibs/langdetect/test/analysis_boundaries.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "langdetect XLang test failed: $source" }
    }

    $cOutput = & $runner -Package langdetect `
        -Source release/extlibs/langdetect/examples/langdetect.c
    if ($LASTEXITCODE -ne 0) { throw 'langdetect C package example failed' }
    $direct = & $xl run release/extlibs/langdetect/examples/langdetect.c
    if ($LASTEXITCODE -ne 0) { throw 'langdetect direct C example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/langdetect.xl
    if ($LASTEXITCODE -ne 0) { throw 'langdetect XLang example failed' }
    if (($cOutput -join "`n") -cne ($direct -join "`n") -or
        ($direct -join "`n") -cne ($xlangOutput -join "`n") -or
        ($direct -join "`n") -notmatch 'langdetect example: PASS') {
        throw 'langdetect C/XLang example output differs'
    }

    if (-not $SkipAot) {
        [void][IO.Directory]::CreateDirectory($work)
        $coreExe = Join-Path $work 'core.exe'
        & $xl build release/extlibs/langdetect/test/aot_core.xl -o $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'langdetect AOT core build failed' }
        & $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'langdetect AOT core failed' }

        $exampleExe = Join-Path $work 'example.exe'
        & $xl build release/examples/extlibs/langdetect.xl -o $exampleExe
        if ($LASTEXITCODE -ne 0) { throw 'langdetect AOT example build failed' }
        $aotOutput = & $exampleExe
        if ($LASTEXITCODE -ne 0) { throw 'langdetect AOT example failed' }
        if (($aotOutput -join "`n") -cne ($cOutput -join "`n")) {
            throw 'langdetect AOT/C example output differs'
        }
    }

    if ($SkipAot) {
        Write-Output 'langdetect Windows C/XLang fast gate: PASS (AOT skipped)'
    } else {
        Write-Output 'langdetect Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('langdetect_guard_')) {
        throw "langdetect test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
