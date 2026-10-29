param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('semver_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/semver')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$outcome = 'failed'
$failure = ''
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/semver') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','test/cases/extlib_semver.xl')) {
        Join-Path $root $path
    }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments, [int]$seconds = 120) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path $seconds $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[semver] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'C ABI contract' $gcc ($flags + @('-fsyntax-only',
        'release/extlibs/semver/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/semver/test/xrt_runtime.c', '-o', $runtime))

    $native = 'release/extlibs/semver/semver_native.c'
    $test = Join-Path $work 'semver_test.exe'
    $null = Run 'C test compile' $gcc ($flags + @($native,
        'release/extlibs/semver/test/test_semver.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'semver_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/semver/examples/semver_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/semver/examples/semver_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }
    $rangesGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/semver/examples/ranges.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $rangesC = Join-Path $work 'ranges-c.exe'
    $null = Run 'C ranges example compile' $gcc ($flags + @('release/extlibs/semver/examples/ranges_c.c',$runtime) + $links + @('-o',$rangesC))
    $rangesOutput = Run 'C ranges example' $rangesC @()
    $rangesDirect = Run 'C ranges direct xl run' $xl @('run','release/extlibs/semver/examples/ranges_c.c')
    if ($rangesOutput -cne $rangesGolden -or $rangesDirect -cne $rangesGolden) { throw 'C range filtering example output differs' }

    $module = Join-Path $work 'module.c'
    $null = Run 'generated ABI source' $xl @('cgen','release/extlibs/semver/examples/basic.xl','--emit',$module)
    $decls = @(Get-Content -LiteralPath $module -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bxsemver_[a-z_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/semver/src/semver.xl')),'extern "(xsemver_[a-z_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxsemver_[a-z_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne $expected.Count -or @(Compare-Object $expected $actual).Count) { throw 'Semver generated ABI coverage incomplete' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"semver_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated C prototypes' $gcc ($flags + @('-fsyntax-only',$abi))

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang existing test' $xl @('run', '../test/cases/extlib_semver.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/semver/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/semver/test/allocations.xl')
        $null = Run 'XLang prerelease ranges' $xl @('run','extlibs/semver/test/prerelease_ranges.xl')
        $rangesXL = Run 'XLang ranges example' $xl @('run','extlibs/semver/examples/ranges.xl')
        if ($rangesXL -cne $rangesGolden) { throw 'XLang range filtering output differs from C' }
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/semver/examples/basic.xl')
        $goldenPath = Join-Path $root 'release/extlibs/semver/examples/basic.expected.txt'
        $golden = [IO.File]::ReadAllText($goldenPath).Replace("`r`n", "`n").TrimEnd("`n")
        if ($cOutput -cne $golden -or $xlOutput -cne $golden) {
            throw 'C/XLang example output differs from the common golden file'
        }
        Write-Host '[semver] C/XLang golden: PASS'

        if (-not $SkipAot) {
            $centralAot = Join-Path $work 'semver_central.exe'
            $null = Run 'XLang central build' $xl @('build',
                '../test/cases/extlib_semver.xl', '-o', $centralAot)
            $null = Run 'XLang central AOT' $centralAot @()
            $target = Join-Path $work 'semver_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/semver/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $allocationsAot = Join-Path $work 'semver_allocations.exe'
            $null = Run 'XLang allocation failures build' $xl @('build',
                'extlibs/semver/test/allocations.xl', '-o', $allocationsAot)
            $null = Run 'XLang allocation failures AOT' $allocationsAot @()
            $exampleAot = Join-Path $work 'semver_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/semver/examples/basic.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $golden) {
                throw 'XLang AOT example differs from the common golden file'
            }
            $cExampleAot = Join-Path $work 'semver_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/semver/examples/semver_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $golden) {
                throw 'C AOT example differs from the common golden file'
            }
            $rangeTest = Join-Path $work 'prerelease-ranges.exe'
            $null = Run 'prerelease ranges build' $xl @('build','extlibs/semver/test/prerelease_ranges.xl','-o',$rangeTest)
            $null = Run 'prerelease ranges AOT' $rangeTest @()
            foreach ($source in @('ranges.xl','ranges_c.c')) {
                $binary = Join-Path $work ($source.Replace('.','-')+'.exe')
                $null = Run "$source build" $xl @('build',"extlibs/semver/examples/$source",'-o',$binary)
                $text = Run "$source AOT" $binary @()
                if ($text -cne $rangesGolden) { throw "$source AOT output differs from C" }
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
    $report = Join-Path $testRoot (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[semver] report: $report"
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if ($outcome -eq 'passed' -and (Test-Path -LiteralPath $safeWork)) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
    if ($changed.Count) { throw 'Inputs changed during semver gate' }
}
