param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('units_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/units/include')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/units') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'test/cases/extlib_units.xl',
    'release/examples/extlibs/units.xl') | ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$name, [string]$program, [string[]]$arguments) {
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
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[units] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/units/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/units/test/xrt_runtime.c', '-o', $runtime))
    $native = Join-Path $work 'units_native.o'
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/units/units_native.c', '-o', $native))
    foreach ($case in @('test_units', 'test_threads')) {
        $target = Join-Path $work "$case.exe"
        $null = Run "$case compile" $gcc ($flags + @(
            "release/extlibs/units/test/$case.c", $native, $runtime) +
            $links + @('-o', $target))
        $null = Run $case $target @()
    }
    $example = Join-Path $work 'example_units.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/units/examples/units_c.c', $native, $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/units/examples/units_c.c')
    if ($directOutput -cne $cOutput) {
        throw 'direct C example output differs from GCC example'
    }

    $null = Run 'XLang central boundaries' $xl @('run', 'test/cases/extlib_units.xl')
    $null = Run 'XLang package boundaries' $xl @('run',
        'release/extlibs/units/test/boundaries.xl')
    $null = Run 'XLang allocation failures' $xl @('run',
        'release/extlibs/units/test/allocations.xl')
    $null = Run 'XLang owned result budgets' $xl @('run',
        'release/extlibs/units/test/owned_results.xl')
    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/units/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($name in @('xu_bytes', 'xu_bytes_decimal', 'xu_duration',
        'xu_duration_short', 'xu_count')) {
        if ([regex]::Matches($generatedText, "\b\w+\.Data\s*=\s*$name\s*\(").Count -ne 1) {
            throw "$name must adopt the native C string exactly once in the probe"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'units must not copy or re-adopt native text'
    }
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/units.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[units] C/XLang parity: PASS'

    if (-not $SkipAot) {
        foreach ($case in @('boundaries', 'allocations', 'owned_results')) {
            $binary = Join-Path $work "$case.exe"
            $null = Run "$case AOT build" $xl @('build',
                "release/extlibs/units/test/$case.xl", '-o', $binary)
            $null = Run "$case AOT" $binary @()
        }
        $aot = Join-Path $work 'units_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'test/cases/extlib_units.xl', '-o', $aot)
        $null = Run 'XLang core AOT' $aot @()
        $exampleAot = Join-Path $work 'units_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/units.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $cOutput) {
            throw 'XLang AOT example output differs from C'
        }
        $cExampleAot = Join-Path $work 'units_c_example.exe'
        $null = Run 'C example AOT build' $xl @('build',
            'release/extlibs/units/examples/units_c.c', '-o', $cExampleAot)
        $cAotOutput = Run 'C example AOT' $cExampleAot @()
        if ($cAotOutput -cne $cOutput) {
            throw 'C AOT example output differs from GCC'
        }
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "units validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host 'units Windows package gate: PASS'
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        if ($passed) {
            Get-ChildItem -LiteralPath $safeWork -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $safeWork -Force
        } else {
            Write-Warning "units failure artifacts retained: $safeWork"
        }
    }
}
