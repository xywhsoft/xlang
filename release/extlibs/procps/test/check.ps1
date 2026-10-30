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
$powershell = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$checks = [Collections.Generic.List[object]]::new()
$utf8 = [Text.UTF8Encoding]::new($false)
function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/procps') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
        'lib/xruntime/xruntime.h','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1',
        'tools/xlir_native_process.ps1','test/cases/extlib_procps.xl','release/examples/extlibs/procps.xl')) {
        Join-Path $root $item
    }
    $gcc
    $powershell
}
$inputs = @(InputPaths | Sort-Object -Unique)
$hashes = [ordered]@{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false
$failure = ''

function Confirm-Output([string]$name, [string[]]$lines) {
    $actual = [string]::Join("`n", @($lines | ForEach-Object { $_ -replace "`r$", '' }))
    if ($actual -cne $expected) { throw "$name differs from fixed procps example" }
}

function Run([string]$name, [string]$program, [string[]]$arguments) {
    if ([IO.Path]::GetExtension($program) -eq '.ps1') {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $powershell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') { throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)" }
    Write-Host "[procps] ${name}: PASS"
    return $result.stdout.Replace("`r`n","`n").TrimEnd("`n").Split("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public C header' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-x', 'c', '-fsyntax-only', '-Ilib/xrt', 'release/extlibs/procps/include/procps_native.h')
    $null = Run 'public C header with value' $gcc @('-std=c11','-Wall','-Wextra','-Werror',
        '-DXRT_MODULE_VALUE_CONTAINER','-x','c','-fsyntax-only','-Ilib/xrt','release/extlibs/procps/include/procps_native.h')
    $null = Run 'manifest macro compile' $gcc (@('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/procps/include') +
        $moduleFlags + @('release/extlibs/procps/procps_native.c'))
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
    $runtime = Join-Path $work 'runtime.o'
    $native = Join-Path $work 'native.o'
    $links = @('-lPsapi','-lWs2_32','-lAdvapi32','-lIphlpapi','-lm')
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/procps/test/xrt_runtime.c', '-o', $runtime))
    $null = Run 'native object' $gcc ($flags + @('-c','release/extlibs/procps/procps_native.c','-o',$native))
    $nativeTest = Join-Path $work 'test_procps.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/procps/test/test_procps.c',$native,$runtime) + $links + @('-o',$nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    $null = Run 'TCC C boundaries' $runner @('procps', 'release/extlibs/procps/test/test_procps.c')
    $names = Join-Path $work 'names.exe'
    $nameSource = 'release/extlibs/procps/test/name_windows_c.c'
    $null = Run 'Windows name compile' $gcc ($flags + @('-DNDEBUG',$nameSource,$runtime) + $links + @('-o',$names))
    $null = Run 'Windows Unicode names' $names @()
    $null = Run 'Windows names C direct' $xl @('run',$nameSource)
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
    $generated = Join-Path $work 'module.c'
    $null = Run 'full API cgen' $xl @('cgen','release/examples/extlibs/procps.xl','--emit',$generated)
    $decls = @(Get-Content -Encoding UTF8 -LiteralPath $generated | Where-Object { $_ -match '^extern .*\bxprocps_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expectedNames = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/procps/src/procps.xl')),'extern "(xprocps_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxprocps_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 7 -or @(Compare-Object $expectedNames $actual).Count) { throw 'Incomplete procps generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"procps_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'seven actual prototypes' $gcc ($flags + @('-Irelease/extlibs/procps/include','-fsyntax-only',$abi))

    $example = Join-Path $work 'example.exe'
    $null = Run 'GCC C example compile' $gcc ($flags + @('-DNDEBUG','release/extlibs/procps/examples/procps_c.c',$native,$runtime) + $links + @('-o',$example))
    Confirm-Output 'GCC C example' @(Run 'GCC C example' $example @())
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
            @{Name='owned'; Source='release/extlibs/procps/test/owned_results.xl'},
            @{Name='windows_names'; Source=$nameSource}
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
    $passed = $true
    Write-Host $(if ($SkipAot) { 'procps Windows focused gate: PASS (AOT skipped)' }
        else { 'procps Windows complete package gate: PASS' })
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    Pop-Location
    $changed = @($hashes.Keys | Where-Object { !(Test-Path -LiteralPath $_) -or (Get-FileHash -LiteralPath $_).Hash -cne $hashes[$_] })
    $changed += @(InputPaths | Where-Object { !$hashes.Contains($_) })
    if ($changed.Count) { $passed = $false }
    [IO.File]::WriteAllText(($work+'.json'),([ordered]@{date=(Get-Date).ToString('o');passed=$passed;
        failure=$failure;aotSkipped=[bool]$SkipAot;inputs=$hashes;changedInputs=$changed;
        checks=@($checks.ToArray())} | ConvertTo-Json -Depth 8),$utf8)
    Write-Host "[procps] report: $work.json"
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
    if ($changed.Count) { throw 'Procps inputs changed during validation' }
}
