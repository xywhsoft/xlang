param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$python = (Get-Command python -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/epub/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-epub-' + [guid]::NewGuid().ToString('N'))
$cBook = Join-Path $work 'c.epub'
$xlBook = Join-Path $work 'xlang.epub'
$expected = (Get-Content -LiteralPath (Join-Path $root 'release/extlibs/epub/examples/basic.expected.txt') `
    -Encoding UTF8 -Raw).Replace("`r`n", "`n").TrimEnd("`n")

function Confirm-Output([string] $name, [string[]] $lines) {
    if ([string]::Join("`n", $lines) -cne $expected) {
        throw "$name differs from fixed EPUB example"
    }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/epub/include' @moduleFlags `
        release/extlibs/epub/epub_native.c release/extlibs/epub/epub_zip.c
    if ($LASTEXITCODE -ne 0) { throw 'epub manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/epub/include' release/extlibs/epub/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'epub C ABI contract failed' }

    & $runner -Package epub -Source release/extlibs/epub/test/test_epub.c
    if ($LASTEXITCODE -ne 0) { throw 'epub C boundaries failed' }
    foreach ($source in @(
        'test/cases/extlib_epub.xl',
        'test/cases/extlib_epub_boundaries.xl',
        'release/extlibs/epub/test/ffi_boundaries.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "epub XLang test failed: $source" }
    }

    [void][IO.Directory]::CreateDirectory($work)
    $cLines = @(& $xl run release/extlibs/epub/examples/basic.c -- $cBook)
    if ($LASTEXITCODE -ne 0) { throw 'epub direct C example failed' }
    Confirm-Output 'C example' $cLines
    $xlLines = @(& $xl run release/examples/extlibs/epub.xl -- $xlBook)
    if ($LASTEXITCODE -ne 0) { throw 'epub XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    & $python -I release/extlibs/epub/test/check_external.py $cBook $xlBook
    if ($LASTEXITCODE -ne 0) { throw 'epub independent ZIP/XML check failed' }

    if (-not $SkipAot) {
        $core = Join-Path $work 'aot_core.exe'
        & $xl build release/extlibs/epub/test/aot_core.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'epub core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'epub core AOT run failed' }
        Write-Output 'epub core AOT: PASS'

        $example = Join-Path $work 'aot_example.exe'
        $aotBook = Join-Path $work 'aot.epub'
        & $xl build release/examples/extlibs/epub.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'epub example AOT build failed' }
        $aotLines = @(& $example $aotBook)
        if ($LASTEXITCODE -ne 0) { throw 'epub example AOT run failed' }
        Confirm-Output 'AOT example' $aotLines
        & $python -I release/extlibs/epub/test/check_external.py $cBook $aotBook
        if ($LASTEXITCODE -ne 0) { throw 'epub AOT EPUB differs from C example' }
        Write-Output 'epub AOT/C example parity: PASS'
    }
    Write-Output 'epub Windows C/XLang/ZIP/XML gate: PASS'
} finally {
    Pop-Location
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('xlang-epub-')) {
        throw "EPUB test directory escapes system temp: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
