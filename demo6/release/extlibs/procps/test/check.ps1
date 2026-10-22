param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/procps/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'procps.expected.txt') -Encoding UTF8))

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed procps example" }
}

function Build-Aot([string] $source, [string] $output) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $buildOutput = @(& $xl build $source -o $output 2>&1)
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "procps AOT build failed: $source`n$($buildOutput -join "`n")"
    }
}

$work = $null
Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -x c -fsyntax-only '-Ilib/xrt' `
        release/extlibs/procps/include/procps_native.h
    if ($LASTEXITCODE -ne 0) { throw 'procps public header compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only '-Ilib/xrt' `
        '-Irelease/extlibs/procps/include' @moduleFlags release/extlibs/procps/procps_native.c
    if ($LASTEXITCODE -ne 0) { throw 'procps manifest macro compile failed' }

    & $runner -Package procps -Source release/extlibs/procps/test/test_procps.c
    if ($LASTEXITCODE -ne 0) { throw 'procps C boundaries failed' }
    foreach ($source in @('test/cases/extlib_procps.xl', 'release/extlibs/procps/test/boundaries.xl')) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "procps XLang test failed: $source" }
    }

    $cLines = @(& $runner -Package procps -Source release/extlibs/procps/examples/procps_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'procps C package example failed' }
    Confirm-Output 'C package example' $cLines
    $directLines = @(& $xl run release/extlibs/procps/examples/procps_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'procps direct C example failed' }
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(& $xl run release/examples/extlibs/procps.xl)
    if ($LASTEXITCODE -ne 0) { throw 'procps XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    if (-not $SkipAot) {
        $work = Join-Path ([IO.Path]::GetTempPath()) ('xlang-procps-aot-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $work | Out-Null
        $core = Join-Path $work 'procps_core.exe'
        $example = Join-Path $work 'procps_example.exe'
        Build-Aot 'test/cases/extlib_procps.xl' $core
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'procps core AOT failed' }
        Build-Aot 'release/examples/extlibs/procps.xl' $example
        $aotLines = @(& $example)
        if ($LASTEXITCODE -ne 0) { throw 'procps example AOT failed' }
        Confirm-Output 'XLang AOT example' $aotLines
    }

    if ($SkipAot) {
        Write-Output 'procps Windows C/XLang focused gate: PASS'
    } else {
        Write-Output 'procps Windows C/XLang and AOT release gate: PASS'
    }
} finally {
    Pop-Location
    if ($work -ne $null) {
        foreach ($output in @('procps_core.exe', 'procps_example.exe')) {
            $file = Join-Path $work $output
            if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
        }
        if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Force }
    }
}
