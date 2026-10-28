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
$inputs = @(Get-ChildItem (Join-Path $root 'release/extlibs/pkcs') -Recurse -File |
    ForEach-Object FullName)
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h', 'release/include/xruntime_decl.h',
    'release/include/xl_extlib_abi.h', 'tools/run_extlib_c.ps1',
    'test/cases/extlib_pkcs.xl', 'release/examples/extlibs/pkcs.xl') |
    ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false
$ownedSymbols = @('get_content', 'encode_int', 'encode_oid', 'decode_oid',
    'wrap_sequence', 'wrap_set', 'encode_octet', 'encode_null', 'encode_bitstring',
    'concat', 'tag_name')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    [IO.File]::AppendAllText((Join-Path $work 'commands.log'),
        "${name}: $program $($arguments -join ' ')`nexit=$code`n$($output -join "`n")`n")
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[pkcs] ${name}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
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
    $generated = Join-Path $work 'owned_text.c'
    $null = Run 'owned-text cgen' $xl @('cgen',
        'release/examples/extlibs/pkcs.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
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
            @{Name='owned'; Source='release/extlibs/pkcs/test/owned_results.xl'}
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
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "pkcs validation input changed: $path"
        }
    }
    $passed = $true
    Write-Host 'pkcs Windows package gate: PASS'
} finally {
    Pop-Location
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
}
