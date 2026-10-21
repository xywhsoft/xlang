param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$runner = Join-Path $root 'tools/run_extlib_c.ps1'
$workParent = Join-Path $root 'build/test/terraform_state_guard'
$work = Join-Path $workParent ('run_' + [guid]::NewGuid().ToString('N'))
$gcc = (Get-Command gcc -CommandType Application -ErrorAction Stop).Source
[IO.Directory]::CreateDirectory($work) | Out-Null

function Run([string]$name, [string]$program, [string[]]$arguments) {
    $ErrorActionPreference = 'Continue'
    $output = & $program @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "${name}: exit $LASTEXITCODE`n$($output -join "`n")"
    }
    Write-Host "[terraform_state] ${name}: PASS"
    return ($output -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
}

Push-Location $root
try {
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', '-Irelease/extlibs/terraform_state/include', '-fsyntax-only')
    $null = Run 'strict GCC' $gcc ($flags + @(
        'release/extlibs/terraform_state/terraform_state_native.c',
        'release/extlibs/terraform_state/test/test_terraform_state.c',
        'release/extlibs/terraform_state/examples/basic_c.c',
        'release/extlibs/terraform_state/test/abi_contract.c'))
    $runtime = Join-Path $work 'xrt_runtime.o'
    $null = Run 'XRT GCC compile' $gcc @('-std=c11', '-O2', '-Ilib/xrt', '-c',
        'release/extlibs/terraform_state/test/xrt_runtime.c', '-o', $runtime)
    $linkFlags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Ilib/xrt', '-Irelease/extlibs/terraform_state/include',
        'release/extlibs/terraform_state/terraform_state_native.c', $runtime,
        '-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm')
    $cTest = Join-Path $work 'c_boundaries.exe'
    $null = Run 'C GCC boundaries build' $gcc ($linkFlags + @(
        'release/extlibs/terraform_state/test/test_terraform_state.c', '-o', $cTest))
    $null = Run 'C GCC boundaries' $cTest @()
    $cExample = Join-Path $work 'c_example.exe'
    $null = Run 'C GCC example build' $gcc ($linkFlags + @(
        'release/extlibs/terraform_state/examples/basic_c.c', '-o', $cExample))
    $gccOutput = Run 'C GCC example' $cExample @()
    $null = Run 'C boundaries' (Get-Process -Id $PID).Path @('-NoProfile',
        '-ExecutionPolicy', 'Bypass', '-File', $runner, '-Package', 'terraform_state',
        '-Source', 'release/extlibs/terraform_state/test/test_terraform_state.c')
    $cOutput = Run 'C example' (Get-Process -Id $PID).Path @('-NoProfile',
        '-ExecutionPolicy', 'Bypass', '-File', $runner, '-Package', 'terraform_state',
        '-Source', 'release/extlibs/terraform_state/examples/basic_c.c')
    $expected = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'basic.expected.txt') -Raw -Encoding UTF8).Replace("`r", '').TrimEnd("`n")
    if ($gccOutput -cne $expected) { throw 'C example differs from fixed expected output' }
    if ($cOutput -cne $gccOutput) { throw 'GCC/TCC C example output differs' }
    $directOutput = Run 'direct C example' $xl @('run',
        'release/extlibs/terraform_state/examples/basic_c.c')
    if ($directOutput -cne $gccOutput) { throw 'direct C example output differs' }
    Push-Location (Join-Path $root 'release')
    try {
        $null = Run 'XLang boundaries JIT' $xl @('run', 'extlibs/terraform_state/test/boundaries.xl')
        $null = Run 'XLang allocation failures JIT' $xl @('run', 'extlibs/terraform_state/test/allocations.xl')
        $null = Run 'XLang existing case' $xl @('run', '../test/cases/extlib_terraform_state.xl')
        $xlOutput = Run 'XLang example JIT' $xl @('run', 'extlibs/terraform_state/examples/basic.xl')
        if ($cOutput -cne $xlOutput) { throw 'C/XLang example output differs' }
        Write-Host '[terraform_state] C/XLang parity: PASS'
        $core = Join-Path $work 'boundaries.exe'
        $null = Run 'XLang boundaries build' $xl @('build',
            'extlibs/terraform_state/test/boundaries.xl', '-o', $core)
        $null = Run 'XLang boundaries AOT' $core @()
        if (-not $SkipAot) {
            $example = Join-Path $work 'basic.exe'
            $null = Run 'XLang example build' $xl @('build',
                'extlibs/terraform_state/examples/basic.xl', '-o', $example)
            $aotOutput = Run 'XLang example AOT' $example @()
            if ($aotOutput -cne $cOutput) { throw 'C/XLang AOT example output differs' }
        }
    } finally {
        Pop-Location
    }
} finally {
    Pop-Location
    $parentPath = [IO.Path]::GetFullPath($workParent).TrimEnd('\') + '\'
    $workPath = [IO.Path]::GetFullPath($work)
    if (-not $workPath.StartsWith($parentPath,
        [StringComparison]::OrdinalIgnoreCase)) {
        throw "terraform_state temporary work path escaped build/test: $workPath"
    }
    Remove-Item -LiteralPath $workPath -Recurse -Force
}
