param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ("fuzzy_guard_$([guid]::NewGuid().ToString('N'))")
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/fuzzy') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_fuzzy.xl', 'release/examples/extlibs/fuzzy.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${label}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${label}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[fuzzy] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'strict C syntax' $gcc @(
        '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only',
        '-Ilib/xrt', '-Irelease/extlibs/fuzzy',
        'release/extlibs/fuzzy/fuzzy_native.c',
        'release/extlibs/fuzzy/test/abi_contract.c',
        'release/extlibs/fuzzy/test/test_fuzzy.c',
        'release/extlibs/fuzzy/test/test_unterminated.c',
        'release/extlibs/fuzzy/test/test_threads.c',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/fuzzy/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_fuzzy.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/fuzzy/fuzzy_native.c', 'release/extlibs/fuzzy/test/test_fuzzy.c',
        $runtime, '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    foreach ($test in @('test_fuzzy', 'test_unterminated', 'test_threads')) {
        $null = Run $test $runner @('fuzzy', "release/extlibs/fuzzy/test/$test.c")
    }
    $cOutput = Run 'C example' $runner @('fuzzy',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    $directOutput = Run 'single-file C example' $xl @('run',
        'release/extlibs/fuzzy/examples/fuzzy_c.c')
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/fuzzy/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/fuzzy/test/allocations.xl')
    $null = Run 'XLang input and owned-result budgets' $xl @('run',
        'release/extlibs/fuzzy/test/owned_results.xl')
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_fuzzy.xl')
    $generated = Join-Path $work 'fuzzy_owned.c'
    $null = Run 'whole-call ownership cgen' $xl @('cgen',
        'release/extlibs/fuzzy/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    if ([regex]::Matches($generatedText, '\b\w+\.Data\s*=\s*xfuzzy_best\s*\(').Count -ne 1 -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'fuzzy must borrow validated inputs and adopt best results without copying'
    }
    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/fuzzy.xl')
    $expected = @(
        'levenshtein(kitten, sitting): 3',
        'ratio(hello, hallo): 80',
        'score(fb, foobar): 7',
        'isMatch(xyz, foobar): false',
        'best(pc, ...): lib/parser.c',
        'best(abc, ...): abc',
        'fuzzy example: PASS'
    ) -join "`n"
    if ($cOutput -cne $expected -or $cOutput -cne $directOutput -or
        $cOutput -cne $xlOutput) {
        throw '[fuzzy] C runner, single-file C, and XLang example output differs'
    }
    Write-Host '[fuzzy] C/XLang example parity: PASS'
    if (-not $SkipAot) {
        $aot = Join-Path $work 'core.exe'
        $null = Run 'core AOT build' $xl @('build',
            'test/cases/extlib_fuzzy.xl', '-o', $aot)
        if ((Run 'core AOT' $aot @()) -cne '') {
            throw '[fuzzy] core AOT produced unexpected output'
        }
        foreach ($case in @('boundaries', 'allocations', 'owned_results')) {
            $caseExe = Join-Path $work "$case.exe"
            $null = Run "$case AOT build" $xl @('build',
                "release/extlibs/fuzzy/test/$case.xl", '-o', $caseExe)
            $null = Run "$case AOT" $caseExe @()
        }
        $example = Join-Path $work 'example.exe'
        $null = Run 'example AOT build' $xl @('build',
            'release/examples/extlibs/fuzzy.xl', '-o', $example)
        if ((Run 'example AOT' $example @()) -cne $cOutput) {
            throw '[fuzzy] example AOT and C output differs'
        }
        $cExample = Join-Path $work 'c_example.exe'
        $null = Run 'C example AOT build' $xl @('build',
            'release/extlibs/fuzzy/examples/fuzzy_c.c', '-o', $cExample)
        if ((Run 'C example AOT' $cExample @()) -cne $expected) {
            throw '[fuzzy] C example AOT differs from fixed output'
        }
        Write-Host '[fuzzy] AOT/C example parity: PASS'
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "fuzzy validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host 'fuzzy Windows package gate: PASS'
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('fuzzy_guard_')) {
        throw "Fuzzy test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        if ($passed) {
            Get-ChildItem -LiteralPath $resolvedWork -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolvedWork -Force
        } else {
            Write-Warning "fuzzy failure artifacts retained: $resolvedWork"
        }
    }
}
