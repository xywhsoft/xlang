$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/stb_image_resize2/stb_image_resize2_native.c',
        'release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c',
        'release/examples/extlibs/stb_image_resize2_c.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/stb_image_resize2/include $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_resize2 GCC check failed: $source" }
    }

    & $runner -Package stb_image_resize2 `
        -Source release/extlibs/stb_image_resize2/test/test_stb_image_resize2.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_resize2 C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_stb_image_resize2.xl',
        'release/extlibs/stb_image_resize2/test/boundaries.xl',
        'release/extlibs/stb_image_resize2/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_resize2 XLang test failed: $source" }
    }

    $direct = & $xl run release/examples/extlibs/stb_image_resize2_c.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_resize2 direct C example failed' }
    $runnerOutput = & $runner -Package 'stb_image_resize2,stb_image' `
        -Source release/examples/extlibs/stb_image_resize2_c.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_resize2 C package example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/stb_image_resize2.xl
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_resize2 XLang example failed' }
    $expected = @(
        'sRGB bytes:  64',
        'linear bytes:  36',
        'gray-alpha bytes:  2',
        'stb_image_resize2 C/XL example: PASS'
    ) -join "`n"
    if (($xlangOutput -join "`n") -cne $expected -or
        ($direct -join "`n") -cne ($xlangOutput -join "`n") -or
        ($runnerOutput -join "`n") -cne ($xlangOutput -join "`n")) {
        throw 'stb_image_resize2 C/XLang example output differs'
    }

    Write-Output 'stb_image_resize2 Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
