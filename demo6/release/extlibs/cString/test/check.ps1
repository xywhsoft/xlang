param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/cString_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$hashes = [ordered]@{}
$utf8 = [Text.UTF8Encoding]::new($false)
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$outcome = 'failed'
$failure = ''

function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/cString') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($path in @('lib/xrt/xrt.h','lib/xruntime/xruntime.h','release/xl.exe','release/xl.dll','release/xlc.dll',
        'tools/extlib_smoke_process.ps1','tools/xlir_native_process.ps1','dev/extlib_blockers/native_c_scalar_interpolation.xl')) {
        Join-Path $root $path
    }
    $gcc
}
foreach ($path in @(InputPaths | Sort-Object -Unique)) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $result = Invoke-ExtlibSmokeProcess $program $arguments (Get-Location).Path 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[cString] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'compiler version' $gcc @('--version')
    $test = Join-Path $work 'cString_test.exe'
    $null = Run 'C test compile' $gcc @('-std=c11', '-O2', '-Wall',
        '-Wextra', '-Werror', '-DNDEBUG', 'release/extlibs/cString/test/test_cstring.c',
        '-o', $test)
    $null = Run 'C test' $test @()

    $example = Join-Path $work 'cString_example.exe'
    $null = Run 'C example compile' $gcc @('-std=c11', '-O2', '-Wall',
        '-Wextra', '-Werror', 'release/extlibs/cString/examples/cString_c.c',
        '-o', $example)
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/cString/examples/cString_c.c')
    $golden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/cString/examples/cString.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    if ($cOutput -cne $golden -or $directOutput -cne $golden) { throw 'cString basic golden mismatch' }
    $recordsGolden = [IO.File]::ReadAllText((Join-Path $root 'release/extlibs/cString/examples/records.expected.txt')).Replace("`r`n","`n").TrimEnd("`n")
    $recordsExe = Join-Path $work 'records-c.exe'
    $null = Run 'C records compile' $gcc @('-std=c11','-O2','-Wall','-Wextra','-Werror',
        'release/extlibs/cString/examples/records_c.c','-o',$recordsExe)
    $recordsC = Run 'C records' $recordsExe @()
    $recordsDirect = Run 'C records xl run' $xl @('run','release/extlibs/cString/examples/records_c.c')
    if ($recordsC -cne $recordsGolden -or $recordsDirect -cne $recordsGolden) { throw 'cString C records golden mismatch' }
    $generated = Join-Path $work 'module.c'
    $null = Run 'generated source' $xl @('cgen','release/extlibs/cString/test/interop.xl','--emit',$generated)
    $decls = @(Get-Content -LiteralPath $generated -Encoding UTF8 | Where-Object { $_ -match '^extern .*\bstrlen\(' } | Sort-Object -Unique)
    if ($decls.Count -ne 1) { throw 'cString must declare one CRT strlen symbol' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include <string.h>`n" + ($decls -join "`n"),$utf8)
    $null = Run 'generated CRT prototype' $gcc @('-std=c11','-Wall','-Wextra','-Werror','-fsyntax-only',$abi)

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/cString/test/boundaries.xl')
        $null = Run 'XLang interop' $xl @('run','extlibs/cString/test/interop.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/cString/examples/cString.xl')
        if ($cOutput -cne $directOutput -or $cOutput -cne $xlOutput) {
            throw "C/XLang cString example outputs differ:`nC:`n$cOutput`nTCC:`n$directOutput`nXL:`n$xlOutput"
        }
        Write-Host '[cString] C/XLang parity: PASS'

        if (-not $SkipAot) {
            $target = Join-Path $work 'cString_xl_test.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/cString/test/boundaries.xl', '-o', $target)
            $null = Run 'XLang boundaries AOT' $target @()
            $exampleAot = Join-Path $work 'cString_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/cString/examples/cString.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT cString example differs from C'
            }
            $cExampleAot = Join-Path $work 'cString_c_example_aot.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/cString/examples/cString_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT cString example differs from GCC C'
            }
            $interopExe = Join-Path $work 'interop.exe'
            $null = Run 'XLang interop build' $xl @('build','extlibs/cString/test/interop.xl','-o',$interopExe)
            $null = Run 'XLang interop AOT' $interopExe @()
            $recordsAot = Join-Path $work 'records-c-aot.exe'
            $null = Run 'C records build' $xl @('build','extlibs/cString/examples/records_c.c','-o',$recordsAot)
            $recordsText = Run 'C records AOT' $recordsAot @()
            if ($recordsText -cne $recordsGolden) { throw 'cString C records AOT golden mismatch' }
        }
        # 保留真实的 c_size 插值正例；当前主线拒绝时，默认门禁必须失败。
        $recordsXL = Run 'XLang records' $xl @('run','extlibs/cString/examples/records.xl')
        if ($recordsXL -cne $recordsGolden) { throw 'cString records language golden mismatch' }
        if (-not $SkipAot) {
            $recordsExe = Join-Path $work 'records-xl.exe'
            $null = Run 'XLang records build' $xl @('build','extlibs/cString/examples/records.xl','-o',$recordsExe)
            $recordsText = Run 'XLang records AOT' $recordsExe @()
            if ($recordsText -cne $recordsGolden) { throw 'cString records AOT golden mismatch' }
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
    $report = Join-Path $workParent (([IO.Path]::GetFileName($work)) + '.json')
    [IO.File]::WriteAllText($report,([ordered]@{date=(Get-Date).ToString('o');outcome=$outcome;failure=$failure;
        aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[cString] report: $report"
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "cString temporary work path escaped build/test: $workPath"
    }
    if ($outcome -eq 'passed') { Remove-Item -LiteralPath $workPath -Recurse -Force }
    if ($changed.Count) { throw 'Inputs changed during cString gate' }
}
