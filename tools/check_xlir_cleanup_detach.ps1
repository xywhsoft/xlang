param([string]$Root = '')
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $source = 'test/cases/xlir_cleanup_detach_reentry.xl'
    $prefix = 'build/test/xlir_cleanup_detach_reentry'
    & release/xl.exe ir $source --lowered --format json --emit "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup reentry lowering failed' }
    $ir = [IO.File]::ReadAllText("$prefix.json") | ConvertFrom-Json
    if (@($ir.instructions | Where-Object { $_.kindName -eq 'release' }).Count -lt 2) {
        throw 'Cleanup reentry fixture lost its explicit RELEASE instructions'
    }
    foreach ($suffix in @('', '.repeat')) {
        & release/xl.exe cgen $source --backend xlir --emit "$prefix$suffix.c"
        if ($LASTEXITCODE -ne 0) { throw 'Cleanup reentry Cgen failed' }
    }
    $c = [IO.File]::ReadAllText("$prefix.c")
    if ($c -cne [IO.File]::ReadAllText("$prefix.repeat.c")) {
        throw 'Cleanup reentry C is not deterministic'
    }
    $detach = [regex]::Match($c,
        'xvalue\* xlir_released;.*?\{ &\(xlir_released\), &\((?<source>s[0-9]+)\), sizeof\(xlir_released\) \},.*?\{ &\((?<cleared>s[0-9]+)\), &\(xvalue\*\)\{0\}, sizeof\(s[0-9]+\) \},.*?xl6SlotRuntimeV1Get\(\)->Apply.*?xrtValueRelease\(xlir_released\)',
        'Singleline')
    if (!$detach.Success -or $detach.Groups['source'].Value -ne $detach.Groups['cleared'].Value) {
        throw 'RELEASE must detach the owner before invoking its destructor'
    }
    if (!(Test-Path -LiteralPath build/test/xl.dll) -or
        (Get-FileHash release/xl.dll).Hash -ne (Get-FileHash build/test/xl.dll).Hash) {
        Copy-Item -LiteralPath release/xl.dll -Destination build/test/xl.dll -Force
    }
    & gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-label -Wno-unused-variable -Wno-unused-parameter -Wno-unused-const-variable -Wno-missing-field-initializers -Ibuild/test test/cases/xlir_cleanup_detach_reentry_host.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Cleanup reentry strict C compile failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Destructor reentry observed an attached owner' }
} finally { Pop-Location }
Write-Output 'XLIR RELEASE passed: storage detached before destructor reentry, three init/fini cycles and deterministic strict C.'
exit 0
