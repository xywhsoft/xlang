param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$release = Join-Path $root 'release'
$workRoot = Join-Path $root 'build/test'
$work = Join-Path $workRoot ('srt_guard_' + [guid]::NewGuid().ToString('N'))
$xl = Join-Path $release 'xl.exe'
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
[void][IO.Directory]::CreateDirectory($work)

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previous
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($output -join "`n")" }
    Write-Host "[srt] ${label}: PASS"
    return ($output -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    $null = Run 'strict C syntax' $gcc @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-fsyntax-only', '-Ilib/xrt', '-Irelease/extlibs/srt/include',
        'release/extlibs/srt/srt_native.c',
        'release/extlibs/srt/test/test_srt.c',
        'release/extlibs/srt/test/abi_contract.c')
    $null = Run 'C boundaries' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'srt', 'release/extlibs/srt/test/test_srt.c')
    $cExample = Run 'C example' (Join-Path $root 'tools/run_extlib_c.ps1') @(
        'srt', 'release/examples/extlibs/srt_c.c')
    Push-Location $release
    try {
        $null = Run 'central XLang case' $xl @('run', '../test/cases/extlib_srt.xl')
        $null = Run 'XLang boundaries' $xl @('run', 'extlibs/srt/test/boundaries.xl')
        $null = Run 'XLang allocation failures' $xl @('run', 'extlibs/srt/test/allocations.xl')
        $xlExample = Run 'XLang example' $xl @('run', 'examples/extlibs/srt_pair.xl')
    } finally { Pop-Location }
    if ($cExample -cne $xlExample) { throw '[srt] C/XLang example output differs' }
    Write-Host '[srt] C/XLang example parity: PASS'
    if (-not $SkipAot) {
        $coreExe = Join-Path $work 'core.exe'
        $null = Run 'AOT core build' $xl @('build',
            'release/extlibs/srt/test/core_aot.xl', '-o', $coreExe)
        $null = Run 'AOT core' $coreExe @()
        $exampleExe = Join-Path $work 'example.exe'
        $null = Run 'AOT example build' $xl @('build',
            'release/examples/extlibs/srt_pair.xl', '-o', $exampleExe)
        $aotExample = Run 'AOT example' $exampleExe @()
        if ($aotExample -cne $cExample) { throw '[srt] AOT/C example output differs' }
        Write-Host '[srt] AOT/C example parity: PASS'
    }
} finally {
    Pop-Location
    $resolvedRoot = [IO.Path]::GetFullPath($workRoot).TrimEnd('\') + '\'
    $resolvedWork = [IO.Path]::GetFullPath($work)
    if (-not $resolvedWork.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase) -or
        -not [IO.Path]::GetFileName($resolvedWork).StartsWith('srt_guard_')) {
        throw "SRT test directory escapes build/test: $resolvedWork"
    }
    if (Test-Path -LiteralPath $resolvedWork -PathType Container) {
        Remove-Item -LiteralPath $resolvedWork -Recurse -Force
    }
}
