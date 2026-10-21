param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$workRoot = [IO.Path]::GetFullPath((Join-Path $root 'build/test'))
$work = Join-Path $workRoot ('crontab_guard_' + [guid]::NewGuid().ToString('N'))

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $lines = @(& $program @arguments 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    if ($code -ne 0) { throw "${label}: exit $code`n$($lines -join "`n")" }
    Write-Host "[crontab] ${label}: PASS"
    return ($lines -join "`n").Replace("`r", '').TrimEnd("`n")
}

Push-Location $root
try {
    Run 'manifest C syntax' 'gcc' @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_TIME', '-DXRT_MODULE_TIME_LOCAL', '-Ilib/xrt',
        '-Irelease/extlibs/crontab/include', '-fsyntax-only',
        'release/extlibs/crontab/crontab_native.c') | Out-Null
    Run 'C boundaries' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'crontab',
        '-Source', 'release/extlibs/crontab/test/test_crontab.c') | Out-Null
    Run 'C long horizon' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'crontab',
        '-Source', 'release/extlibs/crontab/test/test_long_horizon.c') | Out-Null
    $c = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'crontab',
        '-Source', 'release/extlibs/crontab/examples/crontab.c')
    $direct = Run 'direct C example' $xl @('run',
        'release/extlibs/crontab/examples/crontab.c')
    Push-Location (Join-Path $root 'release')
    try {
        Run 'XLang base' $xl @('run', '../test/cases/extlib_crontab.xl') | Out-Null
        Run 'XLang boundaries' $xl @('run', '../test/cases/extlib_crontab_boundaries.xl') | Out-Null
        Run 'XLang allocation' $xl @('run', 'extlibs/crontab/test/allocations.xl') | Out-Null
        $xlang = Run 'XLang example' $xl @('run', 'examples/extlibs/crontab.xl')
    } finally {
        Pop-Location
    }
    if ($c -cne $direct -or $c -cne $xlang) {
        throw 'C/XLang example output differs'
    }
    Write-Host '[crontab] C/XLang output parity: PASS'
    if (-not $SkipAot) {
        [IO.Directory]::CreateDirectory($work) | Out-Null
        $core = Join-Path $work 'aot_core.exe'
        $null = Run 'AOT core build' $xl @('build',
            'release/extlibs/crontab/test/aot_core.xl', '-o', $core)
        if ((Run 'AOT core' $core @()) -cne '') {
            throw 'crontab AOT core produced unexpected output'
        }
        $example = Join-Path $work 'aot_example.exe'
        $null = Run 'AOT example build' $xl @('build',
            'release/examples/extlibs/crontab.xl', '-o', $example)
        if ((Run 'AOT example' $example @()) -cne $c) {
            throw 'crontab AOT example differs from C'
        }
    }
} finally {
    Pop-Location
    $safeWork = [IO.Path]::GetFullPath($work)
    if (-not $safeWork.StartsWith($workRoot.TrimEnd('\', '/') +
        [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe crontab work directory: $safeWork"
    }
    if (Test-Path -LiteralPath $safeWork) {
        Remove-Item -LiteralPath $safeWork -Recurse -Force
    }
}
