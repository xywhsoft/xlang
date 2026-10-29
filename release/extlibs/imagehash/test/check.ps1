param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = [IO.Path]::GetFullPath((Join-Path $testRoot ('imagehash_guard_' + [guid]::NewGuid().ToString('N'))))
$inputs = @('release/extlibs/imagehash', 'release/extlibs/stb_image',
    'release/extlibs/stb_image_resize2', 'release/include') | ForEach-Object {
    Get-ChildItem (Join-Path $root $_) -Recurse -File | ForEach-Object FullName
}
$inputs += @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll', 'lib/xrt/xrt.h',
    'lib/xruntime/xruntime.h', 'tools/run_extlib_c.ps1', 'test/cases/extlib_imagehash.xl',
    'release/examples/extlibs/imagehash.xl', 'release/examples/extlibs/assets/tiny.png',
    'release/examples/games/assets/player.png') | ForEach-Object { Join-Path $root $_ }
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

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
    Write-Host "[imagehash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $work 'inputs.before.json')
    $null = Run 'strict GCC compile' $gcc @('-std=c11', '-Wall', '-Wextra',
        '-Werror', '-DXRT_MODULE_BUFFER', '-DXRT_MODULE_ONCE',
        '-DXRT_MODULE_THREAD_KEY', '-Ilib/xrt', '-Irelease/extlibs/imagehash',
        '-Irelease/extlibs/imagehash/include', '-fsyntax-only',
        'release/extlibs/imagehash/imagehash_native.c')

    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
        '-Irelease/extlibs/imagehash/include')
    $runtime = Join-Path $work 'runtime.o'
    $null = Run 'selected XRT runtime' $gcc ($flags + @('-c',
        'release/extlibs/imagehash/test/runtime.c', '-o', $runtime))
    $objects = @($runtime)
    foreach ($name in @('imagehash', 'stb_image', 'stb_image_resize2')) {
        $object = Join-Path $work ($name + '.o')
        $null = Run "$name GCC object" $gcc ($flags + @('-c',
            "release/extlibs/$name/${name}_native.c", '-o', $object))
        $objects += $object
    }
    $binary = Join-Path $work 'test_imagehash.exe'
    $null = Run 'GCC C boundaries build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/test_imagehash.c') + $objects +
        @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $binary))
    $null = Run 'GCC C boundaries' $binary @()
    $budgetExe = Join-Path $work 'test_owned_text.exe'
    $null = Run 'GCC C result budgets build' $gcc ($flags + @(
        'release/extlibs/imagehash/test/test_owned_text.c') + $objects +
        @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm', '-o', $budgetExe))
    $null = Run 'GCC C result budgets' $budgetExe @()
    $null = Run 'C result budgets JIT' $xl @('run', 'release/extlibs/imagehash/test/test_owned_text.c')
    foreach ($name in @('test_imagehash', 'test_error_threads')) {
        $null = Run $name $runner @('imagehash', "release/extlibs/imagehash/test/$name.c")
    }
    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_imagehash.xl')
    $null = Run 'XLang allocations' $xl @('run', 'release/extlibs/imagehash/test/allocations.xl')
    $null = Run 'XLang result budgets' $xl @('run',
        'release/extlibs/imagehash/test/owned_results.xl', '--', '15')

    $generated = Join-Path $work 'owned_probe.c'
    $null = Run 'whole-call ownership cgen' $xl @('cgen',
        'release/extlibs/imagehash/test/owned_probe.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    foreach ($name in @('xihash_ahash', 'xihash_dhash', 'xihash_phash')) {
        if ([regex]::Matches($generatedText, "\b\w+\.Data\s*=\s*$name\s*\(").Count -ne 1) {
            throw "$name must adopt the native C string exactly once in the probe"
        }
    }
    if ($generatedText.Contains('xl6_xrt_take_cstr(') -or
        $generatedText.Contains('xl6CStringCopyV1') -or
        $generatedText -match '=\s*xlir_string_copy\s*\(') {
        throw 'imagehash copied adopted result text in the complete public return chain'
    }

    $expected = (Get-Content -LiteralPath 'release/extlibs/imagehash/examples/imagehash.expected.txt' -Raw).
        Replace("`r`n", "`n").TrimEnd("`n")
    $cOutput = Run 'C example' $runner @('imagehash', 'release/extlibs/imagehash/examples/imagehash.c')
    $xlOutput = Run 'XLang example' $xl @('run', 'release/examples/extlibs/imagehash.xl')
    $directSource = 'release/extlibs/imagehash/examples/imagehash.c'
    $directOutput = Run 'C example direct JIT' $xl @('run', $directSource)
    if ($cOutput -cne $expected -or $xlOutput -cne $expected -or $directOutput -cne $expected) {
        throw 'C/XLang examples differ from the fixed imagehash result'
    }
    $directExe = Join-Path $work 'imagehash_c.exe'
    $null = Run 'C example direct AOT build' $xl @('build', $directSource, '-o', $directExe)
    $builtOutput = Run 'C example direct AOT' $directExe @()
    if ($builtOutput -cne $expected) { throw 'C AOT imagehash result differs from fixed output' }

    if (-not $SkipAot) {
        $budgetAot = Join-Path $work 'owned_c.exe'
        $null = Run 'C result budgets AOT build' $xl @('build',
            'release/extlibs/imagehash/test/test_owned_text.c', '-o', $budgetAot)
        $null = Run 'C result budgets AOT' $budgetAot @()
        foreach ($entry in @(
            @{Name='core'; Source='release/extlibs/imagehash/test/aot_core.xl'; Args=@()},
            @{Name='central'; Source='test/cases/extlib_imagehash.xl'; Args=@()},
            @{Name='allocations'; Source='release/extlibs/imagehash/test/allocations.xl'; Args=@()},
            @{Name='owned'; Source='release/extlibs/imagehash/test/owned_results.xl'; Args=@('15')}
        )) {
            $exe = Join-Path $work ($entry.Name + '.exe')
            $null = Run "$($entry.Name) AOT build" $xl @('build', $entry.Source, '-o', $exe)
            $null = Run "$($entry.Name) AOT" $exe $entry.Args
        }
        $exampleExe = Join-Path $work 'imagehash_example.exe'
        $null = Run 'XLang example AOT build' $xl @('build',
            'release/examples/extlibs/imagehash.xl', '-o', $exampleExe)
        $aotOutput = Run 'XLang example AOT' $exampleExe @()
        if ($aotOutput -cne $expected) { throw 'XLang AOT imagehash result differs from fixed output' }
    }
    foreach ($path in $inputs) {
        if ((Get-FileHash -LiteralPath $path).Hash -cne $hashes[$path]) {
            throw "imagehash validation input changed: $path"
        }
    }
    $passed = $true
    if ($SkipAot) { Write-Output 'imagehash Windows focused gate: PASS' }
    else { Write-Output 'imagehash Windows complete AOT gate: PASS' }
} finally {
    Pop-Location
    if (-not $work.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe imagehash cleanup path: $work"
    }
    if (Test-Path -LiteralPath $work) {
        if ($passed) {
            Get-ChildItem -LiteralPath $work -File | ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
            Remove-Item -LiteralPath $work -Force
        } else { Write-Warning "imagehash failure artifacts retained: $work" }
    }
}
