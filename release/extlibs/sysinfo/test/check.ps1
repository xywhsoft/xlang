param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = Join-Path $root 'build/test'
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$work = Join-Path $testRoot ('sysinfo_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/sysinfo')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/sysinfo') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'release/examples/extlibs/sysinfo.xl','test/cases/extlib_sysinfo.xl',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1')) { Join-Path $root $path }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[sysinfo] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

function CheckMetrics([string]$name, [string]$output) {
    if ($output -cnotmatch '\Aavailable: [0-9]+\nusage: ([0-9]+)\nuptime: [0-9]+\z' -or
        [int]$Matches[1] -gt 100) {
        throw "${name}: invalid dynamic metrics`n$output"
    }
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'manifest C syntax' $gcc ($flags + @('-DXRT_MODULE_UNICODE',
        '-DXRT_MODULE_NUMBER_INTEGER','-fsyntax-only','release/extlibs/sysinfo/sysinfo_native.c'))
    $null = Run 'ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/sysinfo/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $runtimeSource = Join-Path $root 'release/extlibs/sysinfo/test/xrt_runtime.c'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        $runtimeSource, '-o', $runtime))
    $native = 'release/extlibs/sysinfo/sysinfo_native.c'
    $test = Join-Path $work 'sysinfo_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @($native,
        'release/extlibs/sysinfo/test/test_sysinfo.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $registry = Join-Path $work 'sysinfo_registry.exe'
    $null = Run 'C registry fixture compile' $gcc ($flags + @(
        'release/extlibs/sysinfo/test/registry_name.c', $runtime) +
        $links + @('-o', $registry))
    $null = Run 'C registry fixture' $registry @()

    $example = Join-Path $work 'sysinfo_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/sysinfo/examples/sysinfo_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $metrics = Join-Path $work 'sysinfo_metrics.exe'
    $null = Run 'C metrics compile' $gcc ($flags + @(
        'release/extlibs/sysinfo/examples/metrics_c.c', $runtime) +
        $links + @('-o', $metrics))
    $cMetrics = Run 'C metrics' $metrics @()
    CheckMetrics 'C metrics' $cMetrics

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_sysinfo.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/sysinfo/test/boundaries.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/sysinfo/test/allocations.xl')
        $generated = Join-Path $work 'owned_text.c'
        $null = Run 'direct-text cgen' $xl @('cgen',
            'extlibs/sysinfo/test/boundaries.xl', '--emit', $generated)
        $generatedText = [IO.File]::ReadAllText($generated)
        $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxsysinfo_[a-z0-9_]+\(' } | Sort-Object -Unique)
        $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/sysinfo/src/sysinfo.xl')),'extern "(xsysinfo_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        $actual = @([regex]::Matches(($decls -join "`n"),'\bxsysinfo_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
        if ($decls.Count -ne 11 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete sysinfo generated ABI coverage' }
        $abi = Join-Path $work 'abi.c'
        [IO.File]::WriteAllText($abi,"#include `"sysinfo_native.h`"`n" + ($decls -join "`n"),$utf8)
        $null = Run 'generated eleven prototypes' $gcc ($flags + @('-I../lib/xrt',
            '-Iextlibs/sysinfo','-fsyntax-only',$abi))
        foreach ($name in @('os_name', 'os_version', 'arch', 'hostname', 'cpu_name')) {
            if ($generatedText -notmatch "(?m)\.Data\s*=\s*xsysinfo_$name\s*\(") {
                throw "owned sysinfo result was not captured directly: xsysinfo_$name"
            }
        }
        Write-Host '[sysinfo] owned string adoption: PASS'
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/sysinfo/examples/basic.xl')
        if ($cOutput -cne $xlOutput) {
            throw 'C/XLang system information differs on the same machine'
        }
        $directC = Run 'C example xl run' $xl @('run', 'extlibs/sysinfo/examples/sysinfo_c.c')
        if ($directC -cne $cOutput) {
            throw 'direct C system information differs from GCC output'
        }
        Write-Host '[sysinfo] C/XLang parity: PASS'
        $xlMetrics = Run 'XLang metrics' $xl @('run', 'extlibs/sysinfo/examples/metrics.xl')
        CheckMetrics 'XLang metrics' $xlMetrics
        $directMetrics = Run 'C metrics xl run' $xl @('run', 'extlibs/sysinfo/examples/metrics_c.c')
        CheckMetrics 'C metrics xl run' $directMetrics
        $null = Run 'public XLang example' $xl @('run','examples/extlibs/sysinfo.xl')

        if (-not $SkipAot) {
            $target = Join-Path $work 'sysinfo_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/sysinfo/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $allocAot = Join-Path $work 'sysinfo_xl_allocations.exe'
            $null = Run 'XLang allocations build' $xl @('build',
                'extlibs/sysinfo/test/allocations.xl', '-o', $allocAot)
            $allocOutput = Run 'XLang allocations AOT' $allocAot @()
            if ($allocOutput -cne 'sysinfo allocations: PASS') {
                throw "unexpected XLang allocation AOT output: $allocOutput"
            }
            $exampleAot = Join-Path $work 'sysinfo_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/sysinfo/examples/basic.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT system information differs from C'
            }
            $metricsAot = Join-Path $work 'sysinfo_xl_metrics.exe'
            $null = Run 'XLang metrics build' $xl @('build',
                'extlibs/sysinfo/examples/metrics.xl', '-o', $metricsAot)
            $aotMetrics = Run 'XLang metrics AOT' $metricsAot @()
            CheckMetrics 'XLang metrics AOT' $aotMetrics
            $cExampleAot = Join-Path $work 'sysinfo_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/sysinfo/examples/sysinfo_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT system information differs from GCC'
            }
            $cMetricsAot = Join-Path $work 'sysinfo_c_metrics.exe'
            $null = Run 'C metrics build' $xl @('build',
                'extlibs/sysinfo/examples/metrics_c.c', '-o', $cMetricsAot)
            $cAotMetrics = Run 'C metrics AOT' $cMetricsAot @()
            CheckMetrics 'C metrics AOT' $cAotMetrics
            foreach ($source in @('examples/extlibs/sysinfo.xl','../test/cases/extlib_sysinfo.xl')) {
                $target = Join-Path $work ([IO.Path]::GetFileNameWithoutExtension($source)+'.exe')
                $null = Run "$source build" $xl @('build',$source,'-o',$target)
                $null = Run "$source AOT" $target @()
            }
        }
    } finally {
        Pop-Location
    }
    $outcome = 'passed'
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $outcome = 'invalid-input-change' }
    $report = $work + '.json'
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[sysinfo] report: $report"
    $safeRoot = [IO.Path]::GetFullPath($testRoot).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($safeRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'refusing to remove a build directory outside build/test'
    }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $safeWork -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during sysinfo gate' }
}
