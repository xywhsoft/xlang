param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('hyperloglog_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'hyperloglog.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/hyperloglog/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed HLL example" }
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/hyperloglog/include @moduleFlags `
        release/extlibs/hyperloglog/hyperloglog_native.c
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog manifest macro compile failed' }
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        release/extlibs/hyperloglog/test/abi_contract.c
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C ABI contract failed' }

    foreach ($source in @('test_hyperloglog.c', 'test_threads.c')) {
        & $runner -Package hyperloglog -Source "release/extlibs/hyperloglog/test/$source"
        if ($LASTEXITCODE -ne 0) { throw "hyperloglog C test failed: $source" }
    }
    foreach ($source in @(
        'test/cases/extlib_hyperloglog.xl',
        'release/extlibs/hyperloglog/test/boundaries.xl',
        'release/extlibs/hyperloglog/test/named_type.xl',
        'release/extlibs/hyperloglog/test/allocations.xl'
    )) {
        & $xl run $source
        if ($LASTEXITCODE -ne 0) { throw "hyperloglog XLang test failed: $source" }
    }
    foreach ($method in @('add', 'merge')) {
        $oldPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $diagnostic = ((& $xl check `
                "release/extlibs/hyperloglog/test/private_${method}_negative.xl" 2>&1 |
                ForEach-Object { $_.ToString() }) -join "`n")
            $negativeExitCode = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $oldPreference
        }
        if ($negativeExitCode -eq 0 -or
            -not $diagnostic.Contains("${method}Native") -or
            -not $diagnostic.Contains('access')) {
            throw "hyperloglog private $method check failed: $diagnostic"
        }
    }

    $xlLines = @(& $xl run release/examples/extlibs/hyperloglog.xl)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog XLang example failed' }
    Confirm-Output 'XLang example' $xlLines
    $cLines = @(& $runner -Package hyperloglog `
        -Source release/extlibs/hyperloglog/examples/hyperloglog_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C package example failed' }
    Confirm-Output 'C package example' $cLines
    $direct = @(& $xl run release/extlibs/hyperloglog/examples/hyperloglog_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'hyperloglog direct C example failed' }
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'hyperloglog_core.exe'
        & $xl build test/cases/extlib_hyperloglog.xl -o $core
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog core AOT build failed' }
        & $core
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog core AOT run failed' }

        $example = Join-Path $work 'hyperloglog_example.exe'
        & $xl build release/examples/extlibs/hyperloglog.xl -o $example
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog example AOT build failed' }
        $aotLines = @(& $example)
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog example AOT run failed' }
        Confirm-Output 'XLang example AOT' $aotLines

        foreach ($name in @('boundaries', 'named_type', 'allocations')) {
            $source = "release/extlibs/hyperloglog/test/$name.xl"
            $output = Join-Path $work "hyperloglog_$name.exe"
            & $xl build $source -o $output
            if ($LASTEXITCODE -ne 0) { throw "hyperloglog $name AOT build failed" }
            & $output
            if ($LASTEXITCODE -ne 0) { throw "hyperloglog $name AOT run failed" }
        }

        $cExample = Join-Path $work 'hyperloglog_c_example.exe'
        & $xl build release/extlibs/hyperloglog/examples/hyperloglog_c.c -o $cExample
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C example AOT build failed' }
        $cAotLines = @(& $cExample)
        if ($LASTEXITCODE -ne 0) { throw 'hyperloglog C example AOT run failed' }
        Confirm-Output 'C example AOT' $cAotLines
    }

    Write-Output $(if ($SkipAot) { 'hyperloglog Windows C/XLang focused gate: PASS' }
        else { 'hyperloglog Windows C/XLang and AOT gate: PASS' })
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
