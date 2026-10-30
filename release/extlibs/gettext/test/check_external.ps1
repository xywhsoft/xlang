param([string]$Python = 'python')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$work = Join-Path $root 'build/test/gettext_external_guard'
[IO.Directory]::CreateDirectory($work) | Out-Null
$mo = Join-Path $work 'external.mo'
$exe = Join-Path $work 'external_c.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'

Push-Location $root
try {
    & $Python 'release/extlibs/gettext/test/check_external.py' $mo
    if ($LASTEXITCODE -ne 0) { throw 'External MO generation/reference check failed' }

    & $xl run 'release/extlibs/gettext/test/external_mo.xl' -- $mo
    if ($LASTEXITCODE -ne 0) { throw 'XLang external MO check failed' }

    & $gcc -std=c11 -O2 -Wall -Wextra -Werror `
        -Ilib/xrt -Irelease/include -Irelease/extlibs/gettext `
        release/extlibs/gettext/test/xrt_runtime.c `
        release/extlibs/gettext/gettext_native.c `
        release/extlibs/gettext/gettext_plural.c `
        release/extlibs/gettext/test/external_mo_c.c `
        -lWs2_32 -lAdvapi32 -lIphlpapi -lm -o $exe
    if ($LASTEXITCODE -ne 0) { throw 'C external MO check build failed' }

    & $exe $mo
    if ($LASTEXITCODE -ne 0) { throw 'C external MO check failed' }
} finally {
    Pop-Location
}
