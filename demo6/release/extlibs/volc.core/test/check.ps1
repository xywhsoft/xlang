$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$runC = Join-Path $root 'tools/run_extlib_c.ps1'

if (-not (Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw 'gcc is required for the volc.core C gate'
}

Push-Location $root
try {
    & gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only `
        -DXRT_MODULE_ALL -DXRUNTIME_MODULE_ALL `
        -Ilib/xrt -Ilib/xruntime -Irelease/include -Irelease/extlibs/xhttp/include `
        release/extlibs/volc.core/x_volc_core_native.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.core strict GCC check failed' }

    & gcc -std=c11 -DNDEBUG -Wall -Wextra -Werror -fsyntax-only `
        -Ilib/xrt -Ilib/xruntime -Irelease/include -Irelease/extlibs/xhttp/include `
        release/extlibs/volc.core/test/test_volc_core.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.core release-config C test compile failed' }

    & $runC -Package volc.core -Source release/extlibs/volc.core/test/test_volc_core.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.core C boundary test failed' }

    & $runC -Package volc.core -Source release/extlibs/volc.core/examples/signing.c
    if ($LASTEXITCODE -ne 0) { throw 'volc.core C example failed' }
}
finally {
    Pop-Location
}
