param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('pkcs_guard_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$powershell = Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
. (Join-Path $root 'tools/extlib_smoke_process.ps1')
$checks = [Collections.Generic.List[object]]::new()
$environment = @{XLANG_PACKAGE_PATH = Join-Path $root 'release/extlibs'}
$utf8 = [Text.UTF8Encoding]::new($false)
function InputPaths {
    Get-ChildItem -LiteralPath (Join-Path $root 'release/extlibs/pkcs') -Recurse -File | ForEach-Object FullName
    Get-ChildItem -LiteralPath (Join-Path $root 'release/include') -Recurse -File | ForEach-Object FullName
    foreach ($item in @('release/xl.exe','release/xl.dll','release/xlc.dll','lib/xrt/xrt.h',
        'lib/xruntime/xruntime.h','tools/run_extlib_c.ps1','tools/extlib_smoke_process.ps1',
        'tools/xlir_native_process.ps1','test/cases/extlib_pkcs.xl','release/examples/extlibs/pkcs.xl')) {
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
$ownedSymbols = @('get_content', 'encode_int', 'encode_oid', 'decode_oid',
    'wrap_sequence', 'wrap_set', 'encode_octet', 'encode_null', 'encode_bitstring',
    'concat', 'tag_name')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    if ([IO.Path]::GetExtension($program) -eq '.ps1') {
        $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$program) + $arguments
        $program = $powershell
    }
    $result = Invoke-ExtlibSmokeProcess $program $arguments $root 120 $environment
    $checks.Add([ordered]@{name=$name;program=$program;arguments=$arguments;result=$result})
    if ($result.status -ne 'pass') {
        throw "${name}: $($result.status) exit=$($result.exitCode)`n$($result.stderr)`n$($result.stdout)"
    }
    Write-Host "[pkcs] ${name}: PASS"
    return $result.stdout.Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'compiler version' $gcc @('--version')
    $null = Run 'public header' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-x', 'c', '-Ilib/xrt',
        'release/extlibs/pkcs/include/pkcs_native.h')
    $null = Run 'public header with XRT value' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-x', 'c', '-DXRT_MODULE_VALUE', '-Ilib/xrt',
        'release/extlibs/pkcs/include/pkcs_native.h')
    $null = Run 'strict GCC syntax' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-fsyntax-only', '-Ilib/xrt', 'release/extlibs/pkcs/pkcs_native.c')
    $null = Run 'C boundaries' $runner @('pkcs',
        'release/extlibs/pkcs/test/test_pkcs.c')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime compile' $gcc ($flags + @('-c',
        'release/extlibs/pkcs/test/xrt_runtime.c', '-o', $runtime))
    $nativeTest = Join-Path $work 'test_pkcs.exe'
    $null = Run 'GCC C boundary build' $gcc ($flags + @('release/extlibs/pkcs/pkcs_native.c',
        'release/extlibs/pkcs/test/test_pkcs.c', $runtime) + $links + @('-o', $nativeTest))
    $null = Run 'GCC C boundaries and result budgets' $nativeTest @()
    $cOutput = Run 'C example' $runner @('pkcs',
        'release/extlibs/pkcs/examples/pkcs_c.c')
    $directOutput = Run 'direct C example' $xl @('run',
        'release/extlibs/pkcs/examples/pkcs_c.c')
    if ($directOutput -cne $cOutput) { throw 'direct C example output differs' }
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/pkcs.xl')
    if ($cOutput -cne $xlOutput) { throw 'C/XLang example output differs' }
    $null = Run 'XLang boundaries' $xl @('run',
        'release/extlibs/pkcs/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/pkcs/test/allocations.xl')
    $null = Run 'XLang owned result budgets' $xl @('run',
        'release/extlibs/pkcs/test/owned_results.xl')
    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_pkcs.xl')
    $null = Run 'XLang OID errors' $xl @('run', 'release/extlibs/pkcs/test/oid_errors.xl')
    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'owned-text cgen' $xl @('cgen',
        'release/examples/extlibs/pkcs.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    $decls = @($generatedText -split "`r?`n" | Where-Object { $_ -match '^extern .*\bxpkcs_[a-z0-9_]+\(' } | Sort-Object -Unique)
    $expected = @([regex]::Matches([IO.File]::ReadAllText((Join-Path $root 'release/extlibs/pkcs/src/pkcs.xl')),'extern "(xpkcs_[a-z0-9_]+)"') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
    $actual = @([regex]::Matches(($decls -join "`n"),'\bxpkcs_[a-z0-9_]+(?=\()') | ForEach-Object Value | Sort-Object -Unique)
    if ($decls.Count -ne 16 -or @(Compare-Object $expected $actual).Count) { throw 'Incomplete PKCS generated ABI coverage' }
    $abi = Join-Path $work 'abi.c'
    [IO.File]::WriteAllText($abi,"#include `"pkcs_native.h`"`n" + ($decls -join "`n"),$utf8)
    $null = Run 'sixteen actual prototypes' $gcc ($flags + @('-Irelease/extlibs/pkcs/include','-fsyntax-only',$abi))
    foreach ($symbol in $ownedSymbols) {
        $pattern = '\b\w+\.Data\s*=\s*xpkcs_' + $symbol + '\s*\('
        if ([regex]::Matches($generatedText, $pattern).Count -ne 1) {
            throw "pkcs owned string was not adopted directly: $symbol"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr') -or
        $generatedText.Contains('xl6CStringCopyV1')) {
        throw 'pkcs generated code contains a string transfer wrapper or copy'
    }
    Write-Host '[pkcs] owned string cgen: PASS'
    $probe = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-return ownership cgen' $xl @('cgen',
        'release/extlibs/pkcs/test/owned_probe.xl', '--emit', $probe)
    $probeText = [IO.File]::ReadAllText($probe)
    foreach ($symbol in $ownedSymbols) {
        $pattern = '\b\w+\.Data\s*=\s*xpkcs_' + $symbol + '\s*\('
        if ([regex]::Matches($probeText, $pattern).Count -ne 1) {
            throw "whole-return probe did not cover $symbol exactly once"
        }
    }
    if ($probeText -match '=\s*xlir_string_copy\s*\(') {
        throw 'PKCS still copies the adopted result in a later return function'
    }

    if (-not $SkipAot) {
        foreach ($entry in @(
            @{Name='central'; Source='test/cases/extlib_pkcs.xl'},
            @{Name='allocations'; Source='release/extlibs/pkcs/test/allocations.xl'},
            @{Name='owned'; Source='release/extlibs/pkcs/test/owned_results.xl'},
            @{Name='oid'; Source='release/extlibs/pkcs/test/oid_errors.xl'}
        )) {
            $binary = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $binary)
            $null = Run "$($entry.Name) AOT" $binary @()
        }
        $boundary = Join-Path $work 'boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/pkcs/test/boundaries.xl', '-o', $boundary)
        $null = Run 'XLang boundaries AOT' $boundary @()
        $example = Join-Path $work 'example.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/pkcs.xl', '-o', $example)
        $aotOutput = Run 'XLang example AOT' $example @()
        if ($aotOutput -cne $cOutput) { throw 'C/XLang AOT output differs' }
        $cExampleAot = Join-Path $work 'c_example.exe'
        $null = Run 'C example build' $xl @('build',
            'release/extlibs/pkcs/examples/pkcs_c.c', '-o', $cExampleAot)
        $cAotOutput = Run 'C example AOT' $cExampleAot @()
        if ($cAotOutput -cne $cOutput) { throw 'C AOT output differs' }
    }
    $oid = Join-Path $work 'oid_errors.exe'
    $null = Run 'C OID errors compile' $gcc ($flags + @('-DNDEBUG',
        'release/extlibs/pkcs/test/oid_errors_c.c','release/extlibs/pkcs/pkcs_native.c',$runtime) + $links + @('-o',$oid))
    $null = Run 'C OID error diagnostics' $oid @()
    $passed = $true
    Write-Host 'pkcs Windows package gate: PASS'
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
    Write-Host "[pkcs] report: $work.json"
    if (Test-Path -LiteralPath $work -PathType Container) {
        $resolved = (Resolve-Path -LiteralPath $work).Path
        $base = [IO.Path]::GetFullPath($workRoot).TrimEnd('\', '/')
        if (-not $resolved.StartsWith($base + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw 'pkcs test directory escaped the build root'
        }
        if ($passed) {
            Get-ChildItem -LiteralPath $resolved -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $resolved -Force
        } else {
            Write-Warning "pkcs failure artifacts retained: $resolved"
        }
    }
    if ($changed.Count) { throw 'PKCS inputs changed during validation' }
}
