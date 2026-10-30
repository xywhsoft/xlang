param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$manifest = Get-Content -LiteralPath (Join-Path $root 'release/extlibs/diff/xlang.package.json') -Raw | ConvertFrom-Json
$moduleFlags = @($manifest.artifact.xrtModules | ForEach-Object { "-D$_" })
$expected = [string]::Join("`n", (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'diff.expected.txt') -Encoding UTF8))
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = [IO.Path]::GetFullPath((Join-Path $testRoot ('diff_guard_' + [guid]::NewGuid().ToString('N'))))
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/diff') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_diff.xl', 'release/examples/extlibs/diff.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Confirm-Output([string] $name, [string[]] $lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed diff example" }
}

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[diff] ${name}: PASS"
    return $output
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/diff/include') + $moduleFlags +
        @('release/extlibs/diff/diff_native.c'))
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/diff/test/runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_diff.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/diff/diff_native.c', 'release/extlibs/diff/test/test_diff.c',
        $runtime, '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()

    foreach ($source in @(
        'release/extlibs/diff/test/test_diff.c',
        'release/extlibs/diff/test/test_unterminated.c',
        'release/extlibs/diff/test/test_threads.c'
    )) {
        $null = Run $source $runner @('diff', $source)
    }
    foreach ($source in @('test/cases/extlib_diff.xl', 'release/extlibs/diff/test/boundaries.xl',
        'release/extlibs/diff/test/owned_results.xl')) {
        $null = Run $source $xl @('run', $source)
    }
    $generated = Join-Path $work 'diff_owned.c'
    $null = Run 'whole-call ownership cgen' $xl @('cgen',
        'release/extlibs/diff/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($symbol in @('xdiff_lines', 'xdiff_stats')) {
        if ([regex]::Matches($generatedText, "\b\w+\.Data\s*=\s*$symbol\s*\(").Count -ne 1) {
            throw "$symbol must adopt the native C string exactly once in the probe"
        }
    }
    if ($generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'diff must not copy validated inputs or adopted result text'
    }

    $cLines = @(Run 'C example' $runner @('diff', 'release/extlibs/diff/examples/diff_c.c'))
    Confirm-Output 'C package example' $cLines
    $directLines = @(Run 'direct C example' $xl @('run', 'release/extlibs/diff/examples/diff_c.c'))
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(Run 'XLang example' $xl @('run', 'release/examples/extlibs/diff.xl'))
    Confirm-Output 'XLang example' $xlLines

    if (-not $SkipAot) {
        foreach ($entry in @(
            @{Name='central'; Source='test/cases/extlib_diff.xl'},
            @{Name='owned'; Source='release/extlibs/diff/test/owned_results.xl'}
        )) {
            $binary = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $binary)
            $null = Run "$($entry.Name) AOT" $binary @()
        }
        $example = Join-Path $work 'diff_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build', 'release/examples/extlibs/diff.xl', '-o', $example)
        $aotLines = @(Run 'XLang example AOT' $example @())
        Confirm-Output 'XLang example AOT' $aotLines

        $boundary = Join-Path $work 'diff_boundaries.exe'
        $null = Run 'boundaries AOT build' $xl @('build', 'release/extlibs/diff/test/boundaries.xl', '-o', $boundary)
        $boundaryLines = @(Run 'boundaries AOT' $boundary @())
        if ([string]::Join("`n", @($boundaryLines | ForEach-Object { $_ -replace "`r$", '' })) -cne
            'diff boundaries: PASS') {
            throw 'diff XLang boundaries AOT output changed'
        }
        $cExample = Join-Path $work 'diff_c_example.exe'
        $null = Run 'C example AOT build' $xl @('build', 'release/extlibs/diff/examples/diff_c.c', '-o', $cExample)
        $cAotLines = @(Run 'C example AOT' $cExample @())
        Confirm-Output 'C example AOT' $cAotLines
    }

    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "diff validation input changed: $path"
        }
    }
    $passed = $true

    if ($SkipAot) {
        Write-Output 'diff Windows C/XLang focused gate: PASS'
    } else {
        Write-Output 'diff Windows C/XLang AOT gate: PASS'
    }
} finally {
    Pop-Location
    if (-not $work.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe diff cleanup path: $work"
    }
    if (Test-Path -LiteralPath $work) {
        if ($passed) {
            Get-ChildItem -LiteralPath $work -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $work -Force
        } else {
            Write-Warning "diff failure artifacts retained: $work"
        }
    }
}
