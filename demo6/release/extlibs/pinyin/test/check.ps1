param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'pinyin.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/pinyin/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('pinyin_guard_' + [guid]::NewGuid().ToString('N'))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed pinyin example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/pinyin/include @moduleFlags release/extlibs/pinyin/pinyin_native.c
    if ($LASTEXITCODE -ne 0) { throw 'pinyin manifest macro compile failed' }

    & $runner -Package pinyin -Source release/extlibs/pinyin/test/test_pinyin.c
    if ($LASTEXITCODE -ne 0) { throw 'pinyin C boundaries failed' }

    foreach ($source in @('test/cases/extlib_pinyin.xl', 'release/extlibs/pinyin/test/boundaries.xl',
        'release/extlibs/pinyin/test/allocations.xl')) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "pinyin XLang test failed: $source" }
    }

    $xlLines = @(& $xl run release/examples/extlibs/pinyin.xl)
    if ($LASTEXITCODE -ne 0) { throw 'pinyin XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    $cLines = @(& $runner -Package pinyin -Source release/examples/extlibs/pinyin_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'pinyin C package example failed' }
    Confirm-Output 'C package example' $cLines

    $direct = @(& $xl run release/examples/extlibs/pinyin_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'pinyin direct C example failed' }
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'pinyin_core.exe'
        & $xl build test/cases/extlib_pinyin.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'pinyin core AOT build failed' }
        $coreLines = @(& $core)
        if ($LASTEXITCODE -ne 0 -or $coreLines.Count -ne 0) {
            throw 'pinyin core AOT result differs'
        }
        Write-Output 'pinyin core AOT: PASS'
        $example = Join-Path $work 'pinyin_example.exe'
        & $xl build release/examples/extlibs/pinyin.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'pinyin example AOT build failed' }
        $aotLines = @(& $example)
        if ($LASTEXITCODE -ne 0) { throw 'pinyin example AOT failed' }
        Confirm-Output 'XLang example AOT' $aotLines
        Write-Output 'pinyin example AOT: PASS'
    }

    Write-Output 'pinyin Windows C/XLang gate: PASS'
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe pinyin work directory: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
