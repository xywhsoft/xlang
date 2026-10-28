param([switch]$SkipAot, [string]$ReferenceDir = '')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $testRoot ('json5_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/json5')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$sources = @('release/extlibs/json5/json5_native.c',
    'release/extlibs/json5/vendor/pdjson5.c')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($code -ne 0) {
        throw "${name}: exit $code`n$($output -join "`n")"
    }
    Write-Host "[json5] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/json5/test/xrt_runtime.c', '-o', $runtime))
    $target = Join-Path $work 'test_boundaries.exe'
    $null = Run 'C boundaries compile' $gcc ($flags + $sources + @(
        'release/extlibs/json5/test/test_boundaries.c', $runtime) +
        $links + @('-o', $target))
    $null = Run 'C boundaries' $target @()
    if ($ReferenceDir -ne '') {
        $reference = (Resolve-Path -LiteralPath $ReferenceDir -ErrorAction Stop).Path
        $node = (Get-Command node -CommandType Application -ErrorAction Stop).Source
        $probe = Join-Path $work 'reference_probe.exe'
        $null = Run 'C reference probe compile' $gcc ($flags + $sources + @(
            'release/extlibs/json5/test/reference_probe.c', $runtime) +
            $links + @('-o', $probe))
        $null = Run 'JSON5 reference' $node @(
            'release/extlibs/json5/test/reference_probe.mjs', $probe, $reference)
    }
    $example = Join-Path $work 'json5_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/json5/examples/json5_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directCOutput = Run 'C example direct xl run' $xl @('run',
        'release/extlibs/json5/examples/json5_c.c')
    if ($directCOutput -cne $cOutput) {
        throw 'direct C example output differs'
    }
    $generated = Join-Path $work 'json5_owned.c'
    $null = Run 'XLang direct string cgen' $xl @('cgen',
        'release/extlibs/json5/test/boundaries.xl', '--emit', $generated)
    $generatedText = [IO.File]::ReadAllText($generated)
    if ($generatedText -notmatch '\.Data\s*=\s*xjson5_get_text_len\s*\(' -or
        $generatedText.Contains('xjson5_get_text_abi') -or
        $generatedText.Contains('xl6_xrt_take_cstr(')) {
        throw 'json5.get must adopt the original owned C string directly'
    }

    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/json5/test/boundaries.xl')
        $null = Run 'XLang allocations' $xl @('run', 'extlibs/json5/test/allocations.xl')
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/json5/examples/json5.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang JSON5 examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[json5] C/XLang parity: PASS'
        $core = Join-Path $work 'json5_xl_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'extlibs/json5/test/aot_core.xl', '-o', $core)
        $null = Run 'XLang core AOT' $core @()
        $publicOutput = Run 'Public XLang example' $xl @('run', 'examples/extlibs/json5.xl')
        if (-not $SkipAot) {
            $testAot = Join-Path $work 'json5_xl_boundaries.exe'
            $null = Run 'XLang boundaries build' $xl @('build',
                'extlibs/json5/test/boundaries.xl', '-o', $testAot)
            $null = Run 'XLang boundaries AOT' $testAot @()
            $allocAot = Join-Path $work 'json5_xl_allocations.exe'
            $null = Run 'XLang allocations build' $xl @('build',
                'extlibs/json5/test/allocations.xl', '-o', $allocAot)
            $null = Run 'XLang allocations AOT' $allocAot @()
            $exampleAot = Join-Path $work 'json5_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/json5/examples/json5.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT JSON5 example differs from C'
            }
            $publicAot = Join-Path $work 'json5_public_example.exe'
            $null = Run 'Public XLang example build' $xl @('build',
                'examples/extlibs/json5.xl', '-o', $publicAot)
            $publicAotOutput = Run 'Public XLang example AOT' $publicAot @()
            if ($publicAotOutput -cne $publicOutput) {
                throw 'public JSON5 example AOT differs from JIT'
            }
            $cExampleAot = Join-Path $work 'json5_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/json5/examples/json5_c.c', '-o', $cExampleAot)
            $cAotOutput = Run 'C example AOT' $cExampleAot @()
            if ($cAotOutput -cne $cOutput) {
                throw 'C JSON5 example AOT differs from GCC'
            }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
