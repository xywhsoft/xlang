param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$workParent = Join-Path $root 'build/test/consistent_hash_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work) | Out-Null
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
$xl = Join-Path $root 'release/xl.exe'
$flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Ilib/xrt', '-Irelease/extlibs/consistent_hash')
$links = @('-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $program @arguments 2>&1
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[consistent_hash] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT compile' $gcc ($flags + @('-c',
        'release/extlibs/consistent_hash/test/xrt_runtime.c', '-o', $runtime))
    $native = Join-Path $work 'consistent_hash_native.o'
    $null = Run 'native compile' $gcc ($flags + @('-c',
        'release/extlibs/consistent_hash/consistent_hash_native.c', '-o', $native))
    foreach ($name in @('test_consistent_hash', 'test_threads')) {
        $target = Join-Path $work "$name.exe"
        $null = Run "$name compile" $gcc ($flags + @(
            "release/extlibs/consistent_hash/test/$name.c", $native, $runtime) +
            $links + @('-o', $target))
        $null = Run $name $target @()
    }
    $example = Join-Path $work 'consistent_hash_example.exe'
    $null = Run 'C example compile' $gcc ($flags + @(
        'release/extlibs/consistent_hash/examples/consistent_hash_c.c',
        $runtime) + $links + @('-o', $example))
    $cOutput = Run 'C example' $example @()
    $directOutput = Run 'C example xl run' $xl @('run',
        'release/extlibs/consistent_hash/examples/consistent_hash_c.c')
    if ($directOutput -cne $cOutput) {
        throw "direct C example differs from GCC output:`nGCC:`n$cOutput`nXL:`n$directOutput"
    }

    $null = Run 'XLang central' $xl @('run', 'test/cases/extlib_consistent_hash.xl')
    foreach ($name in @('boundaries', 'allocations', 'layout')) {
        $null = Run "XLang $name" $xl @('run',
            "release/extlibs/consistent_hash/test/$name.xl")
    }
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $negative = & $xl check `
            'release/extlibs/consistent_hash/test/private_native.xl' 2>&1
        $negativeExit = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    $diagnostic = $negative -join "`n"
    if ($negativeExit -eq 0 -or -not $diagnostic.Contains('addNative') -or
        -not $diagnostic.Contains('access')) {
        throw "private native entry should be rejected:`n$diagnostic"
    }
    Write-Host '[consistent_hash] private native entry: PASS'

    $xlOutput = Run 'XLang example' $xl @('run',
        'release/examples/extlibs/consistent_hash.xl')
    if ($cOutput -cne $xlOutput) {
        throw "C/XLang examples differ:`nC:`n$cOutput`nXL:`n$xlOutput"
    }
    Write-Host '[consistent_hash] C/XLang parity: PASS'

    if (-not $SkipAot) {
        $aot = Join-Path $work 'consistent_hash_boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'release/extlibs/consistent_hash/test/boundaries.xl', '-o', $aot)
        $null = Run 'XLang boundaries AOT' $aot @()
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "consistent_hash temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
