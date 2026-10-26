param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('totp_guard_' + [guid]::NewGuid().ToString('N'))
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'totp.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/totp/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed RFC example" }
}

function Run-Aot([string] $name, [string] $program, [string[]] $arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    return $output
}

Push-Location $root
try {
    & $gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only -Ilib/xrt `
        -Irelease/extlibs/totp/include @moduleFlags release/extlibs/totp/totp_native.c
    if ($LASTEXITCODE -ne 0) { throw 'totp manifest macro compile failed' }

    foreach ($source in @('test_totp.c', 'test_threads.c')) {
        & $runner -Package totp -Source "release/extlibs/totp/test/$source"
        if ($LASTEXITCODE -ne 0) { throw "totp C test failed: $source" }
    }
    $xlOutputs = @{}
    foreach ($source in @(
        'test/cases/extlib_totp.xl',
        'release/extlibs/totp/test/boundaries.xl',
        'release/extlibs/totp/test/allocations.xl'
    )) {
        $lines = @(& $xl run $source)
        if ($LASTEXITCODE -ne 0) { throw "totp XLang test failed: $source" }
        $xlOutputs[$source] = [string]::Join("`n", $lines)
    }

    [IO.Directory]::CreateDirectory($work) | Out-Null
    $generated = Join-Path $work 'owned_text.c'
    & $xl cgen release/extlibs/totp/test/boundaries.xl --emit $generated
    if ($LASTEXITCODE -ne 0) { throw 'totp direct-text cgen failed' }
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($name in @('xtotp_generate', 'xtotp_generate_at', 'xtotp_hotp')) {
        if ($generatedText -cnotmatch "\bv\d+\.Data\s*=\s*$name\s*\(") {
            throw "$name must adopt the native C string directly"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1')) {
        throw 'totp must not copy or re-adopt native text'
    }

    $xlLines = @(& $xl run release/examples/extlibs/totp.xl)
    if ($LASTEXITCODE -ne 0) { throw 'totp XLang example failed' }
    Confirm-Output 'XLang example' $xlLines

    $cLines = @(& $runner -Package totp -Source release/extlibs/totp/examples/totp_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'totp C package example failed' }
    Confirm-Output 'C package example' $cLines

    $direct = @(& $xl run release/extlibs/totp/examples/totp_c.c)
    if ($LASTEXITCODE -ne 0) { throw 'totp direct C example failed' }
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'totp_core.exe'
        $null = Run-Aot 'totp core build' $xl @('build',
            'test/cases/extlib_totp.xl', '-o', $core)
        $null = Run-Aot 'totp core AOT' $core @()
        $example = Join-Path $work 'totp_example.exe'
        $null = Run-Aot 'totp example build' $xl @('build',
            'release/examples/extlibs/totp.xl', '-o', $example)
        $aotLines = @(Run-Aot 'totp example AOT' $example @())
        Confirm-Output 'XLang AOT example' $aotLines
        foreach ($case in @('boundaries', 'allocations')) {
            $target = Join-Path $work "totp_${case}.exe"
            $null = Run-Aot "totp $case build" $xl @('build',
                "release/extlibs/totp/test/$case.xl", '-o', $target)
            $aotLines = @(Run-Aot "totp $case AOT" $target @())
            if ($xlOutputs["release/extlibs/totp/test/$case.xl"] -cne [string]::Join("`n", $aotLines)) {
                throw "totp $case AOT output differs from JIT"
            }
        }
        $cExample = Join-Path $work 'totp_c_example.exe'
        $null = Run-Aot 'totp C example build' $xl @('build',
            'release/extlibs/totp/examples/totp_c.c', '-o', $cExample)
        $cAotLines = @(Run-Aot 'totp C example AOT' $cExample @())
        Confirm-Output 'C example AOT' $cAotLines
    }

    Write-Output $(if ($SkipAot) { 'totp Windows C/XLang quick gate: PASS (AOT skipped)' }
        else { 'totp Windows C/XLang/AOT gate: PASS' })
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "totp temporary work path escaped build/test: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
