$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
Push-Location $root
try {
    $result = & powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_extlib_c.ps1 -Package volc.core -Source release/extlibs/volc.phone/examples/preview.c
    if ($LASTEXITCODE -ne 0 -or (($result -join "`n").Trim() -ne 'volc.phone offline example: ok')) {
        throw 'volc.phone C preview failed'
    }
    & python release/extlibs/volc.phone/test/check_response.py
    if ($LASTEXITCODE -ne 0) { throw 'volc.phone response JIT/AOT failed' }
    Write-Output 'volc.phone offline gate: PASS (full package import not tested)'
}
finally {
    Pop-Location
}
