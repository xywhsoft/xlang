$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$runC = Join-Path $root 'tools/run_extlib_c.ps1'
$coreInclude = Join-Path $root 'release/extlibs/volc.core/include'

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw 'gcc is required for the volc.sts C gate'
}

Push-Location $root
try {
    & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        "-I$(Join-Path $root 'lib/xrt')" "-I$coreInclude" `
        release/extlibs/volc.sts/test/test_sts.c `
        release/extlibs/volc.sts/examples/assume_role.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.sts strict GCC check failed' }

    & $runC -Package volc.sts -Source release/extlibs/volc.sts/test/test_sts.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.sts C boundary test failed' }

    & $runC -Package volc.sts -Source release/extlibs/volc.sts/examples/assume_role.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.sts C example failed' }

    & python release/extlibs/volc.sts/test/check_response.py
    if ($LASTEXITCODE -ne 0) { throw 'volc.sts response JIT/AOT failed' }
}
finally {
    Pop-Location
}
