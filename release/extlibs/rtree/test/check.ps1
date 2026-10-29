param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('rtree_guard_' + [guid]::NewGuid().ToString('N'))
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'rtree.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/rtree/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed R-tree example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/rtree/include @moduleFlags release/extlibs/rtree/rtree_native.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/rtree/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree C ABI contract failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt @moduleFlags `
        release/extlibs/rtree/test/split_seeds.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree split seed syntax failed' }

    & $runner -Package rtree -Source release/extlibs/rtree/test/test_rtree.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree C oracle and boundaries failed' }
    & $runner -Package rtree -Source release/extlibs/rtree/test/split_seeds.c
    if ($LASTEXITCODE -ne 0) { throw 'rtree split seed regression failed' }
    foreach ($source in @(
        'test/cases/extlib_rtree.xl',
        'release/extlibs/rtree/test/boundaries.xl',
        'release/extlibs/rtree/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "rtree XLang test failed: $source" }
    }

    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostics = @(& $xl check release/extlibs/rtree/test/private_native_negative.xl 2>&1 |
            ForEach-Object { $_.ToString() })
        $negativeCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    $diagnosticText = $diagnostics -join "`n"
    if ($negativeCode -eq 0 -or $diagnosticText -notmatch 'searchNative' -or
        $diagnosticText -notmatch 'access') {
        throw "rtree private native method was not rejected: $diagnosticText"
    }

    $xlLines = @(& $xl run release/examples/extlibs/rtree.xl)
    if ($LASTEXITCODE -ne 0) { throw 'rtree XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package rtree -Source release/examples/extlibs/rtree_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'rtree C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/examples/extlibs/rtree_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'rtree direct C example failed' }
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        [void][IO.Directory]::CreateDirectory($work)
        $coreExe = Join-Path $work 'core.exe'
        & $xl build test/cases/extlib_rtree.xl -o $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'rtree AOT core build failed' }
        & $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'rtree AOT core failed' }

        $exampleExe = Join-Path $work 'example.exe'
        & $xl build release/examples/extlibs/rtree.xl -o $exampleExe
        if ($LASTEXITCODE -ne 0) { throw 'rtree AOT example build failed' }
        $aotLines = @(& $exampleExe)
        if ($LASTEXITCODE -ne 0) { throw 'rtree AOT example failed' }
        Confirm-Output 'AOT example' $aotLines

        foreach ($name in @('boundaries', 'allocations')) {
            $source = "release/extlibs/rtree/test/$name.xl"
            $output = Join-Path $work "$name.exe"
            & $xl build $source -o $output
            if ($LASTEXITCODE -ne 0) { throw "rtree $name AOT build failed" }
            & $output
            if ($LASTEXITCODE -ne 0) { throw "rtree $name AOT failed" }
        }

        $cExampleExe = Join-Path $work 'c_example.exe'
        & $xl build release/examples/extlibs/rtree_c.c -o $cExampleExe
        if ($LASTEXITCODE -ne 0) { throw 'rtree C example AOT build failed' }
        $cAotLines = @(& $cExampleExe)
        if ($LASTEXITCODE -ne 0) { throw 'rtree C example AOT failed' }
        Confirm-Output 'C example AOT' $cAotLines
    }

    if ($SkipAot) {
        Write-Output 'rtree Windows C/XLang fast gate: PASS (AOT skipped)'
    } else {
        Write-Output 'rtree Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('rtree_guard_')) {
        throw "R-tree test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
