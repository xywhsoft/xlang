param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('totp_guard_' + [guid]::NewGuid().ToString('N'))
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'totp.expected.txt') -Encoding UTF8))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/totp/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/totp') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_totp.xl', 'release/examples/extlibs/totp.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name output differs from fixed RFC example" }
}

function Run([string] $name, [string] $program, [string[]] $arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[totp] ${name}: PASS"
    return $output
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/totp/include') +
        $moduleFlags + @('release/extlibs/totp/totp_native.c'))
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/totp/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_totp.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/totp/totp_native.c', 'release/extlibs/totp/test/test_totp.c',
        $runtime, '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()

    foreach ($source in @('test_totp.c', 'test_threads.c')) {
        $null = Run $source $runner @('totp', "release/extlibs/totp/test/$source")
    }
    $xlOutputs = @{}
    foreach ($source in @(
        'test/cases/extlib_totp.xl',
        'release/extlibs/totp/test/boundaries.xl',
        'release/extlibs/totp/test/allocations.xl',
        'release/extlibs/totp/test/owned_results.xl'
    )) {
        $lines = @(Run $source $xl @('run', $source))
        $xlOutputs[$source] = [string]::Join("`n", $lines)
    }

    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/totp/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($name in @('xtotp_generate', 'xtotp_generate_at', 'xtotp_hotp')) {
        if ([regex]::Matches($generatedText, "\b\w+\.Data\s*=\s*$name\s*\(").Count -ne 1) {
            throw "$name must adopt the native C string exactly once in the probe"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'totp must not copy or re-adopt native text'
    }

    $xlLines = @(Run 'XLang example' $xl @('run', 'release/examples/extlibs/totp.xl'))
    Confirm-Output 'XLang example' $xlLines

    $cLines = @(Run 'C example' $runner @('totp', 'release/extlibs/totp/examples/totp_c.c'))
    Confirm-Output 'C package example' $cLines

    $direct = @(Run 'direct C example' $xl @('run', 'release/extlibs/totp/examples/totp_c.c'))
    Confirm-Output 'direct C example' $direct

    if (-not $SkipAot) {
        $core = Join-Path $work 'totp_core.exe'
        $null = Run 'totp core build' $xl @('build',
            'test/cases/extlib_totp.xl', '-o', $core)
        $null = Run 'totp core AOT' $core @()
        $example = Join-Path $work 'totp_example.exe'
        $null = Run 'totp example build' $xl @('build',
            'release/examples/extlibs/totp.xl', '-o', $example)
        $aotLines = @(Run 'totp example AOT' $example @())
        Confirm-Output 'XLang AOT example' $aotLines
        foreach ($case in @('boundaries', 'allocations', 'owned_results')) {
            $target = Join-Path $work "totp_${case}.exe"
            $null = Run "totp $case build" $xl @('build',
                "release/extlibs/totp/test/$case.xl", '-o', $target)
            $aotLines = @(Run "totp $case AOT" $target @())
            if ($xlOutputs["release/extlibs/totp/test/$case.xl"] -cne [string]::Join("`n", $aotLines)) {
                throw "totp $case AOT output differs from JIT"
            }
        }
        $cExample = Join-Path $work 'totp_c_example.exe'
        $null = Run 'totp C example build' $xl @('build',
            'release/extlibs/totp/examples/totp_c.c', '-o', $cExample)
        $cAotLines = @(Run 'totp C example AOT' $cExample @())
        Confirm-Output 'C example AOT' $cAotLines
    }

    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "totp validation input changed: $path"
        }
    }
    $passed = $true
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
        if ($passed) {
            Get-ChildItem -LiteralPath $resolved -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolved -Force
        } else {
            Write-Warning "totp failure artifacts retained: $resolved"
        }
    }
}
