param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test'
$work = Join-Path $workParent ('sharedmem_guard_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
[IO.Directory]::CreateDirectory($work) | Out-Null
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/sharedmem')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "${name}: exit $code`n$($output -join "`n")" }
    Write-Host "[sharedmem] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $native = Join-Path $work 'sharedmem_native.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/sharedmem/test/xrt_runtime.c', '-o', $runtime))
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/sharedmem/sharedmem_native.c', '-o', $native))

    $test = Join-Path $work 'test_sharedmem.exe'
    $null = Run 'C cross-process compile' $gcc ($flags + @(
        'release/extlibs/sharedmem/test/test_sharedmem.c', $native, $runtime) +
        $links + @('-o', $test))
    $null = Run 'C cross-process boundaries' $test @()

    $allocation = Join-Path $work 'test_sharedmem_alloc.exe'
    $null = Run 'C allocation compile' $gcc ($flags + @('-DNDEBUG',
        'release/extlibs/sharedmem/test/test_sharedmem_alloc.c', $native, $runtime) +
        $links + @('-o', $allocation))
    $null = Run 'C allocation boundaries' $allocation @()

    $example = Join-Path $work 'sharedmem_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/sharedmem/examples/sharedmem_c.c', $native, $runtime) +
        $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/sharedmem/examples/sharedmem_c.c')
    if ($cOutput -cne $directOutput) {
        throw 'C example differs between GCC and xl run'
    }

    $null = Run 'XLang central boundaries' $xl @('run',
        'test/cases/extlib_sharedmem.xl')
    $null = Run 'XLang package boundaries' $xl @('run',
        'release/extlibs/sharedmem/test/boundaries.xl')
    $null = Run 'XLang allocations' $xl @('run',
        'release/extlibs/sharedmem/test/allocations.xl')
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostic = @(& $xl check 'release/extlibs/sharedmem/test/private_native_negative.xl' 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n"
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -eq 0 -or -not $diagnostic.Contains('writeNative') -or
        -not $diagnostic.Contains('access')) {
        throw "private native access was not rejected:`n$diagnostic"
    }
    Write-Host '[sharedmem] private native access: PASS'

    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/sharedmem.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[sharedmem] C/XLang parity: PASS'

    if (-not $SkipAot) {
        $aot = Join-Path $work 'sharedmem_core.exe'
        $null = Run 'XLang core build' $xl @('build',
            'test/cases/extlib_sharedmem.xl', '-o', $aot)
        $null = Run 'XLang core AOT' $aot @()

        $exampleAot = Join-Path $work 'sharedmem_example_xl.exe'
        $null = Run 'XLang example build' $xl @('build',
            'release/examples/extlibs/sharedmem.xl', '-o', $exampleAot)
        $aotOutput = Run 'XLang example AOT' $exampleAot @()
        if ($aotOutput -cne $cOutput) {
            throw "C/XLang AOT example differs:`nC:`n$cOutput`nXL:`n$aotOutput"
        }
        Write-Host '[sharedmem] C/XLang AOT parity: PASS'
    }

    # 私有原生构造器不能从包外接管任意指针；只做编译检查，绝不执行负例。
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $diagnostic = @(& $xl check 'release/extlibs/sharedmem/test/private_constructor_negative.xl' 2>&1 |
            ForEach-Object { $_.ToString() }) -join "`n"
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -eq 0 -or -not $diagnostic.Contains('access')) {
        throw "private native constructor was not rejected:`n$diagnostic"
    }
    Write-Host '[sharedmem] private native constructor: PASS'
} finally {
    Pop-Location
    $resolvedParent = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedParent, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('sharedmem_guard_')) {
        throw "sharedmem test directory escapes build/test: $resolvedWork"
    }
    Remove-Item -LiteralPath $resolvedWork -Recurse -Force
}
