param([switch]$SkipAot)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../../..')).Path
$xl = Join-Path $root 'release/xl.exe'
$out = Join-Path $root ('build/test/floatEnv/aot_core_' + [guid]::NewGuid().ToString('N') + '.exe')
$exampleOut = Join-Path $root ('build/test/floatEnv/aot_example_' + [guid]::NewGuid().ToString('N') + '.exe')
$boundsOut = Join-Path $root ('build/test/floatEnv/aot_boundaries_' + [guid]::NewGuid().ToString('N') + '.exe')
[void][IO.Directory]::CreateDirectory((Split-Path $out))

function Run([string]$label, [string]$program, [string[]]$arguments) {
    $output = & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$label failed with exit $LASTEXITCODE" }
    Write-Host "[floatEnv] ${label}: PASS"
    return $output
}

function RunNegative([string]$label, [string]$source, [string]$symbol,
    [string]$reason = 'access') {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = @(& $xl check $source 2>&1 | ForEach-Object { $_.ToString() })
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prior
    }
    $diagnostic = $output -join "`n"
    if ($code -eq 0 -or ($symbol -and -not $diagnostic.Contains($symbol)) -or
        -not $diagnostic.Contains($reason)) {
        throw "${label}: expected access rejection`n$diagnostic"
    }
    Write-Host "[floatEnv] ${label}: PASS"
}

Push-Location $root
try {
    Run 'strict C syntax' 'gcc' @('-std=c11', '-Wall', '-Wextra', '-Werror',
        '-DXRT_MODULE_THREAD', '-Ilib/xrt', '-fsyntax-only',
        'release/extlibs/floatEnv/float_env_native.c',
        'release/extlibs/floatEnv/test/test_float_env.c') | Out-Null
    Run 'C boundaries' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'floatEnv',
        '-Source', 'release/extlibs/floatEnv/test/test_float_env.c') | Out-Null
    Run 'XLang boundaries' $xl @('run', 'release/extlibs/floatEnv/test/boundaries.xl') | Out-Null
    RunNegative 'restoreNative access' 'release/extlibs/floatEnv/test/private_restore_native_negative.xl' 'restoreNative'
    RunNegative 'holdNative access' 'release/extlibs/floatEnv/test/private_hold_native_negative.xl' 'holdNative'
    RunNegative 'module native export' 'release/extlibs/floatEnv/test/private_module_native_negative.xl' '' 'no such exported symbol'
    RunNegative 'round native export' 'release/extlibs/floatEnv/test/private_round_native_negative.xl' '' 'no such exported symbol'
    Run 'XLang allocation boundary' $xl @('run', 'release/extlibs/floatEnv/test/allocations.xl') | Out-Null
    $c = Run 'C example' 'powershell' @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', 'tools/run_extlib_c.ps1', '-Package', 'floatEnv',
        '-Source', 'release/extlibs/floatEnv/examples/float_env_c.c')
    $direct = Run 'C example xl run' $xl @('run',
        'release/extlibs/floatEnv/examples/float_env_c.c')
    if (($c -join "`n") -cne ($direct -join "`n")) {
        throw 'C example differs between package runner and xl run'
    }
    $xlang = Run 'XLang example' $xl @('run', 'release/examples/extlibs/floatEnv.xl')
    if (($c -join "`n") -cne ($xlang -join "`n")) {
        throw 'C/XLang example output differs'
    }
    Write-Host '[floatEnv] C/XLang output parity: PASS'
    if (-not $SkipAot) {
        Run 'AOT core build' $xl @('build', 'test/cases/stdlib_crt_float_env.xl', '-o', $out) | Out-Null
        Run 'AOT core' $out @() | Out-Null
        Run 'AOT example build' $xl @('build',
            'release/examples/extlibs/floatEnv.xl', '-o', $exampleOut) | Out-Null
        $aotOutput = Run 'AOT example' $exampleOut @()
        if (($aotOutput -join "`n") -cne ($c -join "`n")) {
            throw 'AOT example differs from C output'
        }
        Run 'AOT boundaries build' $xl @('build',
            'release/extlibs/floatEnv/test/boundaries.xl', '-o', $boundsOut) | Out-Null
        Run 'AOT boundaries' $boundsOut @() | Out-Null
    }
    RunNegative 'Snapshot constructor access' `
        'release/extlibs/floatEnv/test/private_adopt_constructor_negative.xl' 'Snapshot'
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $out -PathType Leaf) {
        Remove-Item -LiteralPath $out -Force
    }
    if (Test-Path -LiteralPath $exampleOut -PathType Leaf) {
        Remove-Item -LiteralPath $exampleOut -Force
    }
    if (Test-Path -LiteralPath $boundsOut -PathType Leaf) {
        Remove-Item -LiteralPath $boundsOut -Force
    }
}
