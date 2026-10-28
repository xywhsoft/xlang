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
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('procps_guard_' + [guid]::NewGuid().ToString('N'))
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/procps') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_procps.xl', 'release/examples/extlibs/procps.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

function Confirm-Output([string]$name, [string[]]$lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed procps example" }
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
    Write-Host "[procps] ${name}: PASS"
    return $output
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'public C header' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-x', 'c', '-fsyntax-only', '-Ilib/xrt', 'release/extlibs/procps/include/procps_native.h')
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/procps/include') +
        $moduleFlags + @('release/extlibs/procps/procps_native.c'))
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/procps/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_procps.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/procps/procps_native.c', 'release/extlibs/procps/test/test_procps.c',
        $runtime, '-lPsapi', '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    $null = Run 'TCC C boundaries' $runner @('procps', 'release/extlibs/procps/test/test_procps.c')
    foreach ($source in @('test/cases/extlib_procps.xl',
        'release/extlibs/procps/test/boundaries.xl', 'release/extlibs/procps/test/owned_results.xl')) {
        $null = Run $source $xl @('run', $source)
    }

    $generated = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/procps/test/owned_probe.xl', '--emit', $generated)
    $text = [IO.File]::ReadAllText($generated)
    if ([regex]::Matches($text, '\b\w+\.Data\s*=\s*xprocps_name\s*\(').Count -ne 1 -or
        $text.Contains('xprocps_name_abi') -or $text -match '=\s*xlir_string_copy\s*\(') {
        throw 'procps must transfer nullable success directly without copying'
    }

    $cLines = @(Run 'C example' $runner @('procps', 'release/extlibs/procps/examples/procps_c.c'))
    Confirm-Output 'C example' $cLines
    $directLines = @(Run 'direct C example' $xl @('run', 'release/extlibs/procps/examples/procps_c.c'))
    Confirm-Output 'direct C example' $directLines
    $xlLines = @(Run 'XLang example' $xl @('run', 'release/examples/extlibs/procps.xl'))
    Confirm-Output 'XLang example' $xlLines

    if (-not $SkipAot) {
        foreach ($entry in @(
            @{Name='core'; Source='test/cases/extlib_procps.xl'},
            @{Name='boundaries'; Source='release/extlibs/procps/test/boundaries.xl'},
            @{Name='owned'; Source='release/extlibs/procps/test/owned_results.xl'}
        )) {
            $binary = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $binary)
            $null = Run "$($entry.Name) AOT" $binary @()
        }
        foreach ($entry in @(
            @{Name='xl_example'; Source='release/examples/extlibs/procps.xl'},
            @{Name='c_example'; Source='release/extlibs/procps/examples/procps_c.c'}
        )) {
            $binary = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $binary)
            Confirm-Output "$($entry.Name) AOT" @(Run "$($entry.Name) AOT" $binary @())
        }
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "procps validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host $(if ($SkipAot) { 'procps Windows focused gate: PASS (AOT skipped)' }
        else { 'procps Windows complete package gate: PASS' })
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        if (-not $resolved.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe procps cleanup path: $resolved"
        }
        if ($passed) {
            Get-ChildItem -LiteralPath $resolved -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolved -Force
        } else {
            Write-Warning "procps failure artifacts retained: $resolved"
        }
    }
}
