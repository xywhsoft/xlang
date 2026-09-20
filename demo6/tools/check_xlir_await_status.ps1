param([string]$Root = '', [Parameter(Mandatory=$true)][string]$SourceDriver)
$ErrorActionPreference = 'Stop'
if ($Root -eq '') { $Root = Split-Path -Parent $PSScriptRoot }
Push-Location $Root
try {
    $prefix = 'build/test/xlir_await_status'
    & $SourceDriver test/cases/xlir_await_status.xl "$prefix.c" "$prefix.h" "$prefix.json"
    if ($LASTEXITCODE -ne 0) { throw 'Await status source generation failed' }
    & $SourceDriver test/cases/xlir_await_status.xl "$prefix.repeat.c" "$prefix.repeat.h" "$prefix.repeat.json"
    if ($LASTEXITCODE -ne 0) { throw 'Await status repeat generation failed' }
    $ir = Get-Content "$prefix.json" -Raw | ConvertFrom-Json
    $awaits = @($ir.instructions | Where-Object kindName -eq 'await')
    if ($awaits.Count -ne 2 -or @($awaits | Where-Object awaitOutcomeAbi -ne 1).Count) { throw 'Blocking/generator await status carrier missing' }
    foreach ($extension in @('c','h','json')) {
        if ((Get-FileHash "$prefix.$extension").Hash -ne (Get-FileHash "$prefix.repeat.$extension").Hash) { throw "Await nondeterministic $extension" }
    }
    $flags = @('-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-label','-Wno-unused-variable',
        '-Wno-unused-parameter','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-I.','-Ibuild/test')
    & gcc @flags -DxrtStrDup=await_status_copy -include "$prefix.c" -c test/cases/xlir_await_status_host.c -o "$prefix.host.o"
    if ($LASTEXITCODE -ne 0) { throw 'Await status strict C compile failed' }
    & gcc @flags "$prefix.host.o" test/cases/xlir_await_status_runtime.c release/xl.dll -o "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Await status link failed' }
    & "$prefix.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Await status acquisition/cleanup contract failed' }
} finally { Pop-Location }
