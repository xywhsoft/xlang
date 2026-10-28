param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$testRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$workRelative = 'build/test/gettext_guard_' + [guid]::NewGuid().ToString('N')
$work = Join-Path $root $workRelative
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-Ilib/xrt',
    '-Irelease/extlibs/gettext')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
$inputs = @('release/xl.exe', 'release/xl.dll', 'release/xlc.dll',
    'lib/xrt/xrt.h', 'lib/xruntime/xruntime.h', 'release/include/xrt_decl.h',
    'release/include/xruntime_decl.h', 'release/include/xl_extlib_abi.h',
    'test/cases/extlib_gettext.xl', 'test/fixtures/gettext_mini.mo',
    'release/examples/extlibs/gettext.xl') | ForEach-Object { Join-Path $root $_ }
$inputs += @(Get-ChildItem (Join-Path $root 'release/extlibs/gettext') -Recurse -File |
    ForEach-Object FullName)
$hashes = @{}
foreach ($path in $inputs) { $hashes[$path] = (Get-FileHash -LiteralPath $path).Hash }
$passed = $false

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
    Write-Host "[gettext] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    [IO.Directory]::CreateDirectory($work) | Out-Null
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/gettext/test/xrt_runtime.c', '-o', $runtime))
    $native = @('release/extlibs/gettext/gettext_native.c',
        'release/extlibs/gettext/gettext_plural.c')
    $test = Join-Path $work 'gettext_test.exe'
    $null = Run 'C test compile' $gcc ($flags + $native +
        @('release/extlibs/gettext/test/test_gettext.c', $runtime) +
        $links + @('-o', $test))
    $null = Run 'C test' $test @($workRelative)

    $contextTest = Join-Path $work 'gettext_context_test.exe'
    $null = Run 'C context compile' $gcc ($flags + $native +
        @('release/extlibs/gettext/test/test_context_queries.c', $runtime) +
        $links + @('-o', $contextTest))
    $null = Run 'C context queries' $contextTest @()

    $example = Join-Path $work 'gettext_example.exe'
    $null = Run 'C example compile' $gcc ($flags +
        @('release/extlibs/gettext/examples/gettext_c.c', $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directC = Run 'C example xl run' $xl @('run',
        'release/extlibs/gettext/examples/gettext_c.c')
    if ($directC -cne $cOutput) {
        throw 'direct C gettext example differs from GCC output'
    }

    $null = Run 'XLang existing test' $xl @('run', 'test/cases/extlib_gettext.xl')
    $null = Run 'XLang duplicate keys' $xl @('run', 'release/extlibs/gettext/test/duplicates.xl')
    $null = Run 'XLang boundaries' $xl @('run', 'release/extlibs/gettext/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run', 'release/extlibs/gettext/test/allocations.xl')
    $null = Run 'XLang input limits' $xl @('run', 'release/extlibs/gettext/test/input_limits.xl')
    if (-not $SkipAot) {
        $cases = @(
            @('central', 'test/cases/extlib_gettext.xl'),
            @('duplicates', 'release/extlibs/gettext/test/duplicates.xl'),
            @('boundaries', 'release/extlibs/gettext/test/boundaries.xl'),
            @('allocations', 'release/extlibs/gettext/test/allocations.xl'),
            @('input_limits', 'release/extlibs/gettext/test/input_limits.xl')
        )
        foreach ($case in $cases) {
            $name = $case[0]
            $target = Join-Path $work "gettext_$name.exe"
            $null = Run "XLang $name build" $xl @('build', $case[1], '-o', $target)
            $null = Run "XLang $name AOT" $target @()
        }
    }

    Push-Location (Join-Path $root 'release')
    try {
        $xlOutput = Run 'XLang example' $xl @('run', 'extlibs/gettext/examples/gettext.xl')
        if ($cOutput -cne $xlOutput) {
            throw "C/XLang gettext example outputs differ:`nC:`n$cOutput`nXL:`n$xlOutput"
        }
        Write-Host '[gettext] C/XLang parity: PASS'
        $publicOutput = Run 'Public XLang example' $xl @('run', 'examples/extlibs/gettext.xl')

        if (-not $SkipAot) {
            $exampleAot = Join-Path $work 'gettext_xl_example.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/gettext/examples/gettext.xl', '-o', $exampleAot)
            $aotOutput = Run 'XLang example AOT' $exampleAot @()
            if ($aotOutput -cne $cOutput) {
                throw 'XLang AOT gettext example differs from C'
            }
            $publicAot = Join-Path $work 'gettext_public_example.exe'
            $null = Run 'Public XLang example build' $xl @('build',
                'examples/extlibs/gettext.xl', '-o', $publicAot)
            $publicAotOutput = Run 'Public XLang example AOT' $publicAot @()
            if ($publicAotOutput -cne $publicOutput) {
                throw 'public XLang AOT gettext example differs from JIT'
            }
            $cExampleAot = Join-Path $work 'gettext_c_example.exe'
            $null = Run 'C example build' $xl @('build',
                'extlibs/gettext/examples/gettext_c.c', '-o', $cExampleAot)
            Push-Location $root
            try {
                $cAotOutput = Run 'C example AOT' $cExampleAot @()
            } finally {
                Pop-Location
            }
            if ($cAotOutput -cne $cOutput) {
                throw 'C AOT gettext example differs from GCC'
            }
        }
    } finally {
        Pop-Location
    }
    foreach ($path in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $hashes[$path]) {
            throw "gettext input changed during gate: $path"
        }
    }
    $passed = $true
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($testRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe test cleanup path: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        if ($passed) {
            Remove-Item -LiteralPath $safeWork -Recurse -Force
        } else {
            Write-Warning "gettext failure artifacts: $safeWork"
        }
    }
}
