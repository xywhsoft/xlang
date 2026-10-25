param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('skiplist_guard_' + [guid]::NewGuid().ToString('N'))
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'skiplist.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/skiplist/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed skiplist example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/skiplist/include @moduleFlags `
        release/extlibs/skiplist/skiplist_native.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/skiplist/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C ABI contract failed' }

    & $runner -Package skiplist -Source release/extlibs/skiplist/test/test_skiplist.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C model and boundaries failed' }
    & $runner -Package skiplist -Source release/extlibs/skiplist/test/entropy_failure.c
    if ($LASTEXITCODE -ne 0) { throw 'skiplist entropy failure cleanup failed' }
    Set-Location (Join-Path $root 'release')
    foreach ($source in @(
        '../test/cases/extlib_skiplist.xl',
        'extlibs/skiplist/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "skiplist XLang test failed: $source" }
    }

    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostics = @(& $xl check extlibs/skiplist/test/private_native.xl 2>&1 |
            ForEach-Object { $_.ToString() })
        $negativeCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    $diagnosticText = $diagnostics -join "`n"
    if ($negativeCode -eq 0 -or $diagnosticText -notmatch 'setNative' -or
        $diagnosticText -notmatch 'access') {
        throw "skiplist private native method was not rejected: $diagnosticText"
    }

    $xlLines = @(& $xl run examples/extlibs/skiplist.xl)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    Set-Location $root
    $cLines = @(& $runner -Package skiplist -Source release/examples/extlibs/skiplist_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist C package example failed' }
    Confirm-Output 'C package example' $cLines
    Set-Location (Join-Path $root 'release')
    $direct = @(& $xl run examples/extlibs/skiplist_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'skiplist direct C example failed' }
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        [void][IO.Directory]::CreateDirectory($work)
        $coreExe = Join-Path $work 'core.exe'
        & $xl build extlibs/skiplist/test/aot_core.xl -o $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'skiplist AOT core build failed' }
        & $coreExe
        if ($LASTEXITCODE -ne 0) { throw 'skiplist AOT core failed' }

        $exampleExe = Join-Path $work 'example.exe'
        & $xl build examples/extlibs/skiplist.xl -o $exampleExe
        if ($LASTEXITCODE -ne 0) { throw 'skiplist AOT example build failed' }
        $aotLines = @(& $exampleExe)
        if ($LASTEXITCODE -ne 0) { throw 'skiplist AOT example failed' }
        Confirm-Output 'AOT example' $aotLines
    }

    if ($SkipAot) {
        Write-Output 'skiplist Windows C/XLang fast gate: PASS (AOT skipped)'
    } else {
        Write-Output 'skiplist Windows C/XLang/AOT gate: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('skiplist_guard_')) {
        throw "skiplist test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
