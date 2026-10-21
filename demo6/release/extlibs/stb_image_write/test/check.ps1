$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'

Push-Location $root
try {
    foreach ($source in @(
        'release/extlibs/stb_image_write/stb_image_write_native.c',
        'release/extlibs/stb_image_write/test/test_stb_image_write.c',
        'release/examples/extlibs/stb_image_write_c.c'
    )) {
        & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
            -Ilib/xrt -Irelease/extlibs/stb_image_write/include `
            -Irelease/extlibs/stb_image/include $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_write GCC check failed: $source" }
    }

    & $runner -Package 'stb_image_write,stb_image' `
        -Source release/extlibs/stb_image_write/test/test_stb_image_write.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write C boundaries failed' }

    foreach ($source in @(
        'test/cases/extlib_stb_image_write.xl',
        'release/extlibs/stb_image_write/test/boundaries.xl',
        'release/extlibs/stb_image_write/test/output_limit.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "stb_image_write XLang test failed: $source" }
    }

    $direct = & $xl run release/examples/extlibs/stb_image_write_c.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write direct C example failed' }
    $runnerOutput = & $runner -Package 'stb_image_write,stb_image' `
        -Source release/examples/extlibs/stb_image_write_c.c
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write C package example failed' }
    $xlangOutput = & $xl run release/examples/extlibs/stb_image_write.xl
    if ($LASTEXITCODE -ne 0) { throw 'stb_image_write XLang example failed' }
    $expected = 'stb_image_write C/XL example: PASS'
    if (($xlangOutput -join "`n") -cne $expected -or
        ($direct -join "`n") -cne $expected -or
        ($runnerOutput -join "`n") -cne $expected) {
        throw 'stb_image_write C/XLang example output differs'
    }

    Write-Output 'stb_image_write Windows C/XLang gate: PASS'
} finally {
    Pop-Location
}
